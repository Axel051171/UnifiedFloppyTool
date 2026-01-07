/**
 * @file uft_partial_recovery.c
 * @brief Partial sector and error recovery implementation
 */

#include "uft_partial_recovery.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Sector Management
 * ============================================================================ */

int uft_partial_init(uft_partial_sector_t *sector, size_t max_size) {
    if (!sector || max_size == 0 || max_size > UFT_SECTOR_MAX_SIZE) {
        return -1;
    }
    
    memset(sector, 0, sizeof(*sector));
    
    sector->data = calloc(max_size, 1);
    sector->byte_status = calloc(max_size, sizeof(uft_byte_status_t));
    
    if (!sector->data || !sector->byte_status) {
        uft_partial_free(sector);
        return -2;
    }
    
    sector->data_len = max_size;
    sector->first_error_pos = max_size;  /* No error yet */
    
    return 0;
}

void uft_partial_free(uft_partial_sector_t *sector) {
    if (!sector) return;
    
    free(sector->data);
    free(sector->byte_status);
    
    for (size_t i = 0; i < sector->revision_count; i++) {
        free(sector->revisions[i].data);
        free(sector->revisions[i].confidence);
    }
    
    memset(sector, 0, sizeof(*sector));
}

void uft_partial_reset(uft_partial_sector_t *sector) {
    if (!sector) return;
    
    /* Keep allocated buffers, reset state */
    size_t max_size = sector->data_len;
    uint8_t *data = sector->data;
    uft_byte_status_t *status = sector->byte_status;
    
    /* Free revision data */
    for (size_t i = 0; i < sector->revision_count; i++) {
        free(sector->revisions[i].data);
        free(sector->revisions[i].confidence);
    }
    
    memset(sector, 0, sizeof(*sector));
    
    sector->data = data;
    sector->byte_status = status;
    sector->data_len = max_size;
    sector->first_error_pos = max_size;
    
    memset(data, 0, max_size);
    memset(status, 0, max_size * sizeof(uft_byte_status_t));
}

/* ============================================================================
 * Data Addition
 * ============================================================================ */

void uft_partial_set_header(uft_partial_sector_t *sector,
                            const uft_sector_header_t *header) {
    if (!sector || !header) return;
    
    sector->header = *header;
    sector->header_valid = header->crc_valid;
    
    /* Update expected data length from size code */
    size_t expected = 128 << header->size_code;
    if (expected > sector->data_len) {
        expected = sector->data_len;
    }
}

int uft_partial_add_revision(uft_partial_sector_t *sector,
                             const uint8_t *data,
                             const uint8_t *confidence,
                             size_t len,
                             uint16_t crc_calc,
                             uint16_t crc_stored) {
    if (!sector || !data || len == 0) return -1;
    if (sector->revision_count >= UFT_MAX_REVISIONS) return -2;
    
    size_t idx = sector->revision_count;
    uft_sector_revision_t *rev = &sector->revisions[idx];
    
    /* Allocate and copy data */
    rev->data = malloc(len);
    rev->confidence = malloc(len);
    if (!rev->data || !rev->confidence) {
        free(rev->data);
        free(rev->confidence);
        return -3;
    }
    
    memcpy(rev->data, data, len);
    
    if (confidence) {
        memcpy(rev->confidence, confidence, len);
    } else {
        /* Default confidence based on CRC */
        uint8_t default_conf = (crc_calc == crc_stored) ? 255 : 128;
        memset(rev->confidence, default_conf, len);
    }
    
    rev->data_len = len;
    rev->crc_calc = crc_calc;
    rev->crc_stored = crc_stored;
    rev->crc_valid = (crc_calc == crc_stored);
    
    sector->revision_count++;
    
    /* If this revision has valid CRC, mark as complete */
    if (rev->crc_valid && !sector->data_crc_valid) {
        memcpy(sector->data, data, len);
        sector->data_crc_valid = true;
        sector->data_complete = true;
    }
    
    return (int)idx;
}

/* ============================================================================
 * Data Fusion
 * ============================================================================ */

bool uft_partial_fuse(uft_partial_sector_t *sector) {
    if (!sector || sector->revision_count == 0) return false;
    
    /* Already have valid CRC? */
    if (sector->data_crc_valid) return true;
    
    size_t len = sector->revisions[0].data_len;
    
    /* For each byte position */
    for (size_t i = 0; i < len; i++) {
        /* Weighted voting across revisions */
        int votes[256] = {0};
        int total_weight = 0;
        uint8_t best_value = 0;
        int best_votes = 0;
        
        for (size_t r = 0; r < sector->revision_count; r++) {
            uft_sector_revision_t *rev = &sector->revisions[r];
            if (i >= rev->data_len) continue;
            
            uint8_t value = rev->data[i];
            uint8_t conf = rev->confidence[i];
            
            /* Higher weight for valid CRC revisions */
            int weight = conf;
            if (rev->crc_valid) weight += 100;
            
            votes[value] += weight;
            total_weight += weight;
            
            if (votes[value] > best_votes) {
                best_votes = votes[value];
                best_value = value;
            }
        }
        
        /* Set fused value */
        sector->data[i] = best_value;
        
        /* Calculate confidence */
        uft_byte_status_t *status = &sector->byte_status[i];
        status->value = best_value;
        status->confidence = (total_weight > 0) ?
            (uint8_t)(best_votes * 255 / total_weight) : 0;
        
        /* Check consistency across revisions */
        bool all_same = true;
        for (size_t r = 1; r < sector->revision_count; r++) {
            if (i < sector->revisions[r].data_len &&
                sector->revisions[r].data[i] != sector->revisions[0].data[i]) {
                all_same = false;
                break;
            }
        }
        status->is_weak = !all_same;
        status->is_error = (status->confidence < 128);
        
        /* Build revision mask */
        status->revision_mask = 0;
        for (size_t r = 0; r < sector->revision_count && r < 8; r++) {
            if (i < sector->revisions[r].data_len) {
                status->revision_mask |= (1 << r);
            }
        }
        
        /* Update error tracking */
        if (status->is_error && i < sector->first_error_pos) {
            sector->first_error_pos = i;
        }
        if (status->is_weak) sector->weak_bytes++;
        if (status->is_error) sector->error_bytes++;
        else sector->valid_bytes++;
    }
    
    sector->data_complete = true;
    
    /* TODO: Recalculate CRC on fused data */
    
    return sector->data_crc_valid;
}

size_t uft_partial_correct(uft_partial_sector_t *sector) {
    if (!sector) return 0;
    
    size_t corrected = 0;
    
    /* Simple correction: if one revision disagrees and has lower confidence,
     * use the majority value */
    for (size_t i = 0; i < sector->data_len; i++) {
        if (!sector->byte_status[i].is_error) continue;
        
        /* Already handled in fusion, but could add ECC here */
        /* For now, just count what fusion corrected */
        if (sector->byte_status[i].confidence > 128) {
            corrected++;
        }
    }
    
    return corrected;
}

bool uft_partial_fix_crc(uft_partial_sector_t *sector, int max_bits) {
    if (!sector || !sector->data) return false;
    if (sector->data_crc_valid) return true;
    
    /* This is a brute-force approach - try flipping bits */
    /* Only practical for small max_bits (1-2) */
    
    if (max_bits > 2) max_bits = 2;  /* Limit for performance */
    
    size_t len = sector->data_len;
    
    /* Try single bit flips */
    for (size_t byte_idx = 0; byte_idx < len && max_bits >= 1; byte_idx++) {
        for (int bit = 0; bit < 8; bit++) {
            uint8_t original = sector->data[byte_idx];
            sector->data[byte_idx] ^= (1 << bit);
            
            /* TODO: Recalculate CRC and check */
            /* uint16_t new_crc = crc16_calc(sector->data, len); */
            
            /* Restore for now */
            sector->data[byte_idx] = original;
        }
    }
    
    return sector->data_crc_valid;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

double uft_partial_get_recovery_rate(const uft_partial_sector_t *sector) {
    if (!sector || sector->data_len == 0) return 0;
    
    return (double)sector->valid_bytes / sector->data_len;
}

bool uft_partial_get_byte(const uft_partial_sector_t *sector,
                          size_t pos,
                          uint8_t *value,
                          uint8_t *confidence) {
    if (!sector || pos >= sector->data_len) return false;
    
    if (value) *value = sector->data[pos];
    if (confidence) *confidence = sector->byte_status[pos].confidence;
    
    return true;
}

bool uft_partial_get_valid_range(const uft_partial_sector_t *sector,
                                 size_t *start,
                                 size_t *length) {
    if (!sector) return false;
    
    /* Find longest contiguous valid range */
    size_t best_start = 0;
    size_t best_len = 0;
    size_t curr_start = 0;
    size_t curr_len = 0;
    bool in_valid = false;
    
    for (size_t i = 0; i < sector->data_len; i++) {
        bool valid = !sector->byte_status[i].is_error;
        
        if (valid) {
            if (!in_valid) {
                curr_start = i;
                curr_len = 0;
                in_valid = true;
            }
            curr_len++;
        } else {
            if (in_valid) {
                if (curr_len > best_len) {
                    best_start = curr_start;
                    best_len = curr_len;
                }
                in_valid = false;
            }
        }
    }
    
    /* Check final range */
    if (in_valid && curr_len > best_len) {
        best_start = curr_start;
        best_len = curr_len;
    }
    
    if (start) *start = best_start;
    if (length) *length = best_len;
    
    return best_len > 0;
}

/* ============================================================================
 * Forensic Export
 * ============================================================================ */

size_t uft_partial_export(const uft_partial_sector_t *sector,
                          uint8_t *data,
                          uint8_t *error_map,
                          size_t len) {
    if (!sector || !data) return 0;
    
    size_t copy_len = (len < sector->data_len) ? len : sector->data_len;
    
    memcpy(data, sector->data, copy_len);
    
    if (error_map) {
        for (size_t i = 0; i < copy_len; i++) {
            error_map[i] = sector->byte_status[i].is_error ? 1 : 0;
        }
    }
    
    return copy_len;
}

void uft_partial_dump(const uft_partial_sector_t *sector) {
    if (!sector) {
        printf("Partial Sector: NULL\n");
        return;
    }
    
    printf("=== Partial Sector ===\n");
    
    if (sector->header_valid) {
        printf("Header: C=%u H=%u R=%u N=%u CRC=%s\n",
               sector->header.cylinder,
               sector->header.head,
               sector->header.sector,
               sector->header.size_code,
               sector->header.crc_valid ? "OK" : "BAD");
    } else {
        printf("Header: INVALID\n");
    }
    
    printf("Data: %zu bytes, complete=%s, CRC=%s\n",
           sector->data_len,
           sector->data_complete ? "yes" : "no",
           sector->data_crc_valid ? "OK" : "BAD");
    
    printf("Bytes: valid=%zu weak=%zu error=%zu\n",
           sector->valid_bytes, sector->weak_bytes, sector->error_bytes);
    
    if (sector->first_error_pos < sector->data_len) {
        printf("First error at byte %zu\n", sector->first_error_pos);
    }
    
    printf("Revisions: %zu\n", sector->revision_count);
    for (size_t i = 0; i < sector->revision_count; i++) {
        const uft_sector_revision_t *rev = &sector->revisions[i];
        printf("  [%zu] %zu bytes, CRC %s\n",
               i, rev->data_len,
               rev->crc_valid ? "OK" : "BAD");
    }
    
    double rate = uft_partial_get_recovery_rate(sector);
    printf("Recovery rate: %.1f%%\n", rate * 100);
}

void uft_recovery_stats_dump(const uft_recovery_stats_t *stats) {
    if (!stats) return;
    
    printf("=== Recovery Statistics ===\n");
    printf("Sectors: %zu total, %zu full, %zu partial, %zu failed\n",
           stats->total_sectors,
           stats->fully_recovered,
           stats->partially_recovered,
           stats->unrecoverable);
    printf("Bytes: %zu total, %zu recovered (%.1f%%)\n",
           stats->total_bytes,
           stats->recovered_bytes,
           stats->recovery_rate * 100);
    printf("CRC fixes: %zu, Weak bits resolved: %zu\n",
           stats->crc_fixed_count,
           stats->weak_bits_resolved);
}
