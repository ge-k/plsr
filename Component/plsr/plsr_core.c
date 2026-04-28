#include "plsr_core.h"
#include "plsr_config.h"
#include "plsr_port.h"
#include "plsr_hw.h"

static uint32_t clamp_freq(uint32_t freq_hz)
{
    if (freq_hz < PLSR_FREQ_MIN_HZ) {
        return PLSR_FREQ_MIN_HZ;
    }
    if (freq_hz > PLSR_FREQ_MAX_HZ) {
        return PLSR_FREQ_MAX_HZ;
    }
    return freq_hz;
}

static uint32_t abs_i64_to_u32(int64_t value)
{
    uint64_t abs_value = (value < 0) ? (uint64_t)(-value) : (uint64_t)value;
    if (abs_value > 0xFFFFFFFFULL) {
        return 0xFFFFFFFFUL;
    }
    return (uint32_t)abs_value;
}

static bool current_waits_ext(const PlsrCore *core)
{
    if ((core->current_segment == 0U) ||
        (core->current_segment > core->active_config.segment_count)) {
        return false;
    }

    return core->active_config.segments[core->current_segment - 1U].wait_condition == PLSR_WAIT_EXT_RISING;
}

static void add_delta_pulses(PlsrCore *core, uint32_t delta)
{
    PlsrIrqState irq_state;

    if ((core == NULL) || (delta == 0U)) {
        return;
    }

    irq_state = plsr_port_enter_critical();
    core->record.total_distance += delta;
    core->job_sent_pulses += delta;
    core->segment_sent_pulses += delta;
    core->record.position += ((int64_t)delta * (int64_t)core->current_dir_sign);
    plsr_port_exit_critical(irq_state);
}

static void finish_done(PlsrCore *core)
{
    PlsrHwPollResult ignored;

    plsr_hw_stop_now(0, &ignored);
    core->state = PLSR_STATE_DONE;
    core->current_segment = 0;
    core->current_freq_hz = 0;
    core->target_abs_pulses = 0;
    core->segment_sent_pulses = 0;
    core->ext_latched = false;
    core->ext_stop_latched = 0;
    core->freq_request_pending = false;
}

static void enter_error(PlsrCore *core, PlsrResult error)
{
    PlsrHwPollResult ignored;

    plsr_hw_stop_now(0, &ignored);
    core->last_error = error;
    core->state = PLSR_STATE_ERROR;
    core->current_freq_hz = 0;
    core->ext_latched = false;
    core->ext_stop_latched = 0;
    core->freq_request_pending = false;
}

static uint32_t segment_elapsed_ms(const PlsrCore *core)
{
    return (uint32_t)(plsr_port_get_tick_ms() - core->segment_start_tick);
}

static void build_current_profile(PlsrCore *core, uint32_t start_freq_hz, uint32_t target_freq_hz)
{
    plsr_profile_build(&core->profile,
                       (PlsrAccelMode)core->active_config.accel_mode,
                       clamp_freq(start_freq_hz),
                       clamp_freq(target_freq_hz),
                       core->active_config.accel_time_ms,
                       core->active_config.decel_time_ms);
}

static void update_current_freq_snapshot(PlsrCore *core)
{
    if (core->state == PLSR_STATE_RUNNING) {
        core->current_freq_hz = plsr_profile_eval(&core->profile, segment_elapsed_ms(core));
        core->next_start_freq_hz = core->current_freq_hz;
    }
}

static bool update_running_frequency(PlsrCore *core)
{
    uint32_t next_freq = plsr_profile_eval(&core->profile, segment_elapsed_ms(core));

    if (next_freq != core->current_freq_hz) {
        if (plsr_hw_set_frequency(0, next_freq) != PLSR_HW_OK) {
            enter_error(core, PLSR_ERR_HW);
            return false;
        }
        core->current_freq_hz = next_freq;
        core->next_start_freq_hz = next_freq;
    }
    return true;
}

