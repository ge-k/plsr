#ifndef __PLSR_BOARD_H__
#define __PLSR_BOARD_H__

#include "my_define.h"

#define PLSR_CHANNEL_COUNT                 1U

#define PLSR_HW_TIMER_CLOCK_HZ             72000000UL
#define PLSR_HW_MAX_FREQ_HZ                100000UL
#define PLSR_HW_MIN_FREQ_HZ                1UL
#define PLSR_HW_STOP_ARM_WINDOW            5000UL

#define PLSR_Y0_PULSE_GPIO_PORT            GPIOA
#define PLSR_Y0_PULSE_GPIO_PIN             GPIO_PIN_6
#define PLSR_Y1_PULSE_GPIO_PORT            GPIOA
#define PLSR_Y1_PULSE_GPIO_PIN             GPIO_PIN_6
#define PLSR_Y2_PULSE_GPIO_PORT            GPIOA
#define PLSR_Y2_PULSE_GPIO_PIN             GPIO_PIN_6
#define PLSR_Y3_PULSE_GPIO_PORT            GPIOA
#define PLSR_Y3_PULSE_GPIO_PIN             GPIO_PIN_6

#define PLSR_Y12_DIR_GPIO_PORT             GPIOA
#define PLSR_Y12_DIR_GPIO_PIN              GPIO_PIN_7
#define PLSR_Y13_DIR_GPIO_PORT             GPIOA
#define PLSR_Y13_DIR_GPIO_PIN              GPIO_PIN_7
#define PLSR_Y14_DIR_GPIO_PORT             GPIOA
#define PLSR_Y14_DIR_GPIO_PIN              GPIO_PIN_7
#define PLSR_Y15_DIR_GPIO_PORT             GPIOA
#define PLSR_Y15_DIR_GPIO_PIN              GPIO_PIN_7

#define PLSR_X4_EXT_GPIO_PORT              GPIOA
#define PLSR_X4_EXT_GPIO_PIN               GPIO_PIN_5
#define PLSR_X4_EXT_IRQN                   EXTI9_5_IRQn
#define PLSR_X5_EXT_GPIO_PORT              GPIOA
#define PLSR_X5_EXT_GPIO_PIN               GPIO_PIN_5
#define PLSR_X5_EXT_IRQN                   EXTI9_5_IRQn

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern DMA_HandleTypeDef hdma_tim4_ch1;

#endif
