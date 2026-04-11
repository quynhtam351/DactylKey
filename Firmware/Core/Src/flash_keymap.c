#include "flash_keymap.h"
#include "default_keymap.h"
#include "crc16.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Module state
 * --------------------------------------------------------------------- */
static KeymapStagingBuffer_t s_staging;

/*
 * s_active_left / s_active_right trỏ vào:
 *   - default keymap (RAM, const) khi Flash không hợp lệ
 *   - s_flash_copy   (RAM)        khi Flash hợp lệ
 *
 * Fix unaligned warning: KHÔNG trỏ thẳng vào Flash struct packed.
 * Thay vào đó, copy ra RAM buffer và trỏ vào RAM.
 */
static keycode_t s_flash_copy_left [NUM_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
static keycode_t s_flash_copy_right[NUM_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];

static const keycode_t *s_active_left [NUM_KEYMAP_LAYERS];
static const keycode_t *s_active_right[NUM_KEYMAP_LAYERS];

/* -----------------------------------------------------------------------
 * Flash HAL helpers
 * --------------------------------------------------------------------- */

static bool _Flash_Unlock(void)
{
    return (HAL_FLASH_Unlock() == HAL_OK);
}

static void _Flash_Lock(void)
{
    HAL_FLASH_Lock();
}

static bool _Flash_EraseSector5(void)
{
    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        .Sector       = FLASH_SECTOR_5,
        .NbSectors    = 1U,
    };
    uint32_t sector_error = 0U;
    return (HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK);
}

static bool _Flash_WriteBuffer(uint32_t addr,
                                const uint8_t *data, uint32_t size)
{
    for (uint32_t i = 0U; i < size; i += 4U) {
        uint32_t word;
        memcpy(&word, data + i, 4U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              addr + i, word) != HAL_OK) {
            return false;
        }
    }
    return true;
}

/* -----------------------------------------------------------------------
 * CRC — tính từ field 'flags' đến hết struct
 * --------------------------------------------------------------------- */

static uint16_t _CalcHeaderCRC(const FlashKeymapHeader_t *h)
{
    const uint8_t *start =
        (const uint8_t *)h + offsetof(FlashKeymapHeader_t, flags);
    size_t len =
        sizeof(FlashKeymapHeader_t) - offsetof(FlashKeymapHeader_t, flags);
    return CRC16_Calculate(start, len);
}

/* -----------------------------------------------------------------------
 * Fallback to default keymap (ROM constants)
 * --------------------------------------------------------------------- */

static void _UseDefaults(void)
{
    s_active_left[0]  = &KEYMAP_LEFT_LAYER0[0][0];
    s_active_left[1]  = &KEYMAP_LEFT_LAYER1[0][0];
    s_active_right[0] = &KEYMAP_RIGHT_LAYER0[0][0];
    s_active_right[1] = &KEYMAP_RIGHT_LAYER1[0][0];
}

/*
 * Copy Flash content vào RAM buffer rồi cập nhật active pointers.
 *
 * Fix unaligned warning:
 *   Flash struct dùng __attribute__((packed)) (hoặc không, nhưng
 *   địa chỉ Flash có thể không align với keycode_t nếu struct layout
 *   thay đổi). Dùng memcpy qua byte pointer luôn an toàn.
 */
static void _CopyFromFlashToRAM(const FlashKeymapHeader_t *hdr)
{
    for (uint8_t layer = 0U; layer < NUM_KEYMAP_LAYERS; layer++) {
        size_t layer_bytes =
            (size_t)(MATRIX_ROWS * MATRIX_COLS) * sizeof(keycode_t);

        memcpy(s_flash_copy_left[layer],
               hdr->left[layer],
               layer_bytes);

        memcpy(s_flash_copy_right[layer],
               hdr->right[layer],
               layer_bytes);

        s_active_left[layer]  = &s_flash_copy_left[layer][0][0];
        s_active_right[layer] = &s_flash_copy_right[layer][0][0];
    }
}

/* -----------------------------------------------------------------------
 * Public: Init
 * --------------------------------------------------------------------- */

bool FlashKeymap_Init(void)
{
    s_staging.dirty = false;

    const FlashKeymapHeader_t *hdr =
        (const FlashKeymapHeader_t *)KEYMAP_FLASH_ADDR;

    if (hdr->magic != KEYMAP_MAGIC_NUMBER) {
        _UseDefaults();
        return false;
    }

    if (hdr->version != (uint16_t)KEYMAP_VERSION) {
        _UseDefaults();
        return false;
    }

    uint16_t calc_crc = _CalcHeaderCRC(hdr);
    if (calc_crc != hdr->crc16) {
        _UseDefaults();
        return false;
    }

    /* Fix: copy ra RAM, không lưu pointer vào Flash struct */
    _CopyFromFlashToRAM(hdr);
    return true;
}

/* -----------------------------------------------------------------------
 * Public: Accessors
 * --------------------------------------------------------------------- */

const keycode_t *FlashKeymap_GetLeft(uint8_t layer)
{
    if (layer >= NUM_KEYMAP_LAYERS) return NULL;
    return s_active_left[layer];
}

const keycode_t *FlashKeymap_GetRight(uint8_t layer)
{
    if (layer >= NUM_KEYMAP_LAYERS) return NULL;
    return s_active_right[layer];
}

/* -----------------------------------------------------------------------
 * Public: Staging
 * --------------------------------------------------------------------- */

