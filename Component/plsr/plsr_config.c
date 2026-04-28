#include "plsr_config.h"

static bool is_freq_valid(uint32_t freq_hz)
{
    return (freq_hz >= PLSR_FREQ_MIN_HZ) && (freq_hz <= PLSR_FREQ_MAX_HZ);
}

void plsr_config_set_defaults(PlsrConfig *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->pulse_terminal = 0;
    config->dir_terminal = 0;
    config->ext_terminal = 0;
    config->dir_delay_ms = 0;
    config->dir_positive_logic = 0;
    config->accel_mode = PLSR_ACCEL_LINEAR;
    config->run_mode = PLSR_RUN_RELATIVE;
    config->segment_count = 1;
    config->start_segment = 1;
    config->default_speed_hz = 1;
    config->accel_time_ms = 0;
    config->decel_time_ms = 0;

    config->segments[0].freq_hz = 1000;
    config->segments[0].pulse_count = 0;
    config->segments[0].wait_condition = PLSR_WAIT_PULSE_DONE;
    config->segments[0].jump_segment = 0;
}

uint8_t plsr_config_next_segment(const PlsrConfig *config, uint8_t current_segment)
{
    uint8_t next;

    if ((config == NULL) ||
        (current_segment == 0) ||
        (current_segment > config->segment_count)) {
        return 0;
    }

    next = config->segments[current_segment - 1U].jump_segment;
    if (next != 0U) {
        return next;
    }

    if (current_segment >= config->segment_count) {
        return 0;
    }

    return (uint8_t)(current_segment + 1U);
}

static PlsrResult validate_basic_fields(const PlsrConfig *config)
{
    if (config == NULL) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (config->pulse_terminal > 3U) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (config->dir_terminal > 3U) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (config->ext_terminal > 1U) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (config->dir_positive_logic > 1U) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (config->accel_mode > PLSR_ACCEL_SINE) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if (config->run_mode > PLSR_RUN_ABSOLUTE) {
        return PLSR_ERR_INVALID_VALUE;
    }
    if ((config->segment_count == 0U) ||
        (config->segment_count > PLSR_MAX_SEGMENTS)) {
        return PLSR_ERR_INVALID_SEGMENT_COUNT;
    }
    if ((config->start_segment == 0U) ||
        (config->start_segment > config->segment_count)) {
        return PLSR_ERR_INVALID_START_SEGMENT;
    }
    if (!is_freq_valid(config->default_speed_hz)) {
        return PLSR_ERR_INVALID_FREQ;
    }

    return PLSR_OK;
}

static PlsrResult validate_segments(const PlsrConfig *config)
{
    for (uint8_t i = 0; i < config->segment_count; i++) {
        const PlsrSegmentConfig *segment = &config->segments[i];

        if (!is_freq_valid(segment->freq_hz)) {
            return PLSR_ERR_INVALID_FREQ;
        }
        if (segment->wait_condition > PLSR_WAIT_EXT_RISING) {
            return PLSR_ERR_INVALID_VALUE;
        }
        if (segment->jump_segment > config->segment_count) {
            return PLSR_ERR_INVALID_JUMP;
        }
    }

    return PLSR_OK;
}

PlsrResult plsr_config_validate_for_start(const PlsrConfig *config)
{
    PlsrResult result = validate_basic_fields(config);
    if (result != PLSR_OK) {
        return result;
    }

    result = validate_segments(config);
    if (result != PLSR_OK) {
        return result;
    }

    return PLSR_OK;
}

bool plsr_config_is_current_freq_addr(uint16_t modbus_addr, uint8_t current_segment)
{
    uint16_t base;

    if ((current_segment == 0U) || (current_segment > PLSR_MAX_SEGMENTS)) {
        return false;
    }

    base = (uint16_t)(0x1100U + ((uint16_t)(current_segment - 1U) * 0x10U));
    return (modbus_addr == base) || (modbus_addr == (uint16_t)(base + 1U));
}
