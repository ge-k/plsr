#include "plsr_service.h"
#include "plsr_config.h"
#include "plsr_core.h"
#include "plsr_hw.h"
#include "plsr_port.h"
#include "plsr_storage.h"

#define PLSR_EXT_DEBOUNCE_MS 20U

static PlsrCore s_core;
static PlsrConfig s_pending_config;
static uint32_t s_last_ext_tick_ms = 0;

static uint32_t low_u32_from_u64(uint64_t value)
{
    return (uint32_t)(value & 0xFFFFFFFFULL);
}

static uint32_t low_u32_from_i64(int64_t value)
{
    return (uint32_t)((uint64_t)value & 0xFFFFFFFFULL);
}

static bool records_equal(const PlsrRecord *a, const PlsrRecord *b)
{
    return (a->total_distance == b->total_distance) && (a->position == b->position);
}

static PlsrHwConfig make_hw_config(const PlsrConfig *config)
{
    PlsrHwConfig hw_config;

    hw_config.channel_id = 0;
    hw_config.pulse_terminal = config->pulse_terminal;
    hw_config.dir_terminal = config->dir_terminal;
    hw_config.ext_terminal = config->ext_terminal;
    hw_config.dir_positive_logic = config->dir_positive_logic;
    return hw_config;
}

uint32_t plsr_time_ms(void)
{
    return plsr_port_get_tick_ms();
}

void plsr_service_init(void)
{
    PlsrRecord record;
    PlsrHwConfig hw_config;

    if (plsr_storage_load(&s_pending_config, &record) != PLSR_OK) {
        plsr_storage_set_defaults(&s_pending_config, &record);
    }

    plsr_core_init(&s_core);
    plsr_core_set_record(&s_core, &record);

    hw_config = make_hw_config(&s_pending_config);
    (void)plsr_hw_init(0, &hw_config);
}

void plsr_service_task_1ms(void)
{
    PlsrRecord before;
    PlsrRecord after;

    plsr_core_get_record(&s_core, &before);
    plsr_core_task_1ms(&s_core);
    plsr_core_get_record(&s_core, &after);

    if (!records_equal(&before, &after)) {
        plsr_storage_mark_dirty();
    }
}

void plsr_service_storage_task_100ms(void)
{
    PlsrRecord record;
    PlsrConfig config;

    plsr_service_get_config(&config);
    plsr_core_get_record(&s_core, &record);
    plsr_storage_task_100ms(&config, &record, plsr_core_is_idle(&s_core));
}

void plsr_service_on_exti(uint16_t gpio_pin)
{
    uint32_t now;

    if (!plsr_hw_is_ext_pin(gpio_pin)) {
        return;
    }

    now = plsr_time_ms();
    if ((uint32_t)(now - s_last_ext_tick_ms) < PLSR_EXT_DEBOUNCE_MS) {
        return;
    }
    s_last_ext_tick_ms = now;

    plsr_core_on_ext_rising(&s_core);
}

PlsrResult plsr_service_start_once(void)
{
    PlsrConfig config;
    PlsrRecord record;
    PlsrResult result;

    plsr_service_get_config(&config);
    result = plsr_config_validate_for_start(&config);
    if (result != PLSR_OK) {
        return result;
    }

    plsr_core_get_record(&s_core, &record);
    return plsr_core_start(&s_core, &config, &record);
}

PlsrResult plsr_service_clear_total_distance(void)
{
    PlsrRecord record;

    plsr_core_get_record(&s_core, &record);
    record.total_distance = 0;
    plsr_core_set_record(&s_core, &record);
    plsr_storage_mark_dirty();
    return PLSR_OK;
}

PlsrResult plsr_service_set_position(int64_t position)
{
    PlsrRecord record;

    if (!plsr_core_is_idle(&s_core)) {
        return PLSR_ERR_BUSY;
    }

    plsr_core_get_record(&s_core, &record);
    record.position = position;
    plsr_core_set_record(&s_core, &record);
    plsr_storage_mark_dirty();
    return PLSR_OK;
}

void plsr_service_get_config(PlsrConfig *config)
{
    PlsrIrqState irq_state;

    if (config == NULL) {
        return;
    }

    irq_state = plsr_port_enter_critical();
    *config = s_pending_config;
    plsr_port_exit_critical(irq_state);
}

PlsrResult plsr_service_set_config(const PlsrConfig *config)
{
    PlsrIrqState irq_state;
    PlsrResult result;

    if (config == NULL) {
        return PLSR_ERR_INVALID_VALUE;
    }

    result = plsr_config_validate_for_start(config);
    if (result != PLSR_OK) {
        return result;
    }

    irq_state = plsr_port_enter_critical();
    s_pending_config = *config;
    plsr_port_exit_critical(irq_state);
    plsr_storage_mark_dirty();
    return PLSR_OK;
}

