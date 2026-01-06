/**
 * @file uft_ipf_parser.c
 * @brief IPF (Interchangeable Preservation Format) Parser Implementation
 * 
 * Implements parsing of IPF format files from Software Preservation Society.
 * 
 * IPF Format Structure:
 * - Header: "CAPS" signature (4 bytes)
 * - Records: Type (4 bytes) + Length (4 bytes) + CRC (4 bytes) + Data
 * 
 * Record Types:
 * - CAPS: File header with version
 * - INFO: Image metadata
 * - IMGE: Image structure
 * - DATA: Track data
 * - TRCK: Track info (v2)
 * 
 * @author UFT Team
 * @version 3.4.0
 */

#include "uft/parsers/uft_ipf_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

/** Record header (12 bytes) */
typedef struct {
    uint32_t    type;           /**< Record type */
    uint32_t    length;         /**< Data length */
    uint32_t    crc;            /**< CRC-32 of data */
} record_header_t;

/** CAPS record (v1: 12 bytes header + 12 bytes data) */
typedef struct {
    uint32_t    encoder;        /**< Encoder type */
    uint32_t    version;        /**< Format version */
    uint32_t    reserved;       /**< Reserved */
} caps_record_t;

/** INFO record layout */
typedef struct {
    uint32_t    media_type;
    uint32_t    encoder_type;
    uint32_t    encoder_rev;
    uint32_t    file_key;
    uint32_t    file_rev;
    uint32_t    origin;
    uint32_t    min_cylinder;
    uint32_t    max_cylinder;
    uint32_t    min_head;
    uint32_t    max_head;
    uint32_t    creation_date;
    uint32_t    creation_time;
    uint32_t    platforms[4];
    uint32_t    disk_number;
    uint32_t    creator_id;
    uint32_t    reserved[2];
} info_record_t;

/** IMGE record (image block) */
typedef struct {
    uint32_t    cylinder;
    uint32_t    head;
    uint32_t    density_type;
    uint32_t    signal_type;
    uint32_t    track_bytes;
    uint32_t    start_byte_pos;
    uint32_t    start_bit_pos;
    uint32_t    data_bits;
    uint32_t    gap_bits;
    uint32_t    track_bits;
    uint32_t    block_count;
    uint32_t    encoder_process;
    uint32_t    flags;
    uint32_t    data_key;
    uint32_t    reserved[3];
} imge_record_t;

/** DATA record header */
typedef struct {
    uint32_t    size;
    uint32_t    bits;
    uint32_t    crc;
    uint32_t    key;
} data_header_t;

/*============================================================================
 * Error Strings
 *============================================================================*/

static const char *error_strings[IPF_ERR_COUNT] = {
    "OK",
    "Null parameter",
    "Cannot open file",
    "File read error",
    "Invalid IPF signature",
    "Unsupported IPF version",
    "Invalid record",
    "Missing INFO record",
    "No track data",
    "Track out of range",
    "Sector out of range",
    "CRC error",
    "Memory allocation failed",
    "Decode error",
    "Corrupt data"
};

static const char *platform_strings[] = {
    "Unknown",
    "Amiga",
    "Atari ST",
    "PC",
    "Amstrad CPC",
    "ZX Spectrum",
    "SAM Coupé",
    "Archimedes",
    "Commodore 64",
    "Atari 8-bit"
};

static const char *density_strings[] = {
    "Auto",
    "Noise",
    "Double Density",
    "High Density",
    "Extra Density"
};

/*============================================================================
 * CRC-32 Implementation
 *============================================================================*/

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void) {
    if (crc32_table_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = true;
}

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

/*============================================================================
 * CRC-16 CCITT (for sector CRC)
 *============================================================================*/

uint16_t ipf_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    return crc;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

/** Read big-endian uint32_t (IPF uses BE) */
static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | 
           ((uint32_t)p[1] << 16) | 
           ((uint32_t)p[2] << 8) | 
           (uint32_t)p[3];
}

/** Read little-endian uint32_t (some fields) */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | 
           ((uint32_t)p[3] << 24);
}

/** Safe string copy */
static void safe_strcpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/*============================================================================
 * Record Parsing
 *============================================================================*/

