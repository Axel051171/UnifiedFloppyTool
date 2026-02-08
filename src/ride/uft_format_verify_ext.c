/**
 * @file uft_format_verify_ext.c
 * @brief Extended Format Verification Functions
 * 
 * Implements verification for:
 * - WOZ (Apple II preservation format)
 * - A2R (Applesauce Raw Disk Image)
 * - TD0 (Teledisk archive format)
 * 
 * @license MIT
 */

#include "uft/ride/uft_flux_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * WOZ FORMAT VERIFICATION
 *============================================================================*/

#define WOZ_MAGIC_V1        "WOZ1"
#define WOZ_MAGIC_V2        "WOZ2"
#define WOZ_MAGIC_LEN       4
#define WOZ_FF_BYTES        0xFF0A0D0A

/* WOZ chunk types */
#define WOZ_CHUNK_INFO      "INFO"
#define WOZ_CHUNK_TMAP      "TMAP"
#define WOZ_CHUNK_TRKS      "TRKS"
#define WOZ_CHUNK_FLUX      "FLUX"
#define WOZ_CHUNK_WRIT      "WRIT"
#define WOZ_CHUNK_META      "META"

/* WOZ INFO flags */
#define WOZ_INFO_DISK_525   1
#define WOZ_INFO_DISK_35    2

/**
 * @brief WOZ verification result
 */
typedef struct {
    bool     valid;                 /**< Overall validity */
    int      version;               /**< WOZ version (1 or 2) */
    uint32_t file_crc;              /**< File CRC-32 */
    uint32_t calc_crc;              /**< Calculated CRC-32 */
    bool     crc_valid;             /**< CRC matches */
    uint8_t  disk_type;             /**< Disk type (1=5.25", 2=3.5") */
    uint8_t  write_protected;       /**< Write protected flag */
    uint8_t  synchronized;          /**< Synchronized flag */
    uint8_t  cleaned;               /**< MC3470 fake bits cleaned */
    char     creator[33];           /**< Creator application */
    uint8_t  sides;                 /**< Number of sides */
    uint8_t  boot_sector_format;    /**< Boot sector format */
    uint8_t  tracks_present;        /**< Number of tracks with data */
    size_t   info_chunk_size;       /**< INFO chunk size */
    size_t   tmap_chunk_size;       /**< TMAP chunk size */
    size_t   trks_chunk_size;       /**< TRKS chunk size */
    bool     has_flux;              /**< Has FLUX chunk */
    bool     has_meta;              /**< Has META chunk */
    char     error_msg[128];        /**< Error message if invalid */
} uft_woz_verify_result_t;

/**
 * @brief CRC-32 table for WOZ verification
 */
static uint32_t woz_crc_table[256];
static bool woz_crc_table_init = false;

static void init_woz_crc_table(void) {
    if (woz_crc_table_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        woz_crc_table[i] = crc;
    }
    woz_crc_table_init = true;
}

static uint32_t woz_crc32(const uint8_t *data, size_t len) {
    init_woz_crc_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = woz_crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Verify WOZ file
 */
int uft_verify_woz(const char *path, uft_woz_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        strncpy(result->error_msg, "Cannot open file", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < 12) {
        fclose(f);
        strncpy(result->error_msg, "File too small", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Read header */
    uint8_t header[12];
    if (fread(header, 1, 12, f) != 12) {
        fclose(f);
        strncpy(result->error_msg, "Cannot read header", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Check magic */
    if (memcmp(header, WOZ_MAGIC_V1, 4) == 0) {
        result->version = 1;
    } else if (memcmp(header, WOZ_MAGIC_V2, 4) == 0) {
        result->version = 2;
    } else {
        fclose(f);
        strncpy(result->error_msg, "Invalid WOZ magic", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Check 0xFF 0x0A 0x0D 0x0A sequence */
    uint32_t ff_bytes = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);
    if (ff_bytes != WOZ_FF_BYTES) {
        fclose(f);
        strncpy(result->error_msg, "Invalid header sequence", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Get stored CRC */
    result->file_crc = header[8] | (header[9] << 8) | (header[10] << 16) | (header[11] << 24);
    
    /* Read rest of file for CRC calculation */
    uint8_t *data = malloc(file_size - 12);
    if (!data) {
        fclose(f);
        strncpy(result->error_msg, "Memory allocation failed", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    if (fread(data, 1, file_size - 12, f) != (size_t)(file_size - 12)) {
        free(data);
        fclose(f);
        strncpy(result->error_msg, "Cannot read file data", sizeof(result->error_msg) - 1);
        return -1;
    }
    fclose(f);
    
    /* Calculate CRC */
    result->calc_crc = woz_crc32(data, file_size - 12);
    result->crc_valid = (result->file_crc == result->calc_crc);
    
    /* Parse chunks */
    size_t pos = 0;
    while (pos + 8 <= (size_t)(file_size - 12)) {
        char chunk_id[5] = {0};
        memcpy(chunk_id, data + pos, 4);
        uint32_t chunk_size = data[pos + 4] | (data[pos + 5] << 8) | 
                              (data[pos + 6] << 16) | (data[pos + 7] << 24);
        
        if (pos + 8 + chunk_size > (size_t)(file_size - 12)) {
            break;
        }
        
        if (strcmp(chunk_id, WOZ_CHUNK_INFO) == 0) {
            result->info_chunk_size = chunk_size;
            if (chunk_size >= 60) {
                uint8_t *info = data + pos + 8;
                result->disk_type = info[1];
                result->write_protected = info[2];
                result->synchronized = info[3];
                result->cleaned = info[4];
                memcpy(result->creator, info + 5, 32);
                result->creator[32] = '\0';
                if (result->version == 2 && chunk_size >= 62) {
                    result->sides = info[37];
                    result->boot_sector_format = info[38];
                }
            }
        } else if (strcmp(chunk_id, WOZ_CHUNK_TMAP) == 0) {
            result->tmap_chunk_size = chunk_size;
            /* Count non-0xFF entries */
            uint8_t *tmap = data + pos + 8;
            for (size_t i = 0; i < chunk_size && i < 160; i++) {
                if (tmap[i] != 0xFF) {
                    result->tracks_present++;
                }
            }
        } else if (strcmp(chunk_id, WOZ_CHUNK_TRKS) == 0) {
            result->trks_chunk_size = chunk_size;
        } else if (strcmp(chunk_id, WOZ_CHUNK_FLUX) == 0) {
            result->has_flux = true;
        } else if (strcmp(chunk_id, WOZ_CHUNK_META) == 0) {
            result->has_meta = true;
        }
        
        pos += 8 + chunk_size;
    }
    
    free(data);
    
    /* Overall validity check */
    result->valid = result->crc_valid && 
                    result->info_chunk_size > 0 &&
                    result->tmap_chunk_size > 0 &&
                    result->trks_chunk_size > 0;
    
    if (!result->valid && result->error_msg[0] == '\0') {
        if (!result->crc_valid) {
            strncpy(result->error_msg, "CRC mismatch", sizeof(result->error_msg) - 1);
        } else {
            strncpy(result->error_msg, "Missing required chunks", sizeof(result->error_msg) - 1);
        }
    }
    
    return result->valid ? 0 : -1;
}

/*============================================================================
 * A2R FORMAT VERIFICATION
 *============================================================================*/

#define A2R_MAGIC           "A2R2"
#define A2R_MAGIC_V3        "A2R3"

/**
 * @brief A2R verification result
 */
typedef struct {
    bool     valid;                 /**< Overall validity */
    int      version;               /**< A2R version (2 or 3) */
    uint8_t  disk_type;             /**< Disk type */
    uint8_t  write_protected;       /**< Write protected */
    uint8_t  synchronized;          /**< Synchronized */
    char     creator[33];           /**< Creator string */
    uint8_t  capture_resolution;    /**< Capture resolution */
    uint32_t track_count;           /**< Number of tracks */
    bool     has_meta;              /**< Has META chunk */
    bool     has_rwcp;              /**< Has RWCP chunk */
    char     error_msg[128];        /**< Error message */
} uft_a2r_verify_result_t;

/**
 * @brief Verify A2R file
 */
int uft_verify_a2r(const char *path, uft_a2r_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        strncpy(result->error_msg, "Cannot open file", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Read header */
    uint8_t header[8];
    if (fread(header, 1, 8, f) != 8) {
        fclose(f);
        strncpy(result->error_msg, "Cannot read header", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Check magic */
    if (memcmp(header, A2R_MAGIC, 4) == 0) {
        result->version = 2;
    } else if (memcmp(header, A2R_MAGIC_V3, 4) == 0) {
        result->version = 3;
    } else {
        fclose(f);
        strncpy(result->error_msg, "Invalid A2R magic", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Check 0xFF 0x0A 0x0D 0x0A sequence */
    if (header[4] != 0xFF || header[5] != 0x0A || 
        header[6] != 0x0D || header[7] != 0x0A) {
        fclose(f);
        strncpy(result->error_msg, "Invalid header sequence", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Parse chunks */
    while (!feof(f)) {
        uint8_t chunk_header[8];
        if (fread(chunk_header, 1, 8, f) != 8) break;
        
        char chunk_id[5] = {0};
        memcpy(chunk_id, chunk_header, 4);
        uint32_t chunk_size = chunk_header[4] | (chunk_header[5] << 8) |
                              (chunk_header[6] << 16) | (chunk_header[7] << 24);
        
        if (strcmp(chunk_id, "INFO") == 0) {
            uint8_t info[64];
            size_t to_read = chunk_size < 64 ? chunk_size : 64;
            if (fread(info, 1, to_read, f) != to_read) break;
            
            result->disk_type = info[1];
            result->write_protected = info[2];
            result->synchronized = info[3];
            if (to_read > 5) {
                memcpy(result->creator, info + 5, to_read - 5 < 32 ? to_read - 5 : 32);
            }
            
            /* Skip rest of chunk */
            if (chunk_size > to_read) {
                fseek(f, chunk_size - to_read, SEEK_CUR);
            }
        } else if (strcmp(chunk_id, "STRM") == 0 || strcmp(chunk_id, "RWCP") == 0) {
            /* Count tracks */
            result->track_count++;
            result->has_rwcp = (strcmp(chunk_id, "RWCP") == 0);
            fseek(f, chunk_size, SEEK_CUR);
        } else if (strcmp(chunk_id, "META") == 0) {
            result->has_meta = true;
            fseek(f, chunk_size, SEEK_CUR);
        } else {
            /* Unknown chunk, skip */
            fseek(f, chunk_size, SEEK_CUR);
        }
    }
    
    fclose(f);
    
    result->valid = (result->track_count > 0);
    
    if (!result->valid) {
        strncpy(result->error_msg, "No track data found", sizeof(result->error_msg) - 1);
    }
    
    return result->valid ? 0 : -1;
}

/*============================================================================
 * TD0 FORMAT VERIFICATION
 *============================================================================*/

#define TD0_MAGIC           "TD"
#define TD0_MAGIC_ADV       "td"  /* Advanced compression */

/**
 * @brief TD0 verification result
 */
typedef struct {
    bool     valid;                 /**< Overall validity */
    bool     advanced_compression;  /**< Uses advanced compression */
    uint8_t  volume_sequence;       /**< Volume sequence (multi-volume) */
    uint8_t  check_signature;       /**< Check signature */
    uint8_t  td_version;            /**< Teledisk version */
    uint8_t  data_rate;             /**< Data rate */
    uint8_t  drive_type;            /**< Drive type */
    uint8_t  stepping;              /**< Stepping (single/double) */
    uint8_t  dos_allocation;        /**< DOS allocation flag */
    uint8_t  sides;                 /**< Number of sides */
    uint16_t crc;                   /**< Header CRC */
    uint32_t track_count;           /**< Number of tracks */
    uint32_t sector_count;          /**< Total sectors */
    char     comment[128];          /**< Comment (if present) */
    char     error_msg[128];        /**< Error message */
} uft_td0_verify_result_t;

/**
 * @brief Calculate TD0 CRC
 */
static uint16_t td0_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0xA097;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Verify TD0 file
 */
int uft_verify_td0(const char *path, uft_td0_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        strncpy(result->error_msg, "Cannot open file", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Read header (12 bytes minimum) */
    uint8_t header[12];
    if (fread(header, 1, 12, f) != 12) {
        fclose(f);
        strncpy(result->error_msg, "Cannot read header", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Check magic */
    if (header[0] == 'T' && header[1] == 'D') {
        result->advanced_compression = false;
    } else if (header[0] == 't' && header[1] == 'd') {
        result->advanced_compression = true;
    } else {
        fclose(f);
        strncpy(result->error_msg, "Invalid TD0 magic", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    result->volume_sequence = header[2];
    result->check_signature = header[3];
    result->td_version = header[4];
    result->data_rate = header[5];
    result->drive_type = header[6];
    result->stepping = header[7];
    result->dos_allocation = header[8];
    result->sides = header[9];
    result->crc = header[10] | (header[11] << 8);
    
    /* Verify header CRC */
    uint16_t calc_crc = td0_crc16(header, 10);
    if (calc_crc != result->crc) {
        fclose(f);
        strncpy(result->error_msg, "Header CRC mismatch", sizeof(result->error_msg) - 1);
        return -1;
    }
    
    /* Check for comment block */
    if (header[7] & 0x80) {
        uint8_t comment_header[10];
        if (fread(comment_header, 1, 10, f) == 10) {
            uint16_t comment_len = comment_header[2] | (comment_header[3] << 8);
            if (comment_len > 0 && comment_len < sizeof(result->comment)) {
                fread(result->comment, 1, comment_len, f);
                result->comment[comment_len] = '\0';
            } else if (comment_len > 0) {
                fseek(f, comment_len, SEEK_CUR);
            }
        }
    }
    
    /* Count tracks (basic scan - doesn't decompress) */
    if (!result->advanced_compression) {
        while (!feof(f)) {
            uint8_t track_header[4];
            if (fread(track_header, 1, 4, f) != 4) break;
            
            uint8_t sector_count = track_header[0];
            if (sector_count == 0xFF) {
                /* End of tracks */
                break;
            }
            
            result->track_count++;
            result->sector_count += sector_count;
            
            /* Skip sector data */
            for (int s = 0; s < sector_count; s++) {
                uint8_t sector_header[6];
                if (fread(sector_header, 1, 6, f) != 6) break;
                
                uint8_t flags = sector_header[4];
                if (!(flags & 0x30)) {  /* Has data */
                    uint16_t data_size = sector_header[5] | (sector_header[6] << 8);
                    fseek(f, data_size - 1, SEEK_CUR);
                }
            }
        }
    } else {
        /* For compressed files, we'd need to decompress */
        /* Just mark as valid if header passed */
        result->track_count = 1;  /* Unknown */
    }
    
    fclose(f);
    
    result->valid = (result->track_count > 0 || result->advanced_compression);
    
    if (!result->valid) {
        strncpy(result->error_msg, "No valid track data", sizeof(result->error_msg) - 1);
    }
    
    return result->valid ? 0 : -1;
}

/*============================================================================
 * UNIFIED VERIFICATION API
 *============================================================================*/

/**
 * @brief Verify disk image file (auto-detect format)
 */
int uft_verify_image(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Try each format */
    uft_woz_verify_result_t woz;
    if (uft_verify_woz(path, &woz) == 0) {
        result->format_name = "WOZ";
        result->valid = woz.valid;
        snprintf(result->details, sizeof(result->details),
                 "WOZ v%d, %d tracks, CRC %s",
                 woz.version, woz.tracks_present,
                 woz.crc_valid ? "OK" : "FAIL");
        return 0;
    }
    
    uft_a2r_verify_result_t a2r;
    if (uft_verify_a2r(path, &a2r) == 0) {
        result->format_name = "A2R";
        result->valid = a2r.valid;
        snprintf(result->details, sizeof(result->details),
                 "A2R v%d, %d tracks",
                 a2r.version, a2r.track_count);
        return 0;
    }
    
    uft_td0_verify_result_t td0;
    if (uft_verify_td0(path, &td0) == 0) {
        result->format_name = "TD0";
        result->valid = td0.valid;
        snprintf(result->details, sizeof(result->details),
                 "TD0 v%d.%d, %d tracks, %d sectors%s",
                 td0.td_version >> 4, td0.td_version & 0x0F,
                 td0.track_count, td0.sector_count,
                 td0.advanced_compression ? " (compressed)" : "");
        return 0;
    }
    
    result->format_name = "Unknown";
    result->valid = false;
    result->error_code = -1;
    strncpy(result->details, "Unrecognized format", sizeof(result->details) - 1);
    
    return -1;
}
