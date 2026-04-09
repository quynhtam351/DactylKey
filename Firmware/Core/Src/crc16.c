#include "crc16.h"

/* CRC-16/CCITT-FALSE: poly=0x1021, init=0xFFFF */
uint16_t CRC16_Update(uint16_t crc, uint8_t byte)
{
    crc ^= ((uint16_t)byte << 8);
    for (uint8_t i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

uint16_t CRC16_Calculate(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = CRC16_Update(crc, data[i]);
    }
    return crc;
}