/** Parse INFO record */
static ipf_error_t parse_info_record(const uint8_t *data, size_t size,
                                     ipf_info_t *info) {
    if (!data || !info || size < 96) return IPF_ERR_BAD_RECORD;
    
    memset(info, 0, sizeof(*info));
    
    info->media_type = read_be32(data + 0);
    info->encoder_type = read_be32(data + 4);
    info->encoder_rev = read_be32(data + 8);
    info->file_key = read_be32(data + 12);
    info->file_rev = read_be32(data + 16);
    info->origin = read_be32(data + 20);
    info->min_cylinder = read_be32(data + 24);
    info->max_cylinder = read_be32(data + 28);
    info->min_head = read_be32(data + 32);
    info->max_head = read_be32(data + 36);
    
    /* Date: stored as days since some epoch */
    uint32_t date = read_be32(data + 40);
    snprintf(info->date, sizeof(info->date), "%u", date);
    
    info->platform = read_be32(data + 48);
    
    /* Determine density from media type */
    if (info->media_type == 1) info->density = IPF_DENSITY_DD;
    else if (info->media_type == 2) info->density = IPF_DENSITY_HD;
    else info->density = IPF_DENSITY_AUTO;
    
    return IPF_OK;
}

/** Parse IMGE record and create track entry */
static ipf_error_t parse_imge_record(ipf_context_t *ctx,
                                     const uint8_t *data, size_t size) {
    if (!ctx || !data || size < sizeof(imge_record_t)) {
        return IPF_ERR_BAD_RECORD;
    }
    
    uint32_t cylinder = read_be32(data + 0);
    uint32_t head = read_be32(data + 4);
    uint32_t density = read_be32(data + 8);
    uint32_t track_bytes = read_be32(data + 16);
    uint32_t track_bits = read_be32(data + 36);
    uint32_t block_count = read_be32(data + 40);
    uint32_t flags = read_be32(data + 48);
    uint32_t data_key = read_be32(data + 52);
    
    /* Find or create track slot */
    uint8_t track_idx = cylinder * 2 + head;
    if (track_idx >= ctx->track_count) {
        /* Reallocate tracks array */
        uint8_t new_count = track_idx + 1;
        ipf_track_t *new_tracks = realloc(ctx->tracks, 
                                          new_count * sizeof(ipf_track_t));
        if (!new_tracks) return IPF_ERR_ALLOC;
        
        /* Zero new entries */
        memset(&new_tracks[ctx->track_count], 0,
               (new_count - ctx->track_count) * sizeof(ipf_track_t));
        
        ctx->tracks = new_tracks;
        ctx->track_count = new_count;
    }
    
    ipf_track_t *track = &ctx->tracks[track_idx];
    track->cylinder = (uint8_t)cylinder;
    track->head = (uint8_t)head;
    track->track_bits = track_bits;
    track->flags = flags;
    
    /* Calculate duration based on density */
    double bit_time_ns;
    if (density == IPF_DENSITY_HD) {
        bit_time_ns = 1000.0;  /* 1µs for HD */
    } else {
        bit_time_ns = 2000.0;  /* 2µs for DD */
    }
    track->duration_us = (track_bits * bit_time_ns) / 1000.0;
    track->rpm = (track->duration_us > 0) ? 
                 (60000000.0 / track->duration_us) : 0;
    
    /* Protection detection */
    if (flags & IPF_FLAG_FUZZY) {
        track->has_fuzzy = true;
        ctx->fuzzy_tracks++;
    }
    if (flags & IPF_FLAG_WEAK) {
        track->has_weak = true;
    }
    
    return IPF_OK;
}

/** Parse DATA record and attach to track */
static ipf_error_t parse_data_record(ipf_context_t *ctx,
                                     const uint8_t *data, size_t size,
                                     uint32_t key) {
    if (!ctx || !data) return IPF_ERR_BAD_RECORD;
    
    /* Find track with matching key */
    /* For now, just store raw data in the appropriate track */
    
    /* DATA record structure:
     * - Header (16 bytes): size, bits, crc, key
     * - Data blocks
     */
    
    if (size < 16) return IPF_ERR_BAD_RECORD;
    
    uint32_t data_size = read_be32(data + 0);
    uint32_t data_bits = read_be32(data + 4);
    uint32_t data_crc = read_be32(data + 8);
    uint32_t data_key = read_be32(data + 12);
    
    const uint8_t *raw = data + 16;
    size_t raw_len = size - 16;
    
    /* Find track by iterating (key matching is complex in IPF) */
    for (uint8_t i = 0; i < ctx->track_count; i++) {
        ipf_track_t *track = &ctx->tracks[i];
        
        /* If track has no data yet, assign this data */
        if (track->raw_data == NULL && raw_len > 0) {
            track->raw_data = malloc(raw_len);
            if (track->raw_data) {
                memcpy(track->raw_data, raw, raw_len);
                track->raw_data_len = (uint32_t)raw_len;
                ctx->total_data_bytes += raw_len;
            }
            break;  /* Only assign to first empty track */
        }
    }
    
    return IPF_OK;
}

/** Decode sectors from track data */
static void decode_track_sectors(ipf_track_t *track, uint32_t platform) {
    if (!track || !track->raw_data || track->raw_data_len == 0) return;
    
    /* Platform-specific MFM decoding */
    track->sector_count = 0;
    
    const uint8_t *data = track->raw_data;
    uint32_t len = track->raw_data_len;
    
    /* Simple sector detection: look for address marks */
    for (uint32_t i = 0; i + 10 < len; i++) {
        /* MFM Address Mark: A1 A1 A1 FE (IDAM) or A1 A1 A1 FB (DAM) */
        if (data[i] == 0xA1 && data[i+1] == 0xA1 && 
            data[i+2] == 0xA1 && data[i+3] == 0xFE) {
            
            if (track->sector_count >= IPF_MAX_SECTORS) break;
            
            ipf_sector_t *sector = &track->sectors[track->sector_count];
            sector->cylinder = data[i+4];
            sector->head = data[i+5];
            sector->sector = data[i+6];
            sector->size_code = data[i+7];
            sector->data_size = ipf_sector_size(sector->size_code);
            sector->data_offset = i + 4;  /* Offset to CHRN */
            
            /* CRC follows CHRN */
            if (i + 10 < len) {
                sector->header_crc = (data[i+8] << 8) | data[i+9];
            }
            
            track->sector_count++;
        }
    }
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

ipf_context_t *ipf_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size < 24) {  /* Minimum: header + one record */
        fclose(f);
        return NULL;
    }
    
    /* Read entire file */
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(file_data, 1, file_size, f) != (size_t)file_size) {
        free(file_data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    /* Check first record is CAPS */
    if (memcmp(file_data, "CAPS", 4) != 0) {
        free(file_data);
        return NULL;
    }
    
    /* Allocate context */
    ipf_context_t *ctx = calloc(1, sizeof(ipf_context_t));
    if (!ctx) {
        free(file_data);
        return NULL;
    }
    
    safe_strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    ctx->path[sizeof(ctx->path) - 1] = '\0';
    ctx->file_data = file_data;
    ctx->file_size = file_size;
    
    /* Parse CAPS header record */
    uint32_t caps_len = read_be32(file_data + 4);
    if (caps_len >= 12) {
        ctx->version = read_be32(file_data + 16);
    }
    
    /* Allocate initial tracks array */
    ctx->tracks = calloc(IPF_MAX_TRACKS, sizeof(ipf_track_t));
    if (!ctx->tracks) {
        free(file_data);
        free(ctx);
        return NULL;
    }
    
    /* Parse all records */
    const uint8_t *ptr = file_data + 12 + caps_len;
    const uint8_t *end = file_data + file_size;
    bool has_info = false;
    
    while (ptr + 12 <= end) {
        char type[5] = {ptr[0], ptr[1], ptr[2], ptr[3], 0};
        uint32_t rec_len = read_be32(ptr + 4);
        uint32_t rec_crc = read_be32(ptr + 8);
        
        if (ptr + 12 + rec_len > end) break;
        
        const uint8_t *rec_data = ptr + 12;
        
        if (memcmp(type, "INFO", 4) == 0) {
            parse_info_record(rec_data, rec_len, &ctx->info);
            has_info = true;
        } else if (memcmp(type, "IMGE", 4) == 0) {
            parse_imge_record(ctx, rec_data, rec_len);
        } else if (memcmp(type, "DATA", 4) == 0) {
            parse_data_record(ctx, rec_data, rec_len, 0);
        }
        
        ptr += 12 + rec_len;
    }
    
    if (!has_info) {
        ipf_close(ctx);
        return NULL;
    }
    
    /* Decode sectors for each track */
    for (uint8_t i = 0; i < ctx->track_count; i++) {
        decode_track_sectors(&ctx->tracks[i], ctx->info.platform);
        ctx->total_sectors += ctx->tracks[i].sector_count;
    }
    
    /* Check for copy protection */
    if (ctx->fuzzy_tracks > 0) {
        ctx->info.has_copy_protection = true;
    }
    
    return ctx;
}

void ipf_close(ipf_context_t *ctx) {
    if (!ctx) return;
    
    /* Free tracks */
    if (ctx->tracks) {
        for (uint8_t i = 0; i < ctx->track_count; i++) {
            ipf_track_t *track = &ctx->tracks[i];
            
            /* Free raw data */
            free(track->raw_data);
            
            /* Free cell timings */
            free(track->cell_timings);
            
            /* Free data areas */
            for (uint8_t j = 0; j < track->data_area_count; j++) {
                free(track->data_areas[j].data);
                free(track->data_areas[j].fuzzy_mask);
            }
        }
        free(ctx->tracks);
    }
    
    /* Free file data */
    free(ctx->file_data);
    
    free(ctx);
}

ipf_error_t ipf_read_track(ipf_context_t *ctx,
                           uint8_t cylinder,
                           uint8_t head,
                           ipf_track_t *track) {
    if (!ctx || !track) return IPF_ERR_NULL_PARAM;
    if (!ctx->tracks) return IPF_ERR_NO_DATA;
    
    memset(track, 0, sizeof(*track));
    
    uint8_t track_idx = cylinder * 2 + head;
    if (track_idx >= ctx->track_count) {
        return IPF_ERR_TRACK_RANGE;
    }
    
    const ipf_track_t *src = &ctx->tracks[track_idx];
    
    /* Copy basic info */
    track->cylinder = src->cylinder;
    track->head = src->head;
    track->sector_count = src->sector_count;
    track->track_bits = src->track_bits;
    track->start_bit = src->start_bit;
    track->rpm = src->rpm;
    track->duration_us = src->duration_us;
    track->flags = src->flags;
    track->has_fuzzy = src->has_fuzzy;
    track->has_weak = src->has_weak;
    track->has_timing = src->has_timing;
    
    /* Copy sectors */
    memcpy(track->sectors, src->sectors, 
           src->sector_count * sizeof(ipf_sector_t));
    
    /* Deep copy raw data */
    if (src->raw_data && src->raw_data_len > 0) {
        track->raw_data = malloc(src->raw_data_len);
        if (track->raw_data) {
            memcpy(track->raw_data, src->raw_data, src->raw_data_len);
            track->raw_data_len = src->raw_data_len;
        }
    }
    
    /* Deep copy cell timings */
    if (src->cell_timings && src->timing_count > 0) {
        track->cell_timings = malloc(src->timing_count * sizeof(uint32_t));
        if (track->cell_timings) {
            memcpy(track->cell_timings, src->cell_timings,
                   src->timing_count * sizeof(uint32_t));
            track->timing_count = src->timing_count;
        }
    }
    
    return IPF_OK;
}

void ipf_free_track(ipf_track_t *track) {
    if (!track) return;
    
    free(track->raw_data);
    free(track->cell_timings);
    
    for (uint8_t i = 0; i < track->data_area_count; i++) {
        free(track->data_areas[i].data);
        free(track->data_areas[i].fuzzy_mask);
    }
    
    memset(track, 0, sizeof(*track));
}

ipf_error_t ipf_read_sector(ipf_context_t *ctx,
                            uint8_t cylinder,
                            uint8_t head,
                            uint8_t sector,
                            uint8_t *data,
                            uint32_t data_size,
                            uint32_t *out_size) {
    if (!ctx || !data || !out_size) return IPF_ERR_NULL_PARAM;
    
    *out_size = 0;
    
    uint8_t track_idx = cylinder * 2 + head;
    if (track_idx >= ctx->track_count) {
        return IPF_ERR_TRACK_RANGE;
    }
    
    const ipf_track_t *track = &ctx->tracks[track_idx];
    
    /* Find sector */
    for (uint8_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].sector == sector) {
            const ipf_sector_t *sec = &track->sectors[i];
            uint32_t size = sec->data_size;
            
            if (size > data_size) size = data_size;
            
            /* Copy sector data from raw track data */
            /* This is simplified - real implementation would decode MFM */
            if (track->raw_data && sec->data_offset + size <= track->raw_data_len) {
                memcpy(data, track->raw_data + sec->data_offset, size);
                *out_size = size;
                return IPF_OK;
            }
            
            return IPF_ERR_NO_DATA;
        }
    }
    
    return IPF_ERR_SECTOR_RANGE;
}

