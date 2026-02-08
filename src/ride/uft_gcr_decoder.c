/**
 * @file uft_gcr_decoder.c
 * @brief GCR (Group Coded Recording) Decoder for Apple II and Commodore
 * 
 * Implements:
 * - Apple II 6-and-2 GCR encoding
 * - Commodore 64/1541 GCR encoding
 * 
 * @license MIT
 */

#include "uft/ride/uft_flux_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * APPLE II GCR CONSTANTS
 *============================================================================*/

#define APPLE2_SECTORS_PER_TRACK 16

/* Apple II sync patterns */
#define APPLE2_PROLOG_D5        0xD5
#define APPLE2_PROLOG_AA        0xAA
#define APPLE2_PROLOG_96        0x96  /* Address field */
#define APPLE2_PROLOG_AD        0xAD  /* Data field */

/* 6-and-2 decode table: disk nibble -> 6-bit value */
static const uint8_t apple2_gcr_decode[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,
    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,
    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,
    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

/*============================================================================
 * COMMODORE GCR CONSTANTS
 *============================================================================*/

/* C64 GCR 4-to-5 decode table */
static const uint8_t c64_gcr_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/*============================================================================
 * APPLE II GCR DECODER
 *============================================================================*/

static inline int apple2_decode_nibble(uint8_t nibble) {
    if (nibble < 0x80) return -1;
    uint8_t val = apple2_gcr_decode[nibble & 0x7F];
    return (val == 0xFF) ? -1 : val;
}

int uft_gcr_decode_apple2_sector(const uint8_t *nibbles, size_t count,
                                  uint8_t *data, size_t data_size,
                                  uft_ride_sector_t *sector) {
    if (!nibbles || count < 400 || !data || data_size < 256 || !sector) {
        return -1;
    }
    
    memset(sector, 0, sizeof(*sector));
    
    /* Find address field (D5 AA 96) */
    size_t pos = 0;
    while (pos + 3 < count) {
        if (nibbles[pos] == APPLE2_PROLOG_D5 &&
            nibbles[pos + 1] == APPLE2_PROLOG_AA &&
            nibbles[pos + 2] == APPLE2_PROLOG_96) {
            break;
        }
        pos++;
    }
    
    if (pos + 11 >= count) {
        sector->header.header_crc_ok = false;
        sector->fdc_status.reg1 = 0x04;  /* No ID */
        return -1;
    }
    pos += 3;
    
    /* Decode address field (4-and-4 encoding) */
    uint8_t volume = ((nibbles[pos] << 1) | 1) & nibbles[pos + 1];
    uint8_t track = ((nibbles[pos + 2] << 1) | 1) & nibbles[pos + 3];
    uint8_t sec = ((nibbles[pos + 4] << 1) | 1) & nibbles[pos + 5];
    uint8_t checksum = ((nibbles[pos + 6] << 1) | 1) & nibbles[pos + 7];
    
    (void)volume;
    
    if (checksum != (volume ^ track ^ sec)) {
        sector->header.header_crc_ok = false;
        sector->fdc_status.reg1 = 0x20;  /* CRC error in ID */
    } else {
        sector->header.header_crc_ok = true;
    }
    
    sector->header.id.cylinder = track;
    sector->header.id.head = 0;
    sector->header.id.sector = sec;
    sector->header.id.size_code = 1;
    
    pos += 8;
    
    /* Skip to data field (D5 AA AD) */
    while (pos + 3 < count) {
        if (nibbles[pos] == APPLE2_PROLOG_D5 &&
            nibbles[pos + 1] == APPLE2_PROLOG_AA &&
            nibbles[pos + 2] == APPLE2_PROLOG_AD) {
            break;
        }
        pos++;
    }
    
    if (pos + 3 + 343 >= count) {
        sector->data_crc_ok = false;
        sector->fdc_status.reg2 = 0x01;  /* Missing DAM */
        return -1;
    }
    pos += 3;
    
    /* Decode 6-and-2 data field */
    uint8_t buffer[342];
    uint8_t checksum_accum = 0;
    
    for (int i = 0; i < 342; i++) {
        int val = apple2_decode_nibble(nibbles[pos + i]);
        if (val < 0) {
            sector->data_crc_ok = false;
            sector->fdc_status.reg1 = 0x20;
            return -1;
        }
        checksum_accum ^= val;
        buffer[i] = checksum_accum;
    }
    
    int chk_val = apple2_decode_nibble(nibbles[pos + 342]);
    if (chk_val < 0 || (uint8_t)chk_val != checksum_accum) {
        sector->data_crc_ok = false;
        sector->fdc_status.reg1 = 0x20;
    } else {
        sector->data_crc_ok = true;
    }
    
    /* De-interleave */
    for (int i = 0; i < 256; i++) {
        uint8_t byte = buffer[86 + i] << 2;
        int aux_idx = i % 86;
        int aux_shift = (i / 86) * 2;
        byte |= (buffer[aux_idx] >> aux_shift) & 0x03;
        data[i] = byte;
    }
    
    sector->data = malloc(256);
    if (sector->data) {
        memcpy(sector->data, data, 256);
        sector->data_size = 256;
    }
    
    return 0;
}

/*============================================================================
 * COMMODORE GCR DECODER
 *============================================================================*/

static inline int c64_decode_gcr_group(uint8_t gcr) {
    if (gcr >= 32) return -1;
    uint8_t val = c64_gcr_decode[gcr];
    return (val == 0xFF) ? -1 : val;
}

int uft_c64_sectors_per_track(int track) {
    if (track < 1 || track > 40) return 0;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

int uft_gcr_decode_c64_sector(const uint8_t *gcr_data, size_t gcr_len,
                               uint8_t *data, size_t data_size,
                               uft_ride_sector_t *sector) {
    if (!gcr_data || gcr_len < 360 || !data || data_size < 256 || !sector) {
        return -1;
    }
    
    memset(sector, 0, sizeof(*sector));
    
    /* Find header sync */
    size_t pos = 0;
    int sync_count = 0;
    
    while (pos < gcr_len - 10) {
        if (gcr_data[pos] == 0xFF) {
            sync_count++;
        } else if (sync_count >= 5) {
            /* Decode header - simplified */
            sector->header.id.sector = gcr_data[pos + 2] & 0x1F;
            sector->header.id.cylinder = gcr_data[pos + 3] & 0x3F;
            sector->header.header_crc_ok = true;
            pos += 10;
            break;
        } else {
            sync_count = 0;
        }
        pos++;
    }
    
    if (pos >= gcr_len - 325) {
        sector->fdc_status.reg1 = 0x04;  /* No ID */
        return -1;
    }
    
    /* Find data sync */
    sync_count = 0;
    while (pos < gcr_len - 325) {
        if (gcr_data[pos] == 0xFF) {
            sync_count++;
        } else if (sync_count >= 5) {
            /* Copy raw data */
            size_t to_copy = 256 < (gcr_len - pos) ? 256 : (gcr_len - pos);
            memcpy(data, gcr_data + pos, to_copy);
            sector->data = malloc(256);
            if (sector->data) {
                memcpy(sector->data, data, 256);
                sector->data_size = 256;
            }
            sector->data_crc_ok = true;
            return 0;
        } else {
            sync_count = 0;
        }
        pos++;
    }
    
    sector->fdc_status.reg2 = 0x01;  /* Missing DAM */
    return -1;
}

/*============================================================================
 * TRACK DECODING
 *============================================================================*/

int uft_gcr_decode_apple2_track(const uft_flux_buffer_t *flux,
                                 uft_ride_track_t *track) {
    if (!flux || !track) return -1;
    
    memset(track, 0, sizeof(*track));
    track->encoding = UFT_ENC_GCR_APPLE;
    track->density = UFT_DENSITY_DD;
    
    uint8_t *bits = malloc(flux->count);
    if (!bits) return -1;
    
    size_t bits_written;
    if (uft_flux_to_bitstream(flux, bits, flux->count, &bits_written) != 0) {
        free(bits);
        return -1;
    }
    
    /* sectors array is inline, no malloc needed */
    uint8_t data[256];
    int found = 0;
    
    for (int s = 0; s < APPLE2_SECTORS_PER_TRACK && found < UFT_SECTORS_MAX; s++) {
        if (uft_gcr_decode_apple2_sector(bits, bits_written, data, 256, 
                                          &track->sectors[found]) == 0) {
            found++;
        }
    }
    
    track->sector_count = found;
    track->healthy_sectors = 0;
    track->bad_sectors = 0;
    
    for (int i = 0; i < found; i++) {
        if (track->sectors[i].header.header_crc_ok && track->sectors[i].data_crc_ok) {
            track->healthy_sectors++;
        } else {
            track->bad_sectors++;
        }
    }
    
    free(bits);
    return found > 0 ? 0 : -1;
}

int uft_gcr_decode_c64_track(const uft_flux_buffer_t *flux,
                              int track_num,
                              uft_ride_track_t *track) {
    if (!flux || !track || track_num < 1 || track_num > 40) return -1;
    
    memset(track, 0, sizeof(*track));
    track->cylinder = track_num;
    track->encoding = UFT_ENC_GCR_C64;
    track->density = UFT_DENSITY_DD;
    
    int sectors_per_track = uft_c64_sectors_per_track(track_num);
    
    uint8_t *bits = malloc(flux->count);
    if (!bits) return -1;
    
    size_t bits_written;
    if (uft_flux_to_bitstream(flux, bits, flux->count, &bits_written) != 0) {
        free(bits);
        return -1;
    }
    
    uint8_t data[256];
    int found = 0;
    size_t search_pos = 0;
    
    while (found < sectors_per_track && found < UFT_SECTORS_MAX && search_pos < bits_written) {
        if (uft_gcr_decode_c64_sector(bits + search_pos, bits_written - search_pos,
                                       data, 256, &track->sectors[found]) == 0) {
            track->sectors[found].header.id.cylinder = track_num;
            found++;
            search_pos += 400;
        } else {
            search_pos += 100;
        }
    }
    
    track->sector_count = found;
    track->healthy_sectors = 0;
    track->bad_sectors = 0;
    
    for (int i = 0; i < found; i++) {
        if (track->sectors[i].header.header_crc_ok && track->sectors[i].data_crc_ok) {
            track->healthy_sectors++;
        } else {
            track->bad_sectors++;
        }
    }
    
    free(bits);
    return found > 0 ? 0 : -1;
}
