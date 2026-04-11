#include "matrix_driver.h"
#include "keyboard_config.h"
#include <string.h>

static MatrixState_t   s_matrix_state;
static KeyEventQueue_t s_event_queue;

static const uint16_t s_row_pins[MATRIX_ROWS] = ROW_PINS_ARRAY;
static const uint16_t s_col_pins[MATRIX_COLS] = COL_PINS_ARRAY;

static void     _Matrix_ScanRaw(void);
static void     _Matrix_ApplyDebounce(void);
static void     _Matrix_GenerateEvents(uint8_t row, uint8_t col, bool new_state);
static bool     _Queue_Push(const KeyEvent_t *event);

static inline void    _Row_DriveHigh(uint8_t row);
static inline void    _Row_DriveLow(uint8_t row);
static inline uint8_t _Read_Columns(void);

static void _Row_SettleDelay(void)
{
    volatile uint32_t count = ROW_SETTLE_US * (SystemCoreClock / 1000000UL);
    while (count > 0U) {
        count--;
    }
}

/* ══════════════════════════════════════════════════════════
 * Init / Reset
 * ══════════════════════════════════════════════════════════ */

void Matrix_Init(void)
{
    memset(&s_matrix_state, 0, sizeof(MatrixState_t));
    memset(&s_event_queue,  0, sizeof(KeyEventQueue_t));

    for (uint8_t row = 0U; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0U; col < MATRIX_COLS; col++) {
            s_matrix_state.debounce[row][col].status        = DEBOUNCE_STABLE;
            s_matrix_state.debounce[row][col].timer         = 0U;
            s_matrix_state.debounce[row][col].stable_state  = false;
            s_matrix_state.debounce[row][col].pending_state = false;
        }
        s_matrix_state.raw[row]       = 0x00U;
        s_matrix_state.debounced[row] = 0x00U;
    }

    for (uint8_t row = 0U; row < MATRIX_ROWS; row++) {
        _Row_DriveHigh(row);
    }
}

void Matrix_Reset(void)
{
    Matrix_Init();
}

/* ══════════════════════════════════════════════════════════
 * Scan & Debounce (gọi từ TIM2 ISR)
 * ══════════════════════════════════════════════════════════ */

void Matrix_Scan(void)
{
    _Matrix_ScanRaw();
    _Matrix_ApplyDebounce();
    s_matrix_state.scan_count++;
}

