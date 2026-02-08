/**
 * @file uft_nib.c
 * @brief Apple NIB format implementation
 * 
 * P1-008: NIB format read/write support
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_nib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * GCR Tables
 * ============================================================================ */

/* Apple 6&2 GCR encoding table */
const uint8_t apple_gcr_encode_62[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* GCR decode table (inverse of encode) */
const uint8_t apple_gcr_decode_62[256] = {
    /* 0x00-0x0F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x10-0x1F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x20-0x2F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x30-0x3F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x40-0x4F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x50-0x5F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x60-0x6F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x70-0x7F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x80-0x8F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x90-0x9F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
                    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    /* 0xA0-0xAF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,
                    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    /* 0xB0-0xBF */ 0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
                    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    /* 0xC0-0xCF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    /* 0xD0-0xDF */ 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,
                    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    /* 0xE0-0xEF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,
                    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    /* 0xF0-0xFF */ 0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

/* DOS 3.3 sector interleave (physical -> logical) */
static const uint8_t dos33_interleave[16] = {
    0x00, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04,
    0x0B, 0x03, 0x0A, 0x02, 0x09, 0x01, 0x08, 0x0F
};

/* ProDOS sector interleave */
static const uint8_t prodos_interleave[16] = {
    0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x0B,
    0x04, 0x0C, 0x05, 0x0D, 0x06, 0x0E, 0x07, 0x0F
};

/* ============================================================================
 * GCR Functions
 * ============================================================================ */

uint8_t apple_gcr_encode(uint8_t value) {
    if (value >= 64) return 0xFF;
    return apple_gcr_encode_62[value];
}

uint8_t apple_gcr_decode(uint8_t gcr) {
    return apple_gcr_decode_62[gcr];
}

bool apple_gcr_valid(uint8_t gcr) {
    return apple_gcr_decode_62[gcr] != 0xFF;
}

/* ============================================================================
 * 4-and-4 Encoding (for address fields)
 * ============================================================================ */

static void encode_44(uint8_t value, uint8_t *out) {
    out[0] = ((value >> 1) & 0x55) | 0xAA;
    out[1] = (value & 0x55) | 0xAA;
}

static uint8_t decode_44(const uint8_t *in) {
    return ((in[0] & 0x55) << 1) | (in[1] & 0x55);
}

/* ============================================================================
 * Sector Encoding (6&2)
 * ============================================================================ */

void nib_encode_sector_data(const uint8_t *data, uint8_t *gcr) {
    /* 
     * 6&2 encoding: 256 bytes -> 342 bytes
     * First pass: extract 2-bit remainders (86 bytes)
     * Second pass: encode 6-bit values (256 bytes)
     */
    
    uint8_t buffer[342];
    memset(buffer, 0, sizeof(buffer));
    
    /* First 86 bytes: 2-bit values from each triplet */
    for (int i = 0; i < 86; i++) {
        int idx = i;
        uint8_t val = 0;
        
        /* Collect 2-bit pieces from 3 source bytes */
        if (idx < 86) {
            val = (data[idx] & 0x03) << 0;
        }
        if (idx + 86 < 256) {
            val |= (data[idx + 86] & 0x03) << 2;
        }
        if (idx + 172 < 256) {
            val |= (data[idx + 172] & 0x03) << 4;
        }
        
        buffer[i] = val;
    }
    
    /* Next 256 bytes: 6-bit values */
    for (int i = 0; i < 256; i++) {
        buffer[86 + i] = data[i] >> 2;
    }
    
    /* XOR checksum encoding */
    uint8_t prev = 0;
    for (int i = 341; i >= 0; i--) {
        uint8_t curr = buffer[i];
        buffer[i] = curr ^ prev;
        prev = curr;
    }
    
    /* GCR encode all 342 bytes */
    for (int i = 0; i < 342; i++) {
        gcr[i] = apple_gcr_encode(buffer[i]);
    }
}

int nib_decode_sector_data(const uint8_t *gcr, uint8_t *data) {
    uint8_t buffer[342];
    
    /* GCR decode */
    for (int i = 0; i < 342; i++) {
        uint8_t decoded = apple_gcr_decode(gcr[i]);
        if (decoded == 0xFF) {
            return -1;  /* Invalid GCR */
        }
        buffer[i] = decoded;
    }
    
    /* XOR decode */
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        buffer[i] ^= prev;
        prev = buffer[i];
    }
    
    /* Verify checksum (last byte should be 0) */
    if (buffer[341] != 0) {
        return -1;  /* Checksum error */
    }
    
    /* Reconstruct 256 bytes */
    for (int i = 0; i < 256; i++) {
        uint8_t hi = buffer[86 + i];
        uint8_t lo = 0;
        
        int idx = i % 86;
        int shift = (i / 86) * 2;
        
        lo = (buffer[idx] >> shift) & 0x03;
        
        data[i] = (hi << 2) | lo;
    }
    
    return 0;
}

/* ============================================================================
 * Sector Building
 * ============================================================================ */

size_t nib_build_sector(uint8_t volume, uint8_t track, uint8_t sector,
                        const uint8_t *data, uint8_t *output) {
    size_t pos = 0;
    
    /* Self-sync bytes (gap before address) */
    for (int i = 0; i < 5; i++) {
        output[pos++] = APPLE_SYNC_BYTE;
    }
    
    /* Address prologue */
    output[pos++] = APPLE_ADDR_PROLOGUE_1;
    output[pos++] = APPLE_ADDR_PROLOGUE_2;
    output[pos++] = APPLE_ADDR_PROLOGUE_3;
    
    /* Address field (4-and-4 encoded) */
    uint8_t addr_44[8];
    encode_44(volume, &addr_44[0]);
    encode_44(track, &addr_44[2]);
    encode_44(sector, &addr_44[4]);
    encode_44(volume ^ track ^ sector, &addr_44[6]);  /* Checksum */
    
    for (int i = 0; i < 8; i++) {
        output[pos++] = addr_44[i];
    }
    
    /* Address epilogue */
    output[pos++] = APPLE_EPILOGUE_1;
    output[pos++] = APPLE_EPILOGUE_2;
    output[pos++] = APPLE_EPILOGUE_3;
    
    /* Gap between address and data */
    for (int i = 0; i < 5; i++) {
        output[pos++] = APPLE_SYNC_BYTE;
    }
    
    /* Data prologue */
    output[pos++] = APPLE_DATA_PROLOGUE_1;
    output[pos++] = APPLE_DATA_PROLOGUE_2;
    output[pos++] = APPLE_DATA_PROLOGUE_3;
    
    /* Encoded sector data (342 bytes) */
    uint8_t gcr_data[342];
    nib_encode_sector_data(data, gcr_data);
    
    for (int i = 0; i < 342; i++) {
        output[pos++] = gcr_data[i];
    }
    
    /* Data checksum (already included in encoding) */
    output[pos++] = apple_gcr_encode(0);  /* Checksum byte */
    
    /* Data epilogue */
    output[pos++] = APPLE_EPILOGUE_1;
    output[pos++] = APPLE_EPILOGUE_2;
    output[pos++] = APPLE_EPILOGUE_3;
    
    return pos;
}

/* ============================================================================
 * File I/O
 * ============================================================================ */

void uft_nib_write_options_init(nib_write_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->tracks = 35;
    opts->volume = 254;
    opts->sync_align = true;
    opts->gap_size = 14;
}

bool uft_nib_validate(const uint8_t *data, size_t size) {
    if (!data) return false;
    
    /* Check file size */
    if (size != NIB_FILE_SIZE_35 && size != NIB_FILE_SIZE_40) {
        return false;
    }
    
    /* Check for valid GCR data (sample first track) */
    int valid_count = 0;
    for (size_t i = 0; i < NIB_TRACK_SIZE && i < size; i++) {
        if (apple_gcr_valid(data[i]) || data[i] == APPLE_SYNC_BYTE) {
            valid_count++;
        }
    }
    
    /* At least 80% should be valid */
    return (valid_count * 100 / NIB_TRACK_SIZE) >= 80;
}

uft_error_t uft_nib_read(const char *path,
                         uft_disk_image_t **out_disk,
                         nib_read_result_t *result) {
    if (!path || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (result) {
            result->error = UFT_ERR_IO;
            result->error_detail = "Failed to open file";
        }
        return UFT_ERR_IO;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    long file_size_l = ftell(fp);
    if (file_size_l < 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    size_t file_size = (size_t)file_size_l;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, file_size, fp) != file_size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    uft_error_t err = uft_nib_read_mem(data, file_size, out_disk, result);
    free(data);
    
    return err;
}

uft_error_t uft_nib_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             nib_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (result) memset(result, 0, sizeof(*result));
    
    if (!uft_nib_validate(data, size)) {
        if (result) {
            result->error = UFT_ERR_CORRUPT;
            result->error_detail = "Invalid NIB file";
        }
        return UFT_ERR_CORRUPT;
    }
    
    uint8_t num_tracks = (size == NIB_FILE_SIZE_40) ? 40 : 35;
    
    if (result) {
        result->tracks = num_tracks;
        result->file_size = size;
        result->format = 1;  /* Assume DOS 3.3 */
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(num_tracks, 1);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_NIB;
    snprintf(disk->format_name, sizeof(disk->format_name), "NIB");
    disk->sectors_per_track = 16;
    disk->bytes_per_sector = 256;
    
    /* Process each track */
    for (uint8_t t = 0; t < num_tracks; t++) {
        const uint8_t *track_data = data + (t * NIB_TRACK_SIZE);
        
        uft_track_t *track = uft_track_alloc(16, NIB_TRACK_SIZE * 8);
        if (!track) {
            uft_disk_free(disk);
            return UFT_ERR_MEMORY;
        }
        
        track->track_num = t;
        track->head = 0;
        track->encoding = UFT_ENC_GCR_APPLE;
        
        /* Copy raw track data */
        memcpy(track->raw_data, track_data, NIB_TRACK_SIZE);
        track->raw_bits = NIB_TRACK_SIZE * 8;
        
        /* Decode sectors */
        /* (simplified - full implementation would search for sectors) */
        
        if (result) {
            result->track_info[t].valid = true;
        }
        
        disk->track_data[t] = track;
    }
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_nib_write(const uft_disk_image_t *disk,
                          const char *path,
                          const nib_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    nib_write_options_t default_opts;
    if (!opts) {
        uft_nib_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    uint8_t num_tracks = opts->tracks;
    if (num_tracks != 35 && num_tracks != 40) {
        num_tracks = 35;
    }
    
    size_t file_size = num_tracks * NIB_TRACK_SIZE;
    uint8_t *nib_data = calloc(1, file_size);
    if (!nib_data) {
        return UFT_ERR_MEMORY;
    }
    
    /* Fill with sync bytes */
    memset(nib_data, APPLE_SYNC_BYTE, file_size);
    
    /* Process each track */
    for (uint8_t t = 0; t < num_tracks && t < disk->tracks; t++) {
        uint8_t *track_out = nib_data + (t * NIB_TRACK_SIZE);
        uft_track_t *track = (t < disk->track_count) ? disk->track_data[t] : NULL;
        
        if (!track) continue;
        
        /* If track has raw data, use it directly */
        if (track->raw_data && track->raw_bits >= NIB_TRACK_SIZE * 8) {
            memcpy(track_out, track->raw_data, NIB_TRACK_SIZE);
            continue;
        }
        
        /* Otherwise, encode from sectors */
        size_t pos = 0;
        
        /* Initial sync */
        for (int i = 0; i < 48 && pos < NIB_TRACK_SIZE; i++) {
            track_out[pos++] = APPLE_SYNC_BYTE;
        }
        
        /* Encode sectors */
        for (uint8_t s = 0; s < 16 && s < track->sector_count; s++) {
            /* Map logical to physical sector (DOS 3.3 interleave) */
            uint8_t phys_sector = dos33_interleave[s];
            
            uft_sector_t *sector = NULL;
            for (size_t i = 0; i < track->sector_count; i++) {
                if (track->sectors[i].id.sector == phys_sector) {
                    sector = &track->sectors[i];
                    break;
                }
            }
            
            uint8_t sector_data[256];
            if (sector && sector->data) {
                memcpy(sector_data, sector->data, 256);
            } else {
                memset(sector_data, 0, 256);
            }
            
            /* Build sector */
            uint8_t sector_buf[400];
            size_t sector_len = nib_build_sector(opts->volume, t, s,
                                                  sector_data, sector_buf);
            
            /* Copy to track (with wrap) */
            for (size_t i = 0; i < sector_len && pos < NIB_TRACK_SIZE; i++) {
                track_out[pos++] = sector_buf[i];
            }
            
            /* Gap */
            for (int i = 0; i < opts->gap_size && pos < NIB_TRACK_SIZE; i++) {
                track_out[pos++] = APPLE_SYNC_BYTE;
            }
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(nib_data);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(nib_data, 1, file_size, fp);
    fclose(fp);
    free(nib_data);
    
    if (written != file_size) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

uft_error_t uft_nib_from_dsk(const uint8_t *dsk_data, size_t dsk_size,
                             uint8_t *nib_data, size_t *nib_size,
                             uint8_t volume) {
    if (!dsk_data || !nib_data || !nib_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* DSK is 140K = 35 tracks * 16 sectors * 256 bytes */
    if (dsk_size != 143360) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t required_size = NIB_FILE_SIZE_35;
    if (*nib_size < required_size) {
        *nib_size = required_size;
        return UFT_ERR_MEMORY;
    }
    
    /* Fill with sync */
    memset(nib_data, APPLE_SYNC_BYTE, required_size);
    
    /* Convert each track */
    for (int t = 0; t < 35; t++) {
        uint8_t *track_out = nib_data + (t * NIB_TRACK_SIZE);
        size_t pos = 48;  /* Initial gap */
        
        for (int s = 0; s < 16; s++) {
            /* DOS 3.3 interleave */
            int logical = dos33_interleave[s];
            const uint8_t *sector_data = dsk_data + 
                (t * 16 + logical) * 256;
            
            uint8_t sector_buf[400];
            size_t sector_len = nib_build_sector(volume, t, s,
                                                  sector_data, sector_buf);
            
            for (size_t i = 0; i < sector_len && pos < NIB_TRACK_SIZE; i++) {
                track_out[pos++] = sector_buf[i];
            }
            
            /* Gap */
            for (int i = 0; i < 14 && pos < NIB_TRACK_SIZE; i++) {
                track_out[pos++] = APPLE_SYNC_BYTE;
            }
        }
    }
    
    *nib_size = required_size;
    return UFT_OK;
}
