#include "split_comm.h"
#include "crc16.h"
#include "usart.h"
#include <string.h>

/* ========================================================================
 *  Module State
 * ======================================================================== */
static SplitCommState_t s_comm;

/* Remote key event queue (master receives from slave) */
#define REMOTE_EVENT_QUEUE_SIZE  16
static KeyEvent_t   s_remote_events[REMOTE_EVENT_QUEUE_SIZE];
static volatile uint8_t s_remote_head = 0;
static volatile uint8_t s_remote_tail = 0;
static volatile uint8_t s_remote_count = 0;

/* ========================================================================
 *  Forward Declarations
 * ======================================================================== */
static bool     _SendPacket(uint8_t type, const uint8_t *payload, uint8_t len);
static void     _ProcessRxByte(uint8_t byte);
static void     _HandlePacket(const SplitPacket_t *pkt);
static void     _HandleKeyEvent(const SplitPacket_t *pkt);
static void     _HandleKeyState(const SplitPacket_t *pkt);
static void     _HandlePing(void);
static void     _HandlePong(void);
static void     _PushRemoteEvent(const KeyEvent_t *event);
static void     _MasterHeartbeat(void);
static void     _CheckTimeout(void);
static uint16_t _DMA_GetRxWritePos(void);

/* ========================================================================
 *  Public Functions
 * ======================================================================== */

