#ifndef __SPLIT_COMM_H
#define __SPLIT_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keyboard_config.h"
#include "matrix_driver.h"

/* ── Framing ──────────────────────────────────────────────────── */

#define SPLIT_SOF               0xAAU

/* ── Packet types ─────────────────────────────────────────────── */

#define SPLIT_PKT_KEY_STATE     0x01U
#define SPLIT_PKT_KEY_EVENT     0x02U
#define SPLIT_PKT_PING          0x03U
#define SPLIT_PKT_PONG          0x04U
#define SPLIT_PKT_SYNC_REQ      0x10U
#define SPLIT_PKT_SYNC_RSP      0x11U

/* ── Limits ───────────────────────────────────────────────────── */

#define SPLIT_MAX_PAYLOAD           16U
#define SPLIT_HEADER_SIZE           3U
#define SPLIT_CRC_SIZE              2U
#define SPLIT_MAX_PACKET_SIZE       (SPLIT_HEADER_SIZE + SPLIT_MAX_PAYLOAD + SPLIT_CRC_SIZE)
#define SPLIT_TX_QUEUE_SIZE         16U
#define SPLIT_RX_PARSE_TIMEOUT_MS   10U

/* ── RX state machine ─────────────────────────────────────────── */

typedef enum {
    RX_STATE_WAIT_SOF = 0,
    RX_STATE_WAIT_TYPE,
    RX_STATE_WAIT_LEN,
    RX_STATE_WAIT_PAYLOAD,
    RX_STATE_WAIT_CRC_L,
    RX_STATE_WAIT_CRC_H
} SplitRxState_t;

/* ── Packet ───────────────────────────────────────────────────── */

typedef struct {
    uint8_t  type;
    uint8_t  length;
    uint8_t  payload[SPLIT_MAX_PAYLOAD];
    uint16_t crc;
} SplitPacket_t;

/* ── Payload structures ───────────────────────────────────────── */

typedef struct {
    uint8_t key_index;
    uint8_t state;
    uint8_t reserved;
} __attribute__((packed)) SplitKeyEventPayload_t;

typedef struct {
    uint8_t row_state[MATRIX_ROWS];
} __attribute__((packed)) SplitKeyStatePayload_t;

/* ── Connection status ────────────────────────────────────────── */

typedef enum {
    SPLIT_DISCONNECTED = 0,
    SPLIT_CONNECTED    = 1
} SplitConnStatus_t;

/* ── TX queue ─────────────────────────────────────────────────── */

typedef struct {
    uint8_t data[SPLIT_MAX_PACKET_SIZE];
    uint8_t length;
} SplitTxEntry_t;

typedef struct {
    SplitTxEntry_t  entries[SPLIT_TX_QUEUE_SIZE];
    uint8_t         head;
    uint8_t         tail;
    uint8_t         count;
} SplitTxQueue_t;

/* ── Comm state ───────────────────────────────────────────────── */

typedef struct {
    SplitConnStatus_t   status;
    uint32_t            last_rx_tick;
    uint32_t            last_heartbeat_tick;
    uint32_t            rx_packet_count;
    uint32_t            rx_error_count;
    uint32_t            tx_packet_count;
    uint32_t            tx_drop_count;

    SplitRxState_t      rx_state;
    SplitPacket_t       rx_packet;
    uint8_t             rx_payload_idx;
    uint32_t            rx_state_timestamp;

    uint8_t             rx_dma_buf[SPLIT_RX_BUFFER_SIZE];
    volatile uint16_t   rx_dma_read_pos;

    uint8_t             tx_buf[SPLIT_MAX_PACKET_SIZE];
    volatile bool       tx_busy;
    SplitTxQueue_t      tx_queue;
} SplitCommState_t;

/* ── Public API ───────────────────────────────────────────────── */

void SplitComm_Init(void);
void SplitComm_Process(void);

bool SplitComm_SendKeyEvent(uint8_t key_index, uint8_t state);
bool SplitComm_SendKeyState(const uint8_t *row_state, uint8_t num_rows);

SplitConnStatus_t   SplitComm_GetStatus(void);
bool                SplitComm_GetRemoteEvent(KeyEvent_t *event);
bool                SplitComm_HasRemoteEvent(void);

void SplitComm_TxCompleteCallback(void);
void SplitComm_UART_IdleCallback(void);

const SplitCommState_t* SplitComm_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
