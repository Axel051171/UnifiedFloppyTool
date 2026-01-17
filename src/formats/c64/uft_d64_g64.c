/**
 * @file uft_d64_g64.c
 * @brief D64/G64 Disk Image Format Conversion Implementation
 * 
 * Based on nibtools by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/c64/uft_d64_g64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Sectors per track for 1541 (track 1-42) */
static const int sector_map[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1 - 10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11 - 20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21 - 30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  /* 31 - 40 */
    17, 17                                    /* 41 - 42 */
};

/** Speed zone per track */
static const int speed_map[43] = {
    0,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /*  1 - 10 */
    3, 3, 3, 3, 3, 3, 3, 2, 2, 2,  /* 11 - 20 */
    2, 2, 2, 2, 1, 1, 1, 1, 1, 1,  /* 21 - 30 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 31 - 40 */
    0, 0                           /* 41 - 42 */
};

/** Default inter-sector gap length */
static const int gap_map[43] = {
    0,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9,   /*  1 - 10 */
    9, 9, 9, 9, 9, 9, 9, 19, 19, 19, /* 11 - 20 */
    19, 19, 19, 19, 13, 13, 13, 13, 13, 13, /* 21 - 30 */
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, /* 31 - 40 */
    10, 10                          /* 41 - 42 */
};

/** Track capacity at each density */
static const size_t capacity_map[4] = { 6250, 6666, 7142, 7692 };

/** Cumulative block count before each track */
static const int block_offset[43] = {
    0,
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189,     /*  1 - 10 */
    210, 231, 252, 273, 294, 315, 336, 357, 376, 395, /* 11 - 20 */
    414, 433, 452, 471, 490, 508, 526, 544, 562, 580, /* 21 - 30 */
    598, 615, 632, 649, 666, 683, 700, 717, 734, 751, /* 31 - 40 */
    768, 785                                          /* 41 - 42 */
};

/** GCR encode table */
static const uint8_t gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/** GCR decode table (high nibble) */
static const uint8_t gcr_decode_high[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x80, 0x00, 0x10, 0xFF, 0xC0, 0x40, 0x50,
    0xFF, 0xFF, 0x20, 0x30, 0xFF, 0xF0, 0x60, 0x70,
    0xFF, 0x90, 0xA0, 0xB0, 0xFF, 0xD0, 0xE0, 0xFF
};

/** GCR decode table (low nibble) */
static const uint8_t gcr_decode_low[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/** Error names */
static const char *error_names[] = {
    "Unknown",
    "OK",
    "Header not found",
    "No sync",
    "Data not found",
    "Data checksum",
    "Write verify",
    "Write protected",
    "Unknown",
    "Header checksum",
    "Data extend",
    "ID mismatch"
};

/* ============================================================================
 * GCR Encoding Helpers
 * ============================================================================ */

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 */
static void encode_4_to_5(const uint8_t *plain, uint8_t *gcr)
{
    uint8_t g0 = gcr_encode[plain[0] >> 4];
    uint8_t g1 = gcr_encode[plain[0] & 0x0F];
    uint8_t g2 = gcr_encode[plain[1] >> 4];
    uint8_t g3 = gcr_encode[plain[1] & 0x0F];
    uint8_t g4 = gcr_encode[plain[2] >> 4];
    uint8_t g5 = gcr_encode[plain[2] & 0x0F];
    uint8_t g6 = gcr_encode[plain[3] >> 4];
    uint8_t g7 = gcr_encode[plain[3] & 0x0F];
    
    gcr[0] = (g0 << 3) | (g1 >> 2);
    gcr[1] = (g1 << 6) | (g2 << 1) | (g3 >> 4);
    gcr[2] = (g3 << 4) | (g4 >> 1);
    gcr[3] = (g4 << 7) | (g5 << 2) | (g6 >> 3);
    gcr[4] = (g6 << 5) | g7;
}

/**
 * @brief Decode 5 GCR bytes to 4 bytes
 */
static int decode_5_to_4(const uint8_t *gcr, uint8_t *plain)
{
    uint8_t g0 = (gcr[0] >> 3) & 0x1F;
    uint8_t g1 = ((gcr[0] << 2) | (gcr[1] >> 6)) & 0x1F;
    uint8_t g2 = (gcr[1] >> 1) & 0x1F;
    uint8_t g3 = ((gcr[1] << 4) | (gcr[2] >> 4)) & 0x1F;
    uint8_t g4 = ((gcr[2] << 1) | (gcr[3] >> 7)) & 0x1F;
    uint8_t g5 = (gcr[3] >> 2) & 0x1F;
    uint8_t g6 = ((gcr[3] << 3) | (gcr[4] >> 5)) & 0x1F;
    uint8_t g7 = gcr[4] & 0x1F;
    
    int errors = 0;
    
    uint8_t h0 = gcr_decode_high[g0];
    uint8_t l0 = gcr_decode_low[g1];
    uint8_t h1 = gcr_decode_high[g2];
    uint8_t l1 = gcr_decode_low[g3];
    uint8_t h2 = gcr_decode_high[g4];
    uint8_t l2 = gcr_decode_low[g5];
    uint8_t h3 = gcr_decode_high[g6];
    uint8_t l3 = gcr_decode_low[g7];
    
    if (h0 == 0xFF || l0 == 0xFF) errors++;
    if (h1 == 0xFF || l1 == 0xFF) errors++;
    if (h2 == 0xFF || l2 == 0xFF) errors++;
    if (h3 == 0xFF || l3 == 0xFF) errors++;
    
    plain[0] = (h0 & 0xF0) | (l0 & 0x0F);
    plain[1] = (h1 & 0xF0) | (l1 & 0x0F);
    plain[2] = (h2 & 0xF0) | (l2 & 0x0F);
    plain[3] = (h3 & 0xF0) | (l3 & 0x0F);
    
    return errors;
}

/* ============================================================================
 * D64 Functions
 * ============================================================================ */

/**
 * @brief Load D64 from buffer
 */
int d64_load_buffer(const uint8_t *data, size_t size, d64_image_t **image)
{
    if (!data || !image) return -1;
    
    int num_tracks;
    int num_blocks;
    bool has_errors = false;
    
    /* Determine image type from size */
    switch (size) {
        case D64_SIZE_35:
            num_tracks = 35;
            num_blocks = D64_BLOCKS_35;
            break;
        case D64_SIZE_35_ERR:
            num_tracks = 35;
            num_blocks = D64_BLOCKS_35;
            has_errors = true;
            break;
        case D64_SIZE_40:
            num_tracks = 40;
            num_blocks = D64_BLOCKS_40;
            break;
        case D64_SIZE_40_ERR:
            num_tracks = 40;
            num_blocks = D64_BLOCKS_40;
            has_errors = true;
            break;
        default:
            /* Try to handle non-standard sizes */
            if (size >= D64_SIZE_35 && size < D64_SIZE_35_ERR) {
                num_tracks = 35;
                num_blocks = D64_BLOCKS_35;
            } else if (size >= D64_SIZE_40) {
                num_tracks = 40;
                num_blocks = D64_BLOCKS_40;
                has_errors = (size > D64_SIZE_40);
            } else {
                return -2;  /* Invalid size */
            }
    }
    
    /* Allocate image */
    d64_image_t *img = calloc(1, sizeof(d64_image_t));
    if (!img) return -3;
    
    img->num_tracks = num_tracks;
    img->num_blocks = num_blocks;
    img->has_errors = has_errors;
    
    /* Allocate data */
    img->data = malloc(num_blocks * D64_SECTOR_SIZE);
    if (!img->data) {
        free(img);
        return -4;
    }
    memcpy(img->data, data, num_blocks * D64_SECTOR_SIZE);
    
    /* Allocate errors if present */
    if (has_errors) {
        img->errors = malloc(num_blocks);
        if (!img->errors) {
            free(img->data);
            free(img);
            return -5;
        }
        memcpy(img->errors, data + (num_blocks * D64_SECTOR_SIZE), num_blocks);
    }
    
    /* Extract disk ID and name from BAM (track 18, sector 0) */
    int bam_offset = d64_block_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    if (bam_offset >= 0) {
        const uint8_t *bam = img->data + (bam_offset * D64_SECTOR_SIZE);
        
        /* Disk ID at offset 0xA2 */
        img->disk_id[0] = bam[D64_BAM_ID_OFFSET];
        img->disk_id[1] = bam[D64_BAM_ID_OFFSET + 1];
        
        /* Disk name at offset 0x90 (16 chars, padded with 0xA0) */
        memcpy(img->disk_name, bam + D64_BAM_NAME_OFFSET, 16);
        img->disk_name[16] = '\0';
        
        /* Trim 0xA0 padding */
        for (int i = 15; i >= 0; i--) {
            if ((uint8_t)img->disk_name[i] == 0xA0) {
                img->disk_name[i] = '\0';
            } else {
                break;
            }
        }
        
        /* DOS type at offset 0xA5 */
        img->dos_type[0] = bam[0xA5];
        img->dos_type[1] = bam[0xA6];
        img->dos_type[2] = '\0';
    }
    
    *image = img;
    return 0;
}

/**
 * @brief Load D64 file
 */
int d64_load(const char *filename, d64_image_t **image)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -2;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    
    int result = d64_load_buffer(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Save D64 to buffer
 */
int d64_save_buffer(const d64_image_t *image, uint8_t **data, size_t *size, bool include_errors)
{
    if (!image || !data || !size) return -1;
    
    size_t data_size = image->num_blocks * D64_SECTOR_SIZE;
    size_t total_size = data_size;
    
    if (include_errors && image->has_errors) {
        total_size += image->num_blocks;
    }
    
    uint8_t *buf = malloc(total_size);
    if (!buf) return -2;
    
    memcpy(buf, image->data, data_size);
    
    if (include_errors && image->errors) {
        memcpy(buf + data_size, image->errors, image->num_blocks);
    }
    
    *data = buf;
    *size = total_size;
    
    return 0;
}

/**
 * @brief Save D64 file
 */
int d64_save(const char *filename, const d64_image_t *image, bool include_errors)
{
    uint8_t *data;
    size_t size;
    
    int result = d64_save_buffer(image, &data, &size, include_errors);
    if (result != 0) return result;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(data);
        return -1;
    }
    
    if (fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        free(data);
        return -2;
    }
    
    fclose(fp);
    free(data);
    
    return 0;
}

/**
 * @brief Free D64 image
 */
void d64_free(d64_image_t *image)
{
    if (!image) return;
    free(image->data);
    free(image->errors);
    free(image);
}

/**
 * @brief Create new D64 image
 */
d64_image_t *d64_create(int num_tracks)
{
    if (num_tracks != 35 && num_tracks != 40) return NULL;
    
    d64_image_t *img = calloc(1, sizeof(d64_image_t));
    if (!img) return NULL;
    
    img->num_tracks = num_tracks;
    img->num_blocks = (num_tracks == 35) ? D64_BLOCKS_35 : D64_BLOCKS_40;
    
    img->data = calloc(img->num_blocks, D64_SECTOR_SIZE);
    if (!img->data) {
        free(img);
        return NULL;
    }
    
    /* Set default disk ID */
    img->disk_id[0] = '0';
    img->disk_id[1] = '0';
    
    return img;
}

/**
 * @brief Get sector from D64
 */
int d64_get_sector(const d64_image_t *image, int track, int sector,
                   uint8_t *data, d64_error_t *error)
{
    if (!image || !data) return -1;
    if (track < 1 || track > image->num_tracks) return -2;
    if (sector < 0 || sector >= sector_map[track]) return -3;
    
    int offset = d64_block_offset(track, sector);
    if (offset < 0 || offset >= image->num_blocks) return -4;
    
    memcpy(data, image->data + (offset * D64_SECTOR_SIZE), D64_SECTOR_SIZE);
    
    if (error) {
        *error = (image->errors) ? image->errors[offset] : D64_ERR_OK;
    }
    
    return 0;
}

/**
 * @brief Set sector in D64
 */
int d64_set_sector(d64_image_t *image, int track, int sector,
                   const uint8_t *data, d64_error_t error)
{
    if (!image || !data) return -1;
    if (track < 1 || track > image->num_tracks) return -2;
    if (sector < 0 || sector >= sector_map[track]) return -3;
    
    int offset = d64_block_offset(track, sector);
    if (offset < 0 || offset >= image->num_blocks) return -4;
    
    memcpy(image->data + (offset * D64_SECTOR_SIZE), data, D64_SECTOR_SIZE);
    
    if (image->errors) {
        image->errors[offset] = error;
    }
    
    return 0;
}

/**
 * @brief Get sectors on track
 */
int d64_sectors_on_track(int track)
{
    if (track < 1 || track > 42) return 0;
    return sector_map[track];
}

/**
 * @brief Get block offset
 */
int d64_block_offset(int track, int sector)
{
    if (track < 1 || track > 42) return -1;
    if (sector < 0 || sector >= sector_map[track]) return -1;
    return block_offset[track] + sector;
}

/* ============================================================================
 * G64 Functions
 * ============================================================================ */

/**
 * @brief Load G64 from buffer
 */
int g64_load_buffer(const uint8_t *data, size_t size, g64_image_t **image)
{
    if (!data || !image || size < G64_HEADER_SIZE) return -1;
    
    /* Check signature */
    if (memcmp(data, G64_SIGNATURE, G64_SIGNATURE_LEN) != 0) {
        return -2;
    }
    
    /* Allocate image */
    g64_image_t *img = calloc(1, sizeof(g64_image_t));
    if (!img) return -3;
    
    img->version = data[8];
    img->num_tracks = data[9];
    img->max_track_size = data[10] | (data[11] << 8);
    
    /* Check for extended format */
    if (size >= G64_HEADER_SIZE_EXT && memcmp(data + 0x2AC, G64_EXT_SIGNATURE, 3) == 0) {
        img->extended = true;
    }
    
    /* Read track entries */
    for (int i = 0; i < img->num_tracks && i < G64_MAX_TRACKS; i++) {
        int halftrack = i + 2;  /* G64 starts at halftrack 2 */
        
        /* Track offset */
        int offset_pos = G64_TRACK_OFFSET + (i * 4);
        img->tracks[halftrack].offset = data[offset_pos] | 
                                        (data[offset_pos + 1] << 8) |
                                        (data[offset_pos + 2] << 16) |
                                        (data[offset_pos + 3] << 24);
        
        /* Speed zone */
        img->tracks[halftrack].speed = data[G64_SPEED_OFFSET + i];
        
        /* Load track data if present */
        if (img->tracks[halftrack].offset > 0 && img->tracks[halftrack].offset < size) {
            uint32_t track_offset = img->tracks[halftrack].offset;
            
            /* Read track length */
            img->tracks[halftrack].length = data[track_offset] | (data[track_offset + 1] << 8);
            
            if (img->tracks[halftrack].length > 0 && 
                track_offset + 2 + img->tracks[halftrack].length <= size) {
                
                img->track_data[halftrack] = malloc(img->tracks[halftrack].length);
                if (img->track_data[halftrack]) {
                    memcpy(img->track_data[halftrack], 
                           data + track_offset + 2, 
                           img->tracks[halftrack].length);
                }
            }
        }
    }
    
    *image = img;
    return 0;
}

/**
 * @brief Load G64 file
 */
int g64_load(const char *filename, g64_image_t **image)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -2;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    
    int result = g64_load_buffer(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Save G64 to buffer
 */
int g64_save_buffer(const g64_image_t *image, uint8_t **data, size_t *size)
{
    if (!image || !data || !size) return -1;
    
    /* Calculate total size */
    size_t total_size = G64_HEADER_SIZE;
    
    for (int i = 2; i < G64_MAX_TRACKS; i++) {
        if (image->track_data[i] && image->tracks[i].length > 0) {
            total_size += 2 + image->tracks[i].length;  /* 2 bytes for length + data */
        }
    }
    
    /* Allocate buffer */
    uint8_t *buf = calloc(1, total_size);
    if (!buf) return -2;
    
    /* Write header */
    memcpy(buf, G64_SIGNATURE, G64_SIGNATURE_LEN);
    buf[8] = image->version;
    buf[9] = image->num_tracks;
    buf[10] = image->max_track_size & 0xFF;
    buf[11] = (image->max_track_size >> 8) & 0xFF;
    
    /* Write track offsets and data */
    uint32_t current_offset = G64_HEADER_SIZE;
    
    for (int i = 0; i < image->num_tracks && i < G64_MAX_TRACKS - 2; i++) {
        int halftrack = i + 2;
        
        if (image->track_data[halftrack] && image->tracks[halftrack].length > 0) {
            /* Write offset */
            int offset_pos = G64_TRACK_OFFSET + (i * 4);
            buf[offset_pos] = current_offset & 0xFF;
            buf[offset_pos + 1] = (current_offset >> 8) & 0xFF;
            buf[offset_pos + 2] = (current_offset >> 16) & 0xFF;
            buf[offset_pos + 3] = (current_offset >> 24) & 0xFF;
            
            /* Write speed zone */
            buf[G64_SPEED_OFFSET + i] = image->tracks[halftrack].speed;
            
            /* Write track length */
            buf[current_offset] = image->tracks[halftrack].length & 0xFF;
            buf[current_offset + 1] = (image->tracks[halftrack].length >> 8) & 0xFF;
            
            /* Write track data */
            memcpy(buf + current_offset + 2, 
                   image->track_data[halftrack], 
                   image->tracks[halftrack].length);
            
            current_offset += 2 + image->tracks[halftrack].length;
        }
    }
    
    *data = buf;
    *size = total_size;
    
    return 0;
}

/**
 * @brief Save G64 file
 */
int g64_save(const char *filename, const g64_image_t *image)
{
    uint8_t *data;
    size_t size;
    
    int result = g64_save_buffer(image, &data, &size);
    if (result != 0) return result;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(data);
        return -1;
    }
    
    if (fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        free(data);
        return -2;
    }
    
    fclose(fp);
    free(data);
    
    return 0;
}

/**
 * @brief Free G64 image
 */
void g64_free(g64_image_t *image)
{
    if (!image) return;
    
    for (int i = 0; i < G64_MAX_TRACKS; i++) {
        free(image->track_data[i]);
    }
    
    free(image);
}

/**
 * @brief Create new G64 image
 */
g64_image_t *g64_create(int num_tracks, bool include_halftracks)
{
    g64_image_t *img = calloc(1, sizeof(g64_image_t));
    if (!img) return NULL;
    
    img->version = G64_VERSION;
    img->num_tracks = include_halftracks ? (num_tracks * 2) : num_tracks;
    img->max_track_size = G64_MAX_TRACK_SIZE;
    
    return img;
}

/**
 * @brief Get track from G64
 */
int g64_get_track(const g64_image_t *image, int halftrack,
                  const uint8_t **data, size_t *length, uint8_t *speed)
{
    if (!image || halftrack < 2 || halftrack >= G64_MAX_TRACKS) return -1;
    if (!image->track_data[halftrack]) return -2;
    
    if (data) *data = image->track_data[halftrack];
    if (length) *length = image->tracks[halftrack].length;
    if (speed) *speed = image->tracks[halftrack].speed;
    
    return 0;
}

/**
 * @brief Set track in G64
 */
int g64_set_track(g64_image_t *image, int halftrack,
                  const uint8_t *data, size_t length, uint8_t speed)
{
    if (!image || !data || halftrack < 2 || halftrack >= G64_MAX_TRACKS) return -1;
    
    /* Free existing */
    free(image->track_data[halftrack]);
    
    /* Allocate and copy */
    image->track_data[halftrack] = malloc(length);
    if (!image->track_data[halftrack]) return -2;
    
    memcpy(image->track_data[halftrack], data, length);
    image->tracks[halftrack].length = length;
    image->tracks[halftrack].speed = speed;
    
    return 0;
}

/* ============================================================================
 * GCR Sector Conversion
 * ============================================================================ */

/**
 * @brief Convert sector to GCR
 */
size_t sector_to_gcr(const uint8_t *sector_data, uint8_t *gcr_output,
                     int track, int sector, const uint8_t *disk_id,
                     d64_error_t error)
{
    if (!sector_data || !gcr_output || !disk_id) return 0;
    
    uint8_t *out = gcr_output;
    
    /* Sync mark (5 bytes of 0xFF) */
    memset(out, 0xFF, 5);
    out += 5;
    
    /* Header block */
    uint8_t header[8];
    header[0] = 0x08;  /* Header marker */
    header[1] = sector ^ track ^ disk_id[0] ^ disk_id[1];  /* Checksum */
    header[2] = sector;
    header[3] = track;
    header[4] = disk_id[1];
    header[5] = disk_id[0];
    header[6] = 0x0F;  /* Padding */
    header[7] = 0x0F;
    
    /* Encode header (8 bytes -> 10 GCR bytes) */
    encode_4_to_5(header, out);
    encode_4_to_5(header + 4, out + 5);
    out += 10;
    
    /* Header gap */
    memset(out, 0x55, 9);
    out += 9;
    
    /* Sync mark before data */
    memset(out, 0xFF, 5);
    out += 5;
    
    /* Data block */
    uint8_t data_block[260];
    data_block[0] = 0x07;  /* Data marker */
    memcpy(data_block + 1, sector_data, 256);
    
    /* Calculate data checksum */
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        checksum ^= sector_data[i];
    }
    data_block[257] = checksum;
    data_block[258] = 0x00;  /* Padding */
    data_block[259] = 0x00;
    
    /* Encode data (260 bytes -> 325 GCR bytes) */
    for (int i = 0; i < 260; i += 4) {
        encode_4_to_5(data_block + i, out);
        out += 5;
    }
    
    /* Inter-sector gap */
    int gap_len = gap_map[track];
    memset(out, 0x55, gap_len);
    out += gap_len;
    
    return (size_t)(out - gcr_output);
}

/**
 * @brief Convert GCR to sector
 */
int gcr_to_sector(const uint8_t *gcr_data, size_t gcr_length,
                  uint8_t *sector_output, int *track_out, int *sector_out,
                  uint8_t *id_out, d64_error_t *error_out)
{
    if (!gcr_data || !sector_output || gcr_length < 350) return -1;
    
    d64_error_t error = D64_ERR_OK;
    
    /* Find sync mark */
    size_t pos = 0;
    while (pos < gcr_length - 5 && gcr_data[pos] != 0xFF) pos++;
    while (pos < gcr_length && gcr_data[pos] == 0xFF) pos++;
    
    if (pos >= gcr_length - 10) {
        if (error_out) *error_out = D64_ERR_NO_SYNC;
        return -2;
    }
    
    /* Decode header */
    uint8_t header[8];
    if (decode_5_to_4(gcr_data + pos, header) > 0 ||
        decode_5_to_4(gcr_data + pos + 5, header + 4) > 0) {
        error = D64_ERR_HEADER_CHECKSUM;
    }
    pos += 10;
    
    if (header[0] != 0x08) {
        if (error_out) *error_out = D64_ERR_HEADER_NOT_FOUND;
        return -3;
    }
    
    /* Extract header info */
    uint8_t h_checksum = header[1];
    uint8_t h_sector = header[2];
    uint8_t h_track = header[3];
    uint8_t h_id1 = header[5];
    uint8_t h_id0 = header[4];
    
    /* Verify header checksum */
    if (h_checksum != (h_sector ^ h_track ^ h_id0 ^ h_id1)) {
        error = D64_ERR_HEADER_CHECKSUM;
    }
    
    if (track_out) *track_out = h_track;
    if (sector_out) *sector_out = h_sector;
    if (id_out) {
        id_out[0] = h_id1;
        id_out[1] = h_id0;
    }
    
    /* Skip gap and find data sync */
    while (pos < gcr_length && gcr_data[pos] != 0xFF) pos++;
    while (pos < gcr_length && gcr_data[pos] == 0xFF) pos++;
    
    if (pos >= gcr_length - 325) {
        if (error_out) *error_out = D64_ERR_DATA_NOT_FOUND;
        return -4;
    }
    
    /* Decode data block */
    uint8_t data_block[260];
    int gcr_errors = 0;
    
    for (int i = 0; i < 260; i += 4) {
        gcr_errors += decode_5_to_4(gcr_data + pos, data_block + i);
        pos += 5;
    }
    
    if (gcr_errors > 0) {
        error = D64_ERR_CHECKSUM;
    }
    
    if (data_block[0] != 0x07) {
        if (error_out) *error_out = D64_ERR_DATA_NOT_FOUND;
        return -5;
    }
    
    /* Copy sector data */
    memcpy(sector_output, data_block + 1, 256);
    
    /* Verify data checksum */
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        checksum ^= sector_output[i];
    }
    
    if (checksum != data_block[257]) {
        error = D64_ERR_CHECKSUM;
    }
    
    if (error_out) *error_out = error;
    
    return (error == D64_ERR_OK) ? 0 : -6;
}