static void start_current_segment_output(PlsrCore *core)
{
    core->segment_start_tick = plsr_port_get_tick_ms();
    core->current_freq_hz = plsr_profile_eval(&core->profile, 0);
    core->next_start_freq_hz = core->current_freq_hz;

    if (plsr_hw_set_frequency(0, core->current_freq_hz) != PLSR_HW_OK) {
        enter_error(core, PLSR_ERR_HW);
        return;
    }

    if (plsr_hw_start(0, core->target_abs_pulses) != PLSR_HW_OK) {
        enter_error(core, PLSR_ERR_HW);
        return;
    }

    core->state = PLSR_STATE_RUNNING;
}

static void begin_segment(PlsrCore *core, uint8_t segment)
{
    uint8_t guard;

    for (guard = 0; guard <= (core->active_config.segment_count + 1U); guard++) {
        const PlsrSegmentConfig *segment_config;
        int64_t signed_pulses;
        uint32_t abs_pulses;
        int8_t next_dir_sign;
        bool needs_dir_delay;

        if ((segment == 0U) || (segment > core->active_config.segment_count)) {
            finish_done(core);
            return;
        }

        core->current_segment = segment;
        segment_config = &core->active_config.segments[segment - 1U];

        if (core->active_config.run_mode == PLSR_RUN_ABSOLUTE) {
            signed_pulses = (int64_t)segment_config->pulse_count - core->record.position;
        } else {
            signed_pulses = (int64_t)segment_config->pulse_count;
        }

        if (signed_pulses == 0) {
            core->no_progress_jumps++;
            if (core->no_progress_jumps > (core->active_config.segment_count + 1U)) {
                enter_error(core, PLSR_ERR_INVALID_TOTAL_PULSES);
                return;
            }
            segment = plsr_config_next_segment(&core->active_config, segment);
            continue;
        }

        abs_pulses = abs_i64_to_u32(signed_pulses);
        if (abs_pulses == 0U) {
            finish_done(core);
            return;
        }

        next_dir_sign = (signed_pulses >= 0) ? 1 : -1;
        needs_dir_delay = core->dir_sign_valid &&
                          (core->current_dir_sign != next_dir_sign) &&
                          (core->active_config.dir_delay_ms > 0U);

        core->current_dir_sign = next_dir_sign;
        core->target_abs_pulses = abs_pulses;
        core->segment_sent_pulses = 0;
        core->no_progress_jumps = 0;
        core->dir_set_tick = plsr_port_get_tick_ms();
        core->ext_latched = false;
        core->ext_stop_latched = 0;
        core->freq_request_pending = false;

        if (plsr_hw_set_direction(0,
                                  core->current_dir_sign > 0,
                                  core->active_config.dir_positive_logic == 0U) != PLSR_HW_OK) {
            enter_error(core, PLSR_ERR_HW);
            return;
        }

        core->dir_sign_valid = true;
        build_current_profile(core,
                              core->next_start_freq_hz,
                              segment_config->freq_hz);

        if (needs_dir_delay) {
            core->current_freq_hz = 0;
            core->state = PLSR_STATE_DIR_DELAY;
        } else {
            start_current_segment_output(core);
        }
        return;
    }

    enter_error(core, PLSR_ERR_INVALID_TOTAL_PULSES);
}

static void complete_segment(PlsrCore *core)
{
    uint8_t next_segment;

    next_segment = plsr_config_next_segment(&core->active_config, core->current_segment);
    if (next_segment == 0U) {
        finish_done(core);
        return;
    }

    begin_segment(core, next_segment);
}

static void apply_frequency_request(PlsrCore *core)
{
    uint32_t requested;

    if (!core->freq_request_pending) {
        return;
    }

    update_current_freq_snapshot(core);
    requested = clamp_freq(core->requested_freq_hz);
    core->active_config.segments[core->current_segment - 1U].freq_hz = requested;
    build_current_profile(core, core->next_start_freq_hz, requested);
    core->segment_start_tick = plsr_port_get_tick_ms();
    core->freq_request_pending = false;
}

static void run_dir_delay(PlsrCore *core)
{
    uint32_t now = plsr_port_get_tick_ms();

    if (current_waits_ext(core) && core->ext_latched) {
        core->ext_latched = false;
        complete_segment(core);
        return;
    }

    if ((uint32_t)(now - core->dir_set_tick) < core->active_config.dir_delay_ms) {
        return;
    }

    apply_frequency_request(core);
    start_current_segment_output(core);
}