void SplitComm_Init(void)
{
    memset(&s_comm, 0, sizeof(SplitCommState_t));

    s_comm.status           = SPLIT_DISCONNECTED;
    s_comm.rx_state         = RX_STATE_WAIT_SOF;
    s_comm.rx_dma_read_pos  = 0;
    s_comm.tx_busy          = false;
    s_comm.last_rx_tick     = HAL_GetTick();
    s_comm.last_heartbeat_tick = HAL_GetTick();

    s_remote_head  = 0;
    s_remote_tail  = 0;
    s_remote_count = 0;

    /* Start DMA circular receive */
    HAL_UART_Receive_DMA(&huart1, s_comm.rx_dma_buf, SPLIT_RX_BUFFER_SIZE);

    /* Enable IDLE line interrupt for faster processing */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

void SplitComm_Process(void)
{
    /* --- Parse incoming DMA data --- */
    uint16_t write_pos = _DMA_GetRxWritePos();

    while (s_comm.rx_dma_read_pos != write_pos) {
        uint8_t byte = s_comm.rx_dma_buf[s_comm.rx_dma_read_pos];
        s_comm.rx_dma_read_pos =
            (s_comm.rx_dma_read_pos + 1) % SPLIT_RX_BUFFER_SIZE;

        _ProcessRxByte(byte);
    }

    /* --- Master: send heartbeat periodically --- */
#if (KEYBOARD_ROLE == KEYBOARD_ROLE_MASTER)
    _MasterHeartbeat();
#endif

    /* --- Check for timeout (both roles) --- */
    _CheckTimeout();
}

bool SplitComm_SendKeyEvent(uint8_t key_index, uint8_t state)
{
    SplitKeyEventPayload_t payload;
    payload.key_index = key_index;
    payload.state     = state;
    payload.reserved  = 0;

    return _SendPacket(SPLIT_PKT_KEY_EVENT,
                       (const uint8_t *)&payload, sizeof(payload));
}

bool SplitComm_SendKeyState(const uint8_t *row_state, uint8_t num_rows)
{
    if (num_rows > SPLIT_MAX_PAYLOAD) return false;

    return _SendPacket(SPLIT_PKT_KEY_STATE, row_state, num_rows);
}

SplitConnStatus_t SplitComm_GetStatus(void)
{
    return s_comm.status;
}

bool SplitComm_GetRemoteEvent(KeyEvent_t *event)
{
    if (event == NULL || s_remote_count == 0) return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    *event = s_remote_events[s_remote_head];
    s_remote_head = (s_remote_head + 1) % REMOTE_EVENT_QUEUE_SIZE;
    s_remote_count--;

    __set_PRIMASK(primask);
    return true;
}

bool SplitComm_HasRemoteEvent(void)
{
    return (s_remote_count > 0);
}

void SplitComm_TxCompleteCallback(void)
{
    s_comm.tx_busy = false;
}

const SplitCommState_t* SplitComm_GetState(void)
{
    return &s_comm;
}

/* ========================================================================
 *  Packet TX
 * ======================================================================== */

static bool _SendPacket(uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (s_comm.tx_busy) return false;
    if (len > SPLIT_MAX_PAYLOAD) return false;

    uint8_t idx = 0;

    /* Header */
    s_comm.tx_buf[idx++] = SPLIT_SOF;
    s_comm.tx_buf[idx++] = type;
    s_comm.tx_buf[idx++] = len;

    /* Payload */
    if (len > 0 && payload != NULL) {
        memcpy(&s_comm.tx_buf[idx], payload, len);
        idx += len;
    }

    /* CRC over TYPE + LENGTH + PAYLOAD */
    uint16_t crc = CRC16_Calculate(&s_comm.tx_buf[1], 2 + len);
    s_comm.tx_buf[idx++] = (uint8_t)(crc & 0xFF);        /* CRC Low */
    s_comm.tx_buf[idx++] = (uint8_t)((crc >> 8) & 0xFF); /* CRC High */

    /* Send via DMA */
    s_comm.tx_busy = true;
    if (HAL_UART_Transmit_DMA(&huart1, s_comm.tx_buf, idx) != HAL_OK) {
        s_comm.tx_busy = false;
        return false;
    }

    s_comm.tx_packet_count++;
    return true;
}

/* ========================================================================
 *  Packet RX Parser (byte-by-byte state machine)
 * ======================================================================== */

static void _ProcessRxByte(uint8_t byte)
{
    switch (s_comm.rx_state) {

    case RX_STATE_WAIT_SOF:
        if (byte == SPLIT_SOF) {
            s_comm.rx_state = RX_STATE_WAIT_TYPE;
        }
        break;

    case RX_STATE_WAIT_TYPE:
        s_comm.rx_packet.type = byte;
        s_comm.rx_state = RX_STATE_WAIT_LEN;
        break;

    case RX_STATE_WAIT_LEN:
        s_comm.rx_packet.length = byte;
        s_comm.rx_payload_idx = 0;

        if (byte > SPLIT_MAX_PAYLOAD) {
            /* Invalid length — reset */
            s_comm.rx_error_count++;
            s_comm.rx_state = RX_STATE_WAIT_SOF;
        } else if (byte == 0) {
            /* No payload — go directly to CRC */
            s_comm.rx_state = RX_STATE_WAIT_CRC_L;
        } else {
            s_comm.rx_state = RX_STATE_WAIT_PAYLOAD;
        }
        break;

    case RX_STATE_WAIT_PAYLOAD:
        s_comm.rx_packet.payload[s_comm.rx_payload_idx++] = byte;
        if (s_comm.rx_payload_idx >= s_comm.rx_packet.length) {
            s_comm.rx_state = RX_STATE_WAIT_CRC_L;
        }
        break;

    case RX_STATE_WAIT_CRC_L:
        s_comm.rx_packet.crc = byte;  /* Low byte */
        s_comm.rx_state = RX_STATE_WAIT_CRC_H;
        break;

    case RX_STATE_WAIT_CRC_H:
        s_comm.rx_packet.crc |= ((uint16_t)byte << 8);  /* High byte */

        /* Verify CRC: calculate over [TYPE, LENGTH, PAYLOAD] */
        {
            uint8_t crc_buf[2 + SPLIT_MAX_PAYLOAD];
            crc_buf[0] = s_comm.rx_packet.type;
            crc_buf[1] = s_comm.rx_packet.length;
            if (s_comm.rx_packet.length > 0) {
                memcpy(&crc_buf[2], s_comm.rx_packet.payload,
                       s_comm.rx_packet.length);
            }

            uint16_t calc_crc = CRC16_Calculate(crc_buf,
                                    2 + s_comm.rx_packet.length);

            if (calc_crc == s_comm.rx_packet.crc) {
                _HandlePacket(&s_comm.rx_packet);
                s_comm.rx_packet_count++;
            } else {
                s_comm.rx_error_count++;
            }
        }

        s_comm.rx_state = RX_STATE_WAIT_SOF;
        break;

    default:
        s_comm.rx_state = RX_STATE_WAIT_SOF;
        break;
    }
}

/* ========================================================================
 *  Packet Handler (dispatches by type)
 * ======================================================================== */

static void _HandlePacket(const SplitPacket_t *pkt)
{
    s_comm.last_rx_tick = HAL_GetTick();

    if (s_comm.status == SPLIT_DISCONNECTED) {
        s_comm.status = SPLIT_CONNECTED;
    }

    switch (pkt->type) {
    case SPLIT_PKT_KEY_EVENT:
        _HandleKeyEvent(pkt);
        break;

    case SPLIT_PKT_KEY_STATE:
        _HandleKeyState(pkt);
        break;

    case SPLIT_PKT_PING:
        _HandlePing();
        break;

    case SPLIT_PKT_PONG:
        _HandlePong();
        break;

    default:
        break;
    }
}

/* Master receives key event from slave */
static void _HandleKeyEvent(const SplitPacket_t *pkt)
{
#if (KEYBOARD_ROLE == KEYBOARD_ROLE_MASTER)
    if (pkt->length < sizeof(SplitKeyEventPayload_t)) return;

    const SplitKeyEventPayload_t *kep =
        (const SplitKeyEventPayload_t *)pkt->payload;

    KeyEvent_t event;
    event.key_index = kep->key_index;  /* Slave local index 0-43 */
    event.row       = kep->key_index / MATRIX_COLS;
    event.col       = kep->key_index % MATRIX_COLS;
    event.state     = kep->state ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
    event.timestamp = HAL_GetTick();

    _PushRemoteEvent(&event);
#else
    (void)pkt;
#endif
}

/* Master receives full matrix state from slave */
static void _HandleKeyState(const SplitPacket_t *pkt)
{
#if (KEYBOARD_ROLE == KEYBOARD_ROLE_MASTER)
    if (pkt->length < MATRIX_ROWS) return;

    /* Compare with previous state and generate events for differences */
    static uint8_t s_prev_remote_state[MATRIX_ROWS] = {0};

    for (uint8_t row = 0; row < MATRIX_ROWS && row < pkt->length; row++) {
        uint8_t changed = s_prev_remote_state[row] ^ pkt->payload[row];

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (changed & (1 << col)) {
                bool pressed = (pkt->payload[row] >> col) & 0x01;

                KeyEvent_t event;
                event.key_index = row * MATRIX_COLS + col;
                event.row       = row;
                event.col       = col;
                event.state     = pressed ? KEY_STATE_PRESSED
                                          : KEY_STATE_RELEASED;
                event.timestamp = HAL_GetTick();

                _PushRemoteEvent(&event);
            }
        }

        s_prev_remote_state[row] = pkt->payload[row];
    }
#else
    (void)pkt;
#endif
}

