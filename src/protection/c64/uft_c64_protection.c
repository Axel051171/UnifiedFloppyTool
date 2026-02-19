/**
 * @file uft_c64_protection.c
 * @brief C64/1541 Copy Protection Detection - Main API
 * @version 4.1.5
 *
 * Based on Super-Kit 1541 V2.0 documentation and reverse engineering.
 *
 * Split into modules:
 *   c64_protection_db.c       - Known title database (400+ titles)
 *   c64_protection_analysis.c - Individual protection scheme detectors
 *   uft_c64_protection.c      - This file: core analysis + public API
 */

#include "c64_protection_internal.h"

/* ============================================================================
 * Shared Helper: D64 Sector Offset Calculation
 * ============================================================================ */

size_t c64_d64_get_sector_offset(int track, int sector) {
    if (track < 1 || track > 40) return (size_t)-1;
    if (sector < 0 || sector >= c64_sectors_per_track[track]) return (size_t)-1;

    size_t offset = 0;
    for (int t = 1; t < track; t++) {
        offset += c64_sectors_per_track[t] * D64_SECTOR_SIZE;
    }
    offset += sector * D64_SECTOR_SIZE;

    return offset;
}

/* ============================================================================
 * Error Code Strings
 * ============================================================================ */

const char *c64_error_to_string(c64_error_code_t error_code) {
    switch (error_code) {
        case C64_ERR_OK: return "OK - No error";
        case C64_ERR_HEADER_NOT_FOUND: return "Error 20: Header block not found";
        case C64_ERR_NO_SYNC: return "Error 21: No sync found (unformatted sector)";
        case C64_ERR_DATA_NOT_FOUND: return "Error 22: Data block not found";
        case C64_ERR_CHECKSUM: return "Error 23: Data block checksum error";
        case C64_ERR_VERIFY: return "Error 25: Verify error after write";
        case C64_ERR_WRITE_PROTECT: return "Error 26: Write protect error";
        case C64_ERR_HEADER_CHECKSUM: return "Error 27: Header checksum error";
        case C64_ERR_LONG_DATA: return "Error 28: Long data block";
        case C64_ERR_ID_MISMATCH: return "Error 29: Disk ID mismatch";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * Protection Type to String
 * ============================================================================ */

int c64_protection_to_string(uint32_t protection_type, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return 0;

    buffer[0] = '\0';
    int written = 0;

    if (protection_type == C64_PROT_NONE) {
        return snprintf(buffer, buffer_size, "No protection detected");
    }

    #define ADD_FLAG(flag, name) \
        if (protection_type & (flag)) { \
            if (written > 0) written += snprintf(buffer + written, buffer_size - written, ", "); \
            written += snprintf(buffer + written, buffer_size - written, "%s", name); \
        }

    ADD_FLAG(C64_PROT_ERRORS_T18, "Directory Errors");
    ADD_FLAG(C64_PROT_ERRORS_T36_40, "Extended Track Errors");
    ADD_FLAG(C64_PROT_CUSTOM_ERRORS, "Custom Errors");
    ADD_FLAG(C64_PROT_EXTRA_TRACKS, "Extra Tracks (36-40)");
    ADD_FLAG(C64_PROT_HALF_TRACKS, "Half-Tracks");
    ADD_FLAG(C64_PROT_KILLER_TRACKS, "Killer Tracks");
    ADD_FLAG(C64_PROT_EXTRA_SECTORS, "Extra Sectors");
    ADD_FLAG(C64_PROT_MISSING_SECTORS, "Missing Sectors");
    ADD_FLAG(C64_PROT_INTERLEAVE, "Non-standard Interleave");
    ADD_FLAG(C64_PROT_GCR_TIMING, "GCR Timing");
    ADD_FLAG(C64_PROT_GCR_DENSITY, "GCR Density");
    ADD_FLAG(C64_PROT_GCR_SYNC, "GCR Sync Marks");
    ADD_FLAG(C64_PROT_GCR_LONG_TRACK, "Long Track");
    ADD_FLAG(C64_PROT_GCR_BAD_GCR, "Bad GCR Patterns");
    ADD_FLAG(C64_PROT_VORPAL, "Vorpal (Epyx)");
    ADD_FLAG(C64_PROT_V_MAX, "V-Max!");
    ADD_FLAG(C64_PROT_RAPIDLOK, "RapidLok");
    ADD_FLAG(C64_PROT_FAT_TRACK, "Fat Track");
    ADD_FLAG(C64_PROT_SPEEDLOCK, "Speedlock");
    ADD_FLAG(C64_PROT_NOVALOAD, "Novaload");
    ADD_FLAG(C64_PROT_DATASOFT, "Datasoft Long Track");
    ADD_FLAG(C64_PROT_SSI_RDOS, "SSI RapidDOS");
    ADD_FLAG(C64_PROT_EA_INTERLOCK, "EA Interlock");
    ADD_FLAG(C64_PROT_ABACUS, "Abacus");
    ADD_FLAG(C64_PROT_RAINBIRD, "Rainbird/Firebird");

    #undef ADD_FLAG

    return written;
}

/* ============================================================================
 * BAM Analysis (static helper)
 * ============================================================================ */

static void analyze_bam(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    size_t bam_offset = c64_d64_get_sector_offset(C64_BAM_TRACK, C64_BAM_SECTOR);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) {
        result->bam_valid = false;
        return;
    }

    const uint8_t *bam = data + bam_offset;

    /* Check BAM structure */
    result->bam_valid = true;
    result->bam_free_blocks = 0;
    result->bam_allocated_blocks = 0;

    /* BAM entries start at offset 4, 4 bytes per track */
    for (int track = 1; track <= 35; track++) {
        int bam_idx = 4 + (track - 1) * 4;
        int free_sectors = bam[bam_idx];

        result->bam_free_blocks += free_sectors;
        result->bam_allocated_blocks += c64_sectors_per_track[track] - free_sectors;
    }

    /* Check for extended BAM (tracks 36-40) */
    if (size >= D64_40_TRACKS) {
        result->bam_track_36_40 = true;
    }
}

/* ============================================================================
 * D64 Error Analysis (static helper)
 * ============================================================================ */

static void analyze_d64_errors(const uint8_t *data, size_t size,
                                size_t error_offset, int sector_count,
                                c64_protection_analysis_t *result) {
    if (error_offset >= size) return;

    const uint8_t *errors = data + error_offset;

    /* Map sector index to track */
    int sector_idx = 0;
    for (int track = 1; track <= 40 && sector_idx < sector_count; track++) {
        for (int sector = 0; sector < c64_sectors_per_track[track] && sector_idx < sector_count; sector++) {
            if (sector_idx < (int)(size - error_offset)) {
                uint8_t err = errors[sector_idx];
                if (err != C64_ERR_OK && err != 0x00) {
                    result->total_errors++;
                    if (err < 16) result->error_counts[err]++;
                    result->error_tracks[track] = 1;

                    /* Flag specific error-based protection */
                    if (track == C64_DIR_TRACK) {
                        result->protection_flags |= C64_PROT_ERRORS_T18;
                    }
                    if (track >= 36) {
                        result->protection_flags |= C64_PROT_ERRORS_T36_40;
                    }
                }
            }
            sector_idx++;
        }
    }

    if (result->total_errors > 0) {
        result->protection_flags |= C64_PROT_CUSTOM_ERRORS;
    }
}

/* ============================================================================
 * Main D64 Analysis
 * ============================================================================ */

int c64_analyze_d64(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    if (!data || !result) return -1;

    memset(result, 0, sizeof(c64_protection_analysis_t));

    /* Determine image type */
    bool has_errors = false;
    int tracks = 35;
    int sectors = D64_SECTORS_35;

    switch (size) {
        case D64_35_TRACKS:
            tracks = 35;
            sectors = D64_SECTORS_35;
            break;
        case D64_35_TRACKS_ERRORS:
            tracks = 35;
            sectors = D64_SECTORS_35;
            has_errors = true;
            break;
        case D64_40_TRACKS:
            tracks = 40;
            sectors = D64_SECTORS_40;
            result->uses_track_36_40 = true;
            result->protection_flags |= C64_PROT_EXTRA_TRACKS;
            break;
        case D64_40_TRACKS_ERRORS:
            tracks = 40;
            sectors = D64_SECTORS_40;
            has_errors = true;
            result->uses_track_36_40 = true;
            result->protection_flags |= C64_PROT_EXTRA_TRACKS;
            break;
        default:
            /* Non-standard size */
            if (size > D64_40_TRACKS) {
                tracks = 40;
                sectors = D64_SECTORS_40;
                has_errors = (size > D64_40_TRACKS);
            }
            break;
    }

    result->tracks_used = tracks;
    result->total_sectors = sectors;

    /* Analyze BAM */
    analyze_bam(data, size, result);

    /* Analyze errors if present */
    if (has_errors) {
        size_t error_offset = (tracks == 40) ? D64_40_TRACKS : D64_35_TRACKS;
        analyze_d64_errors(data, size, error_offset, sectors, result);
    }

    /* Try to extract disk name from directory */
    size_t bam_offset = c64_d64_get_sector_offset(C64_BAM_TRACK, C64_BAM_SECTOR);
    if (bam_offset != (size_t)-1 && bam_offset + 256 <= size) {
        const uint8_t *bam = data + bam_offset;

        /* Disk name is at offset 0x90-0x9F (16 chars, padded with 0xA0) */
        char disk_name[17];
        for (int i = 0; i < 16; i++) {
            uint8_t c = bam[0x90 + i];
            if (c == 0xA0) c = ' ';  /* Convert PETSCII padding */
            if (c >= 0xC1 && c <= 0xDA) c = c - 0xC1 + 'A';  /* PETSCII uppercase */
            if (c < 0x20 || c > 0x7E) c = '?';
            disk_name[i] = c;
        }
        disk_name[16] = '\0';

        /* Trim trailing spaces */
        for (int i = 15; i >= 0 && disk_name[i] == ' '; i--) {
            disk_name[i] = '\0';
        }

        strncpy(result->title, disk_name, sizeof(result->title) - 1);

        /* Look up in database */
        c64_known_title_t known;
        if (c64_lookup_title(disk_name, &known)) {
            result->publisher = known.publisher;
            result->protection_flags |= known.protection_flags;
            strncpy(result->protection_name, known.protection_name,
                    sizeof(result->protection_name) - 1);
            result->confidence = 85;
        }
    }

    /* Calculate confidence based on findings */
    if (result->confidence == 0) {
        result->confidence = 50;  /* Base confidence */
        if (result->total_errors > 0) result->confidence += 20;
        if (result->uses_track_36_40) result->confidence += 10;
        if (result->protection_flags != C64_PROT_NONE) result->confidence += 10;
    }

    return 0;
}

/* ============================================================================
 * D64 with explicit error info
 * ============================================================================ */

int c64_analyze_d64_errors(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    /* Same as c64_analyze_d64, which handles error bytes automatically */
    return c64_analyze_d64(data, size, result);
}

/* ============================================================================
 * G64 Analysis (GCR-level)
 * ============================================================================ */

int c64_analyze_g64(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    if (!data || !result || size < 12) return -1;

    memset(result, 0, sizeof(c64_protection_analysis_t));
    result->has_gcr_data = true;

    /* Check G64 header */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        return -2;  /* Not a valid G64 */
    }

    uint8_t version = data[8];
    uint8_t track_count = data[9];
    uint16_t max_track_size = data[10] | (data[11] << 8);

    (void)version;
    (void)max_track_size;

    result->tracks_used = track_count / 2;  /* G64 counts half-tracks */

    /* Scan track table for half-tracks and anomalies */
    const uint32_t *track_offsets = (const uint32_t *)(data + 12);
    const uint32_t *speed_zones = (const uint32_t *)(data + 12 + track_count * 4);

    for (int i = 0; i < track_count && i < 84; i++) {
        uint32_t offset = track_offsets[i];

        if (offset != 0) {
            /* Track exists */
            if (i % 2 == 1) {
                /* Half-track */
                result->uses_half_tracks = true;
                result->half_track_count++;
                result->protection_flags |= C64_PROT_HALF_TRACKS;
            }

            if (i / 2 >= 36) {
                result->uses_track_36_40 = true;
                result->protection_flags |= C64_PROT_EXTRA_TRACKS;
            }

            /* Check track data for sync anomalies */
            if (offset < size) {
                uint16_t track_size = data[offset] | (data[offset + 1] << 8);
                const uint8_t *track_data = data + offset + 2;

                /* Count sync marks (0xFF bytes) */
                int sync_count = 0;
                int long_sync = 0;
                int consecutive_ff = 0;

                for (uint16_t j = 0; j < track_size && offset + 2 + j < size; j++) {
                    if (track_data[j] == 0xFF) {
                        consecutive_ff++;
                        if (consecutive_ff >= 5) sync_count++;
                        if (consecutive_ff > 10) long_sync++;
                    } else {
                        consecutive_ff = 0;
                    }
                }

                /* Non-standard sync patterns */
                if (sync_count == 0 && track_size > 100) {
                    result->protection_flags |= C64_PROT_KILLER_TRACKS;
                }
                if (long_sync > 5) {
                    result->sync_anomalies++;
                    result->protection_flags |= C64_PROT_GCR_SYNC;
                }

                /* Check for long track */
                int expected_size = 7692;  /* Standard track size */
                if (i / 2 < 18) expected_size = 7692;
                else if (i / 2 < 25) expected_size = 7142;
                else if (i / 2 < 31) expected_size = 6666;
                else expected_size = 6250;

                if (track_size > expected_size + 200) {
                    result->protection_flags |= C64_PROT_GCR_LONG_TRACK;
                }
            }
        }
    }

    /* Check for non-standard speed zones */
    for (int i = 0; i < track_count && i < 84; i++) {
        uint32_t speed = speed_zones[i];
        if (speed > 3) {  /* Custom speed zone */
            result->density_anomalies++;
            result->protection_flags |= C64_PROT_GCR_DENSITY;
        }
    }

    result->confidence = 60;
    if (result->uses_half_tracks) result->confidence += 15;
    if (result->sync_anomalies > 0) result->confidence += 10;
    if (result->density_anomalies > 0) result->confidence += 10;

    return 0;
}

/* ============================================================================
 * Report Generation
 * ============================================================================ */

int c64_generate_report(const c64_protection_analysis_t *analysis, char *buffer, size_t buffer_size) {
    if (!analysis || !buffer || buffer_size == 0) return 0;

    int written = 0;

    written += snprintf(buffer + written, buffer_size - written,
        "======================================================================\n"
        "          C64 COPY PROTECTION ANALYSIS REPORT                         \n"
        "======================================================================\n\n");

    if (analysis->title[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "Disk Title: %s\n", analysis->title);
    }

    if (analysis->protection_name[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "Protection: %s\n", analysis->protection_name);
    }

    written += snprintf(buffer + written, buffer_size - written,
        "Confidence: %d%%\n\n", analysis->confidence);

    /* Protection types */
    char prot_str[512];
    c64_protection_to_string(analysis->protection_flags, prot_str, sizeof(prot_str));
    written += snprintf(buffer + written, buffer_size - written,
        "Protection Types Detected:\n  %s\n\n", prot_str);

    /* Track info */
    written += snprintf(buffer + written, buffer_size - written,
        "Track Analysis:\n"
        "  Tracks Used: %d\n"
        "  Extended Tracks (36-40): %s\n"
        "  Half-Tracks: %s (%d found)\n\n",
        analysis->tracks_used,
        analysis->uses_track_36_40 ? "Yes" : "No",
        analysis->uses_half_tracks ? "Yes" : "No",
        analysis->half_track_count);

    /* Error analysis */
    if (analysis->total_errors > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "Error Analysis:\n"
            "  Total Error Sectors: %d\n", analysis->total_errors);

        for (int i = 0; i < 16; i++) {
            if (analysis->error_counts[i] > 0) {
                written += snprintf(buffer + written, buffer_size - written,
                    "    %s: %d\n",
                    c64_error_to_string((c64_error_code_t)i),
                    analysis->error_counts[i]);
            }
        }

        written += snprintf(buffer + written, buffer_size - written,
            "  Tracks with Errors: ");
        bool first = true;
        for (int t = 1; t <= 40; t++) {
            if (analysis->error_tracks[t]) {
                written += snprintf(buffer + written, buffer_size - written,
                    "%s%d", first ? "" : ", ", t);
                first = false;
            }
        }
        written += snprintf(buffer + written, buffer_size - written, "\n\n");
    }

    /* BAM info */
    written += snprintf(buffer + written, buffer_size - written,
        "BAM Analysis:\n"
        "  Valid: %s\n"
        "  Free Blocks: %d\n"
        "  Allocated Blocks: %d\n",
        analysis->bam_valid ? "Yes" : "No",
        analysis->bam_free_blocks,
        analysis->bam_allocated_blocks);

    /* GCR info */
    if (analysis->has_gcr_data) {
        written += snprintf(buffer + written, buffer_size - written,
            "\nGCR Analysis:\n"
            "  Sync Anomalies: %d\n"
            "  Density Anomalies: %d\n"
            "  Timing Anomalies: %d\n",
            analysis->sync_anomalies,
            analysis->density_anomalies,
            analysis->timing_anomalies);
    }

    if (analysis->notes[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "\nNotes: %s\n", analysis->notes);
    }

    return written;
}

/* ============================================================================
 * Unified Protection Detection - Run all detectors
 * ============================================================================ */

void c64_detect_all_protections(const uint8_t *data, size_t size,
                                 c64_protection_analysis_t *result) {
    if (!data || !result) return;

    memset(result, 0, sizeof(*result));

    /* Run base analysis first */
    bool is_g64 = (size >= 12 && memcmp(data, "GCR-1541", 8) == 0);

    if (is_g64) {
        c64_analyze_g64(data, size, result);
    } else {
        c64_analyze_d64(data, size, result);
    }

    /* Now run specific protection detectors */

    /* V-MAX! */
    c64_detect_vmax_version(data, size, result);

    /* RapidLok */
    c64_detect_rapidlok_version(data, size, result);

    /* Datasoft */
    c64_detect_datasoft(data, size, result);

    /* SSI RapidDOS */
    c64_detect_ssi_rdos(data, size, result);

    /* EA Interlock */
    c64_detect_ea_interlock(data, size, result);

    /* Novaload */
    c64_detect_novaload(data, size, result);

    /* Speedlock */
    c64_detect_speedlock(data, size, result);

    /* Try to match known title if no protection name set */
    if (result->protection_name[0] == '\0' && result->protection_flags != C64_PROT_NONE) {
        c64_protection_to_string(result->protection_flags, result->protection_name,
                                  sizeof(result->protection_name));
    }
}