static void run_running(PlsrCore *core)
{
    PlsrHwPollResult poll_result;

    if (plsr_hw_poll(0, &poll_result) != PLSR_HW_OK) {
        enter_error(core, PLSR_ERR_HW);
        return;
    }

    add_delta_pulses(core, poll_result.delta_pulses);

    if (current_waits_ext(core) && core->ext_stop_latched) {
        update_current_freq_snapshot(core);
        core->ext_stop_latched = 0;
        core->ext_latched = false;
        complete_segment(core);
        return;
    }

    if (current_waits_ext(core) && core->ext_latched) {
        PlsrHwPollResult stop_result;
        plsr_hw_stop_now(0, &stop_result);
        add_delta_pulses(core, stop_result.delta_pulses);
        update_current_freq_snapshot(core);
        core->ext_latched = false;
        complete_segment(core);
        return;
    }

    if (poll_result.pwm_stopped ||
        (core->segment_sent_pulses >= core->target_abs_pulses)) {
        update_current_freq_snapshot(core);
        if (current_waits_ext(core)) {
            core->current_freq_hz = 0;
            core->state = PLSR_STATE_WAIT_EXT;
        } else {
            complete_segment(core);
        }
        return;
    }

    apply_frequency_request(core);
    if (!update_running_frequency(core)) {
        return;
    }
}

static void run_wait_ext(PlsrCore *core)
{
    if (!core->ext_latched) {
        return;
    }

    core->ext_latched = false;
    complete_segment(core);
}

void plsr_core_init(PlsrCore *core)
{
    if (core == NULL) {
        return;
    }

    memset(core, 0, sizeof(*core));
    plsr_config_set_defaults(&core->active_config);
    core->state = PLSR_STATE_IDLE;
    core->last_error = PLSR_OK;
    core->current_freq_hz = 0;
    core->next_start_freq_hz = core->active_config.default_speed_hz;
    core->current_dir_sign = 1;
    core->dir_sign_valid = false;
}

PlsrResult plsr_core_start(PlsrCore *core, const PlsrConfig *config, const PlsrRecord *record)
{
    PlsrHwConfig hw_config;
    PlsrResult result;

    if ((core == NULL) || (config == NULL) || (record == NULL)) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (!plsr_core_is_idle(core)) {
        return PLSR_ERR_BUSY;
    }

    result = plsr_config_validate_for_start(config);
    if (result != PLSR_OK) {
        core->last_error = result;
        return result;
    }

    core->active_config = *config;
    core->record = *record;
    core->state = PLSR_STATE_IDLE;
    core->last_error = PLSR_OK;
    core->current_segment = 0;
    core->job_sent_pulses = 0;
    core->target_abs_pulses = 0;
    core->segment_sent_pulses = 0;
    core->no_progress_jumps = 0;
    core->current_freq_hz = 0;
    core->next_start_freq_hz = clamp_freq(config->default_speed_hz);
    core->current_dir_sign = 1;
    core->dir_sign_valid = false;
    core->segment_start_tick = 0;
    core->ext_latched = false;
    core->ext_stop_latched = 0;
    core->freq_request_pending = false;

    hw_config.channel_id = 0;
    hw_config.pulse_terminal = config->pulse_terminal;
    hw_config.dir_terminal = config->dir_terminal;
    hw_config.ext_terminal = config->ext_terminal;
    hw_config.dir_positive_logic = config->dir_positive_logic;
    if (plsr_hw_configure_terminals(0, &hw_config) != PLSR_HW_OK) {
        core->last_error = PLSR_ERR_HW;
        return PLSR_ERR_HW;
    }

    begin_segment(core, config->start_segment);
    return core->last_error;
}

PlsrResult plsr_core_stop(PlsrCore *core)
{
    PlsrHwPollResult stop_result;

    if (core == NULL) {
        return PLSR_ERR_INVALID_VALUE;
    }

    if ((core->state == PLSR_STATE_RUNNING) ||
        (core->state == PLSR_STATE_DIR_DELAY) ||
        (core->state == PLSR_STATE_WAIT_EXT)) {
        if (plsr_hw_stop_now(0, &stop_result) != PLSR_HW_OK) {
            enter_error(core, PLSR_ERR_HW);
            return PLSR_ERR_HW;
        }
        add_delta_pulses(core, stop_result.delta_pulses);
        update_current_freq_snapshot(core);
        finish_done(core);
    }

    return PLSR_OK;
}

