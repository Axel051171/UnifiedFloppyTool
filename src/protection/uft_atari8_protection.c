/**
 * @file uft_atari8_protection.c
 * @brief Atari 8-bit Copy Protection Detection Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/protection/uft_atari8_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Protection Name Tables
 *===========================================================================*/

static const char *protection_names[] = {
    [UFT_A8PROT_NONE] = "None",
    [UFT_A8PROT_BOOT_CRC] = "Boot CRC Check",
    [UFT_A8PROT_BOOT_TIMING] = "Boot Timing",
    [UFT_A8PROT_BOOT_SIGNATURE] = "Boot Signature",
    [UFT_A8PROT_BAD_SECTOR] = "Bad Sector",
    [UFT_A8PROT_DUPLICATE_SECTOR] = "Duplicate Sector",
    [UFT_A8PROT_PHANTOM_SECTOR] = "Phantom Sector",
    [UFT_A8PROT_LONG_SECTOR] = "Long Sector",
    [UFT_A8PROT_SHORT_SECTOR] = "Short Sector",
    [UFT_A8PROT_SECTOR_TIMING] = "Sector Timing",
    [UFT_A8PROT_TRACK_TIMING] = "Track Timing",
    [UFT_A8PROT_REVOLUTION_TIMING] = "Revolution Timing",
    [UFT_A8PROT_GAP_TIMING] = "Gap Timing",
    [UFT_A8PROT_MIXED_DENSITY] = "Mixed Density",
    [UFT_A8PROT_CUSTOM_DENSITY] = "Custom Density",
    [UFT_A8PROT_HALF_TRACK] = "Half Track",
    [UFT_A8PROT_SOFTKEY] = "Softkey",
    [UFT_A8PROT_PICOBOARD] = "PicoBoard",
    [UFT_A8PROT_HAPPY_COPY] = "Happy Copy",
    [UFT_A8PROT_ARCHIVER] = "Archiver",
    [UFT_A8PROT_SPARTA_PROT] = "SpartaDOS Protection",
    [UFT_A8PROT_OSS_PROT] = "OSS Protection",
    [UFT_A8PROT_SSI_PROT] = "SSI Protection",
    [UFT_A8PROT_EA_PROT] = "Electronic Arts",
    [UFT_A8PROT_BRODERBUND_PROT] = "Brøderbund",
    [UFT_A8PROT_INFOCOM_PROT] = "Infocom",
    [UFT_A8PROT_ATX_WEAK_BITS] = "ATX Weak Bits",
    [UFT_A8PROT_ATX_EXTENDED] = "ATX Extended Sector",
    [UFT_A8PROT_VAPI_PROTECTION] = "VAPI Protection",
};

/*===========================================================================
 * Commercial Protection Signatures
 *===========================================================================*/

typedef struct {
    const uint8_t *pattern;
    size_t pattern_len;
    int offset;             /* -1 = anywhere */
    uft_a8prot_type_t type;
    const char *name;
} a8_prot_sig_t;

/* OSS signature: typically in boot sector */
static const uint8_t sig_oss[] = { 0x4F, 0x53, 0x53 };  /* "OSS" */

/* SSI signature */
static const uint8_t sig_ssi[] = { 0x53, 0x53, 0x49 };  /* "SSI" */

/* Electronic Arts */
static const uint8_t sig_ea[] = { 0x45, 0x41 };  /* "EA" */

/* Infocom Z-machine header */
static const uint8_t sig_infocom[] = { 0x03, 0x00 };  /* Z3 header start */

/* SpartaDOS boot signature */
static const uint8_t sig_sparta[] = { 0x53, 0x50, 0x41, 0x52 };  /* "SPAR" */

static const a8_prot_sig_t boot_signatures[] = {
    { sig_oss, sizeof(sig_oss), -1, UFT_A8PROT_OSS_PROT, "OSS" },
    { sig_ssi, sizeof(sig_ssi), -1, UFT_A8PROT_SSI_PROT, "SSI" },
    { sig_ea, sizeof(sig_ea), -1, UFT_A8PROT_EA_PROT, "Electronic Arts" },
    { sig_infocom, sizeof(sig_infocom), 0, UFT_A8PROT_INFOCOM_PROT, "Infocom" },
    { sig_sparta, sizeof(sig_sparta), -1, UFT_A8PROT_SPARTA_PROT, "SpartaDOS" },
};

