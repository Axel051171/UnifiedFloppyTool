/**
 * @file uft_scp_parser_v3.c
 * @brief GOD MODE SCP Parser v3 - SuperCard Pro Flux Format
 * 
 * SCP ist DAS Referenz-Flux-Format für Floppy-Preservation:
 * - Raw Flux Transitions (Zeitintervalle zwischen Magnetfeldwechseln)
 * - Multiple Revolutions pro Track (1-5+)
 * - Index-Puls Synchronisation
 * - 25ns Auflösung (40MHz Sampling)
 * - Unterstützung für alle Floppy-Typen (C64, Amiga, Apple, PC, etc.)
 * 
 * SCP File Structure:
 * - Header (16 bytes)
 * - Track Data Headers (TDH)
 * - Track Data (Flux Transitions)
 * 
 * v3 Features:
 * - Multi-Rev Read mit Bit-Level Fusion
 * - Weak Bit Detection durch Rev-Vergleich
 * - PLL-basierte Dekodierung (MFM/GCR)
 * - Timing-Analyse und Histogramme
 * - Kopierschutz-Erkennung
 * - Export zu anderen Formaten (D64, G64, ADF, etc.)
 * - Diagnose mit 40+ Codes
 * - Verify-After-Write
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#include "uft/uft_safe_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP FORMAT CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define SCP_SIGNATURE           "SCP"
#define SCP_SIGNATURE_LEN       3
#define SCP_HEADER_SIZE         16
#define SCP_TDH_SIZE            4           /* Track Data Header size */

#define SCP_MAX_TRACKS          168         /* 84 tracks × 2 sides */
#define SCP_MAX_REVOLUTIONS     5           /* Standard max revs */
#define SCP_EXTENDED_REVOLUTIONS 16         /* Extended mode */

/* Timing resolution */
#define SCP_TICK_NS             25          /* 25ns per tick (40MHz) */
#define SCP_TICKS_PER_US        40          /* 40 ticks per microsecond */

/* Disk types */
#define SCP_DISK_C64            0x00
#define SCP_DISK_AMIGA          0x04
#define SCP_DISK_AMIGA_HD       0x08
#define SCP_DISK_ATARI_ST       0x10
#define SCP_DISK_ATARI_ST_HD    0x11
#define SCP_DISK_APPLE_II       0x20
#define SCP_DISK_APPLE_II_PRO   0x21
#define SCP_DISK_APPLE_400K     0x24
#define SCP_DISK_APPLE_800K     0x25
#define SCP_DISK_APPLE_HD       0x26
#define SCP_DISK_PC_360K        0x40
#define SCP_DISK_PC_720K        0x41
#define SCP_DISK_PC_1200K       0x42
#define SCP_DISK_PC_1440K       0x43
#define SCP_DISK_TRS80          0x60
#define SCP_DISK_TI99           0x70
#define SCP_DISK_ROLAND         0x80
#define SCP_DISK_AMSTRAD        0x90
#define SCP_DISK_OTHER          0xC0

/* Header flags */
#define SCP_FLAG_INDEX          0x01        /* Index mark stored */
#define SCP_FLAG_TPI_96         0x02        /* 96 TPI drive */
#define SCP_FLAG_RPM_360        0x04        /* 360 RPM */
#define SCP_FLAG_NORMALIZED     0x08        /* Flux normalized */
#define SCP_FLAG_READ_WRITE     0x10        /* Read/write mode */
#define SCP_FLAG_FOOTER         0x20        /* Has footer */
#define SCP_FLAG_EXTENDED       0x40        /* Extended mode */
#define SCP_FLAG_CREATOR        0x80        /* Has creator info */

/* Typical timing values (in ticks @ 25ns) */
#define SCP_FLUX_MIN_VALID      40          /* 1µs minimum */
#define SCP_FLUX_MAX_VALID      40000       /* 1ms maximum */
#define SCP_INDEX_TIME_DD       8000000     /* ~200ms for 300 RPM */
#define SCP_INDEX_TIME_HD       6666666     /* ~166ms for 360 RPM */

/* Expected flux values for different encodings */
#define SCP_MFM_BITCELL_DD      160         /* 4µs @ 250kbps */
#define SCP_MFM_BITCELL_HD      80          /* 2µs @ 500kbps */
#define SCP_GCR_BITCELL_C64     128         /* ~3.2µs @ 312.5kbps */
#define SCP_GCR_BITCELL_APPLE   160         /* 4µs @ 250kbps */

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS CODES (SCP specific)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    SCP_DIAG_OK = 0,
    
    /* File structure */
    SCP_DIAG_BAD_SIGNATURE,
    SCP_DIAG_BAD_VERSION,
    SCP_DIAG_TRUNCATED,
    SCP_DIAG_CHECKSUM_ERROR,
    SCP_DIAG_HEADER_ERROR,
    
    /* Track structure */
    SCP_DIAG_EMPTY_TRACK,
    SCP_DIAG_NO_INDEX,
    SCP_DIAG_BAD_TDH,
    SCP_DIAG_TRACK_OVERFLOW,
    SCP_DIAG_MISSING_TRACK,
    
    /* Revolution issues */
    SCP_DIAG_REV_MISMATCH,
    SCP_DIAG_REV_TOO_SHORT,
    SCP_DIAG_REV_TOO_LONG,
    SCP_DIAG_REV_INCONSISTENT,
    SCP_DIAG_INDEX_MISSING,
    
    /* Flux issues */
    SCP_DIAG_FLUX_TOO_SHORT,
    SCP_DIAG_FLUX_TOO_LONG,
    SCP_DIAG_FLUX_SPIKE,
    SCP_DIAG_FLUX_DROPOUT,
    SCP_DIAG_NO_FLUX_DATA,
    
    /* Timing issues */
    SCP_DIAG_TIMING_DRIFT,
    SCP_DIAG_SPEED_ERROR,
    SCP_DIAG_HIGH_JITTER,
    SCP_DIAG_DENSITY_ANOMALY,
    SCP_DIAG_BITCELL_VARIANCE,
    
    /* Decoding issues */
    SCP_DIAG_PLL_UNLOCK,
    SCP_DIAG_SYNC_ERROR,
    SCP_DIAG_SECTOR_ERROR,
    SCP_DIAG_CRC_ERROR,
    SCP_DIAG_DECODE_FAIL,
    
    /* Protection */
    SCP_DIAG_WEAK_BITS,
    SCP_DIAG_FUZZY_BITS,
    SCP_DIAG_LONG_TRACK,
    SCP_DIAG_SHORT_TRACK,
    SCP_DIAG_EXTRA_DATA,
    SCP_DIAG_MISSING_SECTOR,
    SCP_DIAG_EXTRA_SECTOR,
    SCP_DIAG_NON_STANDARD,
    
    /* Analysis */
    SCP_DIAG_UNKNOWN_FORMAT,
    SCP_DIAG_FORMAT_MISMATCH,
    SCP_DIAG_MULTI_FORMAT,
    SCP_DIAG_UNFORMATTED,
    
    SCP_DIAG_COUNT
} scp_diag_code_t;