void plsr_core_task_1ms(PlsrCore *core)
{
    if (core == NULL) {
        return;
    }

    switch (core->state) {
    case PLSR_STATE_DIR_DELAY:
        run_dir_delay(core);
        break;

    case PLSR_STATE_RUNNING:
        run_running(core);
        break;

    case PLSR_STATE_WAIT_EXT:
        run_wait_ext(core);
        break;

    case PLSR_STATE_DONE:
        core->state = PLSR_STATE_IDLE;
        break;

    case PLSR_STATE_ERROR:
    case PLSR_STATE_IDLE:
    default:
        break;
    }
}

void plsr_core_on_ext_rising(PlsrCore *core)
{
    PlsrHwPollResult stop_result;

    if ((core == NULL) || !current_waits_ext(core)) {
        return;
    }

    if (core->state == PLSR_STATE_RUNNING) {
        if (plsr_hw_stop_now(0, &stop_result) == PLSR_HW_OK) {
            add_delta_pulses(core, stop_result.delta_pulses);
            core->ext_stop_latched = 1U;
        }
        return;
    }

    if ((core->state == PLSR_STATE_DIR_DELAY) ||
        (core->state == PLSR_STATE_WAIT_EXT)) {
        core->ext_latched = true;
    }
}

void plsr_core_request_current_freq(PlsrCore *core, uint32_t freq_hz)
{
    PlsrIrqState irq_state;

    if ((core == NULL) ||
        (freq_hz < PLSR_FREQ_MIN_HZ) ||
        (freq_hz > PLSR_FREQ_MAX_HZ) ||
        (core->current_segment == 0U)) {
        return;
    }

    irq_state = plsr_port_enter_critical();
    if ((core->state == PLSR_STATE_RUNNING) ||
        (core->state == PLSR_STATE_DIR_DELAY)) {
        core->requested_freq_hz = freq_hz;
        core->freq_request_pending = true;
    }
    plsr_port_exit_critical(irq_state);
}

void plsr_core_get_status(const PlsrCore *core, PlsrStatus *status)
{
    PlsrIrqState irq_state;

    if ((core == NULL) || (status == NULL)) {
        return;
    }

    irq_state = plsr_port_enter_critical();
    status->state = core->state;
    status->last_error = core->last_error;
    status->current_segment = core->current_segment;
    status->current_freq_hz = core->current_freq_hz;
    status->job_sent_pulses = core->job_sent_pulses;
    status->current_segment_target_pulses = core->target_abs_pulses;
    status->current_segment_sent_pulses = core->segment_sent_pulses;
    status->total_distance = core->record.total_distance;
    status->position = core->record.position;
    plsr_port_exit_critical(irq_state);
}

void plsr_core_set_record(PlsrCore *core, const PlsrRecord *record)
{
    PlsrIrqState irq_state;

    if ((core == NULL) || (record == NULL)) {
        return;
    }

    irq_state = plsr_port_enter_critical();
    core->record = *record;
    plsr_port_exit_critical(irq_state);
}

void plsr_core_get_record(const PlsrCore *core, PlsrRecord *record)
{
    PlsrIrqState irq_state;

    if ((core == NULL) || (record == NULL)) {
        return;
    }

    irq_state = plsr_port_enter_critical();
    *record = core->record;
    plsr_port_exit_critical(irq_state);
}

bool plsr_core_is_idle(const PlsrCore *core)
{
    bool idle;
    PlsrIrqState irq_state;

    if (core == NULL) {
        return true;
    }

    irq_state = plsr_port_enter_critical();
    idle = (core->state == PLSR_STATE_IDLE) ||
           (core->state == PLSR_STATE_DONE) ||
           (core->state == PLSR_STATE_ERROR);
    plsr_port_exit_critical(irq_state);
    return idle;
}