ipf_error_t ipf_get_info(const ipf_context_t *ctx, ipf_info_t *info) {
    if (!ctx || !info) return IPF_ERR_NULL_PARAM;
    *info = ctx->info;
    return IPF_OK;
}

bool ipf_is_valid_file(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t header[4];
    bool valid = false;
    
    if (fread(header, 1, 4, f) == 4) {
        valid = (memcmp(header, "CAPS", 4) == 0);
    }
    
    fclose(f);
    return valid;
}

const char *ipf_error_string(ipf_error_t err) {
    if (err >= IPF_ERR_COUNT) return "Unknown error";
    return error_strings[err];
}

const char *ipf_platform_string(uint32_t platform) {
    if (platform > 9) return "Unknown";
    return platform_strings[platform];
}

const char *ipf_density_string(uint32_t density) {
    if (density > 4) return "Unknown";
    return density_strings[density];
}

ipf_error_t ipf_decode_ctraw(const ipf_track_t *track,
                             double *timings,
                             uint32_t max_timings,
                             uint32_t *out_count) {
    if (!track || !timings || !out_count) return IPF_ERR_NULL_PARAM;
    if (!track->cell_timings) return IPF_ERR_NO_DATA;
    
    *out_count = 0;
    
    for (uint32_t i = 0; i < track->timing_count && *out_count < max_timings; i++) {
        /* CTRaw timings are in nanoseconds */
        timings[(*out_count)++] = (double)track->cell_timings[i];
    }
    
    return IPF_OK;
}

ipf_error_t ipf_ctraw_to_mfm(const ipf_track_t *track,
                             double bit_time_ns,
                             uint8_t *mfm,
                             uint32_t max_bytes,
                             uint32_t *out_bytes) {
    if (!track || !mfm || !out_bytes) return IPF_ERR_NULL_PARAM;
    if (!track->cell_timings) return IPF_ERR_NO_DATA;
    
    if (bit_time_ns <= 0.0) bit_time_ns = 2000.0;  /* Default DD timing */
    
    *out_bytes = 0;
    uint8_t current_byte = 0;
    int bit_count = 0;
    
    for (uint32_t i = 0; i < track->timing_count; i++) {
        double timing = (double)track->cell_timings[i];
        
        /* Quantize to bit cells */
        int cells = (int)((timing + bit_time_ns / 2) / bit_time_ns);
        
        /* Generate bits */
        for (int b = 0; b < cells && *out_bytes < max_bytes; b++) {
            int bit = (b == cells - 1) ? 1 : 0;
            current_byte = (current_byte << 1) | bit;
            bit_count++;
            
            if (bit_count == 8) {
                mfm[(*out_bytes)++] = current_byte;
                current_byte = 0;
                bit_count = 0;
            }
        }
    }
    
    return IPF_OK;
}

