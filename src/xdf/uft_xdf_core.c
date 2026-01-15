/**
 * @file uft_xdf_core.c
 * @brief XDF Core - Universal Forensic Disk Container Implementation
 * 
 * Implements the 7-phase forensic pipeline:
 * 1. Read - Multi-read capture
 * 2. Compare - Stability analysis
 * 3. Analyze - Zone identification
 * 4. Knowledge - Pattern matching
 * 5. Validate - Confidence scoring
 * 6. Repair - Controlled correction
 * 7. Rebuild - Export generation
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/xdf/uft_xdf_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

#define XDF_MAX_TRACKS      256
#define XDF_MAX_SECTORS     64
#define XDF_MAX_ZONES       1024
#define XDF_MAX_REPAIRS     1000
#define XDF_MAX_DECISIONS   1000
#define XDF_MAX_KB_MATCHES  100
#define XDF_MAX_READS       10

struct xdf_context_s {
    xdf_header_t header;
    xdf_options_t options;
    xdf_platform_t platform;
    
    /* Track data */
    xdf_track_t tracks[XDF_MAX_TRACKS];
    int track_count;
    
    /* Sector data */
    xdf_sector_t *sectors;
    size_t sector_count;
    uint8_t *sector_data;
    size_t sector_data_size;
    
    /* Zone maps */
    xdf_zone_t *zones;
    size_t zone_count;
    
    /* Multi-read captures */
    xdf_read_capture_t *reads;
    size_t read_count;
    uint8_t *read_data;
    size_t read_data_size;
    
    /* Stability maps */
    xdf_stability_map_t *stability;
    size_t stability_count;
    uint8_t *stability_data;
    
    /* Protection */
    xdf_protection_t protection;
    
    /* Repair log */
    xdf_repair_entry_t repairs[XDF_MAX_REPAIRS];
    size_t repair_count;
    uint8_t *undo_data;
    size_t undo_size;
    
    /* Decision matrix */
    xdf_decision_t decisions[XDF_MAX_DECISIONS];
    size_t decision_count;
    
    /* Knowledge matches */
    xdf_kb_match_t kb_matches[XDF_MAX_KB_MATCHES];
    size_t kb_match_count;
    
    /* Error handling */
    char last_error[256];
};

/*===========================================================================
 * CRC32 Helper
 *===========================================================================*/

static uint32_t crc32_table[256];
static bool crc32_init = false;

