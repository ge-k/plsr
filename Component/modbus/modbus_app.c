#include "modbus_app.h"
#include "plsr_service.h"

static bool is_segment_addr(uint16_t addr)
{
    uint16_t delta;
    uint8_t segment;
    uint8_t offset;

    if ((addr < 0x1100U) || (addr > 0x1195U)) {
        return false;
    }

    delta = (uint16_t)(addr - 0x1100U);
    segment = (uint8_t)(delta / 0x10U);
    offset = (uint8_t)(delta % 0x10U);
    return (segment < PLSR_MAX_SEGMENTS) && (offset <= 5U);
}

static bool is_holding_addr_valid(uint16_t addr)
{
    if ((addr >= 0x1000U) && (addr <= 0x100CU)) {
        return true;
    }
    if (is_segment_addr(addr)) {
        return true;
    }
    if ((addr >= 0x2000U) && (addr <= 0x200CU)) {
        return true;
    }
    if ((addr >= 0x3000U) && (addr <= 0x3003U)) {
        return true;
    }
    return false;
}

static bool is_holding_addr_writable(uint16_t addr)
{
    if ((addr >= 0x1000U) && (addr <= 0x100CU)) {
        return true;
    }
    if (is_segment_addr(addr)) {
        return true;
    }
    if ((addr >= 0x2000U) && (addr <= 0x2003U)) {
        return true;
    }
    if ((addr >= 0x3000U) && (addr <= 0x3003U)) {
        return true;
    }
    return false;
}

static int plsr_result_to_modbus(PlsrResult result)
{
    if (result == PLSR_OK) {
        return 0;
    }
    if (result == PLSR_ERR_BUSY) {
        return -AGILE_MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY;
    }
    return -AGILE_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
}

static int get_register_block(uint16_t start_addr, uint16_t count, void *buf, int bufsz)
{
    uint16_t *registers = (uint16_t *)buf;

    if ((buf == NULL) || (bufsz < (int)(count * sizeof(uint16_t)))) {
        return -AGILE_MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE;
    }

    for (uint16_t i = 0; i < count; i++) {
        registers[i] = plsr_service_read_register((uint16_t)(start_addr + i));
    }

    return 0;
}

static int set_register_block(uint16_t start_addr, int index, int len, void *buf, int bufsz)
{
    uint16_t *registers = (uint16_t *)buf;
    PlsrResult result;

    if ((buf == NULL) ||
        (index < 0) ||
        (len <= 0) ||
        (bufsz < (int)((index + len) * sizeof(uint16_t)))) {
        return -AGILE_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    result = plsr_service_write_registers((uint16_t)(start_addr + (uint16_t)index),
                                          &registers[index],
                                          (uint16_t)len);
    return plsr_result_to_modbus(result);
}

static int system_get(void *buf, int bufsz)
{
    return get_register_block(0x1000U, 13U, buf, bufsz);
}

static int system_set(int index, int len, void *buf, int bufsz)
{
    return set_register_block(0x1000U, index, len, buf, bufsz);
}

static int status_get(void *buf, int bufsz)
{
    return get_register_block(0x2000U, 13U, buf, bufsz);
}

static int status_set(int index, int len, void *buf, int bufsz)
{
    return set_register_block(0x2000U, index, len, buf, bufsz);
}

static int command_get(void *buf, int bufsz)
{
    return get_register_block(0x3000U, 4U, buf, bufsz);
}

static int command_set(int index, int len, void *buf, int bufsz)
{
    return set_register_block(0x3000U, index, len, buf, bufsz);
}

#define DEFINE_SEGMENT_ACCESS(idx, base_addr) \
static int segment_##idx##_get(void *buf, int bufsz) \
{ \
    return get_register_block((base_addr), 6U, buf, bufsz); \
} \
static int segment_##idx##_set(int index, int len, void *buf, int bufsz) \
{ \
    return set_register_block((base_addr), index, len, buf, bufsz); \
}

DEFINE_SEGMENT_ACCESS(1, 0x1100U)
DEFINE_SEGMENT_ACCESS(2, 0x1110U)
DEFINE_SEGMENT_ACCESS(3, 0x1120U)
DEFINE_SEGMENT_ACCESS(4, 0x1130U)
DEFINE_SEGMENT_ACCESS(5, 0x1140U)
DEFINE_SEGMENT_ACCESS(6, 0x1150U)
DEFINE_SEGMENT_ACCESS(7, 0x1160U)
DEFINE_SEGMENT_ACCESS(8, 0x1170U)
DEFINE_SEGMENT_ACCESS(9, 0x1180U)
DEFINE_SEGMENT_ACCESS(10, 0x1190U)

static int modbus_app_addr_check(agile_modbus_t *ctx, struct agile_modbus_slave_info *slave_info)
{
    int slave;
    int function;
    uint16_t address;
    uint16_t nb;
    bool is_write = false;

    if ((ctx == NULL) || (slave_info == NULL) || (slave_info->sft == NULL)) {
        return -AGILE_MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE;
    }

    slave = slave_info->sft->slave;
    if ((slave != ctx->slave) &&
        (slave != AGILE_MODBUS_BROADCAST_ADDRESS) &&
        (slave != 0xFF)) {
        return -AGILE_MODBUS_EXCEPTION_UNKNOW;
    }

    function = slave_info->sft->function;
    address = (uint16_t)slave_info->address;

    switch (function) {
    case AGILE_MODBUS_FC_READ_HOLDING_REGISTERS:
        nb = (uint16_t)slave_info->nb;
        break;

    case AGILE_MODBUS_FC_WRITE_SINGLE_REGISTER:
        nb = 1U;
        is_write = true;
        break;

    case AGILE_MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        nb = (uint16_t)slave_info->nb;
        is_write = true;
        break;

    default:
        return -AGILE_MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    }

    if (nb == 0U) {
        return -AGILE_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    for (uint16_t i = 0; i < nb; i++) {
        uint16_t now = (uint16_t)(address + i);
        if (!is_holding_addr_valid(now)) {
            return -AGILE_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        }
        if (is_write && !is_holding_addr_writable(now)) {
            return -AGILE_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        }
    }

    return 0;
}

static const agile_modbus_slave_util_map_t s_holding_maps[] = {
    {0x1000, 0x100C, system_get, system_set},
    {0x1100, 0x1105, segment_1_get, segment_1_set},
    {0x1110, 0x1115, segment_2_get, segment_2_set},
    {0x1120, 0x1125, segment_3_get, segment_3_set},
    {0x1130, 0x1135, segment_4_get, segment_4_set},
    {0x1140, 0x1145, segment_5_get, segment_5_set},
    {0x1150, 0x1155, segment_6_get, segment_6_set},
    {0x1160, 0x1165, segment_7_get, segment_7_set},
    {0x1170, 0x1175, segment_8_get, segment_8_set},
    {0x1180, 0x1185, segment_9_get, segment_9_set},
    {0x1190, 0x1195, segment_10_get, segment_10_set},
    {0x2000, 0x200C, status_get, status_set},
    {0x3000, 0x3003, command_get, command_set}
};

static const agile_modbus_slave_util_t s_slave_util = {
    NULL,
    0,
    NULL,
    0,
    s_holding_maps,
    sizeof(s_holding_maps) / sizeof(s_holding_maps[0]),
    NULL,
    0,
    modbus_app_addr_check,
    NULL,
    NULL
};

void modbus_app_init(void)
{
}

const agile_modbus_slave_util_t *modbus_app_get_slave_util(void)
{
    return &s_slave_util;
}
