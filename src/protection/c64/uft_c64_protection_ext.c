/**
 * @file uft_c64_protection_ext.c
 * @brief Extended C64 Copy Protection Detection Implementation
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/protection/uft_c64_protection_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Protection names */
static const struct {
    c64_prot_ext_type_t type;
    const char *name;
    const char *category;
} prot_info[] = {
    { C64_PROT_EXT_TIMEWARP,     "TimeWarp",         "Track-based" },
    { C64_PROT_EXT_DENSITRON,    "Densitron",        "Track-based" },
    { C64_PROT_EXT_KRACKER_JAX,  "Kracker Jax",      "Track-based" },
    { C64_PROT_EXT_FORMASTER,    "Formaster",        "Track-based" },
    { C64_PROT_EXT_MICROFORTE,   "Microforte",       "Track-based" },
    { C64_PROT_EXT_RAINBOW_ARTS, "Rainbow Arts",     "Track-based" },
    { C64_PROT_EXT_GMA,          "GMA",              "Sector-based" },
    { C64_PROT_EXT_ABACUS,       "Abacus",           "Sector-based" },
    { C64_PROT_EXT_BUBBLE_BURST, "Bubble Burst",     "Sector-based" },
    { C64_PROT_EXT_TRILOGIC,     "Trilogic",         "Sector-based" },
    { C64_PROT_EXT_TURBO_TAPE,   "Turbo Tape",       "Loader-based" },
    { C64_PROT_EXT_PAVLODA,      "Pavloda",          "Loader-based" },
    { C64_PROT_EXT_FLASHLOAD,    "Flashload",        "Loader-based" },
    { C64_PROT_EXT_HYPRA_LOAD,   "Hypra Load",       "Loader-based" },
    { C64_PROT_EXT_OCEAN,        "Ocean",            "Publisher" },
    { C64_PROT_EXT_US_GOLD,      "US Gold",          "Publisher" },
    { C64_PROT_EXT_MASTERTRONIC, "Mastertronic",     "Publisher" },
    { C64_PROT_EXT_CODEMASTERS,  "Codemasters",      "Publisher" },
    { C64_PROT_EXT_ACTIVISION,   "Activision",       "Publisher" },
    { C64_PROT_EXT_EPYX,         "Epyx",             "Publisher" },
    { C64_PROT_EXT_FAT_TRACK,    "Fat Track",        "Misc" },
    { C64_PROT_EXT_SYNC_MARK,    "Custom Sync",      "Misc" },
    { C64_PROT_EXT_GAP_LENGTH,   "Gap Length",       "Misc" },
    { C64_PROT_EXT_DENSITY_KEY,  "Density Key",      "Misc" },
    { C64_PROT_EXT_NONE,         NULL,               NULL }
};

/** TimeWarp signatures */
static const uint8_t timewarp_sig_v1[] = { 0xA9, 0x00, 0x85, 0x02, 0xA9, 0x36 };
static const uint8_t timewarp_sig_v2[] = { 0xA9, 0x00, 0x8D, 0x00, 0xDD, 0xA9 };
static const uint8_t timewarp_sig_v3[] = { 0x78, 0xA9, 0x7F, 0x8D, 0x0D, 0xDC };

/** Kracker Jax signature */
static const uint8_t kracker_jax_sig[] = { 0x4B, 0x52, 0x41, 0x43, 0x4B };  /* "KRACK" */

/** Formaster signature */
static const uint8_t formaster_sig[] = { 0xEE, 0x00, 0x1C, 0xAD, 0x00, 0x1C };

/** Rainbow Arts signature */
static const uint8_t rainbow_arts_sig[] = { 0x52, 0x41, 0x49, 0x4E };  /* "RAIN" */

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Search for signature in data
 */
static const uint8_t *find_signature(const uint8_t *data, size_t size,
                                     const uint8_t *sig, size_t sig_len)
{
    if (!data || !sig || size < sig_len) return NULL;
    
    for (size_t i = 0; i + sig_len <= size; i++) {
        if (memcmp(data + i, sig, sig_len) == 0) {
            return data + i;
        }
    }
    
    return NULL;
}

