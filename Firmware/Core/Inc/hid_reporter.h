#ifndef __HID_REPORTER_H
#define __HID_REPORTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_hid_custom.h"
#include <stdbool.h>

void HIDReporter_Init(void);
void HIDReporter_SendIfChanged(USBD_HandleTypeDef *pdev);
void HIDReporter_SendMediaIfChanged(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif
