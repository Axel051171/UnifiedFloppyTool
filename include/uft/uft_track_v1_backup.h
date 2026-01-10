/**
 * @file uft_track.h
 * @brief Central Track Type Definition
 */
#ifndef UFT_TRACK_H
#define UFT_TRACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_MAX_SECTORS 64

/* Only define if not already defined in uft_types.h */
#ifndef UFT_SECTOR_T_DEFINED
#define UFT_SECTOR_T_DEFINED

/** Sector data structure */
typedef struct uft_sector {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector_id;
    uint8_t size_code;
    uint16_t logical_size;
    uint8_t *data;
    size_t data_len;
    uint16_t crc_stored;
    uint16_t crc_calculated;
    bool crc_ok;
    bool deleted;
    bool weak;
    int read_count;
    float confidence;
} uft_sector_t;

#endif /* UFT_SECTOR_T_DEFINED */

#ifndef UFT_TRACK_T_DEFINED
#define UFT_TRACK_T_DEFINED

/** Track data structure */
typedef struct uft_track {
    uint8_t cylinder;
    uint8_t head;
    uint32_t encoding;
    uint32_t bitrate;
    uint32_t rpm;
    
    uint8_t *raw_data;
    size_t raw_len;
    
    uft_sector_t sectors[UFT_MAX_SECTORS];
    int sector_count;
    
    uint32_t *flux_data;
    size_t flux_count;
    
    bool decoded;
    int errors;
    float quality;
} uft_track_t;

#endif /* UFT_TRACK_T_DEFINED */

void uft_track_init(uft_track_t *track);
void uft_track_free(uft_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_H */
