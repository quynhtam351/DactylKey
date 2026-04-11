#include "main.h"
#include "stm32f4xx_it.h"
#include "split_comm.h"

extern PCD_HandleTypeDef  hpcd_USB_OTG_FS;
extern TIM_HandleTypeDef  htim2;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim5;

/* -----------------------------------------------------------------------
 * Fault handlers
 * --------------------------------------------------------------------- */

void NMI_Handler(void)      { while (1) { } }
void HardFault_Handler(void){ while (1) { } }
void MemManage_Handler(void){ while (1) { } }
void BusFault_Handler(void) { while (1) { } }
void UsageFault_Handler(void){ while (1) { } }

void SVC_Handler(void)      { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void)   { }
void SysTick_Handler(void)  { }

/* -----------------------------------------------------------------------
 * TIM2 — matrix scan (1ms)
 * --------------------------------------------------------------------- */

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* -----------------------------------------------------------------------
 * USART1 — split communication
 * LOGIC#7 fix: clear IDLE flag đúng cách theo STM32F4 reference manual
 *
 * STM32F4 UART IDLE clear sequence: đọc SR rồi đọc DR
 * (HAL không tự clear IDLE khi dùng DMA circular mode)
 * --------------------------------------------------------------------- */

void USART1_IRQHandler(void)
{
    /* LOGIC#7 fix: Phải clear IDLE flag TRƯỚC KHI gọi HAL_UART_IRQHandler
     * vì HAL không handle IDLE flag trong DMA mode.
     *
     * Clear sequence cho STM32F4: read SR, then read DR.
     * Dùng __HAL_UART_CLEAR_IDLEFLAG() không đủ trên F4 —
     * phải đọc cả SR lẫn DR để clear hardware flag.
     */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) &&
        __HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_IDLE))
    {
        /* Clear IDLE flag: đọc SR (đã xong qua GET_FLAG), đọc DR */
        volatile uint32_t tmp;
        tmp = huart1.Instance->SR;   /* đọc SR */
        tmp = huart1.Instance->DR;   /* đọc DR — clear IDLE flag */
        (void)tmp;

        /* Notify split comm (hiện tại không cần action,
         * nhưng callback tồn tại cho tương lai) */
        SplitComm_UART_IdleCallback();
    }

    HAL_UART_IRQHandler(&huart1);
}

/* -----------------------------------------------------------------------
 * TIM5 — HAL timebase
 * --------------------------------------------------------------------- */

void TIM5_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim5);
}

/* -----------------------------------------------------------------------
 * DMA2 — USART1 RX/TX
 * --------------------------------------------------------------------- */

void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/* -----------------------------------------------------------------------
 * USB OTG FS
 * --------------------------------------------------------------------- */

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}
