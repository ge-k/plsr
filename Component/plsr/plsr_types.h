#ifndef __PLSR_TYPES_H__
#define __PLSR_TYPES_H__

#include "my_define.h"

#define PLSR_MAX_CHANNELS        1U
#define PLSR_MAX_SEGMENTS        10U
#define PLSR_FREQ_MIN_HZ         1UL
#define PLSR_FREQ_MAX_HZ         100000UL
#define PLSR_DEFAULT_SLAVE_ID    1U

typedef enum {
    PLSR_ACCEL_LINEAR = 0,
    PLSR_ACCEL_S_CURVE = 1,
    PLSR_ACCEL_SINE = 2
} PlsrAccelMode;

typedef enum {
    PLSR_RUN_RELATIVE = 0,
    PLSR_RUN_ABSOLUTE = 1
} PlsrRunMode;

typedef enum {
    PLSR_WAIT_PULSE_DONE = 0,
    PLSR_WAIT_EXT_RISING = 1
} PlsrWaitCondition;

typedef enum {
    PLSR_STATE_IDLE = 0,
    PLSR_STATE_DIR_DELAY,
    PLSR_STATE_RUNNING,
    PLSR_STATE_WAIT_EXT,
    PLSR_STATE_DONE,
    PLSR_STATE_ERROR
} PlsrState;

typedef enum {
    PLSR_OK = 0,
    PLSR_ERR_BUSY,
    PLSR_ERR_INVALID_FREQ,
    PLSR_ERR_INVALID_TOTAL_PULSES,
    PLSR_ERR_INVALID_SEGMENT_COUNT,
    PLSR_ERR_INVALID_START_SEGMENT,
    PLSR_ERR_INVALID_JUMP,
    PLSR_ERR_INVALID_VALUE,
    PLSR_ERR_HW,
    PLSR_ERR_STORAGE
} PlsrResult;

typedef struct {
    uint32_t freq_hz;
    int32_t pulse_count;
    uint8_t wait_condition;
    uint8_t jump_segment;
} PlsrSegmentConfig;

typedef struct {
    uint8_t pulse_terminal;
    uint8_t dir_terminal;
    uint8_t ext_terminal;
    uint16_t dir_delay_ms;
    uint8_t dir_positive_logic;
    uint8_t accel_mode;
    uint8_t run_mode;
    uint8_t segment_count;
    uint8_t start_segment;
    uint32_t default_speed_hz;
    uint16_t accel_time_ms;
    uint16_t decel_time_ms;
    uint32_t reserved;
    PlsrSegmentConfig segments[PLSR_MAX_SEGMENTS];
} PlsrConfig;

typedef struct {
    uint64_t total_distance;
    int64_t position;
} PlsrRecord;

typedef struct {
    PlsrState state;
    PlsrResult last_error;
    uint8_t current_segment;
    uint32_t current_freq_hz;
    uint32_t job_sent_pulses;
    uint32_t current_segment_target_pulses;
    uint32_t current_segment_sent_pulses;
    uint64_t total_distance;
    int64_t position;
} PlsrStatus;

#endif
