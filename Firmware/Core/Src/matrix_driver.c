#include "matrix_driver.h"
#include <string.h>

/* ============================================================
 * STATIC VARIABLES
 * ============================================================*/
static MatrixState_t   s_matrix_state;
static KeyEventQueue_t s_event_queue;

/* Mảng pin từ keyboard_config.h */
static const uint16_t s_row_pins[MATRIX_ROWS] = ROW_PINS_ARRAY;
static const uint16_t s_col_pins[MATRIX_COLS] = COL_PINS_ARRAY;

/* ============================================================
 * PRIVATE FUNCTION PROTOTYPES
 * ============================================================*/
static void _Matrix_ScanRaw(void);
static void _Matrix_ApplyDebounce(void);
static void _Matrix_GenerateEvents(uint8_t row, uint8_t col, bool new_state);
static bool _Queue_Push(const KeyEvent_t *event);

static inline void _Row_DriveHigh(uint8_t row);
static inline void _Row_DriveLow(uint8_t row);
static inline uint8_t _Read_Columns(void);

/* ============================================================
 * PUBLIC FUNCTIONS
 * ============================================================*/

void Matrix_Init(void)
{
    memset(&s_matrix_state, 0, sizeof(MatrixState_t));
    memset(&s_event_queue, 0, sizeof(KeyEventQueue_t));

    /* Initialize debounce states */
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            s_matrix_state.debounce[row][col].status       = DEBOUNCE_STABLE;
            s_matrix_state.debounce[row][col].timer        = 0;
            s_matrix_state.debounce[row][col].stable_state = false;
        }
        s_matrix_state.raw[row]       = 0x00;
        s_matrix_state.debounced[row] = 0x00;
    }

    /* Set all rows HIGH (inactive) */
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
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            DebounceState_t *db = &s_matrix_state.debounce[row][col];

            if (db->status == DEBOUNCE_DEBOUNCING) {
                if (db->timer > 0) {
                    db->timer--;
                }
                if (db->timer == 0) {
                    db->status = DEBOUNCE_STABLE;
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

/* ============================================================
 * PRIVATE FUNCTIONS
 * ============================================================*/

/**
 * @brief Đọc trạng thái raw của tất cả columns
 * @return Bitmask của columns (bit=1 nghĩa là phím được nhấn)
 */
static inline uint8_t _Read_Columns(void)
{
    uint8_t col_state = 0;
    uint32_t idr = COL_GPIO_PORT->IDR;

    /* Đọc từng pin column và map vào bitmask */
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (!(idr & s_col_pins[col])) {  /* Active LOW */
            col_state |= (1 << col);
        }
    }

    return col_state;
}

static void _Matrix_ScanRaw(void)
{
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        /* Drive row LOW (active) */
        _Row_DriveLow(row);

        /* Chờ ổn định - tùy thuộc vào điện trở pull-up và capacitance */
        __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP();

        /* Đọc columns */
        s_matrix_state.raw[row] = _Read_Columns();

        /* Drive row HIGH (inactive) */
        _Row_DriveHigh(row);
    }
}

static void _Matrix_ApplyDebounce(void)
{
    s_matrix_state.changed = false;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            DebounceState_t *db = &s_matrix_state.debounce[row][col];

            bool raw_pressed = (s_matrix_state.raw[row] >> col) & 0x01;

            if (db->status == DEBOUNCE_STABLE) {
                if (raw_pressed != db->stable_state) {
                    db->stable_state = raw_pressed;
                    db->status       = DEBOUNCE_DEBOUNCING;
                    db->timer        = DEBOUNCE_TIME_MS;

                    if (raw_pressed) {
                        s_matrix_state.debounced[row] |= (1 << col);
                    } else {
                        s_matrix_state.debounced[row] &= ~(1 << col);
                    }

                    s_matrix_state.changed = true;
                    _Matrix_GenerateEvents(row, col, raw_pressed);
                }
            }
        }
    }
}

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
    if (s_event_queue.count >= KEY_EVENT_QUEUE_SIZE) {
        /* Overflow - discard oldest */
        s_event_queue.head = (s_event_queue.head + 1) % KEY_EVENT_QUEUE_SIZE;
        s_event_queue.count--;
    }

    s_event_queue.buffer[s_event_queue.tail] = *event;
    s_event_queue.tail = (s_event_queue.tail + 1) % KEY_EVENT_QUEUE_SIZE;
    s_event_queue.count++;

    return true;
}

static inline void _Row_DriveHigh(uint8_t row)
{
    ROW_GPIO_PORT->BSRR = s_row_pins[row];
}

static inline void _Row_DriveLow(uint8_t row)
{
    ROW_GPIO_PORT->BSRR = (uint32_t)s_row_pins[row] << 16U;
}