/* ============================================================================
 * TimeWarp Detection
 * ============================================================================ */

/**
 * @brief Detect TimeWarp protection
 */
bool c64_detect_timewarp(const uint8_t *data, size_t size,
                         c64_timewarp_result_t *result)
{
    if (!data || !result || size == 0) return false;
    
    memset(result, 0, sizeof(c64_timewarp_result_t));
    
    /* Check for Version 1 */
    const uint8_t *found = find_signature(data, size, timewarp_sig_v1, sizeof(timewarp_sig_v1));
    if (found) {
        result->detected = true;
        result->version = 1;
        memcpy(result->signature, found, 6);
        snprintf(result->description, sizeof(result->description),
                 "TimeWarp v1 detected");
        return true;
    }
    
    /* Check for Version 2 */
    found = find_signature(data, size, timewarp_sig_v2, sizeof(timewarp_sig_v2));
    if (found) {
        result->detected = true;
        result->version = 2;
        memcpy(result->signature, found, 6);
        snprintf(result->description, sizeof(result->description),
                 "TimeWarp v2 detected");
        return true;
    }
    
    /* Check for Version 3 */
    found = find_signature(data, size, timewarp_sig_v3, sizeof(timewarp_sig_v3));
    if (found) {
        result->detected = true;
        result->version = 3;
        memcpy(result->signature, found, 6);
        snprintf(result->description, sizeof(result->description),
                 "TimeWarp v3 detected");
        return true;
    }
    
    return false;
}

/**
 * @brief Detect TimeWarp in GCR track
 */
bool c64_detect_timewarp_track(const uint8_t *track_data, size_t track_len,
                               int track, c64_timewarp_result_t *result)
{
    if (!track_data || !result) return false;
    
    /* TimeWarp typically uses track 36 or 37 */
    if (track < 36 || track > 40) return false;
    
    return c64_detect_timewarp(track_data, track_len, result);
}

/* ============================================================================
 * Densitron Detection
 * ============================================================================ */

/**
 * @brief Check Densitron pattern
 */
bool c64_is_densitron_pattern(const uint8_t *densities)
{
    if (!densities) return false;
    
    /* Check for density gradient (3, 2, 1, 0) or reverse */
    if (densities[0] == 3 && densities[1] == 2 && 
        densities[2] == 1 && densities[3] == 0) {
        return true;
    }
    
    if (densities[0] == 0 && densities[1] == 1 && 
        densities[2] == 2 && densities[3] == 3) {
        return true;
    }
    
    return false;
}

/**
 * @brief Detect Densitron protection
 */
