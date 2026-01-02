// SPDX-License-Identifier: MIT
/*
 * woz2_converters.c - WOZ2 Format Converters
 * 
 * Converters for:
 * - DSK → WOZ2
 * - WOZ1 → WOZ2
 * 
 * @version 2.8.4
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include "woz2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================*
 * DSK TO WOZ2 CONVERTER
 *============================================================================*/

/* Apple II GCR encoding tables */
static const uint8_t gcr_6and2_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* DOS 3.3 sector interleave */
static const uint8_t dos33_interleave[16] = {
    0x0, 0xD, 0xB, 0x9, 0x7, 0x5, 0x3, 0x1,
    0xE, 0xC, 0xA, 0x8, 0x6, 0x4, 0x2, 0xF
};

/**
 * @brief Encode 256 data bytes to 6-and-2 GCR format
 * 
 * @param src Source data (256 bytes)
 * @param dst Destination buffer (342 bytes)
 */
static void encode_6and2(const uint8_t *src, uint8_t *dst)
{
    uint8_t buffer[342];
    
    /* Convert 256 bytes to 342 6-bit values */
    for (int i = 0; i < 86; i++) {
        int idx = i * 3;
        if (idx + 2 < 256) {
            uint32_t val = (src[idx] << 16) | (src[idx+1] << 8) | src[idx+2];
            buffer[i] = (val >> 18) & 0x3F;
            buffer[i + 86] = (val >> 12) & 0x3F;
            buffer[i + 172] = (val >> 6) & 0x3F;
            buffer[i + 258] = val & 0x3F;
        }
    }
    
    /* Apply GCR encoding */
    for (int i = 0; i < 342; i++) {
        dst[i] = gcr_6and2_encode[buffer[i] & 0x3F];
    }
}

/**
 * @brief Calculate checksum for GCR sector
 */
static uint8_t gcr_checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/**
 * @brief Create GCR sector from DSK sector data
 * 
 * @param track Track number (0-34)
 * @param sector Sector number (0-15)
 * @param volume Volume number (default 254)
 * @param data Sector data (256 bytes)
 * @param out Output GCR buffer
 * @return Size of GCR sector in bytes
 */
static size_t create_gcr_sector(uint8_t track, uint8_t sector, uint8_t volume,
                                const uint8_t *data, uint8_t *out)
{
    size_t pos = 0;
    
    /* Address prologue: D5 AA 96 */
    out[pos++] = 0xD5;
    out[pos++] = 0xAA;
    out[pos++] = 0x96;
    
    /* Volume, Track, Sector, Checksum (4-and-4 encoding) */
    uint8_t vol_odd = (volume >> 1) | 0xAA;
    uint8_t vol_even = volume | 0xAA;
    uint8_t trk_odd = (track >> 1) | 0xAA;
    uint8_t trk_even = track | 0xAA;
    uint8_t sec_odd = (sector >> 1) | 0xAA;
    uint8_t sec_even = sector | 0xAA;
    uint8_t chk = vol_odd ^ trk_odd ^ sec_odd;
    
    out[pos++] = vol_odd;
    out[pos++] = vol_even;
    out[pos++] = trk_odd;
    out[pos++] = trk_even;
    out[pos++] = sec_odd;
    out[pos++] = sec_even;
    out[pos++] = chk;
    out[pos++] = (chk >> 1) | 0xAA;
    
    /* Address epilogue: DE AA EB */
    out[pos++] = 0xDE;
    out[pos++] = 0xAA;
    out[pos++] = 0xEB;
    
    /* Gap (5-10 sync bytes) */
    for (int i = 0; i < 6; i++) {
        out[pos++] = 0xFF;
    }
    
    /* Data prologue: D5 AA AD */
    out[pos++] = 0xD5;
    out[pos++] = 0xAA;
    out[pos++] = 0xAD;
    
    /* Encode data (342 bytes) */
    uint8_t encoded[342];
    encode_6and2(data, encoded);
    memcpy(out + pos, encoded, 342);
    pos += 342;
    
    /* Data checksum */
    out[pos++] = gcr_checksum(encoded, 342);
    
    /* Data epilogue: DE AA EB */
    out[pos++] = 0xDE;
    out[pos++] = 0xAA;
    out[pos++] = 0xEB;
    
    /* Trailing gap */
    for (int i = 0; i < 16; i++) {
        out[pos++] = 0xFF;
    }
    
    return pos;
}