ipf_error_t ipf_analyze_protection(const ipf_track_t *track,
                                   uint32_t *protection_type,
                                   uint8_t *confidence) {
    if (!track) return IPF_ERR_NULL_PARAM;
    
    uint32_t prot = 0;
    uint8_t conf = 0;
    
    if (track->has_fuzzy) {
        prot |= 0x01;  /* Fuzzy bits */
        conf += 30;
    }
    
    if (track->has_weak) {
        prot |= 0x02;  /* Weak bits */
        conf += 30;
    }
    
    if (track->has_timing) {
        prot |= 0x04;  /* Timing protection */
        conf += 20;
    }
    
    /* Check for long tracks */
    if (track->track_bits > 110000) {  /* More than ~200ms at DD */
        prot |= 0x08;  /* Long track */
        conf += 20;
    }
    
    if (protection_type) *protection_type = prot;
    if (confidence) *confidence = (conf > 100) ? 100 : conf;
    
    return IPF_OK;
}

ipf_error_t ipf_get_fuzzy_bits(const ipf_track_t *track,
                               uint8_t *fuzzy_mask,
                               size_t mask_size,
                               uint32_t *fuzzy_count) {
    if (!track) return IPF_ERR_NULL_PARAM;
    
    if (fuzzy_count) *fuzzy_count = 0;
    if (fuzzy_mask) memset(fuzzy_mask, 0, mask_size);
    
    if (!track->has_fuzzy) return IPF_OK;
    
    /* Scan data areas for fuzzy bits */
    uint32_t count = 0;
    
    for (uint8_t i = 0; i < track->data_area_count; i++) {
        const ipf_data_area_t *area = &track->data_areas[i];
        if (!area->has_fuzzy || !area->fuzzy_mask) continue;
        
        for (uint32_t j = 0; j < area->data_len; j++) {
            uint8_t mask = area->fuzzy_mask[j];
            if (mask) {
                /* Count bits set */
                for (int b = 0; b < 8; b++) {
                    if (mask & (1 << b)) count++;
                }
                
                /* Copy to output mask */
                if (fuzzy_mask && j < mask_size) {
                    fuzzy_mask[j] |= mask;
                }
            }
        }
    }
    
    if (fuzzy_count) *fuzzy_count = count;
    
    return IPF_OK;
}

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS

#include <assert.h>

static void test_ipf_basics(void) {
    /* Test error strings */
    assert(strcmp(ipf_error_string(IPF_OK), "OK") == 0);
    assert(strcmp(ipf_error_string(IPF_ERR_BAD_SIGNATURE), 
                  "Invalid IPF signature") == 0);
    
    /* Test platform strings */
    assert(strcmp(ipf_platform_string(IPF_PLATFORM_AMIGA), "Amiga") == 0);
    assert(strcmp(ipf_platform_string(IPF_PLATFORM_ATARI_ST), "Atari ST") == 0);
    
    /* Test density strings */
    assert(strcmp(ipf_density_string(IPF_DENSITY_DD), "Double Density") == 0);
    
    /* Test sector size */
    assert(ipf_sector_size(0) == 128);
    assert(ipf_sector_size(1) == 256);
    assert(ipf_sector_size(2) == 512);
    assert(ipf_sector_size(3) == 1024);
    
    printf("  ✓ ipf_basics passed\n");
}

static void test_ipf_crc(void) {
    /* Test CRC-16 CCITT */
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = ipf_crc16(test_data, 4);
    assert(crc != 0);  /* Just verify it computes something */
    
    /* Test CRC-32 */
    uint32_t crc32 = calc_crc32(test_data, 4);
    assert(crc32 != 0);
    
    printf("  ✓ ipf_crc passed\n");
}

static void test_ipf_file_detection(void) {
    /* Test with non-existent file */
    assert(ipf_is_valid_file("/nonexistent.ipf") == false);
    
    printf("  ✓ ipf_file_detection passed\n");
}

static void test_ipf_be_read(void) {
    uint8_t buf[] = {0x12, 0x34, 0x56, 0x78};
    assert(read_be32(buf) == 0x12345678);
    
    printf("  ✓ ipf_be_read passed\n");
}

void uft_ipf_parser_tests(void) {
    printf("Running IPF parser tests...\n");
    test_ipf_basics();
    test_ipf_crc();
    test_ipf_file_detection();
    test_ipf_be_read();
    printf("All IPF parser tests passed!\n");
}

#endif /* UFT_UNIT_TESTS */
