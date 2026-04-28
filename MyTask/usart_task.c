#include "usart_task.h"
#include "agile_modbus.h"
#include "agile_modbus_slave_util.h"
#include "modbus_app.h"
#include "plsr_types.h"

static agile_modbus_rtu_t modbus_rtu;
static uint8_t modbus_send_buf[AGILE_MODBUS_RTU_MAX_ADU_LENGTH];
static uint8_t modbus_read_buf[AGILE_MODBUS_RTU_MAX_ADU_LENGTH];

static agile_modbus_t *modbus_ctx(void)
{
    return &modbus_rtu._ctx;
}

static void modbus_rtu_slave_init(void)
{
    agile_modbus_rtu_init(&modbus_rtu,
                          modbus_send_buf,
                          sizeof(modbus_send_buf),
                          modbus_read_buf,
                          sizeof(modbus_read_buf));
    agile_modbus_set_slave(modbus_ctx(), PLSR_DEFAULT_SLAVE_ID);
}

void my_usart1_init(void)
{
    modbus_app_init();
    modbus_rtu_slave_init();
    UART_RxIdle_DMA_Init();
}

void usart1_task(void)
{
    int send_len;

    if (usart1_rx_flag == 0U) {
        return;
    }
    if (huart1.gState != HAL_UART_STATE_READY) {
        return;
    }

    usart1_rx_flag = 0;
    if ((usart1_rx_len == 0U) || (usart1_rx_len > sizeof(modbus_read_buf))) {
        usart1_rx_len = 0;
        memset(usart1_rx_buffer, 0, sizeof(usart1_rx_buffer));
        return;
    }

    memcpy(modbus_ctx()->read_buf, usart1_rx_buffer, usart1_rx_len);
    memset(usart1_rx_buffer, 0, sizeof(usart1_rx_buffer));

    send_len = agile_modbus_slave_handle(modbus_ctx(),
                                         usart1_rx_len,
                                         0,
                                         agile_modbus_slave_util_callback,
                                         modbus_app_get_slave_util(),
                                         NULL);
    usart1_rx_len = 0;

    if (send_len > 0) {
        HAL_UART_Transmit_DMA(&huart1, modbus_ctx()->send_buf, send_len);
    }
}
