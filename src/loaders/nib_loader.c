/**
 * @file nib_loader.c
 * @brief NIB (Nibbler) Image Loader/Writer for Commodore 64
 * 
 * NIB format stores raw GCR-encoded track data as read by nibbler tools.
 * Each track is 8192 bytes (half-tracks supported).
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* NIB Constants */
#define NIB_TRACK_SIZE      8192
#define NIB_TRACKS          35
#define NIB_HALF_TRACKS     70
#define NIB_FILE_SIZE       (NIB_TRACKS * NIB_TRACK_SIZE)
#define NIB_HALF_FILE_SIZE  (NIB_HALF_TRACKS * NIB_TRACK_SIZE)

/* Sector counts per track */
static const uint8_t SECTORS_PER_TRACK[] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

/* GCR decode table (5 bits -> 4 bits, 0xFF = invalid) */
static const uint8_t GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* GCR encode table (4 bits -> 5 bits) */
static const uint8_t GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

typedef struct {
    uint8_t *tracks[NIB_HALF_TRACKS];
    uint16_t track_size[NIB_HALF_TRACKS];
    bool     half_tracks;
    int      num_tracks;
} nib_image_t;

typedef struct {
    uint8_t  track;
    uint8_t  sector;
    uint8_t  data[256];
    uint16_t header_crc;
    uint16_t data_crc;
    bool     valid;
} nib_sector_t;

/* ============================================================================
 * GCR Decoding
 * ============================================================================ */

/**
 * @brief Decode 5 GCR bytes to 4 data bytes
 */
static int gcr_decode_5to4(const uint8_t *gcr, uint8_t *out)
{
    uint8_t g[8];
    
    /* Extract 8 x 5-bit values from 5 bytes */
    g[0] = (gcr[0] >> 3) & 0x1F;
    g[1] = ((gcr[0] << 2) | (gcr[1] >> 6)) & 0x1F;
    g[2] = (gcr[1] >> 1) & 0x1F;
    g[3] = ((gcr[1] << 4) | (gcr[2] >> 4)) & 0x1F;
    g[4] = ((gcr[2] << 1) | (gcr[3] >> 7)) & 0x1F;
    g[5] = (gcr[3] >> 2) & 0x1F;
    g[6] = ((gcr[3] << 3) | (gcr[4] >> 5)) & 0x1F;
    g[7] = gcr[4] & 0x1F;
    
    /* Decode each 5-bit value to 4-bit nibble */
    for (int i = 0; i < 8; i++) {
        if (GCR_DECODE[g[i]] == 0xFF) return -1;
    }
    
    out[0] = (GCR_DECODE[g[0]] << 4) | GCR_DECODE[g[1]];
    out[1] = (GCR_DECODE[g[2]] << 4) | GCR_DECODE[g[3]];
    out[2] = (GCR_DECODE[g[4]] << 4) | GCR_DECODE[g[5]];
    out[3] = (GCR_DECODE[g[6]] << 4) | GCR_DECODE[g[7]];
    
    return 0;
}

/**
 * @brief Find sync mark in track data
 */
static int find_sync(const uint8_t *track, size_t size, size_t start)
{
    int sync_count = 0;
    
    for (size_t i = start; i < size; i++) {
        if (track[i] == 0xFF) {
            sync_count++;
            if (sync_count >= 5) {
                /* Find end of sync */
                while (i < size && track[i] == 0xFF) i++;
                return (i < size) ? i : -1;
            }
        } else {
            sync_count = 0;
        }
    }
    
    return -1;
}

/**
 * @brief Decode sector from GCR track data
 */
static int decode_sector(const uint8_t *track, size_t size, size_t pos,
                         nib_sector_t *sector)
{
    if (pos + 10 >= size) return -1;
    
    uint8_t header[4];
    
    /* Decode header block */
    if (gcr_decode_5to4(track + pos, header) != 0) return -1;
    
    if (header[0] != 0x08) return -1;  /* Not a header block */
    
    sector->sector = header[2];
    sector->track = header[3];
    
    /* Decode second part of header */
    uint8_t header2[4];
    if (gcr_decode_5to4(track + pos + 5, header2) != 0) return -1;
    
    /* Verify header checksum */
    uint8_t expected_checksum = sector->sector ^ sector->track ^ header2[0] ^ header2[1];
    if (expected_checksum != header[1]) {
        sector->valid = false;
        return -1;
    }
    
    /* Find data sync */
    int data_pos = find_sync(track, size, pos + 10);
    if (data_pos < 0 || data_pos + 325 >= (int)size) return -1;
    
    /* Decode data block ID */
    uint8_t data_header[4];
    if (gcr_decode_5to4(track + data_pos, data_header) != 0) return -1;
    
    if (data_header[0] != 0x07) return -1;  /* Not a data block */
    
    /* Decode 256 data bytes */
    sector->data[0] = data_header[1];
    sector->data[1] = data_header[2];
    sector->data[2] = data_header[3];
    
    size_t gcr_pos = data_pos + 5;
    for (int i = 3; i < 256; i += 4) {
        uint8_t decoded[4];
        if (gcr_decode_5to4(track + gcr_pos, decoded) != 0) return -1;
        
        sector->data[i] = decoded[0];
        if (i + 1 < 256) sector->data[i + 1] = decoded[1];
        if (i + 2 < 256) sector->data[i + 2] = decoded[2];
        if (i + 3 < 256) sector->data[i + 3] = decoded[3];
        
        gcr_pos += 5;
    }
    
    sector->valid = true;
    return 0;
}

/* ============================================================================
 * NIB Loader
 * ============================================================================ */

/**
 * @brief Load NIB file
 */
