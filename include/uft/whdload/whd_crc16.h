#pragma once
/*
 * whd_crc16.h — CRC16 helper for WHDLoad-style version checks (TDREASON_WRONGVER mentions CRC16).
 * Implements CRC-16/IBM (ANSI) reflected poly 0xA001, init 0x0000, xorout 0x0000.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t uft_crc16_ansi(const void *data, size_t len);

#ifdef __cplusplus
}
#endif
