/**
 * @file uft_protection_pipeline.c
 * @brief Copy Protection Preserve Pipeline Implementation
 * 
 * P2-002: Protection Preserve Pipeline
 */

#include "uft/uft_protection_pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pipeline Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_protection_pipeline {
    uft_protection_options_t options;
    char log_buffer[4096];
    size_t log_offset;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void pipeline_log(uft_protection_pipeline_t *pipe, const char *fmt, ...) {
    if (!pipe || !pipe->options.verbose_log) return;
    if (pipe->log_offset >= sizeof(pipe->log_buffer) - 256) return;
    
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(pipe->log_buffer + pipe->log_offset,
                      sizeof(pipe->log_buffer) - pipe->log_offset,
                      fmt, args);
    va_end(args);
    
    if (n > 0) pipe->log_offset += n;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_protection_pipeline_t* uft_protection_pipeline_create(
    const uft_protection_options_t *options)
{
    uft_protection_pipeline_t *pipe = calloc(1, sizeof(uft_protection_pipeline_t));
    if (!pipe) return NULL;
    
    if (options) {
        pipe->options = *options;
    } else {
        uft_protection_options_t defaults = UFT_PROTECTION_OPTIONS_DEFAULT;
        pipe->options = defaults;
    }
    
    return pipe;
}

void uft_protection_pipeline_destroy(uft_protection_pipeline_t *pipe)
{
    free(pipe);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Map Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_protection_map_t* protection_map_create(int cylinders, int heads)
{
    uft_protection_map_t *map = calloc(1, sizeof(uft_protection_map_t));
    if (!map) return NULL;
    
    map->cylinders = cylinders;
    map->heads = heads;
    map->track_count = cylinders * heads;
    
    map->tracks = calloc(map->track_count, sizeof(uft_track_protection_t));
    if (!map->tracks) {
        free(map);
        return NULL;
    }
    
    /* Initialize track coordinates */
    for (int c = 0; c < cylinders; c++) {
        for (int h = 0; h < heads; h++) {
            int idx = c * heads + h;
            map->tracks[idx].cylinder = c;
            map->tracks[idx].head = h;
        }
    }
    
    return map;
}

void uft_protection_map_free(uft_protection_map_t *map)
{
    if (!map) return;
    
    if (map->tracks) {
        for (int i = 0; i < map->track_count; i++) {
            uft_track_protection_free(&map->tracks[i]);
        }
        free(map->tracks);
    }
    
    if (map->raw_data) free(map->raw_data);
    free(map);
}

void uft_track_protection_free(uft_track_protection_t *track)
{
    if (!track) return;
    
    if (track->elements) {
        for (int i = 0; i < track->element_count; i++) {
            if (track->elements[i].weak_mask)
                free(track->elements[i].weak_mask);
            if (track->elements[i].original_data)
                free(track->elements[i].original_data);
        }
        free(track->elements);
    }
    
    track->elements = NULL;
    track->element_count = 0;
    track->element_capacity = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Weak Bit Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_detect_weak_bits_multirev(
    const uint8_t **rev_data,
    int rev_count,
    size_t data_size,
    float threshold,
    uint8_t *weak_mask_out)
{
    if (!rev_data || rev_count < 2 || !data_size || !weak_mask_out)
        return 0;
    
    memset(weak_mask_out, 0, data_size);
    int weak_count = 0;
    
    /* Compare each bit across revolutions */
    for (size_t byte_idx = 0; byte_idx < data_size; byte_idx++) {
        uint8_t byte_mask = 0;
        
        for (int bit = 0; bit < 8; bit++) {
            int ones = 0;
            int zeros = 0;
            
            for (int rev = 0; rev < rev_count; rev++) {
                if (rev_data[rev][byte_idx] & (1 << bit))
                    ones++;
                else
                    zeros++;
            }
            
            /* Check if bit varies across revolutions */
            float variance = (float)(ones < zeros ? ones : zeros) / rev_count;
            
            if (variance >= threshold) {
                byte_mask |= (1 << bit);
                weak_count++;
            }
        }
        
        weak_mask_out[byte_idx] = byte_mask;
    }
    
    return weak_count;
}

void uft_weak_bits_randomize(
    uint8_t *data,
    const uint8_t *weak_mask,
    size_t size)
{
    if (!data || !weak_mask) return;
    
    for (size_t i = 0; i < size; i++) {
        if (weak_mask[i]) {
            /* Randomize weak bit positions */
            uint8_t random_bits = rand() & weak_mask[i];
            data[i] = (data[i] & ~weak_mask[i]) | random_bits;
        }
    }
}

int uft_weak_bits_count(const uint8_t *mask, size_t size)
{
    if (!mask) return 0;
    
    int count = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t b = mask[i];
        while (b) {
            count += b & 1;
            b >>= 1;
        }
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void add_protection_element(uft_track_protection_t *track,
                                    const uft_protection_element_t *elem)
{
    if (track->element_count >= track->element_capacity) {
        int new_cap = track->element_capacity == 0 ? 8 : track->element_capacity * 2;
        uft_protection_element_t *new_elems = realloc(track->elements,
            new_cap * sizeof(uft_protection_element_t));
        if (!new_elems) return;
        track->elements = new_elems;
        track->element_capacity = new_cap;
    }
    
    track->elements[track->element_count++] = *elem;
}

uft_error_t uft_protection_analyze_track(
    uft_protection_pipeline_t *pipe,
    int cylinder, int head,
    const uint8_t *track_data,
    size_t track_size,
    const uint8_t **multi_rev_data,
    int rev_count,
    uft_track_protection_t *track_out)
{
    if (!pipe || !track_data || !track_out)
        return UFT_ERR_INVALID_PARAM;
    
    memset(track_out, 0, sizeof(uft_track_protection_t));
    track_out->cylinder = cylinder;
    track_out->head = head;
    
    /* Analyze weak bits from multiple revolutions */
    if (pipe->options.detect_weak_bits && multi_rev_data && rev_count >= 2) {
        uint8_t *weak_mask = malloc(track_size);
        if (weak_mask) {
            int weak_count = uft_detect_weak_bits_multirev(
                multi_rev_data, rev_count, track_size,
                pipe->options.weak_bit_threshold,
                weak_mask);
            
            if (weak_count > 0) {
                track_out->artifacts |= UFT_ARTIFACT_WEAK_BITS;
                
                uft_protection_element_t elem = {0};
                elem.cylinder = cylinder;
                elem.head = head;
                elem.sector = -1;  /* Track-level */
                elem.type = UFT_ARTIFACT_WEAK_BITS;
                elem.weak_mask = weak_mask;
                elem.weak_mask_size = track_size;
                elem.weak_bit_count = weak_count;
                elem.confidence = 90;
                snprintf(elem.description, sizeof(elem.description),
                        "%d weak bits detected", weak_count);
                
                add_protection_element(track_out, &elem);
                pipeline_log(pipe, "Track %d/%d: %d weak bits\n",
                            cylinder, head, weak_count);
            } else {
                free(weak_mask);
            }
        }
    }
    
    /* Analyze timing (track length) */
    if (pipe->options.analyze_timing) {
        /* Standard MFM DD track is ~100000 bits */
        double expected_bits = 100000.0;  /* Will be adjusted based on format */
        double actual_bits = track_size * 8.0;
        double variance = (actual_bits - expected_bits) / expected_bits * 100.0;
        
        track_out->track_length_bits = actual_bits;
        track_out->expected_length_bits = expected_bits;
        
        if (variance > pipe->options.timing_tolerance_pct) {
            track_out->artifacts |= UFT_ARTIFACT_LONG_TRACK;
            
            uft_protection_element_t elem = {0};
            elem.cylinder = cylinder;
            elem.head = head;
            elem.sector = -1;
            elem.type = UFT_ARTIFACT_LONG_TRACK;
            elem.variance_pct = variance;
            elem.confidence = 80;
            snprintf(elem.description, sizeof(elem.description),
                    "Long track: +%.1f%%", variance);
            
            add_protection_element(track_out, &elem);
        } else if (variance < -pipe->options.timing_tolerance_pct) {
            track_out->artifacts |= UFT_ARTIFACT_SHORT_TRACK;
            
            uft_protection_element_t elem = {0};
            elem.cylinder = cylinder;
            elem.head = head;
            elem.sector = -1;
            elem.type = UFT_ARTIFACT_SHORT_TRACK;
            elem.variance_pct = variance;
            elem.confidence = 80;
            snprintf(elem.description, sizeof(elem.description),
                    "Short track: %.1f%%", variance);
            
            add_protection_element(track_out, &elem);
        }
    }
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_protection_analyze_file(
    uft_protection_pipeline_t *pipe,
    const char *path,
    uft_protection_map_t **map_out)
{
    if (!pipe || !path || !map_out)
        return UFT_ERR_INVALID_PARAM;
    
    double start_time = get_time_ms();
    
    /* Read file */
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, file_size, f) != file_size) {
        free(data);
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    fclose(f);
    
    /* Determine geometry from file size */
    int cylinders = 80;
    int heads = 2;
    size_t track_size = 0;
    
    if (file_size == 901120) {  /* ADF */
        cylinders = 80;
        heads = 2;
        track_size = 11 * 512;  /* 11 sectors */
    } else if (file_size == 174848 || file_size == 175531) {  /* D64 */
        cylinders = 35;
        heads = 1;
        track_size = file_size / 35;
    } else if (file_size == 737280 || file_size == 1474560) {  /* IMG */
        heads = 2;
        cylinders = file_size == 737280 ? 80 : 80;
        track_size = file_size / (cylinders * heads);
    } else {
        /* Guess */
        track_size = 6250;  /* ~MFM DD */
        cylinders = file_size / (track_size * 2);
        if (cylinders > 84) cylinders = 84;
    }
    
    /* Create protection map */
    uft_protection_map_t *map = protection_map_create(cylinders, heads);
    if (!map) {
        free(data);
        return UFT_ERR_MEMORY;
    }
    
    /* Analyze each track */
    size_t offset = 0;
    for (int c = 0; c < cylinders && offset < file_size; c++) {
        for (int h = 0; h < heads && offset < file_size; h++) {
            int idx = c * heads + h;
            size_t this_track_size = track_size;
            if (offset + this_track_size > file_size) {
                this_track_size = file_size - offset;
            }
            
            /* Note: For sector images, we can't detect weak bits without flux */
            /* For flux formats (SCP, HFE), we would parse revolutions here */
            
            uft_protection_analyze_track(pipe, c, h,
                                         data + offset, this_track_size,
                                         NULL, 0,  /* No multi-rev for sector images */
                                         &map->tracks[idx]);
            
            map->artifacts_present |= map->tracks[idx].artifacts;
            offset += this_track_size;
        }
    }
    
    /* Update statistics */
    for (int i = 0; i < map->track_count; i++) {
        uft_track_protection_t *t = &map->tracks[i];
        for (int j = 0; j < t->element_count; j++) {
            uft_protection_element_t *e = &t->elements[j];
            if (e->type & UFT_ARTIFACT_WEAK_BITS) {
                map->total_weak_bits += e->weak_bit_count;
            }
            if (e->type & UFT_ARTIFACT_BAD_SECTOR) {
                map->total_bad_sectors++;
            }
            if (e->type & (UFT_ARTIFACT_LONG_TRACK | UFT_ARTIFACT_SHORT_TRACK)) {
                map->total_timing_anomalies++;
            }
        }
    }
    
    map->analysis_time_ms = get_time_ms() - start_time;
    
    /* Store raw data for potential conversion */
    map->raw_data = data;
    map->raw_data_size = file_size;
    
    *map_out = map;
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Application
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_protection_apply_to_write(
    uft_protection_pipeline_t *pipe,
    const uft_protection_map_t *map,
    int cylinder, int head,
    uint8_t *track_buffer,
    size_t *track_size,
    uint8_t *weak_mask_out)
{
    if (!pipe || !map || !track_buffer || !track_size)
        return UFT_ERR_INVALID_PARAM;
    
    int idx = cylinder * map->heads + head;
    if (idx >= map->track_count)
        return UFT_ERR_INVALID_PARAM;
    
    const uft_track_protection_t *track = &map->tracks[idx];
    
    /* Apply each protection element */
    for (int i = 0; i < track->element_count; i++) {
        const uft_protection_element_t *elem = &track->elements[i];
        
        if (elem->type & UFT_ARTIFACT_WEAK_BITS) {
            if (weak_mask_out && elem->weak_mask) {
                size_t copy_size = elem->weak_mask_size;
                if (copy_size > *track_size) copy_size = *track_size;
                memcpy(weak_mask_out, elem->weak_mask, copy_size);
            }
        }
        
        /* Apply other artifacts as needed */
    }
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_protection_convert(
    uft_protection_pipeline_t *pipe,
    const uft_protection_map_t *source_map,
    uft_format_t target_format,
    uft_protection_map_t **target_map_out)
{
    if (!pipe || !source_map || !target_map_out)
        return UFT_ERR_INVALID_PARAM;
    
    /* Create new map with same dimensions */
    uft_protection_map_t *target = protection_map_create(
        source_map->cylinders, source_map->heads);
    if (!target) return UFT_ERR_MEMORY;
    
    /* Copy protection info */
    target->scheme = source_map->scheme;
    target->scheme_name = source_map->scheme_name;
    target->confidence = source_map->confidence;
    target->artifacts_present = source_map->artifacts_present;
    target->total_weak_bits = source_map->total_weak_bits;
    target->total_bad_sectors = source_map->total_bad_sectors;
    
    /* Copy track data with format-specific adjustments */
    for (int i = 0; i < source_map->track_count; i++) {
        const uft_track_protection_t *src = &source_map->tracks[i];
        uft_track_protection_t *dst = &target->tracks[i];
        
        dst->artifacts = src->artifacts;
        dst->track_length_bits = src->track_length_bits;
        
        /* Copy elements */
        for (int j = 0; j < src->element_count; j++) {
            const uft_protection_element_t *se = &src->elements[j];
            uft_protection_element_t de = *se;
            
            /* Deep copy allocated data */
            if (se->weak_mask) {
                de.weak_mask = malloc(se->weak_mask_size);
                if (de.weak_mask) {
                    memcpy(de.weak_mask, se->weak_mask, se->weak_mask_size);
                }
            }
            if (se->original_data) {
                de.original_data = malloc(se->data_size);
                if (de.original_data) {
                    memcpy(de.original_data, se->original_data, se->data_size);
                }
            }
            
            add_protection_element(dst, &de);
        }
    }
    
    *target_map_out = target;
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_protection_generate_report(
    const uft_protection_map_t *map,
    char *buffer,
    size_t buffer_size)
{
    if (!map || !buffer || buffer_size < 256)
        return UFT_ERR_INVALID_PARAM;
    
    int n = 0;
    
    n += snprintf(buffer + n, buffer_size - n,
        "═══════════════════════════════════════════════════════════════\n"
        "  COPY PROTECTION ANALYSIS REPORT\n"
        "═══════════════════════════════════════════════════════════════\n\n");
    
    n += snprintf(buffer + n, buffer_size - n,
        "Scheme:     %s\n"
        "Confidence: %d%%\n\n",
        map->scheme_name ? map->scheme_name : "None detected",
        map->confidence);
    
    n += snprintf(buffer + n, buffer_size - n,
        "ARTIFACTS DETECTED:\n");
    
    if (map->artifacts_present & UFT_ARTIFACT_WEAK_BITS)
        n += snprintf(buffer + n, buffer_size - n,
            "  ✓ Weak bits:      %d total\n", map->total_weak_bits);
    if (map->artifacts_present & UFT_ARTIFACT_BAD_SECTOR)
        n += snprintf(buffer + n, buffer_size - n,
            "  ✓ Bad sectors:    %d\n", map->total_bad_sectors);
    if (map->artifacts_present & (UFT_ARTIFACT_LONG_TRACK | UFT_ARTIFACT_SHORT_TRACK))
        n += snprintf(buffer + n, buffer_size - n,
            "  ✓ Timing anomalies: %d tracks\n", map->total_timing_anomalies);
    if (map->artifacts_present & UFT_ARTIFACT_DUP_SECTOR)
        n += snprintf(buffer + n, buffer_size - n,
            "  ✓ Duplicate sectors: %d\n", map->total_duplicate_sectors);
    if (map->artifacts_present & UFT_ARTIFACT_HALF_TRACK)
        n += snprintf(buffer + n, buffer_size - n,
            "  ✓ Half tracks:    %d\n", map->half_track_count);
    
    if (map->artifacts_present == 0) {
        n += snprintf(buffer + n, buffer_size - n,
            "  (No protection artifacts detected)\n");
    }
    
    n += snprintf(buffer + n, buffer_size - n,
        "\nGEOMETRY:\n"
        "  Cylinders: %d\n"
        "  Heads:     %d\n"
        "  Tracks:    %d\n\n",
        map->cylinders, map->heads, map->track_count);
    
    n += snprintf(buffer + n, buffer_size - n,
        "Analysis time: %.2f ms\n",
        map->analysis_time_ms);
    
    n += snprintf(buffer + n, buffer_size - n,
        "═══════════════════════════════════════════════════════════════\n");
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char* uft_artifact_name(uft_artifact_flags_t type)
{
    switch (type) {
        case UFT_ARTIFACT_WEAK_BITS:      return "Weak Bits";
        case UFT_ARTIFACT_BAD_SECTOR:     return "Bad Sector";
        case UFT_ARTIFACT_TIMING_VAR:     return "Timing Variation";
        case UFT_ARTIFACT_DUP_SECTOR:     return "Duplicate Sector";
        case UFT_ARTIFACT_MISSING_SECTOR: return "Missing Sector";
        case UFT_ARTIFACT_EXTRA_SECTOR:   return "Extra Sector";
        case UFT_ARTIFACT_LONG_TRACK:     return "Long Track";
        case UFT_ARTIFACT_SHORT_TRACK:    return "Short Track";
        case UFT_ARTIFACT_HALF_TRACK:     return "Half Track";
        case UFT_ARTIFACT_SYNC_PATTERN:   return "Sync Pattern";
        case UFT_ARTIFACT_GAP_LENGTH:     return "Gap Length";
        case UFT_ARTIFACT_DENSITY_VAR:    return "Density Variation";
        case UFT_ARTIFACT_SECTOR_ID:      return "Sector ID Anomaly";
        case UFT_ARTIFACT_CRC_ERROR:      return "CRC Error";
        case UFT_ARTIFACT_DATA_MARK:      return "Data Mark Anomaly";
        default: return "Unknown";
    }
}

bool uft_format_supports_protection(uft_format_t format, uft_artifact_flags_t artifact)
{
    /* Flux formats support all artifacts */
    switch (format) {
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_UFT_KF_STREAM:
        case UFT_FORMAT_UFT_KF_RAW:
        case UFT_FORMAT_HFE:
        case UFT_FORMAT_IPF:
        case UFT_FORMAT_A2R:
        case UFT_FORMAT_WOZ:
            return true;
        default:
            break;
    }
    
    /* Sector formats support limited artifacts */
    switch (format) {
        case UFT_FORMAT_ADF:
        case UFT_FORMAT_ST:
            /* Can support bad sectors via filesystem */
            return (artifact & UFT_ARTIFACT_BAD_SECTOR) != 0;
        
        case UFT_FORMAT_G64:
        case UFT_FORMAT_G71:
            /* GCR images can store some protection info */
            return (artifact & (UFT_ARTIFACT_WEAK_BITS | 
                               UFT_ARTIFACT_SYNC_PATTERN |
                               UFT_ARTIFACT_GAP_LENGTH)) != 0;
        
        case UFT_FORMAT_NIB:
            /* Nibble format preserves encoding */
            return (artifact & (UFT_ARTIFACT_SYNC_PATTERN |
                               UFT_ARTIFACT_HALF_TRACK)) != 0;
        
        default:
            return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform-Specific Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_amiga_protection_t uft_detect_amiga_protection(
    const uint8_t *track_data,
    size_t track_size,
    int cylinder,
    int head)
{
    if (!track_data || track_size < 1024) return UFT_AMIGA_PROT_NONE;
    
    /* RNC Copylock signature check */
    /* Typically on track 79 or similar outer tracks */
    if (cylinder >= 79) {
        /* Look for weak bit patterns characteristic of Copylock */
        /* This is a simplified check - real detection needs multi-rev */
    }
    
    /* Dungeon Master used specific patterns */
    /* Check for characteristic byte sequences */
    
    return UFT_AMIGA_PROT_NONE;
}

uft_c64_protection_t uft_detect_c64_protection(
    const uint8_t *track_data,
    size_t track_size,
    int track_number)
{
    if (!track_data || track_size < 256) return UFT_C64_PROT_NONE;
    
    /* Rapidlok: Check for non-standard sync patterns */
    /* V-MAX: Check for extra sectors and timing */
    /* Fat Track: Check for oversized track data */
    
    /* Fat Track detection - track significantly larger than expected */
    size_t expected_size = 7500;  /* Approximate GCR track size */
    if (track_size > expected_size * 1.3) {
        return UFT_C64_PROT_FAT_TRACK;
    }
    
    return UFT_C64_PROT_NONE;
}

uft_apple_protection_t uft_detect_apple_protection(
    const uint8_t *track_data,
    size_t track_size,
    int track_number)
{
    if (!track_data || track_size < 256) return UFT_APPLE_PROT_NONE;
    
    /* Half track: Check for data between standard tracks */
    /* Spiral protection: Check for unusual sector arrangements */
    /* Sync count protection: Check for non-standard sync patterns */
    
    return UFT_APPLE_PROT_NONE;
}