static bool request_has_addr(uint16_t start, uint16_t count, uint16_t addr, const uint16_t *values, uint16_t *value)
{
    if ((addr < start) || (addr >= (uint16_t)(start + count))) {
        return false;
    }

    if (value != NULL) {
        *value = values[addr - start];
    }
    return true;
}

static uint32_t request_u32(uint16_t start,
                            const uint16_t *values,
                            uint16_t count,
                            uint16_t high_addr,
                            uint32_t current)
{
    uint16_t high = (uint16_t)(current >> 16U);
    uint16_t low = (uint16_t)(current & 0xFFFFU);

    (void)request_has_addr(start, count, high_addr, values, &high);
    (void)request_has_addr(start, count, (uint16_t)(high_addr + 1U), values, &low);

    return ((uint32_t)high << 16U) | low;
}

static bool is_segment_addr(uint16_t addr, uint8_t *segment, uint8_t *offset)
{
    uint16_t delta;
    uint8_t seg;
    uint8_t off;

    if ((addr < 0x1100U) || (addr > 0x1195U)) {
        return false;
    }

    delta = (uint16_t)(addr - 0x1100U);
    seg = (uint8_t)(delta / 0x10U);
    off = (uint8_t)(delta % 0x10U);
    if ((seg >= PLSR_MAX_SEGMENTS) || (off > 5U)) {
        return false;
    }

    if (segment != NULL) {
        *segment = seg;
    }
    if (offset != NULL) {
        *offset = off;
    }
    return true;
}

static bool is_holding_addr_valid(uint16_t addr)
{
    uint8_t segment;
    uint8_t offset;
    (void)segment;
    (void)offset;

    if ((addr >= 0x1000U) && (addr <= 0x100CU)) {
        return true;
    }
    if (is_segment_addr(addr, &segment, &offset)) {
        return true;
    }
    if ((addr >= 0x2000U) && (addr <= 0x200CU)) {
        return true;
    }
    if ((addr >= 0x3000U) && (addr <= 0x3003U)) {
        return true;
    }
    return false;
}

static bool is_holding_addr_writable(uint16_t addr)
{
    if ((addr >= 0x1000U) && (addr <= 0x100CU)) {
        return true;
    }
    if (is_segment_addr(addr, NULL, NULL)) {
        return true;
    }
    if ((addr >= 0x2000U) && (addr <= 0x2003U)) {
        return true;
    }
    if ((addr >= 0x3000U) && (addr <= 0x3003U)) {
        return true;
    }
    return false;
}

static PlsrResult precheck_write_request(uint16_t addr, const uint16_t *values, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        uint16_t now = (uint16_t)(addr + i);
        uint16_t value = values[i];

        if (!is_holding_addr_valid(now) || !is_holding_addr_writable(now)) {
            return PLSR_ERR_INVALID_VALUE;
        }

        if (((now == 0x2000U) || (now == 0x2001U)) && (value != 0U)) {
            return PLSR_ERR_INVALID_VALUE;
        }
        if ((now == 0x3000U) && !((value == 1U) || (value == 2U))) {
            return PLSR_ERR_INVALID_VALUE;
        }
        if (((now == 0x3001U) || (now == 0x3002U) || (now == 0x3003U)) &&
            (value != 1U)) {
            return PLSR_ERR_INVALID_VALUE;
        }
    }

    return PLSR_OK;
}

static void apply_scalar_config_write(PlsrConfig *candidate, uint16_t addr, uint16_t value)
{
    uint8_t segment;
    uint8_t offset;

    switch (addr) {
    case 0x1000:
        candidate->pulse_terminal = (uint8_t)value;
        break;
    case 0x1001:
        candidate->dir_terminal = (uint8_t)value;
        break;
    case 0x1002:
        candidate->ext_terminal = (uint8_t)value;
        break;
    case 0x1003:
        candidate->dir_delay_ms = value;
        break;
    case 0x1004:
        candidate->dir_positive_logic = (uint8_t)value;
        break;
    case 0x1005:
        candidate->accel_mode = (uint8_t)value;
        break;
    case 0x1006:
        candidate->run_mode = (uint8_t)value;
        break;
    case 0x1007:
        candidate->segment_count = (uint8_t)value;
        break;
    case 0x1008:
        candidate->start_segment = (uint8_t)value;
        break;
    case 0x100B:
        candidate->accel_time_ms = value;
        break;
    case 0x100C:
        candidate->decel_time_ms = value;
        break;
    default:
        if (is_segment_addr(addr, &segment, &offset)) {
            if (offset == 4U) {
                candidate->segments[segment].wait_condition = (uint8_t)value;
            } else if (offset == 5U) {
                candidate->segments[segment].jump_segment = (uint8_t)value;
            }
        }
        break;
    }
}