bool FlashKeymap_StagePage(uint8_t page_idx, const uint8_t *data)
{
    if (page_idx >= KEYMAP_NUM_PAGES) return false;
    if (data == NULL) return false;

    uint32_t offset    = (uint32_t)page_idx * KEYMAP_PAGE_SIZE;
    uint32_t remaining = (uint32_t)KEYMAP_RAW_BYTES - offset;
    uint32_t copy_len  = (remaining < KEYMAP_PAGE_SIZE)
                          ? remaining : KEYMAP_PAGE_SIZE;

    /* Staging buffer layout giống GetPage:
     * [left layer0][left layer1][right layer0][right layer1] */
    uint8_t *staging_raw = (uint8_t *)s_staging.left;
    memcpy(staging_raw + offset, data, copy_len);
    s_staging.dirty = true;

    return true;
}

/* -----------------------------------------------------------------------
 * Public: Commit staging buffer to Flash
 * --------------------------------------------------------------------- */

bool FlashKeymap_Commit(void)
{
    if (!s_staging.dirty) return true;

    /* Build header in RAM */
    FlashKeymapHeader_t hdr;
    memset(&hdr, 0xFF, sizeof(hdr));
    hdr.magic   = KEYMAP_MAGIC_NUMBER;
    hdr.version = (uint16_t)KEYMAP_VERSION;
    hdr.flags   = 0U;
    memset(hdr.reserved, 0, sizeof(hdr.reserved));
    memcpy(hdr.left,  s_staging.left,  sizeof(hdr.left));
    memcpy(hdr.right, s_staging.right, sizeof(hdr.right));
    hdr.crc16 = _CalcHeaderCRC(&hdr);

    /* Round up write size to word boundary */
    uint32_t write_size = (uint32_t)sizeof(FlashKeymapHeader_t);
    if (write_size % 4U != 0U) {
        write_size += 4U - (write_size % 4U);
    }

    /* Temp write buffer — word aligned on stack */
    uint8_t write_buf[sizeof(FlashKeymapHeader_t) + 4U];
    memset(write_buf, 0xFF, sizeof(write_buf));
    memcpy(write_buf, &hdr, sizeof(FlashKeymapHeader_t));

    /* Erase + write */
    if (!_Flash_Unlock()) return false;

    bool ok = _Flash_EraseSector5();
    if (ok) {
        ok = _Flash_WriteBuffer(KEYMAP_FLASH_ADDR, write_buf, write_size);
    }

    _Flash_Lock();
    if (!ok) return false;

    /* Verify */
    const FlashKeymapHeader_t *verify =
        (const FlashKeymapHeader_t *)KEYMAP_FLASH_ADDR;

    if (verify->magic != KEYMAP_MAGIC_NUMBER ||
        verify->crc16 != _CalcHeaderCRC(verify)) {
        _UseDefaults();
        return false;
    }

    /* Fix: copy verified data ra RAM */
    _CopyFromFlashToRAM(verify);
    s_staging.dirty = false;
    return true;
}

/* -----------------------------------------------------------------------
 * Public: Reset — erase Flash, revert to defaults
 * --------------------------------------------------------------------- */

bool FlashKeymap_Reset(void)
{
    if (!_Flash_Unlock()) return false;
    bool ok = _Flash_EraseSector5();
    _Flash_Lock();

    _UseDefaults();
    s_staging.dirty = false;
    return ok;
}

/* -----------------------------------------------------------------------
 * Public: GetPage — đọc trang keymap hiện tại (active, không phải staging)
 *
 * Fix BUG#1: build full keymap từ active pointers (RAM), không copy pointer.
 * Layout: [left layer0][left layer1][right layer0][right layer1]
 * --------------------------------------------------------------------- */

bool FlashKeymap_GetPage(uint8_t page_idx, uint8_t *data_out)
{
    if (page_idx >= KEYMAP_NUM_PAGES || data_out == NULL) return false;

    uint32_t offset    = (uint32_t)page_idx * KEYMAP_PAGE_SIZE;
    uint32_t remaining = (uint32_t)KEYMAP_RAW_BYTES - offset;
    uint32_t copy_len  = (remaining < KEYMAP_PAGE_SIZE)
                          ? remaining : KEYMAP_PAGE_SIZE;

    /* Build full keymap in local buffer */
    uint8_t full_keymap[KEYMAP_RAW_BYTES];
    uint8_t *p = full_keymap;

    const size_t layer_bytes =
        (size_t)(MATRIX_ROWS * MATRIX_COLS) * sizeof(keycode_t);

    for (uint8_t layer = 0U; layer < NUM_KEYMAP_LAYERS; layer++) {
        if (s_active_left[layer] != NULL) {
            memcpy(p, s_active_left[layer], layer_bytes);
        } else {
            memset(p, 0, layer_bytes);
        }
        p += layer_bytes;
    }

    for (uint8_t layer = 0U; layer < NUM_KEYMAP_LAYERS; layer++) {
        if (s_active_right[layer] != NULL) {
            memcpy(p, s_active_right[layer], layer_bytes);
        } else {
            memset(p, 0, layer_bytes);
        }
        p += layer_bytes;
    }

    memset(data_out, 0, KEYMAP_PAGE_SIZE);
    memcpy(data_out, full_keymap + offset, copy_len);
    return true;
}

/* -----------------------------------------------------------------------
 * Public: dirty flag
 * --------------------------------------------------------------------- */

bool FlashKeymap_IsDirty(void)
{
    return s_staging.dirty;
}
