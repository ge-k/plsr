#ifndef __PLSR_CONFIG_H__
#define __PLSR_CONFIG_H__

#include "plsr_types.h"

void plsr_config_set_defaults(PlsrConfig *config);
PlsrResult plsr_config_validate_for_start(const PlsrConfig *config);
uint8_t plsr_config_next_segment(const PlsrConfig *config, uint8_t current_segment);
bool plsr_config_is_current_freq_addr(uint16_t modbus_addr, uint8_t current_segment);

#endif
