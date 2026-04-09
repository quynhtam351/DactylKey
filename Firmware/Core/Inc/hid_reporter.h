#ifndef __HID_REPORTER_H
#define __HID_REPORTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_hid_custom.h"
#include <stdbool.h>

void HIDReporter_Init(void);

/**
 * Build report from KeyProcessor state and send if changed.
 * Call this from main loop when USB endpoint is ready.
 */
void HIDReporter_SendIfChanged(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif