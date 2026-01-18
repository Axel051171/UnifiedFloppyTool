/**
 * @file sector_search.h
 * @brief Sector Search Compatibility Header
 * 
 * This header provides compatibility for legacy track analyzer code.
 * The actual sector search functionality is in tracks/sector_extractor.h
 */

#ifndef UFT_PRIVATE_SECTOR_SEARCH_H
#define UFT_PRIVATE_SECTOR_SEARCH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sector search result structure */
typedef struct {
    int found;
    uint32_t bit_offset;
    uint32_t byte_offset;
    uint8_t sector_id;
    uint8_t track_id;
    uint8_t head_id;
} sector_search_result_t;

/* Basic sector search - find sync pattern */
static inline int search_sync_pattern(const uint8_t *data, size_t data_size, 
                                       uint32_t pattern, int pattern_bits,
                                       uint32_t start_offset) {
    (void)data; (void)data_size; (void)pattern; (void)pattern_bits; (void)start_offset;
    /* Actual implementation in sector_extractor */
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_PRIVATE_SECTOR_SEARCH_H */
