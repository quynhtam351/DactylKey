#include "iwdg.h"

IWDG_HandleTypeDef hiwdg;

void MX_IWDG_Init(void)
{
    /*
     * BUG#7 fix: Flash sector erase (128KB) có thể mất đến ~2s trên
     * STM32F401 (datasheet: typ 1s, max 4s). Trong thời gian erase,
     * CPU bị stall — không thể refresh IWDG.
     *
     * Cần timeout > 4s để an toàn:
     * Timeout = (Prescaler × Reload) / LSI_freq
     *         = (256 × 4000)  / 32000
     *         = 32s   ← đủ dư cho Flash erase tệ nhất
     *
     * Prescaler 256 = IWDG_PRESCALER_256
     * Reload    4000
     * LSI       ~32kHz (±30%, min ~22kHz → worst case: 256×4000/22000 ≈ 46s)
     *
     * Trong normal operation, main loop refresh IWDG mỗi vòng lặp
     * (< 1ms) → không bao giờ timeout trừ khi firmware hang.
     */
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload    = 4000U;

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}