static void apply_u32_config_writes(PlsrConfig *candidate, uint16_t addr, const uint16_t *values, uint16_t count)
{
    uint32_t old_value;

    if (request_has_addr(addr, count, 0x1009U, values, NULL) ||
        request_has_addr(addr, count, 0x100AU, values, NULL)) {
        candidate->default_speed_hz = request_u32(addr, values, count, 0x1009U, candidate->default_speed_hz);
    }

    for (uint8_t i = 0; i < PLSR_MAX_SEGMENTS; i++) {
        uint16_t base = (uint16_t)(0x1100U + ((uint16_t)i * 0x10U));

        if (request_has_addr(addr, count, base, values, NULL) ||
            request_has_addr(addr, count, (uint16_t)(base + 1U), values, NULL)) {
            candidate->segments[i].freq_hz = request_u32(addr, values, count, base, candidate->segments[i].freq_hz);
        }

        if (request_has_addr(addr, count, (uint16_t)(base + 2U), values, NULL) ||
            request_has_addr(addr, count, (uint16_t)(base + 3U), values, NULL)) {
            old_value = (uint32_t)candidate->segments[i].pulse_count;
            candidate->segments[i].pulse_count = (int32_t)request_u32(addr, values, count, (uint16_t)(base + 2U), old_value);
        }
    }
}

static bool request_changes_config(uint16_t addr, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        uint16_t now = (uint16_t)(addr + i);
        if (((now >= 0x1000U) && (now <= 0x100CU)) ||
            is_segment_addr(now, NULL, NULL)) {
            return true;
        }
    }
    return false;
}

static bool request_changes_current_freq(uint16_t addr, uint16_t count, uint8_t current_segment)
{
    for (uint16_t i = 0; i < count; i++) {
        if (plsr_config_is_current_freq_addr((uint16_t)(addr + i), current_segment)) {
            return true;
        }
    }
    return false;
}

PlsrResult plsr_service_write_register(uint16_t addr, uint16_t value)
{
    return plsr_service_write_registers(addr, &value, 1);
}

PlsrResult plsr_service_write_registers(uint16_t addr, const uint16_t *values, uint16_t count)
{
    PlsrConfig candidate;
    PlsrStatus status;
    PlsrRecord record;
    PlsrResult result;
    bool config_changed;
    bool clear_distance = false;
    bool set_position = false;
    bool start_once = false;
    bool force_save = false;
    bool stop_current = false;
    int32_t new_position = 0;

    if ((values == NULL) || (count == 0U)) {
        return PLSR_ERR_INVALID_VALUE;
    }

    result = precheck_write_request(addr, values, count);
    if (result != PLSR_OK) {
        return result;
    }

    plsr_service_get_config(&candidate);

    for (uint16_t i = 0; i < count; i++) {
        uint16_t now = (uint16_t)(addr + i);
        uint16_t value = values[i];

        apply_scalar_config_write(&candidate, now, value);

        if ((now == 0x2000U) || (now == 0x2001U) || (now == 0x3001U)) {
            clear_distance = true;
        } else if ((now == 0x2002U) || (now == 0x2003U)) {
            set_position = true;
        } else if (now == 0x3000U) {
            if (value == 1U) {
                start_once = true;
            }
        } else if (now == 0x3002U) {
            force_save = true;
        } else if (now == 0x3003U) {
            stop_current = true;
        }
    }

    apply_u32_config_writes(&candidate, addr, values, count);

    if (set_position) {
        plsr_core_get_record(&s_core, &record);
        new_position = (int32_t)request_u32(addr, values, count, 0x2002U, low_u32_from_i64(record.position));
        if (!plsr_core_is_idle(&s_core)) {
            return PLSR_ERR_BUSY;
        }
    }

    config_changed = request_changes_config(addr, count);
    if (config_changed) {
        result = plsr_config_validate_for_start(&candidate);
        if (result != PLSR_OK) {
            return result;
        }
    }

    if (start_once && !plsr_core_is_idle(&s_core)) {
        return PLSR_ERR_BUSY;
    }

    if (config_changed) {
        PlsrIrqState irq_state = plsr_port_enter_critical();
        s_pending_config = candidate;
        plsr_port_exit_critical(irq_state);
        plsr_storage_mark_dirty();

        plsr_core_get_status(&s_core, &status);
        if ((status.current_segment > 0U) &&
            (status.current_segment <= PLSR_MAX_SEGMENTS) &&
            request_changes_current_freq(addr, count, status.current_segment)) {
            plsr_core_request_current_freq(&s_core,
                                           candidate.segments[status.current_segment - 1U].freq_hz);
        }
    }

    if (clear_distance) {
        (void)plsr_service_clear_total_distance();
    }

    if (set_position) {
        result = plsr_service_set_position((int64_t)new_position);
        if (result != PLSR_OK) {
            return result;
        }
    }

    if (force_save) {
        plsr_storage_mark_dirty();
        if (plsr_core_is_idle(&s_core)) {
            PlsrConfig save_config;
            PlsrRecord save_record;

            plsr_service_get_config(&save_config);
            plsr_core_get_record(&s_core, &save_record);
            (void)plsr_storage_save(&save_config, &save_record);
        }
    }

    if (stop_current) {
        result = plsr_core_stop(&s_core);
        if (result != PLSR_OK) {
            return result;
        }
    }

    if (start_once) {
        return plsr_service_start_once();
    }

    return PLSR_OK;
}

