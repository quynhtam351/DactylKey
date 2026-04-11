#include "main.h"
#include "stm32f4xx_it.h"
#include "split_comm.h"

extern PCD_HandleTypeDef  hpcd_USB_OTG_FS;
extern TIM_HandleTypeDef  htim2;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim5;

/* ── Cortex-M system exception handlers ──────────────────────── */

void NMI_Handler(void)
{
    while (1) { }
}

void HardFault_Handler(void)
{
    /* IWDG will reset the system after ~4 seconds */
    while (1) { }
}

void MemManage_Handler(void)
{
    while (1) { }
}

void BusFault_Handler(void)
{
    while (1) { }
}

void UsageFault_Handler(void)
{
    while (1) { }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    /* SysTick is NOT used as HAL timebase (TIM5 is).
     * This handler intentionally does nothing.
     * HAL_IncTick() is called from TIM5_IRQHandler below. */
}

/* ── TIM2: Matrix scan (1ms period, priority 1) ──────────────── */

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* ── USART1: Split communication ──────────────────────────────── */

void USART1_IRQHandler(void)
{
    /*
     * Check for IDLE line interrupt BEFORE calling HAL handler.
     *
     * Why: HAL_UART_IRQHandler() clears the IDLE flag internally in
     * some configurations, which prevents our callback from seeing it.
     * By checking and clearing the flag here first, we guarantee that
     * SplitComm_UART_IdleCallback() is always called when IDLE fires.
     *
     * The IDLE interrupt is enabled in SplitComm_Init() via:
     *   __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE)
     *
     * In our DMA circular RX setup, the IDLE callback itself only
     * clears the flag (no data processing needed — we poll DMA NDTR
     * in the main loop). Its primary value is as a hook for future
     * use (e.g., triggering immediate processing of a partial packet).
     */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) &&
        __HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_IDLE))
    {
        SplitComm_UART_IdleCallback();
    }

    HAL_UART_IRQHandler(&huart1);
}

/* ── TIM5: HAL timebase (1ms, priority 15) ───────────────────── */

void TIM5_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim5);
}

/* ── DMA2 Stream2: USART1 RX ─────────────────────────────────── */

void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/* ── USB OTG FS ───────────────────────────────────────────────── */

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

/* ── DMA2 Stream7: USART1 TX ─────────────────────────────────── */

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
