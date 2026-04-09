#ifndef __USBD_HID_CUSTOM_H
#define __USBD_HID_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"
#include <stdint.h>
#include <stdbool.h>
/* ========================================================================
 *  USB HID Class Request Codes (HID 1.11 spec section 7.2)
 * ======================================================================== */
#define HID_REQ_GET_REPORT      0x01U
#define HID_REQ_GET_IDLE        0x02U
#define HID_REQ_GET_PROTOCOL    0x03U
#define HID_REQ_SET_REPORT      0x09U
#define HID_REQ_SET_IDLE        0x0AU
#define HID_REQ_SET_PROTOCOL    0x0BU

/* ========================================================================
 *  USB HID Descriptor Types (HID 1.11 spec section 7.1)
 * ======================================================================== */
#define HID_DESCRIPTOR_TYPE     0x21U
#define HID_REPORT_DESC         0x22U
#define HID_PHYSICAL_DESC       0x23U

/* ========================================================================
 *  USB Standard Request Type Masks (USB 2.0 spec section 9.3)
 * ======================================================================== */
#ifndef USB_REQ_TYPE_STANDARD
#define USB_REQ_TYPE_STANDARD   0x00U
#endif
#ifndef USB_REQ_TYPE_CLASS
#define USB_REQ_TYPE_CLASS      0x20U
#endif
#ifndef USB_REQ_TYPE_VENDOR
#define USB_REQ_TYPE_VENDOR     0x40U
#endif
#ifndef USB_REQ_TYPE_MASK
#define USB_REQ_TYPE_MASK       0x60U
#endif

/* ========================================================================
 *  Utility Macros
 * ======================================================================== */
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif
/* =========================================================
 * ENDPOINT DEFINITIONS
 * ========================================================= */
#define HID_KB_EP_IN_ADDR               0x81U   /* EP1 IN: Keyboard reports */
#define HID_KB_EP_IN_SIZE               9U   /* 1(ID) + 1(mod) + 1(rsv) + 6(keys) */

#define HID_RAW_EP_IN_ADDR              0x82U   /* EP2 IN:  Raw HID (config response) */
#define HID_RAW_EP_OUT_ADDR             0x02U   /* EP2 OUT: Raw HID (config command) */
#define HID_RAW_EP_SIZE                 32U     /* 32 bytes: VIA-compatible */

#define HID_KB_POLL_INTERVAL            1U      /* 1ms polling interval */
#define HID_RAW_POLL_INTERVAL           1U      /* 1ms polling interval */

/* =========================================================
 * INTERFACE NUMBERS
 * ========================================================= */
#define HID_KB_INTERFACE_NUM            0U
#define HID_RAW_INTERFACE_NUM           1U

/* =========================================================
 * HID REPORT IDs
 * ========================================================= */
#define HID_REPORT_ID_KEYBOARD          0x01U
#define HID_REPORT_ID_NKRO              0x02U
/* Raw HID không dùng Report ID */

/* =========================================================
 * HID KEYBOARD REPORT STRUCTURE
 * Standard Boot Protocol Keyboard Report (8 bytes)
 * ========================================================= */
typedef struct {
    uint8_t report_id;   /* = HID_REPORT_ID_KEYBOARD (0x01) */
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[6];
} __attribute__((packed)) HID_KeyboardReport_t;  /* 9 bytes */

/* =========================================================
 * HID NKRO REPORT STRUCTURE
 * N-Key Rollover via bitmap (17 bytes)
 * ========================================================= */
typedef struct {
    uint8_t report_id;      /* = HID_REPORT_ID_NKRO (0x02) */
    uint8_t modifiers;      /* Modifier keys */
    uint8_t bitmap[15];     /* Bitmap cho keycodes 0x04-0x77 (120 keys) */
} __attribute__((packed)) HID_NKROReport_t;

/* =========================================================
 * RAW HID PACKET STRUCTURE
 * VIA-compatible 32-byte packet
 * ========================================================= */
typedef struct {
    uint8_t command;        /* Command byte */
    uint8_t data[31];       /* Payload */
} __attribute__((packed)) HID_RawPacket_t;

/* =========================================================
 * HID DEVICE STATE
 * ========================================================= */
typedef enum {
    HID_IDLE    = 0x00U,
    HID_BUSY    = 0x01U
} HID_StateTypeDef;

typedef struct {
    HID_StateTypeDef kb_state;       /* Keyboard EP state */
    HID_StateTypeDef raw_state;      /* Raw HID EP state */
    uint8_t          raw_rx_buf[HID_RAW_EP_SIZE];  /* Raw HID receive buffer */
    uint32_t         protocol;       /* HID protocol (Boot=0, Report=1) */
    uint32_t         idle_rate;      /* Idle rate từ SET_IDLE request */
    bool             kb_report_pending;  /* Có report chờ gửi không */
} USBD_HID_Custom_HandleTypeDef;

/* =========================================================
 * CONFIGURATION DESCRIPTOR TOTAL SIZE
 *
 * Config Descriptor:      9 bytes
 * Interface 0 (KB):       9 bytes
 * HID Descriptor 0:       9 bytes
 * Endpoint 0 (EP1 IN):    7 bytes
 * Interface 1 (Raw):      9 bytes
 * HID Descriptor 1:       9 bytes
 * Endpoint 1 (EP2 IN):    7 bytes
 * Endpoint 2 (EP2 OUT):   7 bytes
 * Total:                  66 bytes
 * ========================================================= */
#define HID_CUSTOM_CONFIG_DESC_SIZE     66U

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/* USB Class driver handle - đăng ký với USB stack */
extern USBD_ClassTypeDef USBD_HID_Custom;

/**
 * @brief  Gửi keyboard HID report (6KRO)
 * @param  pdev: USB Device handle
 * @param  report: Con trỏ đến HID_KeyboardReport_t
 * @retval USBD_OK nếu thành công
 */
USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev,
                                                HID_KeyboardReport_t *report);

/**
 * @brief  Gửi NKRO report
 * @param  pdev: USB Device handle
 * @param  report: Con trỏ đến HID_NKROReport_t
 * @retval USBD_OK nếu thành công
 */
USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev,
                                            HID_NKROReport_t *report);

/**
 * @brief  Gửi Raw HID response về host
 * @param  pdev: USB Device handle
 * @param  data: Buffer 32 bytes
 * @retval USBD_OK nếu thành công
 */
USBD_StatusTypeDef USBD_HID_SendRawReport(USBD_HandleTypeDef *pdev,
                                           uint8_t *data);

/**
 * @brief  Kiểm tra có Raw HID packet nhận được không
 * @retval true nếu có packet mới
 */
bool USBD_HID_RawDataAvailable(void);

/**
 * @brief  Lấy Raw HID packet đã nhận
 * @param  buf: Buffer 32 bytes để nhận data
 * @retval true nếu lấy thành công
 */
bool USBD_HID_GetRawData(uint8_t *buf);

/**
 * @brief  Kiểm tra keyboard EP có sẵn sàng gửi không
 * @retval true nếu EP không bận
 */
bool USBD_HID_KeyboardReady(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_HID_CUSTOM_H */
