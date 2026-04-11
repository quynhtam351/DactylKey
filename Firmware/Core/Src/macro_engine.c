#include "macro_engine.h"
#include "key_processor.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static Macro_t s_macros[MAX_MACROS];

typedef struct {
    MacroExecState_t    state;
    uint8_t             macro_id;
    uint8_t             step_idx;
    /* LOGIC#2 fix: dùng absolute tick thay vì countdown in-place */
    uint32_t            delay_start_tick;
    uint32_t            delay_duration_ms;
    bool                tap_release_pending;
} MacroExecContext_t;

static MacroExecContext_t s_exec;

static void _InjectKey(keycode_t kc, KeyState_t state);

/* -----------------------------------------------------------------------
 * Init / Trigger
 * --------------------------------------------------------------------- */

void MacroEngine_Init(void)
{
    memset(s_macros, 0, sizeof(s_macros));
    memset(&s_exec,  0, sizeof(s_exec));
    s_exec.state = MACRO_EXEC_IDLE;
}

void MacroEngine_Trigger(uint8_t macro_id)
{
    if (macro_id >= MAX_MACROS) return;
    if (s_macros[macro_id].count == 0U) return;

    s_exec.macro_id              = macro_id;
    s_exec.step_idx              = 0U;
    s_exec.state                 = MACRO_EXEC_RUNNING;
    s_exec.tap_release_pending   = false;
    s_exec.delay_duration_ms     = 0U;
    s_exec.delay_start_tick      = 0U;
}

/* -----------------------------------------------------------------------
 * Process — call từ main loop mỗi iteration
 * --------------------------------------------------------------------- */

void MacroEngine_Process(void)
{
    if (s_exec.state == MACRO_EXEC_IDLE) return;

    Macro_t *macro = &s_macros[s_exec.macro_id];

    /* --- DELAY state: chờ đủ thời gian --- */
    if (s_exec.state == MACRO_EXEC_DELAY) {
        uint32_t elapsed = HAL_GetTick() - s_exec.delay_start_tick;
        if (elapsed < s_exec.delay_duration_ms) {
            return; /* chưa đủ thời gian */
        }
        /* Delay xong, tiếp tục step kế tiếp */
        s_exec.step_idx++;
        s_exec.state = MACRO_EXEC_RUNNING;
    }

    /* --- Xử lý tap release pending từ step trước --- */
    if (s_exec.tap_release_pending) {
        /* step_idx đã advance sau khi TAP press, nên step trước là idx-1 */
        if (s_exec.step_idx > 0U) {
            const MacroStep_t *prev =
                &macro->steps[s_exec.step_idx - 1U];
            keycode_t kc = ((uint16_t)prev->kc_h << 8U) | prev->kc_l;
            _InjectKey(kc, KEY_STATE_RELEASED);
        }
        s_exec.tap_release_pending = false;
        /* Không return — tiếp tục xử lý step hiện tại ngay */
    }

    /* --- RUNNING state: xử lý các step --- */
    while (s_exec.step_idx < macro->count) {
        const MacroStep_t *step = &macro->steps[s_exec.step_idx];
        keycode_t kc = ((uint16_t)step->kc_h << 8U) | step->kc_l;

        switch ((MacroStepType_t)step->type) {

        case MACRO_STEP_END:
            s_exec.state = MACRO_EXEC_IDLE;
            return;

        case MACRO_STEP_PRESS:
            _InjectKey(kc, KEY_STATE_PRESSED);
            s_exec.step_idx++;
            break;

        case MACRO_STEP_RELEASE:
            _InjectKey(kc, KEY_STATE_RELEASED);
            s_exec.step_idx++;
            break;

        case MACRO_STEP_TAP:
            _InjectKey(kc, KEY_STATE_PRESSED);
            s_exec.step_idx++;
            s_exec.tap_release_pending = true;
            return; /* Yield: release xử lý ở đầu Process() lần sau */

        case MACRO_STEP_DELAY:
            /* LOGIC#2 fix: dùng absolute tick, không modify step data */
            if (step->delay_ms > 0U) {
                s_exec.state            = MACRO_EXEC_DELAY;
                s_exec.delay_start_tick = HAL_GetTick();
                s_exec.delay_duration_ms = (uint32_t)step->delay_ms;
                return; /* Yield cho đến khi delay xong */
            }
            /* delay_ms == 0: bỏ qua */
            s_exec.step_idx++;
            break;

        default:
            s_exec.step_idx++;
            break;
        }
    }

    s_exec.state = MACRO_EXEC_IDLE;
}

/* -----------------------------------------------------------------------
 * Query / Edit
 * --------------------------------------------------------------------- */

bool MacroEngine_IsBusy(void)
{
    return (s_exec.state != MACRO_EXEC_IDLE);
}

bool MacroEngine_SetMacro(uint8_t macro_id,
                           const MacroStep_t *steps, uint8_t step_count)
{
    if (macro_id >= MAX_MACROS) return false;
    if (step_count > MAX_MACRO_STEPS) return false;
    if (steps == NULL && step_count > 0U) return false;

    Macro_t *m = &s_macros[macro_id];
    memset(m, 0, sizeof(Macro_t));
    if (step_count > 0U) {
        memcpy(m->steps, steps, step_count * sizeof(MacroStep_t));
    }
    m->count = step_count;
    return true;
}

bool MacroEngine_GetMacro(uint8_t macro_id,
                           MacroStep_t *steps_out, uint8_t *count_out)
{
    if (macro_id >= MAX_MACROS) return false;
    if (steps_out == NULL || count_out == NULL) return false;

    const Macro_t *m = &s_macros[macro_id];
    *count_out = m->count;
    memcpy(steps_out, m->steps, m->count * sizeof(MacroStep_t));
    return true;
}

void MacroEngine_LoadFromFlash(const uint8_t *flash_data, uint16_t size)
{
    if (flash_data == NULL) return;
    uint16_t max = (uint16_t)sizeof(s_macros);
    if (size > max) size = max;
    memcpy(s_macros, flash_data, size);

    /* Tính lại count cho mỗi macro (không dựa vào count bị lưu sai) */
    for (uint8_t i = 0U; i < MAX_MACROS; i++) {
        s_macros[i].count = 0U;
        for (uint8_t j = 0U; j < MAX_MACRO_STEPS; j++) {
            if (s_macros[i].steps[j].type == (uint8_t)MACRO_STEP_END) break;
            s_macros[i].count++;
        }
    }
}

uint16_t MacroEngine_SerializeToBuffer(uint8_t *buf, uint16_t buf_size)
{
    uint16_t needed = (uint16_t)(MAX_MACROS * MAX_MACRO_STEPS
                                  * sizeof(MacroStep_t));
    if (buf_size < needed) return 0U;
    memcpy(buf, s_macros, needed);
    return needed;
}

/* -----------------------------------------------------------------------
 * Internal
 * --------------------------------------------------------------------- */

static void _InjectKey(keycode_t kc, KeyState_t state)
{
    KeyProcessor_InjectKeycode(kc, state);
}
