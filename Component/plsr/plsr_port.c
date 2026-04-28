#include "plsr_port.h"

uint32_t plsr_port_get_tick_ms(void)
{
    return HAL_GetTick();
}

PlsrIrqState plsr_port_enter_critical(void)
{
    PlsrIrqState state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void plsr_port_exit_critical(PlsrIrqState state)
{
    __set_PRIMASK(state);
}