int nib_load(const char *filename, nib_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    memset(img, 0, sizeof(*img));
    
    /* Determine format from size */
    if (size == NIB_FILE_SIZE) {
        img->half_tracks = false;
        img->num_tracks = NIB_TRACKS;
    } else if (size == NIB_HALF_FILE_SIZE) {
        img->half_tracks = true;
        img->num_tracks = NIB_HALF_TRACKS;
    } else {
        fclose(fp);
        return -1;
    }
    
    /* Load tracks */
    for (int t = 0; t < img->num_tracks; t++) {
        img->tracks[t] = malloc(NIB_TRACK_SIZE);
        if (!img->tracks[t]) {
            fclose(fp);
            nib_free(img);
            return -1;
        }
        
        if (fread(img->tracks[t], 1, NIB_TRACK_SIZE, fp) != NIB_TRACK_SIZE) { /* I/O error */ }
        img->track_size[t] = NIB_TRACK_SIZE;
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Convert NIB to D64
 */
int nib_to_d64(const nib_image_t *nib, uint8_t *d64_data)
{
    if (!nib || !d64_data) return -1;
    
    size_t d64_offset = 0;
    
    for (int t = 0; t < 35; t++) {
        int track_idx = nib->half_tracks ? t * 2 : t;
        if (!nib->tracks[track_idx]) continue;
        
        int sectors = SECTORS_PER_TRACK[t];
        bool found[21] = {false};
        
        /* Find all sectors on track */
        size_t pos = 0;
        while (pos < nib->track_size[track_idx]) {
            int sync_pos = find_sync(nib->tracks[track_idx], 
                                     nib->track_size[track_idx], pos);
            if (sync_pos < 0) break;
            
            nib_sector_t sector;
            if (decode_sector(nib->tracks[track_idx], 
                              nib->track_size[track_idx],
                              sync_pos, &sector) == 0) {
                
                if (sector.track == t + 1 && 
                    sector.sector < sectors && 
                    !found[sector.sector]) {
                    
                    /* Copy sector data to D64 */
                    size_t sector_offset = d64_offset + sector.sector * 256;
                    memcpy(d64_data + sector_offset, sector.data, 256);
                    found[sector.sector] = true;
                }
            }
            
            pos = sync_pos + 10;
        }
        
        d64_offset += sectors * 256;
    }
    
    return 0;
}

/* ============================================================================
 * NIB Writer
 * ============================================================================ */

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 */
static void gcr_encode_4to5(const uint8_t *in, uint8_t *out)
{
    uint8_t g[8];
    
    g[0] = GCR_ENCODE[in[0] >> 4];
    g[1] = GCR_ENCODE[in[0] & 0x0F];
    g[2] = GCR_ENCODE[in[1] >> 4];
    g[3] = GCR_ENCODE[in[1] & 0x0F];
    g[4] = GCR_ENCODE[in[2] >> 4];
    g[5] = GCR_ENCODE[in[2] & 0x0F];
    g[6] = GCR_ENCODE[in[3] >> 4];
    g[7] = GCR_ENCODE[in[3] & 0x0F];
    
    out[0] = (g[0] << 3) | (g[1] >> 2);
    out[1] = (g[1] << 6) | (g[2] << 1) | (g[3] >> 4);
    out[2] = (g[3] << 4) | (g[4] >> 1);
    out[3] = (g[4] << 7) | (g[5] << 2) | (g[6] >> 3);
    out[4] = (g[6] << 5) | g[7];
}

/**
 * @brief Encode sector to GCR
 */
static size_t encode_sector_gcr(int track, int sector, const uint8_t *data,
                                uint8_t id1, uint8_t id2, uint8_t *out)
{
    size_t pos = 0;
    uint8_t raw[4], gcr[5];
    
    /* Sync */
    memset(out + pos, 0xFF, 5);
    pos += 5;
    
    /* Header */
    uint8_t checksum = sector ^ track ^ id2 ^ id1;
    raw[0] = 0x08;
    raw[1] = checksum;
    raw[2] = sector;
    raw[3] = track;
    gcr_encode_4to5(raw, out + pos);
    pos += 5;
    
    raw[0] = id2;
    raw[1] = id1;
    raw[2] = 0x0F;
    raw[3] = 0x0F;
    gcr_encode_4to5(raw, out + pos);
    pos += 5;
    
    /* Header gap */
    memset(out + pos, 0x55, 9);
    pos += 9;
    
    /* Data sync */
    memset(out + pos, 0xFF, 5);
    pos += 5;
    
    /* Data block */
    checksum = 0;
    for (int i = 0; i < 256; i++) checksum ^= data[i];
    
    raw[0] = 0x07;
    raw[1] = data[0];
    raw[2] = data[1];
    raw[3] = data[2];
    gcr_encode_4to5(raw, out + pos);
    pos += 5;
    
    for (int i = 3; i < 255; i += 4) {
        raw[0] = data[i];
        raw[1] = (i + 1 < 256) ? data[i + 1] : 0;
        raw[2] = (i + 2 < 256) ? data[i + 2] : 0;
        raw[3] = (i + 3 < 256) ? data[i + 3] : 0;
        gcr_encode_4to5(raw, out + pos);
        pos += 5;
    }
    
    raw[0] = data[255];
    raw[1] = checksum;
    raw[2] = 0x00;
    raw[3] = 0x00;
    gcr_encode_4to5(raw, out + pos);
    pos += 5;
    
    /* Tail gap */
    memset(out + pos, 0x55, 8);
    pos += 8;
    
    return pos;
}

/**
 * @brief Convert D64 to NIB
 */
int nib_from_d64(const uint8_t *d64_data, size_t d64_size, nib_image_t *nib)
{
    if (!d64_data || !nib) return -1;
    
    memset(nib, 0, sizeof(*nib));
    nib->half_tracks = false;
    nib->num_tracks = 35;
    
    size_t d64_offset = 0;
    
    for (int t = 0; t < 35; t++) {
        nib->tracks[t] = calloc(1, NIB_TRACK_SIZE);
        if (!nib->tracks[t]) {
            nib_free(nib);
            return -1;
        }
        nib->track_size[t] = NIB_TRACK_SIZE;
        
        /* Fill with gap bytes */
        memset(nib->tracks[t], 0x55, NIB_TRACK_SIZE);
        
        int sectors = SECTORS_PER_TRACK[t];
        size_t track_pos = 0;
        
        /* Lead-in sync */
        memset(nib->tracks[t], 0xFF, 40);
        track_pos = 40;
        
        for (int s = 0; s < sectors; s++) {
            size_t len = encode_sector_gcr(t + 1, s, 
                                           d64_data + d64_offset + s * 256,
                                           '0', '0',
                                           nib->tracks[t] + track_pos);
            track_pos += len;
            
            if (track_pos >= NIB_TRACK_SIZE - 400) break;
        }
        
        d64_offset += sectors * 256;
    }
    
    return 0;
}

/**
 * @brief Save NIB image to file
 */
int nib_save(const nib_image_t *nib, const char *filename)
{
    if (!nib || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    for (int t = 0; t < nib->num_tracks; t++) {
        if (nib->tracks[t]) {
            if (fwrite(nib->tracks[t], 1, NIB_TRACK_SIZE, fp) != NIB_TRACK_SIZE) { /* I/O error */ }
        } else {
            uint8_t empty[NIB_TRACK_SIZE] = {0};
            if (fwrite(empty, 1, NIB_TRACK_SIZE, fp) != NIB_TRACK_SIZE) { /* I/O error */ }
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Free NIB image
 */
void nib_free(nib_image_t *nib)
{
    if (nib) {
        for (int t = 0; t < NIB_HALF_TRACKS; t++) {
            free(nib->tracks[t]);
            nib->tracks[t] = NULL;
        }
        nib->num_tracks = 0;
    }
}
