/**
 * @file uft_nibtools.c
 * @brief Nibtools Integration Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_nibtools.h"
#include "uft/hal/uft_xum1541.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Track lengths by zone (bytes) */
static const int ZONE_LENGTHS[] = { 7692, 7142, 6666, 6250 };

/** Sectors per zone */
static const int ZONE_SECTORS[] = { 21, 19, 18, 17 };

/*===========================================================================
 * Configuration
 *===========================================================================*/

struct uft_nib_config_s {
    /* XUM1541 connection */
    uft_xum_config_t *xum;
    bool connected;
    int device_num;
    
    /* Settings */
    int start_track;
    int end_track;
    bool half_tracks;
    int retries;
    uft_nib_mode_t mode;
    
    /* Captured tracks */
    uft_nib_track_t tracks[84];  /* Up to 42 tracks × 2 (half-tracks) */
    int track_count;
    
    /* Error */
    char last_error[256];
};

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_nib_config_t* uft_nib_config_create(void) {
    uft_nib_config_t *cfg = calloc(1, sizeof(uft_nib_config_t));
    if (!cfg) return NULL;
    
    cfg->device_num = 8;
    cfg->start_track = 1;
    cfg->end_track = 35;
    cfg->half_tracks = false;
    cfg->retries = 3;
    cfg->mode = UFT_NIB_MODE_READ;
    
    return cfg;
}

void uft_nib_config_destroy(uft_nib_config_t *cfg) {
    if (!cfg) return;
    
    /* Free track data */
    for (int i = 0; i < 84; i++) {
        free(cfg->tracks[i].gcr_data);
        free(cfg->tracks[i].weak_mask);
    }
    
    if (cfg->connected) {
        uft_nib_close(cfg);
    }
    
    if (cfg->xum) {
        uft_xum_config_destroy(cfg->xum);
    }
    
    free(cfg);
}

int uft_nib_open(uft_nib_config_t *cfg, int device_num) {
    if (!cfg) return -1;
    
    cfg->device_num = device_num;
    
    /* Create XUM1541 connection */
    cfg->xum = uft_xum_config_create();
    if (!cfg->xum) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to create XUM1541 config");
        return -1;
    }
    
    if (uft_xum_open(cfg->xum, device_num) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to connect: %s", uft_xum_get_error(cfg->xum));
        uft_xum_config_destroy(cfg->xum);
        cfg->xum = NULL;
        return -1;
    }
    
    cfg->connected = true;
    return 0;
}

void uft_nib_close(uft_nib_config_t *cfg) {
    if (!cfg || !cfg->connected) return;
    
    if (cfg->xum) {
        uft_xum_close(cfg->xum);
    }
    
    cfg->connected = false;
}

bool uft_nib_is_available(void) {
    /* Check if nibread/nibwrite are in PATH */
#ifdef _WIN32
    FILE *fp = popen("where nibread 2>nul", "r");
#else
    FILE *fp = popen("which nibread 2>/dev/null", "r");
#endif
    
    if (!fp) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    
    return found;
}

/*===========================================================================
 * Configuration
 *===========================================================================*/

int uft_nib_set_track_range(uft_nib_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 1 || start > 42) return -1;
    if (end < start || end > 42) return -1;
    
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_nib_set_half_tracks(uft_nib_config_t *cfg, bool enable) {
    if (!cfg) return -1;
    cfg->half_tracks = enable;
    return 0;
}

int uft_nib_set_retries(uft_nib_config_t *cfg, int count) {
    if (!cfg || count < 0 || count > 20) return -1;
    cfg->retries = count;
    return 0;
}

int uft_nib_set_mode(uft_nib_config_t *cfg, uft_nib_mode_t mode) {
    if (!cfg) return -1;
    cfg->mode = mode;
    return 0;
}

/*===========================================================================
 * Read Operations (via nibread CLI)
 *===========================================================================*/

int uft_nib_read_track(uft_nib_config_t *cfg, int track, int half,
                        uint8_t **gcr, size_t *size) {
    if (!cfg || !gcr || !size) return -1;
    
    (void)half;  /* Half-track support: would use track + 0.5 stepping */
    
    /* Build nibread command */
    char cmd[256];
    char temp_file[128];
    
#ifdef _WIN32
    snprintf(temp_file, sizeof(temp_file), "%%TEMP%%\\uft_nib_%d.raw", track);
#else
    snprintf(temp_file, sizeof(temp_file), "/tmp/uft_nib_%d.raw", track);
#endif
    
    snprintf(cmd, sizeof(cmd),
             "nibread -d%d -t%d -o %s 2>&1",
             cfg->device_num, track, temp_file);
    
    /* Execute */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to execute nibread");
        return -1;
    }
    
    /* Read output for errors */
    char output[1024];
    while (fgets(output, sizeof(output), fp)) {
        /* Check for error messages */
        if (strstr(output, "error") || strstr(output, "Error")) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "nibread: %s", output);
        }
    }
    
    int status = pclose(fp);
    if (status != 0) {
        return -1;
    }
    
    /* Read the raw file */
    fp = fopen(temp_file, "rb");
    if (!fp) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot read temp file: %s", temp_file);
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    *gcr = malloc(file_size);
    if (!*gcr) {
        fclose(fp);
        return -1;
    }
    
    *size = fread(*gcr, 1, file_size, fp);
    fclose(fp);
    
    /* Clean up temp file */
    remove(temp_file);
    
    return 0;
}

int uft_nib_read_disk(uft_nib_config_t *cfg, 
                       uft_nib_callback_t callback, void *user) {
    if (!cfg || !callback) return -1;
    
    int captured = 0;
    
    for (int track = cfg->start_track; track <= cfg->end_track; track++) {
        uft_nib_track_t result = {0};
        result.track = track;
        result.half_track = 0;
        result.density = (uint8_t)uft_nib_get_zone(track);
        
        int retries = cfg->retries;
        while (retries >= 0) {
            if (uft_nib_read_track(cfg, track, 0, 
                                   &result.gcr_data, &result.gcr_size) == 0) {
                break;
            }
            retries--;
        }
        
        if (result.gcr_data) {
            /* Analyze track */
            result.track_length = (uint32_t)result.gcr_size;
            
            /* Count syncs (0xFF patterns) */
            for (size_t i = 0; i < result.gcr_size; i++) {
                if (result.gcr_data[i] == 0xFF) {
                    result.sync_count++;
                }
            }
            
            captured++;
        }
        
        int cb_result = callback(&result, user);
        
        /* Store track */
        if (track < 84) {
            cfg->tracks[track - 1] = result;
            cfg->track_count++;
        } else {
            free(result.gcr_data);
        }
        
        if (cb_result != 0) break;  /* Abort */
    }
    
    return captured;
}

int uft_nib_analyze_track(uft_nib_config_t *cfg, int track,
                           uft_nib_track_t *result, int revolutions) {
    if (!cfg || !result || revolutions < 1) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->density = (uint8_t)uft_nib_get_zone(track);
    
    /* Read multiple revolutions */
    uint8_t *revs[10];
    size_t rev_sizes[10];
    int rev_count = (revolutions > 10) ? 10 : revolutions;
    
    for (int r = 0; r < rev_count; r++) {
        if (uft_nib_read_track(cfg, track, 0, &revs[r], &rev_sizes[r]) != 0) {
            /* Cleanup and fail */
            for (int i = 0; i < r; i++) free(revs[i]);
            return -1;
        }
    }
    
    /* Analyze for weak bits */
    if (rev_count >= 2 && rev_sizes[0] == rev_sizes[1]) {
        result->weak_mask = calloc(1, rev_sizes[0]);
        
        int weak_count = 0;
        for (size_t i = 0; i < rev_sizes[0]; i++) {
            for (int r = 1; r < rev_count; r++) {
                if (revs[0][i] != revs[r][i]) {
                    if (result->weak_mask) result->weak_mask[i] = 1;
                    weak_count++;
                    break;
                }
            }
        }
        
        result->weak_bits_detected = (weak_count > 0);
    }
    
    /* Use first revolution as main data */
    result->gcr_data = revs[0];
    result->gcr_size = rev_sizes[0];
    result->track_length = (uint32_t)rev_sizes[0];
    
    /* Free other revolutions */
    for (int r = 1; r < rev_count; r++) {
        free(revs[r]);
    }
    
    return 0;
}

/*===========================================================================
 * Write Operations (via nibwrite CLI)
 *===========================================================================*/

int uft_nib_write_track(uft_nib_config_t *cfg, int track, int half,
                         const uint8_t *gcr, size_t size) {
    if (!cfg || !gcr || size == 0) return -1;
    
    (void)half;
    
    /* Write to temp file */
    char temp_file[128];
#ifdef _WIN32
    snprintf(temp_file, sizeof(temp_file), "%%TEMP%%\\uft_nib_write_%d.raw", track);
#else
    snprintf(temp_file, sizeof(temp_file), "/tmp/uft_nib_write_%d.raw", track);
#endif
    
    FILE *fp = fopen(temp_file, "wb");
    if (!fp) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot create temp file");
        return -1;
    }
    
    fwrite(gcr, 1, size, fp);
    fclose(fp);
    
    /* Build nibwrite command */
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "nibwrite -d%d -t%d %s 2>&1",
             cfg->device_num, track, temp_file);
    
    /* Execute */
    fp = popen(cmd, "r");
    if (!fp) {
        remove(temp_file);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to execute nibwrite");
        return -1;
    }
    
    char output[1024];
    while (fgets(output, sizeof(output), fp)) {
        if (strstr(output, "error") || strstr(output, "Error")) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "nibwrite: %s", output);
        }
    }
    
    int status = pclose(fp);
    remove(temp_file);
    
    return (status == 0) ? 0 : -1;
}

int uft_nib_write_disk(uft_nib_config_t *cfg, const char *nib_path) {
    if (!cfg || !nib_path) return -1;
    
    /* Import NIB file first */
    if (uft_nib_import_nib(cfg, nib_path) != 0) {
        return -1;
    }
    
    /* Write all tracks */
    int written = 0;
    for (int t = 0; t < cfg->track_count && t < 84; t++) {
        if (cfg->tracks[t].gcr_data && cfg->tracks[t].gcr_size > 0) {
            if (uft_nib_write_track(cfg, cfg->tracks[t].track, 0,
                                     cfg->tracks[t].gcr_data,
                                     cfg->tracks[t].gcr_size) == 0) {
                written++;
            }
        }
    }
    
    return written;
}

int uft_nib_verify_track(uft_nib_config_t *cfg, int track, int half,
                          const uint8_t *gcr, size_t size,
                          int *mismatches) {
    if (!cfg || !gcr || !mismatches) return -1;
    
    uint8_t *read_gcr;
    size_t read_size;
    
    if (uft_nib_read_track(cfg, track, half, &read_gcr, &read_size) != 0) {
        return -1;
    }
    
    *mismatches = 0;
    size_t cmp_size = (size < read_size) ? size : read_size;
    
    for (size_t i = 0; i < cmp_size; i++) {
        if (gcr[i] != read_gcr[i]) {
            (*mismatches)++;
        }
    }
    
    free(read_gcr);
    return 0;
}

/*===========================================================================
 * Protection Detection
 *===========================================================================*/

int uft_nib_detect_protection(uft_nib_config_t *cfg,
                               uft_nib_protection_t *result) {
    if (!cfg || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Analyze captured tracks for protection signatures */
    for (int t = 0; t < cfg->track_count && t < 84; t++) {
        uft_nib_track_t *track = &cfg->tracks[t];
        if (!track->gcr_data || track->gcr_size == 0) continue;
        
        /* Check for fat track (extra data) */
        uft_nib_zone_t zone = uft_nib_get_zone(track->track);
        int expected_len = ZONE_LENGTHS[zone];
        if (track->gcr_size > (size_t)(expected_len * 1.1)) {
            result->has_fat_track = true;
        }
        
        /* Check for weak bits */
        if (track->weak_bits_detected) {
            result->detected = true;
            strncpy(result->protection_name, "Weak Bit Protection", 63);
        }
        
        /* Check for V-MAX! signature (specific sync patterns) */
        int consecutive_ff = 0;
        for (size_t i = 0; i < track->gcr_size; i++) {
            if (track->gcr_data[i] == 0xFF) {
                consecutive_ff++;
                if (consecutive_ff > 20) {
                    result->has_v_max = true;
                    result->detected = true;
                    strncpy(result->protection_name, "V-MAX!", 63);
                }
            } else {
                consecutive_ff = 0;
            }
        }
    }
    
    /* Check for half-tracks */
    if (cfg->half_tracks) {
        result->has_half_tracks = true;
        result->detected = true;
    }
    
    result->confidence = result->detected ? 0.8f : 0.0f;
    return 0;
}

int uft_nib_analyze_weak_bits(const uint8_t **revolutions, int count,
                               size_t size, uint8_t *weak_mask,
                               int *weak_count) {
    if (!revolutions || count < 2 || !weak_mask || !weak_count) return -1;
    
    *weak_count = 0;
    memset(weak_mask, 0, size);
    
    for (size_t i = 0; i < size; i++) {
        bool is_weak = false;
        for (int r = 1; r < count; r++) {
            if (revolutions[0][i] != revolutions[r][i]) {
                is_weak = true;
                break;
            }
        }
        
        if (is_weak) {
            weak_mask[i] = 1;
            (*weak_count)++;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Format Conversion
 *===========================================================================*/

int uft_nib_export_nib(uft_nib_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot create NIB file: %s", path);
        return -1;
    }
    
    /* Write NIB header */
    fwrite(UFT_NIB_MAGIC, 1, UFT_NIB_MAGIC_LEN, f);
    
    /* Write track count */
    uint8_t track_count = (uint8_t)cfg->track_count;
    fwrite(&track_count, 1, 1, f);
    
    /* Write tracks */
    for (int t = 0; t < cfg->track_count && t < 84; t++) {
        uft_nib_track_t *track = &cfg->tracks[t];
        if (!track->gcr_data) continue;
        
        /* Track header */
        uint8_t hdr[4];
        hdr[0] = (uint8_t)track->track;
        hdr[1] = track->density;
        hdr[2] = (track->gcr_size >> 8) & 0xFF;
        hdr[3] = track->gcr_size & 0xFF;
        fwrite(hdr, 1, 4, f);
        
        /* Track data */
        fwrite(track->gcr_data, 1, track->gcr_size, f);
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

int uft_nib_export_g64(uft_nib_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot create G64 file: %s", path);
        return -1;
    }
    
    /* G64 header */
    fwrite(UFT_G64_MAGIC, 1, UFT_G64_MAGIC_LEN, f);
    
    /* Version */
    uint8_t version = 0;
    fwrite(&version, 1, 1, f);
    
    /* Track count */
    uint8_t track_count = 84;  /* Always 84 for G64 */
    fwrite(&track_count, 1, 1, f);
    
    /* Max track size */
    uint16_t max_size = UFT_NIB_MAX_TRACK_SIZE;
    fwrite(&max_size, 2, 1, f);
    
    /* Track offset table (84 × 4 bytes) */
    uint32_t offsets[84] = {0};
    long offset_table_pos = ftell(f);
    fwrite(offsets, 4, 84, f);
    
    /* Speed zone table (84 × 4 bytes) */
    uint32_t speeds[84];
    for (int t = 0; t < 84; t++) {
        speeds[t] = (t < cfg->track_count) ? cfg->tracks[t].density : 3;
    }
    fwrite(speeds, 4, 84, f);
    
    /* Write track data and record offsets */
    for (int t = 0; t < cfg->track_count && t < 84; t++) {
        uft_nib_track_t *track = &cfg->tracks[t];
        if (!track->gcr_data || track->gcr_size == 0) continue;
        
        offsets[t] = (uint32_t)ftell(f);
        
        /* Track size (2 bytes) + data */
        uint16_t size = (uint16_t)track->gcr_size;
        fwrite(&size, 2, 1, f);
        fwrite(track->gcr_data, 1, track->gcr_size, f);
    }
    
    /* Go back and write offsets */
    if (fseek(f, offset_table_pos, SEEK_SET) != 0) return -1;
    fwrite(offsets, 4, 84, f);
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

/**
 * GCR-to-nibble decode table.
 * Commodore uses 4-bit-to-5-bit GCR encoding; this table reverses it.
 * Invalid GCR patterns map to 0xFF.
 */
static const uint8_t gcr_decode_table[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF,  /* 18-1F */
};

/**
 * Decode a 5-bit GCR nybble from the GCR stream.
 */
static int gcr_decode_byte(const uint8_t *gcr, size_t bit_offset) {
    /* Extract 10 GCR bits (two 5-bit groups) starting at bit_offset */
    size_t byte_pos = bit_offset / 8;
    int shift = (int)(bit_offset % 8);

    /* Assemble enough bits from the byte stream */
    uint32_t bits = ((uint32_t)gcr[byte_pos] << 16) |
                    ((uint32_t)gcr[byte_pos + 1] << 8) |
                    (uint32_t)gcr[byte_pos + 2];

    /* Shift to get 10 bits aligned at top */
    bits >>= (14 - shift);

    uint8_t hi_nybble = gcr_decode_table[(bits >> 5) & 0x1F];
    uint8_t lo_nybble = gcr_decode_table[bits & 0x1F];

    if (hi_nybble == 0xFF || lo_nybble == 0xFF) return -1;

    return (hi_nybble << 4) | lo_nybble;
}

/**
 * Find a SYNC mark (at least 10 consecutive 1-bits = 0xFF bytes) in GCR data.
 * Returns the byte offset after the sync, or -1 if not found.
 */
static int find_sync(const uint8_t *gcr, size_t size, size_t start) {
    int consecutive_ff = 0;

    for (size_t i = start; i < size; i++) {
        if (gcr[i] == 0xFF) {
            consecutive_ff++;
        } else {
            if (consecutive_ff >= 1) {
                return (int)i;  /* First non-FF byte after sync */
            }
            consecutive_ff = 0;
        }
    }

    return -1;
}

/**
 * Compute XOR checksum for a decoded sector data block (256 bytes).
 */
static uint8_t sector_checksum(const uint8_t *data, size_t len) {
    uint8_t cksum = 0;
    for (size_t i = 0; i < len; i++) {
        cksum ^= data[i];
    }
    return cksum;
}

int uft_nib_export_d64(uft_nib_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;

    /*
     * D64 export: Decode GCR track data into sectors and write standard
     * Commodore 1541 .d64 image.
     *
     * D64 format: 35 tracks, variable sectors per track (683 sectors total),
     * 256 bytes per sector, total 174848 bytes (no error map) or 175531 bytes
     * (with error map).
     *
     * Each GCR-encoded sector on disk consists of:
     *  - SYNC mark (>=10 bits of 1)
     *  - Sector header (8 GCR bytes = 5 decoded bytes):
     *      [0x08, checksum, sector, track, id2, id1, 0x0F, 0x0F]
     *  - Gap
     *  - SYNC mark
     *  - Data block (325 GCR bytes = 260 decoded bytes):
     *      [0x07, 256 data bytes, checksum, 0x00, 0x00]
     */

    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot create D64 file: %s", path);
        return -1;
    }

    /* D64 sector offset table: track 1 starts at offset 0, etc. */
    int total_sectors = 0;
    int sector_errors = 0;
    uint8_t error_map[683];
    memset(error_map, 0x01, sizeof(error_map));  /* Default: OK */

    for (int track_num = 1; track_num <= 35; track_num++) {
        int idx = track_num - 1;
        if (idx >= 84 || !cfg->tracks[idx].gcr_data || cfg->tracks[idx].gcr_size == 0) {
            /* Track not captured -- write blank sectors */
            int nsectors = uft_nib_sectors_for_zone(uft_nib_get_zone(track_num));
            uint8_t blank[256];
            memset(blank, 0, sizeof(blank));
            for (int s = 0; s < nsectors; s++) {
                fwrite(blank, 1, 256, f);
                error_map[total_sectors + s] = 0x05;  /* Missing sector */
                sector_errors++;
            }
            total_sectors += nsectors;
            continue;
        }

        const uint8_t *gcr = cfg->tracks[idx].gcr_data;
        size_t gcr_size = cfg->tracks[idx].gcr_size;
        int nsectors = uft_nib_sectors_for_zone(uft_nib_get_zone(track_num));

        /* Temporary storage for decoded sectors (indexed by sector number) */
        uint8_t decoded[21][256];
        bool found[21];
        memset(decoded, 0, sizeof(decoded));
        memset(found, 0, sizeof(found));

        /* Scan GCR stream for sector headers and data blocks */
        size_t pos = 0;
        int sectors_found = 0;

        while (pos < gcr_size && sectors_found < nsectors) {
            /* Find next sync mark */
            int sync_end = find_sync(gcr, gcr_size, pos);
            if (sync_end < 0) break;
            pos = (size_t)sync_end;

            /* Need at least 10 bytes for header GCR (8 GCR bytes = 5 decoded) */
            if (pos + 10 >= gcr_size) break;

            /* Try to decode sector header (first byte should decode to 0x08) */
            size_t bit_pos = pos * 8;
            int hdr_byte0 = gcr_decode_byte(gcr, bit_pos);

            if (hdr_byte0 == 0x08) {
                /* Sector header found */
                int hdr_cksum = gcr_decode_byte(gcr, bit_pos + 10);
                int hdr_sector = gcr_decode_byte(gcr, bit_pos + 20);
                int hdr_track = gcr_decode_byte(gcr, bit_pos + 30);

                if (hdr_sector < 0 || hdr_track < 0 || hdr_cksum < 0) {
                    pos += 5;
                    continue;
                }

                if (hdr_track != track_num || hdr_sector >= nsectors) {
                    pos += 5;
                    continue;
                }

                /* Skip header and gap, find data block sync */
                pos += 10;
                int data_sync = find_sync(gcr, gcr_size, pos);
                if (data_sync < 0) continue;
                pos = (size_t)data_sync;

                /* Data block: first byte should decode to 0x07 */
                if (pos + 325 > gcr_size) break;

                bit_pos = pos * 8;
                int data_marker = gcr_decode_byte(gcr, bit_pos);

                if (data_marker == 0x07) {
                    /* Decode 256 data bytes */
                    bool decode_ok = true;
                    for (int b = 0; b < 256; b++) {
                        int val = gcr_decode_byte(gcr, bit_pos + 10 + (size_t)b * 10);
                        if (val < 0) {
                            decode_ok = false;
                            decoded[hdr_sector][b] = 0;
                        } else {
                            decoded[hdr_sector][b] = (uint8_t)val;
                        }
                    }

                    /* Verify checksum */
                    int disk_cksum = gcr_decode_byte(gcr, bit_pos + 10 + 256 * 10);
                    uint8_t computed = sector_checksum(decoded[hdr_sector], 256);

                    if (decode_ok && disk_cksum >= 0 && (uint8_t)disk_cksum == computed) {
                        found[hdr_sector] = true;
                        sectors_found++;
                    } else if (decode_ok) {
                        /* CRC error but data decoded */
                        found[hdr_sector] = true;
                        sectors_found++;
                        error_map[total_sectors + hdr_sector] = 0x05;  /* CRC error */
                        sector_errors++;
                    }

                    pos += 325;
                } else {
                    pos += 5;
                }
            } else {
                pos++;
            }
        }

        /* Write sectors in order to D64 */
        for (int s = 0; s < nsectors; s++) {
            if (found[s]) {
                fwrite(decoded[s], 1, 256, f);
                if (error_map[total_sectors + s] == 0x01) {
                    error_map[total_sectors + s] = 0x01;  /* OK */
                }
            } else {
                uint8_t blank[256];
                memset(blank, 0, sizeof(blank));
                fwrite(blank, 1, 256, f);
                error_map[total_sectors + s] = 0x05;
                sector_errors++;
            }
        }

        total_sectors += nsectors;
    }

    if (ferror(f)) {
        fclose(f);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Write error creating D64 file");
        return -1;
    }

    fclose(f);

    if (sector_errors > 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "D64 export completed with %d sector errors", sector_errors);
    }

    return 0;
}

int uft_nib_import_nib(uft_nib_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot open NIB file: %s", path);
        return -1;
    }
    
    /* Read and verify magic */
    char magic[16];
    if (fread(magic, 1, UFT_NIB_MAGIC_LEN, f) != UFT_NIB_MAGIC_LEN) {
        fclose(f);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot read NIB header");
        return -1;
    }
    if (memcmp(magic, UFT_NIB_MAGIC, UFT_NIB_MAGIC_LEN) != 0) {
        fclose(f);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Invalid NIB file format");
        return -1;
    }
    
    /* Read track count */
    uint8_t track_count;
    if (fread(&track_count, 1, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Read tracks */
    for (int t = 0; t < track_count && t < 84; t++) {
        uint8_t hdr[4];
        if (fread(hdr, 1, 4, f) != 4) break;
        
        int track_num = hdr[0];
        int density = hdr[1];
        size_t size = ((size_t)hdr[2] << 8) | hdr[3];
        
        if (track_num > 0 && track_num <= 42) {
            int idx = track_num - 1;
            cfg->tracks[idx].track = track_num;
            cfg->tracks[idx].density = (uint8_t)density;
            cfg->tracks[idx].gcr_data = malloc(size);
            cfg->tracks[idx].gcr_size = size;
            
            if (cfg->tracks[idx].gcr_data) {
                if (fread(cfg->tracks[idx].gcr_data, 1, size, f) != size) {
                    free(cfg->tracks[idx].gcr_data);
                    cfg->tracks[idx].gcr_data = NULL;
                    cfg->tracks[idx].gcr_size = 0;
                    break;
                }
            }
            
            cfg->track_count++;
        }
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

int uft_nib_import_g64(uft_nib_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot open G64 file: %s", path);
        return -1;
    }
    
    /* Read and verify magic */
    char magic[16];
    if (fread(magic, 1, UFT_G64_MAGIC_LEN, f) != UFT_G64_MAGIC_LEN) {
        fclose(f);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot read G64 header");
        return -1;
    }
    if (memcmp(magic, UFT_G64_MAGIC, UFT_G64_MAGIC_LEN) != 0) {
        fclose(f);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Invalid G64 file format");
        return -1;
    }
    
    /* Skip version */
    if (fseek(f, 1, SEEK_CUR) != 0) return -1;
    
    /* Read track count */
    uint8_t track_count;
    if (fread(&track_count, 1, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Skip max size */
    if (fseek(f, 2, SEEK_CUR) != 0) return -1;
    
    /* Read offset table */
    uint32_t offsets[84];
    if (fread(offsets, 4, 84, f) != 84) {
        fclose(f);
        return -1;
    }
    
    /* Read speed table */
    uint32_t speeds[84];
    if (fread(speeds, 4, 84, f) != 84) {
        fclose(f);
        return -1;
    }
    
    /* Read tracks */
    for (int t = 0; t < track_count && t < 84; t++) {
        if (offsets[t] == 0) continue;
        
        if (fseek(f, offsets[t], SEEK_SET) != 0) continue;
        
        uint16_t size;
        if (fread(&size, 2, 1, f) != 1) break;
        
        int track_num = (t / 2) + 1;
        int idx = t;
        
        cfg->tracks[idx].track = track_num;
        cfg->tracks[idx].half_track = t % 2;
        cfg->tracks[idx].density = (uint8_t)(speeds[t] & 3);
        cfg->tracks[idx].gcr_data = malloc(size);
        cfg->tracks[idx].gcr_size = size;
        
        if (cfg->tracks[idx].gcr_data) {
            if (fread(cfg->tracks[idx].gcr_data, 1, size, f) != size) {
                free(cfg->tracks[idx].gcr_data);
                cfg->tracks[idx].gcr_data = NULL;
                cfg->tracks[idx].gcr_size = 0;
                break;
            }
        }
        
        cfg->track_count++;
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

uft_nib_zone_t uft_nib_get_zone(int track) {
    if (track >= 1 && track <= 17) return UFT_NIB_ZONE_1;
    if (track >= 18 && track <= 24) return UFT_NIB_ZONE_2;
    if (track >= 25 && track <= 30) return UFT_NIB_ZONE_3;
    return UFT_NIB_ZONE_4;
}

int uft_nib_sectors_for_zone(uft_nib_zone_t zone) {
    if (zone >= 0 && zone <= 3) return ZONE_SECTORS[zone];
    return 17;
}

int uft_nib_track_length(uft_nib_zone_t zone) {
    if (zone >= 0 && zone <= 3) return ZONE_LENGTHS[zone];
    return 6250;
}

const char* uft_nib_get_error(const uft_nib_config_t *cfg) {
    return cfg ? cfg->last_error : "NULL config";
}

void uft_nib_track_free(uft_nib_track_t *track) {
    if (!track) return;
    free(track->gcr_data);
    free(track->weak_mask);
    memset(track, 0, sizeof(*track));
}
