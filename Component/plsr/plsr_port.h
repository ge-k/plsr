#ifndef __PLSR_PORT_H__
#define __PLSR_PORT_H__

#include "my_define.h"

typedef uint32_t PlsrIrqState;

uint32_t plsr_port_get_tick_ms(void);
PlsrIrqState plsr_port_enter_critical(void);
void plsr_port_exit_critical(PlsrIrqState state);

#endif
