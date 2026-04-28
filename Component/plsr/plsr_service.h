#ifndef __PLSR_SERVICE_H__
#define __PLSR_SERVICE_H__

#include "plsr_types.h"

void plsr_service_init(void);
void plsr_service_task_1ms(void);
void plsr_service_storage_task_100ms(void);
void plsr_service_on_exti(uint16_t gpio_pin);
uint32_t plsr_time_ms(void);

PlsrResult plsr_service_start_once(void);
PlsrResult plsr_service_clear_total_distance(void);
PlsrResult plsr_service_set_position(int64_t position);

void plsr_service_get_config(PlsrConfig *config);
PlsrResult plsr_service_set_config(const PlsrConfig *config);
PlsrResult plsr_service_write_register(uint16_t addr, uint16_t value);
PlsrResult plsr_service_write_registers(uint16_t addr, const uint16_t *values, uint16_t count);
uint16_t plsr_service_read_register(uint16_t addr);

void plsr_service_get_status(PlsrStatus *status);

#endif
