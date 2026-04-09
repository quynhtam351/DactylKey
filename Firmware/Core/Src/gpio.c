#include "gpio.h"
#include "keyboard_config.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /*=========================================================
         * Giải phóng JTAG pins (PB3, PB4, PA15) cho GPIO
         * Trên STM32F4: Ghi MODER = 00 (Input) và AFR = 0
         *=========================================================*/
        /* PB3: Clear AF (bit 12-15 của AFRL) và set MODER = Input */
    GPIOB->AFR[0] &= ~(0xFUL << (3 * 4));  /* Clear AF cho PB3 */
    /*---------------------------------------------------------
     * ROW PINS - Output Push-Pull, High Speed
     * Mặc định HIGH (không active)
     *---------------------------------------------------------*/
    HAL_GPIO_WritePin(ROW_GPIO_PORT, ROW_ALL_PINS, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = ROW_ALL_PINS;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ROW_GPIO_PORT, &GPIO_InitStruct);

    /*---------------------------------------------------------
     * COLUMN PINS - Input với Pull-up
     * Active LOW khi phím được nhấn
     *---------------------------------------------------------*/
    GPIO_InitStruct.Pin  = COL_ALL_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(COL_GPIO_PORT, &GPIO_InitStruct);

    /*---------------------------------------------------------
     * LED STATUS PIN
     *---------------------------------------------------------*/
    HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = LED_STATUS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_STATUS_GPIO_PORT, &GPIO_InitStruct);

    /*---------------------------------------------------------
     * LED LAYER PINS
     *---------------------------------------------------------*/
    HAL_GPIO_WritePin(LED_LAYER_GPIO_PORT, LED_ALL_PINS, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = LED_ALL_PINS;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_LAYER_GPIO_PORT, &GPIO_InitStruct);
}
