/**
 * @file apple2_gcr_track.h
 * @brief Apple II GCR Track Encoding
 * 
 * Apple II uses 6-and-2 GCR encoding (6 data bits encoded as 8 disk bits).
 * 
 * Track format:
 * - 16 sectors per track (DOS 3.3) or 13 sectors (DOS 3.2)
 * - Self-sync bytes (FF 40-bit patterns)
 * - Address field: D5 AA 96 [vol] [trk] [sec] [chk] DE AA EB
 * - Data field: D5 AA AD [342 bytes] [checksum] DE AA EB
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_APPLE2_GCR_TRACK_H
#define UFT_APPLE2_GCR_TRACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Apple II GCR Constants */
#define APPLE2_SECTORS_DOS33     16
#define APPLE2_SECTORS_DOS32     13
#define APPLE2_SECTOR_SIZE       256
#define APPLE2_TRACKS            35
#define APPLE2_TRACK_SIZE_BITS   51200  /* ~6400 bytes */

/* Prologue/Epilogue markers */
#define APPLE2_ADDR_PROLOGUE_1   0xD5
#define APPLE2_ADDR_PROLOGUE_2   0xAA
#define APPLE2_ADDR_PROLOGUE_3   0x96
#define APPLE2_DATA_PROLOGUE_3   0xAD
#define APPLE2_EPILOGUE_1        0xDE
#define APPLE2_EPILOGUE_2        0xAA
#define APPLE2_EPILOGUE_3        0xEB

/* 6-and-2 encoding tables */
extern const uint8_t apple2_gcr_encode_6and2[64];
extern const uint8_t apple2_gcr_decode_6and2[256];

/* Functions */
bool apple2_gcr_encode_sector(const uint8_t *data, uint8_t *out, 
                               uint8_t volume, uint8_t track, uint8_t sector);
bool apple2_gcr_decode_sector(const uint8_t *gcr, size_t len, uint8_t *data,
                               uint8_t *volume, uint8_t *track, uint8_t *sector);
bool apple2_gcr_find_sector(const uint8_t *track_data, size_t len,
                             uint8_t sector, size_t *offset);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE2_GCR_TRACK_H */
