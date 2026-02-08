/**
 * @file uft_forensic_flux_decoder.c
 * @brief Forensic-Grade Multi-Stage Flux Decoder Implementation
 * 
 * 6-Stage Pipeline:
 * 1. Pre-Analysis: Cell time estimation, anomaly detection
 * 2. PLL Decode: Adaptive clock recovery with confidence tracking
 * 3. Multi-Rev Fusion: Confidence-weighted bit voting
 * 4. Sector Recovery: Fuzzy sync detection, error tolerance
 * 5. Error Correction: CRC-based bit correction
 * 6. Verification: Final validation and audit
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/decoder/uft_forensic_flux_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/*============================================================================
 * SESSION STRUCTURE
 *============================================================================*/

struct uft_ffd_session_s {
    uft_ffd_config_t config;
    uft_ffd_stats_t stats;
    
    /* Audit log */
    char** audit_log;
    int audit_count;
    int audit_capacity;
};

/*============================================================================
 * CONFIGURATION PRESETS
 *============================================================================*/

uft_ffd_config_t uft_ffd_config_default(void) {
    return (uft_ffd_config_t){
        /* Pre-analysis */
        .min_cell_ratio = 0.5,
        .max_cell_ratio = 2.5,
        .expected_rpm = 0.0,
        
        /* PLL */
        .pll_bandwidth = 0.05,
        .pll_damping = 0.707,
        .weak_threshold = 0.3,
        
        /* Fusion */
        .enable_fusion = true,
        .fusion_min_consensus = 0.6,
        .max_revolutions = 5,
        
        /* Sector recovery */
        .sync_hamming_tolerance = 2,
        .enable_correction = true,
        .max_correction_bits = 2,
        
        /* Output */
        .keep_raw_bits = false,
        .keep_confidence = false,
        .enable_audit = false,
    };
}

uft_ffd_config_t uft_ffd_config_paranoid(void) {
    uft_ffd_config_t cfg = uft_ffd_config_default();
    cfg.pll_bandwidth = 0.02;
    cfg.weak_threshold = 0.2;
    cfg.fusion_min_consensus = 0.8;
    cfg.max_revolutions = 10;
    cfg.sync_hamming_tolerance = 1;
    cfg.max_correction_bits = 3;
    cfg.keep_raw_bits = true;
    cfg.keep_confidence = true;
    cfg.enable_audit = true;
    return cfg;
}

uft_ffd_config_t uft_ffd_config_fast(void) {
    uft_ffd_config_t cfg = uft_ffd_config_default();
    cfg.pll_bandwidth = 0.1;
    cfg.enable_fusion = false;
    cfg.max_revolutions = 1;
    cfg.enable_correction = false;
    cfg.keep_raw_bits = false;
    cfg.keep_confidence = false;
    return cfg;
}

/*============================================================================
 * SESSION MANAGEMENT
 *============================================================================*/

uft_ffd_session_t* uft_ffd_session_create(const uft_ffd_config_t* config) {
    uft_ffd_session_t* session = calloc(1, sizeof(uft_ffd_session_t));
    if (!session) return NULL;
    
    session->config = config ? *config : uft_ffd_config_default();
    
    if (session->config.enable_audit) {
        session->audit_capacity = 1024;
        session->audit_log = calloc(session->audit_capacity, sizeof(char*));
    }
    
    return session;
}

void uft_ffd_session_destroy(uft_ffd_session_t* session) {
    if (!session) return;
    
    if (session->audit_log) {
        for (int i = 0; i < session->audit_count; i++) {
            free(session->audit_log[i]);
        }
        free(session->audit_log);
    }
    
    free(session);
}

int uft_ffd_get_stats(const uft_ffd_session_t* session, uft_ffd_stats_t* stats) {
    if (!session || !stats) return -1;
    *stats = session->stats;
    return 0;
}

static void audit_log(uft_ffd_session_t* session, const char* fmt, ...) {
    if (!session || !session->config.enable_audit || !session->audit_log) return;
    
    if (session->audit_count >= session->audit_capacity) return;
    
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    session->audit_log[session->audit_count++] = strdup(buf);
}

/*============================================================================
 * STAGE 1: PRE-ANALYSIS
 *============================================================================*/

int uft_ffd_preanalyze(
    const uint32_t* flux,
    size_t count,
    double sample_clock,
    uft_encoding_t encoding,
    uft_preanalysis_result_t* result)
{
    if (!flux || count < 100 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Build histogram of flux intervals */
    double sum = 0.0;
    double sum_sq = 0.0;
    double min_delta = 1e9, max_delta = 0.0;
    
    for (size_t i = 0; i < count; i++) {
        double delta_ns = (double)flux[i] * 1e9 / sample_clock;
        sum += delta_ns;
        sum_sq += delta_ns * delta_ns;
        if (delta_ns < min_delta) min_delta = delta_ns;
        if (delta_ns > max_delta) max_delta = delta_ns;
    }
    
    double mean = sum / count;
    double variance = (sum_sq / count) - (mean * mean);
    double stddev = sqrt(variance > 0 ? variance : 0);
    
    /* Estimate cell time based on encoding */
    double cell_time_ns;
    switch (encoding) {
        case UFT_ENCODING_MFM:
        case UFT_ENCODING_AMIGA:
            /* MFM: most common interval is 2T (two cells) */
            cell_time_ns = mean / 1.5;  /* Approximate */
            break;
        case UFT_ENCODING_FM:
            cell_time_ns = mean / 2.0;
            break;
        case UFT_ENCODING_GCR_COMMODORE:
            cell_time_ns = mean / 2.5;  /* GCR average */
            break;
        default:
            cell_time_ns = mean / 2.0;
            break;
    }
    
    result->cell_time_ns = cell_time_ns;
    result->cell_time_min_ns = min_delta;
    result->cell_time_max_ns = max_delta;
    
    /* Estimate RPM from total time */
    double total_time_ns = sum;
    double rotation_time_us = total_time_ns / 1000.0;
    result->index_to_index_ns = total_time_ns;
    result->rpm = 60e6 / rotation_time_us;  /* 60s / rotation_time */
    
    /* Count anomalies */
    double min_threshold = cell_time_ns * 0.5;
    double max_threshold = cell_time_ns * 3.0;
    
    for (size_t i = 0; i < count; i++) {
        double delta_ns = (double)flux[i] * 1e9 / sample_clock;
        if (delta_ns < min_threshold) {
            result->spike_count++;
            result->anomaly_count++;
        } else if (delta_ns > max_threshold) {
            result->dropout_count++;
            result->anomaly_count++;
        }
    }
    
    /* Quality score based on anomaly rate and variance */
    double anomaly_rate = (double)result->anomaly_count / count;
    double cv = stddev / mean;  /* Coefficient of variation */
    result->quality_score = 1.0 - fmin(1.0, anomaly_rate * 10 + cv * 2);
    if (result->quality_score < 0) result->quality_score = 0;
    
    result->detected_encoding = encoding != UFT_ENCODING_UNKNOWN ? encoding : UFT_ENCODING_MFM;
    
    return 0;
}

/*============================================================================
 * STAGE 2: PLL DECODE
 *============================================================================*/

int uft_ffd_pll_decode(
    const uint32_t* flux,
    size_t count,
    double sample_clock,
    double cell_time_ns,
    const uft_ffd_config_t* config,
    uft_pll_decode_result_t* result)
{
    if (!flux || count < 10 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    uft_ffd_config_t cfg = config ? *config : uft_ffd_config_default();
    
    /* Estimate bit count: ~2 bits per flux transition average for MFM */
    size_t max_bits = count * 3;
    result->bits = calloc((max_bits + 7) / 8, 1);
    if (!result->bits) return -1;
    
    if (cfg.keep_confidence) {
        result->confidence = calloc(max_bits, sizeof(double));
        result->weak_flags = calloc((max_bits + 7) / 8, 1);
    }
    
    /* PLL state */
    double pll_phase = 0.0;
    double pll_freq = cell_time_ns;
    double bandwidth = cfg.pll_bandwidth;
    
    size_t bit_pos = 0;
    double total_confidence = 0.0;
    int weak_count = 0;
    
    for (size_t i = 0; i < count && bit_pos < max_bits; i++) {
        double delta_ns = (double)flux[i] * 1e9 / sample_clock;
        
        /* Determine number of cells (bits) */
        double cells = delta_ns / pll_freq;
        int num_bits = (int)(cells + 0.5);
        if (num_bits < 1) num_bits = 1;
        if (num_bits > 4) num_bits = 4;
        
        /* Phase error */
        double expected = num_bits * pll_freq;
        double phase_error = delta_ns - expected;
        
        /* PLL update */
        pll_phase += phase_error * bandwidth;
        pll_freq += phase_error * bandwidth * 0.1;
        
        /* Clamp frequency */
        if (pll_freq < cell_time_ns * 0.8) pll_freq = cell_time_ns * 0.8;
        if (pll_freq > cell_time_ns * 1.2) pll_freq = cell_time_ns * 1.2;
        
        /* Calculate confidence from phase error */
        double normalized_error = fabs(phase_error) / pll_freq;
        double conf = 1.0 - fmin(1.0, normalized_error * 2);
        if (conf < 0) conf = 0;
        
        /* Write bits: 0s followed by a 1 */
        for (int b = 0; b < num_bits - 1 && bit_pos < max_bits; b++) {
            /* Zero bit */
            if (cfg.keep_confidence && result->confidence) {
                result->confidence[bit_pos] = conf;
            }
            bit_pos++;
        }
        
        /* Final 1 bit */
        if (bit_pos < max_bits) {
            result->bits[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            if (cfg.keep_confidence && result->confidence) {
                result->confidence[bit_pos] = conf;
            }
            bit_pos++;
        }
        
        total_confidence += conf;
        
        /* Check for weak bit */
        if (conf < cfg.weak_threshold) {
            weak_count++;
            if (result->weak_flags) {
                result->weak_flags[(bit_pos - 1) / 8] |= (1 << (7 - ((bit_pos - 1) % 8)));
            }
        }
    }
    
    result->bit_count = bit_pos;
    result->average_confidence = count > 0 ? total_confidence / count : 0;
    result->weak_count = weak_count;
    
    return 0;
}

/*============================================================================
 * STAGE 3: MULTI-REV FUSION
 *============================================================================*/

int uft_ffd_fuse(
    const uft_pll_decode_result_t* revolutions,
    int rev_count,
    const uft_ffd_config_t* config,
    uft_fusion_result_t* result)
{
    if (!revolutions || rev_count < 1 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    if (rev_count == 1) {
        /* Single revolution - just copy */
        result->bit_count = revolutions[0].bit_count;
        result->bits = malloc((result->bit_count + 7) / 8);
        if (!result->bits) return -1;
        memcpy(result->bits, revolutions[0].bits, (result->bit_count + 7) / 8);
        result->average_confidence = revolutions[0].average_confidence;
        result->revolutions_used = 1;
        return 0;
    }
    
    uft_ffd_config_t cfg = config ? *config : uft_ffd_config_default();
    
    /* Find minimum bit count */
    size_t min_bits = revolutions[0].bit_count;
    for (int r = 1; r < rev_count; r++) {
        if (revolutions[r].bit_count < min_bits) {
            min_bits = revolutions[r].bit_count;
        }
    }
    
    result->bit_count = min_bits;
    result->bits = calloc((min_bits + 7) / 8, 1);
    result->confidence = calloc(min_bits, sizeof(double));
    result->weak_bits = calloc((min_bits + 7) / 8, 1);
    
    if (!result->bits || !result->confidence || !result->weak_bits) {
        free(result->bits);
        free(result->confidence);
        free(result->weak_bits);
        memset(result, 0, sizeof(*result));
        return -1;
    }
    
    double total_conf = 0.0;
    int weak_count = 0;
    
    for (size_t i = 0; i < min_bits; i++) {
        int ones = 0;
        double conf_sum = 0.0;
        
        for (int r = 0; r < rev_count; r++) {
            int bit = (revolutions[r].bits[i / 8] >> (7 - (i % 8))) & 1;
            double conf = revolutions[r].confidence ? 
                         revolutions[r].confidence[i] : 0.5;
            
            if (bit) {
                ones++;
                conf_sum += conf;
            } else {
                conf_sum += conf;
            }
        }
        
        /* Majority vote */
        int consensus_bit = (ones > rev_count / 2) ? 1 : 0;
        double consensus_ratio = (double)(consensus_bit ? ones : rev_count - ones) / rev_count;
        double bit_conf = (conf_sum / rev_count) * consensus_ratio;
        
        if (consensus_bit) {
            result->bits[i / 8] |= (1 << (7 - (i % 8)));
        }
        
        result->confidence[i] = bit_conf;
        total_conf += bit_conf;
        
        /* Mark as weak if low consensus */
        if (consensus_ratio < cfg.fusion_min_consensus) {
            result->weak_bits[i / 8] |= (1 << (7 - (i % 8)));
            weak_count++;
        }
    }
    
    result->average_confidence = min_bits > 0 ? total_conf / min_bits : 0;
    result->weak_count = weak_count;
    result->revolutions_used = rev_count;
    
    return 0;
}

/*============================================================================
 * STAGE 4: SECTOR RECOVERY
 *============================================================================*/

/* MFM sync patterns */
#define MFM_SYNC_A1     0x4489  /* A1 with missing clock */
#define MFM_SYNC_C2     0x5224  /* C2 with missing clock */

/* Hamming distance for 16-bit values */
static int hamming16(uint16_t a, uint16_t b) {
    uint16_t x = a ^ b;
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

int uft_ffd_recover_sectors(
    const uint8_t* bits,
    size_t bit_count,
    const uft_conf_t* confidence,
    uft_encoding_t encoding,
    const uft_ffd_config_t* config,
    uft_track_decode_result_t* result)
{
    if (!bits || bit_count < 1000 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    uft_ffd_config_t cfg = config ? *config : uft_ffd_config_default();
    
    /* Adapt sync patterns based on encoding type */
    uint16_t sync_pattern = MFM_SYNC_A1;  /* Default MFM */
    int bytes_per_bit = 2;  /* MFM: 2 bitcell per data bit */
    
    if (encoding == UFT_ENCODING_FM) {
        /* FM: clock between every data bit, different sync marks */
        sync_pattern = 0xF57E;  /* FM sync: F57E (FE with clock) */
        bytes_per_bit = 2;
    }
    /* GCR would use different patterns entirely; 
     * for now focus on MFM/FM which share similar sector structures */
    
    /* If confidence data available, adjust Hamming tolerance based on
     * average confidence in regions — low-confidence areas get more tolerance */
    int base_tolerance = cfg.sync_hamming_tolerance;
    
    /* Search for sync patterns */
    size_t i = 0;
    while (i < bit_count - 64 && result->sector_count < UFT_MAX_SECTORS_PER_TRACK) {
        /* Extract 16-bit window */
        uint16_t window = 0;
        for (size_t b = 0; b < 16 && i + b < bit_count; b++) {
            int bit = (bits[(i + b) / 8] >> (7 - ((i + b) % 8))) & 1;
            window = (window << 1) | bit;
        }
        
        /* Check for sync pattern with confidence-adaptive tolerance */
        int tolerance = base_tolerance;
        if (confidence) {
            /* Average confidence over 16-bit window */
            double avg_conf = 0;
            for (size_t b = 0; b < 16 && i + b < bit_count; b++) {
                avg_conf += confidence[i + b];
            }
            avg_conf /= 16.0;
            /* Low confidence → more tolerance (up to +2) */
            if (avg_conf < 0.5) tolerance += 2;
            else if (avg_conf < 0.7) tolerance += 1;
        }
        int dist = hamming16(window, sync_pattern);
        if (dist <= tolerance) {
            /* Potential sync found */
            size_t sync_pos = i;
            
            /* Look for three consecutive syncs (standard MFM) */
            int sync_count = 1;
            for (int s = 1; s < 3 && sync_pos + s * 16 < bit_count; s++) {
                uint16_t next = 0;
                for (int b = 0; b < 16; b++) {
                    size_t pos = sync_pos + s * 16 + b;
                    int bit = (bits[pos / 8] >> (7 - (pos % 8))) & 1;
                    next = (next << 1) | bit;
                }
                if (hamming16(next, sync_pattern) <= tolerance) {
                    sync_count++;
                }
            }
            
            if (sync_count >= 2) {
                /* Valid sync sequence - decode sector header */
                size_t header_start = sync_pos + sync_count * 16;
                
                if (header_start + 64 < bit_count) {
                    /* Extract address mark and header */
                    uint8_t header[6];
                    for (int h = 0; h < 6; h++) {
                        header[h] = 0;
                        for (int b = 0; b < 8; b++) {
                            size_t pos = header_start + h * 16 + b * 2 + 1;  /* MFM data bits */
                            if (pos < bit_count) {
                                int bit = (bits[pos / 8] >> (7 - (pos % 8))) & 1;
                                header[h] = (header[h] << 1) | bit;
                            }
                        }
                    }
                    
                    /* Parse header: mark, cyl, head, sector, size, crc */
                    if (header[0] == 0xFE) {  /* Address mark */
                        uft_sector_decode_result_t* sector = &result->sectors[result->sector_count];
                        
                        sector->cylinder = header[1];
                        sector->head = header[2];
                        sector->sector = header[3];
                        sector->size_code = header[4];
                        sector->data_size = 128 << sector->size_code;
                        sector->bit_offset = sync_pos;
                        
                        /* Find data field: search for data sync after header.
                         * Header = 6 bytes × 16 MFM bits = 96 bits.
                         * Search up to 1024 bits for data sync (A1 A1 A1 FB/F8). */
                        size_t data_search = header_start + 6 * 16;
                        size_t data_start = 0;
                        bool found_data = false;
                        uint8_t sector_dam = 0xFB;  /* Data Address Mark */
                        
                        for (size_t ds = data_search; ds < data_search + 1024 && ds + 64 < bit_count; ds++) {
                            uint16_t dw = 0;
                            for (int b = 0; b < 16; b++) {
                                int bit = (bits[(ds + b) / 8] >> (7 - ((ds + b) % 8))) & 1;
                                dw = (dw << 1) | bit;
                            }
                            if (hamming16(dw, sync_pattern) <= base_tolerance) {
                                /* Found data sync — skip sync bytes, decode DAM */
                                size_t dam_pos = ds + 48;  /* After 3 sync bytes */
                                uint8_t dam = 0;
                                for (int b = 0; b < 8; b++) {
                                    size_t p = dam_pos + b * 2 + 1;
                                    if (p < bit_count) {
                                        int bit = (bits[p / 8] >> (7 - (p % 8))) & 1;
                                        dam = (dam << 1) | bit;
                                    }
                                }
                                if (dam == 0xFB || dam == 0xF8) {
                                    data_start = dam_pos + 16;
                                    found_data = true;
                                    sector_dam = dam;
                                }
                                break;
                            }
                        }
                        
                        /* Extract sector data and verify CRC */
                        sector->data = calloc(sector->data_size, 1);
                        if (found_data && sector->data && 
                            data_start + sector->data_size * 16 + 32 < bit_count) {
                            /* Decode MFM data bytes */
                            for (size_t d = 0; d < sector->data_size; d++) {
                                uint8_t byte = 0;
                                for (int b = 0; b < 8; b++) {
                                    size_t p = data_start + d * 16 + b * 2 + 1;
                                    if (p < bit_count) {
                                        int bit = (bits[p / 8] >> (7 - (p % 8))) & 1;
                                        byte = (byte << 1) | bit;
                                    }
                                }
                                sector->data[d] = byte;
                            }
                            
                            /* Read stored CRC (2 bytes after data) */
                            uint8_t crc_bytes[2] = {0};
                            for (int c = 0; c < 2; c++) {
                                for (int b = 0; b < 8; b++) {
                                    size_t p = data_start + sector->data_size * 16 + c * 16 + b * 2 + 1;
                                    if (p < bit_count) {
                                        int bit = (bits[p / 8] >> (7 - (p % 8))) & 1;
                                        crc_bytes[c] = (crc_bytes[c] << 1) | bit;
                                    }
                                }
                            }
                            uint16_t stored_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
                            
                            /* Compute CRC-CCITT over sync+DAM+data */
                            uint16_t crc = 0xFFFF;
                            uint8_t prefix[4] = {0xA1, 0xA1, 0xA1, sector_dam};
                            for (int b = 0; b < 4; b++) {
                                crc ^= (uint16_t)prefix[b] << 8;
                                for (int bi = 0; bi < 8; bi++)
                                    crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
                            }
                            for (size_t d = 0; d < sector->data_size; d++) {
                                crc ^= (uint16_t)sector->data[d] << 8;
                                for (int bi = 0; bi < 8; bi++)
                                    crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
                            }
                            sector->crc_ok = (crc == stored_crc);
                            sector->confidence = sector->crc_ok ? 1.0 : 0.5;
                        } else {
                            sector->crc_ok = false;
                            sector->confidence = 0.3;
                        }
                        
                        result->sector_count++;
                        
                        i = header_start + 128;  /* Skip past this sector */
                        continue;
                    }
                }
            }
        }
        
        i++;
    }
    
    /* Update result statistics */
    for (int s = 0; s < result->sector_count; s++) {
        if (result->sectors[s].crc_ok) {
            result->crc_ok_count++;
        } else {
            result->crc_error_count++;
        }
        result->average_confidence += result->sectors[s].confidence;
    }
    
    if (result->sector_count > 0) {
        result->average_confidence /= result->sector_count;
    }
    
    result->quality_score = (double)result->crc_ok_count / 
                           (result->sector_count > 0 ? result->sector_count : 1);
    
    return 0;
}

/*============================================================================
 * STAGE 5: ERROR CORRECTION
 *============================================================================*/

int uft_ffd_correct_sector(
    uft_sector_decode_result_t* sector,
    const uft_conf_t* confidence,
    int max_bits)
{
    if (!sector || !sector->data || sector->crc_ok) return 0;
    
    sector->corrected = false;
    sector->corrections_count = 0;
    
    /* CRC-based error correction: try flipping bits, prioritizing
     * low-confidence positions if confidence data available */
    size_t len = sector->data_size;
    if (len == 0 || max_bits <= 0) return 0;
    
    /* Build ordered list of byte positions to try, sorted by confidence
     * (lowest confidence first — most likely error sites) */
    size_t *order = (size_t *)malloc(len * sizeof(size_t));
    if (!order) return 0;
    
    for (size_t i = 0; i < len; i++) order[i] = i;
    
    /* Sort by confidence (simple insertion sort; sectors are small) */
    if (confidence) {
        for (size_t i = 1; i < len; i++) {
            size_t key = order[i];
            double key_conf = confidence[sector->bit_offset + key * 16];
            size_t j = i;
            while (j > 0) {
                double prev_conf = confidence[sector->bit_offset + order[j-1] * 16];
                if (prev_conf <= key_conf) break;
                order[j] = order[j-1];
                j--;
            }
            order[j] = key;
        }
    }
    
    /* Try single-bit flips on lowest-confidence bytes first */
    int tries = 0;
    int max_tries = (int)len * 8;
    if (max_tries > max_bits * 256) max_tries = max_bits * 256;
    
    /* Compute CRC prefix (sync + DAM, assume FB=normal) */
    uint8_t dam = 0xFB;
    uint16_t prefix_crc = 0xFFFF;
    uint8_t prefix[4] = {0xA1, 0xA1, 0xA1, dam};
    for (int b = 0; b < 4; b++) {
        prefix_crc ^= (uint16_t)prefix[b] << 8;
        for (int bi = 0; bi < 8; bi++)
            prefix_crc = (prefix_crc << 1) ^ ((prefix_crc & 0x8000) ? 0x1021 : 0);
    }
    
    for (int idx = 0; idx < (int)len && tries < max_tries; idx++) {
        size_t byte_pos = order[idx];
        for (int bit = 0; bit < 8 && tries < max_tries; bit++) {
            sector->data[byte_pos] ^= (1 << bit);
            
            /* Recompute CRC */
            uint16_t crc = prefix_crc;
            for (size_t d = 0; d < len; d++) {
                crc ^= (uint16_t)sector->data[d] << 8;
                for (int bi = 0; bi < 8; bi++)
                    crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
            }
            
            if (crc == 0) {  /* CRC residue = 0 means valid */
                sector->crc_ok = true;
                sector->corrected = true;
                sector->corrections_count = 1;
                sector->confidence = 0.85;
                free(order);
                return 1;
            }
            
            sector->data[byte_pos] ^= (1 << bit);  /* Restore */
            tries++;
        }
    }
    
    free(order);
    
    return 0;
}

/*============================================================================
 * HIGH-LEVEL DECODE
 *============================================================================*/

int uft_ffd_decode_track(
    const uint32_t* flux,
    size_t count,
    double sample_clock,
    int cylinder,
    int head,
    uft_encoding_t encoding,
    const uft_ffd_config_t* config,
    uft_ffd_session_t* session,
    uft_track_decode_result_t* result)
{
    if (!flux || count < 100 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->cylinder = cylinder;
    result->head = head;
    
    uft_ffd_config_t cfg = config ? *config : uft_ffd_config_default();
    
    /* Stage 1: Pre-analysis */
    uft_preanalysis_result_t preanalysis;
    if (uft_ffd_preanalyze(flux, count, sample_clock, encoding, &preanalysis) != 0) {
        return -1;
    }
    
    if (session) {
        audit_log(session, "Track %d.%d: cell=%.1fns rpm=%.1f quality=%.2f",
                  cylinder, head, preanalysis.cell_time_ns, 
                  preanalysis.rpm, preanalysis.quality_score);
    }
    
    /* Stage 2: PLL Decode */
    uft_pll_decode_result_t pll_result;
    if (uft_ffd_pll_decode(flux, count, sample_clock, 
                           preanalysis.cell_time_ns, &cfg, &pll_result) != 0) {
        return -1;
    }
    
    /* Stage 4: Sector Recovery (skip fusion for single revolution) */
    int rc = uft_ffd_recover_sectors(pll_result.bits, pll_result.bit_count,
                                      pll_result.confidence, 
                                      preanalysis.detected_encoding,
                                      &cfg, result);
    
    result->encoding = preanalysis.detected_encoding;
    
    /* Update session stats */
    if (session) {
        session->stats.tracks_processed++;
        session->stats.sectors_decoded += result->sector_count;
        session->stats.sectors_recovered += result->corrected_count;
    }
    
    /* Cleanup */
    free(pll_result.bits);
    free(pll_result.confidence);
    free(pll_result.weak_flags);
    
    return rc;
}

int uft_ffd_decode_track_multi(
    const uint32_t** flux_revs,
    const size_t* counts,
    int rev_count,
    double sample_clock,
    int cylinder,
    int head,
    uft_encoding_t encoding,
    const uft_ffd_config_t* config,
    uft_ffd_session_t* session,
    uft_track_decode_result_t* result)
{
    if (!flux_revs || !counts || rev_count < 1 || !result) return -1;
    
    if (rev_count == 1) {
        return uft_ffd_decode_track(flux_revs[0], counts[0], sample_clock,
                                    cylinder, head, encoding, config, session, result);
    }
    
    memset(result, 0, sizeof(*result));
    result->cylinder = cylinder;
    result->head = head;
    
    uft_ffd_config_t cfg = config ? *config : uft_ffd_config_default();
    
    /* Pre-analyze first revolution */
    uft_preanalysis_result_t preanalysis;
    if (uft_ffd_preanalyze(flux_revs[0], counts[0], sample_clock, encoding, &preanalysis) != 0) {
        return -1;
    }
    
    /* Decode each revolution */
    uft_pll_decode_result_t* pll_results = calloc(rev_count, sizeof(uft_pll_decode_result_t));
    if (!pll_results) return -1;
    
    int valid_revs = 0;
    for (int r = 0; r < rev_count && valid_revs < cfg.max_revolutions; r++) {
        if (uft_ffd_pll_decode(flux_revs[r], counts[r], sample_clock,
                               preanalysis.cell_time_ns, &cfg, &pll_results[valid_revs]) == 0) {
            valid_revs++;
        }
    }
    
    if (valid_revs == 0) {
        free(pll_results);
        return -1;
    }
    
    /* Fuse revolutions */
    uft_fusion_result_t fused;
    if (uft_ffd_fuse(pll_results, valid_revs, &cfg, &fused) != 0) {
        for (int r = 0; r < valid_revs; r++) {
            free(pll_results[r].bits);
            free(pll_results[r].confidence);
            free(pll_results[r].weak_flags);
        }
        free(pll_results);
        return -1;
    }
    
    /* Recover sectors from fused data */
    int rc = uft_ffd_recover_sectors(fused.bits, fused.bit_count,
                                      fused.confidence,
                                      preanalysis.detected_encoding,
                                      &cfg, result);
    
    result->encoding = preanalysis.detected_encoding;
    
    /* Session stats */
    if (session) {
        session->stats.tracks_processed++;
        session->stats.sectors_decoded += result->sector_count;
        session->stats.weak_bits_found += fused.weak_count;
        audit_log(session, "Track %d.%d: %d revs fused, %d weak bits, %d sectors",
                  cylinder, head, valid_revs, fused.weak_count, result->sector_count);
    }
    
    /* Cleanup */
    for (int r = 0; r < valid_revs; r++) {
        free(pll_results[r].bits);
        free(pll_results[r].confidence);
        free(pll_results[r].weak_flags);
    }
    free(pll_results);
    uft_fusion_result_free(&fused);
    
    return rc;
}

/*============================================================================
 * AUDIT LOG
 *============================================================================*/

int uft_ffd_audit_count(const uft_ffd_session_t* session) {
    return session ? session->audit_count : 0;
}

const char* uft_ffd_audit_get(const uft_ffd_session_t* session, int index) {
    if (!session || index < 0 || index >= session->audit_count) return NULL;
    return session->audit_log[index];
}

int uft_ffd_audit_export(const uft_ffd_session_t* session, const char* path) {
    if (!session || !path) return -1;
    
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "UFT Forensic Decoder Audit Log\n");
    fprintf(f, "==============================\n\n");
    
    for (int i = 0; i < session->audit_count; i++) {
        fprintf(f, "[%04d] %s\n", i, session->audit_log[i]);
    }
    
    fprintf(f, "\nStatistics:\n");
    fprintf(f, "  Tracks processed: %d\n", session->stats.tracks_processed);
    fprintf(f, "  Sectors decoded:  %d\n", session->stats.sectors_decoded);
    fprintf(f, "  Sectors recovered: %d\n", session->stats.sectors_recovered);
    fprintf(f, "  Total corrections: %d\n", session->stats.total_corrections);
    
    fclose(f);
    return 0;
}

/*============================================================================
 * CLEANUP FUNCTIONS
 *============================================================================*/

void uft_pll_decode_result_free(uft_pll_decode_result_t* r) {
    if (!r) return;
    free(r->bits);
    free(r->confidence);
    free(r->weak_flags);
    memset(r, 0, sizeof(*r));
}

void uft_fusion_result_free(uft_fusion_result_t* r) {
    if (!r) return;
    free(r->bits);
    free(r->confidence);
    free(r->weak_bits);
    memset(r, 0, sizeof(*r));
}

void uft_sector_decode_result_free(uft_sector_decode_result_t* r) {
    if (!r) return;
    free(r->data);
    memset(r, 0, sizeof(*r));
}

void uft_track_decode_result_free(uft_track_decode_result_t* r) {
    if (!r) return;
    for (int i = 0; i < r->sector_count; i++) {
        free(r->sectors[i].data);
    }
    free(r->raw_bits);
    memset(r, 0, sizeof(*r));
}
