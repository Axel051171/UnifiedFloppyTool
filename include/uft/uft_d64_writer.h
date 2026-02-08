/**
 * @file uft_d64_writer.h
 * @brief D64 Writer with Accurate Gap Timing
 * 
 * P2-006: Writer Gap-Timing D64
 * 
 * Creates authentic 1541 disk images with:
 * - Accurate GCR encoding
 * - Proper inter-sector gaps
 * - Correct sync patterns
 * - Zone-based timing (speed zones 0-3)
 * - Header/data block checksums
 * 
 * 1541 Track Layout:
 * Zone 0 (Tracks 1-17):  21 sectors, 3.25 ms/revolution
 * Zone 1 (Tracks 18-24): 19 sectors, 3.50 ms/revolution
 * Zone 2 (Tracks 25-30): 18 sectors, 3.75 ms/revolution
 * Zone 3 (Tracks 31-35): 17 sectors, 4.00 ms/revolution
 */

#ifndef UFT_D64_WRITER_H
#define UFT_D64_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define D64_TRACK_COUNT         35
#define D64_TRACK_COUNT_EXT     40    /* Extended D64 */
#define D64_SECTOR_SIZE         256
#define D64_GCR_SECTOR_SIZE     325   /* After GCR encoding */
#define D64_HEADER_SIZE         10    /* 8 bytes → 10 GCR */
#define D64_DATA_SIZE           325   /* 260 bytes → 325 GCR */

/* Sync marks */
#define D64_SYNC_BYTE           0xFF
#define D64_SYNC_COUNT          5     /* Standard sync length */
#define D64_HEADER_MARK         0x08  /* Header block ID */
#define D64_DATA_MARK           0x07  /* Data block ID */

/* Speed zones (bits per cell in us @ 300 RPM) */
#define D64_ZONE0_BIT_TIME_US   4.0   /* Tracks 1-17 */
#define D64_ZONE1_BIT_TIME_US   3.75  /* Tracks 18-24 */
#define D64_ZONE2_BIT_TIME_US   3.5   /* Tracks 25-30 */
#define D64_ZONE3_BIT_TIME_US   3.25  /* Tracks 31-35+ */

/* Gap lengths (in GCR bytes) */
#define D64_GAP1_LENGTH         9     /* After header, before data */
#define D64_GAP2_LENGTH         9     /* After data, before next header */
#define D64_HEADER_GAP          5     /* Minimum gap before header */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Speed zone for track
 */
typedef enum {
    D64_ZONE_0 = 0,  /* 21 sectors, fastest */
    D64_ZONE_1 = 1,  /* 19 sectors */
    D64_ZONE_2 = 2,  /* 18 sectors */
    D64_ZONE_3 = 3   /* 17 sectors, slowest */
} d64_speed_zone_t;

/**
 * @brief Sector interleave patterns
 */
typedef enum {
    D64_INTERLEAVE_STANDARD = 10,   /* Standard 1541 interleave */
    D64_INTERLEAVE_FAST     = 6,    /* Fast loader interleave */
    D64_INTERLEAVE_CUSTOM   = 0     /* Use custom table */
} d64_interleave_t;

/**
 * @brief D64 sector header (8 bytes raw → 10 GCR)
 */
typedef struct {
    uint8_t block_id;      /* 0x08 for header */
    uint8_t checksum;      /* XOR of track, sector, id1, id2 */
    uint8_t sector;        /* Sector number (0-20) */
    uint8_t track;         /* Track number (1-35) */
    uint8_t id2;           /* Disk ID byte 2 */
    uint8_t id1;           /* Disk ID byte 1 */
    uint8_t padding[2];    /* 0x0F padding */
} d64_header_t;

/**
 * @brief D64 data block (260 bytes raw → 325 GCR)
 */
typedef struct {
    uint8_t block_id;      /* 0x07 for data */
    uint8_t data[256];     /* Sector data */
    uint8_t checksum;      /* XOR of all data bytes */
    uint8_t padding[2];    /* 0x00 padding */
} d64_data_block_t;

/**
 * @brief Writer configuration
 */
typedef struct {
    /* Timing options */
    bool accurate_timing;      /* Use real 1541 timing */
    bool variable_gaps;        /* Vary gap lengths slightly */
    int gap1_length;           /* Override Gap1 length (-1 = default) */
    int gap2_length;           /* Override Gap2 length (-1 = default) */
    int sync_length;           /* Sync byte count (default 5) */
    
    /* Format options */
    uint8_t disk_id[2];        /* Disk ID bytes */
    d64_interleave_t interleave;
    uint8_t *custom_interleave;/* Custom interleave table (if CUSTOM) */
    int custom_interleave_len;
    
    /* Extended format */
    bool extended_tracks;      /* Write tracks 36-40 */
    int track_count;           /* Total tracks (35 or 40) */
    
    /* Output options */
    bool include_error_info;   /* Include error byte per sector */
    bool generate_g64;         /* Output G64 instead of D64 */
    bool flux_output;          /* Generate flux timing data */
} d64_writer_config_t;

/**
 * @brief Default configuration
 */
#define D64_WRITER_CONFIG_DEFAULT { \
    .accurate_timing = true, \
    .variable_gaps = false, \
    .gap1_length = -1, \
    .gap2_length = -1, \
    .sync_length = 5, \
    .disk_id = {0x30, 0x30}, \
    .interleave = D64_INTERLEAVE_STANDARD, \
    .custom_interleave = NULL, \
    .custom_interleave_len = 0, \
    .extended_tracks = false, \
    .track_count = 35, \
    .include_error_info = false, \
    .generate_g64 = false, \
    .flux_output = false \
}

/**
 * @brief Track write result
 */
typedef struct {
    int track;
    int sectors_written;
    int gcr_bytes;
    double track_time_ms;
    int errors;
    char error_msg[64];
} d64_track_result_t;

/**
 * @brief Writer context
 */
typedef struct d64_writer d64_writer_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create D64 writer
 */
d64_writer_t* d64_writer_create(const d64_writer_config_t *config);

/**
 * @brief Destroy writer
 */
void d64_writer_destroy(d64_writer_t *writer);

/**
 * @brief Write D64 from sector data
 * @param writer Writer context
 * @param sectors Sector data (174848 bytes for 35 tracks)
 * @param sector_count Number of sectors
 * @param output Output buffer (must be large enough)
 * @param output_size Size written to output
 * @return 0 on success
 */
int d64_writer_write(
    d64_writer_t *writer,
    const uint8_t *sectors,
    int sector_count,
    uint8_t *output,
    size_t *output_size);

/**
 * @brief Write single track to GCR
 */
int d64_write_track_gcr(
    d64_writer_t *writer,
    int track,
    const uint8_t *sector_data,
    int sector_count,
    uint8_t *gcr_output,
    size_t *gcr_size,
    d64_track_result_t *result);

/**
 * @brief Get sectors per track for given track number
 */
int d64_sectors_per_track(int track);

/**
 * @brief Get speed zone for track
 */
d64_speed_zone_t d64_track_zone(int track);

/**
 * @brief Get bit time in microseconds for zone
 */
double d64_zone_bit_time(d64_speed_zone_t zone);

/**
 * @brief Calculate track length in bits
 */
int d64_track_length_bits(int track);

/**
 * @brief Get track length in GCR bytes
 */
int d64_track_length_gcr(int track);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Low-Level GCR Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 */
void d64_gcr_encode_4to5(const uint8_t *data, uint8_t *gcr);

/**
 * @brief Decode 5 GCR bytes to 4 bytes
 */
int d64_gcr_decode_5to4(const uint8_t *gcr, uint8_t *data);

/**
 * @brief Encode sector header to GCR
 */
void d64_encode_header(const d64_header_t *header, uint8_t *gcr);

/**
 * @brief Encode data block to GCR
 */
void d64_encode_data_block(const d64_data_block_t *block, uint8_t *gcr);

/**
 * @brief Calculate header checksum
 */
uint8_t d64_header_checksum(int track, int sector, uint8_t id1, uint8_t id2);

/**
 * @brief Calculate data checksum
 */
uint8_t d64_data_checksum(const uint8_t *data, int size);

/**
 * @brief Write sync bytes
 */
void d64_write_sync(uint8_t *output, int count);

/**
 * @brief Write gap bytes
 */
void d64_write_gap(uint8_t *output, int count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Flux Output (for SCP/G64)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert GCR track to flux timing
 */
int d64_gcr_to_flux(
    const uint8_t *gcr_data,
    size_t gcr_size,
    d64_speed_zone_t zone,
    uint32_t *flux_output,
    size_t *flux_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_WRITER_H */
