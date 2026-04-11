#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*
 * Role_Detect() must be called after SystemClock_Config() and
 * before any peripheral initialization. It sets g_keyboard_role
 * by reading the USB OTG FS VBUS comparator.
 *
 * Returns: KEYBOARD_ROLE_MASTER or KEYBOARD_ROLE_SLAVE.
 */
uint8_t Role_Detect(void);

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif
