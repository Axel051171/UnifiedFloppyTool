/**
 * @file uft_cbm_protection.h
 * @brief UnifiedFloppyTool - CBM/C64 Copy Protection Detection
 * @version 3.1.4.008
 *
 * Comprehensive Commodore 1541/C64 copy protection detection system.
 * Preservation-oriented - classifies protection traits without bypassing.
 *
 * Sources analyzed:
 * - ufm_cbm_protection_methods.c/h (Peter Rittwage method taxonomy)
 * - ufm_c64_rapidlok.c/h (RapidLok v1 detection)
 * - ufm_c64_rapidlok6.c/h (RapidLok v6 detection)
 * - ufm_c64_scheme_detect.c/h (scheme classification)
 * - ufm_c64_geos_gap_protection.c/h (GEOS gap validation)
 * - ufm_c64_ealoader.c/h (EA fat-track detection)
 */

#ifndef UFT_CBM_PROTECTION_H
#define UFT_CBM_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CBM Protection Methods (Rittwage Taxonomy)
 *============================================================================*/

typedef enum {
    UFT_CBM_PROT_UNKNOWN = 0,
    UFT_CBM_PROT_INTENTIONAL_ERRORS,
    UFT_CBM_PROT_TRACK_SKEW,
    UFT_CBM_PROT_FAT_TRACKS,
    UFT_CBM_PROT_HALF_TRACKS,
    UFT_CBM_PROT_EXTRA_TRACKS,
    UFT_CBM_PROT_CHANGED_BITRATES,
    UFT_CBM_PROT_GAP_SIGNATURES,
    UFT_CBM_PROT_LONG_SECTORS,
    UFT_CBM_PROT_CUSTOM_FORMATS,
    UFT_CBM_PROT_LONG_TRACKS,
    UFT_CBM_PROT_SYNC_COUNTING,
    UFT_CBM_PROT_TRACK_SYNCHRONIZATION,
    UFT_CBM_PROT_WEAK_BITS_UNFORMATTED,
    UFT_CBM_PROT_SIGNATURE_KEY_TRACKS,
    UFT_CBM_PROT_NO_SYNC,
    UFT_CBM_PROT_SPIRADISC_LIKE
} uft_cbm_prot_method_t;

/*============================================================================
 * C64 Protection Schemes
 *============================================================================*/

typedef enum {
    UFT_C64_SCHEME_UNKNOWN = 0,
    UFT_C64_SCHEME_GEOS_GAPDATA,
    UFT_C64_SCHEME_RAPIDLOK,
    UFT_C64_SCHEME_RAPIDLOK2,
    UFT_C64_SCHEME_RAPIDLOK6,
    UFT_C64_SCHEME_EA_FATTRACK,
    UFT_C64_SCHEME_VORPAL,
    UFT_C64_SCHEME_VMAX
} uft_c64_scheme_t;

/*============================================================================
 * RapidLok 6 Constants
 *============================================================================*/

#define UFT_RL6_MARK_EXTRA   0x7B
#define UFT_RL6_MARK_DOSREF  0x52
#define UFT_RL6_MARK_HDR     0x75
#define UFT_RL6_MARK_DATA    0x6B

typedef enum {
    UFT_RL6_TRK_UNKNOWN = 0,
    UFT_RL6_TRK_1_17,
    UFT_RL6_TRK_18_SPECIAL,
    UFT_RL6_TRK_19_35,
    UFT_RL6_TRK_36_KEY
} uft_rl6_track_group_t;

/*============================================================================
 * Per-Track Metrics
 *============================================================================*/

typedef struct {
    int      track_x2;
    uint8_t  revolutions;
    uint32_t bitlen_min;
    uint32_t bitlen_max;
    uint32_t weak_bits_total;
    uint32_t weak_bits_max_run;
    uint32_t max_sync_run_bits;
    bool     no_sync_detected;
    uint32_t illegal_gcr_events;
    uint32_t sector_count_observed;
    uint32_t sector_crc_failures;
    uint32_t sector_missing;
    uint32_t count_00;
    uint32_t count_52;
    uint32_t count_75;
    uint32_t count_6b;
    uint32_t count_7b;
    uint32_t gap_non55_bytes;
    bool     gap_length_weird;
    bool     nonstandard_bitrate;
    bool     has_meaningful_data;
    bool     has_index_reference;
    bool     track_alignment_locked;
} uft_cbm_track_metrics_t;

/*============================================================================
 * RapidLok Structures
 *============================================================================*/

typedef struct {
    int       track_x2;
    int       track_num;
    uint8_t   revolutions;
    const uint8_t *gcr;
    size_t    gcr_len;
    uint32_t  start_sync_bits;
    uint32_t  sector0_sync_bits;
    uint16_t  start_sync_ff_run;
    uint16_t  dosref_sync_ff_run;
    uint16_t  first_data_hdr_sync_ff_run;
} uft_rapidlok_track_t;

typedef struct {
    bool     gap_has_bad_gcr00;
    bool     start_sync_near_320;
    bool     sector0_sync_near_480;
    bool     key_track36_present;
    bool     trk34_35_sync_sensitive;
    bool     has_multi_rev_capture;
    int      confidence_0_100;
    char     summary[1024];
} uft_rapidlok_result_t;

/*============================================================================
 * GEOS Gap Protection
 *============================================================================*/

typedef struct {
    uint32_t sync_run_min;
    uint8_t  allowed_a;
    uint8_t  allowed_b;
    bool     require_trailing_67;
    int      track_number;
} uft_geos_gap_config_t;

typedef struct {
    int      track_number;
    uint32_t gaps_found;
    uint32_t gaps_bad_bytes;
    uint32_t gaps_bad_trailing;
    uint32_t bad_byte_count;
    int      confidence_0_100;
    char     summary[512];
} uft_geos_gap_result_t;

/*============================================================================
 * EA Fat-Track
 *============================================================================*/

typedef struct {
    bool     trk34_ok;
    bool     trk34p5_ok;
    bool     trk35_ok;
    bool     motor_reg_stable;
    uint8_t  revs_trk34;
    uint8_t  revs_trk34p5;
    uint8_t  revs_trk35;
} uft_ea_fattrack_obs_t;

/*============================================================================
 * Detection Results
 *============================================================================*/

typedef struct {
    uft_cbm_prot_method_t method;
    int      track_x2;
    int      confidence_0_100;
} uft_cbm_method_hit_t;

typedef struct {
    uft_c64_scheme_t scheme;
    int      confidence_0_100;
} uft_c64_scheme_hit_t;

typedef struct {
    int      overall_confidence;
    bool     protection_likely;
    bool     multi_rev_recommended;
    size_t   method_hits_count;
    size_t   scheme_hits_count;
    char     summary[2048];
} uft_cbm_protection_report_t;

/*============================================================================
 * Helper Functions
 *============================================================================*/

static inline bool uft_within_tolerance(uint32_t v, uint32_t t, uint32_t tol)
{
    return v && (v >= t - tol) && (v <= t + tol);
}

static inline int uft_clamp_100(int v) { return v<0?0:(v>100?100:v); }

static inline int uft_c64_speed_zone(int track)
{
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

static inline uft_rl6_track_group_t uft_rl6_get_track_group(int track)
{
    if (track == 18) return UFT_RL6_TRK_18_SPECIAL;
    if (track == 36) return UFT_RL6_TRK_36_KEY;
    if (track >= 1 && track <= 17) return UFT_RL6_TRK_1_17;
    if (track >= 19 && track <= 35) return UFT_RL6_TRK_19_35;
    return UFT_RL6_TRK_UNKNOWN;
}

/*============================================================================
 * Trait Scoring Functions
 *============================================================================*/

static inline int uft_score_weak_bits(const uft_cbm_track_metrics_t *t)
{
    if (!t || t->revolutions < 2) return 0;
    int s = 0;
    if (t->weak_bits_max_run >= 256) s += 40;
    else if (t->weak_bits_max_run >= 128) s += 25;
    else if (t->weak_bits_max_run >= 64) s += 15;
    if (t->weak_bits_total >= 2048) s += 45;
    else if (t->weak_bits_total >= 1024) s += 30;
    else if (t->weak_bits_total >= 512) s += 20;
    return uft_clamp_100(s);
}

static inline int uft_score_long_track(const uft_cbm_track_metrics_t *t)
{
    if (!t || t->bitlen_max == 0) return 0;
    if (t->bitlen_max >= 240000) return 90;
    if (t->bitlen_max >= 225000) return 70;
    if (t->bitlen_max >= 210000) return 50;
    return 0;
}

static inline int uft_score_long_sync(const uft_cbm_track_metrics_t *t)
{
    if (!t || t->max_sync_run_bits == 0) return 0;
    if (t->max_sync_run_bits >= 1400) return 85;
    if (t->max_sync_run_bits >= 1000) return 65;
    if (t->max_sync_run_bits >= 700) return 40;
    return 0;
}

static inline int uft_score_illegal_gcr(const uft_cbm_track_metrics_t *t)
{
    if (!t) return 0;
    if (t->illegal_gcr_events >= 200) return 90;
    if (t->illegal_gcr_events >= 50) return 70;
    if (t->illegal_gcr_events >= 10) return 45;
    return 0;
}

static inline int uft_score_half_track(const uft_cbm_track_metrics_t *t)
{
    if (!t) return 0;
    if ((t->track_x2 % 2) == 0) return 0;
    if (t->has_meaningful_data) return 80;
    if (t->illegal_gcr_events || t->count_75 || t->count_6b || t->count_7b) return 75;
    return 35;
}

static inline int uft_score_gap_signatures(const uft_cbm_track_metrics_t *t)
{
    if (!t) return 0;
    int s = 0;
    if (t->gap_non55_bytes >= 32) s += 55;
    else if (t->gap_non55_bytes >= 8) s += 35;
    if (t->gap_length_weird) s += 20;
    return uft_clamp_100(s);
}

static inline int uft_score_errors(const uft_cbm_track_metrics_t *t)
{
    if (!t) return 0;
    uint32_t e = t->sector_crc_failures + t->sector_missing;
    if (e >= 40) return 85;
    if (e >= 10) return 60;
    if (e >= 3) return 35;
    return 0;
}

/*============================================================================
 * API Functions
 *============================================================================*/

const char* uft_cbm_method_name(uft_cbm_prot_method_t m);
const char* uft_c64_scheme_name(uft_c64_scheme_t s);

bool uft_cbm_analyze_protection(const uft_cbm_track_metrics_t *tracks,
                                 size_t track_count,
                                 uft_cbm_method_hit_t *hits, size_t hits_cap,
                                 uft_cbm_protection_report_t *report);

bool uft_rapidlok_analyze(const uft_rapidlok_track_t *tracks,
                          size_t track_count,
                          uft_rapidlok_result_t *result);

bool uft_geos_gap_validate(const uint8_t *track_bytes, size_t track_len,
                           const uft_geos_gap_config_t *config,
                           uft_geos_gap_result_t *result);

uint32_t uft_geos_gap_rewrite(uint8_t *track_bytes, size_t track_len,
                              const uft_geos_gap_config_t *config);

int uft_ea_fattrack_score(const uft_ea_fattrack_obs_t *obs,
                          char *reason_buf, size_t reason_cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_PROTECTION_H */
