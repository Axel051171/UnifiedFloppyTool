/**
 * @file g71.c
 * @brief G71 (GCR Track Image for 1571) Format Implementation
 * @version 4.1.0
 * 
 * G71 is the double-sided version of G64 for the 1571 drive.
 * It stores raw GCR-encoded track data for both sides of a disk.
 * 
 * File structure:
 * - Header (same format as G64)
 * - Track data for side 0 (tracks 1-35)
 * - Track data for side 1 (tracks 36-70)
 * - Speed zones for each track
 * 
 * Reference: VICE emulator, nibtools, DirMaster
 */

#include "uft/formats/uft_g71.h"
#include "uft/core/uft_unified_types.h"
#include "uft/core/uft_error_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * G71 Format Constants
 * ============================================================================ */

#define G71_SIGNATURE       "GCR-1571"
#define G71_SIGNATURE_LEN   8
#define G71_VERSION         0x00
#define G71_TRACK_COUNT     70          /* 35 tracks x 2 sides */
#define G71_MAX_TRACK_SIZE  7928        /* Maximum GCR track size */
#define G71_HEADER_SIZE     12

/* G64/G71 Header offsets */
#define G71_OFF_SIGNATURE   0
#define G71_OFF_VERSION     8
#define G71_OFF_TRACKS      9
#define G71_OFF_TRACK_SIZE  10

/* Track table starts at offset 12 */
#define G71_TRACK_TABLE_OFF 12
#define G71_SPEED_TABLE_OFF (G71_TRACK_TABLE_OFF + G71_TRACK_COUNT * 4)

/* Speed zones for 1571 (same as 1541) */
static const int g71_speed_zones[] = {
    /* Zone 3: tracks 1-17 */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    /* Zone 2: tracks 18-24 */
    2, 2, 2, 2, 2, 2, 2,
    /* Zone 1: tracks 25-30 */
    1, 1, 1, 1, 1, 1,
    /* Zone 0: tracks 31-35 */
    0, 0, 0, 0, 0
};

