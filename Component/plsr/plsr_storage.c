#include "plsr_storage.h"
#include "plsr_config.h"
#include "plsr_port.h"
#include "flash_driver.h"

#define PLSR_STORAGE_MAGIC      0x504C5352UL
#define PLSR_STORAGE_VERSION    1U
#define PLSR_STORAGE_SAVE_DELAY_MS 200U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc32;
    PlsrConfig config;
    PlsrRecord record;
} PlsrStorageImage;

static bool s_storage_dirty = false;
static uint32_t s_dirty_tick_ms = 0;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            if ((crc & 1UL) != 0UL) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}

static uint32_t image_crc32(const PlsrStorageImage *image)
{
    uint32_t crc = 0;
    const uint8_t *bytes = (const uint8_t *)image;

    crc = crc32_update(crc, bytes, 8U);
    crc = crc32_update(crc, bytes + 12U, (uint32_t)sizeof(*image) - 12U);
    return crc;
}

void plsr_storage_set_defaults(PlsrConfig *config, PlsrRecord *record)
{
    if (config != NULL) {
        plsr_config_set_defaults(config);
    }
    if (record != NULL) {
        memset(record, 0, sizeof(*record));
    }
}

PlsrResult plsr_storage_load(PlsrConfig *config, PlsrRecord *record)
{
    PlsrStorageImage image;

    if ((config == NULL) || (record == NULL)) {
        return PLSR_ERR_INVALID_VALUE;
    }

    Flash_ReadData(MY_CONFIG_ADDR,
                   (uint32_t *)&image,
                   (uint16_t)((sizeof(image) + 3U) / 4U));

    if ((image.magic != PLSR_STORAGE_MAGIC) ||
        (image.version != PLSR_STORAGE_VERSION) ||
        (image.size != sizeof(image)) ||
        (image.crc32 != image_crc32(&image)) ||
        (plsr_config_validate_for_start(&image.config) != PLSR_OK)) {
        plsr_storage_set_defaults(config, record);
        return PLSR_ERR_STORAGE;
    }

    *config = image.config;
    *record = image.record;
    return PLSR_OK;
}

PlsrResult plsr_storage_save(const PlsrConfig *config, const PlsrRecord *record)
{
    PlsrStorageImage image;

    if ((config == NULL) || (record == NULL)) {
        return PLSR_ERR_INVALID_VALUE;
    }

    memset(&image, 0xFF, sizeof(image));
    image.magic = PLSR_STORAGE_MAGIC;
    image.version = PLSR_STORAGE_VERSION;
    image.size = sizeof(image);
    image.config = *config;
    image.record = *record;
    image.crc32 = image_crc32(&image);

    if (!Flash_ErasePage(MY_CONFIG_ADDR)) {
        return PLSR_ERR_STORAGE;
    }

    if (!Flash_WriteData(MY_CONFIG_ADDR,
                         (const uint32_t *)&image,
                         (uint16_t)((sizeof(image) + 3U) / 4U))) {
        return PLSR_ERR_STORAGE;
    }

    s_storage_dirty = false;
    return PLSR_OK;
}

void plsr_storage_mark_dirty(void)
{
    s_storage_dirty = true;
    s_dirty_tick_ms = plsr_port_get_tick_ms();
}

void plsr_storage_task_100ms(const PlsrConfig *config, const PlsrRecord *record, bool allow_write)
{
    uint32_t now;

    if (!s_storage_dirty || !allow_write) {
        return;
    }

    now = plsr_port_get_tick_ms();
    if ((uint32_t)(now - s_dirty_tick_ms) < PLSR_STORAGE_SAVE_DELAY_MS) {
        return;
    }

    (void)plsr_storage_save(config, record);
}
