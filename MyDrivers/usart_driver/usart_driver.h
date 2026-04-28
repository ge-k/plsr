#ifndef __USART_DRIVER_H__
#define __USART_DRIVER_H__

#include "my_define.h"

extern uint8_t usart1_rx_buffer[256];
extern volatile uint8_t usart1_rx_flag;
extern volatile uint16_t usart1_rx_len;

void UART_RxIdle_DMA_Init(void);


#endif