/**
 * @brief Build GCR track from sectors
 */
size_t build_gcr_track(const uint8_t **sectors, int num_sectors,
                       uint8_t *gcr_output, int track, const uint8_t *disk_id,
                       uint8_t gap_fill)
{
    if (!sectors || !gcr_output || !disk_id || num_sectors <= 0) return 0;
    
    uint8_t *out = gcr_output;
    
    for (int s = 0; s < num_sectors; s++) {
        if (sectors[s]) {
            size_t sector_len = sector_to_gcr(sectors[s], out, track, s, disk_id, D64_ERR_OK);
            out += sector_len;
        }
    }
    
    /* Fill remaining with gap */
    size_t used = (size_t)(out - gcr_output);
    size_t capacity = capacity_map[speed_map[track]];
    
    if (used < capacity) {
        memset(out, gap_fill, capacity - used);
        out += (capacity - used);
    }
    
    return (size_t)(out - gcr_output);
}

/* ============================================================================
 * Conversion Functions
 * ============================================================================ */

/**
 * @brief Get default options
 */
void convert_get_defaults(convert_options_t *options)
{
    if (!options) return;
    
    options->align_tracks = false;
    options->include_halftracks = false;
    options->extended_tracks = false;
    options->generate_errors = true;
    options->gap_fill = 0x55;
    options->sync_length = 5;
}

