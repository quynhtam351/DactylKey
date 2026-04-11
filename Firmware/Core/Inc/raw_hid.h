#ifndef __RAW_HID_H
#define __RAW_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "usbd_hid_custom.h"
#include "flash_keymap.h"

#define RAW_CMD_GET_VERSION     0x01U
#define RAW_CMD_GET_KEYMAP      0x02U
#define RAW_CMD_SET_KEYMAP      0x03U
#define RAW_CMD_COMMIT_KEYMAP   0x04U
#define RAW_CMD_RESET_KEYMAP    0x05U
#define RAW_CMD_GET_MACRO       0x06U
#define RAW_CMD_SET_MACRO       0x07U
#define RAW_CMD_COMMIT_MACRO    0x08U
#define RAW_CMD_GET_STATUS      0x09U
#define RAW_CMD_REBOOT          0xFFU

#define RAW_RESP_FLAG           0x80U

#define RAW_STATUS_OK           0x00U
#define RAW_STATUS_ERROR        0x01U
#define RAW_STATUS_BUSY         0x02U
#define RAW_STATUS_INVALID      0x03U

#define RAW_KEYMAP_PAGE_SIZE    KEYMAP_PAGE_SIZE
#define RAW_KEYMAP_TOTAL_BYTES  KEYMAP_RAW_BYTES
#define RAW_KEYMAP_NUM_PAGES    KEYMAP_NUM_PAGES

#define RAW_MACRO_STEP_SIZE         4U

/*
 * M4 fix: Tính đúng số steps per page dựa trên payload thực tế.
 *
 * HID_RAW_EP_SIZE = 32 bytes total.
 * Response: [cmd|0x80][status][payload...] → payload max = 30 bytes
 * Macro payload: [macro_id(1)][step_page(1)][count(1)][steps...] → header = 3
 * Steps per page = (30 - 3) / 4 = 6
 *
 * Giá trị này phải khớp với MACRO_STEPS_PER_RESP trong raw_hid.c
 */
#define RAW_MACRO_STEPS_PER_PAGE    6U

void RawHID_Init(void);
void RawHID_Process(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif
