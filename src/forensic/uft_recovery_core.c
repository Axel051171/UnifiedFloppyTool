/**
 * @file uft_recovery_core.c
 * @brief Core Recovery Implementation
 * 
 * HAFTUNGSMODUS: Unified Recovery Pipeline
 */

#include "uft/forensic/uft_recovery.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// CONFIG PRESETS
// ============================================================================

void uft_recovery_config_default(uft_recovery_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    
    config->max_retries = 5;
    config->retry_delay_ms = 100;
    config->aggressive_mode = false;
    config->min_confidence = 0.90;
    
    config->min_revolutions = 3;
    config->max_revolutions = 10;
    config->revs_after_success = 2;
    
    config->enable_crc_correction = true;
    config->max_crc_corrections = 2;
    
    config->detect_weak_bits = true;
    config->preserve_weak_bits = false;
    config->weak_bit_threshold = 0.7;
    
    config->repair_bam = false;
    config->repair_directory = false;
    config->validate_chain = true;
    
    config->enable_remap = false;
    config->remap_strategy = 0;
    config->fill_pattern = 0x00;
    
    config->preserve_all_passes = false;
    config->preserve_flux_timing = false;
    config->create_audit_log = false;
    config->audit_log = NULL;
}

void uft_recovery_config_paranoid(uft_recovery_config_t* config) {
    uft_recovery_config_default(config);
    
    config->max_retries = 20;
    config->min_confidence = 0.99;
    config->min_revolutions = 5;
    config->max_revolutions = 20;
    config->revs_after_success = 5;
    config->preserve_all_passes = true;
    config->preserve_flux_timing = true;
    config->create_audit_log = true;
    config->aggressive_mode = false;
}

void uft_recovery_config_aggressive(uft_recovery_config_t* config) {
    uft_recovery_config_default(config);
    
    config->max_retries = 10;
    config->min_confidence = 0.70;
    config->aggressive_mode = true;
    config->max_crc_corrections = 4;
    config->repair_bam = true;
    config->repair_directory = true;
    config->enable_remap = true;
    config->remap_strategy = 2;  // Fill pattern
}

// ============================================================================
// CRC CALCULATION
// ============================================================================

static uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
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

// ============================================================================
// CRC ERROR CORRECTION
// ============================================================================

static bool try_crc_correction(uint8_t* data, size_t len, uint16_t expected_crc, int max_bits) {
    if (max_bits > 4) max_bits = 4;  // Limit complexity
    
    // Try single-bit corrections
    if (max_bits >= 1) {
        for (size_t byte = 0; byte < len; byte++) {
            for (int bit = 0; bit < 8; bit++) {
                data[byte] ^= (1 << bit);
                if (crc16_ccitt(data, len) == expected_crc) {
                    return true;
                }
                data[byte] ^= (1 << bit);  // Restore
            }
        }
    }
    
    // Try two-bit corrections
    if (max_bits >= 2) {
        for (size_t b1 = 0; b1 < len * 8; b1++) {
            for (size_t b2 = b1 + 1; b2 < len * 8; b2++) {
                data[b1 / 8] ^= (1 << (b1 % 8));
                data[b2 / 8] ^= (1 << (b2 % 8));
                
                if (crc16_ccitt(data, len) == expected_crc) {
                    return true;
                }
                
                data[b1 / 8] ^= (1 << (b1 % 8));
                data[b2 / 8] ^= (1 << (b2 % 8));
            }
        }
    }
    
    return false;
}

// ============================================================================
// SECTOR RECOVERY
// ============================================================================

int uft_recovery_sector_single(const uint8_t* sector_data, size_t sector_size,
                               int track, int sector,
                               const uft_recovery_config_t* config,
                               uft_sector_result_t* result) {
    if (!sector_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->sector = sector;
    result->status = UFT_RECOV_STATUS_OK;
    result->confidence = 1.0;
    
    // If no config, use defaults
    uft_recovery_config_t default_config;
    if (!config) {
        uft_recovery_config_default(&default_config);
        config = &default_config;
    }
    
    // Validate sector data
    if (sector_size < 256) {
        result->status = UFT_RECOV_STATUS_FAILED;
        result->confidence = 0.0;
        strncpy(result->method, "invalid_size", sizeof(result->method) - 1);
        return -1;
    }
    
    // Check CRC (assuming last 2 bytes are CRC)
    if (sector_size > 2) {
        uint16_t stored_crc = (sector_data[sector_size - 2] << 8) | 
                              sector_data[sector_size - 1];
        uint16_t calc_crc = crc16_ccitt(sector_data, sector_size - 2);
        
        if (stored_crc != calc_crc) {
            // CRC mismatch - try correction
            if (config->enable_crc_correction) {
                uint8_t* temp = malloc(sector_size);
                if (temp) {
                    memcpy(temp, sector_data, sector_size);
                    
                    if (try_crc_correction(temp, sector_size - 2, stored_crc, 
                                          config->max_crc_corrections)) {
                        result->status = UFT_RECOV_STATUS_OK;
                        result->corrections_applied = 1;
                        result->confidence = 0.95;
                        strncpy(result->method, "crc_corrected", sizeof(result->method) - 1);
                    } else {
                        result->status = UFT_RECOV_STATUS_PARTIAL;
                        result->confidence = 0.5;
                        strncpy(result->method, "crc_failed", sizeof(result->method) - 1);
                    }
                    
                    free(temp);
                }
            } else {
                result->status = UFT_RECOV_STATUS_PARTIAL;
                result->confidence = 0.5;
                strncpy(result->method, "crc_error", sizeof(result->method) - 1);
            }
        } else {
            strncpy(result->method, "crc_ok", sizeof(result->method) - 1);
        }
    }
    
    return 0;
}

// ============================================================================
// TRACK RECOVERY
// ============================================================================

int uft_recovery_track(const uint8_t* track_data, size_t track_size,
                       int track_num,
                       const uft_recovery_config_t* config,
                       uft_recovery_result_t* result) {
    if (!track_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->status = UFT_RECOV_STATUS_OK;
    result->overall_confidence = 1.0;
    
    // Estimate sector count based on track size
    // Assuming 256-byte sectors with some overhead
    int est_sectors = track_size / 300;
    if (est_sectors < 1) est_sectors = 1;
    if (est_sectors > 21) est_sectors = 21;
    
    result->sector_results = calloc(est_sectors, sizeof(uft_sector_result_t));
    if (!result->sector_results) return -1;
    
    result->total_sectors = est_sectors;
    result->sector_result_count = est_sectors;
    
    // Process each sector
    for (int s = 0; s < est_sectors; s++) {
        size_t offset = s * 256;
        if (offset + 256 > track_size) break;
        
        uft_recovery_sector_single(track_data + offset, 256,
                                   track_num, s, config,
                                   &result->sector_results[s]);
        
        if (result->sector_results[s].status == UFT_RECOV_STATUS_OK) {
            result->recovered_sectors++;
        } else if (result->sector_results[s].status == UFT_RECOV_STATUS_PARTIAL) {
            result->partial_sectors++;
        } else {
            result->failed_sectors++;
        }
    }
    
    // Calculate overall confidence
    if (result->total_sectors > 0) {
        double total_conf = 0;
        for (int s = 0; s < result->sector_result_count; s++) {
            total_conf += result->sector_results[s].confidence;
        }
        result->overall_confidence = total_conf / result->sector_result_count;
    }
    
    if (result->failed_sectors > 0) {
        result->status = UFT_RECOV_STATUS_PARTIAL;
    }
    
    return 0;
}

// ============================================================================
// DISK RECOVERY
// ============================================================================

int uft_recovery_disk(const uint8_t* input, size_t input_size,
                      const uft_recovery_config_t* config,
                      uft_recovery_result_t* result) {
    if (!input || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    clock_t start = clock();
    
    // Use default config if none provided
    uft_recovery_config_t default_config;
    if (!config) {
        uft_recovery_config_default(&default_config);
        config = &default_config;
    }
    
    // Estimate track count
    // D64: 174848 bytes = 683 sectors × 256 bytes
    // Typical track: ~6000-7500 bytes
    int est_tracks = input_size / 7000;
    if (est_tracks < 1) est_tracks = 1;
    if (est_tracks > 84) est_tracks = 84;
    
    result->total_sectors = 0;
    
    // Process each track
    size_t track_size = input_size / est_tracks;
    
    for (int t = 0; t < est_tracks; t++) {
        size_t offset = t * track_size;
        if (offset + track_size > input_size) break;
        
        uft_recovery_result_t track_result;
        if (uft_recovery_track(input + offset, track_size, t, config, &track_result) == 0) {
            result->total_sectors += track_result.total_sectors;
            result->recovered_sectors += track_result.recovered_sectors;
            result->partial_sectors += track_result.partial_sectors;
            result->failed_sectors += track_result.failed_sectors;
            
            uft_recovery_result_free(&track_result);
        }
        
        // Progress callback
        if (config->progress_cb) {
            int percent = (t + 1) * 100 / est_tracks;
            config->progress_cb(percent, "Recovering tracks", config->callback_user);
        }
    }
    
    // Calculate overall status
    if (result->failed_sectors == 0) {
        result->status = UFT_RECOV_STATUS_OK;
    } else if (result->recovered_sectors > 0) {
        result->status = UFT_RECOV_STATUS_PARTIAL;
    } else {
        result->status = UFT_RECOV_STATUS_FAILED;
    }
    
    if (result->total_sectors > 0) {
        result->overall_confidence = (double)result->recovered_sectors / result->total_sectors;
    }
    
    result->elapsed_seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    return 0;
}

// ============================================================================
// WEAK SECTOR RECOVERY
// ============================================================================

int uft_recovery_weak_sector(const uint8_t** revolutions,
                             const size_t* sizes,
                             int count,
                             uint8_t* output,
                             size_t* output_size,
                             double* confidence) {
    if (!revolutions || !sizes || !output || count < 2) return -1;
    
    // Find minimum size
    size_t min_size = sizes[0];
    for (int i = 1; i < count; i++) {
        if (sizes[i] < min_size) min_size = sizes[i];
    }
    
    if (output_size) *output_size = min_size;
    
    // Consensus voting for each byte
    int good_bytes = 0;
    
    for (size_t byte = 0; byte < min_size; byte++) {
        // Count occurrences of each value
        int votes[256] = {0};
        for (int r = 0; r < count; r++) {
            votes[revolutions[r][byte]]++;
        }
        
        // Find majority
        int best_val = 0;
        int best_count = 0;
        for (int v = 0; v < 256; v++) {
            if (votes[v] > best_count) {
                best_count = votes[v];
                best_val = v;
            }
        }
        
        output[byte] = best_val;
        
        // Check consensus strength
        if (best_count >= (count + 1) / 2) {
            good_bytes++;
        }
    }
    
    if (confidence && min_size > 0) {
        *confidence = (double)good_bytes / min_size;
    }
    
    return 0;
}

// ============================================================================
// CLEANUP
// ============================================================================

void uft_recovery_result_free(uft_recovery_result_t* result) {
    if (!result) return;
    
    free(result->sector_results);
    free(result->recovered_data);
    free(result->weak_map);
    
    if (result->repair_log) {
        for (int i = 0; i < result->repair_log_count; i++) {
            free(result->repair_log[i]);
        }
        free(result->repair_log);
    }
    
    memset(result, 0, sizeof(*result));
}
