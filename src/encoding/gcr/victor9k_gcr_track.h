/**
 * @file victor9k_gcr_track.h
 * @brief Victor 9000/Sirius 1 GCR Track Encoding
 * 
 * The Victor 9000 uses a unique variable-speed GCR format:
 * - 80 tracks, double-sided
 * - Variable sectors per track (19-12 depending on zone)
 * - 512 bytes per sector
 * - Custom GCR encoding
 * 
 * Speed zones (approximate):
 * - Tracks 0-3:   19 sectors
 * - Tracks 4-15:  18 sectors
 * - Tracks 16-26: 17 sectors
 * - Tracks 27-37: 16 sectors
 * - Tracks 38-47: 15 sectors
 * - Tracks 48-59: 14 sectors
 * - Tracks 60-70: 13 sectors
 * - Tracks 71-79: 12 sectors
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_VICTOR9K_GCR_TRACK_H
#define UFT_VICTOR9K_GCR_TRACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Victor 9000 Constants */
#define VICTOR9K_TRACKS          80
#define VICTOR9K_SIDES           2
#define VICTOR9K_SECTOR_SIZE     512
#define VICTOR9K_MAX_SECTORS     19
#define VICTOR9K_MIN_SECTORS     12

/* Sync pattern */
#define VICTOR9K_SYNC_BYTE       0xFF
#define VICTOR9K_SYNC_COUNT      10

/* Sector header */
#define VICTOR9K_HEADER_MARK     0x01
#define VICTOR9K_DATA_MARK       0x02

/* Functions */
int victor9k_get_sectors_for_track(int track);
int victor9k_get_speed_zone(int track);
size_t victor9k_get_track_size(int track);

bool victor9k_gcr_encode_sector(const uint8_t *data, uint8_t *out,
                                 int track, int sector, int side);
bool victor9k_gcr_decode_sector(const uint8_t *gcr, size_t len,
                                 uint8_t *data, int *track, int *sector);

/* Interleave table for optimal access */
extern const uint8_t victor9k_interleave[VICTOR9K_MAX_SECTORS];

#ifdef __cplusplus
}
#endif

#endif /* UFT_VICTOR9K_GCR_TRACK_H */