void Matrix_DebounceTask(void)
{
    for (uint8_t row = 0U; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0U; col < MATRIX_COLS; col++) {
            DebounceState_t *db = &s_matrix_state.debounce[row][col];
            if (db->status == DEBOUNCE_DEBOUNCING && db->timer > 0U) {
                db->timer--;
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════
 * Event queue access (gọi từ main loop)
 * ══════════════════════════════════════════════════════════ */

bool Matrix_GetEvent(KeyEvent_t *event)
{
    if (event == NULL) return false;

    /*
     * Disable IRQ để bảo vệ truy cập concurrent:
     * - Producer: TIM2 ISR (qua _Queue_Push)
     * - Consumer: main loop (hàm này)
     *
     * Cả head, tail, count đều được bảo vệ trong critical section.
     */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (s_event_queue.count == 0U) {
        __set_PRIMASK(primask);
        return false;
    }

    *event = s_event_queue.buffer[s_event_queue.head];
    s_event_queue.head = (s_event_queue.head + 1U) % KEY_EVENT_QUEUE_SIZE;
    s_event_queue.count--;

    __set_PRIMASK(primask);
    return true;
}

bool Matrix_HasEvent(void)
{
    /*
     * Đọc count mà không disable IRQ là OK cho mục đích polling
     * (worst case: đọc cũ 1 tick, không critical).
     */
    return (s_event_queue.count > 0U);
}

/* ══════════════════════════════════════════════════════════
 * State query
 * ══════════════════════════════════════════════════════════ */

const MatrixState_t *Matrix_GetState(void)
{
    return &s_matrix_state;
}

KeyState_t Matrix_GetKeyState(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KEY_STATE_RELEASED;
    }
    bool pressed = (s_matrix_state.debounced[row] >> col) & 0x01U;
    return pressed ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
}

uint8_t Matrix_ToKeyIndex(uint8_t row, uint8_t col)
{
    return (uint8_t)(row * MATRIX_COLS + col);
}

/* ══════════════════════════════════════════════════════════
 * Private: GPIO helpers
 * ══════════════════════════════════════════════════════════ */

static inline uint8_t _Read_Columns(void)
{
    uint8_t  col_state = 0U;
    uint32_t idr       = COL_GPIO_PORT->IDR;

    for (uint8_t col = 0U; col < MATRIX_COLS; col++) {
        if (!(idr & s_col_pins[col])) {
            col_state |= (uint8_t)(1U << col);
        }
    }
    return col_state;
}

static inline void _Row_DriveHigh(uint8_t row)
{
    ROW_GPIO_PORT->BSRR = s_row_pins[row];
}

static inline void _Row_DriveLow(uint8_t row)
{
    ROW_GPIO_PORT->BSRR = (uint32_t)s_row_pins[row] << 16U;
}

/* ══════════════════════════════════════════════════════════
 * Private: Scan
 * ══════════════════════════════════════════════════════════ */

static void _Matrix_ScanRaw(void)
{
    for (uint8_t row = 0U; row < MATRIX_ROWS; row++) {
        _Row_DriveLow(row);
        _Row_SettleDelay();
        s_matrix_state.raw[row] = _Read_Columns();
        _Row_DriveHigh(row);
    }
}

/* ══════════════════════════════════════════════════════════
 * Private: Debounce
 * ══════════════════════════════════════════════════════════ */

static void _Matrix_ApplyDebounce(void)
{
    s_matrix_state.changed = false;

    for (uint8_t row = 0U; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0U; col < MATRIX_COLS; col++) {
            DebounceState_t *db    = &s_matrix_state.debounce[row][col];
            bool             raw_p = (bool)((s_matrix_state.raw[row] >> col) & 0x01U);

            if (db->status == DEBOUNCE_STABLE) {
                if (raw_p != db->stable_state) {
                    db->status        = DEBOUNCE_DEBOUNCING;
                    db->pending_state = raw_p;
                    db->timer         = DEBOUNCE_TIME_MS;
                }
            } else {
                /* DEBOUNCE_DEBOUNCING */
                if (raw_p == db->stable_state) {
                    /*
                     * Tín hiệu trở về trạng thái ổn định trước khi
                     * hết timer → bounce, hủy debounce.
                     */
                    db->status = DEBOUNCE_STABLE;
                    db->timer  = 0U;
                } else if (db->timer == 0U) {
                    /* Hết timer, kiểm tra trạng thái hiện tại */
                    if (raw_p == db->pending_state) {
                        /* Xác nhận thay đổi trạng thái */
                        db->stable_state = raw_p;
                        db->status       = DEBOUNCE_STABLE;

                        if (raw_p) {
                            s_matrix_state.debounced[row] |=  (uint8_t)(1U << col);
                        } else {
                            s_matrix_state.debounced[row] &= (uint8_t)~(1U << col);
                        }

                        s_matrix_state.changed = true;
                        _Matrix_GenerateEvents(row, col, raw_p);
                    } else {
                        /* Trạng thái thay đổi lần nữa trong lúc debounce */
                        db->pending_state = raw_p;
                        db->timer         = DEBOUNCE_TIME_MS;
                    }
                } else if (raw_p != db->pending_state) {
                    /* Bounce trong quá trình debounce → reset timer */
                    db->pending_state = raw_p;
                    db->timer         = DEBOUNCE_TIME_MS;
                }
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════
 * Private: Event generation
 * ══════════════════════════════════════════════════════════ */

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

static bool _Queue_Push(const KeyEvent_t *event)
{
    /*
     * Gọi từ TIM2 ISR (_Matrix_ApplyDebounce → _Matrix_GenerateEvents).
     *
     * Chiến lược overflow: DROP NEWEST (drop event hiện tại nếu đầy).
     * Lý do:
     *   - An toàn hơn DROP OLDEST vì không cần modify head từ ISR
     *     (tránh conflict với consumer trong main loop đang modify head).
     *   - Trong thực tế, queue 32 events đủ cho tất cả keystrokes
     *     trong 1 poll interval (1ms scan rate).
     *
     * Race condition analysis:
     *   - Producer (ISR): chỉ modify tail, count
     *   - Consumer (main loop): disable IRQ trước khi modify head, count
     *   - Vì ISR có priority cao hơn main loop, ISR không thể interrupt
     *     chính nó → tail chỉ có 1 writer → safe.
     *   - count được đọc bởi ISR (check đầy) và modify bởi cả ISR và
     *     consumer. Consumer disable IRQ khi modify → safe.
     *   - ISR đọc count (không disable IRQ): worst case đọc giá trị
     *     cũ hơn 1 → nghĩ queue chưa đầy trong khi consumer vừa lấy
     *     1 item → có thể push khi count = KEY_EVENT_QUEUE_SIZE - 1
     *     → tối đa lấp đầy → OK, không overflow.
     */
    if (s_event_queue.count >= KEY_EVENT_QUEUE_SIZE) {
        return false;  /* Queue đầy, drop event mới */
    }

    s_event_queue.buffer[s_event_queue.tail] = *event;
    s_event_queue.tail = (s_event_queue.tail + 1U) % KEY_EVENT_QUEUE_SIZE;

    /*
     * Memory barrier trước khi tăng count, đảm bảo data ghi vào
     * buffer được visible trước khi consumer thấy count tăng.
     */
    __DMB();
    s_event_queue.count++;

    return true;
}
