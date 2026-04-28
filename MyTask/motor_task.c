#include "motor_task.h"
#include "plsr_service.h"

void motor_task_init(void)
{
    plsr_service_init();
}

void motor_task_1ms(void)
{
    plsr_service_task_1ms();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    plsr_service_on_exti(GPIO_Pin);
}
