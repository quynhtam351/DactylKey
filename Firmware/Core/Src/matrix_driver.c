#include "matrix_driver.h"
#include <string.h>

/*
 * Debounce strategy: DEFERRED DEBOUNCE
 * ─────────────────────────────────────
 * When a raw state change is detected, we do NOT act on it immediately.
 * Instead, we record the new reading as "pending_state" and start a
 * countdown timer. Only when the timer expires AND the raw reading still
 * matches the pending_state do we accept the change as real and generate
 * an event. If the raw reading bounces back to the original stable_state
 * before the timer expires, we cancel the debounce (false trigger).
 *
 * This eliminates the "phantom release" bug of eager debounce where a
 * bounce during the lockout window could cause a spurious event after
 * the timer expired.
 *
 * Timing: ~DEBOUNCE_TIME_MS latency on key press/release (typically 5ms).
 * This is imperceptible to users (human reaction time ~150ms).
 *
 * Thread safety:
 * - Matrix_Scan() and Matrix_DebounceTask() are called from TIM2 ISR.
 * - Matrix_GetEvent() is called from main loop with __disable_irq()
 *   to protect the queue read. Since the ISR only writes (push) and
 *   main only reads (pop), and the pop is atomic (IRQ disabled),
 *   this is safe without a full mutex.
 */

static MatrixState_t   s_matrix_state;
static KeyEventQueue_t s_event_queue;

static const uint16_t s_row_pins[MATRIX_ROWS] = ROW_PINS_ARRAY;
static const uint16_t s_col_pins[MATRIX_COLS] = COL_PINS_ARRAY;

static void _Matrix_ScanRaw(void);
static void _Matrix_ApplyDebounce(void);
static void _Matrix_GenerateEvents(uint8_t row, uint8_t col, bool new_state);
static bool _Queue_Push(const KeyEvent_t *event);

static inline void _Row_DriveHigh(uint8_t row);
static inline void _Row_DriveLow(uint8_t row);
static inline uint8_t _Read_Columns(void);

void Matrix_Init(void)
{
    memset(&s_matrix_state, 0, sizeof(MatrixState_t));
    memset(&s_event_queue, 0, sizeof(KeyEventQueue_t));

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            s_matrix_state.debounce[row][col].status        = DEBOUNCE_STABLE;
            s_matrix_state.debounce[row][col].timer         = 0;
            s_matrix_state.debounce[row][col].stable_state  = false;
            s_matrix_state.debounce[row][col].pending_state = false;
        }
        s_matrix_state.raw[row]       = 0x00;
        s_matrix_state.debounced[row] = 0x00;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        _Row_DriveHigh(row);
    }
}

void Matrix_Scan(void)
{
    _Matrix_ScanRaw();
    _Matrix_ApplyDebounce();
    s_matrix_state.scan_count++;
}

void Matrix_DebounceTask(void)
{
    /*
     * Called from TIM2 ISR every 1ms, BEFORE Matrix_Scan().
     * Only decrements timers. The actual state transition
     * happens in _Matrix_ApplyDebounce() which checks timer == 0.
     */
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            DebounceState_t *db = &s_matrix_state.debounce[row][col];

            if (db->status == DEBOUNCE_DEBOUNCING) {
                if (db->timer > 0) {
                    db->timer--;
                }
            }
        }
    }
}

bool Matrix_GetEvent(KeyEvent_t *event)
{
    if (event == NULL) return false;
    if (s_event_queue.count == 0) return false;

    __disable_irq();

    *event = s_event_queue.buffer[s_event_queue.head];
    s_event_queue.head = (s_event_queue.head + 1) % KEY_EVENT_QUEUE_SIZE;
    s_event_queue.count--;

    __enable_irq();

    return true;
}

bool Matrix_HasEvent(void)
{
    return (s_event_queue.count > 0);
}

const MatrixState_t* Matrix_GetState(void)
{
    return &s_matrix_state;
}

KeyState_t Matrix_GetKeyState(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KEY_STATE_RELEASED;
    }
    bool pressed = (s_matrix_state.debounced[row] >> col) & 0x01;
    return pressed ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
}

uint8_t Matrix_ToKeyIndex(uint8_t row, uint8_t col)
{
    return (row * MATRIX_COLS) + col;
}

void Matrix_Reset(void)
{
    Matrix_Init();
}

/* ── Low-level column read ────────────────────────────────────── */

static inline uint8_t _Read_Columns(void)
{
    uint8_t col_state = 0;
    uint32_t idr = COL_GPIO_PORT->IDR;

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (!(idr & s_col_pins[col])) {
            col_state |= (1 << col);
        }
    }

    return col_state;
}

/* ── Raw scan: drive each row LOW, read columns ──────────────── */

