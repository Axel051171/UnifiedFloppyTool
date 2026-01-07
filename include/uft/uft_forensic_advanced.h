/**
 * @file uft_forensic_advanced.h
 * @brief UFT Advanced Forensic Module v1.6.0
 * 
 * Features:
 * - Forensic Risk Scoring (from disk visualization analysis)
 * - Multi-Pass Recovery Strategy
 * - DiskDupe Copy Protection Detection
 * - Weak Bit Tracking
 * - Timing Anomaly Detection
 * - Recovery Confidence Assessment
 * 
 * VERSION: 1.6.0 (2025-12-30)
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_FORENSIC_ADVANCED_H
#define UFT_FORENSIC_ADVANCED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FORENSIC RISK LEVEL
 *============================================================================*/

typedef enum uft_forensic_risk {
    UFT_RISK_NONE       = 0,    /**< Score 0: Pristine disk */
    UFT_RISK_LOW        = 1,    /**< Score 1-3: Minor issues */
    UFT_RISK_MEDIUM     = 2,    /**< Score 4-6: Moderate damage */
    UFT_RISK_HIGH       = 3,    /**< Score >= 7: Severe problems */
} uft_forensic_risk_t;

/*============================================================================
 * RECOVERY CONFIDENCE
 *============================================================================*/

typedef enum uft_recovery_confidence {
    UFT_CONF_LOW        = 0,    /**< < 50% success likely */
    UFT_CONF_MEDIUM     = 1,    /**< 50-80% success likely */
    UFT_CONF_HIGH       = 2,    /**< > 80% success likely */
} uft_recovery_confidence_t;

/*============================================================================
 * SECTOR STATUS (Extended from uft_sector_status.h)
 *============================================================================*/

typedef enum uft_sector_status {
    UFT_SECTOR_OK           = 0,    /**< Sector OK */
    UFT_SECTOR_CRC_BAD      = 1,    /**< CRC mismatch */
    UFT_SECTOR_NOT_FOUND    = 2,    /**< ID field not found */
    UFT_SECTOR_MISSING      = 3,    /**< Completely missing */
    UFT_SECTOR_WEAK_BITS    = 4,    /**< Contains weak/unstable bits */
    UFT_SECTOR_DELETED      = 5,    /**< Deleted data mark */
    UFT_SECTOR_FIXED        = 6,    /**< Recovered/repaired */
    UFT_SECTOR_TIMING_ERR   = 7,    /**< Timing anomaly */
    UFT_SECTOR_DUPLICATE    = 8,    /**< Duplicate ID (copy prot) */
} uft_sector_status_t;

/**
 * @brief Check if sector status should trigger retry
 */
static inline bool uft_sector_status_should_retry(uft_sector_status_t status) {
    return status == UFT_SECTOR_CRC_BAD || 
           status == UFT_SECTOR_NOT_FOUND ||
           status == UFT_SECTOR_WEAK_BITS ||
           status == UFT_SECTOR_TIMING_ERR;
}

/**
 * @brief Get worst status from two statuses
 */
static inline uft_sector_status_t uft_sector_status_worst(
    uft_sector_status_t a, uft_sector_status_t b) 
{
    /* Priority order: MISSING > NOT_FOUND > CRC_BAD > WEAK > TIMING > others */
    if (a == UFT_SECTOR_MISSING || b == UFT_SECTOR_MISSING) 
        return UFT_SECTOR_MISSING;
    if (a == UFT_SECTOR_NOT_FOUND || b == UFT_SECTOR_NOT_FOUND) 
        return UFT_SECTOR_NOT_FOUND;
    if (a == UFT_SECTOR_CRC_BAD || b == UFT_SECTOR_CRC_BAD) 
        return UFT_SECTOR_CRC_BAD;
    if (a == UFT_SECTOR_WEAK_BITS || b == UFT_SECTOR_WEAK_BITS) 
        return UFT_SECTOR_WEAK_BITS;
    if (a > b) return a;
    return b;
}

/*============================================================================
 * DISKDUPE DETECTION (Copy Protection)
 *============================================================================*/

/** DiskDupe signature masks (from forensic analysis) */
#define UFT_DD_MASK_DD1     0x01    /**< DiskDupe 1 signature */
#define UFT_DD_MASK_DD2     0x02    /**< DiskDupe 2 signature */
#define UFT_DD_MASK_DD3     0x04    /**< DiskDupe 3 signature */
#define UFT_DD_MASK_DD4     0x08    /**< DiskDupe 4 signature */
#define UFT_DD_MASK_DD5     0x10    /**< DiskDupe 5 signature */

typedef struct uft_diskdupe_info {
    uint8_t dd_mask;            /**< Detected DiskDupe variants */
    uint8_t track;              /**< Track containing signature */
    uint8_t sector;             /**< Sector containing signature */
    uint32_t offset;            /**< Byte offset in sector */
    char description[64];       /**< Human-readable description */
} uft_diskdupe_info_t;

