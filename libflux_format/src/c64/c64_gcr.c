// SPDX-License-Identifier: MIT
/*
 * c64_gcr.c - Commodore 64 GCR Encoding/Decoding
 * 
 * Professional implementation of C64 Group Code Recording.
 * Based on documented GCR algorithms and 1541 disk format specifications.
 * 
 * GCR Format:
 *   - 4 data bits → 5 GCR bits encoding
 *   - Sync pattern: 0xFF (10 consecutive 1's)
 *   - Sector structure: SYNC + HEADER + GAP + SYNC + DATA
 * 
 * References:
 *   - C64 Programmer's Reference Guide
 *   - 1541 Drive ROM Disassembly
 *   - Public domain GCR specifications
 * 
 * @version 2.7.3
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================*
 * C64 GCR CONSTANTS
 *============================================================================*/

#define C64_MAX_TRACKS_1541     42
#define C64_SECTORS_PER_TRACK   21  /* Maximum */
#define C64_SECTOR_SIZE         256

/* GCR Structure lengths */
#define C64_SYNC_LENGTH         5
#define C64_HEADER_LENGTH       10
#define C64_HEADER_GAP          9
#define C64_DATA_LENGTH         325  /* 65 * 5 GCR bytes */

/* Speed zones (tracks per zone) */
#define C64_SPEED_ZONE_0        17   /* Tracks 1-17: 3 MHz */
#define C64_SPEED_ZONE_1        24   /* Tracks 18-24: 2.86 MHz */
#define C64_SPEED_ZONE_2        30   /* Tracks 25-30: 2.67 MHz */
#define C64_SPEED_ZONE_3        42   /* Tracks 31-42: 2.5 MHz */

/* Sectors per track by zone */
static const uint8_t sectors_per_track[C64_MAX_TRACKS_1541 + 1] = {
    0,  /* Track 0 (unused) */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, /* 1-10 */
    21, 21, 21, 21, 21, 21, 21,             /* 11-17 */
    19, 19, 19, 19, 19, 19, 19,             /* 18-24 */
    18, 18, 18, 18, 18, 18,                 /* 25-30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, /* 31-40 */
    17, 17                                   /* 41-42 */
};

/* Track capacity in bytes (approximate) */
static const size_t track_capacity[C64_MAX_TRACKS_1541 + 1] = {
    0,      /* Track 0 */
    7820, 7820, 7820, 7820, 7820, 7820, 7820, 7820, 7820, 7820,
    7820, 7820, 7820, 7820, 7820, 7820, 7820,
    7170, 7170, 7170, 7170, 7170, 7170, 7170,
    6300, 6300, 6300, 6300, 6300, 6300,
    6020, 6020, 6020, 6020, 6020, 6020, 6020, 6020, 6020, 6020,
    6020, 6020
};

/*============================================================================*
 * GCR ENCODING TABLES
 *============================================================================*/

/* 4-bit to 5-bit GCR encoding table */
static const uint8_t gcr_encode_table[16] = {
    0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17,
    0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15
};

/* 5-bit GCR to 4-bit data decoding table (inverse) */
static const uint8_t gcr_decode_table[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x00-0x07 */
    0xff, 0x08, 0x00, 0x01, 0xff, 0x0c, 0x04, 0x05, /* 0x08-0x0f */
    0xff, 0xff, 0x02, 0x03, 0xff, 0x0f, 0x06, 0x07, /* 0x10-0x17 */
    0xff, 0x09, 0x0a, 0x0b, 0xff, 0x0d, 0x0e, 0xff  /* 0x18-0x1f */
};

/*============================================================================*
 * C64 STRUCTURES
 *============================================================================*/

/**
 * @brief C64 sector header
 */
typedef struct {
    uint8_t checksum;       /* Header checksum */
    uint8_t sector;         /* Sector number */
    uint8_t track;          /* Track number */
    uint8_t id2;            /* Disk ID byte 2 */
    uint8_t id1;            /* Disk ID byte 1 */
} c64_sector_header_t;

/**
 * @brief C64 sector
 */
typedef struct {
    uint8_t track;
    uint8_t sector;
    uint8_t data[C64_SECTOR_SIZE];
    uint8_t disk_id[2];
    bool valid;
    bool crc_ok;
} c64_sector_t;

