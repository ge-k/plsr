#ifndef __MODBUS_APP_H__
#define __MODBUS_APP_H__

#include "my_define.h"
#include "agile_modbus.h"
#include "agile_modbus_slave_util.h"

void modbus_app_init(void);
const agile_modbus_slave_util_t *modbus_app_get_slave_util(void);

#endif
