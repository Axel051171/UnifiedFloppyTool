/**
 * @file uft_interleave.c
 * @brief Sector interleave detection and optimization implementation
 */

#include "uft_interleave.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Detection Functions
 * ============================================================================ */

void uft_interleave_detect(const uft_sector_id_t *sectors,
                           size_t count,
                           uft_interleave_map_t *map) {
    if (!sectors || !map || count == 0) return;
    
    memset(map, 0, sizeof(*map));
    
    if (count > UFT_MAX_SECTORS_PER_TRACK) {
        count = UFT_MAX_SECTORS_PER_TRACK;
    }
    
    /* Copy physical order */
    for (size_t i = 0; i < count; i++) {
        map->physical_order[i] = sectors[i].sector;
    }
    map->sector_count = (uint8_t)count;
    
    /* Calculate differences between consecutive sectors */
    int diffs[UFT_MAX_SECTORS_PER_TRACK];
    int diff_count = 0;
    int diff_freq[UFT_MAX_SECTORS_PER_TRACK] = {0};
    
    for (size_t i = 1; i < count; i++) {
        int diff = map->physical_order[i] - map->physical_order[i-1];
        
        /* Handle wrap-around */
        if (diff < 0) diff += count;
        if (diff == 0) diff = count;  /* Same sector shouldn't happen */
        
        diffs[diff_count++] = diff;
        
        if (diff > 0 && (size_t)diff < UFT_MAX_SECTORS_PER_TRACK) {
            diff_freq[diff]++;
        }
    }
    
    /* Find most common difference = interleave */
    int max_freq = 0;
    map->interleave = 1;
    
    for (int i = 1; i < (int)count; i++) {
        if (diff_freq[i] > max_freq) {
            max_freq = diff_freq[i];
            map->interleave = (uint8_t)i;
        }
    }
    
    /* Check if order is valid (>50% consistent) */
    map->order_valid = (max_freq > diff_count / 2);
    
    /* Calculate consistency */
    map->consistency = (diff_count > 0) ? 
        (uint8_t)(max_freq * 100 / diff_count) : 0;
    
    /* Find gaps/missing sectors */
    bool seen[256] = {false};
    uint8_t min_sector = 255, max_sector = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint8_t s = map->physical_order[i];
        seen[s] = true;
        if (s < min_sector) min_sector = s;
        if (s > max_sector) max_sector = s;
    }
    
    /* Count missing in range */
    for (uint8_t s = min_sector; s <= max_sector; s++) {
        if (!seen[s]) map->gaps++;
    }
    
    /* Generate optimal order */
    uft_interleave_generate_order(map);
}

uint8_t uft_interleave_detect_skew(const uft_interleave_map_t *map1,
                                   const uft_interleave_map_t *map2) {
    if (!map1 || !map2 || map1->sector_count == 0) return 0;
    
    /* Find where sector 1 appears in each track */
    int pos1 = -1, pos2 = -1;
    
    for (size_t i = 0; i < map1->sector_count; i++) {
        if (map1->physical_order[i] == 1) pos1 = (int)i;
    }
    for (size_t i = 0; i < map2->sector_count; i++) {
        if (map2->physical_order[i] == 1) pos2 = (int)i;
    }
    
    if (pos1 < 0 || pos2 < 0) return 0;
    
    int skew = pos2 - pos1;
    if (skew < 0) skew += map1->sector_count;
    
    return (uint8_t)skew;
}

uint8_t uft_interleave_detect_head_offset(const uft_interleave_map_t *head0_map,
                                          const uft_interleave_map_t *head1_map) {
    /* Same as skew detection */
    return uft_interleave_detect_skew(head0_map, head1_map);
}

/* ============================================================================
 * Order Generation
 * ============================================================================ */

void uft_interleave_generate_order(uft_interleave_map_t *map) {
    if (!map || map->sector_count == 0) return;
    
    uft_interleave_generate_table(map->optimal_order,
                                   map->sector_count,
                                   map->interleave,
                                   1);  /* Start from sector 1 */
}

void uft_interleave_generate_table(uint8_t *order,
                                   uint8_t sector_count,
                                   uint8_t interleave,
                                   uint8_t start_sector) {
    if (!order || sector_count == 0) return;
    
    bool used[256] = {false};
    uint8_t current = start_sector;
    
    for (uint8_t i = 0; i < sector_count; i++) {
        order[i] = current;
        used[current] = true;
        
        /* Find next unused sector with interleave */
        for (uint8_t j = 0; j < sector_count; j++) {
            uint8_t next = start_sector + ((current - start_sector + interleave) % sector_count);
            if (!used[next]) {
                current = next;
                break;
            }
            /* If next is used, try incrementing */
            current = start_sector + ((current - start_sector + 1) % sector_count);
            if (!used[current]) break;
        }
    }
}

