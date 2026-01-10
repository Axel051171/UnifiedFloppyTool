/**
 * @file uft_xcopy_algo.c
 * @brief XCopy-Style Algorithms Implementation
 * @version 1.0.0
 * 
 * Core algorithms ported from:
 * - XCopy Pro (Amiga): getracklen, mestrack, NibbleRead
 * - ManageDsk: FD_TIMED_SCAN_RESULT sector timing
 */

#include "uft/analysis/uft_xcopy_algo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Standard sync patterns */
#define MFM_SYNC_AMIGA      0x4489  /* Amiga MFM sync */
#define MFM_SYNC_IBM        0x4489  /* IBM MFM sync (same) */
#define FM_SYNC_IBM         0xF57E  /* IBM FM sync */
#define GCR_SYNC_C64        0xFF    /* C64 GCR sync byte */

/* Track timing constants (microseconds) */
#define TRACK_TIME_300RPM   200000  /* 200ms at 300 RPM */
#define TRACK_TIME_360RPM   166667  /* 166.67ms at 360 RPM */
#define RPM_TOLERANCE       0.02    /* 2% RPM tolerance */

/* MFM bit cell time at 250kbps */
#define BIT_CELL_US_250K    4.0     /* 4µs per bit */
#define BIT_CELL_US_500K    2.0     /* 2µs per bit */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Length Measurement (XCopy Pro: getracklen)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * XCopy Pro algorithm (translated from 68k):
 * 
 * getracklen:
 *   move.w #RLEN-2,D0      ; Read LEN in Bytes
 *   lea    RLEN*2(A3),A1   ; Ende des Puffers
 * 1$ tst.w -(A1)           ; letztes Word im Puffer suchen
 *   dbne   D0,1$
 *   addq.l #2,A1
 *   move.l A1,D0
 *   sub.l  A3,D0           ; 2*Tracklaenge in Bytes
 *   lsr.w  #1,D0           ; durch 2 => Mitte, D0 = Tracklaenge
 *   and.w  #$FFFE,D0       ; gerade, wegen LEA!
 *   rts
 */
uft_error_t uft_track_measure_length(const uint8_t *raw_data,
                                     size_t buffer_size,
                                     uft_track_measure_t *out) {
    if (!raw_data || !out || buffer_size < 4) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memset(out, 0, sizeof(*out));
    
    /* Find last non-zero word (XCopy Pro algorithm) */
    size_t last_pos = buffer_size;
    const uint16_t *words = (const uint16_t *)raw_data;
    size_t word_count = buffer_size / 2;
    
    /* Scan backwards for last non-zero word */
    while (word_count > 0) {
        word_count--;
        if (words[word_count] != 0) {
            last_pos = (word_count + 1) * 2;
            break;
        }
    }
    
    /* Find first non-zero data */
    size_t first_pos = 0;
    for (size_t i = 0; i < buffer_size; i++) {
        if (raw_data[i] != 0) {
            first_pos = i;
            break;
        }
    }
    
    /* Calculate track length (XCopy: takes middle of 2 revolutions) */
    out->length_bytes = (uint32_t)(last_pos - first_pos);
    out->length_bits = out->length_bytes * 8;
    out->first_data_offset = (uint32_t)first_pos;
    out->last_data_offset = (uint32_t)last_pos;
    
    /* If we read 2 revolutions, actual length is half */
    if (buffer_size > 20000 && last_pos > buffer_size / 2) {
        out->length_bytes /= 2;
        out->length_bits /= 2;
    }
    
    /* Make even (XCopy: and.w #$FFFE,D0) */
    out->length_bytes &= ~1;
    
    /* Calculate density ratio */
    if (out->length_bytes > 0) {
        out->density_ratio = (float)out->length_bytes / 12500.0f; /* vs standard MFM DD */
    }
    
    out->valid = (out->length_bytes > 1000 && out->length_bytes < 50000);
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sync Pattern Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_track_find_sync_positions(const uint8_t *raw_data,
                                          size_t len,
                                          uint16_t sync_pattern,
                                          uft_sync_pos_t *positions,
                                          size_t max_positions,
                                          size_t *found_count) {
    if (!raw_data || !positions || !found_count || len < 2) {
        return UFT_ERR_INVALID_ARG;
    }
    
    *found_count = 0;
    
    /* Sync pattern bytes (big-endian) */
    uint8_t sync_hi = (sync_pattern >> 8) & 0xFF;
    uint8_t sync_lo = sync_pattern & 0xFF;
    
    for (size_t i = 0; i < len - 1 && *found_count < max_positions; i++) {
        /* Check for sync pattern (handle byte alignment) */
        if (raw_data[i] == sync_hi && raw_data[i + 1] == sync_lo) {
            positions[*found_count].offset = (uint32_t)i;
            positions[*found_count].pattern = sync_pattern;
            positions[*found_count].type = (sync_pattern == MFM_SYNC_AMIGA) ? 1 : 0;
            positions[*found_count].confidence = 100;
            (*found_count)++;
        }
        /* Also check bit-shifted patterns for raw flux data */
        else {
            uint16_t word = ((uint16_t)raw_data[i] << 8) | raw_data[i + 1];
            /* Check various bit shifts */
            for (int shift = 1; shift < 8; shift++) {
                uint16_t shifted = (word << shift) | (raw_data[i + 2] >> (8 - shift));
                if (shifted == sync_pattern && *found_count < max_positions) {
                    positions[*found_count].offset = (uint32_t)i;
                    positions[*found_count].pattern = sync_pattern;
                    positions[*found_count].type = 1;
                    positions[*found_count].confidence = (uint8_t)(100 - shift * 10);
                    (*found_count)++;
                    break;
                }
            }
        }
    }
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Multi-Revolution Reading (XCopy Pro: NibbleRead)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_track_split_revolutions(const uint8_t *buffer,
                                        size_t buffer_size,
                                        size_t expected_rev_len,
                                        uft_multirev_t *out) {
    if (!buffer || !out || buffer_size < expected_rev_len) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memset(out, 0, sizeof(*out));
    
    /* Find index positions by looking for gaps in data */
    size_t pos = 0;
    size_t rev = 0;
    
    while (pos < buffer_size && rev < 16) {
        /* Estimate revolution boundary */
        size_t rev_end = pos + expected_rev_len;
        if (rev_end > buffer_size) {
            rev_end = buffer_size;
        }
        
        /* Measure actual data length for this revolution */
        size_t actual_len = rev_end - pos;
        
        /* Find actual end by looking for trailing zeros */
        while (actual_len > 100) {
            /* Check last 16 bytes */
            bool all_zero = true;
            for (size_t i = 0; i < 16 && pos + actual_len - 16 + i < buffer_size; i++) {
                if (buffer[pos + actual_len - 16 + i] != 0) {
                    all_zero = false;
                    break;
                }
            }
            if (all_zero) {
                actual_len -= 16;
            } else {
                break;
            }
        }
        
        /* Allocate and copy revolution data */
        out->revolutions[rev] = (uint8_t *)malloc(actual_len);
        if (!out->revolutions[rev]) {
            uft_multirev_free(out);
            return UFT_ERR_MEMORY;
        }
        
        memcpy(out->revolutions[rev], buffer + pos, actual_len);
        out->rev_lengths[rev] = actual_len;
        out->index_positions[rev] = (uint32_t)pos;
        
        /* Calculate RPM from length (assuming standard bit rate) */
        /* At 250kbps: 12500 bytes = 200ms = 300 RPM */
        float time_ms = (float)actual_len / 62.5f; /* bytes to ms at 250kbps */
        out->rpm_measured[rev] = 60000.0f / time_ms;
        
        pos = rev_end;
        rev++;
    }
    
    out->num_revolutions = rev;
    
    /* Calculate average RPM */
    if (rev > 0) {
        float sum = 0;
        for (size_t i = 0; i < rev; i++) {
            sum += out->rpm_measured[i];
        }
        out->rpm_average = sum / (float)rev;
        
        /* Calculate jitter */
        float variance = 0;
        for (size_t i = 0; i < rev; i++) {
            float diff = out->rpm_measured[i] - out->rpm_average;
            variance += diff * diff;
        }
        out->rpm_jitter = sqrtf(variance / (float)rev);
    }
    
    return UFT_SUCCESS;
}

uft_error_t uft_track_align_revolutions(uft_multirev_t *multirev) {
    if (!multirev || multirev->num_revolutions < 2) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Find sync positions in first revolution as reference */
    uft_sync_pos_t ref_syncs[64];
    size_t ref_count = 0;
    
    uft_track_find_sync_positions(multirev->revolutions[0],
                                  multirev->rev_lengths[0],
                                  MFM_SYNC_AMIGA,
                                  ref_syncs, 64, &ref_count);
    
    if (ref_count == 0) {
        return UFT_SUCCESS; /* No syncs to align to */
    }
    
    /* Align subsequent revolutions to first */
    for (size_t rev = 1; rev < multirev->num_revolutions; rev++) {
        uft_sync_pos_t rev_syncs[64];
        size_t rev_sync_count = 0;
        
        uft_track_find_sync_positions(multirev->revolutions[rev],
                                      multirev->rev_lengths[rev],
                                      MFM_SYNC_AMIGA,
                                      rev_syncs, 64, &rev_sync_count);
        
        if (rev_sync_count > 0) {
            /* Calculate offset from first sync position */
            int32_t offset = (int32_t)rev_syncs[0].offset - (int32_t)ref_syncs[0].offset;
            
            /* Store alignment offset (negative means shift left) */
            /* Note: actual shift would require reallocation */
            multirev->index_positions[rev] += offset;
        }
    }
    
    return UFT_SUCCESS;
}

uft_error_t uft_track_merge_revolutions(const uft_multirev_t *multirev,
                                        uint8_t *output,
                                        size_t output_size,
                                        size_t *merged_len) {
    if (!multirev || !output || !merged_len || multirev->num_revolutions == 0) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Use first revolution as base */
    size_t base_len = multirev->rev_lengths[0];
    if (base_len > output_size) {
        base_len = output_size;
    }
    
    memcpy(output, multirev->revolutions[0], base_len);
    
    /* If only one revolution, we're done */
    if (multirev->num_revolutions == 1) {
        *merged_len = base_len;
        return UFT_SUCCESS;
    }
    
    /* Confidence-based merging: vote on each byte */
    for (size_t i = 0; i < base_len; i++) {
        uint8_t votes[256] = {0};
        
        for (size_t rev = 0; rev < multirev->num_revolutions; rev++) {
            if (i < multirev->rev_lengths[rev]) {
                votes[multirev->revolutions[rev][i]]++;
            }
        }
        
        /* Find majority vote */
        uint8_t best = output[i];
        uint8_t best_count = votes[best];
        
        for (int v = 0; v < 256; v++) {
            if (votes[v] > best_count) {
                best = (uint8_t)v;
                best_count = votes[v];
            }
        }
        
        output[i] = best;
    }
    
    *merged_len = base_len;
    return UFT_SUCCESS;
}

void uft_multirev_free(uft_multirev_t *multirev) {
    if (!multirev) return;
    
    for (size_t i = 0; i < 16; i++) {
        if (multirev->revolutions[i]) {
            free(multirev->revolutions[i]);
            multirev->revolutions[i] = NULL;
        }
    }
    memset(multirev, 0, sizeof(*multirev));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Timed Sector Scanning (fdrawcmd.sys style)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_track_analyze_timing(const uint8_t *raw_data,
                                     size_t len,
                                     uint8_t encoding,
                                     uft_track_timing_t *out) {
    if (!raw_data || !out || len < 100) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memset(out, 0, sizeof(*out));
    
    /* Find sync positions */
    uft_sync_pos_t syncs[64];
    size_t sync_count = 0;
    uint16_t sync_pattern = (encoding == 0) ? MFM_SYNC_AMIGA : FM_SYNC_IBM;
    
    uft_track_find_sync_positions(raw_data, len, sync_pattern, syncs, 64, &sync_count);
    
    /* Analyze each sector */
    float bit_cell_us = (encoding == 0) ? BIT_CELL_US_250K : BIT_CELL_US_500K;
    
    for (size_t i = 0; i < sync_count && out->sector_count < 64; i++) {
        uft_sector_timing_t *sec = &out->sectors[out->sector_count];
        
        /* Calculate time from index (assuming first byte = index) */
        sec->rel_time_us = (uint32_t)(syncs[i].offset * 8 * bit_cell_us);
        
        /* Parse sector header if possible */
        size_t hdr_pos = syncs[i].offset + 2; /* Skip sync */
        if (hdr_pos + 4 < len) {
            /* IDAM: C, H, R, N */
            sec->cyl = raw_data[hdr_pos];
            sec->head = raw_data[hdr_pos + 1];
            sec->sector = raw_data[hdr_pos + 2];
            sec->size_code = raw_data[hdr_pos + 3];
            sec->valid = true;
        }
        
        /* Estimate field durations */
        sec->header_time_us = (uint32_t)(6 * 8 * bit_cell_us); /* 6 bytes header */
        sec->data_time_us = (uint32_t)((128 << sec->size_code) * 8 * bit_cell_us);
        
        /* Calculate gap to next sector */
        if (i + 1 < sync_count) {
            uint32_t next_time = (uint32_t)(syncs[i + 1].offset * 8 * bit_cell_us);
            uint32_t expected_end = sec->rel_time_us + sec->header_time_us + sec->data_time_us;
            if (next_time > expected_end) {
                sec->gap_after_us = next_time - expected_end;
            }
        }
        
        out->sector_count++;
    }
    
    /* Calculate track time and RPM */
    if (out->sector_count > 0) {
        uft_sector_timing_t *last = &out->sectors[out->sector_count - 1];
        out->track_time_us = last->rel_time_us + last->header_time_us + 
                            last->data_time_us + last->gap_after_us;
        
        /* Estimate full track time from data length */
        out->track_time_us = (uint32_t)(len * 8 * bit_cell_us);
        
        /* RPM = 60,000,000 / track_time_us */
        out->rpm_calculated = 60000000.0f / (float)out->track_time_us;
        
        out->index_to_first_us = out->sectors[0].rel_time_us;
        out->first_seen = out->sectors[0].sector;
    }
    
    /* Check timing consistency */
    out->consistent_timing = true;
    for (size_t i = 1; i < out->sector_count; i++) {
        /* Check gap variance */
        if (out->sectors[i].gap_after_us > 0 && i > 0) {
            float ratio = (float)out->sectors[i].gap_after_us / 
                         (float)out->sectors[i - 1].gap_after_us;
            if (ratio < 0.5f || ratio > 2.0f) {
                out->consistent_timing = false;
            }
        }
    }
    
    /* Detect copy protection */
    out->protection_detected = uft_protection_detect_from_timing(out, 
                                                                 out->protection_type,
                                                                 sizeof(out->protection_type));
    
    return UFT_SUCCESS;
}

bool uft_protection_detect_from_timing(const uft_track_timing_t *timing,
                                       char *protection_name,
                                       size_t name_size) {
    if (!timing || !protection_name || name_size == 0) {
        return false;
    }
    
    protection_name[0] = '\0';
    
    /* Check for various protection schemes */
    
    /* 1. Varying sector sizes (Rob Northen) */
    bool varying_sizes = false;
    uint8_t first_size = timing->sectors[0].size_code;
    for (size_t i = 1; i < timing->sector_count; i++) {
        if (timing->sectors[i].size_code != first_size) {
            varying_sizes = true;
            break;
        }
    }
    if (varying_sizes) {
        snprintf(protection_name, name_size, "Variable Sectors");
        return true;
    }
    
    /* 2. Out-of-order sectors (typical protection) */
    bool out_of_order = false;
    for (size_t i = 1; i < timing->sector_count; i++) {
        int diff = timing->sectors[i].sector - timing->sectors[i - 1].sector;
        if (diff < 0 || diff > 2) {
            out_of_order = true;
            break;
        }
    }
    
    /* 3. Unusual gaps (timing protection) */
    bool unusual_gaps = false;
    for (size_t i = 0; i < timing->sector_count; i++) {
        if (timing->sectors[i].gap_after_us > 10000 || 
            timing->sectors[i].gap_after_us < 100) {
            unusual_gaps = true;
            break;
        }
    }
    
    /* 4. Non-sequential sector IDs */
    bool non_sequential = false;
    uint8_t expected_sector = 1;
    for (size_t i = 0; i < timing->sector_count; i++) {
        if (timing->sectors[i].sector != expected_sector && 
            timing->sectors[i].sector != expected_sector + timing->sector_count) {
            non_sequential = true;
        }
        expected_sector++;
    }
    
    /* 5. Long track (more data than standard) */
    bool long_track = (timing->track_time_us > TRACK_TIME_300RPM * 1.05f);
    
    /* Determine protection type */
    if (unusual_gaps && out_of_order) {
        snprintf(protection_name, name_size, "Timing Protection");
        return true;
    }
    if (long_track) {
        snprintf(protection_name, name_size, "Long Track");
        return true;
    }
    if (non_sequential) {
        snprintf(protection_name, name_size, "Non-Sequential IDs");
        return true;
    }
    
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Per-Drive Calibration (XCopy Pro: mestrack/drilen[])
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_drive_calibration_init(uft_drive_calibration_t *cal) {
    if (!cal) return;
    memset(cal, 0, sizeof(*cal));
    
    /* Default track lengths (MFM DD at 300 RPM) */
    for (int i = 0; i < 4; i++) {
        cal->track_lengths[i] = 12500; /* ~12.5KB */
        cal->rpm_measured[i] = 300.0f;
        cal->offset_bytes[i] = 0;
        cal->calibrated[i] = false;
        cal->drive_type[i] = 0; /* Unknown */
    }
}

uft_error_t uft_drive_calibrate(uft_drive_calibration_t *cal,
                                int drive,
                                const uint8_t *track_data,
                                size_t track_len) {
    if (!cal || !track_data || drive < 0 || drive > 3) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Measure actual track length */
    uft_track_measure_t measure;
    uft_error_t err = uft_track_measure_length(track_data, track_len, &measure);
    if (err != UFT_SUCCESS) {
        return err;
    }
    
    if (!measure.valid) {
        return UFT_ERR_FORMAT;
    }
    
    /* Store calibration */
    cal->track_lengths[drive] = measure.length_bytes;
    
    /* Calculate RPM from track length (at 250kbps) */
    /* 12500 bytes = 200ms = 300 RPM */
    float time_ms = (float)measure.length_bytes / 62.5f;
    cal->rpm_measured[drive] = 60000.0f / time_ms;
    
    cal->calibrated[drive] = true;
    
    return UFT_SUCCESS;
}

uint32_t uft_drive_get_write_length(const uft_drive_calibration_t *cal,
                                    int source_drive,
                                    int target_drive,
                                    int32_t offset) {
    if (!cal || source_drive < 0 || source_drive > 3 ||
        target_drive < 0 || target_drive > 3) {
        return 12500; /* Default */
    }
    
    /* XCopy Pro: MIN(SourceLen, TargetLen) + offset */
    uint32_t source_len = cal->track_lengths[source_drive];
    uint32_t target_len = cal->track_lengths[target_drive];
    
    uint32_t write_len = (source_len < target_len) ? source_len : target_len;
    
    /* Apply offset */
    if (offset > 0) {
        write_len += (uint32_t)offset;
    } else if (offset < 0 && (uint32_t)(-offset) < write_len) {
        write_len -= (uint32_t)(-offset);
    }
    
    /* XCopy Pro subtracts 32 bytes for drive variations */
    if (write_len > 32) {
        write_len -= 32;
    }
    
    return write_len;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Copy Mode Selection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_copy_mode_t uft_recommend_copy_mode(const char *format_name,
                                        bool has_protection,
                                        const uft_track_timing_t *timing) {
    if (!format_name) {
        return UFT_COPY_NIBBLE;
    }
    
    /* Protected disks need nibble or flux copy */
    if (has_protection) {
        if (timing && timing->protection_detected) {
            /* Complex protection - use flux */
            if (strstr(timing->protection_type, "Timing") ||
                strstr(timing->protection_type, "Long Track")) {
                return UFT_COPY_FLUX;
            }
        }
        return UFT_COPY_NIBBLE;
    }
    
    /* Format-specific recommendations */
    if (strstr(format_name, "ADF") || strstr(format_name, "Amiga")) {
        return UFT_COPY_DOS; /* Amiga DOS copy is reliable */
    }
    if (strstr(format_name, "D64") || strstr(format_name, "1541")) {
        return UFT_COPY_BAM; /* C64 BAM copy saves time */
    }
    if (strstr(format_name, "XDF") || strstr(format_name, "DMF")) {
        return UFT_COPY_NIBBLE; /* Variable sectors need nibble */
    }
    if (strstr(format_name, "Apple") || strstr(format_name, "GCR")) {
        return UFT_COPY_NIBBLE; /* Apple GCR needs nibble */
    }
    if (strstr(format_name, "SCP") || strstr(format_name, "Flux")) {
        return UFT_COPY_FLUX;
    }
    
    /* Default: DOS copy for known formats, nibble for unknown */
    return UFT_COPY_DOS;
}

const char *uft_copy_mode_name(uft_copy_mode_t mode) {
    switch (mode) {
        case UFT_COPY_DOS:     return "DOS Copy";
        case UFT_COPY_BAM:     return "BAM Copy";
        case UFT_COPY_DOSPLUS: return "DOS Plus";
        case UFT_COPY_NIBBLE:  return "Nibble Copy";
        case UFT_COPY_OPTIMIZE:return "Optimized";
        case UFT_COPY_FORMAT:  return "Format";
        case UFT_COPY_QFORMAT: return "Quick Format";
        case UFT_COPY_FLUX:    return "Flux Copy";
        default:               return "Unknown";
    }
}