bool c64_detect_densitron(const uint8_t *track_densities, int num_tracks,
                          c64_densitron_result_t *result)
{
    if (!track_densities || !result || num_tracks < 70) return false;
    
    memset(result, 0, sizeof(c64_densitron_result_t));
    
    /* Look for unusual density patterns on protection tracks (typically 36-40) */
    for (int t = 36 * 2; t <= 40 * 2 - 4; t++) {
        uint8_t pattern[4] = {
            track_densities[t] & 0x03,
            track_densities[t + 1] & 0x03,
            track_densities[t + 2] & 0x03,
            track_densities[t + 3] & 0x03
        };
        
        if (c64_is_densitron_pattern(pattern)) {
            result->detected = true;
            result->key_tracks[0] = t / 2;
            result->key_tracks[1] = (t + 1) / 2;
            result->key_tracks[2] = (t + 2) / 2;
            result->key_tracks[3] = (t + 3) / 2;
            result->num_key_tracks = 4;
            memcpy(result->density_pattern, pattern, 4);
            
            snprintf(result->description, sizeof(result->description),
                     "Densitron detected on tracks %d-%d",
                     result->key_tracks[0], result->key_tracks[3]);
            
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * Kracker Jax Detection
 * ============================================================================ */

/**
 * @brief Detect Kracker Jax
 */
bool c64_detect_kracker_jax(const uint8_t *data, size_t size,
                            c64_kracker_jax_result_t *result)
{
    if (!data || !result || size == 0) return false;
    
    memset(result, 0, sizeof(c64_kracker_jax_result_t));
    
    /* Search for "KRACK" signature */
    const uint8_t *found = find_signature(data, size, kracker_jax_sig, sizeof(kracker_jax_sig));
    if (found) {
        result->detected = true;
        memcpy(result->signature, found, sizeof(kracker_jax_sig));
        
        /* Try to extract volume/issue info */
        /* Typically follows the signature */
        if ((size_t)(found - data) + 10 < size) {
            result->volume = found[6];
            result->issue = found[7];
        }
        
        snprintf(result->description, sizeof(result->description),
                 "Kracker Jax detected (Vol %d, Issue %d)",
                 result->volume, result->issue);
        
        return true;
    }
    
    return false;
}

/**
 * @brief Detect Kracker Jax in D64
 */
bool c64_detect_kracker_jax_d64(const uint8_t *d64_data, size_t d64_size,
                                c64_kracker_jax_result_t *result)
{
    if (!d64_data || !result) return false;
    
    /* D64 is just linear sector data */
    return c64_detect_kracker_jax(d64_data, d64_size, result);
}

/* ============================================================================
 * Generic Detection
 * ============================================================================ */

/**
 * @brief Detect specific protection
 */
bool c64_detect_protection_ext(c64_prot_ext_type_t type,
                               const uint8_t *data, size_t size,
                               c64_prot_ext_result_t *result)
{
    if (!data || !result || size == 0) return false;
    
    memset(result, 0, sizeof(c64_prot_ext_result_t));
    result->type = type;
    
    const uint8_t *found = NULL;
    
    switch (type) {
        case C64_PROT_EXT_TIMEWARP: {
            c64_timewarp_result_t tw;
            if (c64_detect_timewarp(data, size, &tw)) {
                result->detected = true;
                result->confidence = 95;
                memcpy(result->signature, tw.signature, 6);
                result->signature_len = 6;
                strncpy(result->name, "TimeWarp", sizeof(result->name));
                strncpy(result->description, tw.description, sizeof(result->description) - 1); result->description[sizeof(result->description) - 1] = '\0';
                return true;
            }
            break;
        }
        
        case C64_PROT_EXT_KRACKER_JAX: {
            c64_kracker_jax_result_t kj;
            if (c64_detect_kracker_jax(data, size, &kj)) {
                result->detected = true;
                result->confidence = 90;
                memcpy(result->signature, kj.signature, sizeof(kracker_jax_sig));
                result->signature_len = sizeof(kracker_jax_sig);
                strncpy(result->name, "Kracker Jax", sizeof(result->name));
                strncpy(result->description, kj.description, sizeof(result->description) - 1); result->description[sizeof(result->description) - 1] = '\0';
                return true;
            }
            break;
        }
        
        case C64_PROT_EXT_FORMASTER:
            found = find_signature(data, size, formaster_sig, sizeof(formaster_sig));
            if (found) {
                result->detected = true;
                result->confidence = 85;
                memcpy(result->signature, found, sizeof(formaster_sig));
                result->signature_len = sizeof(formaster_sig);
                strncpy(result->name, "Formaster", sizeof(result->name));
                snprintf(result->description, sizeof(result->description),
                         "Formaster protection detected");
                return true;
            }
            break;
            
        case C64_PROT_EXT_RAINBOW_ARTS:
            found = find_signature(data, size, rainbow_arts_sig, sizeof(rainbow_arts_sig));
            if (found) {
                result->detected = true;
                result->confidence = 80;
                memcpy(result->signature, found, sizeof(rainbow_arts_sig));
                result->signature_len = sizeof(rainbow_arts_sig);
                strncpy(result->name, "Rainbow Arts", sizeof(result->name));
                snprintf(result->description, sizeof(result->description),
                         "Rainbow Arts protection detected");
                return true;
            }
            break;
            
        default:
            break;
    }
    
    return false;
}

/**
 * @brief Scan for all protections
 */
int c64_scan_protections_ext(const uint8_t *data, size_t size,
                             c64_prot_ext_scan_t *scan)
{
    if (!data || !scan || size == 0) return 0;
    
    memset(scan, 0, sizeof(c64_prot_ext_scan_t));
    
    /* List of protections to check */
    c64_prot_ext_type_t types_to_check[] = {
        C64_PROT_EXT_TIMEWARP,
        C64_PROT_EXT_KRACKER_JAX,
        C64_PROT_EXT_FORMASTER,
        C64_PROT_EXT_RAINBOW_ARTS,
    };
    
    int num_types = sizeof(types_to_check) / sizeof(types_to_check[0]);
    
    for (int i = 0; i < num_types && scan->num_found < 16; i++) {
        c64_prot_ext_result_t result;
        if (c64_detect_protection_ext(types_to_check[i], data, size, &result)) {
            scan->results[scan->num_found++] = result;
        }
    }
    
    /* Generate summary */
    if (scan->num_found > 0) {
        snprintf(scan->summary, sizeof(scan->summary),
                 "Found %d protection(s): ", scan->num_found);
        
        for (int i = 0; i < scan->num_found; i++) {
            if (i > 0) strncat(scan->summary, ", ", sizeof(scan->summary) - strlen(scan->summary) - 1);
            strncat(scan->summary, scan->results[i].name, sizeof(scan->summary) - strlen(scan->summary) - 1);
        }
    } else {
        snprintf(scan->summary, sizeof(scan->summary), "No protections detected");
    }
    
    return scan->num_found;
}

/**
 * @brief Scan GCR tracks for protections
 */
int c64_scan_gcr_protections(const uint8_t **track_data,
                             const size_t *track_lengths,
                             const uint8_t *track_densities,
                             int num_tracks,
                             c64_prot_ext_scan_t *scan)
{
    if (!track_data || !track_lengths || !scan) return 0;
    
    memset(scan, 0, sizeof(c64_prot_ext_scan_t));
    
    /* Scan each track */
    for (int t = 0; t < num_tracks && scan->num_found < 16; t++) {
        if (!track_data[t] || track_lengths[t] == 0) continue;
        
        /* Check for TimeWarp on extended tracks */
        if (t >= 72) {  /* Track 36+ */
            c64_timewarp_result_t tw;
            if (c64_detect_timewarp_track(track_data[t], track_lengths[t], t / 2, &tw)) {
                c64_prot_ext_result_t *r = &scan->results[scan->num_found];
                r->type = C64_PROT_EXT_TIMEWARP;
                r->detected = true;
                r->confidence = 95;
                r->track = t / 2;
                strncpy(r->name, "TimeWarp", sizeof(r->name));
                strncpy(r->description, tw.description, sizeof(r->description) - 1); r->description[sizeof(r->description) - 1] = '\0';
                scan->num_found++;
            }
        }
        
        /* Scan track data for other signatures */
        c64_prot_ext_result_t result;
        if (c64_detect_protection_ext(C64_PROT_EXT_KRACKER_JAX, 
                                      track_data[t], track_lengths[t], &result)) {
            result.track = t / 2;
            scan->results[scan->num_found++] = result;
        }
    }
    
    /* Check density patterns */
    if (track_densities && num_tracks >= 80) {
        c64_densitron_result_t dens;
        if (c64_detect_densitron(track_densities, num_tracks, &dens)) {
            c64_prot_ext_result_t *r = &scan->results[scan->num_found];
            r->type = C64_PROT_EXT_DENSITRON;
            r->detected = true;
            r->confidence = 90;
            r->track = dens.key_tracks[0];
            strncpy(r->name, "Densitron", sizeof(r->name));
            strncpy(r->description, dens.description, sizeof(r->description) - 1); r->description[sizeof(r->description) - 1] = '\0';
            scan->num_found++;
        }
    }
    
    /* Generate summary */
    if (scan->num_found > 0) {
        snprintf(scan->summary, sizeof(scan->summary),
                 "Found %d protection(s) in GCR data", scan->num_found);
    } else {
        snprintf(scan->summary, sizeof(scan->summary), "No protections detected");
    }
    
    return scan->num_found;
}

/* ============================================================================
 * Track Analysis
 * ============================================================================ */

/**
 * @brief Check for fat track
 */
bool c64_is_fat_track(const uint8_t *track_data, size_t track_len,
                      size_t expected_capacity)
{
    if (!track_data || track_len == 0) return false;
    
    /* Fat track is typically 10%+ longer */
    return (track_len > expected_capacity * 110 / 100);
}

/**
 * @brief Check for custom sync
 */
int c64_check_custom_sync(const uint8_t *track_data, size_t track_len,
                          uint8_t sync_byte)
{
    if (!track_data || track_len == 0) return 0;
    
    int non_standard = 0;
    bool in_sync = false;
    
    for (size_t i = 0; i < track_len; i++) {
        if (track_data[i] == 0xFF) {
            in_sync = true;
        } else if (in_sync) {
            /* End of sync, check if standard */
            if (track_data[i] != sync_byte && (track_data[i] & 0x80)) {
                non_standard++;
            }
            in_sync = false;
        }
    }
    
    return non_standard;
}

/**
 * @brief Analyze gaps
 */
int c64_analyze_gaps(const uint8_t *track_data, size_t track_len,
                     int *min_gap, int *max_gap, int *avg_gap)
{
    if (!track_data || track_len == 0) return 0;
    
    int gaps_found = 0;
    int total_gap = 0;
    int current_gap = 0;
    int local_min = 0x7FFFFFFF;
    int local_max = 0;
    bool in_gap = false;
    
    for (size_t i = 0; i < track_len; i++) {
        if (track_data[i] == 0x55) {
            if (!in_gap) {
                in_gap = true;
                current_gap = 0;
            }
            current_gap++;
        } else if (in_gap) {
            /* End of gap */
            if (current_gap >= 3) {  /* Minimum gap size */
                gaps_found++;
                total_gap += current_gap;
                if (current_gap < local_min) local_min = current_gap;
                if (current_gap > local_max) local_max = current_gap;
            }
            in_gap = false;
        }
    }
    
    if (min_gap) *min_gap = (gaps_found > 0) ? local_min : 0;
    if (max_gap) *max_gap = local_max;
    if (avg_gap) *avg_gap = (gaps_found > 0) ? (total_gap / gaps_found) : 0;
    
    return gaps_found;
}

/**
 * @brief Check for density key
 */
bool c64_is_density_key(const uint8_t *track_data, size_t track_len,
                        int actual_density, int expected_density)
{
    (void)track_data;
    (void)track_len;
    
    /* Density mismatch indicates protection */
    return (actual_density != expected_density);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get protection name
 */
const char *c64_prot_ext_name(c64_prot_ext_type_t type)
{
    for (int i = 0; prot_info[i].name != NULL; i++) {
        if (prot_info[i].type == type) {
            return prot_info[i].name;
        }
    }
    return "Unknown";
}

/**
 * @brief Get protection category
 */
const char *c64_prot_ext_category(c64_prot_ext_type_t type)
{
    for (int i = 0; prot_info[i].name != NULL; i++) {
        if (prot_info[i].type == type) {
            return prot_info[i].category;
        }
    }
    return "Unknown";
}

/**
 * @brief Check if track-based
 */
bool c64_prot_ext_is_track_based(c64_prot_ext_type_t type)
{
    return (type >= 0x0100 && type < 0x0200);
}

/**
 * @brief Check if density-based
 */
bool c64_prot_ext_is_density_based(c64_prot_ext_type_t type)
{
    return (type == C64_PROT_EXT_DENSITRON || type == C64_PROT_EXT_DENSITY_KEY);
}