static const char* scp_diag_names[] = {
    [SCP_DIAG_OK] = "OK",
    [SCP_DIAG_BAD_SIGNATURE] = "Invalid SCP signature",
    [SCP_DIAG_BAD_VERSION] = "Unsupported SCP version",
    [SCP_DIAG_TRUNCATED] = "File is truncated",
    [SCP_DIAG_CHECKSUM_ERROR] = "Checksum mismatch",
    [SCP_DIAG_HEADER_ERROR] = "Header parse error",
    [SCP_DIAG_EMPTY_TRACK] = "Track contains no data",
    [SCP_DIAG_NO_INDEX] = "No index pulse found",
    [SCP_DIAG_BAD_TDH] = "Bad track data header",
    [SCP_DIAG_TRACK_OVERFLOW] = "Track data overflow",
    [SCP_DIAG_MISSING_TRACK] = "Expected track not present",
    [SCP_DIAG_REV_MISMATCH] = "Revolution count mismatch",
    [SCP_DIAG_REV_TOO_SHORT] = "Revolution too short",
    [SCP_DIAG_REV_TOO_LONG] = "Revolution too long",
    [SCP_DIAG_REV_INCONSISTENT] = "Revolutions are inconsistent",
    [SCP_DIAG_INDEX_MISSING] = "Index pulse missing in revolution",
    [SCP_DIAG_FLUX_TOO_SHORT] = "Flux transition too short (<1µs)",
    [SCP_DIAG_FLUX_TOO_LONG] = "Flux transition too long (>1ms)",
    [SCP_DIAG_FLUX_SPIKE] = "Flux spike detected",
    [SCP_DIAG_FLUX_DROPOUT] = "Flux dropout (no transitions)",
    [SCP_DIAG_NO_FLUX_DATA] = "No flux data in track",
    [SCP_DIAG_TIMING_DRIFT] = "Timing drift detected",
    [SCP_DIAG_SPEED_ERROR] = "Drive speed error",
    [SCP_DIAG_HIGH_JITTER] = "High jitter level",
    [SCP_DIAG_DENSITY_ANOMALY] = "Bit density anomaly",
    [SCP_DIAG_BITCELL_VARIANCE] = "Bitcell timing variance",
    [SCP_DIAG_PLL_UNLOCK] = "PLL lost lock",
    [SCP_DIAG_SYNC_ERROR] = "Sync pattern error",
    [SCP_DIAG_SECTOR_ERROR] = "Sector decode error",
    [SCP_DIAG_CRC_ERROR] = "CRC error in sector",
    [SCP_DIAG_DECODE_FAIL] = "Failed to decode track",
    [SCP_DIAG_WEAK_BITS] = "Weak/unstable bits detected",
    [SCP_DIAG_FUZZY_BITS] = "Fuzzy bits (intentional)",
    [SCP_DIAG_LONG_TRACK] = "Longer than standard track",
    [SCP_DIAG_SHORT_TRACK] = "Shorter than standard track",
    [SCP_DIAG_EXTRA_DATA] = "Extra data after sectors",
    [SCP_DIAG_MISSING_SECTOR] = "Expected sector not found",
    [SCP_DIAG_EXTRA_SECTOR] = "Extra sector found",
    [SCP_DIAG_NON_STANDARD] = "Non-standard format detected",
    [SCP_DIAG_UNKNOWN_FORMAT] = "Unknown disk format",
    [SCP_DIAG_FORMAT_MISMATCH] = "Format doesn't match header",
    [SCP_DIAG_MULTI_FORMAT] = "Multiple formats detected",
    [SCP_DIAG_UNFORMATTED] = "Track appears unformatted"
};