#define BOOT_SIG_COUNT (sizeof(boot_signatures) / sizeof(boot_signatures[0]))

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

static int find_pattern(const uint8_t *data, size_t size,
                        const uint8_t *pattern, size_t pattern_len)
{
    if (!data || !pattern || pattern_len == 0 || size < pattern_len) {
        return -1;
    }
    
    for (size_t i = 0; i <= size - pattern_len; i++) {
        bool match = true;
        for (size_t j = 0; j < pattern_len; j++) {
            if (data[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) return (int)i;
    }
    return -1;
}

/*===========================================================================
 * Result Management
 *===========================================================================*/

static uft_a8prot_result_t *result_create(void)
{
    uft_a8prot_result_t *result = calloc(1, sizeof(uft_a8prot_result_t));
    if (!result) return NULL;
    
    result->hit_capacity = 64;
    result->hits = calloc(result->hit_capacity, sizeof(uft_a8prot_hit_t));
    if (!result->hits) {
        free(result);
        return NULL;
    }
    
    return result;
}

static int result_add_hit(uft_a8prot_result_t *result, const uft_a8prot_hit_t *hit)
{
    if (!result || !hit) return -1;
    
    if (result->hit_count >= result->hit_capacity) {
        size_t new_cap = result->hit_capacity * 2;
        uft_a8prot_hit_t *new_hits = realloc(result->hits,
                                              new_cap * sizeof(uft_a8prot_hit_t));
        if (!new_hits) return -1;
        result->hits = new_hits;
        result->hit_capacity = new_cap;
    }
    
    result->hits[result->hit_count++] = *hit;
    
    /* Update primary if higher confidence */
    if (hit->confidence > result->overall_confidence) {
        result->primary = hit->type;
        result->overall_confidence = hit->confidence;
    }
    
    /* Mark track as protected */
    if (hit->track < 40) {
        result->bad_tracks[hit->track / 8] |= (1 << (hit->track % 8));
    }
    
    return 0;
}

void uft_a8prot_result_free(uft_a8prot_result_t *result)
{
    if (!result) return;
    free(result->hits);
    free(result);
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

int uft_a8prot_detect_bad_sectors(const uint8_t *track_data, size_t size,
                                   uint8_t track, uint8_t *bad_sectors,
                                   size_t max_count)
{
    if (!track_data || !bad_sectors || max_count == 0) return 0;
    
    /* For ATR format: look for sectors with status indicating errors */
    /* Atari FDC status: bit 2 = lost data, bit 3 = CRC error, bit 4 = not found */
    
    int count = 0;
    
    /* Simple heuristic: sectors with all 0x00 or all 0xFF are suspicious */
    /* In real implementation, we'd parse the actual sector structure */
    
    size_t sector_size = 128;  /* Standard Atari sector size */
    size_t num_sectors = size / sector_size;
    
    for (size_t s = 0; s < num_sectors && (size_t)count < max_count; s++) {
        const uint8_t *sector = track_data + s * sector_size;
        
        /* Check for all zeros or all 0xFF (likely bad) */
        bool all_zero = true;
        bool all_ff = true;
        
        for (size_t i = 0; i < sector_size; i++) {
            if (sector[i] != 0x00) all_zero = false;
            if (sector[i] != 0xFF) all_ff = false;
        }
        
        if (all_zero || all_ff) {
            bad_sectors[count++] = (uint8_t)(s + 1);  /* Sectors are 1-based */
        }
    }
    
    return count;
}

int uft_a8prot_detect_duplicate_sectors(const uint8_t *track_data, size_t size,
                                         uint8_t track, uint8_t *dup_sectors,
                                         size_t max_count)
{
    if (!track_data || !dup_sectors || max_count == 0) return 0;
    
    /* For raw track data, look for duplicate sector IDs */
    /* This requires parsing the track structure */
    
    int count = 0;
    uint8_t seen[256] = {0};
    
    /* Simplified: scan for sector ID patterns */
    /* Real implementation would parse FM/MFM encoding */
    
    for (size_t i = 0; i < size - 4 && (size_t)count < max_count; i++) {
        /* Look for IDAM pattern: FE xx xx yy (FE = ID mark, yy = sector) */
        if (track_data[i] == 0xFE) {
            uint8_t sector_id = track_data[i + 3];
            if (sector_id < 128) {
                if (seen[sector_id]) {
                    dup_sectors[count++] = sector_id;
                }
                seen[sector_id] = 1;
            }
        }
    }
    
    return count;
}

int uft_a8prot_detect_timing(const uint32_t *timing_data, size_t count,
                              uint32_t nominal_ns, uint8_t threshold_pct)
{
    if (!timing_data || count == 0 || nominal_ns == 0) return 0;
    
    uint32_t threshold = nominal_ns * threshold_pct / 100;
    int anomalies = 0;
    
    for (size_t i = 0; i < count; i++) {
        int32_t diff = (int32_t)timing_data[i] - (int32_t)nominal_ns;
        if (diff < 0) diff = -diff;
        
        if ((uint32_t)diff > threshold) {
            anomalies++;
        }
    }
    
    /* Return confidence based on anomaly percentage */
    int anomaly_pct = (anomalies * 100) / (int)count;
    
    if (anomaly_pct > 50) return 90;
    if (anomaly_pct > 30) return 75;
    if (anomaly_pct > 15) return 50;
    if (anomaly_pct > 5) return 30;
    
    return 0;
}

int uft_a8prot_detect_commercial(const uint8_t *boot_sector, size_t boot_size,
                                  uft_a8prot_type_t *type, char *name)
{
    if (!boot_sector || boot_size == 0) return 0;
    
    int best_conf = 0;
    uft_a8prot_type_t best_type = UFT_A8PROT_NONE;
    const char *best_name = NULL;
    
    for (size_t i = 0; i < BOOT_SIG_COUNT; i++) {
        const a8_prot_sig_t *sig = &boot_signatures[i];
        int pos;
        
        if (sig->offset >= 0) {
            /* Check at specific offset */
            if ((size_t)sig->offset + sig->pattern_len <= boot_size) {
                if (memcmp(boot_sector + sig->offset, sig->pattern, sig->pattern_len) == 0) {
                    if (70 > best_conf) {
                        best_conf = 70;
                        best_type = sig->type;
                        best_name = sig->name;
                    }
                }
            }
        } else {
            /* Search anywhere */
            pos = find_pattern(boot_sector, boot_size, sig->pattern, sig->pattern_len);
            if (pos >= 0) {
                if (60 > best_conf) {
                    best_conf = 60;
                    best_type = sig->type;
                    best_name = sig->name;
                }
            }
        }
    }
    
    if (type) *type = best_type;
    if (name && best_name) {
        strncpy(name, best_name, 63);
        name[63] = '\0';
    }
    
    return best_conf;
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_a8prot_analyze_track(const uint8_t *track_data, size_t size,
                              uint8_t track, uft_a8_track_analysis_t *analysis)
{
    if (!track_data || !analysis) return -1;
    
    memset(analysis, 0, sizeof(*analysis));
    analysis->track = track;
    analysis->side = 0;  /* Atari 8-bit is typically single-sided */
    
    /* Determine expected sector count based on track position */
    if (track < 3) {
        analysis->expected_sectors = 18;  /* Boot tracks */
    } else {
        analysis->expected_sectors = 18;  /* Standard */
    }
    
    /* Parse sector structure */
    size_t sector_size = 128;
    analysis->sector_count = (uint8_t)(size / sector_size);
    
    if (analysis->sector_count > 32) {
        analysis->sector_count = 32;
    }
    
    /* Analyze each sector */
    for (uint8_t s = 0; s < analysis->sector_count; s++) {
        uft_a8_sector_info_t *info = &analysis->sectors[s];
        const uint8_t *sector = track_data + s * sector_size;
        
        info->sector_id = s + 1;  /* 1-based */
        info->data_size = (uint16_t)sector_size;
        info->crc_valid = true;  /* Would need actual CRC calculation */
        
        /* Check for suspicious patterns */
        bool all_same = true;
        uint8_t first = sector[0];
        for (size_t i = 1; i < sector_size; i++) {
            if (sector[i] != first) {
                all_same = false;
                break;
            }
        }
        
        if (all_same && (first == 0x00 || first == 0xFF)) {
            info->status = 0x10;  /* Record not found */
            info->is_phantom = true;
        }
    }
    
    /* Check for protection indicators */
    if (analysis->sector_count != analysis->expected_sectors) {
        analysis->has_protection = true;
        if (analysis->sector_count < analysis->expected_sectors) {
            analysis->protection = UFT_A8PROT_PHANTOM_SECTOR;
        } else {
            analysis->protection = UFT_A8PROT_DUPLICATE_SECTOR;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Image Scanning
 *===========================================================================*/

uft_a8prot_result_t *uft_a8prot_scan_image(const char *path,
                                            const uft_a8prot_options_t *options)
{
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size <= 0 || file_size > 16 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    
    /* Read file */
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    uft_a8prot_result_t *result = result_create();
    if (!result) {
        free(data);
        return NULL;
    }
    
    /* Check for ATR header */
    size_t data_offset = 0;
    size_t sector_size = 128;
    uint8_t num_tracks = 40;
    
    if (file_size >= 16 && data[0] == 0x96 && data[1] == 0x02) {
        /* ATR format */
        data_offset = 16;
        uint16_t para_size = data[4] | (data[5] << 8);
        sector_size = para_size;
        
        /* Calculate tracks from file size */
        size_t data_size = file_size - 16;
        size_t sectors = data_size / sector_size;
        num_tracks = (uint8_t)(sectors / 18);
    }
    
    uft_a8prot_options_t opts = UFT_A8PROT_OPTIONS_DEFAULT;
    if (options) opts = *options;
    
    /* Scan boot sector */
    if (opts.scan_boot && data_offset < (size_t)file_size) {
        uft_a8prot_type_t prot_type;
        char prot_name[64];
        int conf = uft_a8prot_detect_commercial(data + data_offset, 
                                                 sector_size * 3, 
                                                 &prot_type, prot_name);
        if (conf > 30) {
            uft_a8prot_hit_t hit = {
                .type = prot_type,
                .track = 0,
                .sector = 1,
                .confidence = (uint16_t)conf,
            };
            snprintf(hit.details, sizeof(hit.details), 
                     "Commercial protection: %s", prot_name);
            result_add_hit(result, &hit);
        }
    }
    
    /* Scan each track */
    uint8_t start = opts.start_track;
    uint8_t end = opts.end_track < num_tracks ? opts.end_track : num_tracks - 1;
    
    for (uint8_t track = start; track <= end; track++) {
        size_t track_offset = data_offset + track * 18 * sector_size;
        size_t track_size = 18 * sector_size;
        
        if (track_offset + track_size > (size_t)file_size) break;
        
        /* Progress callback */
        if (opts.on_progress) {
            opts.on_progress(track, opts.user_data);
        }
        
        /* Check for bad sectors */
        uint8_t bad[32];
        int bad_count = uft_a8prot_detect_bad_sectors(data + track_offset, 
                                                       track_size, track, 
                                                       bad, sizeof(bad));
        
        for (int i = 0; i < bad_count; i++) {
            uft_a8prot_hit_t hit = {
                .type = UFT_A8PROT_BAD_SECTOR,
                .track = track,
                .sector = bad[i],
                .confidence = 80,
            };
            snprintf(hit.details, sizeof(hit.details),
                     "Bad sector at track %d, sector %d", track, bad[i]);
            result_add_hit(result, &hit);
            result->needs_atx = true;
        }
    }
    
    /* Determine preservability */
    result->preservable = true;
    if (result->primary >= UFT_A8PROT_ATX_WEAK_BITS) {
        result->needs_atx = true;
    }
    
    /* Count protected tracks */
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 8; j++) {
            if (result->bad_tracks[i] & (1 << j)) {
                result->protected_track_count++;
            }
        }
    }
    
    free(data);
    return result;
}

uft_a8prot_result_t *uft_a8prot_scan_track(const uint8_t *track_data, size_t size,
                                            uint8_t track,
                                            const uft_a8prot_options_t *options)
{
    if (!track_data || size == 0) return NULL;
    
    uft_a8prot_result_t *result = result_create();
    if (!result) return NULL;
    
    /* Analyze track */
    uft_a8_track_analysis_t analysis;
    uft_a8prot_analyze_track(track_data, size, track, &analysis);
    
    if (analysis.has_protection) {
        uft_a8prot_hit_t hit = {
            .type = analysis.protection,
            .track = track,
            .sector = 0,
            .confidence = 70,
        };
        snprintf(hit.details, sizeof(hit.details),
                 "Track %d: %d sectors (expected %d)",
                 track, analysis.sector_count, analysis.expected_sectors);
        result_add_hit(result, &hit);
    }
    
    return result;
}

/*===========================================================================
 * ATX Support
 *===========================================================================*/

bool uft_a8prot_needs_atx(const uft_a8prot_result_t *result)
{
    if (!result) return false;
    
    if (result->needs_atx) return true;
    
    /* Check if any protection requires ATX */
    for (size_t i = 0; i < result->hit_count; i++) {
        switch (result->hits[i].type) {
            case UFT_A8PROT_SECTOR_TIMING:
            case UFT_A8PROT_GAP_TIMING:
            case UFT_A8PROT_ATX_WEAK_BITS:
            case UFT_A8PROT_ATX_EXTENDED:
            case UFT_A8PROT_DUPLICATE_SECTOR:
            case UFT_A8PROT_PHANTOM_SECTOR:
                return true;
            default:
                break;
        }
    }
    
    return false;
}

size_t uft_a8prot_get_atx_data(const uft_a8prot_result_t *result,
                                uint8_t track, uint8_t *atx_data, size_t max_size)
{
    if (!result || !atx_data || max_size == 0) return 0;
    
    /* Find hits for this track */
    size_t written = 0;
    
    for (size_t i = 0; i < result->hit_count && written < max_size - 8; i++) {
        if (result->hits[i].track == track) {
            /* ATX protection chunk format (simplified):
             * 4 bytes: type
             * 2 bytes: sector
             * 2 bytes: data length
             */
            atx_data[written++] = (uint8_t)result->hits[i].type;
            atx_data[written++] = 0;
            atx_data[written++] = 0;
            atx_data[written++] = 0;
            atx_data[written++] = result->hits[i].sector;
            atx_data[written++] = 0;
            atx_data[written++] = 0;
            atx_data[written++] = 0;
        }
    }
    
    return written;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char *uft_a8prot_name(uft_a8prot_type_t type)
{
    if (type >= UFT_A8PROT_COUNT) return "Unknown";
    return protection_names[type] ? protection_names[type] : "Unknown";
}

const char *uft_a8prot_description(uft_a8prot_type_t type)
{
    switch (type) {
        case UFT_A8PROT_NONE:
            return "No protection detected";
        case UFT_A8PROT_BAD_SECTOR:
            return "Intentionally damaged sectors that fail to read";
        case UFT_A8PROT_DUPLICATE_SECTOR:
            return "Multiple sectors with the same ID on one track";
        case UFT_A8PROT_PHANTOM_SECTOR:
            return "Missing sectors that software expects to fail";
        case UFT_A8PROT_SECTOR_TIMING:
            return "Protection based on specific sector timing";
        case UFT_A8PROT_ATX_WEAK_BITS:
            return "Weak bits that read differently each time";
        default:
            return "Copy protection scheme";
    }
}

void uft_a8prot_preservability(uft_a8prot_type_t type,
                                bool *in_atr, bool *in_atx, bool *in_vapi)
{
    bool atr = true, atx = true, vapi = true;
    
    switch (type) {
        case UFT_A8PROT_NONE:
            atr = true; atx = true; vapi = true;
            break;
        case UFT_A8PROT_BAD_SECTOR:
        case UFT_A8PROT_DUPLICATE_SECTOR:
        case UFT_A8PROT_PHANTOM_SECTOR:
        case UFT_A8PROT_SECTOR_TIMING:
        case UFT_A8PROT_GAP_TIMING:
            atr = false; atx = true; vapi = true;
            break;
        case UFT_A8PROT_ATX_WEAK_BITS:
        case UFT_A8PROT_ATX_EXTENDED:
            atr = false; atx = true; vapi = true;
            break;
        case UFT_A8PROT_HALF_TRACK:
        case UFT_A8PROT_CUSTOM_DENSITY:
            atr = false; atx = false; vapi = true;
            break;
        default:
            break;
    }
    
    if (in_atr) *in_atr = atr;
    if (in_atx) *in_atx = atx;
    if (in_vapi) *in_vapi = vapi;
}

size_t uft_a8prot_to_json(const uft_a8prot_result_t *result,
                           char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size == 0) return 0;
    
    int pos = 0;
    pos += snprintf(buffer + pos, buffer_size - pos,
        "{\n"
        "  \"primary\": \"%s\",\n"
        "  \"confidence\": %d,\n"
        "  \"preservable\": %s,\n"
        "  \"needs_atx\": %s,\n"
        "  \"protected_tracks\": %d,\n"
        "  \"hits\": [\n",
        uft_a8prot_name(result->primary),
        result->overall_confidence,
        result->preservable ? "true" : "false",
        result->needs_atx ? "true" : "false",
        result->protected_track_count);
    
    for (size_t i = 0; i < result->hit_count && pos < (int)buffer_size - 200; i++) {
        const uft_a8prot_hit_t *hit = &result->hits[i];
        pos += snprintf(buffer + pos, buffer_size - pos,
            "    {\n"
            "      \"type\": \"%s\",\n"
            "      \"track\": %d,\n"
            "      \"sector\": %d,\n"
            "      \"confidence\": %d,\n"
            "      \"details\": \"%s\"\n"
            "    }%s\n",
            uft_a8prot_name(hit->type),
            hit->track,
            hit->sector,
            hit->confidence,
            hit->details,
            i < result->hit_count - 1 ? "," : "");
    }
    
    pos += snprintf(buffer + pos, buffer_size - pos, "  ]\n}\n");
    
    return (size_t)pos;
}

void uft_a8prot_print_result(const uft_a8prot_result_t *result)
{
    if (!result) {
        printf("No result\n");
        return;
    }
    
    printf("Atari 8-bit Protection Scan Result\n");
    printf("===================================\n");
    printf("Primary: %s (confidence: %d%%)\n", 
           uft_a8prot_name(result->primary), result->overall_confidence);
    printf("Protected tracks: %d\n", result->protected_track_count);
    printf("Preservable: %s\n", result->preservable ? "yes" : "no");
    printf("Needs ATX: %s\n", result->needs_atx ? "yes" : "no");
    printf("Total hits: %zu\n\n", result->hit_count);
    
    for (size_t i = 0; i < result->hit_count; i++) {
        const uft_a8prot_hit_t *hit = &result->hits[i];
        printf("  [%zu] Track %d, Sector %d: %s (%d%%)\n",
               i + 1, hit->track, hit->sector,
               uft_a8prot_name(hit->type), hit->confidence);
        if (hit->details[0]) {
            printf("       %s\n", hit->details);
        }
    }
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_A8PROT_TEST

int main(void)
{
    printf("Atari 8-bit Protection Detection Self-Test\n");
    printf("==========================================\n\n");
    
    /* Test name functions */
    printf("Protection names:\n");
    printf("  BAD_SECTOR: %s\n", uft_a8prot_name(UFT_A8PROT_BAD_SECTOR));
    printf("  DUPLICATE: %s\n", uft_a8prot_name(UFT_A8PROT_DUPLICATE_SECTOR));
    printf("  ATX_WEAK: %s\n", uft_a8prot_name(UFT_A8PROT_ATX_WEAK_BITS));
    
    /* Test preservability */
    printf("\nPreservability:\n");
    bool atr, atx, vapi;
    uft_a8prot_preservability(UFT_A8PROT_BAD_SECTOR, &atr, &atx, &vapi);
    printf("  BAD_SECTOR: ATR=%s, ATX=%s, VAPI=%s\n",
           atr ? "yes" : "no", atx ? "yes" : "no", vapi ? "yes" : "no");
    
    uft_a8prot_preservability(UFT_A8PROT_HALF_TRACK, &atr, &atx, &vapi);
    printf("  HALF_TRACK: ATR=%s, ATX=%s, VAPI=%s\n",
           atr ? "yes" : "no", atx ? "yes" : "no", vapi ? "yes" : "no");
    
    /* Test track analysis */
    printf("\nTrack analysis test:\n");
    uint8_t test_track[128 * 18];
    memset(test_track, 0x00, sizeof(test_track));  /* Bad sectors */
    
    uft_a8_track_analysis_t analysis;
    uft_a8prot_analyze_track(test_track, sizeof(test_track), 0, &analysis);
    printf("  Track 0: %d sectors, expected %d\n", 
           analysis.sector_count, analysis.expected_sectors);
    printf("  Has protection: %s\n", analysis.has_protection ? "yes" : "no");
    
    /* Test JSON export */
    printf("\nJSON export test:\n");
    uft_a8prot_result_t *result = result_create();
    if (result) {
        uft_a8prot_hit_t hit = {
            .type = UFT_A8PROT_BAD_SECTOR,
            .track = 5,
            .sector = 3,
            .confidence = 85,
        };
        strcpy(hit.details, "Test bad sector");
        result_add_hit(result, &hit);
        
        char json[2048];
        size_t len = uft_a8prot_to_json(result, json, sizeof(json));
        printf("  JSON length: %zu bytes\n", len);
        
        uft_a8prot_result_free(result);
    }
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_A8PROT_TEST */