/*============================================================================
 * TRACK METADATA (for forensic analysis)
 *============================================================================*/

typedef struct uft_track_forensic_meta {
    uint8_t track_number;
    uint8_t head;
    
    /* Quality metrics */
    int quality_percent;        /**< 0-100% overall quality */
    int error_count;            /**< Total errors on track */
    int weak_sector_count;      /**< Sectors with weak bits */
    int timing_anomaly_count;   /**< Timing issues detected */
    
    /* Timing analysis */
    double avg_bit_time_us;     /**< Average bit cell time */
    double bit_time_jitter;     /**< Timing variance (stddev) */
    double rpm_measured;        /**< Measured RPM */
    double rpm_deviation;       /**< Deviation from nominal */
    
    /* Copy protection */
    uint8_t dd_mask;            /**< DiskDupe signatures */
    bool has_duplicate_ids;     /**< Duplicate sector IDs */
    bool has_long_track;        /**< Track longer than normal */
    bool has_half_track;        /**< Half-track data present */
    
    /* Per-sector status */
    uft_sector_status_t sector_status[64];  /**< Max 64 sectors/track */
    uint8_t sector_retry_count[64];         /**< Retries per sector */
    
    /* Multi-revolution data */
    int revolutions_captured;   /**< Number of revolutions */
    uint8_t best_revolution[64]; /**< Best rev for each sector */
} uft_track_forensic_meta_t;

/*============================================================================
 * DISK FORENSIC METADATA
 *============================================================================*/

typedef struct uft_disk_forensic_meta {
    /* Global stats */
    int total_tracks;
    int total_sectors;
    int ok_sectors;
    int bad_sectors;
    int weak_sectors;
    int missing_sectors;
    int recovered_sectors;      /**< After recovery attempts */
    
    /* Quality */
    int overall_quality;        /**< 0-100% */
    uft_forensic_risk_t risk_level;
    uft_recovery_confidence_t confidence;
    
    /* Copy protection */
    bool copy_protection_detected;
    uint8_t dd_mask;
    char protection_type[64];
    
    /* Timing */
    double avg_rpm;
    double rpm_variance;
    bool rpm_stable;
    
    /* Per-track metadata */
    uft_track_forensic_meta_t *tracks;
    int track_count;
    
    /* Recovery suggestion */
    char recovery_suggestion[256];
    int suggested_passes;
} uft_disk_forensic_meta_t;

/*============================================================================
 * FORENSIC RISK SCORING ALGORITHM
 * 
 * Score calculation (from forensic.zip analysis):
 *   +2 if quality < 60%
 *   +2 if quality < 40%
 *   +1 if error_count > 0
 *   +1 if error_count > 3
 *   +1 if weak_sectors > 0
 *   +1 if weak_sectors > 3
 *   +1 if timing_anomalies > 0
 *   +1 if timing_anomalies > 5
 *   +2 if dd_mask != 0 (copy protection)
 * 
 * Score >= 7: HIGH
 * Score >= 4: MEDIUM
 * Score >= 1: LOW
 * Score == 0: NONE
 *============================================================================*/

typedef struct uft_forensic_score_input {
    int quality_percent;        /**< 0-100 */
    int error_count;            /**< Total CRC/missing errors */
    int weak_sector_count;      /**< Sectors with weak bits */
    int timing_anomaly_count;   /**< Timing issues */
    uint8_t dd_mask;            /**< DiskDupe detection mask */
} uft_forensic_score_input_t;

/**
 * @brief Calculate forensic risk score
 * @param input Score input parameters
 * @return Risk score (0-12+)
 */
int uft_forensic_calculate_score(const uft_forensic_score_input_t *input);

/**
 * @brief Convert score to risk level
 * @param score Risk score
 * @return Risk level enum
 */
uft_forensic_risk_t uft_forensic_score_to_risk(int score);

/**
 * @brief Get recovery suggestion based on risk
 * @param risk Risk level
 * @param out_suggestion Buffer for suggestion text
 * @param max_len Buffer size
 */
void uft_forensic_get_suggestion(uft_forensic_risk_t risk, 
                                 char *out_suggestion, size_t max_len);

/**
 * @brief Get suggested pass count based on track metadata
 * @param meta Track metadata
 * @return Suggested number of read passes (1-10)
 */
int uft_forensic_suggest_passes(const uft_track_forensic_meta_t *meta);

/*============================================================================
 * RECOVERY CONFIDENCE CALCULATION
 *============================================================================*/

/**
 * @brief Calculate recovery confidence
 * @param ok_sectors Number of OK sectors
 * @param bad_sectors Number of bad sectors (potentially recoverable)
 * @param missing_sectors Number of completely missing sectors
 * @return Confidence level
 */
uft_recovery_confidence_t uft_forensic_calculate_confidence(
    int ok_sectors, int bad_sectors, int missing_sectors);