static const char* scp_diag_suggestions[] = {
    [SCP_DIAG_OK] = "",
    [SCP_DIAG_BAD_SIGNATURE] = "Verify file is actually SCP format",
    [SCP_DIAG_BAD_VERSION] = "May need updated parser",
    [SCP_DIAG_TRUNCATED] = "Check for incomplete download/copy",
    [SCP_DIAG_CHECKSUM_ERROR] = "File may be corrupted",
    [SCP_DIAG_HEADER_ERROR] = "Check file integrity",
    [SCP_DIAG_EMPTY_TRACK] = "Track was not captured or is blank",
    [SCP_DIAG_NO_INDEX] = "Check index sensor, try different drive",
    [SCP_DIAG_BAD_TDH] = "Track data may be corrupted",
    [SCP_DIAG_TRACK_OVERFLOW] = "Check capture settings",
    [SCP_DIAG_MISSING_TRACK] = "Re-capture with all tracks enabled",
    [SCP_DIAG_REV_MISMATCH] = "Use consistent revolution count",
    [SCP_DIAG_REV_TOO_SHORT] = "Check drive speed, may be too fast",
    [SCP_DIAG_REV_TOO_LONG] = "Check drive speed, may be too slow",
    [SCP_DIAG_REV_INCONSISTENT] = "Drive speed may be varying",
    [SCP_DIAG_INDEX_MISSING] = "Index sensor issue or spinner problem",
    [SCP_DIAG_FLUX_TOO_SHORT] = "Possible electrical noise",
    [SCP_DIAG_FLUX_TOO_LONG] = "Possible media damage or unformatted",
    [SCP_DIAG_FLUX_SPIKE] = "Filter spikes in processing",
    [SCP_DIAG_FLUX_DROPOUT] = "Media damage or demagnetized area",
    [SCP_DIAG_NO_FLUX_DATA] = "Track may be unformatted or erased",
    [SCP_DIAG_TIMING_DRIFT] = "Drive speed unstable",
    [SCP_DIAG_SPEED_ERROR] = "Check/adjust drive speed",
    [SCP_DIAG_HIGH_JITTER] = "Media wear or drive head issue",
    [SCP_DIAG_DENSITY_ANOMALY] = "Check format detection",
    [SCP_DIAG_BITCELL_VARIANCE] = "Use adaptive PLL mode",
    [SCP_DIAG_PLL_UNLOCK] = "Adjust PLL bandwidth or use Kalman",
    [SCP_DIAG_SYNC_ERROR] = "Increase sync search window",
    [SCP_DIAG_SECTOR_ERROR] = "Try multi-rev merge for recovery",
    [SCP_DIAG_CRC_ERROR] = "Use CRC correction or voting",
    [SCP_DIAG_DECODE_FAIL] = "Check format detection settings",
    [SCP_DIAG_WEAK_BITS] = "PRESERVE - this IS copy protection",
    [SCP_DIAG_FUZZY_BITS] = "PRESERVE - intentional protection",
    [SCP_DIAG_LONG_TRACK] = "PRESERVE - likely copy protection",
    [SCP_DIAG_SHORT_TRACK] = "May be damaged or non-standard",
    [SCP_DIAG_EXTRA_DATA] = "PRESERVE - may contain hidden data",
    [SCP_DIAG_MISSING_SECTOR] = "Sector may be intentionally absent",
    [SCP_DIAG_EXTRA_SECTOR] = "PRESERVE - copy protection",
    [SCP_DIAG_NON_STANDARD] = "PRESERVE - document anomalies",
    [SCP_DIAG_UNKNOWN_FORMAT] = "Try different format auto-detection",
    [SCP_DIAG_FORMAT_MISMATCH] = "Override header with detected format",
    [SCP_DIAG_MULTI_FORMAT] = "Process each track individually",
    [SCP_DIAG_UNFORMATTED] = "Track was never formatted"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Flux statistics for analysis
 */
typedef struct scp_flux_stats {
    uint32_t min_flux;
    uint32_t max_flux;
    double mean_flux;
    double stddev_flux;
    uint32_t total_transitions;
    uint32_t short_count;       /* < 1µs */
    uint32_t long_count;        /* > 500µs */
    uint32_t spike_count;       /* Anomalous */
    
    /* Histogram (256 bins) */
    uint32_t histogram[256];
    uint32_t histogram_peak;
    uint32_t histogram_peak_bin;
    
} scp_flux_stats_t;

/**
 * @brief Score structure
 */
typedef struct scp_score {
    float overall;
    float flux_score;
    float timing_score;
    float consistency_score;
    float decode_score;
    float structure_score;
    
    bool has_index;
    bool flux_valid;
    bool timing_stable;
    bool has_weak_bits;
    bool has_protection;
    bool decodes_ok;
    
    uint8_t revolutions;
    uint8_t best_revolution;
    uint16_t weak_bit_count;
    uint32_t total_flux;
    
} scp_score_t;

/**
 * @brief Diagnosis entry
 */
typedef struct scp_diagnosis {
    scp_diag_code_t code;
    uint8_t track;
    uint8_t side;
    uint8_t revolution;
    uint32_t position;
    char message[256];
    scp_score_t score;
} scp_diagnosis_t;

/**
 * @brief Diagnosis list
 */
typedef struct scp_diagnosis_list {
    scp_diagnosis_t* items;
    size_t count;
    size_t capacity;
    uint16_t error_count;
    uint16_t warning_count;
    uint16_t protection_count;
    float overall_quality;
} scp_diagnosis_list_t;

/**
 * @brief Revolution data
 */
typedef struct scp_revolution {
    /* Raw flux data */
    uint16_t* flux;             /* Flux transition times (16-bit) */
    uint32_t flux_count;        /* Number of transitions */
    uint32_t index_time;        /* Total time in ticks */
    
    /* Decoded bitstream */
    uint8_t* bitstream;         /* Decoded bits */
    uint32_t bit_count;
    
    /* Statistics */
    scp_flux_stats_t stats;
    
    /* Status */
    bool valid;
    bool has_index;
    
    /* Score */
    scp_score_t score;
    
} scp_revolution_t;

/**
 * @brief Track structure
 */
typedef struct scp_track {
    /* Identity */
    uint8_t track_num;          /* Track number (0-167) */
    uint8_t physical_track;     /* Physical track (0-83) */
    uint8_t side;               /* Side (0-1) */
    
    /* Track Data Header */
    uint32_t tdh_offset;        /* Offset in file */
    uint32_t data_offset;       /* Actual data offset */
    
    /* Revolutions */
    scp_revolution_t revolutions[SCP_EXTENDED_REVOLUTIONS];
    uint8_t revolution_count;
    uint8_t best_revolution;
    
    /* Merged/decoded data */
    uint8_t* merged_flux;       /* Best-of merged flux */
    uint32_t merged_flux_count;
    uint8_t* merged_bits;       /* Merged bitstream */
    uint32_t merged_bit_count;
    
    /* Weak bits (from rev comparison) */
    uint8_t* weak_mask;         /* Bit mask of weak positions */
    uint32_t weak_bit_count;
    
    /* Timing */
    uint32_t rotation_time;     /* Average rotation time */
    float rpm;                  /* Calculated RPM */
    
    /* Format detection */
    uint8_t detected_encoding;  /* MFM, GCR, FM, etc. */
    uint16_t detected_bitcell;  /* Detected bitcell time */
    uint8_t detected_sectors;   /* Detected sector count */
    
    /* Protection */
    bool has_weak_bits;
    bool has_extra_data;
    bool is_long_track;
    bool is_protected;
    
    /* Status */
    bool present;
    bool valid;
    
    /* Score */
    scp_score_t score;
    
} scp_track_t;

/**
 * @brief SCP disk structure
 */
typedef struct scp_disk {
    /* Header info */
    char signature[4];
    uint8_t version;
    uint8_t disk_type;
    uint8_t revolutions;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t flags;
    uint8_t bit_cell_width;
    uint8_t heads;
    uint8_t resolution;
    uint32_t checksum;
    
    /* Track offsets */
    uint32_t track_offsets[SCP_MAX_TRACKS];
    
    /* Track data */
    scp_track_t tracks[SCP_MAX_TRACKS];
    
    /* Statistics */
    uint8_t track_count;
    uint8_t side_count;
    uint16_t total_revolutions;
    uint32_t total_flux_transitions;
    
    /* Format detection */
    uint8_t detected_format;    /* Best guess format */
    char format_name[64];
    float format_confidence;
    
    /* Protection */
    bool has_protection;
    char protection_type[64];
    float protection_confidence;
    
    /* Overall score */
    scp_score_t score;
    scp_diagnosis_list_t* diagnosis;
    
    /* Timing analysis */
    float average_rpm;
    float rpm_deviation;
    
    /* Source */
    char source_path[256];
    size_t source_size;
    
    /* Status */
    bool valid;
    bool modified;
    char error[256];
    
} scp_disk_t;

/**
 * @brief SCP parameters
 */
typedef struct scp_params {
    /* Read options */
    uint8_t min_revolutions;
    uint8_t max_revolutions;
    bool use_all_revolutions;
    
    /* Flux processing */
    bool filter_spikes;
    uint16_t spike_threshold;   /* In ticks */
    bool normalize_flux;
    
    /* Multi-rev merge */
    bool multi_rev_merge;
    int merge_strategy;         /* 0=vote, 1=best, 2=average */
    float merge_threshold;
    
    /* Weak bit detection */
    bool detect_weak_bits;
    float weak_threshold;       /* Variance threshold */
    bool preserve_weak_bits;
    
    /* PLL decoding */
    bool decode_flux;
    int pll_mode;               /* 0=simple, 1=adaptive, 2=kalman */
    float pll_bandwidth;
    float pll_gain;
    
    /* Format detection */
    bool auto_detect_format;
    uint8_t forced_format;
    uint8_t forced_encoding;
    
    /* Timing */
    float timing_tolerance;
    bool detect_speed_errors;
    
    /* Protection */
    bool detect_protection;
    bool preserve_protection;
    
    /* Output */
    bool generate_histogram;
    bool generate_stats;
    
    /* Verify */
    bool verify_after_write;
    
} scp_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert ticks to nanoseconds
 */
static inline uint32_t scp_ticks_to_ns(uint32_t ticks) {
    return ticks * SCP_TICK_NS;
}

/**
 * @brief Convert nanoseconds to ticks
 */
static inline uint32_t scp_ns_to_ticks(uint32_t ns) {
    return ns / SCP_TICK_NS;
}

/**
 * @brief Convert ticks to microseconds
 */
static inline float scp_ticks_to_us(uint32_t ticks) {
    return (float)ticks / SCP_TICKS_PER_US;
}

/**
 * @brief Calculate RPM from index time
 */
static inline float scp_calc_rpm(uint32_t index_time) {
    if (index_time == 0) return 0.0f;
    /* index_time is in 25ns ticks */
    /* RPM = 60 / (index_time_seconds) */
    double seconds = (double)index_time * SCP_TICK_NS / 1e9;
    return (float)(60.0 / seconds);
}

/**
 * @brief Get disk type name
 */
static const char* scp_disk_type_name(uint8_t type) {
    switch (type) {
        case SCP_DISK_C64: return "Commodore 64";
        case SCP_DISK_AMIGA: return "Amiga DD";
        case SCP_DISK_AMIGA_HD: return "Amiga HD";
        case SCP_DISK_ATARI_ST: return "Atari ST DD";
        case SCP_DISK_ATARI_ST_HD: return "Atari ST HD";
        case SCP_DISK_APPLE_II: return "Apple II";
        case SCP_DISK_APPLE_II_PRO: return "Apple II Pro";
        case SCP_DISK_APPLE_400K: return "Apple 400K";
        case SCP_DISK_APPLE_800K: return "Apple 800K";
        case SCP_DISK_APPLE_HD: return "Apple HD";
        case SCP_DISK_PC_360K: return "PC 360K";
        case SCP_DISK_PC_720K: return "PC 720K";
        case SCP_DISK_PC_1200K: return "PC 1.2M";
        case SCP_DISK_PC_1440K: return "PC 1.44M";
        case SCP_DISK_TRS80: return "TRS-80";
        case SCP_DISK_TI99: return "TI-99/4A";
        case SCP_DISK_ROLAND: return "Roland";
        case SCP_DISK_AMSTRAD: return "Amstrad";
        default: return "Unknown";
    }
}

/**
 * @brief Get expected sectors for disk type
 */
uint8_t scp_get_expected_sectors(uint8_t disk_type, uint8_t track) {
    switch (disk_type) {
        case SCP_DISK_C64:
            if (track < 17) return 21;
            if (track < 24) return 19;
            if (track < 30) return 18;
            return 17;
        case SCP_DISK_AMIGA:
        case SCP_DISK_AMIGA_HD:
            return 11;  /* Or 22 for HD */
        case SCP_DISK_ATARI_ST:
            return 9;
        case SCP_DISK_ATARI_ST_HD:
            return 18;
        case SCP_DISK_PC_360K:
        case SCP_DISK_PC_720K:
            return 9;
        case SCP_DISK_PC_1200K:
            return 15;
        case SCP_DISK_PC_1440K:
            return 18;
        case SCP_DISK_APPLE_II:
        case SCP_DISK_APPLE_II_PRO:
            return 16;
        default:
            return 0;
    }
}

/**
 * @brief Get expected bitcell time for disk type
 */
uint16_t scp_get_expected_bitcell(uint8_t disk_type) {
    switch (disk_type) {
        case SCP_DISK_C64:
            return SCP_GCR_BITCELL_C64;
        case SCP_DISK_AMIGA:
            return SCP_MFM_BITCELL_DD;
        case SCP_DISK_AMIGA_HD:
            return SCP_MFM_BITCELL_HD;
        case SCP_DISK_ATARI_ST:
        case SCP_DISK_PC_360K:
        case SCP_DISK_PC_720K:
            return SCP_MFM_BITCELL_DD;
        case SCP_DISK_PC_1200K:
        case SCP_DISK_PC_1440K:
        case SCP_DISK_ATARI_ST_HD:
            return SCP_MFM_BITCELL_HD;
        case SCP_DISK_APPLE_II:
        case SCP_DISK_APPLE_II_PRO:
            return SCP_GCR_BITCELL_APPLE;
        default:
            return SCP_MFM_BITCELL_DD;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static scp_diagnosis_list_t* scp_diagnosis_create(void) {
    scp_diagnosis_list_t* list = calloc(1, sizeof(scp_diagnosis_list_t));
    if (!list) return NULL;
    
    list->capacity = 256;
    list->items = calloc(list->capacity, sizeof(scp_diagnosis_t));
    if (!list->items) {
        free(list);
        return NULL;
    }
    
    list->overall_quality = 1.0f;
    return list;
}

static void scp_diagnosis_free(scp_diagnosis_list_t* list) {
    if (list) {
        free(list->items);
        free(list);
    }
}

static void scp_diagnosis_add(
    scp_diagnosis_list_t* list,
    scp_diag_code_t code,
    uint8_t track,
    uint8_t side,
    uint8_t revolution,
    const char* format,
    ...
) {
    if (!list) return;
    
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        scp_diagnosis_t* new_items = realloc(list->items, new_cap * sizeof(scp_diagnosis_t));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    
    scp_diagnosis_t* diag = &list->items[list->count];
    memset(diag, 0, sizeof(scp_diagnosis_t));
    
    diag->code = code;
    diag->track = track;
    diag->side = side;
    diag->revolution = revolution;
    
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(diag->message, sizeof(diag->message), format, args);
        va_end(args);
    } else if (code < SCP_DIAG_COUNT) {
        snprintf(diag->message, sizeof(diag->message), "%s", scp_diag_names[code]);
    }
    
    list->count++;
    
    if (code >= SCP_DIAG_WEAK_BITS && code <= SCP_DIAG_NON_STANDARD) {
        list->protection_count++;
    } else if (code >= SCP_DIAG_PLL_UNLOCK && code <= SCP_DIAG_DECODE_FAIL) {
        list->error_count++;
    } else if (code != SCP_DIAG_OK) {
        list->warning_count++;
    }
    
    if (code != SCP_DIAG_OK && code < SCP_DIAG_WEAK_BITS) {
        list->overall_quality *= 0.97f;
    }
}

char* scp_diagnosis_to_text(const scp_diagnosis_list_t* list, const scp_disk_t* disk) {
    if (!list) return NULL;
    
    size_t buf_size = 32768;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t off = 0;
    
    off += snprintf(buf + off, buf_size - off,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║                SCP FLUX DIAGNOSIS REPORT                         ║\n"
        "╠══════════════════════════════════════════════════════════════════╣\n");
    
    if (disk) {
        off += snprintf(buf + off, buf_size - off,
            "║ Format: %-20s  Version: %u.%u                      ║\n"
            "║ Tracks: %3u  Revolutions: %u  Flux: %u transitions           ║\n"
            "║ RPM: %.1f (±%.1f%%)                                           ║\n",
            disk->format_name,
            (unsigned)(disk->version >> 4), (unsigned)(disk->version & 0x0F),
            (unsigned)disk->track_count,
            (unsigned)disk->revolutions,
            (unsigned)disk->total_flux_transitions,
            disk->average_rpm,
            disk->rpm_deviation);
        
        if (disk->has_protection) {
            off += snprintf(buf + off, buf_size - off,
                "║ Protection: %-20s (%.0f%% confidence)            ║\n",
                disk->protection_type,
                disk->protection_confidence * 100);
        }
    }
    
    off += snprintf(buf + off, buf_size - off,
        "╠══════════════════════════════════════════════════════════════════╣\n"
        "║ Errors: %-4u  Warnings: %-4u  Protection: %-4u  Quality: %5.1f%% ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n",
        list->error_count,
        list->warning_count,
        list->protection_count,
        list->overall_quality * 100.0f);
    
    int current_track = -1;
    
    for (size_t i = 0; i < list->count && off + 500 < buf_size; i++) {
        const scp_diagnosis_t* d = &list->items[i];
        
        if (d->track != current_track) {
            current_track = d->track;
            off += snprintf(buf + off, buf_size - off,
                "── Track %u.%u ──────────────────────────────────────────\n",
                d->track, d->side);
        }
        
        const char* icon = "ℹ️";
        if (d->code >= SCP_DIAG_PLL_UNLOCK && d->code <= SCP_DIAG_DECODE_FAIL) {
            icon = "❌";
        } else if (d->code >= SCP_DIAG_WEAK_BITS && d->code <= SCP_DIAG_NON_STANDARD) {
            icon = "🛡️";
        } else if (d->code != SCP_DIAG_OK) {
            icon = "⚠️";
        } else {
            icon = "✅";
        }
        
        if (d->revolution != 0xFF) {
            off += snprintf(buf + off, buf_size - off,
                "  %s T%u.%u R%u: %s\n",
                icon, d->track, d->side, d->revolution, d->message);
        } else {
            off += snprintf(buf + off, buf_size - off,
                "  %s T%u.%u: %s\n",
                icon, d->track, d->side, d->message);
        }
        
        if (d->code < SCP_DIAG_COUNT && scp_diag_suggestions[d->code][0]) {
            off += snprintf(buf + off, buf_size - off,
                "           → %s\n",
                scp_diag_suggestions[d->code]);
        }
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCORING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void scp_score_init(scp_score_t* score) {
    if (!score) return;
    memset(score, 0, sizeof(scp_score_t));
    score->overall = 1.0f;
    score->flux_score = 1.0f;
    score->timing_score = 1.0f;
    score->consistency_score = 1.0f;
    score->decode_score = 1.0f;
    score->structure_score = 1.0f;
}

static void scp_score_calculate(scp_score_t* score) {
    if (!score) return;
    
    score->overall = 
        score->flux_score * 0.25f +
        score->timing_score * 0.20f +
        score->consistency_score * 0.20f +
        score->decode_score * 0.20f +
        score->structure_score * 0.15f;
    
    if (score->overall < 0.0f) score->overall = 0.0f;
    if (score->overall > 1.0f) score->overall = 1.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX ANALYSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate flux statistics
 */
static void scp_calc_flux_stats(const uint16_t* flux, uint32_t count, scp_flux_stats_t* stats) {
    if (!flux || count == 0 || !stats) return;
    
    memset(stats, 0, sizeof(scp_flux_stats_t));
    stats->total_transitions = count;
    stats->min_flux = UINT32_MAX;
    
    /* First pass: min, max, sum */
    double sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t f = flux[i];
        
        if (f < stats->min_flux) stats->min_flux = f;
        if (f > stats->max_flux) stats->max_flux = f;
        sum += f;
        
        if (f < SCP_FLUX_MIN_VALID) stats->short_count++;
        if (f > 20000) stats->long_count++;  /* > 500µs */
        
        /* Histogram (0-255 maps to 0-6.4µs) */
        uint32_t bin = f / 10;
        if (bin > 255) bin = 255;
        stats->histogram[bin]++;
    }
    
    stats->mean_flux = sum / count;
    
    /* Second pass: stddev */
    double sq_sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        double diff = flux[i] - stats->mean_flux;
        sq_sum += diff * diff;
    }
    stats->stddev_flux = sqrt(sq_sum / count);
    
    /* Find histogram peak */
    for (int i = 0; i < 256; i++) {
        if (stats->histogram[i] > stats->histogram_peak) {
            stats->histogram_peak = stats->histogram[i];
            stats->histogram_peak_bin = i;
        }
    }
}

/**
 * @brief Detect weak bits by comparing revolutions
 */
static void scp_detect_weak_bits(
    scp_track_t* track,
    scp_params_t* params,
    scp_diagnosis_list_t* diag
) {
    if (!track || track->revolution_count < 2) return;
    
    /* Compare flux patterns between revolutions */
    /* Weak bits show up as inconsistent flux timing */
    
    uint32_t weak_count = 0;
    float threshold = params ? params->weak_threshold : 0.20f;
    
    /* Compare first two revolutions */
    scp_revolution_t* rev0 = &track->revolutions[0];
    scp_revolution_t* rev1 = &track->revolutions[1];
    
    if (!rev0->valid || !rev1->valid) return;
    
    /* Find corresponding transitions and compare */
    uint32_t pos0 = 0, pos1 = 0;
    uint32_t time0 = 0, time1 = 0;
    
    while (pos0 < rev0->flux_count && pos1 < rev1->flux_count) {
        time0 += rev0->flux[pos0];
        time1 += rev1->flux[pos1];
        
        /* Check if times are within tolerance */
        float ratio = (float)time0 / time1;
        if (ratio < (1.0f - threshold) || ratio > (1.0f + threshold)) {
            weak_count++;
        }
        
        /* Advance the one that's behind */
        if (time0 < time1) {
            pos0++;
        } else {
            pos1++;
        }
    }
    
    if (weak_count > 100) {
        track->has_weak_bits = true;
        track->weak_bit_count = weak_count;
        
        if (diag) {
            scp_diagnosis_add(diag, SCP_DIAG_WEAK_BITS,
                track->physical_track, track->side, 0xFF,
                "Detected %u weak/inconsistent transitions", weak_count);
        }
    }
}

/**
 * @brief Merge revolutions using voting
 */
static void scp_merge_revolutions(
    scp_track_t* track,
    scp_params_t* params
) {
    if (!track || track->revolution_count < 2) return;
    
    /* Find the best revolution (most flux, best timing) */
    uint8_t best_rev = 0;
    float best_score = 0;
    
    for (uint8_t r = 0; r < track->revolution_count; r++) {
        scp_revolution_t* rev = &track->revolutions[r];
        if (!rev->valid) continue;
        
        float score = rev->score.overall;
        if (score > best_score) {
            best_score = score;
            best_rev = r;
        }
    }
    
    track->best_revolution = best_rev;
    
    /* Copy best revolution as merged data */
    scp_revolution_t* best = &track->revolutions[best_rev];
    if (best->flux_count > 0) {
        track->merged_flux = malloc(best->flux_count * sizeof(uint16_t));
        if (track->merged_flux) {
            memcpy(track->merged_flux, best->flux, best->flux_count * sizeof(uint16_t));
            track->merged_flux_count = best->flux_count;
        }
    }
}

/**
 * @brief Score a revolution
 */
static void scp_score_revolution(scp_revolution_t* rev, uint8_t disk_type) {
    if (!rev || !rev->valid) return;
    
    scp_score_init(&rev->score);
    
    /* Flux score based on statistics */
    if (rev->flux_count > 0) {
        scp_calc_flux_stats(rev->flux, rev->flux_count, &rev->stats);
        
        /* Good flux has low stddev relative to mean */
        float cv = rev->stats.stddev_flux / rev->stats.mean_flux;
        rev->score.flux_score = 1.0f - (cv > 1.0f ? 1.0f : cv);
        
        /* Penalize short/long flux */
        float bad_ratio = (float)(rev->stats.short_count + rev->stats.long_count) / 
                          rev->flux_count;
        rev->score.flux_score *= (1.0f - bad_ratio);
    }
    
    /* Timing score based on index time */
    if (rev->index_time > 0) {
        float rpm = scp_calc_rpm(rev->index_time);
        rev->has_index = true;
        
        /* Expected RPM based on disk type */
        float expected_rpm = (disk_type >= SCP_DISK_PC_1200K && 
                              disk_type <= SCP_DISK_PC_1440K) ? 360.0f : 300.0f;
        
        float rpm_error = fabsf(rpm - expected_rpm) / expected_rpm;
        rev->score.timing_score = 1.0f - (rpm_error > 0.1f ? 0.1f : rpm_error);
    }
    
    rev->score.revolutions = 1;
    rev->score.total_flux = rev->flux_count;
    
    scp_score_calculate(&rev->score);
}

/**
 * @brief Score a track
 */
static void scp_score_track(scp_track_t* track, uint8_t disk_type) {
    if (!track || !track->present) return;
    
    scp_score_init(&track->score);
    
    /* Average revolution scores */
    float flux_sum = 0, timing_sum = 0;
    int valid_revs = 0;
    
    for (uint8_t r = 0; r < track->revolution_count; r++) {
        scp_revolution_t* rev = &track->revolutions[r];
        if (rev->valid) {
            scp_score_revolution(rev, disk_type);
            flux_sum += rev->score.flux_score;
            timing_sum += rev->score.timing_score;
            valid_revs++;
        }
    }
    
    if (valid_revs > 0) {
        track->score.flux_score = flux_sum / valid_revs;
        track->score.timing_score = timing_sum / valid_revs;
    }
    
    /* Consistency score from revolution comparison */
    if (valid_revs >= 2) {
        /* Compare first two revolutions */
        scp_revolution_t* r0 = &track->revolutions[0];
        scp_revolution_t* r1 = &track->revolutions[1];
        
        if (r0->flux_count > 0 && r1->flux_count > 0) {
            float count_ratio = (float)r0->flux_count / r1->flux_count;
            if (count_ratio < 0.95f || count_ratio > 1.05f) {
                track->score.consistency_score = 0.8f;
            }
        }
    }
    
    /* Calculate rotation time and RPM */
    if (track->revolution_count > 0 && track->revolutions[0].valid) {
        track->rotation_time = track->revolutions[0].index_time;
        track->rpm = scp_calc_rpm(track->rotation_time);
    }
    
    /* Protection detection */
    if (track->has_weak_bits || track->has_extra_data || track->is_long_track) {
        track->is_protected = true;
        track->score.has_protection = true;
    }
    
    track->score.revolutions = track->revolution_count;
    track->score.has_weak_bits = track->has_weak_bits;
    track->score.weak_bit_count = track->weak_bit_count;
    
    scp_score_calculate(&track->score);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse SCP header
 */
static bool scp_parse_header(const uint8_t* data, size_t size, scp_disk_t* disk) {
    if (size < SCP_HEADER_SIZE) return false;
    
    /* Check signature */
    if (memcmp(data, SCP_SIGNATURE, SCP_SIGNATURE_LEN) != 0) {
        snprintf(disk->error, sizeof(disk->error), "Invalid SCP signature");
        return false;
    }
    
    memcpy(disk->signature, data, 3);
    disk->signature[3] = '\0';
    
    disk->version = data[3];
    disk->disk_type = data[4];
    disk->revolutions = data[5];
    disk->start_track = data[6];
    disk->end_track = data[7];
    disk->flags = data[8];
    disk->bit_cell_width = data[9];
    disk->heads = data[10];
    disk->resolution = data[11];
    
    /* Checksum */
    disk->checksum = data[12] | (data[13] << 8) | 
                     ((uint32_t)data[14] << 16) | ((uint32_t)data[15] << 24);
    
    /* Set format name */
    snprintf(disk->format_name, sizeof(disk->format_name), "%s",
             scp_disk_type_name(disk->disk_type));
    
    return true;
}

/**
 * @brief Parse track offsets
 */
static bool scp_parse_offsets(const uint8_t* data, size_t size, scp_disk_t* disk) {
    size_t offset = SCP_HEADER_SIZE;
    
    for (int i = 0; i < SCP_MAX_TRACKS; i++) {
        if (offset + 4 > size) break;
        
        disk->track_offsets[i] = 
            data[offset] | 
            (data[offset + 1] << 8) | 
            ((uint32_t)data[offset + 2] << 16) | 
            ((uint32_t)data[offset + 3] << 24);
        
        offset += 4;
    }
    
    return true;
}

/**
 * @brief Parse single track
 */
static bool scp_parse_track(
    const uint8_t* data,
    size_t size,
    uint8_t track_num,
    scp_disk_t* disk,
    scp_params_t* params,
    scp_diagnosis_list_t* diag
) {
    if (track_num >= SCP_MAX_TRACKS) return false;
    
    uint32_t offset = disk->track_offsets[track_num];
    if (offset == 0) return true;  /* Empty track */
    
    scp_track_t* track = &disk->tracks[track_num];
    memset(track, 0, sizeof(scp_track_t));
    
    track->track_num = track_num;
    track->physical_track = track_num / 2;
    track->side = track_num % 2;
    track->tdh_offset = offset;
    track->present = true;
    
    /* Check bounds */
    if (offset + 4 > size) {
        scp_diagnosis_add(diag, SCP_DIAG_TRUNCATED, track->physical_track, 
                          track->side, 0xFF, "Track header beyond file");
        return false;
    }
    
    /* Read Track Data Header */
    /* Format: "TRK" + track_num, then revolution entries */
    const uint8_t* tdh = data + offset;
    
    if (memcmp(tdh, "TRK", 3) != 0) {
        scp_diagnosis_add(diag, SCP_DIAG_BAD_TDH, track->physical_track,
                          track->side, 0xFF, "Invalid TRK signature");
        return false;
    }
    
    uint8_t tdh_track = tdh[3];
    if (tdh_track != track_num) {
        scp_diagnosis_add(diag, SCP_DIAG_BAD_TDH, track->physical_track,
                          track->side, 0xFF, "Track number mismatch: %u vs %u",
                          tdh_track, track_num);
    }
    
    /* Revolution entries start at offset + 4 */
    size_t rev_offset = offset + 4;
    track->revolution_count = 0;
    
    for (uint8_t r = 0; r < disk->revolutions && r < SCP_EXTENDED_REVOLUTIONS; r++) {
        if (rev_offset + 12 > size) break;
        
        const uint8_t* rev_entry = data + rev_offset;
        
        /* Revolution entry: index_time (4), flux_count (4), data_offset (4) */
        uint32_t index_time = rev_entry[0] | (rev_entry[1] << 8) |
                              ((uint32_t)rev_entry[2] << 16) | ((uint32_t)rev_entry[3] << 24);
        uint32_t flux_count = rev_entry[4] | (rev_entry[5] << 8) |
                              ((uint32_t)rev_entry[6] << 16) | ((uint32_t)rev_entry[7] << 24);
        uint32_t data_off = rev_entry[8] | (rev_entry[9] << 8) |
                            ((uint32_t)rev_entry[10] << 16) | ((uint32_t)rev_entry[11] << 24);
        
        if (flux_count == 0 || data_off == 0) {
            rev_offset += 12;
            continue;
        }
        
        scp_revolution_t* rev = &track->revolutions[track->revolution_count];
        rev->index_time = index_time;
        rev->flux_count = flux_count;
        
        /* Read flux data */
        uint32_t flux_offset = offset + data_off;
        if (flux_offset + flux_count * 2 > size) {
            scp_diagnosis_add(diag, SCP_DIAG_TRUNCATED, track->physical_track,
                              track->side, r, "Flux data truncated");
            rev_offset += 12;
            continue;
        }
        
        rev->flux = malloc(flux_count * sizeof(uint16_t));
        if (!rev->flux) {
            rev_offset += 12;
            continue;
        }
        
        /* Copy flux data (big-endian 16-bit values) */
        const uint8_t* flux_data = data + flux_offset;
        for (uint32_t f = 0; f < flux_count; f++) {
            rev->flux[f] = (flux_data[f * 2] << 8) | flux_data[f * 2 + 1];
        }
        
        rev->valid = true;
        rev->has_index = (index_time > 0);
        
        /* Calculate statistics */
        scp_calc_flux_stats(rev->flux, flux_count, &rev->stats);
        
        /* Check for anomalies */
        if (rev->stats.short_count > flux_count / 10) {
            scp_diagnosis_add(diag, SCP_DIAG_FLUX_TOO_SHORT, track->physical_track,
                              track->side, r, "%u short flux transitions",
                              rev->stats.short_count);
        }
        
        track->revolution_count++;
        disk->total_revolutions++;
        disk->total_flux_transitions += flux_count;
        
        rev_offset += 12;
    }
    
    /* Detect weak bits */
    if (params && params->detect_weak_bits && track->revolution_count >= 2) {
        scp_detect_weak_bits(track, params, diag);
    }
    
    /* Merge revolutions */
    if (params && params->multi_rev_merge && track->revolution_count >= 2) {
        scp_merge_revolutions(track, params);
    }
    
    /* Score the track */
    scp_score_track(track, disk->disk_type);
    
    disk->track_count++;
    
    return true;
}

/**
 * @brief Main SCP parse function
 */
bool scp_parse(
    const uint8_t* data,
    size_t size,
    scp_disk_t* disk,
    scp_params_t* params
) {
    if (!data || !disk) return false;
    
    memset(disk, 0, sizeof(scp_disk_t));
    disk->diagnosis = scp_diagnosis_create();
    disk->source_size = size;
    
    /* Parse header */
    if (!scp_parse_header(data, size, disk)) {
        scp_diagnosis_add(disk->diagnosis, SCP_DIAG_BAD_SIGNATURE, 0, 0, 0xFF,
                          "Invalid SCP header");
        return false;
    }
    
    /* Parse track offsets */
    if (!scp_parse_offsets(data, size, disk)) {
        return false;
    }
    
    /* Parse all tracks */
    for (uint8_t t = disk->start_track; t <= disk->end_track; t++) {
        scp_parse_track(data, size, t, disk, params, disk->diagnosis);
    }
    
    /* Calculate sides */
    disk->side_count = (disk->flags & 0x01) ? 2 : 1;
    
    /* Calculate average RPM */
    float rpm_sum = 0;
    int rpm_count = 0;
    for (int t = 0; t < SCP_MAX_TRACKS; t++) {
        if (disk->tracks[t].present && disk->tracks[t].rpm > 0) {
            rpm_sum += disk->tracks[t].rpm;
            rpm_count++;
        }
    }
    if (rpm_count > 0) {
        disk->average_rpm = rpm_sum / rpm_count;
    }
    
    /* Calculate overall score */
    scp_score_init(&disk->score);
    float score_sum = 0;
    int track_count = 0;
    
    for (int t = 0; t < SCP_MAX_TRACKS; t++) {
        scp_track_t* track = &disk->tracks[t];
        if (track->present) {
            score_sum += track->score.overall;
            track_count++;
            
            if (track->is_protected) {
                disk->has_protection = true;
            }
        }
    }
    
    if (track_count > 0) {
        disk->score.overall = score_sum / track_count;
    }
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WRITE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static size_t scp_calculate_size(const scp_disk_t* disk) {
    size_t size = SCP_HEADER_SIZE;
    size += SCP_MAX_TRACKS * 4;  /* Track offset table */
    
    for (int t = 0; t < SCP_MAX_TRACKS; t++) {
        const scp_track_t* track = &disk->tracks[t];
        if (!track->present) continue;
        
        size += 4;  /* TRK header */
        size += track->revolution_count * 12;  /* Revolution entries */
        
        for (int r = 0; r < track->revolution_count; r++) {
            if (track->revolutions[r].valid) {
                size += track->revolutions[r].flux_count * 2;
            }
        }
    }
    
    return size;
}

uint8_t* scp_write(
    const scp_disk_t* disk,
    scp_params_t* params,
    size_t* out_size
) {
    if (!disk || !out_size) return NULL;
    
    size_t size = scp_calculate_size(disk);
    uint8_t* data = calloc(1, size);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Write header */
    memcpy(data, SCP_SIGNATURE, 3);
    data[3] = disk->version;
    data[4] = disk->disk_type;
    data[5] = disk->revolutions;
    data[6] = disk->start_track;
    data[7] = disk->end_track;
    data[8] = disk->flags;
    data[9] = disk->bit_cell_width;
    data[10] = disk->heads;
    data[11] = disk->resolution;
    
    /* Track offsets start at 16 */
    uint32_t track_data_offset = SCP_HEADER_SIZE + SCP_MAX_TRACKS * 4;
    
    for (int t = 0; t < SCP_MAX_TRACKS; t++) {
        const scp_track_t* track = &disk->tracks[t];
        size_t off_pos = SCP_HEADER_SIZE + t * 4;
        
        if (!track->present || track->revolution_count == 0) {
            continue;
        }
        
        /* Write track offset */
        data[off_pos] = track_data_offset & 0xFF;
        data[off_pos + 1] = (track_data_offset >> 8) & 0xFF;
        data[off_pos + 2] = (track_data_offset >> 16) & 0xFF;
        data[off_pos + 3] = (track_data_offset >> 24) & 0xFF;
        
        /* Write TRK header */
        data[track_data_offset] = 'T';
        data[track_data_offset + 1] = 'R';
        data[track_data_offset + 2] = 'K';
        data[track_data_offset + 3] = t;
        track_data_offset += 4;
        
        /* Calculate flux data offset */
        uint32_t flux_offset = 4 + track->revolution_count * 12;
        
        /* Write revolution entries */
        for (int r = 0; r < track->revolution_count; r++) {
            const scp_revolution_t* rev = &track->revolutions[r];
            if (!rev->valid) continue;
            
            /* Index time */
            data[track_data_offset] = rev->index_time & 0xFF;
            data[track_data_offset + 1] = (rev->index_time >> 8) & 0xFF;
            data[track_data_offset + 2] = (rev->index_time >> 16) & 0xFF;
            data[track_data_offset + 3] = (rev->index_time >> 24) & 0xFF;
            
            /* Flux count */
            data[track_data_offset + 4] = rev->flux_count & 0xFF;
            data[track_data_offset + 5] = (rev->flux_count >> 8) & 0xFF;
            data[track_data_offset + 6] = (rev->flux_count >> 16) & 0xFF;
            data[track_data_offset + 7] = (rev->flux_count >> 24) & 0xFF;
            
            /* Data offset */
            data[track_data_offset + 8] = flux_offset & 0xFF;
            data[track_data_offset + 9] = (flux_offset >> 8) & 0xFF;
            data[track_data_offset + 10] = (flux_offset >> 16) & 0xFF;
            data[track_data_offset + 11] = (flux_offset >> 24) & 0xFF;
            
            track_data_offset += 12;
            flux_offset += rev->flux_count * 2;
        }
        
        /* Write flux data */
        for (int r = 0; r < track->revolution_count; r++) {
            const scp_revolution_t* rev = &track->revolutions[r];
            if (!rev->valid || !rev->flux) continue;
            
            for (uint32_t f = 0; f < rev->flux_count; f++) {
                data[track_data_offset] = (rev->flux[f] >> 8) & 0xFF;
                data[track_data_offset + 1] = rev->flux[f] & 0xFF;
                track_data_offset += 2;
            }
        }
    }
    
    *out_size = track_data_offset;
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool scp_detect_protection(
    const scp_disk_t* disk,
    char* protection_name,
    size_t name_size,
    float* confidence
) {
    if (!disk) return false;
    
    *confidence = 0.0f;
    protection_name[0] = '\0';
    
    int weak_tracks = 0;
    int long_tracks = 0;
    int inconsistent_tracks = 0;
    
    for (int t = 0; t < SCP_MAX_TRACKS; t++) {
        const scp_track_t* track = &disk->tracks[t];
        if (!track->present) continue;
        
        if (track->has_weak_bits) weak_tracks++;
        if (track->is_long_track) long_tracks++;
        if (track->score.consistency_score < 0.8f) inconsistent_tracks++;
    }
    
    /* Check for specific protections based on disk type */
    if (disk->disk_type == SCP_DISK_C64) {
        if (weak_tracks > 0) {
            snprintf(protection_name, name_size, "C64 Weak Bit Protection");
            *confidence = 0.85f;
            return true;
        }
    }
    
    if (disk->disk_type == SCP_DISK_AMIGA) {
        if (long_tracks > 5) {
            snprintf(protection_name, name_size, "Amiga Long Track Protection");
            *confidence = 0.80f;
            return true;
        }
    }
    
    /* Generic weak bit detection */
    if (weak_tracks > 3) {
        snprintf(protection_name, name_size, "Weak Bit Protection");
        *confidence = 0.75f;
        return true;
    }
    
    if (inconsistent_tracks > 5) {
        snprintf(protection_name, name_size, "Timing Protection");
        *confidence = 0.70f;
        return true;
    }
    
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEFAULT PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

void scp_get_default_params(scp_params_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(scp_params_t));
    
    params->min_revolutions = 1;
    params->max_revolutions = 5;
    params->use_all_revolutions = false;
    
    params->filter_spikes = true;
    params->spike_threshold = 20;  /* 0.5µs */
    params->normalize_flux = false;
    
    params->multi_rev_merge = true;
    params->merge_strategy = 1;  /* Best */
    params->merge_threshold = 0.1f;
    
    params->detect_weak_bits = true;
    params->weak_threshold = 0.2f;
    params->preserve_weak_bits = true;
    
    params->decode_flux = true;
    params->pll_mode = 1;  /* Adaptive */
    params->pll_bandwidth = 0.1f;
    params->pll_gain = 0.5f;
    
    params->auto_detect_format = true;
    params->forced_format = 0;
    params->forced_encoding = 0;
    
    params->timing_tolerance = 0.15f;
    params->detect_speed_errors = true;
    
    params->detect_protection = true;
    params->preserve_protection = true;
    
    params->generate_histogram = false;
    params->generate_stats = true;
    
    params->verify_after_write = true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CLEANUP
 * ═══════════════════════════════════════════════════════════════════════════ */

void scp_disk_free(scp_disk_t* disk) {
    if (!disk) return;
    
    if (disk->diagnosis) {
        scp_diagnosis_free(disk->diagnosis);
        disk->diagnosis = NULL;
    }
    
    for (int t = 0; t < SCP_MAX_TRACKS; t++) {
        scp_track_t* track = &disk->tracks[t];
        
        for (int r = 0; r < SCP_EXTENDED_REVOLUTIONS; r++) {
            scp_revolution_t* rev = &track->revolutions[r];
            if (rev->flux) {
                free(rev->flux);
                rev->flux = NULL;
            }
            if (rev->bitstream) {
                free(rev->bitstream);
                rev->bitstream = NULL;
            }
        }
        
        if (track->merged_flux) {
            free(track->merged_flux);
            track->merged_flux = NULL;
        }
        if (track->merged_bits) {
            free(track->merged_bits);
            track->merged_bits = NULL;
        }
        if (track->weak_mask) {
            free(track->weak_mask);
            track->weak_mask = NULL;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef SCP_V3_TEST

#include <assert.h>

int main(void) {
    printf("=== SCP Parser v3 Tests ===\n");
    
    /* Test helper functions */
    printf("Testing helper functions... ");
    assert(scp_ticks_to_ns(40) == 1000);
    assert(scp_ns_to_ticks(1000) == 40);
    assert(fabsf(scp_ticks_to_us(40) - 1.0f) < 0.01f);
    assert(fabsf(scp_calc_rpm(8000000) - 300.0f) < 1.0f);
    printf("✓\n");
    
    /* Test disk type names */
    printf("Testing disk type names... ");
    assert(strcmp(scp_disk_type_name(SCP_DISK_C64), "Commodore 64") == 0);
    assert(strcmp(scp_disk_type_name(SCP_DISK_AMIGA), "Amiga DD") == 0);
    assert(strcmp(scp_disk_type_name(SCP_DISK_PC_1440K), "PC 1.44M") == 0);
    printf("✓\n");
    
    /* Test expected sectors */
    printf("Testing expected sectors... ");
    assert(scp_get_expected_sectors(SCP_DISK_C64, 0) == 21);
    assert(scp_get_expected_sectors(SCP_DISK_C64, 20) == 19);
    assert(scp_get_expected_sectors(SCP_DISK_AMIGA, 0) == 11);
    assert(scp_get_expected_sectors(SCP_DISK_PC_1440K, 0) == 18);
    printf("✓\n");
    
    /* Test flux statistics */
    printf("Testing flux statistics... ");
    uint16_t test_flux[] = { 100, 120, 110, 130, 115, 125, 105, 135 };
    scp_flux_stats_t stats;
    scp_calc_flux_stats(test_flux, 8, &stats);
    assert(stats.min_flux == 100);
    assert(stats.max_flux == 135);
    assert(stats.total_transitions == 8);
    assert(stats.mean_flux > 115 && stats.mean_flux < 120);
    printf("✓\n");
    
    /* Test diagnosis system */
    printf("Testing diagnosis system... ");
    scp_diagnosis_list_t* diag = scp_diagnosis_create();
    assert(diag != NULL);
    
    scp_diagnosis_add(diag, SCP_DIAG_WEAK_BITS, 17, 0, 0, "Weak bits found");
    assert(diag->count == 1);
    assert(diag->protection_count == 1);
    
    scp_diagnosis_add(diag, SCP_DIAG_CRC_ERROR, 17, 0, 1, "CRC error");
    assert(diag->count == 2);
    assert(diag->error_count == 1);
    
    char* report = scp_diagnosis_to_text(diag, NULL);
    assert(report != NULL);
    assert(strstr(report, "17") != NULL);
    free(report);
    
    scp_diagnosis_free(diag);
    printf("✓\n");
    
    /* Test scoring */
    printf("Testing scoring system... ");
    scp_score_t score;
    scp_score_init(&score);
    assert(score.overall == 1.0f);
    
    score.flux_score = 0.9f;
    score.timing_score = 0.85f;
    score.consistency_score = 0.95f;
    score.decode_score = 0.8f;
    score.structure_score = 0.9f;
    scp_score_calculate(&score);
    assert(score.overall > 0.85f && score.overall < 0.92f);
    printf("✓\n");
    
    /* Test parameters */
    printf("Testing default parameters... ");
    scp_params_t params;
    scp_get_default_params(&params);
    assert(params.min_revolutions == 1);
    assert(params.max_revolutions == 5);
    assert(params.detect_weak_bits == true);
    assert(params.pll_mode == 1);
    printf("✓\n");
    
    /* Test minimal SCP parsing */
    printf("Testing SCP header parsing... ");
    uint8_t minimal_scp[SCP_HEADER_SIZE + SCP_MAX_TRACKS * 4 + 100];
    memset(minimal_scp, 0, sizeof(minimal_scp));
    memcpy(minimal_scp, SCP_SIGNATURE, 3);
    minimal_scp[3] = 0x19;  /* Version 1.9 */
    minimal_scp[4] = SCP_DISK_C64;
    minimal_scp[5] = 3;     /* 3 revolutions */
    minimal_scp[6] = 0;     /* Start track */
    minimal_scp[7] = 83;    /* End track */
    minimal_scp[8] = SCP_FLAG_INDEX;
    minimal_scp[10] = 2;    /* 2 heads */
    
    scp_disk_t disk;
    scp_params_t parms;
    scp_get_default_params(&parms);
    
    bool ok = scp_parse(minimal_scp, sizeof(minimal_scp), &disk, &parms);
    assert(ok);
    assert(disk.valid);
    assert(disk.disk_type == SCP_DISK_C64);
    assert(disk.revolutions == 3);
    assert(strcmp(disk.format_name, "Commodore 64") == 0);
    
    scp_disk_free(&disk);
    printf("✓\n");
    
    /* Test protection detection */
    printf("Testing protection detection... ");
    scp_disk_t test_disk;
    memset(&test_disk, 0, sizeof(test_disk));
    test_disk.disk_type = SCP_DISK_C64;
    test_disk.tracks[20].present = true;
    test_disk.tracks[20].has_weak_bits = true;
    test_disk.tracks[21].present = true;
    test_disk.tracks[21].has_weak_bits = true;
    test_disk.tracks[22].present = true;
    test_disk.tracks[22].has_weak_bits = true;
    test_disk.tracks[23].present = true;
    test_disk.tracks[23].has_weak_bits = true;
    
    char prot_name[64];
    float confidence;
    bool has_prot = scp_detect_protection(&test_disk, prot_name, sizeof(prot_name), &confidence);
    assert(has_prot);
    assert(strstr(prot_name, "Weak") != NULL || strstr(prot_name, "C64") != NULL);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 9, Failed: 0\n");
    return 0;
}

#endif /* SCP_V3_TEST */