void uft_interleave_apply_skew(uint8_t *order,
                               uint8_t sector_count,
                               uint8_t skew) {
    if (!order || sector_count == 0 || skew == 0) return;
    
    /* Rotate order by skew positions */
    uint8_t temp[UFT_MAX_SECTORS_PER_TRACK];
    memcpy(temp, order, sector_count);
    
    for (uint8_t i = 0; i < sector_count; i++) {
        order[(i + skew) % sector_count] = temp[i];
    }
}

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

uint8_t uft_interleave_check_consistency(const uft_interleave_map_t *map) {
    if (!map) return 0;
    return map->consistency;
}

size_t uft_interleave_find_missing(const uft_interleave_map_t *map,
                                   uint8_t *missing,
                                   size_t max_missing) {
    if (!map || !missing) return 0;
    
    bool seen[256] = {false};
    uint8_t min_sector = 255, max_sector = 0;
    
    for (size_t i = 0; i < map->sector_count; i++) {
        uint8_t s = map->physical_order[i];
        seen[s] = true;
        if (s < min_sector) min_sector = s;
        if (s > max_sector) max_sector = s;
    }
    
    size_t missing_count = 0;
    for (uint8_t s = min_sector; s <= max_sector && missing_count < max_missing; s++) {
        if (!seen[s]) {
            missing[missing_count++] = s;
        }
    }
    
    return missing_count;
}

double uft_interleave_efficiency(const uft_interleave_map_t *map, double rpm) {
    if (!map || map->sector_count == 0 || rpm <= 0) return 0;
    
    (void)rpm;  /* For future use with timing */
    
    /* Efficiency = 100% if interleave allows reading all sectors in one rotation */
    /* With interleave N, need N rotations */
    
    double rotations_needed = 1.0;
    
    if (map->interleave > 1) {
        /* Simple approximation */
        rotations_needed = map->interleave;
    }
    
    /* Account for gaps */
    if (map->gaps > 0) {
        rotations_needed += map->gaps * 0.1;
    }
    
    return 100.0 / rotations_needed;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void uft_interleave_stats_init(uft_interleave_stats_t *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
}

void uft_interleave_stats_add(uft_interleave_stats_t *stats,
                              const uft_interleave_map_t *map) {
    if (!stats || !map) return;
    
    stats->tracks_analyzed++;
    
    if (map->interleave < 32) {
        stats->interleave_histogram[map->interleave]++;
    }
    
    if (map->consistency >= 80) {
        stats->consistent_tracks++;
    }
    
    stats->avg_consistency = 
        (stats->avg_consistency * (stats->tracks_analyzed - 1) + map->consistency) /
        stats->tracks_analyzed;
}

void uft_interleave_stats_finalize(uft_interleave_stats_t *stats) {
    if (!stats) return;
    
    /* Find dominant interleave */
    size_t max_count = 0;
    for (int i = 0; i < 32; i++) {
        if (stats->interleave_histogram[i] > max_count) {
            max_count = stats->interleave_histogram[i];
            stats->dominant_interleave = (uint8_t)i;
        }
    }
}

/* ============================================================================
 * Debug Output
 * ============================================================================ */

void uft_interleave_dump(const uft_interleave_map_t *map) {
    if (!map) {
        printf("Interleave Map: NULL\n");
        return;
    }
    
    printf("=== Interleave Map ===\n");
    printf("Sectors: %u, Interleave: %u\n", map->sector_count, map->interleave);
    printf("Consistency: %u%%, Valid: %s, Gaps: %u\n",
           map->consistency,
           map->order_valid ? "yes" : "no",
           map->gaps);
    
    printf("Physical order: ");
    for (size_t i = 0; i < map->sector_count; i++) {
        printf("%u ", map->physical_order[i]);
    }
    printf("\n");
    
    printf("Optimal order:  ");
    for (size_t i = 0; i < map->sector_count; i++) {
        printf("%u ", map->optimal_order[i]);
    }
    printf("\n");
}

void uft_interleave_stats_dump(const uft_interleave_stats_t *stats) {
    if (!stats) return;
    
    printf("=== Interleave Statistics ===\n");
    printf("Tracks analyzed: %zu\n", stats->tracks_analyzed);
    printf("Consistent tracks: %zu (%.1f%%)\n",
           stats->consistent_tracks,
           stats->tracks_analyzed > 0 ?
               100.0 * stats->consistent_tracks / stats->tracks_analyzed : 0);
    printf("Dominant interleave: %u\n", stats->dominant_interleave);
    printf("Average consistency: %.1f%%\n", stats->avg_consistency);
}
