#include "split_comm.h"
#include "crc16.h"
#include "usart.h"
#include "keyboard_config.h"
#include "led_manager.h"
#include <string.h>

/*
 * Split Communication Module — Phase 3
 * ═════════════════════════════════════
 *
 * New: Protocol handshake (SYNC_REQ / SYNC_RSP)
 *
 * On first connection (transition DISCONNECTED → CONNECTED):
 *   Master sends SYNC_REQ with its protocol version, firmware
 *   version, and matrix dimensions.
 *   Slave receives SYNC_REQ, checks compatibility, responds
 *   with SYNC_RSP containing its own info.
 *   Master checks SYNC_RSP. If protocol_version matches
 *   and matrix dimensions match → SYNC_OK.
 *   If mismatch → SYNC_MISMATCH (still operates, but LED warns).
 *   If no response within SPLIT_SYNC_TIMEOUT_MS → SYNC_TIMEOUT
 *   (proceeds normally for backward compatibility with old firmware).
 *
 * Key events from slave are accepted regardless of sync status.
 * Sync status is informational — it enables the user to know
 * if both halves are running compatible firmware via LED pattern.
 */

static SplitCommState_t s_comm;

/* ── Remote event queue ───────────────────────────────────────── */

#define REMOTE_EVENT_QUEUE_SIZE  16U

static KeyEvent_t       s_remote_events[REMOTE_EVENT_QUEUE_SIZE];
static volatile uint8_t s_remote_head  = 0;
static volatile uint8_t s_remote_tail  = 0;
static volatile uint8_t s_remote_count = 0;

/* ── Private prototypes ───────────────────────────────────────── */

static bool     _BuildAndQueuePacket(uint8_t type,
                                     const uint8_t *payload, uint8_t len);
static bool     _TxStartFromBuf(const uint8_t *data, uint8_t len);
static bool     _TxQueue_Enqueue(const uint8_t *data, uint8_t len);
static bool     _TxQueue_Dequeue(uint8_t *data, uint8_t *len);
static void     _TxTryDrainQueue(void);

static void     _ProcessRxByte(uint8_t byte);
static void     _HandlePacket(const SplitPacket_t *pkt);
static void     _HandleKeyEvent(const SplitPacket_t *pkt);
static void     _HandleKeyState(const SplitPacket_t *pkt);
static void     _HandlePing(void);
static void     _HandlePong(void);
static void     _HandleSyncReq(const SplitPacket_t *pkt);
static void     _HandleSyncRsp(const SplitPacket_t *pkt);

static void     _PushRemoteEvent(const KeyEvent_t *event);
static void     _MasterHeartbeat(void);
static void     _CheckTimeout(void);
static void     _CheckRxParseTimeout(void);
static void     _CheckSyncTimeout(void);
static void     _UpdateStatusPattern(void);
static void     _SendSyncReq(void);
static void     _SendSyncRsp(void);
static void     _BuildSyncPayload(SplitSyncPayload_t *p);
static uint16_t _DMA_GetRxWritePos(void);

/* ═══════════════════════════════════════════════════════════════ */
/*                        PUBLIC API                               */
/* ═══════════════════════════════════════════════════════════════ */

void SplitComm_Init(void)
{
    memset(&s_comm, 0, sizeof(SplitCommState_t));

    s_comm.status              = SPLIT_DISCONNECTED;
    s_comm.rx_state            = RX_STATE_WAIT_SOF;
    s_comm.rx_dma_read_pos     = 0;
    s_comm.tx_busy             = false;
    s_comm.last_rx_tick        = HAL_GetTick();
    s_comm.last_heartbeat_tick = HAL_GetTick();
    s_comm.rx_state_timestamp  = HAL_GetTick();
    s_comm.tx_drop_count       = 0;

    s_comm.tx_queue.head  = 0;
    s_comm.tx_queue.tail  = 0;
    s_comm.tx_queue.count = 0;

    s_comm.sync_status         = SYNC_NOT_STARTED;
    s_comm.sync_req_tick       = 0;
    s_comm.remote_protocol_ver = 0;
    s_comm.remote_fw_ver       = 0;

    s_remote_head  = 0;
    s_remote_tail  = 0;
    s_remote_count = 0;

    HAL_UART_Receive_DMA(&huart1, s_comm.rx_dma_buf, SPLIT_RX_BUFFER_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

void SplitComm_Process(void)
{
    /* Drain DMA RX buffer */
    uint16_t write_pos = _DMA_GetRxWritePos();
    while (s_comm.rx_dma_read_pos != write_pos) {
        uint8_t byte = s_comm.rx_dma_buf[s_comm.rx_dma_read_pos];
        s_comm.rx_dma_read_pos =
            (s_comm.rx_dma_read_pos + 1U) % SPLIT_RX_BUFFER_SIZE;
        _ProcessRxByte(byte);
    }

    _CheckRxParseTimeout();

    if (IS_MASTER()) {
        _MasterHeartbeat();
        _CheckSyncTimeout();
    }

    _CheckTimeout();
    _UpdateStatusPattern();
    _TxTryDrainQueue();
}

bool SplitComm_SendKeyEvent(uint8_t key_index, uint8_t state)
{
    SplitKeyEventPayload_t payload;
    payload.key_index = key_index;
    payload.state     = state;
    payload.reserved  = 0U;
    return _BuildAndQueuePacket(SPLIT_PKT_KEY_EVENT,
                                (const uint8_t *)&payload, sizeof(payload));
}

bool SplitComm_SendKeyState(const uint8_t *row_state, uint8_t num_rows)
{
    if (num_rows > SPLIT_MAX_PAYLOAD) return false;
    return _BuildAndQueuePacket(SPLIT_PKT_KEY_STATE, row_state, num_rows);
}

SplitConnStatus_t SplitComm_GetStatus(void)
{
    return s_comm.status;
}

SplitSyncStatus_t SplitComm_GetSyncStatus(void)
{
    return s_comm.sync_status;
}

bool SplitComm_GetRemoteEvent(KeyEvent_t *event)
{
    if (event == NULL || s_remote_count == 0U) return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *event = s_remote_events[s_remote_head];
    s_remote_head = (s_remote_head + 1U) % REMOTE_EVENT_QUEUE_SIZE;
    s_remote_count--;
    __set_PRIMASK(primask);
    return true;
}

bool SplitComm_HasRemoteEvent(void)
{
    return (s_remote_count > 0U);
}

void SplitComm_TxCompleteCallback(void)
{
    s_comm.tx_busy = false;
    uint8_t data[SPLIT_MAX_PACKET_SIZE];
    uint8_t len = 0U;
    if (_TxQueue_Dequeue(data, &len)) {
        _TxStartFromBuf(data, len);
    }
}

const SplitCommState_t* SplitComm_GetState(void)
{
    return &s_comm;
}

/* ═══════════════════════════════════════════════════════════════ */
/*                         TX PATH                                 */
/* ═══════════════════════════════════════════════════════════════ */

static bool _BuildAndQueuePacket(uint8_t type,
                                 const uint8_t *payload, uint8_t len)
{
    if (len > SPLIT_MAX_PAYLOAD) return false;

    uint8_t pkt[SPLIT_MAX_PACKET_SIZE];
    uint8_t idx = 0U;

    pkt[idx++] = SPLIT_SOF;
    pkt[idx++] = type;
    pkt[idx++] = len;
    if (len > 0U && payload != NULL) {
        memcpy(&pkt[idx], payload, len);
        idx += len;
    }
    uint16_t crc = CRC16_Calculate(&pkt[1], 2U + len);
    pkt[idx++] = (uint8_t)(crc & 0xFFU);
    pkt[idx++] = (uint8_t)((crc >> 8U) & 0xFFU);

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool sent = false;
    if (!s_comm.tx_busy) {
        sent = _TxStartFromBuf(pkt, idx);
    }
    if (!sent) {
        if (!_TxQueue_Enqueue(pkt, idx)) {
            s_comm.tx_drop_count++;
            __set_PRIMASK(primask);
            return false;
        }
    }

    __set_PRIMASK(primask);
    s_comm.tx_packet_count++;
    return true;
}

static bool _TxStartFromBuf(const uint8_t *data, uint8_t len)
{
    if (s_comm.tx_busy) return false;
    memcpy(s_comm.tx_buf, data, len);
    s_comm.tx_busy = true;
    if (HAL_UART_Transmit_DMA(&huart1, s_comm.tx_buf, len) != HAL_OK) {
        s_comm.tx_busy = false;
        return false;
    }
    return true;
}

static bool _TxQueue_Enqueue(const uint8_t *data, uint8_t len)
{
    SplitTxQueue_t *q = &s_comm.tx_queue;
    if (q->count >= SPLIT_TX_QUEUE_SIZE) return false;
    memcpy(q->entries[q->tail].data, data, len);
    q->entries[q->tail].length = len;
    q->tail = (q->tail + 1U) % SPLIT_TX_QUEUE_SIZE;
    q->count++;
    return true;
}

static bool _TxQueue_Dequeue(uint8_t *data, uint8_t *len)
{
    SplitTxQueue_t *q = &s_comm.tx_queue;
    if (q->count == 0U) return false;
    memcpy(data, q->entries[q->head].data, q->entries[q->head].length);
    *len = q->entries[q->head].length;
    q->head = (q->head + 1U) % SPLIT_TX_QUEUE_SIZE;
    q->count--;
    return true;
}

static void _TxTryDrainQueue(void)
{
    if (s_comm.tx_busy || s_comm.tx_queue.count == 0U) return;
    uint8_t data[SPLIT_MAX_PACKET_SIZE];
    uint8_t len = 0U;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (!s_comm.tx_busy && _TxQueue_Dequeue(data, &len)) {
        _TxStartFromBuf(data, len);
    }
    __set_PRIMASK(primask);
}

/* ═══════════════════════════════════════════════════════════════ */
/*                         RX PATH                                 */
/* ═══════════════════════════════════════════════════════════════ */

static void _ProcessRxByte(uint8_t byte)
{
    switch (s_comm.rx_state) {
    case RX_STATE_WAIT_SOF:
        if (byte == SPLIT_SOF) {
            s_comm.rx_state           = RX_STATE_WAIT_TYPE;
            s_comm.rx_state_timestamp = HAL_GetTick();
        }
        break;
    case RX_STATE_WAIT_TYPE:
        s_comm.rx_packet.type = byte;
        s_comm.rx_state       = RX_STATE_WAIT_LEN;
        break;
    case RX_STATE_WAIT_LEN:
        s_comm.rx_packet.length = byte;
        s_comm.rx_payload_idx   = 0U;
        if (byte > SPLIT_MAX_PAYLOAD) {
            s_comm.rx_error_count++;
            s_comm.rx_state = RX_STATE_WAIT_SOF;
        } else if (byte == 0U) {
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
        s_comm.rx_packet.crc = byte;
        s_comm.rx_state      = RX_STATE_WAIT_CRC_H;
        break;
    case RX_STATE_WAIT_CRC_H:
        s_comm.rx_packet.crc |= ((uint16_t)byte << 8U);
        {
            uint8_t crc_buf[2U + SPLIT_MAX_PAYLOAD];
            crc_buf[0] = s_comm.rx_packet.type;
            crc_buf[1] = s_comm.rx_packet.length;
            if (s_comm.rx_packet.length > 0U) {
                memcpy(&crc_buf[2], s_comm.rx_packet.payload,
                       s_comm.rx_packet.length);
            }
            uint16_t calc_crc = CRC16_Calculate(crc_buf,
                                    2U + s_comm.rx_packet.length);
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

static void _CheckRxParseTimeout(void)
{
    if (s_comm.rx_state != RX_STATE_WAIT_SOF) {
        if ((HAL_GetTick() - s_comm.rx_state_timestamp)
                >= SPLIT_RX_PARSE_TIMEOUT_MS) {
            s_comm.rx_state = RX_STATE_WAIT_SOF;
            s_comm.rx_error_count++;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════ */
/*                      PACKET HANDLERS                            */
/* ═══════════════════════════════════════════════════════════════ */

static void _HandlePacket(const SplitPacket_t *pkt)
{
    s_comm.last_rx_tick = HAL_GetTick();

    /* Detect first connection */
    if (s_comm.status == SPLIT_DISCONNECTED) {
        s_comm.status = SPLIT_CONNECTED;

        /*
         * On first connection, Master initiates handshake.
         * Reset sync state so handshake runs for every reconnection.
         */
        if (IS_MASTER()) {
            s_comm.sync_status = SYNC_NOT_STARTED;
        }
    }

    switch (pkt->type) {
    case SPLIT_PKT_KEY_EVENT: _HandleKeyEvent(pkt); break;
    case SPLIT_PKT_KEY_STATE: _HandleKeyState(pkt); break;
    case SPLIT_PKT_PING:      _HandlePing();         break;
    case SPLIT_PKT_PONG:      _HandlePong();         break;
    case SPLIT_PKT_SYNC_REQ:  _HandleSyncReq(pkt);  break;
    case SPLIT_PKT_SYNC_RSP:  _HandleSyncRsp(pkt);  break;
    default:                                          break;
    }
}

static void _HandleKeyEvent(const SplitPacket_t *pkt)
{
    if (!IS_MASTER()) return;
    if (pkt->length < sizeof(SplitKeyEventPayload_t)) return;

    const SplitKeyEventPayload_t *kep =
        (const SplitKeyEventPayload_t *)pkt->payload;

    KeyEvent_t event;
    event.key_index = kep->key_index;
    event.row       = kep->key_index / MATRIX_COLS;
    event.col       = kep->key_index % MATRIX_COLS;
    event.state     = kep->state ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
    event.timestamp = HAL_GetTick();
    _PushRemoteEvent(&event);
}

static void _HandleKeyState(const SplitPacket_t *pkt)
{
    if (!IS_MASTER()) return;
    if (pkt->length < MATRIX_ROWS) return;

    static uint8_t s_prev_remote_state[MATRIX_ROWS] = {0};

    for (uint8_t row = 0; row < MATRIX_ROWS && row < pkt->length; row++) {
        uint8_t changed = s_prev_remote_state[row] ^ pkt->payload[row];
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (changed & (1U << col)) {
                bool pressed = (pkt->payload[row] >> col) & 0x01U;
                KeyEvent_t event;
                event.key_index = row * MATRIX_COLS + col;
                event.row       = row;
                event.col       = col;
                event.state     = pressed ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
                event.timestamp = HAL_GetTick();
                _PushRemoteEvent(&event);
            }
        }
        s_prev_remote_state[row] = pkt->payload[row];
    }
}

static void _HandlePing(void)
{
    if (!IS_SLAVE()) return;
    _BuildAndQueuePacket(SPLIT_PKT_PONG, NULL, 0U);
}

static void _HandlePong(void)
{
    if (!IS_MASTER()) return;
    /* PONG confirms connection is alive (already updated last_rx_tick) */
}

/*
 * Slave receives SYNC_REQ from Master.
 * Check compatibility, respond with SYNC_RSP.
 */
static void _HandleSyncReq(const SplitPacket_t *pkt)
{
    if (!IS_SLAVE()) return;
    if (pkt->length < sizeof(SplitSyncPayload_t)) return;

    const SplitSyncPayload_t *req =
        (const SplitSyncPayload_t *)pkt->payload;

    /* Store remote info */
    s_comm.remote_protocol_ver = req->protocol_version;
    s_comm.remote_fw_ver = ((uint16_t)req->fw_version_h << 8U) |
                            req->fw_version_l;

    /* Check compatibility */
    if (req->protocol_version == SPLIT_PROTOCOL_VERSION &&
        req->matrix_rows == MATRIX_ROWS &&
        req->matrix_cols == MATRIX_COLS) {
        s_comm.sync_status = SYNC_OK;
    } else {
        s_comm.sync_status = SYNC_MISMATCH;
    }

    /* Always respond so Master gets our info */
    _SendSyncRsp();
}

/*
 * Master receives SYNC_RSP from Slave.
 * Verify compatibility.
 */
static void _HandleSyncRsp(const SplitPacket_t *pkt)
{
    if (!IS_MASTER()) return;
    if (pkt->length < sizeof(SplitSyncPayload_t)) return;

    const SplitSyncPayload_t *rsp =
        (const SplitSyncPayload_t *)pkt->payload;

    s_comm.remote_protocol_ver = rsp->protocol_version;
    s_comm.remote_fw_ver = ((uint16_t)rsp->fw_version_h << 8U) |
                            rsp->fw_version_l;

    if (rsp->protocol_version == SPLIT_PROTOCOL_VERSION &&
        rsp->matrix_rows == MATRIX_ROWS &&
        rsp->matrix_cols == MATRIX_COLS) {
        s_comm.sync_status = SYNC_OK;
    } else {
        s_comm.sync_status = SYNC_MISMATCH;
    }
}

/* ═══════════════════════════════════════════════════════════════ */
/*                   SYNC HELPERS                                  */
/* ═══════════════════════════════════════════════════════════════ */

static void _BuildSyncPayload(SplitSyncPayload_t *p)
{
    p->protocol_version = SPLIT_PROTOCOL_VERSION;
    p->fw_version_h     = (uint8_t)((FIRMWARE_VERSION >> 8U) & 0xFFU);
    p->fw_version_l     = (uint8_t)(FIRMWARE_VERSION & 0xFFU);
    p->matrix_rows      = MATRIX_ROWS;
    p->matrix_cols      = MATRIX_COLS;
}

static void _SendSyncReq(void)
{
    SplitSyncPayload_t payload;
    _BuildSyncPayload(&payload);
    _BuildAndQueuePacket(SPLIT_PKT_SYNC_REQ,
                         (const uint8_t *)&payload, sizeof(payload));
    s_comm.sync_status   = SYNC_PENDING;
    s_comm.sync_req_tick = HAL_GetTick();
}

static void _SendSyncRsp(void)
{
    SplitSyncPayload_t payload;
    _BuildSyncPayload(&payload);
    _BuildAndQueuePacket(SPLIT_PKT_SYNC_RSP,
                         (const uint8_t *)&payload, sizeof(payload));
}

/* ═══════════════════════════════════════════════════════════════ */
/*                   REMOTE EVENT QUEUE                            */
/* ═══════════════════════════════════════════════════════════════ */

static void _PushRemoteEvent(const KeyEvent_t *event)
{
    if (s_remote_count >= REMOTE_EVENT_QUEUE_SIZE) {
        s_remote_head = (s_remote_head + 1U) % REMOTE_EVENT_QUEUE_SIZE;
        s_remote_count--;
    }
    s_remote_events[s_remote_tail] = *event;
    s_remote_tail = (s_remote_tail + 1U) % REMOTE_EVENT_QUEUE_SIZE;
    s_remote_count++;
}

/* ═══════════════════════════════════════════════════════════════ */
/*                  HEARTBEAT, SYNC, TIMEOUT                       */
/* ═══════════════════════════════════════════════════════════════ */

static void _MasterHeartbeat(void)
{
    uint32_t now = HAL_GetTick();
    if ((now - s_comm.last_heartbeat_tick) >= SPLIT_HEARTBEAT_MS) {
        s_comm.last_heartbeat_tick = now;

        /*
         * If connected and sync not yet started, initiate handshake
         * instead of sending a regular PING. The slave will respond
         * with SYNC_RSP which also serves as proof of life.
         */
        if (s_comm.status == SPLIT_CONNECTED &&
            s_comm.sync_status == SYNC_NOT_STARTED) {
            _SendSyncReq();
        } else {
            _BuildAndQueuePacket(SPLIT_PKT_PING, NULL, 0U);
        }
    }
}

/*
 * Check if SYNC_REQ timed out (no SYNC_RSP received).
 * Proceed anyway — the slave might be running old firmware
 * without SYNC support.
 */
static void _CheckSyncTimeout(void)
{
    if (s_comm.sync_status == SYNC_PENDING) {
        if ((HAL_GetTick() - s_comm.sync_req_tick) >= SPLIT_SYNC_TIMEOUT_MS) {
            s_comm.sync_status = SYNC_TIMEOUT;
        }
    }
}

static void _CheckTimeout(void)
{
    if (s_comm.status == SPLIT_CONNECTED) {
        if ((HAL_GetTick() - s_comm.last_rx_tick) > SPLIT_TIMEOUT_MS) {
            s_comm.status      = SPLIT_DISCONNECTED;
            s_comm.sync_status = SYNC_NOT_STARTED;
        }
    }
}

/*
 * Map connection + sync status to LED pattern.
 *
 * Priority:
 *   Disconnected       → BLINK_2HZ
 *   Connected + sync mismatch → BLINK_4HZ  (firmware mismatch warning)
 *   Connected + OK/pending/timeout → OFF
 *
 * main.c may further override to BLINK_4HZ for USB issues.
 */
static void _UpdateStatusPattern(void)
{
    if (s_comm.status != SPLIT_CONNECTED) {
        LedManager_SetStatusPattern(LED_STATUS_BLINK_2HZ);
    } else if (s_comm.sync_status == SYNC_MISMATCH) {
        LedManager_SetStatusPattern(LED_STATUS_BLINK_4HZ);
    } else {
        LedManager_SetStatusPattern(LED_STATUS_OFF);
    }
}

/* ═══════════════════════════════════════════════════════════════ */
/*                       DMA & CALLBACKS                           */
/* ═══════════════════════════════════════════════════════════════ */

static uint16_t _DMA_GetRxWritePos(void)
{
    uint16_t ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    return (uint16_t)((SPLIT_RX_BUFFER_SIZE - ndtr) % SPLIT_RX_BUFFER_SIZE);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        SplitComm_TxCompleteCallback();
    }
}

void SplitComm_UART_IdleCallback(void)
{
    /* Informational only in DMA circular mode */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);

        s_comm.rx_state        = RX_STATE_WAIT_SOF;
        s_comm.rx_dma_read_pos = 0U;

        HAL_UART_DMAStop(huart);
        HAL_UART_Receive_DMA(huart, s_comm.rx_dma_buf, SPLIT_RX_BUFFER_SIZE);
        s_comm.rx_error_count++;
    }
}
