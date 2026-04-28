#include "flash_driver.h"

/**
 * @brief  读取一个32位字
 * @param  address: 读取的起始绝对地址
 * @retval 读取到的32位数据
 */
uint32_t Flash_ReadWord(uint32_t address)
{
    // 直接通过指针解引用读取该地址的数据
    return *(__IO uint32_t*)address;
}

/**
 * @brief  连续读取多个32位数据
 * @param  address: 读取的起始地址
 * @param  pBuffer: 数据存储的缓冲数组指针
 * @param  length:  要读取的32位数据个数 (字数)
 */
void Flash_ReadData(uint32_t address, uint32_t *pBuffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        // 每次偏移 4 个字节 (32位)
        pBuffer[i] = *(__IO uint32_t*)(address + i * 4);
    }
}

/**
 * @brief  擦除指定地址所在的页 (1页 = 1KB)
 * @param  address: 要擦除的页内部的任意绝对地址
 * @retval true 成功, false 失败
 */
bool Flash_ErasePage(uint32_t address)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    // 1. 地址合法性校验
    if (address < STM32_FLASH_START_ADDR || address > STM32_FLASH_END_ADDR) {
        return false;
    }

    // 2. 解锁Flash
    HAL_FLASH_Unlock();

    // 3. 配置擦除参数
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = address;
    EraseInitStruct.NbPages     = 1;  // 仅擦除1页

    // 4. 执行擦除
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        HAL_FLASH_Lock(); // 发生错误时也要记得上锁
        return false;
    }

    // 5. 上锁Flash
    HAL_FLASH_Lock();
    
    return true;
}

/**
 * @brief  向指定地址写入一个32位字
 * @note   写入前该地址必须已被擦除（数据为0xFFFFFFFF）
 * @param  address: 写入的绝对地址 (必须4字节对齐)
 * @param  data: 要写入的数据
 * @retval true 成功, false 失败
 */
bool Flash_WriteWord(uint32_t address, uint32_t data)
{
    // 地址越界校验
    if (address < STM32_FLASH_START_ADDR || address > STM32_FLASH_END_ADDR) {
        return false;
    }

    HAL_FLASH_Unlock();

    // 写入数据 (按字写入)
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return false;
    }

    HAL_FLASH_Lock();
    return true;
}

/**
 * @brief  连续写入多个32位数据
 * @note   写入前该区域需已擦除
 * @param  address: 起始绝对地址 (4字节对齐)
 * @param  pData:   包含要写入数据的数组指针
 * @param  length:  要写入的字数 (32位为单位)
 * @retval true 成功, false 失败
 */
bool Flash_WriteData(uint32_t address, const uint32_t *pData, uint16_t length)
{
    // 地址和长度范围越界校验
    if (address < STM32_FLASH_START_ADDR || 
       (address + length * 4) > (STM32_FLASH_END_ADDR + 1)) 
    {
        return false;
    }

    HAL_FLASH_Unlock();

    // 循环写入
    for (uint16_t i = 0; i < length; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i * 4, pData[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();
    return true;
}