/* Slave receives ping, responds with pong */
static void _HandlePing(void)
{
#if (KEYBOARD_ROLE == KEYBOARD_ROLE_SLAVE)
    _SendPacket(SPLIT_PKT_PONG, NULL, 0);
#endif
}

/* Master receives pong — connection confirmed */
static void _HandlePong(void)
{
    s_comm.status = SPLIT_CONNECTED;
}

/* ========================================================================
 *  Remote Event Queue
 * ======================================================================== */

static void _PushRemoteEvent(const KeyEvent_t *event)
{
    if (s_remote_count >= REMOTE_EVENT_QUEUE_SIZE) {
        /* Overflow: drop oldest */
        s_remote_head = (s_remote_head + 1) % REMOTE_EVENT_QUEUE_SIZE;
        s_remote_count--;
    }

    s_remote_events[s_remote_tail] = *event;
    s_remote_tail = (s_remote_tail + 1) % REMOTE_EVENT_QUEUE_SIZE;
    s_remote_count++;
}

/* ========================================================================
 *  Master: Periodic Heartbeat
 * ======================================================================== */

static void _MasterHeartbeat(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_comm.last_heartbeat_tick) >= SPLIT_HEARTBEAT_MS) {
        s_comm.last_heartbeat_tick = now;
        _SendPacket(SPLIT_PKT_PING, NULL, 0);
    }
}

/* ========================================================================
 *  Timeout Detection
 * ======================================================================== */

static void _CheckTimeout(void)
{
    if (s_comm.status == SPLIT_CONNECTED) {
        uint32_t elapsed = HAL_GetTick() - s_comm.last_rx_tick;

        if (elapsed > SPLIT_TIMEOUT_MS) {
            s_comm.status = SPLIT_DISCONNECTED;
        }
    }
}

/* ========================================================================
 *  DMA Helper
 * ======================================================================== */

static uint16_t _DMA_GetRxWritePos(void)
{
    /* DMA NDTR counts DOWN from buffer size */
    uint16_t ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    return (SPLIT_RX_BUFFER_SIZE - ndtr) % SPLIT_RX_BUFFER_SIZE;
}

/* ========================================================================
 *  UART Callbacks (call from stm32f4xx_it.c or HAL callbacks)
 * ======================================================================== */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        SplitComm_TxCompleteCallback();
    }
}

/* IDLE line detection — called from USART1_IRQHandler */
void SplitComm_UART_IdleCallback(void)
{
    /* Clear IDLE flag */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    /* Nothing else needed — main loop reads DMA buffer */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        // Clear error flags
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);

        // Reset RX state machine
        s_comm.rx_state = RX_STATE_WAIT_SOF;
        s_comm.rx_dma_read_pos = 0;

        // Restart DMA receive
        HAL_UART_DMAStop(huart);
        HAL_UART_Receive_DMA(huart, s_comm.rx_dma_buf, SPLIT_RX_BUFFER_SIZE);

        // Update error count
        s_comm.rx_error_count++;
    }
}