/**
 * @brief C64 track
 */
typedef struct {
    uint8_t track_num;
    uint8_t *gcr_data;
    size_t gcr_length;
    c64_sector_t *sectors;
    uint8_t sector_count;
} c64_track_t;

/*============================================================================*
 * GCR ENCODING/DECODING
 *============================================================================*/

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 * 
 * @param data 4 input bytes
 * @param gcr 5 output GCR bytes
 */
static void encode_4bytes_to_gcr(const uint8_t data[4], uint8_t gcr[5])
{
    /* Split each byte into nibbles, encode to GCR */
    uint8_t nib_hi[4], nib_lo[4];
    
    for (int i = 0; i < 4; i++) {
        nib_hi[i] = gcr_encode_table[data[i] >> 4];
        nib_lo[i] = gcr_encode_table[data[i] & 0x0f];
    }
    
    /* Pack into 5 GCR bytes (40 bits) */
    gcr[0] = (nib_hi[0] << 3) | (nib_lo[0] >> 2);
    gcr[1] = (nib_lo[0] << 6) | (nib_hi[1] << 1) | (nib_lo[1] >> 4);
    gcr[2] = (nib_lo[1] << 4) | (nib_hi[2] >> 1);
    gcr[3] = (nib_hi[2] << 7) | (nib_lo[2] << 2) | (nib_hi[3] >> 3);
    gcr[4] = (nib_hi[3] << 5) | nib_lo[3];
}

/**
 * @brief Decode 5 GCR bytes to 4 bytes
 * 
 * @param gcr 5 input GCR bytes
 * @param data 4 output bytes
 * @return 0 on success, -1 if invalid GCR
 */
static int decode_gcr_to_4bytes(const uint8_t gcr[5], uint8_t data[4])
{
    /* Unpack 5 GCR bytes to 8 nibbles */
    uint8_t nib[8];
    
    nib[0] = (gcr[0] >> 3) & 0x1f;
    nib[1] = ((gcr[0] << 2) | (gcr[1] >> 6)) & 0x1f;
    nib[2] = (gcr[1] >> 1) & 0x1f;
    nib[3] = ((gcr[1] << 4) | (gcr[2] >> 4)) & 0x1f;
    nib[4] = ((gcr[2] << 1) | (gcr[3] >> 7)) & 0x1f;
    nib[5] = (gcr[3] >> 2) & 0x1f;
    nib[6] = ((gcr[3] << 3) | (gcr[4] >> 5)) & 0x1f;
    nib[7] = gcr[4] & 0x1f;
    
    /* Decode nibbles to bytes */
    for (int i = 0; i < 4; i++) {
        uint8_t hi = gcr_decode_table[nib[i * 2]];
        uint8_t lo = gcr_decode_table[nib[i * 2 + 1]];
        
        if (hi == 0xff || lo == 0xff) {
            return -1;  /* Invalid GCR */
        }
        
        data[i] = (hi << 4) | lo;
    }
    
    return 0;
}

/*============================================================================*
 * SYNC DETECTION
 *============================================================================*/

/**
 * @brief Find GCR sync pattern
 * 
 * Searches for 0xFF bytes (10 consecutive 1's in GCR stream).
 * 
 * @param gcr GCR data
 * @param length GCR data length
 * @param start Start position
 * @return Position of sync, or -1 if not found
 */
static int find_sync_pattern(const uint8_t *gcr, size_t length, size_t start)
{
    for (size_t i = start; i < length - C64_SYNC_LENGTH; i++) {
        /* Check for sequence of 0xFF bytes */
        bool is_sync = true;
        
        for (int j = 0; j < C64_SYNC_LENGTH; j++) {
            if (gcr[i + j] != 0xFF) {
                is_sync = false;
                break;
            }
        }
        
        if (is_sync) {
            return (int)(i + C64_SYNC_LENGTH);
        }
    }
    
    return -1;
}

/*============================================================================*
 * SECTOR DECODING
 *============================================================================*/

/**
 * @brief Decode sector header from GCR
 * 
 * @param gcr_data GCR data starting after sync
 * @param header Output header structure
 * @return 0 on success
 */
