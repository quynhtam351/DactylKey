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

/* ═══════════════════════════════════════════════════════════════ */
/*                      VBUS DETECTION                             */
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
/*                           MAIN                                  */
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

        /*
         * Step 1: SplitComm_Process()
         * Sets base LED pattern:
         *   Connected    → LED_STATUS_OFF
         *   Disconnected → LED_STATUS_BLINK_2HZ
         */
        SplitComm_Process();

        if (IS_MASTER()) {

            /* Local key events */
            KeyEvent_t event;
            while (Matrix_GetEvent(&event)) {
                KeyProcessor_HandleLocalEvent(&event);
            }

            /* Remote key events */
            KeyEvent_t remote_event;
            while (SplitComm_GetRemoteEvent(&remote_event)) {
                KeyProcessor_HandleRemoteEvent(&remote_event);
            }

            /* USB HID report */
            if (USBD_HID_KeyboardReady(&hUsbDeviceFS)) {
                HIDReporter_SendIfChanged(&hUsbDeviceFS);
            }

            /* Raw HID (reserved) */
            uint8_t raw_buf[HID_RAW_EP_SIZE];
            if (USBD_HID_GetRawData(raw_buf)) {
                (void)raw_buf;
            }

            /*
             * Step 2: USB issue detection (Master only).
             *
             * If split is connected but USB is not enumerated,
             * upgrade status LED to BLINK_4HZ.
             * This overrides the LED_STATUS_OFF set by SplitComm_Process().
             *
             * Condition: split connected + USB not configured.
             * We don't signal issue when split is disconnected
             * (BLINK_2HZ is already showing a more urgent state).
             */
            if (SplitComm_GetStatus() == SPLIT_CONNECTED) {
                if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
                    LedManager_SetStatusPattern(LED_STATUS_BLINK_4HZ);
                }
            }

            /* Update Layer LED state */
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

        /*
         * Step 3: LedManager_Update() — always last.
         * Reads the final pattern state and drives GPIO.
         */
        LedManager_Update();
    }
}

/* ═══════════════════════════════════════════════════════════════ */

static void Boot_IndicateRole(void)
{
    /*
     * Blink pattern encodes detected role:
     *   Master: 2 blinks
     *   Slave:  4 blinks
     *
     * After blink, LED is OFF (LedManager will control from now on).
     */
    uint8_t blinks = IS_MASTER() ? 2U : 4U;
    for (uint8_t i = 0U; i < blinks; i++) {
        HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);
        HAL_Delay(100U);
        HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
        HAL_Delay(100U);
    }
    HAL_Delay(200U);
    /* Leave LED OFF; SplitComm_Process will set pattern next loop */
}

/* ═══════════════════════════════════════════════════════════════ */

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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) {
        HAL_IncTick();
    }
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
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
