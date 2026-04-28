#include "plsr_hw.h"
#include "plsr_port.h"

typedef struct {
    TIM_HandleTypeDef *pwm_tim;
    uint32_t pwm_channel;
    TIM_HandleTypeDef *cnt_tim;
    DMA_HandleTypeDef *stop_dma;
    volatile uint32_t stop_cr1_value;
    uint16_t last_cnt;
    uint32_t total_since_start;
    uint32_t target_abs_pulses;
    bool dma_armed;
    bool running;
    PlsrHwConfig config;
} PlsrHwChannel;

static PlsrHwChannel s_channels[PLSR_CHANNEL_COUNT] = {
    {
        &htim3,
        TIM_CHANNEL_1,
        &htim4,
        &hdma_tim4_ch1,
        TIM_CR1_ARPE,
        0,
        0,
        0,
        false,
        false,
        {0, 0, 0, 0, 0}
    }
};

static GPIO_TypeDef *const s_dir_ports[4] = {
    PLSR_Y12_DIR_GPIO_PORT,
    PLSR_Y13_DIR_GPIO_PORT,
    PLSR_Y14_DIR_GPIO_PORT,
    PLSR_Y15_DIR_GPIO_PORT
};

static const uint16_t s_dir_pins[4] = {
    PLSR_Y12_DIR_GPIO_PIN,
    PLSR_Y13_DIR_GPIO_PIN,
    PLSR_Y14_DIR_GPIO_PIN,
    PLSR_Y15_DIR_GPIO_PIN
};

static const uint16_t s_ext_pins[2] = {
    PLSR_X4_EXT_GPIO_PIN,
    PLSR_X5_EXT_GPIO_PIN
};

static bool is_channel_valid(uint8_t channel_id)
{
    return channel_id < PLSR_CHANNEL_COUNT;
}

static bool is_config_valid(const PlsrHwConfig *config)
{
    return (config != NULL) &&
           (config->channel_id < PLSR_CHANNEL_COUNT) &&
           (config->pulse_terminal <= 3U) &&
           (config->dir_terminal <= 3U) &&
           (config->ext_terminal <= 1U) &&
           (config->dir_positive_logic <= 1U);
}

static bool calc_timer_params(uint32_t freq_hz, uint16_t *psc, uint16_t *arr)
{
    uint32_t target_div;
    uint32_t prescaler_div;
    uint32_t period;

    if ((freq_hz < PLSR_HW_MIN_FREQ_HZ) ||
        (freq_hz > PLSR_HW_MAX_FREQ_HZ) ||
        (psc == NULL) ||
        (arr == NULL)) {
        return false;
    }

    target_div = PLSR_HW_TIMER_CLOCK_HZ / freq_hz;
    if (target_div < 2U) {
        target_div = 2U;
    }

    prescaler_div = (target_div + 65535UL) / 65536UL;
    if (prescaler_div == 0U) {
        prescaler_div = 1U;
    }
    if (prescaler_div > 65536UL) {
        return false;
    }

    period = target_div / prescaler_div;
    if (period < 2U) {
        period = 2U;
    }
    if (period > 65536UL) {
        period = 65536UL;
    }

    *psc = (uint16_t)(prescaler_div - 1U);
    *arr = (uint16_t)(period - 1U);
    return true;
}

static void dma_stop_disable(PlsrHwChannel *channel)
{
    __HAL_TIM_DISABLE_DMA(channel->cnt_tim, TIM_DMA_CC1);
    channel->stop_dma->Instance->CCR &= (uint16_t)(~DMA_CCR_EN);
    DMA1->IFCR = DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;
}

static void dma_stop_arm(PlsrHwChannel *channel)
{
    DMA_Channel_TypeDef *dma = channel->stop_dma->Instance;

    __HAL_TIM_DISABLE_DMA(channel->cnt_tim, TIM_DMA_CC1);
    dma->CCR &= (uint16_t)(~DMA_CCR_EN);
    DMA1->IFCR = DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;

    dma->CPAR = (uint32_t)&channel->pwm_tim->Instance->CR1;
    dma->CMAR = (uint32_t)&channel->stop_cr1_value;
    dma->CNDTR = 1U;
    dma->CCR = DMA_MEMORY_TO_PERIPH |
               DMA_PINC_DISABLE |
               DMA_MINC_DISABLE |
               DMA_PDATAALIGN_WORD |
               DMA_MDATAALIGN_WORD |
               DMA_NORMAL |
               DMA_PRIORITY_VERY_HIGH;
    dma->CCR |= DMA_CCR_EN;
    __HAL_TIM_ENABLE_DMA(channel->cnt_tim, TIM_DMA_CC1);
    channel->dma_armed = true;
}

