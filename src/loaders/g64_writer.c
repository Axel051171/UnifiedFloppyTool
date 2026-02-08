/**
 * @file g64_writer.c
 * @brief G64 (GCR-Level) Image Writer for Commodore 64
 * 
 * Converts D64 sector data to G64 GCR-encoded track format.
 * G64 preserves the raw GCR data as read from disk.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* G64 Header */
#define G64_SIGNATURE "GCR-1541"
#define G64_VERSION   0
#define G64_MAX_TRACKS 84
#define G64_TRACK_OFFSET_START 0x0C

/* C64 Disk Geometry */
static const uint8_t SECTORS_PER_TRACK[] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,  /* 1-17 */
    19,19,19,19,19,19,19,                                  /* 18-24 */
    18,18,18,18,18,18,                                     /* 25-30 */
    17,17,17,17,17                                         /* 31-35 */
};

static const uint16_t SPEED_ZONE[] = {
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,  /* Zone 3: tracks 1-17 */
    2,2,2,2,2,2,2,                        /* Zone 2: tracks 18-24 */
    1,1,1,1,1,1,                          /* Zone 1: tracks 25-30 */
    0,0,0,0,0                             /* Zone 0: tracks 31-35 */
};

/* GCR encoding table (4 bits -> 5 bits) */
static const uint8_t GCR_TABLE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* Track sizes per speed zone (bytes) */
static const uint16_t TRACK_SIZE[4] = { 6250, 6666, 7142, 7692 };

typedef struct {
    uint8_t signature[8];
    uint8_t version;
    uint8_t num_tracks;
    uint16_t max_track_size;
} g64_header_t;

/* ============================================================================
 * GCR Encoding
 * ============================================================================ */

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 */
static void gcr_encode_4to5(const uint8_t *in, uint8_t *out)
{
    uint8_t g[8];
    
    /* Convert each nibble to 5-bit GCR */
    g[0] = GCR_TABLE[in[0] >> 4];
    g[1] = GCR_TABLE[in[0] & 0x0F];
    g[2] = GCR_TABLE[in[1] >> 4];
    g[3] = GCR_TABLE[in[1] & 0x0F];
    g[4] = GCR_TABLE[in[2] >> 4];
    g[5] = GCR_TABLE[in[2] & 0x0F];
    g[6] = GCR_TABLE[in[3] >> 4];
    g[7] = GCR_TABLE[in[3] & 0x0F];
    
    /* Pack 8x5 bits into 5 bytes */
    out[0] = (g[0] << 3) | (g[1] >> 2);
    out[1] = (g[1] << 6) | (g[2] << 1) | (g[3] >> 4);
    out[2] = (g[3] << 4) | (g[4] >> 1);
    out[3] = (g[4] << 7) | (g[5] << 2) | (g[6] >> 3);
    out[4] = (g[6] << 5) | g[7];
}

/**
 * @brief Encode a sector to GCR
 */
static size_t gcr_encode_sector(uint8_t track, uint8_t sector,
                                const uint8_t *data, uint8_t *gcr_out)
{
    uint8_t raw[4], gcr[5];
    size_t pos = 0;
    
    /* Sync: 5 bytes of 0xFF */
    memset(gcr_out + pos, 0xFF, 5);
    pos += 5;
    
    /* Header block */
    uint8_t checksum = sector ^ track ^ 0 ^ 0;  /* sector, track, id2, id1 */
    raw[0] = 0x08;      /* Header block ID */
    raw[1] = checksum;
    raw[2] = sector;
    raw[3] = track;
    gcr_encode_4to5(raw, gcr_out + pos);
    pos += 5;
    
    raw[0] = 0;         /* ID2 (disk ID) */
    raw[1] = 0;         /* ID1 */
    raw[2] = 0x0F;      /* Gap byte */
    raw[3] = 0x0F;
    gcr_encode_4to5(raw, gcr_out + pos);
    pos += 5;
    
    /* Header gap: 9 bytes of 0x55 */
    memset(gcr_out + pos, 0x55, 9);
    pos += 9;
    
    /* Data sync: 5 bytes of 0xFF */
    memset(gcr_out + pos, 0xFF, 5);
    pos += 5;
    
    /* Data block */
    raw[0] = 0x07;      /* Data block ID */
    checksum = 0;
    for (int i = 0; i < 256; i++) {
        checksum ^= data[i];
    }
    
    /* Encode data block ID + first 3 data bytes */
    raw[1] = data[0];
    raw[2] = data[1];
    raw[3] = data[2];
    gcr_encode_4to5(raw, gcr_out + pos);
    pos += 5;
    
    /* Encode remaining 253 bytes (63 groups of 4, plus 1 extra) */
    for (int i = 3; i < 255; i += 4) {
        raw[0] = data[i];
        raw[1] = data[i + 1];
        raw[2] = (i + 2 < 256) ? data[i + 2] : 0;
        raw[3] = (i + 3 < 256) ? data[i + 3] : 0;
        gcr_encode_4to5(raw, gcr_out + pos);
        pos += 5;
    }
    
    /* Checksum + padding */
    raw[0] = data[255];
    raw[1] = checksum;
    raw[2] = 0x00;
    raw[3] = 0x00;
    gcr_encode_4to5(raw, gcr_out + pos);
    pos += 5;
    
    /* Inter-sector gap */
    size_t gap = 8;
    memset(gcr_out + pos, 0x55, gap);
    pos += gap;
    
    return pos;
}

/* ============================================================================
 * G64 Writer
 * ============================================================================ */

/**
 * @brief Write D64 data to G64 file
 */
int g64_write(const char *filename, const uint8_t *d64_data, size_t d64_size)
{
    if (!filename || !d64_data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* Determine number of tracks */
    int num_tracks = 35;
    if (d64_size >= 349696) num_tracks = 40;  /* 40 track D64 */
    
    /* Write header */
    g64_header_t header;
    memcpy(header.signature, G64_SIGNATURE, 8);
    header.version = G64_VERSION;
    header.num_tracks = num_tracks * 2;  /* Half-tracks */
    header.max_track_size = 7928;
    
    if (fwrite(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
    /* Reserve space for track offset table */
    uint32_t *track_offsets = calloc(G64_MAX_TRACKS, sizeof(uint32_t));
    uint32_t *speed_offsets = calloc(G64_MAX_TRACKS, sizeof(uint32_t));
    
    long offset_table_pos = ftell(fp);
    if (fwrite(track_offsets, sizeof(uint32_t), G64_MAX_TRACKS, fp) != G64_MAX_TRACKS) { /* I/O error */ }
    if (fwrite(speed_offsets, sizeof(uint32_t), G64_MAX_TRACKS, fp) != G64_MAX_TRACKS) { /* I/O error */ }
    /* Encode each track */
    uint8_t *track_buf = malloc(8192);
    size_t d64_offset = 0;
    
    for (int t = 0; t < num_tracks; t++) {
        int track_num = t + 1;
        int sectors = (track_num <= 35) ? SECTORS_PER_TRACK[t] : 17;
        int zone = (track_num <= 35) ? SPEED_ZONE[t] : 0;
        uint16_t track_size = TRACK_SIZE[zone];
        
        /* Record track offset (only full tracks, not half-tracks) */
        track_offsets[t * 2] = ftell(fp);
        speed_offsets[t * 2] = zone;
        
        /* Write track size */
        if (fwrite(&track_size, sizeof(uint16_t), 1, fp) != 1) { /* I/O error */ }
        /* Encode all sectors */
        size_t gcr_pos = 0;
        memset(track_buf, 0x55, track_size);  /* Fill with gap bytes */
        
        for (int s = 0; s < sectors; s++) {
            size_t sector_len = gcr_encode_sector(track_num, s,
                                                   d64_data + d64_offset,
                                                   track_buf + gcr_pos);
            gcr_pos += sector_len;
            d64_offset += 256;
            
            if (gcr_pos >= track_size - 400) break;  /* Safety margin */
        }
        
        if (fwrite(track_buf, 1, track_size, fp) != track_size) { /* I/O error */ }
    }
    
    /* Update offset tables */
    if (fseek(fp, offset_table_pos, SEEK_SET) != 0) { /* seek error */ }
    if (fwrite(track_offsets, sizeof(uint32_t), G64_MAX_TRACKS, fp) != G64_MAX_TRACKS) { /* I/O error */ }
    if (fwrite(speed_offsets, sizeof(uint32_t), G64_MAX_TRACKS, fp) != G64_MAX_TRACKS) { /* I/O error */ }
    free(track_buf);
    free(track_offsets);
    free(speed_offsets);
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
        fclose(fp);
        return 0;
}

/**
 * @brief Convert D64 file to G64
 */
int g64_convert_from_d64(const char *d64_file, const char *g64_file)
{
    FILE *fp = fopen(d64_file, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(data, 1, size, fp) != size) { /* I/O error */ }
    fclose(fp);
    
    int result = g64_write(g64_file, data, size);
    free(data);
    
    return result;
}
