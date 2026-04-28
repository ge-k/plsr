#include "plsr_profile.h"

#define PLSR_PROFILE_Q16_ONE 65535UL

static const uint16_t s_sine_ease_q16[17] = {
    0U, 630U, 2494U, 5522U, 9598U, 14563U, 20228U, 26375U, 32768U,
    39160U, 45307U, 50972U, 55938U, 60013U, 63041U, 64905U, 65535U
};

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

static uint32_t ease_factor_q16(PlsrAccelMode mode, uint32_t elapsed_ms, uint32_t total_ms)
{
    uint32_t x_q16;

    if (total_ms == 0U) {
        return PLSR_PROFILE_Q16_ONE;
    }
    if (elapsed_ms >= total_ms) {
        return PLSR_PROFILE_Q16_ONE;
    }

    x_q16 = (uint32_t)(((uint64_t)elapsed_ms * PLSR_PROFILE_Q16_ONE) / total_ms);

    if (mode == PLSR_ACCEL_S_CURVE) {
        const uint32_t half = PLSR_PROFILE_Q16_ONE / 2U;

        if (x_q16 < half) {
            return (uint32_t)((2ULL * x_q16 * x_q16) / PLSR_PROFILE_Q16_ONE);
        } else {
            uint32_t dt = x_q16 - half;
            uint32_t factor = half + (2U * dt) -
                              (uint32_t)((2ULL * dt * dt) / PLSR_PROFILE_Q16_ONE);

            return (factor > PLSR_PROFILE_Q16_ONE) ? PLSR_PROFILE_Q16_ONE : factor;
        }
    }

    if (mode == PLSR_ACCEL_SINE) {
        uint32_t scaled = x_q16 * 16U;
        uint32_t index = scaled / PLSR_PROFILE_Q16_ONE;
        uint32_t frac = scaled % PLSR_PROFILE_Q16_ONE;
        uint32_t y0;
        uint32_t y1;

        if (index >= 16U) {
            return PLSR_PROFILE_Q16_ONE;
        }

        y0 = s_sine_ease_q16[index];
        y1 = s_sine_ease_q16[index + 1U];
        return y0 + (uint32_t)(((uint64_t)(y1 - y0) * frac) / PLSR_PROFILE_Q16_ONE);
    }

    return x_q16;
}

static uint32_t interp_freq(uint32_t from_hz, uint32_t to_hz, uint32_t factor_q16)
{
    int64_t delta = (int64_t)to_hz - (int64_t)from_hz;
    int64_t value = (int64_t)from_hz + ((delta * (int64_t)factor_q16) / (int64_t)PLSR_PROFILE_Q16_ONE);

    if (value < (int64_t)PLSR_FREQ_MIN_HZ) {
        return PLSR_FREQ_MIN_HZ;
    }
    if (value > (int64_t)PLSR_FREQ_MAX_HZ) {
        return PLSR_FREQ_MAX_HZ;
    }

    return (uint32_t)value;
}

void plsr_profile_build(PlsrProfilePlan *plan,
                        PlsrAccelMode mode,
                        uint32_t start_freq_hz,
                        uint32_t target_freq_hz,
                        uint32_t accel_time_ms,
                        uint32_t decel_time_ms)
{
    if (plan == NULL) {
        return;
    }

    memset(plan, 0, sizeof(*plan));
    plan->mode = mode;
    plan->start_freq_hz = clamp_freq(start_freq_hz);
    plan->target_freq_hz = clamp_freq(target_freq_hz);
    if (plan->target_freq_hz == plan->start_freq_hz) {
        plan->ramp_time_ms = 0;
    } else if (plan->target_freq_hz > plan->start_freq_hz) {
        plan->ramp_time_ms = accel_time_ms;
    } else {
        plan->ramp_time_ms = decel_time_ms;
    }
}

uint32_t plsr_profile_eval(const PlsrProfilePlan *plan, uint32_t elapsed_ms)
{
    uint32_t factor;

    if (plan == NULL) {
        return PLSR_FREQ_MIN_HZ;
    }

    if ((plan->ramp_time_ms == 0U) || (elapsed_ms >= plan->ramp_time_ms)) {
        return clamp_freq(plan->target_freq_hz);
    }

    factor = ease_factor_q16(plan->mode, elapsed_ms, plan->ramp_time_ms);
    return interp_freq(plan->start_freq_hz, plan->target_freq_hz, factor);
}
