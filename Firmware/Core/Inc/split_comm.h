#ifndef __SPLIT_COMM_H
#define __SPLIT_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keyboard_config.h"
#include "matrix_driver.h"

/* ========================================================================
 *  Packet Protocol Constants
 * ======================================================================== */
#define SPLIT_SOF               0xAAU

/* Packet types */
#define SPLIT_PKT_KEY_STATE     0x01U   /* Full matrix state (slave→master) */
#define SPLIT_PKT_KEY_EVENT     0x02U   /* Single key event (slave→master)  */
#define SPLIT_PKT_PING          0x03U   /* Heartbeat request (master→slave) */
#define SPLIT_PKT_PONG          0x04U   /* Heartbeat response (slave→master)*/
#define SPLIT_PKT_SYNC_REQ      0x10U   /* Sync request (master→slave)      */
#define SPLIT_PKT_SYNC_RSP      0x11U   /* Sync response (slave→master)     */

/* Packet size limits */
#define SPLIT_MAX_PAYLOAD       16U
#define SPLIT_HEADER_SIZE       3U      /* SOF + TYPE + LENGTH */
#define SPLIT_CRC_SIZE          2U
#define SPLIT_MAX_PACKET_SIZE   (SPLIT_HEADER_SIZE + SPLIT_MAX_PAYLOAD + SPLIT_CRC_SIZE)

/* RX parser states */
typedef enum {
    RX_STATE_WAIT_SOF = 0,
    RX_STATE_WAIT_TYPE,
    RX_STATE_WAIT_LEN,
    RX_STATE_WAIT_PAYLOAD,
    RX_STATE_WAIT_CRC_L,
    RX_STATE_WAIT_CRC_H
} SplitRxState_t;

/* ========================================================================
 *  Data Structures
 * ======================================================================== */

/* Parsed packet */
typedef struct {
    uint8_t  type;
    uint8_t  length;
    uint8_t  payload[SPLIT_MAX_PAYLOAD];
    uint16_t crc;
} SplitPacket_t;

/* KEY_EVENT payload (3 bytes) */
typedef struct {
    uint8_t key_index;  /* 0-43, local to slave half */
    uint8_t state;      /* 0=released, 1=pressed */
    uint8_t reserved;
} __attribute__((packed)) SplitKeyEventPayload_t;

/* KEY_STATE payload (MATRIX_ROWS bytes) */
typedef struct {
    uint8_t row_state[MATRIX_ROWS];
} __attribute__((packed)) SplitKeyStatePayload_t;

/* Connection status */
typedef enum {
    SPLIT_DISCONNECTED = 0,
    SPLIT_CONNECTED    = 1
} SplitConnStatus_t;

/* Module state */
typedef struct {
    SplitConnStatus_t   status;
    uint32_t            last_rx_tick;       /* Last successful RX timestamp */
    uint32_t            last_heartbeat_tick;/* Last heartbeat sent */
    uint32_t            rx_packet_count;
    uint32_t            rx_error_count;
    uint32_t            tx_packet_count;

    /* RX parser */
    SplitRxState_t      rx_state;
    SplitPacket_t       rx_packet;
    uint8_t             rx_payload_idx;

    /* DMA RX circular buffer */
    uint8_t             rx_dma_buf[SPLIT_RX_BUFFER_SIZE];
    volatile uint16_t   rx_dma_read_pos;

    /* TX buffer */
    uint8_t             tx_buf[SPLIT_MAX_PACKET_SIZE];
    volatile bool       tx_busy;
} SplitCommState_t;

/* ========================================================================
 *  Public API
 * ======================================================================== */

void SplitComm_Init(void);

/* Call from main loop */
void SplitComm_Process(void);

/* Slave: send key event to master */
bool SplitComm_SendKeyEvent(uint8_t key_index, uint8_t state);

/* Slave: send full matrix state to master */
bool SplitComm_SendKeyState(const uint8_t *row_state, uint8_t num_rows);

/* Master: check if slave is connected */
SplitConnStatus_t SplitComm_GetStatus(void);

/* Master: get remote key event (from slave) */
bool SplitComm_GetRemoteEvent(KeyEvent_t *event);

/* Master: check if remote events available */
bool SplitComm_HasRemoteEvent(void);

/* TX complete callback (called from DMA ISR) */
void SplitComm_TxCompleteCallback(void);

/* Get stats for debug */
const SplitCommState_t* SplitComm_GetState(void);

#ifdef __cplusplus
}
#endif

#endif