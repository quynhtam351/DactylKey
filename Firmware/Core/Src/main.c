#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "usb_device.h"
#include "usbd_hid_custom.h"
#include "gpio.h"

#include "keyboard_config.h"
#include "matrix_driver.h"
#include "key_processor.h"
#include "hid_reporter.h"
#include "split_comm.h"
#include "led_manager.h"
#include "iwdg.h"

uint8_t g_keyboard_role = KEYBOARD_ROLE_UNKNOWN;

void SystemClock_Config(void);
static void Boot_IndicateRole(void);
static void USB_TryRemoteWakeup(void);

/* ═══════════════════════════════════════════════════════════════ */

uint8_t Role_Detect(void)
{
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
    HAL_Delay(VBUS_DETECT_SETTLE_MS);

    uint8_t vbus_count = 0U;
    for (uint8_t i = 0U; i < VBUS_DETECT_SAMPLES; i++) {
        if (USB_OTG_FS->GOTGCTL & USB_OTG_GOTGCTL_BSVLD) {
            vbus_count++;
        }
        HAL_Delay(1U);
    }

    uint8_t role;
    if (vbus_count == VBUS_DETECT_SAMPLES) {
        role = KEYBOARD_ROLE_MASTER;
    } else if (vbus_count == 0U) {
        role = KEYBOARD_ROLE_SLAVE;
    } else {
        role = KEYBOARD_ROLE_FALLBACK;
    }

    if (role == KEYBOARD_ROLE_SLAVE) {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
    }

    return role;
}

/* ═══════════════════════════════════════════════════════════════ */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    g_keyboard_role = Role_Detect();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    Matrix_Init();
    LedManager_Init();
    SplitComm_Init();
    MX_IWDG_Init();

    if (IS_MASTER()) {
        MX_USB_DEVICE_Init();
        KeyProcessor_Init();
        HIDReporter_Init();
    }

    HAL_TIM_Base_Start_IT(&htim2);
    Boot_IndicateRole();

    while (1)
    {
        HAL_IWDG_Refresh(&hiwdg);

        SplitComm_Process();

        if (IS_MASTER()) {

            /*
             * USB Suspend handling:
             * When USB host suspends the bus (PC sleep/hibernate),
             * we skip HID report sending but continue matrix scan
             * to detect key press for remote wakeup.
             *
             * If any key event occurs during suspend, trigger
             * USB remote wakeup to wake the host.
             */
            bool usb_suspended = USBD_HID_IsSuspended(&hUsbDeviceFS);

            /* Local key events */
            KeyEvent_t event;
            while (Matrix_GetEvent(&event)) {
                if (usb_suspended) {
                    USB_TryRemoteWakeup();
                    /* Don't process the key — host will re-enumerate.
                     * The key event is consumed to prevent queue buildup. */
                } else {
                    KeyProcessor_HandleLocalEvent(&event);
                }
            }

            /* Remote key events */
            KeyEvent_t remote_event;
            while (SplitComm_GetRemoteEvent(&remote_event)) {
                if (usb_suspended) {
                    USB_TryRemoteWakeup();
                } else {
                    KeyProcessor_HandleRemoteEvent(&remote_event);
                }
            }

            /* USB HID report — only when not suspended */
            if (!usb_suspended) {
                if (USBD_HID_KeyboardReady(&hUsbDeviceFS)) {
                    HIDReporter_SendIfChanged(&hUsbDeviceFS);
                }

                uint8_t raw_buf[HID_RAW_EP_SIZE];
                if (USBD_HID_GetRawData(raw_buf)) {
                    (void)raw_buf;
                }
            }

            /* USB issue detection (connected but not enumerated) */
            if (SplitComm_GetStatus() == SPLIT_CONNECTED &&
                !usb_suspended &&
                hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
                LedManager_SetStatusPattern(LED_STATUS_BLINK_4HZ);
            }

            /* Sync mismatch also uses BLINK_4HZ (set by split_comm) */

            /* Layer LED */
            const KeyProcessorState_t *kp = KeyProcessor_GetState();
            LedManager_SetLayerState(kp->layer_state);

        } else {
            /* Slave: forward events */
            KeyEvent_t ev;
            while (Matrix_GetEvent(&ev)) {
                SplitComm_SendKeyEvent(ev.key_index,
                    (ev.state == KEY_STATE_PRESSED) ? 1U : 0U);
            }
        }

        LedManager_Update();
    }
}

/* ═══════════════════════════════════════════════════════════════ */

/*
 * USB_TryRemoteWakeup
 * ───────────────────
 * Trigger USB remote wakeup when a key is pressed during USB suspend.
 * The host will resume the bus and re-poll for HID reports.
 *
 * Per USB spec:
 *   1. Device asserts remote wakeup signaling
 *   2. Host sees the signal and resumes the bus
 *   3. Device must stop signaling within 1-15ms
 *
 * We use a static flag to avoid sending multiple wakeup signals
 * from the same suspend cycle.
 */
static void USB_TryRemoteWakeup(void)
{
    static bool s_wakeup_sent = false;

    if (s_wakeup_sent) return;

    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)hUsbDeviceFS.pData;
    if (hpcd == NULL) return;

    /* Check that device is actually suspended and host enabled remote wakeup */
    if (hUsbDeviceFS.dev_state == USBD_STATE_SUSPENDED &&
        hUsbDeviceFS.dev_remote_wakeup == 1U)
    {
        HAL_PCD_ActivateRemoteWakeup(hpcd);
        HAL_Delay(USB_REMOTE_WAKEUP_DURATION_MS);
        HAL_PCD_DeActivateRemoteWakeup(hpcd);

        s_wakeup_sent = true;
    }

    /*
     * Reset flag when USB resumes (suspended flag cleared by
     * ResumeCallback). We check each iteration.
     */
    if (!USBD_HID_IsSuspended(&hUsbDeviceFS)) {
        s_wakeup_sent = false;
    }
}

/* ═══════════════════════════════════════════════════════════════ */

static void Boot_IndicateRole(void)
{
    uint8_t blinks = IS_MASTER() ? 2U : 4U;
    for (uint8_t i = 0U; i < blinks; i++) {
        HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);
        HAL_Delay(100U);
        HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
        HAL_Delay(100U);
    }
    HAL_Delay(200U);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 25;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) HAL_IncTick();
    if (htim->Instance == TIM2) {
        Matrix_DebounceTask();
        Matrix_Scan();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
