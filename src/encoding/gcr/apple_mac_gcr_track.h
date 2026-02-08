/**
 * @file apple_mac_gcr_track.h
 * @brief Apple Macintosh GCR Track Encoding
 * 
 * Macintosh uses variable-speed GCR with different sector counts per zone:
 * - Tracks 0-15:  12 sectors (outer)
 * - Tracks 16-31: 11 sectors
 * - Tracks 32-47: 10 sectors
 * - Tracks 48-63:  9 sectors
 * - Tracks 64-79:  8 sectors (inner)
 * 
 * Uses 8-to-10 GCR encoding (8 data bits → 10 disk bits)
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_APPLE_MAC_GCR_TRACK_H
#define UFT_APPLE_MAC_GCR_TRACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Macintosh GCR Constants */
#define MAC_TRACKS               80
#define MAC_SIDES                2
#define MAC_SECTOR_SIZE          512
#define MAC_SECTOR_DATA_SIZE     524  /* 512 + 12 tag bytes */

/* Sectors per track by zone */
#define MAC_ZONE0_SECTORS        12   /* Tracks 0-15 */
#define MAC_ZONE1_SECTORS        11   /* Tracks 16-31 */
#define MAC_ZONE2_SECTORS        10   /* Tracks 32-47 */
#define MAC_ZONE3_SECTORS         9   /* Tracks 48-63 */
#define MAC_ZONE4_SECTORS         8   /* Tracks 64-79 */

/* Address/Data field markers */
#define MAC_ADDR_MARK_1          0xD5
#define MAC_ADDR_MARK_2          0xAA
#define MAC_ADDR_MARK_3          0x96
#define MAC_DATA_MARK_3          0xAD
#define MAC_SLIP_MARK_3          0xDC  /* Slip mark for resync */

/* Functions */
int mac_gcr_get_sectors_for_track(int track);
bool mac_gcr_encode_sector(const uint8_t *data, const uint8_t *tags,
                           uint8_t *out, int track, int sector, int side);
bool mac_gcr_decode_sector(const uint8_t *gcr, size_t len,
                           uint8_t *data, uint8_t *tags,
                           int *track, int *sector, int *side);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_MAC_GCR_TRACK_H */