bool woz2_from_dsk(const char *dsk_filename, const char *woz2_filename,
                   uint8_t disk_type)
{
    if (!dsk_filename || !woz2_filename) return false;
    
    /* Open DSK file */
    FILE *f = fopen(dsk_filename, "rb");
    if (!f) return false;
    
    /* Check DSK size (35 tracks × 16 sectors × 256 bytes = 143,360) */
    fseek(f, 0, SEEK_END);
    long dsk_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (dsk_size != 143360) {
        fclose(f);
        return false;  /* Not a standard DOS 3.3 DSK */
    }
    
    /* Read entire DSK */
    uint8_t *dsk_data = malloc(dsk_size);
    if (!dsk_data) {
        fclose(f);
        return false;
    }
    
    if (fread(dsk_data, 1, dsk_size, f) != (size_t)dsk_size) {
        free(dsk_data);
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    /* Initialize WOZ2 image */
    woz2_image_t image;
    if (!woz2_init(&image, disk_type)) {
        free(dsk_data);
        return false;
    }
    
    /* Set creator */
    strncpy(image.info.creator, "UFT v2.8.4 DSK→WOZ2", 31);
    
    /* Convert each track */
    for (uint8_t track = 0; track < 35; track++) {
        uint8_t track_buffer[8192];  /* Large enough for full track */
        size_t track_pos = 0;
        
        /* Add self-sync header (48+ sync bytes) */
        for (int i = 0; i < 64; i++) {
            track_buffer[track_pos++] = 0xFF;
        }
        
        /* Add 16 sectors */
        for (int sec = 0; sec < 16; sec++) {
            /* Get physical sector based on interleave */
            uint8_t physical_sector = dos33_interleave[sec];
            
            /* Calculate DSK offset */
            size_t dsk_offset = (track * 16 + physical_sector) * 256;
            
            /* Create GCR sector */
            track_pos += create_gcr_sector(track, sec, 254,
                                          dsk_data + dsk_offset,
                                          track_buffer + track_pos);
        }
        
        /* Add track to WOZ2 */
        uint32_t bit_count = track_pos * 8;
        if (!woz2_add_track(&image, track, 0, track_buffer, bit_count)) {
            woz2_free(&image);
            free(dsk_data);
            return false;
        }
    }
    
    free(dsk_data);
    
    /* Write WOZ2 file */
    bool result = woz2_write(woz2_filename, &image);
    
    woz2_free(&image);
    
    return result;
}

/*============================================================================*
 * WOZ1 TO WOZ2 CONVERTER
 *============================================================================*/

bool woz2_from_woz1(const char *woz1_filename, const char *woz2_filename)
{
    if (!woz1_filename || !woz2_filename) return false;
    
    /* Open WOZ1 file */
    FILE *f = fopen(woz1_filename, "rb");
    if (!f) return false;
    
    /* Read header */
    uint8_t header[12];
    if (fread(header, 1, 12, f) != 12) {
        fclose(f);
        return false;
    }
    
    /* Verify WOZ1 magic */
    if (memcmp(header, "WOZ1", 4) != 0) {
        fclose(f);
        return false;
    }
    
    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *woz1_data = malloc(file_size);
    if (!woz1_data) {
        fclose(f);
        return false;
    }
    
    if (fread(woz1_data, 1, file_size, f) != (size_t)file_size) {
        free(woz1_data);
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    /* Initialize WOZ2 image */
    woz2_image_t image;
    memset(&image, 0, sizeof(image));
    
    if (!woz2_init(&image, WOZ2_DISK_TYPE_5_25)) {
        free(woz1_data);
        return false;
    }
    
    /* Set creator */
    strncpy(image.info.creator, "UFT v2.8.4 WOZ1→WOZ2", 31);
    
    /* Parse WOZ1 chunks and convert to WOZ2 */
    size_t offset = 12;
    
    while (offset + 8 <= (size_t)file_size) {
        uint32_t chunk_id = *(uint32_t*)(woz1_data + offset);
        uint32_t chunk_size = *(uint32_t*)(woz1_data + offset + 4);
        offset += 8;
        
        if (chunk_id == 0x4F464E49) {  /* INFO */
            /* WOZ1 INFO is smaller - copy and upgrade */
            if (chunk_size >= 36) {
                memcpy(&image.info, woz1_data + offset, 36);
                image.info.version = 2;  /* Upgrade to version 2 */
            }
        }
        else if (chunk_id == 0x50414D54) {  /* TMAP */
            memcpy(&image.tmap, woz1_data + offset, WOZ2_TRACK_MAP_SIZE);
        }
        else if (chunk_id == 0x534B5254) {  /* TRKS */
            /* Convert WOZ1 TRKS to WOZ2 TRKS */
            /* WOZ1 has 6656 bytes per track, WOZ2 uses variable blocks */
            /* This is a simplified conversion - real implementation
               would parse the actual track data */
            image.track_data_size = chunk_size;
            image.track_data = malloc(chunk_size);
            if (image.track_data) {
                memcpy(image.track_data, woz1_data + offset, chunk_size);
            }
            image.num_tracks = chunk_size / 6656;
        }
        
        offset += chunk_size;
    }
    
    free(woz1_data);
    
    /* Write WOZ2 file */
    bool result = woz2_write(woz2_filename, &image);
    
    woz2_free(&image);
    
    return result;
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