static void stop_timers_only(PlsrHwChannel *channel)
{
    channel->pwm_tim->Instance->CR1 &= (uint16_t)(~TIM_CR1_CEN);
    channel->cnt_tim->Instance->CR1 &= (uint16_t)(~TIM_CR1_CEN);
    dma_stop_disable(channel);
    __HAL_TIM_CLEAR_FLAG(channel->cnt_tim, TIM_FLAG_CC1);
}

static uint32_t settle_counter_delta(PlsrHwChannel *channel)
{
    uint16_t current_cnt = (uint16_t)channel->cnt_tim->Instance->CNT;
    uint32_t delta = (uint16_t)(current_cnt - channel->last_cnt);
    uint32_t remaining;

    channel->last_cnt = current_cnt;
    remaining = (channel->total_since_start >= channel->target_abs_pulses) ?
                0U :
                (channel->target_abs_pulses - channel->total_since_start);

    if (delta > remaining) {
        delta = remaining;
    }

    channel->total_since_start += delta;
    return delta;
}

PlsrHwResult plsr_hw_init(uint8_t channel_id, const PlsrHwConfig *config)
{
    PlsrHwChannel *channel;

    if (!is_channel_valid(channel_id) || !is_config_valid(config)) {
        return PLSR_HW_ERR_PARAM;
    }

    channel = &s_channels[channel_id];
    channel->stop_cr1_value = TIM_CR1_ARPE;
    channel->running = false;
    channel->dma_armed = false;
    channel->last_cnt = 0;
    channel->total_since_start = 0;
    channel->target_abs_pulses = 0;
    channel->config = *config;

    stop_timers_only(channel);
    return PLSR_HW_OK;
}

PlsrHwResult plsr_hw_configure_terminals(uint8_t channel_id, const PlsrHwConfig *config)
{
    if (!is_channel_valid(channel_id) || !is_config_valid(config)) {
        return PLSR_HW_ERR_PARAM;
    }

    s_channels[channel_id].config = *config;
    return PLSR_HW_OK;
}

PlsrHwResult plsr_hw_set_direction(uint8_t channel_id, bool forward, bool positive_logic)
{
    PlsrHwChannel *channel;
    bool logic_on = forward;
    GPIO_PinState pin_state;

    if (!is_channel_valid(channel_id)) {
        return PLSR_HW_ERR_PARAM;
    }

    channel = &s_channels[channel_id];
    pin_state = (logic_on == positive_logic) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(s_dir_ports[channel->config.dir_terminal],
                      s_dir_pins[channel->config.dir_terminal],
                      pin_state);
    return PLSR_HW_OK;
}

PlsrHwResult plsr_hw_set_frequency(uint8_t channel_id, uint32_t freq_hz)
{
    PlsrHwChannel *channel;
    uint16_t psc;
    uint16_t arr;
    uint32_t ccr;
    bool running;
    PlsrIrqState irq_state;

    if (!is_channel_valid(channel_id) || !calc_timer_params(freq_hz, &psc, &arr)) {
        return PLSR_HW_ERR_PARAM;
    }

    channel = &s_channels[channel_id];
    ccr = ((uint32_t)arr + 1U) / 2U;
    if (ccr == 0U) {
        ccr = 1U;
    }

    irq_state = plsr_port_enter_critical();
    running = channel->running;
    channel->pwm_tim->Instance->PSC = psc;
    channel->pwm_tim->Instance->ARR = arr;
    channel->pwm_tim->Instance->CCR1 = ccr;
    if (!running) {
        channel->pwm_tim->Instance->EGR = TIM_EGR_UG;
    }
    plsr_port_exit_critical(irq_state);

    return PLSR_HW_OK;
}

