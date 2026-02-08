/**
 * @file c64_gcr_track.h
 * @brief Commodore 64/1541 GCR Track Encoding
 * 
 * The 1541 uses 4-to-5 GCR encoding with variable track speeds:
 * - Tracks 1-17:  21 sectors (speed zone 3)
 * - Tracks 18-24: 19 sectors (speed zone 2)
 * - Tracks 25-30: 18 sectors (speed zone 1)
 * - Tracks 31-35: 17 sectors (speed zone 0)
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_C64_GCR_TRACK_H
#define UFT_C64_GCR_TRACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "c64.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C64/1541 GCR Constants */
#define C64_TRACKS               35
#define C64_TRACKS_EXTENDED      40   /* Some drives support 40 tracks */
#define C64_SECTOR_SIZE          256
#define C64_TOTAL_SECTORS        683  /* Standard D64 */

/* Speed zones */
#define C64_ZONE3_SECTORS        21   /* Tracks 1-17 */
#define C64_ZONE2_SECTORS        19   /* Tracks 18-24 */
#define C64_ZONE1_SECTORS        18   /* Tracks 25-30 */
#define C64_ZONE0_SECTORS        17   /* Tracks 31-35 */

/* Sync and markers */
#define C64_SYNC_BYTE            0xFF
#define C64_SYNC_COUNT           5    /* Minimum sync bytes */
#define C64_HEADER_ID            0x08
#define C64_DATA_ID              0x07

/* GCR encoding (4-to-5) */
extern const uint8_t c64_gcr_encode_nibble[16];
extern const uint8_t c64_gcr_decode_nibble[32];

/* Functions */
int c64_gcr_get_sectors_for_track(int track);
int c64_gcr_get_speed_zone(int track);
size_t c64_gcr_get_track_size(int track);

bool c64_gcr_encode_sector(const uint8_t *data, uint8_t *out,
                           uint8_t track, uint8_t sector, uint8_t id1, uint8_t id2);
bool c64_gcr_decode_sector(const uint8_t *gcr, size_t len, uint8_t *data,
                           uint8_t *track, uint8_t *sector);

/* Convert 4 bytes to 5 GCR bytes and vice versa */
void c64_gcr_encode_4to5(const uint8_t in[4], uint8_t out[5]);
bool c64_gcr_decode_5to4(const uint8_t in[5], uint8_t out[4]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_GCR_TRACK_H */
