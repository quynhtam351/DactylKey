#include "raw_hid.h"
#include "flash_keymap.h"
#include "macro_engine.h"
#include "keyboard_config.h"
#include "split_comm.h"
#include "stm32f4xx_hal.h"
#include <string.h>

extern IWDG_HandleTypeDef hiwdg;

#define RAW_PAYLOAD_MAX     ((uint8_t)(HID_RAW_EP_SIZE - 2U))

/*
 * H2 fix: moved to file scope.
 * M4 fix: dùng RAW_MACRO_STEPS_PER_PAGE từ header thay vì tính lại.
 *
 * Compile-time verify: đảm bảo header value khớp với calculated value.
 */
#define MACRO_STEPS_PER_RESP    ((RAW_PAYLOAD_MAX - 3U) / RAW_MACRO_STEP_SIZE)
_Static_assert(MACRO_STEPS_PER_RESP == RAW_MACRO_STEPS_PER_PAGE,
               "RAW_MACRO_STEPS_PER_PAGE in header must match calculated value");

static void _SendResponse(USBD_HandleTypeDef *pdev,
                           uint8_t cmd, uint8_t status,
                           const uint8_t *data, uint8_t data_len);

void RawHID_Init(void)
{
}

void RawHID_Process(USBD_HandleTypeDef *pdev)
{
    uint8_t buf[HID_RAW_EP_SIZE];

    if (!USBD_HID_GetRawData(buf)) return;

    uint8_t cmd = buf[0];

    uint8_t resp[RAW_PAYLOAD_MAX];
    memset(resp, 0, sizeof(resp));

    switch (cmd) {

    case RAW_CMD_GET_VERSION:
        resp[0] = (uint8_t)((FIRMWARE_VERSION >> 8U) & 0xFFU);
        resp[1] = (uint8_t)( FIRMWARE_VERSION        & 0xFFU);
        resp[2] = (uint8_t)SPLIT_PROTOCOL_VERSION;
        resp[3] = (uint8_t)MATRIX_ROWS;
        resp[4] = (uint8_t)MATRIX_COLS;
        resp[5] = (uint8_t)(IS_MASTER() ? 1U : 0U);
        _SendResponse(pdev, cmd, RAW_STATUS_OK, resp, 6U);
        break;

    case RAW_CMD_GET_KEYMAP: {
        uint8_t page = buf[1];
        uint8_t page_data[KEYMAP_PAGE_SIZE];

        if (!FlashKeymap_GetPage(page, page_data)) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
        } else {
            _SendResponse(pdev, cmd, RAW_STATUS_OK,
                          page_data, (uint8_t)KEYMAP_PAGE_SIZE);
        }
        break;
    }

    case RAW_CMD_SET_KEYMAP: {
        uint8_t page = buf[1];
        if (!FlashKeymap_StagePage(page, &buf[2])) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
        } else {
            _SendResponse(pdev, cmd, RAW_STATUS_OK, NULL, 0U);
        }
        break;
    }

    case RAW_CMD_COMMIT_KEYMAP:
        HAL_IWDG_Refresh(&hiwdg);
        if (!FlashKeymap_Commit()) {
            _SendResponse(pdev, cmd, RAW_STATUS_ERROR, NULL, 0U);
        } else {
            _SendResponse(pdev, cmd, RAW_STATUS_OK, NULL, 0U);
        }
        HAL_IWDG_Refresh(&hiwdg);
        break;

    case RAW_CMD_RESET_KEYMAP:
        HAL_IWDG_Refresh(&hiwdg);
        if (!FlashKeymap_Reset()) {
            _SendResponse(pdev, cmd, RAW_STATUS_ERROR, NULL, 0U);
        } else {
            _SendResponse(pdev, cmd, RAW_STATUS_OK, NULL, 0U);
        }
        HAL_IWDG_Refresh(&hiwdg);
        break;

    case RAW_CMD_GET_MACRO: {
        uint8_t macro_id  = buf[1];
        uint8_t step_page = buf[2];

        MacroStep_t steps[MAX_MACRO_STEPS];
        uint8_t     count = 0U;

        if (!MacroEngine_GetMacro(macro_id, steps, &count)) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
            break;
        }

        uint8_t start_step = (uint8_t)((uint16_t)step_page
                                        * MACRO_STEPS_PER_RESP);
        if (start_step >= count && count > 0U) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
            break;
        }

        uint8_t payload[3U + MACRO_STEPS_PER_RESP * RAW_MACRO_STEP_SIZE];
        memset(payload, 0, sizeof(payload));

        payload[0] = macro_id;
        payload[1] = step_page;
        payload[2] = count;

        for (uint8_t i = 0U; i < MACRO_STEPS_PER_RESP; i++) {
            uint8_t idx = (uint8_t)(start_step + i);
            uint8_t *dst = &payload[3U + i * RAW_MACRO_STEP_SIZE];
            if (idx < count) {
                memcpy(dst, &steps[idx], sizeof(MacroStep_t));
            }
        }

        _SendResponse(pdev, cmd, RAW_STATUS_OK,
                      payload, (uint8_t)sizeof(payload));
        break;
    }

    case RAW_CMD_SET_MACRO: {
        uint8_t macro_id    = buf[1];
        uint8_t step_page   = buf[2];
        uint8_t total_steps = buf[3];

        if (macro_id >= MAX_MACROS || total_steps > MAX_MACRO_STEPS) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
            break;
        }

        uint8_t start_step = (uint8_t)((uint16_t)step_page
                                        * MACRO_STEPS_PER_RESP);
        if (start_step >= total_steps) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
            break;
        }

        uint8_t steps_in_pkt = total_steps - start_step;
        if (steps_in_pkt > MACRO_STEPS_PER_RESP) {
            steps_in_pkt = (uint8_t)MACRO_STEPS_PER_RESP;
        }

        uint8_t data_bytes = (uint8_t)(steps_in_pkt * RAW_MACRO_STEP_SIZE);
        if ((uint16_t)4U + data_bytes > HID_RAW_EP_SIZE) {
            _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
            break;
        }

        MacroStep_t steps_buf[MACRO_STEPS_PER_RESP];
        memset(steps_buf, 0, sizeof(steps_buf));
        memcpy(steps_buf, &buf[4], data_bytes);

        if (step_page == 0U) {
            uint8_t set_count = (total_steps > MACRO_STEPS_PER_RESP)
                                 ? (uint8_t)MACRO_STEPS_PER_RESP
                                 : total_steps;
            if (!MacroEngine_SetMacro(macro_id, steps_buf, set_count)) {
                _SendResponse(pdev, cmd, RAW_STATUS_ERROR, NULL, 0U);
                break;
            }
        }

        _SendResponse(pdev, cmd, RAW_STATUS_OK, NULL, 0U);
        break;
    }

    case RAW_CMD_COMMIT_MACRO:
        _SendResponse(pdev, cmd, RAW_STATUS_OK, NULL, 0U);
        break;

    case RAW_CMD_GET_STATUS: {
        const SplitCommState_t *sc = SplitComm_GetState();

        resp[0] = (uint8_t)sc->status;
        resp[1] = (uint8_t)sc->sync_status;
        resp[2] = (uint8_t)(sc->rx_error_count  & 0xFFU);
        resp[3] = (uint8_t)(sc->tx_drop_count   & 0xFFU);
        resp[4] = (uint8_t)(sc->rx_packet_count & 0xFFU);
        resp[5] = (uint8_t)(sc->tx_packet_count & 0xFFU);
        resp[6] = FlashKeymap_IsDirty() ? 1U : 0U;
        resp[7] = MacroEngine_IsBusy()  ? 1U : 0U;

        _SendResponse(pdev, cmd, RAW_STATUS_OK, resp, 8U);
        break;
    }

    case RAW_CMD_REBOOT:
        _SendResponse(pdev, cmd, RAW_STATUS_OK, NULL, 0U);
        HAL_Delay(100U);
        NVIC_SystemReset();
        break;

    default:
        _SendResponse(pdev, cmd, RAW_STATUS_INVALID, NULL, 0U);
        break;
    }
}

static void _SendResponse(USBD_HandleTypeDef *pdev,
                           uint8_t cmd, uint8_t status,
                           const uint8_t *data, uint8_t data_len)
{
    static uint8_t s_resp_buf[HID_RAW_EP_SIZE];
    memset(s_resp_buf, 0, sizeof(s_resp_buf));

    s_resp_buf[0] = cmd | RAW_RESP_FLAG;
    s_resp_buf[1] = status;

    if (data != NULL && data_len > 0U) {
        uint8_t copy_len = (data_len > RAW_PAYLOAD_MAX)
                            ? RAW_PAYLOAD_MAX : data_len;
        memcpy(&s_resp_buf[2], data, copy_len);
    }

    USBD_StatusTypeDef st = USBD_HID_SendRawReport(pdev, s_resp_buf);
    if (st == USBD_BUSY) {
        HAL_Delay(2U);
        USBD_HID_SendRawReport(pdev, s_resp_buf);
    }
}