/* GCR track sizes per speed zone */
static const int g71_gcr_track_sizes[] = {
    6250,   /* Zone 0: 17 sectors */
    6666,   /* Zone 1: 18 sectors */
    7142,   /* Zone 2: 19 sectors */
    7692    /* Zone 3: 21 sectors */
};

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static int get_speed_zone(int track) {
    /* track is 0-based */
    int side_track = track % 35;
    if (side_track < 17) return 3;
    if (side_track < 24) return 2;
    if (side_track < 30) return 1;
    return 0;
}

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_g71_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < G71_HEADER_SIZE) {
        return false;
    }
    
    /* Check for G71 signature */
    if (memcmp(data, G71_SIGNATURE, G71_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    /* Also accept G64 signature with 70 tracks (some tools use this) */
    if (memcmp(data, "GCR-1541", 8) == 0) {
        if (data[9] >= 70) {  /* Track count at offset 9 */
            if (confidence) *confidence = 80;
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * Read Functions
 * ============================================================================ */

int uft_g71_read(const char *path, uft_disk_image_t **out) {
    if (!path || !out) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    /* Read header */
    uint8_t header[G71_HEADER_SIZE];
    if (fread(header, 1, G71_HEADER_SIZE, f) != G71_HEADER_SIZE) {
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    
    /* Verify signature */
    int conf;
    if (!uft_g71_probe(header, G71_HEADER_SIZE, &conf)) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    uint8_t track_count = header[9];
    uint16_t max_track_size = read_le16(&header[10]);
    
    if (track_count < 70) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* Read track offset table */
    uint32_t track_offsets[G71_TRACK_COUNT];
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        uint8_t buf[4];
        if (fseek(f, G71_TRACK_TABLE_OFF + i * 4, SEEK_SET) != 0) continue;
        if (fread(buf, 1, 4, f) != 4) {
            track_offsets[i] = 0;
        } else {
            track_offsets[i] = read_le32(buf);
        }
    }
    
    /* Read speed zone table */
    uint8_t speed_zones[G71_TRACK_COUNT];
    if (fseek(f, G71_SPEED_TABLE_OFF, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERR_IO;
    }
    if (fread(speed_zones, 1, G71_TRACK_COUNT, f) != G71_TRACK_COUNT) {
        memset(speed_zones, 0, G71_TRACK_COUNT);
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    disk->tracks = 35;
    disk->heads = 2;
    disk->track_count = G71_TRACK_COUNT;
    disk->encoding = UFT_ENCODING_GCR;
    
    disk->track_data = calloc(G71_TRACK_COUNT, sizeof(void*));
    if (!disk->track_data) {
        free(disk);
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    /* Read each track */
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        if (track_offsets[i] == 0) continue;
        
        if (fseek(f, track_offsets[i], SEEK_SET) != 0) continue;
        
        /* Read track size */
        uint8_t size_buf[2];
        if (fread(size_buf, 1, 2, f) != 2) continue;
        uint16_t track_size = read_le16(size_buf);
        
        if (track_size == 0 || track_size > G71_MAX_TRACK_SIZE) continue;
        
        /* Allocate track (GCR track is raw data, not sectors) */
        uft_track_t *track = calloc(1, sizeof(uft_track_t));
        if (!track) continue;
        
        track->track_num = i % 35;
        track->head = i / 35;
        track->encoding = UFT_ENCODING_GCR;
        
        /* Store raw GCR data */
        track->raw_data = malloc(track_size);
        track->raw_size = track_size;
        
        if (track->raw_data) {
            if (fread(track->raw_data, 1, track_size, f) != track_size) {
                free(track->raw_data);
                track->raw_data = NULL;
                track->raw_size = 0;
            }
        }
        
        disk->track_data[i] = track;
    }
    
    fclose(f);
    *out = disk;
    return UFT_OK;
}

/* ============================================================================
 * Write Functions
 * ============================================================================ */

int uft_g71_write(const char *path, const uft_disk_image_t *disk) {
    if (!path || !disk) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_FILE_CREATE;
    
    /* Write header */
    uint8_t header[G71_HEADER_SIZE];
    memcpy(header, G71_SIGNATURE, G71_SIGNATURE_LEN);
    header[8] = G71_VERSION;
    header[9] = G71_TRACK_COUNT;
    write_le16(&header[10], G71_MAX_TRACK_SIZE);
    fwrite(header, 1, G71_HEADER_SIZE, f);
    
    /* Calculate track offsets */
    uint32_t track_offsets[G71_TRACK_COUNT];
    uint32_t current_offset = G71_HEADER_SIZE + 
                              G71_TRACK_COUNT * 4 +  /* track offset table */
                              G71_TRACK_COUNT * 4;   /* speed zone table */
    
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        uft_track_t *track = NULL;
        if (i < (int)disk->track_count) {
            track = disk->track_data[i];
        }
        
        if (track && track->raw_data && track->raw_size > 0) {
            track_offsets[i] = current_offset;
            current_offset += 2 + track->raw_size;  /* size word + data */
        } else {
            track_offsets[i] = 0;
        }
    }
    
    /* Write track offset table */
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        uint8_t buf[4];
        write_le32(buf, track_offsets[i]);
        fwrite(buf, 1, 4, f);
    }
    
    /* Write speed zone table */
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        uint8_t speed = get_speed_zone(i);
        fwrite(&speed, 1, 1, f);
    }
    
    /* Pad to 4-byte alignment */
    uint8_t pad[3] = {0, 0, 0};
    int pad_needed = (G71_TRACK_COUNT % 4) ? (4 - (G71_TRACK_COUNT % 4)) : 0;
    if (pad_needed) fwrite(pad, 1, pad_needed, f);
    
    /* Write track data */
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        if (track_offsets[i] == 0) continue;
        
        uft_track_t *track = disk->track_data[i];
        if (!track || !track->raw_data) continue;
        
        uint8_t size_buf[2];
        write_le16(size_buf, track->raw_size);
        fwrite(size_buf, 1, 2, f);
        fwrite(track->raw_data, 1, track->raw_size, f);
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return UFT_OK;
}

/* ============================================================================
 * Create Blank
 * ============================================================================ */

int uft_g71_create_blank(uft_disk_image_t **out) {
    if (!out) return UFT_ERR_INVALID_ARG;
    
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) return UFT_ERR_MEMORY;
    
    disk->tracks = 35;
    disk->heads = 2;
    disk->track_count = G71_TRACK_COUNT;
    disk->encoding = UFT_ENCODING_GCR;
    
    disk->track_data = calloc(G71_TRACK_COUNT, sizeof(void*));
    if (!disk->track_data) {
        free(disk);
        return UFT_ERR_MEMORY;
    }
    
    /* Create empty tracks with appropriate sizes */
    for (int i = 0; i < G71_TRACK_COUNT; i++) {
        uft_track_t *track = calloc(1, sizeof(uft_track_t));
        if (!track) continue;
        
        track->track_num = i % 35;
        track->head = i / 35;
        track->encoding = UFT_ENCODING_GCR;
        
        int zone = get_speed_zone(i);
        int track_size = g71_gcr_track_sizes[zone];
        
        track->raw_data = calloc(1, track_size);
        track->raw_size = track_size;
        
        /* Fill with sync pattern */
        if (track->raw_data) {
            memset(track->raw_data, 0xFF, track_size);
        }
        
        disk->track_data[i] = track;
    }
    
    *out = disk;
    return UFT_OK;
}

/* ============================================================================
 * Info Function
 * ============================================================================ */

int uft_g71_get_info(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    uint8_t header[G71_HEADER_SIZE];
    if (fread(header, 1, G71_HEADER_SIZE, f) != G71_HEADER_SIZE) {
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fclose(f);
    
    uint8_t track_count = header[9];
    uint16_t max_track_size = read_le16(&header[10]);
    
    snprintf(buf, buf_size,
        "Format: G71 (GCR-1571)\n"
        "Tracks: %d (35 per side)\n"
        "Sides: 2\n"
        "Max Track Size: %d bytes\n"
        "File Size: %zu bytes\n"
        "Encoding: GCR\n"
        "Drive: Commodore 1571\n",
        track_count, max_track_size, file_size);
    
    return UFT_OK;
}
