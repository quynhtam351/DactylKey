/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "usb_device.h"
#include "usbd_hid_custom.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "keyboard_config.h"
#include "matrix_driver.h"
#include "key_processor.h"
#include "hid_reporter.h"
#include "split_comm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */
#include "iwdg.h"

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Slave: forward local key events over UART */
#if (KEYBOARD_ROLE == KEYBOARD_ROLE_SLAVE)
static void Slave_ForwardEvents(void);
#endif
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  Matrix_Init();
  SplitComm_Init();
  MX_IWDG_Init();

#if (KEYBOARD_ROLE == KEYBOARD_ROLE_MASTER)
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  /* Khởi tạo matrix driver */
  //Matrix_Init();
  KeyProcessor_Init();
  HIDReporter_Init();
#endif
  /* Bắt đầu TIM2 interrupt để scan ma trận mỗi 1ms */
  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */
  /* Blink LED to indicate boot complete */
  for (int i = 0; i < 6; i++) {
      HAL_GPIO_TogglePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN);
      HAL_Delay(100);
  }
  HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
      /* === SPLIT COMMUNICATION (both roles) === */
	  HAL_IWDG_Refresh(&hiwdg);
      SplitComm_Process();

	#if (KEYBOARD_ROLE == KEYBOARD_ROLE_MASTER)
      /* --- Master: process local key events --- */
      /* === KEY PROCESSING === */
      // Xử lý local events (left half)
      KeyEvent_t event;
      while (Matrix_GetEvent(&event)) {
          KeyProcessor_HandleLocalEvent(&event);
      }

      // Xử lý remote events (right half)
      KeyEvent_t remote_event;
      while (SplitComm_GetRemoteEvent(&remote_event)) {
          KeyProcessor_HandleRemoteEvent(&remote_event);
      }

      /* --- Master: send USB HID report --- */
      /* === USB HID REPORTING === */
      if (USBD_HID_KeyboardReady(&hUsbDeviceFS)) {
          HIDReporter_SendIfChanged(&hUsbDeviceFS);
      }

      /* === RAW HID CONFIG PROTOCOL === */
      uint8_t raw_buf[HID_RAW_EP_SIZE];
      if (USBD_HID_GetRawData(raw_buf)) {
          /* TODO: keymap_protocol_handle(raw_buf); */
      }

/* --- Master: update layer LEDs --- */
      const KeyProcessorState_t *kp = KeyProcessor_GetState();
      if (kp->layer_state & 0x02) {
          HAL_GPIO_WritePin(LED_LAYER_GPIO_PORT, LED_LAYER1_PIN, GPIO_PIN_RESET);
      } else {
          HAL_GPIO_WritePin(LED_LAYER_GPIO_PORT, LED_LAYER1_PIN, GPIO_PIN_SET);
      }

	#else /* KEYBOARD_ROLE_SLAVE */
	/* --- Slave: forward key events to master --- */
	Slave_ForwardEvents();
	#endif
  }
}

/* Slave: read local key events and send over UART */
#if (KEYBOARD_ROLE == KEYBOARD_ROLE_SLAVE)
/**
  * @brief  Slave: read local key events and send over UART
  * @retval None
  */
static void Slave_ForwardEvents(void)
{
    KeyEvent_t event;
    while (Matrix_GetEvent(&event)) {
        SplitComm_SendKeyEvent(event.key_index,
                               (event.state == KEY_STATE_PRESSED) ? 1 : 0);
    }
}
#endif
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM5 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM5)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  if (htim->Instance == TIM2) {
	    Matrix_DebounceTask();  // Giảm timer TRƯỚC
	    Matrix_Scan();           // Scan + detect change SAU
  }
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
