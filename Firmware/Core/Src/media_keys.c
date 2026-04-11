#include "media_keys.h"
#include "keycode_defs.h"
#include <string.h>

static MediaKeyState_t s_media_state;

void MediaKeys_Init(void)
{
    memset(&s_media_state, 0, sizeof(MediaKeyState_t));
}

void MediaKeys_HandleKeycode(keycode_t kc, KeyState_t state)
{
    if (KC_TYPE(kc) != KC_TYPE_MEDIA) return;

    uint16_t usage = (uint16_t)(KC_DATA(kc) & 0x0FFFU);
    if (usage == 0U) return;

    if (state == KEY_STATE_PRESSED) {
        for (uint8_t i = 0U; i < s_media_state.count; i++) {
            if (s_media_state.usages[i] == usage) return;
        }

        if (s_media_state.count < MEDIA_MAX_PRESSED) {
            s_media_state.usages[s_media_state.count++] = usage;
            s_media_state.changed = true;
        }
    } else {
        for (uint8_t i = 0U; i < s_media_state.count; i++) {
            if (s_media_state.usages[i] == usage) {
                for (uint8_t j = i; j < s_media_state.count - 1U; j++) {
                    s_media_state.usages[j] = s_media_state.usages[j + 1U];
                }
                s_media_state.count--;
                s_media_state.usages[s_media_state.count] = 0U;
                s_media_state.changed = true;
                return;
            }
        }
    }
}

const MediaKeyState_t *MediaKeys_GetState(void)
{
    return &s_media_state;
}

bool MediaKeys_HasChanged(void)
{
    return s_media_state.changed;
}

void MediaKeys_ClearChanged(void)
{
    s_media_state.changed = false;
}

uint16_t MediaKeys_GetUsage(uint8_t index)
{
    if (index >= s_media_state.count) return 0U;
    return s_media_state.usages[index];
}

uint8_t MediaKeys_GetCount(void)
{
    return s_media_state.count;
}
