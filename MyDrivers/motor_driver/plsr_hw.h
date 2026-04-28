#ifndef __PLSR_HW_H__
#define __PLSR_HW_H__

#include "plsr_board.h"

typedef enum {
    PLSR_HW_OK = 0,
    PLSR_HW_ERR_PARAM,
    PLSR_HW_ERR_BUSY,
    PLSR_HW_ERR_UNSUPPORTED
} PlsrHwResult;

typedef enum {
    PLSR_TERMINAL_Y0 = 0,
    PLSR_TERMINAL_Y1,
    PLSR_TERMINAL_Y2,
    PLSR_TERMINAL_Y3,
    PLSR_TERMINAL_Y12,
    PLSR_TERMINAL_Y13,
    PLSR_TERMINAL_Y14,
    PLSR_TERMINAL_Y15,
    PLSR_TERMINAL_X4,
    PLSR_TERMINAL_X5
} PlsrTerminal;

typedef struct {
    uint8_t channel_id;
    uint8_t pulse_terminal;
    uint8_t dir_terminal;
    uint8_t ext_terminal;
    uint8_t dir_positive_logic;
} PlsrHwConfig;

typedef struct {
    uint32_t delta_pulses;
    uint8_t pwm_stopped;
} PlsrHwPollResult;

PlsrHwResult plsr_hw_init(uint8_t channel_id, const PlsrHwConfig *config);
PlsrHwResult plsr_hw_configure_terminals(uint8_t channel_id, const PlsrHwConfig *config);
PlsrHwResult plsr_hw_set_direction(uint8_t channel_id, bool forward, bool positive_logic);
PlsrHwResult plsr_hw_set_frequency(uint8_t channel_id, uint32_t freq_hz);
PlsrHwResult plsr_hw_start(uint8_t channel_id, uint32_t target_abs_pulses);
PlsrHwResult plsr_hw_stop_now(uint8_t channel_id, PlsrHwPollResult *result);
PlsrHwResult plsr_hw_poll(uint8_t channel_id, PlsrHwPollResult *result);
bool plsr_hw_is_running(uint8_t channel_id);
bool plsr_hw_is_ext_pin(uint16_t gpio_pin);

#endif