PlsrHwResult plsr_hw_start(uint8_t channel_id, uint32_t target_abs_pulses)
{
    PlsrHwChannel *channel;
    PlsrIrqState irq_state;

    if (!is_channel_valid(channel_id) || (target_abs_pulses == 0U)) {
        return PLSR_HW_ERR_PARAM;
    }

    channel = &s_channels[channel_id];
    if (channel->running) {
        return PLSR_HW_ERR_BUSY;
    }

    irq_state = plsr_port_enter_critical();
    stop_timers_only(channel);

    channel->cnt_tim->Instance->CNT = 0U;
    channel->last_cnt = 0U;
    channel->total_since_start = 0U;
    channel->target_abs_pulses = target_abs_pulses;
    channel->dma_armed = false;

    __HAL_TIM_CLEAR_FLAG(channel->cnt_tim, TIM_FLAG_CC1);
    __HAL_TIM_CLEAR_FLAG(channel->pwm_tim, TIM_FLAG_UPDATE);

    if (target_abs_pulses <= PLSR_HW_STOP_ARM_WINDOW) {
        channel->cnt_tim->Instance->CCR1 = (uint16_t)target_abs_pulses;
        dma_stop_arm(channel);
    } else {
        channel->cnt_tim->Instance->CCR1 = 0xFFFFU;
    }

    channel->cnt_tim->Instance->CCER |= TIM_CCER_CC1E;
    channel->cnt_tim->Instance->CR1 |= TIM_CR1_CEN;
    channel->pwm_tim->Instance->CCER |= TIM_CCER_CC1E;
    channel->pwm_tim->Instance->CR1 |= TIM_CR1_ARPE;
    channel->pwm_tim->Instance->CR1 |= TIM_CR1_CEN;
    channel->running = true;
    plsr_port_exit_critical(irq_state);

    return PLSR_HW_OK;
}

PlsrHwResult plsr_hw_stop_now(uint8_t channel_id, PlsrHwPollResult *result)
{
    PlsrHwChannel *channel;
    PlsrIrqState irq_state;
    uint32_t delta = 0;

    if (!is_channel_valid(channel_id)) {
        return PLSR_HW_ERR_PARAM;
    }

    if (result != NULL) {
        result->delta_pulses = 0;
        result->pwm_stopped = 1U;
    }

    channel = &s_channels[channel_id];
    irq_state = plsr_port_enter_critical();
    if (channel->running) {
        delta = settle_counter_delta(channel);
    }
    stop_timers_only(channel);
    channel->running = false;
    channel->dma_armed = false;
    plsr_port_exit_critical(irq_state);

    if (result != NULL) {
        result->delta_pulses = delta;
        result->pwm_stopped = 1U;
    }

    return PLSR_HW_OK;
}

PlsrHwResult plsr_hw_poll(uint8_t channel_id, PlsrHwPollResult *result)
{
    PlsrHwChannel *channel;
    PlsrIrqState irq_state;
    uint32_t delta;
    uint32_t remaining;
    bool pwm_stopped;
    uint16_t stop_cnt;

    if (!is_channel_valid(channel_id) || (result == NULL)) {
        return PLSR_HW_ERR_PARAM;
    }

    result->delta_pulses = 0;
    result->pwm_stopped = 0U;
    channel = &s_channels[channel_id];

    irq_state = plsr_port_enter_critical();
    if (!channel->running) {
        result->pwm_stopped = ((channel->pwm_tim->Instance->CR1 & TIM_CR1_CEN) == 0U) ? 1U : 0U;
        plsr_port_exit_critical(irq_state);
        return PLSR_HW_OK;
    }

    delta = settle_counter_delta(channel);
    result->delta_pulses = delta;

    pwm_stopped = ((channel->pwm_tim->Instance->CR1 & TIM_CR1_CEN) == 0U);
    remaining = (channel->total_since_start >= channel->target_abs_pulses) ?
                0U :
                (channel->target_abs_pulses - channel->total_since_start);

    if (!channel->dma_armed && (remaining > 0U) && (remaining <= PLSR_HW_STOP_ARM_WINDOW)) {
        stop_cnt = (uint16_t)((uint16_t)channel->cnt_tim->Instance->CNT + (uint16_t)remaining);
        channel->cnt_tim->Instance->CCR1 = stop_cnt;
        __HAL_TIM_CLEAR_FLAG(channel->cnt_tim, TIM_FLAG_CC1);
        dma_stop_arm(channel);
    }

    if (pwm_stopped || (remaining == 0U)) {
        if (remaining == 0U) {
            channel->total_since_start = channel->target_abs_pulses;
        }
        stop_timers_only(channel);
        channel->running = false;
        channel->dma_armed = false;
        result->pwm_stopped = 1U;
    }
    plsr_port_exit_critical(irq_state);

    return PLSR_HW_OK;
}

bool plsr_hw_is_running(uint8_t channel_id)
{
    bool running;
    PlsrIrqState irq_state;

    if (!is_channel_valid(channel_id)) {
        return false;
    }

    irq_state = plsr_port_enter_critical();
    running = s_channels[channel_id].running;
    plsr_port_exit_critical(irq_state);
    return running;
}

bool plsr_hw_is_ext_pin(uint16_t gpio_pin)
{
    return (gpio_pin == s_ext_pins[0]) || (gpio_pin == s_ext_pins[1]);
}
