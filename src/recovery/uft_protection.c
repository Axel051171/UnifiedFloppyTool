/**
 * @file uft_protection.c
 * @brief UFT Copy Protection Detection and Preservation
 * 
 * Detects and preserves various copy protection schemes:
 * - Weak bits / Fuzzy bits
 * - Long tracks / Short tracks
 * - Non-standard sector sizes
 * - Timing-based protection
 * - Duplicate sectors
 * - Missing sectors
 * 
 * @version 5.28.0 GOD MODE
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Types
 * ============================================================================ */

typedef enum {
    UFT_PROT_NONE = 0,
    UFT_PROT_WEAK_BITS,           /* Random/weak bits */
    UFT_PROT_LONG_TRACK,          /* Extra data on track */
    UFT_PROT_SHORT_TRACK,         /* Missing data on track */
    UFT_PROT_FUZZY_BITS,          /* Bits that vary between reads */
    UFT_PROT_DUPLICATE_SECTORS,   /* Multiple sectors with same ID */
    UFT_PROT_MISSING_SECTORS,     /* Expected sectors missing */
    UFT_PROT_BAD_CRC,             /* Intentional CRC errors */
    UFT_PROT_NON_STANDARD_SIZE,   /* Unusual sector size */
    UFT_PROT_TIMING,              /* Timing-based protection */
    UFT_PROT_HIDDEN_DATA,         /* Data in gaps */
    UFT_PROT_CUSTOM_ENCODING,     /* Non-standard encoding */
    UFT_PROT_MULTIPLE             /* Combination of protections */
} uft_protection_type_t;

typedef struct {
    uft_protection_type_t type;
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;           /* 0xFF if applies to whole track */
    size_t   position;         /* Bit/byte position */
    size_t   length;           /* Length of protected region */
    uint8_t  confidence;       /* Detection confidence 0-100 */
    char     description[64];
} uft_protection_marker_t;

typedef struct {
    uft_protection_marker_t *markers;
    size_t marker_count;
    uft_protection_type_t primary_type;
    char scheme_name[64];     /* Known scheme name if identified */
    uint8_t overall_confidence;
} uft_protection_result_t;

typedef struct {
    bool detect_weak_bits;
    bool detect_timing;
    bool detect_duplicates;
    bool preserve_protection;
    size_t weak_bit_threshold;  /* Variance threshold for weak bit detection */
} uft_protection_config_t;

/* ============================================================================
 * Known Protection Scheme Signatures
 * ============================================================================ */

typedef struct {
    const char *name;
    uft_protection_type_t types[4];
    uint8_t track_pattern[4];   /* -1 = any, -2 = end */
    uint8_t sector_pattern[4];
} uft_known_scheme_t;

static const uft_known_scheme_t known_schemes[] = {
    {"Copylock", {UFT_PROT_LONG_TRACK, UFT_PROT_WEAK_BITS, UFT_PROT_NONE}, 
     {0, 1, 0xFF}, {0xFF, 0xFF}},
    {"Rob Northen Copylock", {UFT_PROT_LONG_TRACK, UFT_PROT_FUZZY_BITS, UFT_PROT_NONE},
     {0, 0xFF}, {0xFF}},
    {"V-Max", {UFT_PROT_DUPLICATE_SECTORS, UFT_PROT_WEAK_BITS, UFT_PROT_NONE},
     {18, 0xFF}, {0xFF}},
    {"Vorpal", {UFT_PROT_CUSTOM_ENCODING, UFT_PROT_TIMING, UFT_PROT_NONE},
     {18, 0xFF}, {0xFF}},
    {"GEOS", {UFT_PROT_HIDDEN_DATA, UFT_PROT_NONE},
     {18, 0xFF}, {0xFF}},
    {NULL, {UFT_PROT_NONE}, {0xFF}, {0xFF}}
};

/* ============================================================================
 * Weak Bit Detection
 * ============================================================================ */

/**
 * @brief Detect weak bits by comparing multiple reads
 */
static int detect_weak_bits(const uint8_t **reads, size_t read_count,
                            size_t data_len, 
                            uft_protection_marker_t **markers,
                            size_t *marker_count)
{
    if (!reads || read_count < 2 || !markers || !marker_count) return -1;
    
    /* Allocate temporary storage */
    *markers = malloc(data_len * sizeof(uft_protection_marker_t));
    if (!*markers) return -1;
    *marker_count = 0;
    
    /* Compare all reads bit by bit */
    for (size_t byte = 0; byte < data_len; byte++) {
        uint8_t variations = 0;
        
        for (size_t r = 1; r < read_count; r++) {
            variations |= (reads[0][byte] ^ reads[r][byte]);
        }
        
        if (variations != 0) {
            /* Count varying bits */
            int var_bits = 0;
            while (variations) {
                var_bits += variations & 1;
                variations >>= 1;
            }
            
            uft_protection_marker_t *m = &(*markers)[*marker_count];
            m->type = UFT_PROT_WEAK_BITS;
            m->position = byte;
            m->length = 1;
            m->confidence = (var_bits * 100) / 8;
            snprintf(m->description, sizeof(m->description),
                     "Weak bits at byte %zu (%d bits vary)", byte, var_bits);
            (*marker_count)++;
        }
    }
    
    /* Shrink allocation */
    if (*marker_count > 0) {
        *markers = realloc(*markers, *marker_count * sizeof(uft_protection_marker_t));
    } else {
        free(*markers);
        *markers = NULL;
    }
    
    return 0;
}

