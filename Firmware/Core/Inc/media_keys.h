#ifndef __MEDIA_KEYS_H
#define __MEDIA_KEYS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keycode_defs.h"
#include "matrix_driver.h"

#define MEDIA_MAX_PRESSED       4U

#define HID_REPORT_ID_CONSUMER  0x04U

typedef struct {
    uint16_t usages[MEDIA_MAX_PRESSED];
    uint8_t  count;
    bool     changed;
} MediaKeyState_t;

void MediaKeys_Init(void);

void MediaKeys_HandleKeycode(keycode_t kc, KeyState_t state);

const MediaKeyState_t *MediaKeys_GetState(void);

bool MediaKeys_HasChanged(void);
void MediaKeys_ClearChanged(void);

uint16_t MediaKeys_GetUsage(uint8_t index);
uint8_t  MediaKeys_GetCount(void);

#ifdef __cplusplus
}
#endif

#endif