/**
 * @brief Convert D64 to G64
 */
int d64_to_g64(const d64_image_t *d64, g64_image_t **g64,
               const convert_options_t *options, convert_result_t *result)
{
    if (!d64 || !g64) return -1;
    
    convert_options_t opts;
    if (options) {
        opts = *options;
    } else {
        convert_get_defaults(&opts);
    }
    
    /* Create G64 image */
    int num_tracks = opts.extended_tracks ? 42 : d64->num_tracks;
    g64_image_t *img = g64_create(num_tracks, opts.include_halftracks);
    if (!img) return -2;
    
    int tracks_converted = 0;
    int sectors_converted = 0;
    
    /* Convert each track */
    for (int track = 1; track <= num_tracks; track++) {
        int halftrack = track * 2;
        int num_sectors = sector_map[track];
        
        if (track > d64->num_tracks) {
            num_sectors = 0;  /* Extended tracks are empty */
        }
        
        /* Allocate sector pointers */
        const uint8_t *sector_ptrs[21] = {0};
        uint8_t sector_data[21][256];
        
        /* Read sectors */
        for (int s = 0; s < num_sectors; s++) {
            if (d64_get_sector(d64, track, s, sector_data[s], NULL) == 0) {
                sector_ptrs[s] = sector_data[s];
                sectors_converted++;
            }
        }
        
        /* Build GCR track */
        uint8_t gcr_track[8192];
        size_t gcr_len = build_gcr_track(sector_ptrs, num_sectors, gcr_track,
                                          track, d64->disk_id, opts.gap_fill);
        
        if (gcr_len > 0) {
            g64_set_track(img, halftrack, gcr_track, gcr_len, speed_map[track]);
            tracks_converted++;
        }
    }
    
    *g64 = img;
    
    if (result) {
        result->success = true;
        result->tracks_converted = tracks_converted;
        result->sectors_converted = sectors_converted;
        result->errors_found = 0;
        snprintf(result->description, sizeof(result->description),
                 "Converted %d tracks, %d sectors", tracks_converted, sectors_converted);
    }
    
    return 0;
}