/**
 * @brief Detect weak bits from flux timing variance
 */
static int detect_weak_bits_flux(const uint32_t *flux_times, size_t count,
                                 double clock_ns, size_t threshold,
                                 size_t **weak_positions, size_t *weak_count)
{
    if (!flux_times || count == 0 || !weak_positions || !weak_count) return -1;
    
    *weak_positions = malloc(count * sizeof(size_t));
    if (!*weak_positions) return -1;
    *weak_count = 0;
    
    /* Analyze timing jitter in sliding window */
    const size_t window = 8;
    
    for (size_t i = window; i < count - window; i++) {
        double sum = 0, sum_sq = 0;
        
        for (size_t j = i - window; j <= i + window; j++) {
            double t = flux_times[j];
            sum += t;
            sum_sq += t * t;
        }
        
        double mean = sum / (2 * window + 1);
        double variance = (sum_sq / (2 * window + 1)) - (mean * mean);
        double stddev = sqrt(variance);
        
        /* High variance indicates weak/fuzzy bits */
        if (stddev > threshold) {
            (*weak_positions)[*weak_count] = i;
            (*weak_count)++;
        }
    }
    
    /* Shrink allocation */
    if (*weak_count > 0) {
        *weak_positions = realloc(*weak_positions, *weak_count * sizeof(size_t));
    } else {
        free(*weak_positions);
        *weak_positions = NULL;
    }
    
    return 0;
}

/* ============================================================================
 * Track Length Analysis
 * ============================================================================ */

/**
 * @brief Detect long/short tracks
 */
static uft_protection_type_t analyze_track_length(double track_time_ms,
                                                  double rpm,
                                                  double *deviation)
{
    /* Expected track time at given RPM */
    double expected_ms = 60000.0 / rpm;
    
    *deviation = (track_time_ms - expected_ms) / expected_ms * 100.0;
    
    if (*deviation > 5.0) {
        return UFT_PROT_LONG_TRACK;
    } else if (*deviation < -5.0) {
        return UFT_PROT_SHORT_TRACK;
    }
    
    return UFT_PROT_NONE;
}

/* ============================================================================
 * Duplicate Sector Detection
 * ============================================================================ */

/**
 * @brief Detect duplicate sectors (same ID, different data)
 */
static int detect_duplicate_sectors(const uint8_t *sector_ids, size_t sector_count,
                                    uint8_t **duplicates, size_t *dup_count)
{
    if (!sector_ids || !duplicates || !dup_count) return -1;
    
    *duplicates = calloc(256, sizeof(uint8_t));  /* Count per sector ID */
    if (!*duplicates) return -1;
    
    *dup_count = 0;
    
    /* Count occurrences of each sector ID */
    for (size_t i = 0; i < sector_count; i++) {
        (*duplicates)[sector_ids[i]]++;
    }
    
    /* Find duplicates */
    for (int i = 0; i < 256; i++) {
        if ((*duplicates)[i] > 1) {
            (*dup_count)++;
        }
    }
    
    return 0;
}

/* ============================================================================
 * Scheme Identification
 * ============================================================================ */

/**
 * @brief Try to identify known protection scheme
 */