uint16_t plsr_service_read_register(uint16_t addr)
{
    PlsrConfig config;
    PlsrStatus status;
    uint8_t segment;
    uint8_t offset;
    uint32_t value32;

    plsr_service_get_config(&config);
    plsr_core_get_status(&s_core, &status);

    switch (addr) {
    case 0x1000:
        return config.pulse_terminal;
    case 0x1001:
        return config.dir_terminal;
    case 0x1002:
        return config.ext_terminal;
    case 0x1003:
        return config.dir_delay_ms;
    case 0x1004:
        return config.dir_positive_logic;
    case 0x1005:
        return config.accel_mode;
    case 0x1006:
        return config.run_mode;
    case 0x1007:
        return config.segment_count;
    case 0x1008:
        return config.start_segment;
    case 0x1009:
        return (uint16_t)(config.default_speed_hz >> 16U);
    case 0x100A:
        return (uint16_t)(config.default_speed_hz & 0xFFFFU);
    case 0x100B:
        return config.accel_time_ms;
    case 0x100C:
        return config.decel_time_ms;
    case 0x2000:
        value32 = low_u32_from_u64(status.total_distance);
        return (uint16_t)(value32 >> 16U);
    case 0x2001:
        value32 = low_u32_from_u64(status.total_distance);
        return (uint16_t)(value32 & 0xFFFFU);
    case 0x2002:
        value32 = low_u32_from_i64(status.position);
        return (uint16_t)(value32 >> 16U);
    case 0x2003:
        value32 = low_u32_from_i64(status.position);
        return (uint16_t)(value32 & 0xFFFFU);
    case 0x2004:
        return (uint16_t)status.state;
    case 0x2005:
        return (uint16_t)status.last_error;
    case 0x2006:
        return status.current_segment;
    case 0x2007:
        return (uint16_t)(status.current_segment_sent_pulses >> 16U);
    case 0x2008:
        return (uint16_t)(status.current_segment_sent_pulses & 0xFFFFU);
    case 0x2009:
        return (uint16_t)(status.current_freq_hz >> 16U);
    case 0x200A:
        return (uint16_t)(status.current_freq_hz & 0xFFFFU);
    case 0x200B:
        return (uint16_t)(status.job_sent_pulses >> 16U);
    case 0x200C:
        return (uint16_t)(status.job_sent_pulses & 0xFFFFU);
    case 0x3000:
    case 0x3001:
    case 0x3002:
    case 0x3003:
        return 0;
    default:
        break;
    }

    if (is_segment_addr(addr, &segment, &offset)) {
        const PlsrSegmentConfig *seg = &config.segments[segment];
        switch (offset) {
        case 0:
            return (uint16_t)(seg->freq_hz >> 16U);
        case 1:
            return (uint16_t)(seg->freq_hz & 0xFFFFU);
        case 2:
            return (uint16_t)(((uint32_t)seg->pulse_count) >> 16U);
        case 3:
            return (uint16_t)(((uint32_t)seg->pulse_count) & 0xFFFFU);
        case 4:
            return seg->wait_condition;
        case 5:
            return seg->jump_segment;
        default:
            break;
        }
    }

    return 0;
}

void plsr_service_get_status(PlsrStatus *status)
{
    plsr_core_get_status(&s_core, status);
}
