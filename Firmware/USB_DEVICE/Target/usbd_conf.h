#ifndef __USBD_CONF__H__
#define __USBD_CONF__H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#define USBD_MAX_NUM_INTERFACES        2U    /* 2 interfaces: KB + Raw */
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ         512U
#define USBD_DEBUG_LEVEL               0U
#define USBD_LPM_ENABLED              0U
#define USBD_SELF_POWERED             1U
#define MAX_STATIC_ALLOC_SIZE         512U

#define DEVICE_FS    0
#define DEVICE_HS    1

void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#define USBD_malloc         (void *)USBD_static_malloc
#define USBD_free           USBD_static_free
#define USBD_memset         memset
#define USBD_memcpy         memcpy
#define USBD_Delay          HAL_Delay

#if (USBD_DEBUG_LEVEL > 0U)
#define USBD_UsrLog(...)    do { printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_UsrLog(...)    do {} while (0)
#endif

#if (USBD_DEBUG_LEVEL > 1U)
#define USBD_ErrLog(...)    do { printf("ERROR: "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_ErrLog(...)    do {} while (0)
#endif

#if (USBD_DEBUG_LEVEL > 2U)
#define USBD_DbgLog(...)    do { printf("DEBUG: "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_DbgLog(...)    do {} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif
