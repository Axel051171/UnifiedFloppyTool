/**
 * @file uft_recovery.c
 * @brief Disk Recovery Techniques Implementation
 * 
 * @copyright UFT Project 2026
 */

#include "uft_recovery.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Multi-Revolution Analysis
 *============================================================================*/

uint32_t uft_analyze_revolutions(
    const uint8_t **revolutions,
    size_t rev_count,
    size_t bit_count,
    uint8_t *consensus,
    uint8_t *weak_mask,
    uint8_t *confidence)
{
    if (!revolutions || rev_count < 2 || !consensus) {
        return 0;
    }
    
    uint32_t weak_count = 0;
    size_t byte_count = (bit_count + 7) / 8;
    
    /* Clear outputs */
    memset(consensus, 0, byte_count);
    if (weak_mask) memset(weak_mask, 0, byte_count);
    if (confidence) memset(confidence, 0xFF, byte_count);
    
    /* Analyze each bit position */
    for (size_t bit = 0; bit < bit_count; bit++) {
        size_t byte_idx = bit / 8;
        uint8_t bit_mask = 0x80 >> (bit % 8);
        
        /* Count ones across all revolutions */
        size_t ones = 0;
        for (size_t rev = 0; rev < rev_count; rev++) {
            if (revolutions[rev][byte_idx] & bit_mask) {
                ones++;
            }
        }
        
        /* Determine consensus value */
        bool is_one = (ones > rev_count / 2);
        if (is_one) {
            consensus[byte_idx] |= bit_mask;
        }
        
        /* Determine if bit is weak (not unanimous) */
        bool is_weak = (ones > 0 && ones < rev_count);
        if (is_weak) {
            weak_count++;
            if (weak_mask) {
                weak_mask[byte_idx] |= bit_mask;
            }
        }
        
        /* Calculate confidence */
        if (confidence) {
            /* Scale to 0-255: 255 = unanimous, 128 = split */
            size_t agreement = is_one ? ones : (rev_count - ones);
            confidence[byte_idx] = (uint8_t)((agreement * 255) / rev_count);
        }
    }
    
    return weak_count;
}

/*============================================================================
 * CRC Recovery
 *============================================================================*/

/* Simple CRC-16-CCITT for demonstration */
static uint16_t calc_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

bool uft_fix_crc_single_bit(
    uint8_t *data,
    size_t size,
    uint16_t expected_crc,
    int *fixed_bit)
{
    if (!data || size == 0) {
        if (fixed_bit) *fixed_bit = -1;
        return false;
    }
    
    /* Try flipping each bit */
    for (size_t byte_idx = 0; byte_idx < size; byte_idx++) {
        for (int bit = 0; bit < 8; bit++) {
            uint8_t mask = 1 << bit;
            
            /* Flip bit */
            data[byte_idx] ^= mask;
            
            /* Check CRC */
            uint16_t crc = calc_crc16(data, size);
            if (crc == expected_crc) {
                if (fixed_bit) *fixed_bit = (int)(byte_idx * 8 + (7 - bit));
                return true;
            }
            
            /* Restore bit */
            data[byte_idx] ^= mask;
        }
    }
    
    if (fixed_bit) *fixed_bit = -1;
    return false;
}

bool uft_fix_crc_weak_bits(
    uint8_t *data,
    size_t size,
    const uint8_t *weak_mask,
    uint16_t expected_crc)
{
    if (!data || !weak_mask || size == 0) {
        return false;
    }
    
    /* Count weak bits */
    size_t weak_count = 0;
    for (size_t i = 0; i < size; i++) {
        for (int bit = 0; bit < 8; bit++) {
            if (weak_mask[i] & (1 << bit)) {
                weak_count++;
            }
        }
    }
    
    /* Too many weak bits - combinatorial explosion */
    if (weak_count > 16) {
        return false;
    }
    
    /* Try all combinations of weak bits */
    uint32_t combinations = 1U << weak_count;
    
    for (uint32_t combo = 0; combo < combinations; combo++) {
        /* Apply this combination */
        size_t weak_idx = 0;
        for (size_t byte_idx = 0; byte_idx < size; byte_idx++) {
            for (int bit = 7; bit >= 0; bit--) {
                if (weak_mask[byte_idx] & (1 << bit)) {
                    if (combo & (1U << weak_idx)) {
                        data[byte_idx] ^= (1 << bit);
                    }
                    weak_idx++;
                }
            }
        }
        
        /* Check CRC */
        uint16_t crc = calc_crc16(data, size);
        if (crc == expected_crc) {
            return true;
        }
        
        /* Restore original */
        weak_idx = 0;
        for (size_t byte_idx = 0; byte_idx < size; byte_idx++) {
            for (int bit = 7; bit >= 0; bit--) {
                if (weak_mask[byte_idx] & (1 << bit)) {
                    if (combo & (1U << weak_idx)) {
                        data[byte_idx] ^= (1 << bit);
                    }
                    weak_idx++;
                }
            }
        }
    }
    
    return false;
}

/*============================================================================
 * Sector Interpolation
 *============================================================================*/

uint8_t uft_interpolate_sector(
    const uint8_t *prev_sector,
    const uint8_t *next_sector,
    size_t sector_size,
    uint8_t *output)
{
    if (!output || sector_size == 0) {
        return 0;
    }
    
    /* No neighbors - fill with pattern */
    if (!prev_sector && !next_sector) {
        memset(output, 0xE5, sector_size);  /* Uninitialized pattern */
        return 0;
    }
    
    /* Only previous sector available */
    if (prev_sector && !next_sector) {
        memcpy(output, prev_sector, sector_size);
        return 30;  /* Low confidence */
    }
    
    /* Only next sector available */
    if (!prev_sector && next_sector) {
        memcpy(output, next_sector, sector_size);
        return 30;
    }
    
    /* Both neighbors available - interpolate */
    for (size_t i = 0; i < sector_size; i++) {
        /* Simple average */
        output[i] = (uint8_t)((prev_sector[i] + next_sector[i]) / 2);
    }
    
    return 50;  /* Medium confidence */
}

/*============================================================================
 * Error Map
 *============================================================================*/

uft_error_map_t* uft_error_map_create(size_t initial_capacity)
{
    uft_error_map_t *map = calloc(1, sizeof(uft_error_map_t));
    if (!map) return NULL;
    
    if (initial_capacity > 0) {
        map->entries = calloc(initial_capacity, sizeof(uft_error_entry_t));
        if (!map->entries) {
            free(map);
            return NULL;
        }
        map->capacity = initial_capacity;
    }
    
    return map;
}

void uft_error_map_free(uft_error_map_t *map)
{
    if (map) {
        free(map->entries);
        free(map);
    }
}

int uft_error_map_add(uft_error_map_t *map, const uft_error_entry_t *entry)
{
    if (!map || !entry) return -1;
    
    /* Grow if needed */
    if (map->count >= map->capacity) {
        size_t new_cap = map->capacity ? map->capacity * 2 : 64;
        uft_error_entry_t *new_entries = realloc(map->entries, 
            new_cap * sizeof(uft_error_entry_t));
        if (!new_entries) return -1;
        map->entries = new_entries;
        map->capacity = new_cap;
    }
    
    /* Add entry */
    map->entries[map->count++] = *entry;
    
    /* Update statistics */
    map->total_sectors++;
    switch (entry->status) {
        case UFT_REC_OK:
            map->good_sectors++;
            break;
        case UFT_REC_PARTIAL:
        case UFT_REC_CRC_ERROR:
        case UFT_REC_WEAK:
            map->partial_sectors++;
            break;
        default:
            map->failed_sectors++;
            break;
    }
    
    return 0;
}

static const char* status_name(uft_recovery_status_t status)
{
    switch (status) {
        case UFT_REC_OK:         return "OK";
        case UFT_REC_PARTIAL:    return "PARTIAL";
        case UFT_REC_CRC_ERROR:  return "CRC_ERROR";
        case UFT_REC_WEAK:       return "WEAK";
        case UFT_REC_UNREADABLE: return "UNREADABLE";
        case UFT_REC_NO_SYNC:    return "NO_SYNC";
        case UFT_REC_NO_HEADER:  return "NO_HEADER";
        case UFT_REC_NO_DATA:    return "NO_DATA";
        case UFT_REC_TIMEOUT:    return "TIMEOUT";
        case UFT_REC_IO_ERROR:   return "IO_ERROR";
        default:                 return "UNKNOWN";
    }
}

int uft_error_map_report(const uft_error_map_t *map, char *buffer, size_t buf_size)
{
    if (!map || !buffer || buf_size < 256) return -1;
    
    int written = 0;
    
    /* Header */
    written += snprintf(buffer + written, buf_size - written,
        "=== Error Map Report ===\n"
        "Total sectors: %u\n"
        "Good:          %u (%.1f%%)\n"
        "Partial:       %u (%.1f%%)\n"
        "Failed:        %u (%.1f%%)\n\n",
        map->total_sectors,
        map->good_sectors, 
        map->total_sectors ? (100.0 * map->good_sectors / map->total_sectors) : 0,
        map->partial_sectors,
        map->total_sectors ? (100.0 * map->partial_sectors / map->total_sectors) : 0,
        map->failed_sectors,
        map->total_sectors ? (100.0 * map->failed_sectors / map->total_sectors) : 0);
    
    /* Detail for non-good sectors */
    if (map->partial_sectors + map->failed_sectors > 0) {
        written += snprintf(buffer + written, buf_size - written,
            "Problem sectors:\n"
            "Track Head Sector Status       Attempts WeakBits\n"
            "----- ---- ------ ------------ -------- --------\n");
        
        for (size_t i = 0; i < map->count && written < (int)buf_size - 80; i++) {
            const uft_error_entry_t *e = &map->entries[i];
            if (e->status != UFT_REC_OK) {
                written += snprintf(buffer + written, buf_size - written,
                    "%5u %4u %6u %-12s %8u %8u\n",
                    e->track, e->head, e->sector,
                    status_name(e->status),
                    e->attempt_count, e->weak_bits);
            }
        }
    }
    
    return written;
}