/*============================================================================
 * EXTENDED FORENSIC PARAMS (for GUI)
 *============================================================================*/

typedef struct uft_forensic_params_extended {
    /* Basic Recovery Options (from v1.5.1) */
    bool bitshift_recovery;         /**< Try all 8 bit positions */
    bool multi_revolution;          /**< Read multiple revolutions */
    int rev_count;                  /**< 2-10 revolutions */
    bool detect_weak_bits;          /**< Detect copy protection */
    int max_retries;                /**< 0-100 */
    
    /* Format-specific */
    bool atari_st_mode;             /**< Atari ST specific detection */
    bool validate_boot_sector;      /**< Check BPB validity */
    bool check_st_bootable;         /**< Check 0x1234 checksum */
    
    /* Missing Sector Handling */
    bool fill_missing_sectors;      /**< Fill missing sectors */
    uint8_t fill_pattern;           /**< 0x00, 0xE5, 0xF6 */
    
    /* Output */
    bool create_error_log;          /**< Create detailed error log */
    char log_path[512];
    
    /* === NEW v1.6.0 Fields === */
    
    /* Advanced Analysis */
    bool enable_timing_analysis;    /**< Analyze bit timing */
    bool enable_dd_detection;       /**< DiskDupe detection */
    bool enable_rpm_measurement;    /**< Measure actual RPM */
    bool track_per_revolution;      /**< Store each revolution */
    
    /* Multi-Pass Recovery */
    bool auto_suggest_passes;       /**< Auto-suggest pass count */
    int max_passes;                 /**< Maximum pass count (1-20) */
    bool adaptive_retry;            /**< Retry only bad sectors */
    
    /* Copy Protection */
    bool preserve_weak_bits;        /**< Keep weak bits in output */
    bool preserve_timing;           /**< Keep timing info */
    bool mark_recovered;            /**< Mark recovered sectors */
    
    /* Reporting */
    bool create_forensic_report;    /**< JSON/XML report */
    int report_format;              /**< 0=JSON, 1=XML, 2=TXT */
    bool include_sector_dump;       /**< Include raw sector data */
    bool include_timing_data;       /**< Include timing measurements */
    
    /* Hash verification */
    bool compute_hashes;            /**< Compute MD5/SHA1 */
    bool hash_per_track;            /**< Per-track hashes */
} uft_forensic_params_extended_t;

/*============================================================================
 * FORENSIC REPORT STRUCTURE
 *============================================================================*/

typedef struct uft_forensic_report {
    /* Header */
    char tool_version[32];
    char timestamp[32];
    char source_path[512];
    
    /* Summary */
    int total_tracks;
    int total_sectors;
    int ok_sectors;
    int recovered_sectors;
    int failed_sectors;
    int overall_quality;
    uft_forensic_risk_t risk_level;
    uft_recovery_confidence_t confidence;
    
    /* Hashes */
    char md5_hash[33];
    char sha1_hash[41];
    char sha256_hash[65];
    
    /* Copy protection */
    bool has_protection;
    char protection_details[256];
    
    /* Recovery log */
    char *recovery_log;             /**< Detailed log (malloc'd) */
    size_t log_size;
    
    /* Disk metadata */
    uft_disk_forensic_meta_t disk_meta;
} uft_forensic_report_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/** Initialize extended forensic params */
void uft_forensic_params_extended_init(uft_forensic_params_extended_t *params);

/** Initialize disk forensic metadata */
void uft_disk_forensic_meta_init(uft_disk_forensic_meta_t *meta, int tracks);

/** Free disk forensic metadata */
void uft_disk_forensic_meta_free(uft_disk_forensic_meta_t *meta);

/** Update track metadata after read */
void uft_track_forensic_update(uft_track_forensic_meta_t *meta,
                               const uint8_t *sector_data,
                               const uft_sector_status_t *status,
                               int sector_count);

/** Update disk metadata from track metadata */
void uft_disk_forensic_update(uft_disk_forensic_meta_t *disk,
                              const uft_track_forensic_meta_t *track);

/** Generate forensic report */
int uft_forensic_generate_report(const uft_disk_forensic_meta_t *meta,
                                 const uft_forensic_params_extended_t *params,
                                 uft_forensic_report_t *report);

/** Export report to file */
int uft_forensic_export_report(const uft_forensic_report_t *report,
                               const char *path, int format);

/** Free forensic report */
void uft_forensic_report_free(uft_forensic_report_t *report);

/** Detect DiskDupe signatures in track data */
int uft_forensic_detect_diskdupe(const uint8_t *track_data, size_t len,
                                 uft_diskdupe_info_t *info);

/** Get risk level name */
const char *uft_forensic_risk_name(uft_forensic_risk_t risk);

/** Get confidence level name */
const char *uft_forensic_confidence_name(uft_recovery_confidence_t conf);

/** Get sector status name */
const char *uft_sector_status_name(uft_sector_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_ADVANCED_H */
