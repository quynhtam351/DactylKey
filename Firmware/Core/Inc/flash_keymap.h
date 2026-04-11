#ifndef __FLASH_KEYMAP_H
#define __FLASH_KEYMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "keycode_defs.h"
#include "keyboard_config.h"

#define NUM_KEYMAP_LAYERS       2U

#define KEYMAP_LEFT_SIZE        (NUM_KEYMAP_LAYERS * MATRIX_ROWS * MATRIX_COLS \
                                  * sizeof(keycode_t))
#define KEYMAP_RIGHT_SIZE       (NUM_KEYMAP_LAYERS * MATRIX_ROWS * MATRIX_COLS \
                                  * sizeof(keycode_t))
#define KEYMAP_RAW_BYTES        (KEYMAP_LEFT_SIZE + KEYMAP_RIGHT_SIZE)

#define KEYMAP_PAGE_SIZE        30U
#define KEYMAP_NUM_PAGES        5U

typedef struct {
    uint32_t    magic;
    uint16_t    version;
    uint16_t    crc16;
    uint16_t    flags;
    uint8_t     reserved[6];
    keycode_t   left [NUM_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
    keycode_t   right[NUM_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
} FlashKeymapHeader_t;

_Static_assert(sizeof(FlashKeymapHeader_t) % 4U == 0U,
               "FlashKeymapHeader_t size must be multiple of 4");

typedef struct {
    keycode_t   left [NUM_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
    keycode_t   right[NUM_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
    bool        dirty;
} KeymapStagingBuffer_t;

/*
 * Đảm bảo left và right contiguous trong KeymapStagingBuffer_t.
 * FlashKeymap_StagePage viết liên tục từ &s_staging.left sang &s_staging.right
 * thông qua pointer arithmetic → cần đảm bảo không có padding ở giữa.
 *
 * keycode_t = uint16_t (2 bytes), mảng left có
 * NUM_KEYMAP_LAYERS * MATRIX_ROWS * MATRIX_COLS * 2 bytes.
 * Với 2*4*4*2 = 64 bytes → even → không padding → OK.
 * Static assert này là safety net cho trường hợp config thay đổi.
 */
_Static_assert(
    offsetof(KeymapStagingBuffer_t, right) ==
    offsetof(KeymapStagingBuffer_t, left) + KEYMAP_LEFT_SIZE,
    "KeymapStagingBuffer_t: left and right must be contiguous (no padding)");

_Static_assert(
    KEYMAP_PAGE_SIZE * KEYMAP_NUM_PAGES >= KEYMAP_RAW_BYTES,
    "KEYMAP_NUM_PAGES too small to cover full keymap");

bool FlashKeymap_Init(void);

const keycode_t *FlashKeymap_GetLeft (uint8_t layer);
const keycode_t *FlashKeymap_GetRight(uint8_t layer);

bool FlashKeymap_StagePage(uint8_t page_idx, const uint8_t *data);
bool FlashKeymap_Commit(void);
bool FlashKeymap_Reset(void);

bool FlashKeymap_GetPage(uint8_t page_idx, uint8_t *data_out);
bool FlashKeymap_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_KEYMAP_H */