static int decode_sector_header(const uint8_t *gcr_data, c64_sector_header_t *header)
{
    uint8_t decoded[8];
    
    /* Header is 10 GCR bytes → 8 data bytes */
    if (decode_gcr_to_4bytes(gcr_data, decoded) < 0) {
        return -1;
    }
    
    if (decode_gcr_to_4bytes(gcr_data + 5, decoded + 4) < 0) {
        return -1;
    }
    
    /* Check header marker (should be 0x08) */
    if (decoded[0] != 0x08) {
        return -1;
    }
    
    /* Extract header fields */
    header->checksum = decoded[1];
    header->sector = decoded[2];
    header->track = decoded[3];
    header->id2 = decoded[4];
    header->id1 = decoded[5];
    
    /* Verify checksum */
    uint8_t calc_checksum = decoded[2] ^ decoded[3] ^ decoded[4] ^ decoded[5];
    
    if (calc_checksum != header->checksum) {
        return -1;  /* Bad checksum */
    }
    
    return 0;
}

/**
 * @brief Decode sector data from GCR
 * 
 * @param gcr_data GCR data starting after sync
 * @param sector Output sector structure
 * @return 0 on success
 */
static int decode_sector_data(const uint8_t *gcr_data, c64_sector_t *sector)
{
    uint8_t decoded[260];  /* 256 data + 1 marker + 1 checksum + padding */
    
    /* Data block is 325 GCR bytes → 260 data bytes */
    for (int i = 0; i < 65; i++) {
        if (decode_gcr_to_4bytes(gcr_data + (i * 5), decoded + (i * 4)) < 0) {
            return -1;
        }
    }
    
    /* Check data marker (should be 0x07) */
    if (decoded[0] != 0x07) {
        return -1;
    }
    
    /* Copy sector data */
    memcpy(sector->data, decoded + 1, C64_SECTOR_SIZE);
    
    /* Verify checksum (XOR of all data bytes) */
    uint8_t calc_checksum = 0;
    for (int i = 1; i < 257; i++) {
        calc_checksum ^= decoded[i];
    }
    
    sector->crc_ok = (calc_checksum == decoded[257]);
    
    return 0;
}

/**
 * @brief Decode complete C64 sector from GCR track
 * 
 * @param gcr_track GCR track data
 * @param track_len Track length
 * @param start_pos Start position
 * @param sector_out Decoded sector (output)
 * @return Position after sector, or -1 on error
 */
int c64_decode_sector(
    const uint8_t *gcr_track,
    size_t track_len,
    size_t start_pos,
    c64_sector_t *sector_out
) {
    if (!gcr_track || !sector_out) {
        return -1;
    }
    
    /* Find header sync */
    int header_pos = find_sync_pattern(gcr_track, track_len, start_pos);
    if (header_pos < 0) {
        return -1;
    }
    
    /* Decode header */
    c64_sector_header_t header;
    if (decode_sector_header(gcr_track + header_pos, &header) < 0) {
        return -1;
    }
    
    sector_out->track = header.track;
    sector_out->sector = header.sector;
    sector_out->disk_id[0] = header.id1;
    sector_out->disk_id[1] = header.id2;
    
    /* Find data sync (after header + gap) */
    int data_pos = find_sync_pattern(gcr_track, track_len, 
                                     header_pos + C64_HEADER_LENGTH + C64_HEADER_GAP);
    
    if (data_pos < 0) {
        sector_out->valid = false;
        return -1;
    }
    
    /* Decode data */
    if (decode_sector_data(gcr_track + data_pos, sector_out) < 0) {
        sector_out->valid = false;
        return -1;
    }
    
    sector_out->valid = true;
    
    return data_pos + C64_DATA_LENGTH;
}

/**
 * @brief Get number of sectors for track
 * 
 * @param track Track number (1-42)
 * @return Number of sectors
 */
uint8_t c64_get_sectors_per_track(uint8_t track)
{
    if (track < 1 || track > C64_MAX_TRACKS_1541) {
        return 0;
    }
    
    return sectors_per_track[track];
}

/**
 * @brief Get track capacity in bytes
 * 
 * @param track Track number (1-42)
 * @return Capacity in bytes
 */
size_t c64_get_track_capacity(uint8_t track)
{
    if (track < 1 || track > C64_MAX_TRACKS_1541) {
        return 0;
    }
    
    return track_capacity[track];
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