/**
 * @brief Convert G64 to D64
 */
int g64_to_d64(const g64_image_t *g64, d64_image_t **d64,
               const convert_options_t *options, convert_result_t *result)
{
    if (!g64 || !d64) return -1;
    
    convert_options_t opts;
    if (options) {
        opts = *options;
    } else {
        convert_get_defaults(&opts);
    }
    
    /* Determine number of tracks */
    int num_tracks = 35;
    for (int t = 36 * 2; t <= 40 * 2; t += 2) {
        if (g64->track_data[t] && g64->tracks[t].length > 0) {
            num_tracks = 40;
            break;
        }
    }
    
    /* Create D64 image */
    d64_image_t *img = d64_create(num_tracks);
    if (!img) return -2;
    
    if (opts.generate_errors) {
        img->errors = calloc(img->num_blocks, 1);
        if (img->errors) {
            memset(img->errors, D64_ERR_OK, img->num_blocks);
            img->has_errors = true;
        }
    }
    
    int tracks_converted = 0;
    int sectors_converted = 0;
    int errors_found = 0;
    
    /* Convert each track */
    for (int track = 1; track <= num_tracks; track++) {
        int halftrack = track * 2;
        
        if (!g64->track_data[halftrack] || g64->tracks[halftrack].length == 0) {
            continue;
        }
        
        const uint8_t *gcr_data = g64->track_data[halftrack];
        size_t gcr_len = g64->tracks[halftrack].length;
        
        /* Extract sectors from GCR track */
        uint8_t sector_data[256];
        int num_sectors = sector_map[track];
        
        /* Simple sector extraction - find each sector */
        size_t pos = 0;
        int sectors_found = 0;
        
        while (pos < gcr_len - 350 && sectors_found < num_sectors) {
            /* Find sync */
            while (pos < gcr_len && gcr_data[pos] != 0xFF) pos++;
            if (pos >= gcr_len) break;
            while (pos < gcr_len && gcr_data[pos] == 0xFF) pos++;
            if (pos >= gcr_len - 10) break;
            
            /* Try to decode header */
            uint8_t header[8];
            decode_5_to_4(gcr_data + pos, header);
            decode_5_to_4(gcr_data + pos + 5, header + 4);
            
            if (header[0] == 0x08) {
                int h_sector = header[2];
                int h_track = header[3];
                
                pos += 10;
                
                /* Skip to data */
                while (pos < gcr_len && gcr_data[pos] != 0xFF) pos++;
                while (pos < gcr_len && gcr_data[pos] == 0xFF) pos++;
                
                if (pos < gcr_len - 325) {
                    /* Decode data */
                    uint8_t data_block[260];
                    for (int i = 0; i < 260 && pos < gcr_len; i += 4) {
                        decode_5_to_4(gcr_data + pos, data_block + i);
                        pos += 5;
                    }
                    
                    if (data_block[0] == 0x07 && h_track == track && 
                        h_sector >= 0 && h_sector < num_sectors) {
                        
                        memcpy(sector_data, data_block + 1, 256);
                        
                        /* Verify checksum */
                        uint8_t checksum = 0;
                        for (int i = 0; i < 256; i++) {
                            checksum ^= sector_data[i];
                        }
                        
                        d64_error_t error = D64_ERR_OK;
                        if (checksum != data_block[257]) {
                            error = D64_ERR_CHECKSUM;
                            errors_found++;
                        }
                        
                        d64_set_sector(img, track, h_sector, sector_data, error);
                        sectors_found++;
                        sectors_converted++;
                    }
                }
            } else {
                pos++;
            }
        }
        
        if (sectors_found > 0) {
            tracks_converted++;
        }
    }
    
    /* Try to extract disk ID from track 18 */
    if (g64->track_data[36] && g64->tracks[36].length > 0) {
        uint8_t bam[256];
        int track_num, sector_num;
        uint8_t id[2];
        
        if (gcr_to_sector(g64->track_data[36], g64->tracks[36].length,
                          bam, &track_num, &sector_num, id, NULL) == 0) {
            if (track_num == 18 && sector_num == 0) {
                img->disk_id[0] = bam[D64_BAM_ID_OFFSET];
                img->disk_id[1] = bam[D64_BAM_ID_OFFSET + 1];
                memcpy(img->disk_name, bam + D64_BAM_NAME_OFFSET, 16);
                img->disk_name[16] = '\0';
            }
        }
    }
    
    *d64 = img;
    
    if (result) {
        result->success = true;
        result->tracks_converted = tracks_converted;
        result->sectors_converted = sectors_converted;
        result->errors_found = errors_found;
        snprintf(result->description, sizeof(result->description),
                 "Converted %d tracks, %d sectors, %d errors",
                 tracks_converted, sectors_converted, errors_found);
    }
    
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get speed zone
 */
int d64_speed_zone(int track)
{
    if (track < 1 || track > 42) return 0;
    return speed_map[track];
}

/**
 * @brief Get gap length
 */
int d64_gap_length(int track)
{
    if (track < 1 || track > 42) return 0;
    return gap_map[track];
}

/**
 * @brief Get track capacity
 */
size_t d64_track_capacity(int track)
{
    if (track < 1 || track > 42) return 0;
    return capacity_map[speed_map[track]];
}

/**
 * @brief Get error name
 */
const char *d64_error_name(d64_error_t error)
{
    if (error <= D64_ERR_ID_MISMATCH) {
        return error_names[error];
    }
    return error_names[0];
}
