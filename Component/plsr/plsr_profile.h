#ifndef __PLSR_PROFILE_H__
#define __PLSR_PROFILE_H__

#include "plsr_types.h"

typedef struct {
    PlsrAccelMode mode;
    uint32_t start_freq_hz;
    uint32_t target_freq_hz;
    uint32_t ramp_time_ms;
} PlsrProfilePlan;

void plsr_profile_build(PlsrProfilePlan *plan,
                        PlsrAccelMode mode,
                        uint32_t start_freq_hz,
                        uint32_t target_freq_hz,
                        uint32_t accel_time_ms,
                        uint32_t decel_time_ms);

uint32_t plsr_profile_eval(const PlsrProfilePlan *plan, uint32_t elapsed_ms);

#endif