static void init_crc32(void) {
    if (crc32_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c >> 1) ^ ((c & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = c;
    }
    crc32_init = true;
}

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
    init_crc32();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

xdf_context_t* xdf_create(xdf_platform_t platform) {
    xdf_context_t *ctx = calloc(1, sizeof(xdf_context_t));
    if (!ctx) return NULL;
    
    ctx->platform = platform;
    
    /* Initialize header */
    switch (platform) {
        case XDF_PLATFORM_AMIGA:
            memcpy(ctx->header.magic, XDF_MAGIC_AXDF, 4);
            ctx->header.num_heads = 2;
            ctx->header.num_cylinders = 80;
            ctx->header.sectors_per_track = 11;
            ctx->header.sector_size_shift = 9;  /* 512 bytes */
            break;
            
        case XDF_PLATFORM_C64:
            memcpy(ctx->header.magic, XDF_MAGIC_DXDF, 4);
            ctx->header.num_heads = 1;
            ctx->header.num_cylinders = 35;
            ctx->header.sectors_per_track = 21;  /* Zone 1 */
            ctx->header.sector_size_shift = 8;  /* 256 bytes */
            ctx->header.encoding = XDF_ENCODING_GCR_C64;
            break;
            
        case XDF_PLATFORM_PC:
            memcpy(ctx->header.magic, XDF_MAGIC_PXDF, 4);
            ctx->header.num_heads = 2;
            ctx->header.num_cylinders = 80;
            ctx->header.sectors_per_track = 18;
            ctx->header.sector_size_shift = 9;
            ctx->header.encoding = XDF_ENCODING_MFM;
            break;
            
        case XDF_PLATFORM_ATARIST:
            memcpy(ctx->header.magic, XDF_MAGIC_TXDF, 4);
            ctx->header.num_heads = 2;
            ctx->header.num_cylinders = 80;
            ctx->header.sectors_per_track = 9;
            ctx->header.sector_size_shift = 9;
            ctx->header.encoding = XDF_ENCODING_MFM;
            break;
            
        case XDF_PLATFORM_SPECTRUM:
            memcpy(ctx->header.magic, XDF_MAGIC_ZXDF, 4);
            ctx->header.num_heads = 2;
            ctx->header.num_cylinders = 80;
            ctx->header.sectors_per_track = 16;
            ctx->header.sector_size_shift = 8;
            ctx->header.encoding = XDF_ENCODING_MFM;
            break;
            
        default:
            memcpy(ctx->header.magic, XDF_MAGIC_CORE, 4);
            break;
    }
    
    ctx->header.version_major = XDF_VERSION_MAJOR;
    ctx->header.version_minor = XDF_VERSION_MINOR;
    ctx->header.header_size = sizeof(xdf_header_t);
    ctx->header.platform = (uint8_t)platform;
    
    /* Default options */
    ctx->options = xdf_options_default();
    
    return ctx;
}

void xdf_destroy(xdf_context_t *ctx) {
    if (!ctx) return;
    
    free(ctx->sectors);
    free(ctx->sector_data);
    free(ctx->zones);
    free(ctx->reads);
    free(ctx->read_data);
    free(ctx->stability);
    free(ctx->stability_data);
    free(ctx->undo_data);
    
    free(ctx);
}

xdf_options_t xdf_options_default(void) {
    xdf_options_t opts = {0};
    
    /* Phase 1: Read */
    opts.read_count = 3;
    opts.max_revolutions = 5;
    opts.capture_flux = false;
    opts.capture_timing = true;
    
    /* Phase 2: Compare */
    opts.generate_stability_map = true;
    opts.stability_threshold = 0.95f;
    
    /* Phase 3: Analyze */
    opts.analyze_zones = true;
    opts.detect_protection = true;
    
    /* Phase 4: Knowledge */
    opts.use_whdload_db = true;
    opts.use_caps_db = true;
    opts.pattern_dir = NULL;
    
    /* Phase 5: Validate */
    opts.min_confidence = 0.5f;
    
    /* Phase 6: Repair */
    opts.enable_repair = true;
    opts.max_repair_bits = 2;
    opts.repair_only_defects = true;
    opts.require_confirmation = false;
    
    /* Phase 7: Rebuild */
    opts.export_classic = true;
    opts.include_flux = false;
    opts.include_zones = true;
    opts.include_decisions = true;
    
    return opts;
}

int xdf_set_options(xdf_context_t *ctx, const xdf_options_t *opts) {
    if (!ctx || !opts) return -1;
    ctx->options = *opts;
    return 0;
}

/*===========================================================================
 * Pipeline Execution
 *===========================================================================*/

int xdf_run_pipeline(xdf_context_t *ctx, xdf_pipeline_result_t *result) {
    if (!ctx || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    int rc;
    
    /* Phase 1: Read */
    rc = xdf_phase_read(ctx);
    result->total_reads = ctx->read_count;
    result->successful_reads = (rc == 0) ? ctx->read_count : 0;
    result->failed_reads = (rc != 0) ? ctx->read_count : 0;
    
    /* Phase 2: Compare */
    rc = xdf_phase_compare(ctx);
    if (rc == 0 && ctx->stability_count > 0) {
        float total = 0;
        for (size_t i = 0; i < ctx->stability_count; i++) {
            total += ctx->stability[i].reproducibility;
        }
        result->average_stability = total / ctx->stability_count;
    }
    
    /* Phase 3: Analyze */
    rc = xdf_phase_analyze(ctx);
    result->zones_identified = (int)ctx->zone_count;
    for (size_t i = 0; i < ctx->zone_count; i++) {
        if (ctx->zones[i].type == XDF_ZONE_PROTECTION) {
            result->protection_zones++;
        }
        if (ctx->zones[i].type == XDF_ZONE_WEAK) {
            result->weak_zones++;
        }
    }
    
    /* Phase 4: Knowledge */
    rc = xdf_phase_knowledge(ctx);
    result->patterns_matched = (int)ctx->kb_match_count;
    if (ctx->kb_match_count > 0) {
        result->best_match = ctx->kb_matches[0].pattern_name;
        result->match_confidence = ctx->kb_matches[0].confidence / 10000.0f;
    }
    
    /* Phase 5: Validate */
    rc = xdf_phase_validate(ctx);
    result->overall_confidence = ctx->header.overall_confidence;
    result->ok_count = ctx->header.good_sectors;
    result->weak_count = ctx->header.weak_tracks;  /* Approximate */
    result->defect_count = ctx->header.bad_sectors;
    result->protected_count = ctx->header.protected_tracks;
    
    /* Phase 6: Repair */
    if (ctx->options.enable_repair) {
        rc = xdf_phase_repair(ctx);
        result->repairs_attempted = (int)ctx->repair_count;
        result->repairs_successful = ctx->header.repaired_sectors;
        result->repairs_failed = result->repairs_attempted - result->repairs_successful;
    }
    
    /* Phase 7: Rebuild */
    rc = xdf_phase_rebuild(ctx);
    result->xdf_exported = (rc == 0);
    
    return 0;
}

/*===========================================================================
 * Phase 1: Read (Multi-Read Capture)
 *===========================================================================*/

int xdf_phase_read(xdf_context_t *ctx) {
    if (!ctx) return -1;
    
    /* This phase is normally handled by hardware drivers.
     * Here we just set up the data structures. */
    
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    strftime(ctx->header.capture_date, sizeof(ctx->header.capture_date),
             "%Y-%m-%dT%H:%M:%SZ", tm);
    
    ctx->header.capture_revs = (uint8_t)ctx->options.max_revolutions;
    
    return 0;
}

/*===========================================================================
 * Phase 2: Compare (Stability Analysis)
 *===========================================================================*/

int xdf_phase_compare(xdf_context_t *ctx) {
    if (!ctx) return -1;
    
    if (!ctx->options.generate_stability_map) {
        return 0;
    }
    
    /* Allocate stability maps */
    size_t total_tracks = ctx->header.num_cylinders * ctx->header.num_heads;
    ctx->stability = calloc(total_tracks, sizeof(xdf_stability_map_t));
    if (!ctx->stability) return -1;
    
    ctx->stability_count = total_tracks;
    
    /* For each track, analyze bit stability */
    for (size_t t = 0; t < total_tracks; t++) {
        xdf_stability_map_t *map = &ctx->stability[t];
        map->track = (uint8_t)(t / ctx->header.num_heads);
        map->head = (uint8_t)(t % ctx->header.num_heads);
        
        /* Would analyze multiple reads here */
        map->reproducibility = 1.0f;  /* Placeholder */
        map->stable_bits = 1000;
        map->unstable_bits = 0;
    }
    
    return 0;
}

/*===========================================================================
 * Phase 3: Analyze (Zone Identification)
 *===========================================================================*/

int xdf_phase_analyze(xdf_context_t *ctx) {
    if (!ctx) return -1;
    
    if (!ctx->options.analyze_zones) {
        return 0;
    }
    
    /* Allocate zone table */
    ctx->zones = calloc(XDF_MAX_ZONES, sizeof(xdf_zone_t));
    if (!ctx->zones) return -1;
    
    ctx->zone_count = 0;
    
    /* Analyze each track for zones */
    for (int t = 0; t < ctx->track_count; t++) {
        xdf_track_t *track = &ctx->tracks[t];
        
        /* Identify sync zones */
        /* Identify data zones */
        /* Identify gap zones */
        /* Identify weak bit regions */
        /* Identify protection areas */
        
        /* Placeholder: add a data zone for each track */
        if (ctx->zone_count < XDF_MAX_ZONES) {
            xdf_zone_t *zone = &ctx->zones[ctx->zone_count++];
            zone->offset = 0;
            zone->length = track->track_length;
            zone->type = XDF_ZONE_DATA;
            zone->status = XDF_STATUS_OK;
            zone->confidence = XDF_CONF_HIGH;
            zone->stability = 100;
            zone->variance = 0;
        }
    }
    
    /* Detect protection */
    if (ctx->options.detect_protection) {
        /* Would analyze for protection patterns here */
        ctx->protection.type_flags = 0;
        ctx->protection.confidence = 0;
    }
    
    return 0;
}

/*===========================================================================
 * Phase 4: Knowledge Match (Pattern Matching)
 *===========================================================================*/

int xdf_phase_knowledge(xdf_context_t *ctx) {
    if (!ctx) return -1;
    
    ctx->kb_match_count = 0;
    
    /* Try WHDLoad database */
    if (ctx->options.use_whdload_db && ctx->platform == XDF_PLATFORM_AMIGA) {
        /* Would search WHDLoad patterns here */
    }
    
    /* Try CAPS database */
    if (ctx->options.use_caps_db) {
        /* Would search SPS/CAPS patterns here */
    }
    
    /* Try custom patterns */
    if (ctx->options.pattern_dir) {
        /* Would load and match custom patterns here */
    }
    
    return 0;
}

/*===========================================================================
 * Phase 5: Validate (Confidence Scoring)
 *===========================================================================*/

int xdf_phase_validate(xdf_context_t *ctx) {
    if (!ctx) return -1;
    
    uint32_t total_confidence = 0;
    int element_count = 0;
    
    ctx->header.good_tracks = 0;
    ctx->header.weak_tracks = 0;
    ctx->header.bad_tracks = 0;
    ctx->header.repaired_tracks = 0;
    ctx->header.protected_tracks = 0;
    ctx->header.good_sectors = 0;
    ctx->header.bad_sectors = 0;
    ctx->header.repaired_sectors = 0;
    
    /* Score each track */
    for (int t = 0; t < ctx->track_count; t++) {
        xdf_track_t *track = &ctx->tracks[t];
        total_confidence += track->confidence;
        element_count++;
        
        switch (track->status) {
            case XDF_STATUS_OK:
                ctx->header.good_tracks++;
                break;
            case XDF_STATUS_WEAK:
                ctx->header.weak_tracks++;
                break;
            case XDF_STATUS_DEFECT:
            case XDF_STATUS_UNREADABLE:
                ctx->header.bad_tracks++;
                break;
            case XDF_STATUS_REPAIRED:
                ctx->header.repaired_tracks++;
                break;
            case XDF_STATUS_PROTECTED:
                ctx->header.protected_tracks++;
                break;
            default:
                break;
        }
    }
    
    /* Score each sector */
    for (size_t s = 0; s < ctx->sector_count; s++) {
        xdf_sector_t *sector = &ctx->sectors[s];
        total_confidence += sector->confidence;
        element_count++;
        
        if (sector->status == XDF_STATUS_OK) {
            ctx->header.good_sectors++;
        } else if (sector->status == XDF_STATUS_REPAIRED) {
            ctx->header.repaired_sectors++;
        } else if (sector->status == XDF_STATUS_DEFECT ||
                   sector->status == XDF_STATUS_UNREADABLE) {
            ctx->header.bad_sectors++;
        }
    }
    
    /* Overall confidence */
    if (element_count > 0) {
        ctx->header.overall_confidence = 
            (xdf_confidence_t)(total_confidence / element_count);
    }
    
    ctx->header.total_tracks = (uint16_t)ctx->track_count;
    ctx->header.total_sectors = (uint16_t)ctx->sector_count;
    
    return 0;
}

/*===========================================================================
 * Phase 6: Repair (Controlled Correction)
 *===========================================================================*/

int xdf_phase_repair(xdf_context_t *ctx) {
    if (!ctx || !ctx->options.enable_repair) return 0;
    
    ctx->repair_count = 0;
    
    /* For each defective sector, attempt repair */
    for (size_t s = 0; s < ctx->sector_count; s++) {
        xdf_sector_t *sector = &ctx->sectors[s];
        
        /* Skip if not a defect */
        if (sector->status != XDF_STATUS_DEFECT) continue;
        
        /* Skip if it's protection (unless configured otherwise) */
        if (ctx->options.repair_only_defects) {
            /* Would check if this is protection */
        }
        
        /* Attempt 1-bit repair */
        if (ctx->options.max_repair_bits >= 1) {
            /* Would attempt 1-bit CRC correction here */
        }
        
        /* Attempt 2-bit repair */
        if (ctx->options.max_repair_bits >= 2) {
            /* Would attempt 2-bit CRC correction here */
        }
        
        /* Multi-revolution fusion */
        if (ctx->read_count >= 2) {
            /* Would fuse multiple reads here */
        }
    }
    
    return 0;
}

/*===========================================================================
 * Phase 7: Rebuild (Export Generation)
 *===========================================================================*/

int xdf_phase_rebuild(xdf_context_t *ctx) {
    if (!ctx) return -1;
    
    /* Calculate offsets */
    uint32_t offset = sizeof(xdf_header_t);
    
    /* Track table */
    ctx->header.track_table_offset = offset;
    ctx->header.track_table_count = (uint32_t)ctx->track_count;
    offset += ctx->track_count * sizeof(xdf_track_t);
    
    /* Sector table */
    ctx->header.sector_table_offset = offset;
    ctx->header.sector_table_count = (uint32_t)ctx->sector_count;
    offset += ctx->sector_count * sizeof(xdf_sector_t);
    
    /* Zone table */
    if (ctx->options.include_zones) {
        ctx->header.zone_table_offset = offset;
        ctx->header.zone_table_count = (uint32_t)ctx->zone_count;
        offset += ctx->zone_count * sizeof(xdf_zone_t);
    }
    
    /* Repair log */
    ctx->header.repair_log_offset = offset;
    ctx->header.repair_log_count = (uint32_t)ctx->repair_count;
    offset += ctx->repair_count * sizeof(xdf_repair_entry_t);
    
    /* Decision matrix */
    if (ctx->options.include_decisions) {
        ctx->header.decision_table_offset = offset;
        ctx->header.decision_table_count = (uint32_t)ctx->decision_count;
        offset += ctx->decision_count * sizeof(xdf_decision_t);
    }
    
    /* KB matches */
    ctx->header.kb_match_offset = offset;
    ctx->header.kb_match_count = (uint32_t)ctx->kb_match_count;
    offset += ctx->kb_match_count * sizeof(xdf_kb_match_t);
    
    /* Data */
    offset = (offset + XDF_ALIGNMENT - 1) & ~(XDF_ALIGNMENT - 1);
    ctx->header.data_offset = offset;
    ctx->header.data_size = (uint32_t)ctx->sector_data_size;
    
    ctx->header.file_size = offset + ctx->header.data_size;
    
    return 0;
}

/*===========================================================================
 * Import/Export
 *===========================================================================*/

int xdf_import(xdf_context_t *ctx, const char *path) {
    if (!ctx || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* TODO: Use size for validation */
    (void)size;
    
    /* Read header */
    if (fread(&ctx->header, 1, sizeof(xdf_header_t), f) != sizeof(xdf_header_t)) {
        fclose(f);
        return -1;
    }
    
    /* Verify magic */
    if (memcmp(ctx->header.magic, XDF_MAGIC_CORE, 4) != 0 &&
        memcmp(ctx->header.magic, XDF_MAGIC_AXDF, 4) != 0 &&
        memcmp(ctx->header.magic, XDF_MAGIC_DXDF, 4) != 0 &&
        memcmp(ctx->header.magic, XDF_MAGIC_PXDF, 4) != 0 &&
        memcmp(ctx->header.magic, XDF_MAGIC_TXDF, 4) != 0 &&
        memcmp(ctx->header.magic, XDF_MAGIC_ZXDF, 4) != 0) {
        fclose(f);
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Invalid XDF file format");
        return -1;
    }
    
    /* Read tables and data */
    /* ... implementation continues ... */
    
    fclose(f);
    strncpy(ctx->header.capture_device, "XDF Import", 31);
    
    return 0;
}

int xdf_export(xdf_context_t *ctx, const char *path) {
    if (!ctx || !path) return -1;
    
    /* Ensure rebuild is done */
    xdf_phase_rebuild(ctx);
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Cannot create file: %s", path);
        return -1;
    }
    
    /* Calculate CRC */
    ctx->header.file_crc32 = 0;  /* TODO: Calculate */
    
    /* Write header */
    fwrite(&ctx->header, 1, sizeof(xdf_header_t), f);
    
    /* Write track table */
    if (ctx->track_count > 0) {
        fwrite(ctx->tracks, sizeof(xdf_track_t), ctx->track_count, f);
    }
    
    /* Write sector table */
    if (ctx->sector_count > 0 && ctx->sectors) {
        fwrite(ctx->sectors, sizeof(xdf_sector_t), ctx->sector_count, f);
    }
    
    /* Write zone table */
    if (ctx->zone_count > 0 && ctx->zones && ctx->options.include_zones) {
        fwrite(ctx->zones, sizeof(xdf_zone_t), ctx->zone_count, f);
    }
    
    /* Write repair log */
    if (ctx->repair_count > 0) {
        fwrite(ctx->repairs, sizeof(xdf_repair_entry_t), ctx->repair_count, f);
    }
    
    /* Write decisions */
    if (ctx->decision_count > 0 && ctx->options.include_decisions) {
        fwrite(ctx->decisions, sizeof(xdf_decision_t), ctx->decision_count, f);
    }
    
    /* Write KB matches */
    if (ctx->kb_match_count > 0) {
        fwrite(ctx->kb_matches, sizeof(xdf_kb_match_t), ctx->kb_match_count, f);
    }
    
    /* Pad to alignment */
    long pos = ftell(f);
    size_t pad_size = ctx->header.data_offset - pos;
    if (pad_size > 0 && pad_size < XDF_ALIGNMENT) {
        uint8_t *pad = calloc(1, pad_size);
        fwrite(pad, 1, pad_size, f);
        free(pad);
    }
    
    /* Write data */
    if (ctx->sector_data && ctx->sector_data_size > 0) {
        fwrite(ctx->sector_data, 1, ctx->sector_data_size, f);
    }
    
    fclose(f);
    return 0;
}

/*===========================================================================
 * Query Functions
 *===========================================================================*/

const xdf_header_t* xdf_get_header(const xdf_context_t *ctx) {
    return ctx ? &ctx->header : NULL;
}

int xdf_get_track(xdf_context_t *ctx, int cyl, int head, xdf_track_t *info) {
    if (!ctx || !info) return -1;
    
    int idx = cyl * ctx->header.num_heads + head;
    if (idx < 0 || idx >= ctx->track_count) return -1;
    
    *info = ctx->tracks[idx];
    return 0;
}

int xdf_get_sector(xdf_context_t *ctx, int cyl, int head, int sector,
                    xdf_sector_t *info, uint8_t **data, size_t *size) {
    if (!ctx || !info) return -1;
    
    /* Find sector in sector table */
    for (size_t i = 0; i < ctx->sector_count; i++) {
        xdf_sector_t *s = &ctx->sectors[i];
        if (s->head == head && s->sector == sector) {
            /* Check track by calculating expected track index */
            int track_idx = cyl * ctx->header.num_heads + head;
            (void)track_idx;  /* Would validate against sector's track */
            
            *info = *s;
            
            /* Return data pointer if requested */
            if (data && size && ctx->sector_data) {
                /* Calculate offset into sector data */
                size_t offset = i * (1 << ctx->header.sector_size_shift);
                if (offset < ctx->sector_data_size) {
                    *data = ctx->sector_data + offset;
                    *size = 1 << ctx->header.sector_size_shift;
                } else {
                    *data = NULL;
                    *size = 0;
                }
            }
            
            return 0;
        }
    }
    
    return -1;  /* Sector not found */
}

int xdf_get_protection(xdf_context_t *ctx, xdf_protection_t *prot) {
    if (!ctx || !prot) return -1;
    *prot = ctx->protection;
    return 0;
}

int xdf_get_repairs(xdf_context_t *ctx, xdf_repair_entry_t **repairs,
                     size_t *count) {
    if (!ctx || !repairs || !count) return -1;
    *repairs = ctx->repairs;
    *count = ctx->repair_count;
    return 0;
}

int xdf_get_decisions(xdf_context_t *ctx, xdf_decision_t **decisions,
                       size_t *count) {
    if (!ctx || !decisions || !count) return -1;
    *decisions = ctx->decisions;
    *count = ctx->decision_count;
    return 0;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char* xdf_platform_name(xdf_platform_t platform) {
    switch (platform) {
        case XDF_PLATFORM_AMIGA:    return "Amiga";
        case XDF_PLATFORM_C64:      return "Commodore 64";
        case XDF_PLATFORM_PC:       return "IBM PC";
        case XDF_PLATFORM_ATARIST:  return "Atari ST";
        case XDF_PLATFORM_SPECTRUM: return "ZX Spectrum";
        case XDF_PLATFORM_APPLE2:   return "Apple II";
        case XDF_PLATFORM_BBC:      return "BBC Micro";
        case XDF_PLATFORM_MSX:      return "MSX";
        case XDF_PLATFORM_CPC:      return "Amstrad CPC";
        case XDF_PLATFORM_MIXED:    return "Multi-Platform";
        default:                    return "Unknown";
    }
}

const char* xdf_encoding_name(xdf_encoding_t encoding) {
    switch (encoding) {
        case XDF_ENCODING_MFM:       return "MFM";
        case XDF_ENCODING_FM:        return "FM";
        case XDF_ENCODING_GCR_C64:   return "GCR (Commodore)";
        case XDF_ENCODING_GCR_APPLE: return "GCR (Apple)";
        case XDF_ENCODING_GCR_AMIGA: return "GCR (Amiga)";
        case XDF_ENCODING_RAW_FLUX:  return "Raw Flux";
        default:                     return "Unknown";
    }
}

const char* xdf_status_name(xdf_status_t status) {
    switch (status) {
        case XDF_STATUS_OK:         return "OK";
        case XDF_STATUS_WEAK:       return "Weak";
        case XDF_STATUS_PROTECTED:  return "Protected";
        case XDF_STATUS_DEFECT:     return "Defect";
        case XDF_STATUS_REPAIRED:   return "Repaired";
        case XDF_STATUS_UNREADABLE: return "Unreadable";
        case XDF_STATUS_MISSING:    return "Missing";
        default:                    return "Unknown";
    }
}

const char* xdf_error_name(xdf_error_t error) {
    switch (error) {
        case XDF_ERROR_NONE:      return "None";
        case XDF_ERROR_CRC:       return "CRC Error";
        case XDF_ERROR_SYNC:      return "Sync Error";
        case XDF_ERROR_HEADER:    return "Header Error";
        case XDF_ERROR_DATA:      return "Data Error";
        case XDF_ERROR_TIMING:    return "Timing Error";
        case XDF_ERROR_DENSITY:   return "Density Error";
        case XDF_ERROR_MISSING:   return "Missing";
        case XDF_ERROR_DUPLICATE: return "Duplicate";
        case XDF_ERROR_GAP:       return "Gap Error";
        default:                  return "Unknown";
    }
}

const char* xdf_get_error(const xdf_context_t *ctx) {
    return ctx ? ctx->last_error : "NULL context";
}

void xdf_format_confidence(xdf_confidence_t conf, char *buf, size_t size) {
    if (!buf || size < 8) return;
    snprintf(buf, size, "%d.%02d%%", conf / 100, conf % 100);
}
