/**
 * @file uft_rapidlok.h
 * @brief Rapidlok / Half-Track Protection Detection
 * 
 * P3-004: C64 Half-Track Protection Scanner
 * 
 * Rapidlok was a popular copy protection for C64 disks that used:
 * - Half-tracks (17.5, 18.5, etc.)
 * - Non-standard timing
 * - Hidden sectors
 * - Track 36+ data
 */

#ifndef UFT_RAPIDLOK_H
#define UFT_RAPIDLOK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_PROT_NONE = 0,
    UFT_PROT_RAPIDLOK,          /* Rapidlok v1-v3 */
    UFT_PROT_RAPIDLOK_PLUS,     /* Enhanced Rapidlok */
    UFT_PROT_VORPAL,            /* Vorpal fast loader */
    UFT_PROT_V_MAX,             /* V-Max protection */
    UFT_PROT_GCR_TIMING,        /* Non-standard GCR timing */
    UFT_PROT_HALF_TRACK,        /* Generic half-track data */
    UFT_PROT_TRACK_36_PLUS,     /* Data beyond track 35 */
    UFT_PROT_FAT_TRACK,         /* Extended track data */
    UFT_PROT_SYNC_MARK,         /* Non-standard sync marks */
    UFT_PROT_UNKNOWN,           /* Detected but unidentified */
} uft_c64_protection_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Results
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool                    detected;
    uft_c64_protection_t    type;
    double                  confidence;     /* 0.0 - 1.0 */
    
    /* Half-track info */
    bool                    has_half_tracks;
    uint8_t                 half_tracks[42];    /* Bitmap: bit set = half-track present */
    int                     half_track_count;
    
    /* Extended tracks */
    bool                    has_extended_tracks;
    int                     max_track;          /* Highest track with data */
    int                     extended_track_count;
    
    /* Timing anomalies */
    bool                    has_timing_anomaly;
    double                  timing_deviation;   /* Percentage deviation from normal */
    
    /* Signature */
    char                    signature[64];      /* Human-readable signature */
    uint32_t                signature_hash;
    
    /* Specific detection results */
    int                     rapidlok_version;   /* 1, 2, or 3 if Rapidlok */
    int                     key_track;          /* Track containing protection key */
    int                     key_sector;         /* Sector containing protection key */
} uft_c64_protection_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Scanner Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool        scan_half_tracks;   /* Scan half-track positions */
    bool        scan_extended;      /* Scan beyond track 35 */
    bool        analyze_timing;     /* Analyze flux timing */
    bool        deep_scan;          /* More thorough (slower) */
    int         max_track;          /* Maximum track to scan (default 42) */
} uft_rapidlok_options_t;

typedef struct uft_rapidlok_scanner uft_rapidlok_scanner_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create scanner
 */
uft_rapidlok_scanner_t* uft_rapidlok_create(const uft_rapidlok_options_t *options);

/**
 * @brief Destroy scanner
 */
void uft_rapidlok_destroy(uft_rapidlok_scanner_t *scanner);

/**
 * @brief Scan raw flux data for protection
 * @param scanner Scanner instance
 * @param flux_data Flux timing data
 * @param flux_count Number of flux entries
 * @param track Track number (can be half-track like 17.5)
 * @param result Output detection result
 * @return 0 on success
 */
int uft_rapidlok_scan_flux(uft_rapidlok_scanner_t *scanner,
                           const double *flux_data, size_t flux_count,
                           double track,
                           uft_c64_protection_result_t *result);

/**
 * @brief Scan GCR decoded data for protection
 */
int uft_rapidlok_scan_gcr(uft_rapidlok_scanner_t *scanner,
                          const uint8_t *gcr_data, size_t size,
                          int track,
                          uft_c64_protection_result_t *result);

/**
 * @brief Scan entire disk image
 */
int uft_rapidlok_scan_disk(uft_rapidlok_scanner_t *scanner,
                           const char *image_path,
                           uft_c64_protection_result_t *result);

/**
 * @brief Get protection type name
 */
const char* uft_c64_protection_name(uft_c64_protection_t type);

/**
 * @brief Check if track is a half-track
 */
static inline bool uft_is_half_track(double track) {
    double frac = track - (int)track;
    return (frac > 0.25 && frac < 0.75);
}

/**
 * @brief Convert track number to index
 */
static inline int uft_track_to_index(double track) {
    return (int)(track * 2);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_RAPIDLOK_H */