static void _Matrix_ScanRaw(void)
{
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {

        _Row_DriveLow(row);

        /* Allow GPIO to settle (~100ns at 84MHz, 8 NOPs ≈ 95ns) */
        __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP();

        s_matrix_state.raw[row] = _Read_Columns();

        _Row_DriveHigh(row);
    }
}

/* ── Deferred debounce logic ─────────────────────────────────── */

static void _Matrix_ApplyDebounce(void)
{
    s_matrix_state.changed = false;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            DebounceState_t *db = &s_matrix_state.debounce[row][col];

            bool raw_pressed = (s_matrix_state.raw[row] >> col) & 0x01;

            if (db->status == DEBOUNCE_STABLE) {
                /*
                 * STABLE state: currently settled.
                 * If raw differs from stable → start debouncing.
                 * Do NOT generate event yet (deferred).
                 */
                if (raw_pressed != db->stable_state) {
                    db->status        = DEBOUNCE_DEBOUNCING;
                    db->pending_state = raw_pressed;
                    db->timer         = DEBOUNCE_TIME_MS;
                }

            } else {
                /*
                 * DEBOUNCING state: waiting for timer to expire.
                 *
                 * Case 1: Raw bounced back to stable_state before timer
                 *         expired → false trigger, cancel debounce.
                 *
                 * Case 2: Timer expired and raw still matches pending
                 *         → real change, accept and generate event.
                 *
                 * Case 3: Timer expired but raw bounced back to stable
                 *         → the change was transient, cancel.
                 *
                 * Case 4: Raw changed to yet another value (shouldn't
                 *         happen with a switch, but handle gracefully)
                 *         → restart debounce with new pending.
                 */
                if (raw_pressed == db->stable_state) {
                    /* Case 1: bounce back, cancel debounce */
                    db->status = DEBOUNCE_STABLE;
                    db->timer  = 0;

                } else if (db->timer == 0) {
                    /*
                     * Timer expired. Check if raw still matches
                     * the pending state we recorded.
                     */
                    if (raw_pressed == db->pending_state) {
                        /* Case 2: confirmed real change */
                        db->stable_state = raw_pressed;
                        db->status       = DEBOUNCE_STABLE;

                        if (raw_pressed) {
                            s_matrix_state.debounced[row] |= (1 << col);
                        } else {
                            s_matrix_state.debounced[row] &= ~(1 << col);
                        }

                        s_matrix_state.changed = true;
                        _Matrix_GenerateEvents(row, col, raw_pressed);

                    } else {
                        /*
                         * Case 3/4: raw doesn't match pending.
                         * This means the signal is still unstable.
                         * Restart debounce with current raw as new pending.
                         */
                        db->pending_state = raw_pressed;
                        db->timer         = DEBOUNCE_TIME_MS;
                    }

                } else if (raw_pressed != db->pending_state) {
                    /*
                     * Case 4 (during countdown): raw changed to something
                     * other than both stable and pending. Restart with
                     * new pending. This handles multi-bounce scenarios.
                     */
                    db->pending_state = raw_pressed;
                    db->timer         = DEBOUNCE_TIME_MS;
                }
                /* else: timer still counting, raw matches pending → wait */
            }
        }
    }
}

/* ── Event generation ────────────────────────────────────────── */

static void _Matrix_GenerateEvents(uint8_t row, uint8_t col, bool new_state)
{
    KeyEvent_t event;

    event.row       = row;
    event.col       = col;
    event.key_index = Matrix_ToKeyIndex(row, col);
    event.state     = new_state ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
    event.timestamp = HAL_GetTick();

    _Queue_Push(&event);
}

/*
 * Ring buffer push. If full, drops the oldest event to make room.
 * Called only from TIM2 ISR context → no concurrent push possible.
 */
static bool _Queue_Push(const KeyEvent_t *event)
{
    if (s_event_queue.count >= KEY_EVENT_QUEUE_SIZE) {
        /* Overflow: drop oldest */
        s_event_queue.head = (s_event_queue.head + 1) % KEY_EVENT_QUEUE_SIZE;
        s_event_queue.count--;
    }

    s_event_queue.buffer[s_event_queue.tail] = *event;
    s_event_queue.tail = (s_event_queue.tail + 1) % KEY_EVENT_QUEUE_SIZE;
    s_event_queue.count++;

    return true;
}

/* ── Row drive helpers (direct register access for speed) ────── */

static inline void _Row_DriveHigh(uint8_t row)
{
    ROW_GPIO_PORT->BSRR = s_row_pins[row];
}

static inline void _Row_DriveLow(uint8_t row)
{
    ROW_GPIO_PORT->BSRR = (uint32_t)s_row_pins[row] << 16U;
}
