#ifndef __FLASH_DRIVER_H
#define __FLASH_DRIVER_H

#include "my_define.h"

/* STM32F103C8T6 内部Flash参数配置 */
#define STM32_FLASH_START_ADDR  0x08000000      // Flash 起始地址
#define STM32_FLASH_END_ADDR    0x0800FFFF      // Flash 结束地址 (64KB = 0x10000)
#define STM32_FLASH_PAGE_SIZE   1024            // 扇区/页 大小为 1KB

// 宏定义存储位置：放在最后一页的起始位置
#define MY_CONFIG_ADDR 0x0800FC00 

uint32_t Flash_ReadWord(uint32_t address);
void Flash_ReadData(uint32_t address, uint32_t *pBuffer, uint16_t length);
bool Flash_ErasePage(uint32_t address);
bool Flash_WriteWord(uint32_t address, uint32_t data);
bool Flash_WriteData(uint32_t address, const uint32_t *pData, uint16_t length);

#endif