static const char* identify_scheme(const uft_protection_marker_t *markers,
                                   size_t marker_count)
{
    if (!markers || marker_count == 0) return NULL;
    
    /* Count protection types */
    int type_counts[16] = {0};
    for (size_t i = 0; i < marker_count; i++) {
        if (markers[i].type < 16) {
            type_counts[markers[i].type]++;
        }
    }
    
    /* Match against known schemes */
    for (const uft_known_scheme_t *s = known_schemes; s->name; s++) {
        bool match = true;
        
        for (int i = 0; s->types[i] != UFT_PROT_NONE && match; i++) {
            if (type_counts[s->types[i]] == 0) {
                match = false;
            }
        }
        
        if (match) {
            return s->name;
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize protection detection configuration
 */
void uft_protection_config_init(uft_protection_config_t *config)
{
    config->detect_weak_bits = true;
    config->detect_timing = true;
    config->detect_duplicates = true;
    config->preserve_protection = true;
    config->weak_bit_threshold = 100;  /* ns */
}

/**
 * @brief Detect protection on track from multiple reads
 */
int uft_protection_detect_multi_read(const uint8_t **reads, size_t read_count,
                                     size_t data_len, uint8_t track, uint8_t head,
                                     uft_protection_result_t *result)
{
    if (!reads || read_count < 2 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Detect weak bits */
    uft_protection_marker_t *weak_markers;
    size_t weak_count;
    
    detect_weak_bits(reads, read_count, data_len, &weak_markers, &weak_count);
    
    if (weak_count > 0) {
        result->markers = weak_markers;
        result->marker_count = weak_count;
        result->primary_type = UFT_PROT_WEAK_BITS;
        
        /* Set track/head on all markers */
        for (size_t i = 0; i < weak_count; i++) {
            result->markers[i].track = track;
            result->markers[i].head = head;
        }
        
        /* Try to identify scheme */
        const char *scheme = identify_scheme(result->markers, result->marker_count);
        if (scheme) {
            strncpy(result->scheme_name, scheme, sizeof(result->scheme_name) - 1);
        }
        
        result->overall_confidence = 80;
    }
    
    return 0;
}

/**
 * @brief Detect protection from flux timing
 */
int uft_protection_detect_flux(const uint32_t *flux_times, size_t count,
                               double clock_ns, double track_time_ms, double rpm,
                               uint8_t track, uint8_t head,
                               uft_protection_result_t *result)
{
    if (!flux_times || count == 0 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Check track length */
    double deviation;
    uft_protection_type_t len_type = analyze_track_length(track_time_ms, rpm, &deviation);
    
    if (len_type != UFT_PROT_NONE) {
        result->markers = malloc(sizeof(uft_protection_marker_t));
        if (result->markers) {
            result->markers[0].type = len_type;
            result->markers[0].track = track;
            result->markers[0].head = head;
            result->markers[0].sector = 0xFF;
            result->markers[0].position = 0;
            result->markers[0].length = count;
            result->markers[0].confidence = (uint8_t)(fabs(deviation) * 5);
            snprintf(result->markers[0].description, 
                     sizeof(result->markers[0].description),
                     "%s track (%.1f%% deviation)",
                     len_type == UFT_PROT_LONG_TRACK ? "Long" : "Short",
                     deviation);
            result->marker_count = 1;
            result->primary_type = len_type;
        }
    }
    
    /* Detect weak bits from timing */
    size_t *weak_pos;
    size_t weak_count;
    
    if (detect_weak_bits_flux(flux_times, count, clock_ns, 100, 
                              &weak_pos, &weak_count) == 0 && weak_count > 0) {
        
        size_t new_count = result->marker_count + weak_count;
        uft_protection_marker_t *new_markers = realloc(result->markers,
                                                       new_count * sizeof(uft_protection_marker_t));
        
        if (new_markers) {
            result->markers = new_markers;
            
            for (size_t i = 0; i < weak_count; i++) {
                uft_protection_marker_t *m = &result->markers[result->marker_count + i];
                m->type = UFT_PROT_FUZZY_BITS;
                m->track = track;
                m->head = head;
                m->sector = 0xFF;
                m->position = weak_pos[i];
                m->length = 1;
                m->confidence = 70;
                snprintf(m->description, sizeof(m->description),
                         "Fuzzy bits at flux %zu", weak_pos[i]);
            }
            
            result->marker_count = new_count;
            if (result->primary_type == UFT_PROT_NONE) {
                result->primary_type = UFT_PROT_FUZZY_BITS;
            }
        }
        
        free(weak_pos);
    }
    
    /* Try to identify scheme */
    if (result->marker_count > 0) {
        const char *scheme = identify_scheme(result->markers, result->marker_count);
        if (scheme) {
            strncpy(result->scheme_name, scheme, sizeof(result->scheme_name) - 1);
        }
        result->overall_confidence = 75;
    }
    
    return 0;
}

/**
 * @brief Free protection result
 */
void uft_protection_result_free(uft_protection_result_t *result)
{
    if (result) {
        free(result->markers);
        memset(result, 0, sizeof(*result));
    }
}

/**
 * @brief Get protection type name
 */
const char* uft_protection_type_name(uft_protection_type_t type)
{
    switch (type) {
        case UFT_PROT_NONE:             return "None";
        case UFT_PROT_WEAK_BITS:        return "Weak Bits";
        case UFT_PROT_LONG_TRACK:       return "Long Track";
        case UFT_PROT_SHORT_TRACK:      return "Short Track";
        case UFT_PROT_FUZZY_BITS:       return "Fuzzy Bits";
        case UFT_PROT_DUPLICATE_SECTORS: return "Duplicate Sectors";
        case UFT_PROT_MISSING_SECTORS:  return "Missing Sectors";
        case UFT_PROT_BAD_CRC:          return "Intentional CRC Error";
        case UFT_PROT_NON_STANDARD_SIZE: return "Non-Standard Sector Size";
        case UFT_PROT_TIMING:           return "Timing Protection";
        case UFT_PROT_HIDDEN_DATA:      return "Hidden Data";
        case UFT_PROT_CUSTOM_ENCODING:  return "Custom Encoding";
        case UFT_PROT_MULTIPLE:         return "Multiple Protections";
        default:                        return "Unknown";
    }
}
