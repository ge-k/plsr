#ifndef __PLSR_CORE_H__
#define __PLSR_CORE_H__

#include "plsr_types.h"
#include "plsr_profile.h"

typedef struct PlsrCore PlsrCore;

struct PlsrCore {
    PlsrConfig active_config;
    PlsrRecord record;
    PlsrState state;
    PlsrResult last_error;

    uint8_t current_segment;
    uint32_t job_sent_pulses;
    uint8_t no_progress_jumps;

    int8_t current_dir_sign;
    bool dir_sign_valid;
    uint32_t target_abs_pulses;
    uint32_t segment_sent_pulses;
    uint32_t dir_set_tick;
    uint32_t segment_start_tick;
    uint32_t current_freq_hz;
    uint32_t next_start_freq_hz;

    bool ext_latched;
    uint8_t ext_stop_latched;
    bool freq_request_pending;
    uint32_t requested_freq_hz;
    PlsrProfilePlan profile;
};

void plsr_core_init(PlsrCore *core);
PlsrResult plsr_core_start(PlsrCore *core, const PlsrConfig *config, const PlsrRecord *record);
PlsrResult plsr_core_stop(PlsrCore *core);
void plsr_core_task_1ms(PlsrCore *core);
void plsr_core_on_ext_rising(PlsrCore *core);
void plsr_core_request_current_freq(PlsrCore *core, uint32_t freq_hz);
void plsr_core_get_status(const PlsrCore *core, PlsrStatus *status);
void plsr_core_set_record(PlsrCore *core, const PlsrRecord *record);
void plsr_core_get_record(const PlsrCore *core, PlsrRecord *record);
bool plsr_core_is_idle(const PlsrCore *core);

#endif
