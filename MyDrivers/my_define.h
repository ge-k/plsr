#ifndef __MY_DEFINE_H__
#define __MY_DEFINE_H__

// C库
#include "string.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

// STM32外设
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

int my_printf(const char *format, ...);


#define AGILE_MODBUS_USING_RTU 1
#define AGILE_MODBUS_USING_TCP 0


#endif
