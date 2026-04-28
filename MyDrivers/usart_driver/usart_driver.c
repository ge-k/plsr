#include "usart_driver.h"

int my_printf(const char *format, ...)
{
    // 静态缓冲区，DMA发送期间不会被覆盖
    static char tx_buffer[256]; 
    // 固定检查 huart3 状态
    if(huart3.gState != HAL_UART_STATE_READY) {
        return 0; 
    }
    va_list arg;    
    int len;				 
    va_start(arg, format);
    len = vsnprintf(tx_buffer, sizeof(tx_buffer), format, arg);
    va_end(arg);
    // 固定用 huart3 DMA 发送
    HAL_UART_Transmit_DMA(&huart3, (uint8_t *)tx_buffer, (uint16_t)len);
    
    return len;
}

// usart1_rx的DMA
extern DMA_HandleTypeDef hdma_usart1_rx;
// usart1_rx的DMA的buffer
uint8_t hdma_usart1_rx_buffer[256];	
void UART_RxIdle_DMA_Init(void)
{
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1,hdma_usart1_rx_buffer,sizeof(hdma_usart1_rx_buffer));
  __HAL_DMA_DISABLE_IT(&hdma_usart1_rx,DMA_IT_HT);
}

	
// usart1_rx的用户的buffer
uint8_t usart1_rx_buffer[256];
// usart1_rx的用户的buffer有没有新数据
volatile uint8_t usart1_rx_flag = 0;
volatile uint16_t usart1_rx_len = 0;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        HAL_UART_DMAStop(huart);
        memcpy(usart1_rx_buffer, hdma_usart1_rx_buffer, Size);
				// 先准备好数据，最后再触发标志
				usart1_rx_len=Size;
        usart1_rx_flag = 1;
        memset(hdma_usart1_rx_buffer, 0, sizeof(hdma_usart1_rx_buffer));
        
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, hdma_usart1_rx_buffer, sizeof(hdma_usart1_rx_buffer));
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}

