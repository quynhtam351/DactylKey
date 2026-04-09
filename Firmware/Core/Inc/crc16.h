#ifndef __CRC16_H
#define __CRC16_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

uint16_t CRC16_Calculate(const uint8_t *data, size_t length);
uint16_t CRC16_Update(uint16_t crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif