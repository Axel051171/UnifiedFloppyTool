/**
 * @file uft_metrics.h
 * @brief Disk Analysis Metrics Types and Functions
 * 
 * EXT4-006: Quality metrics for disk analysis
 */

#ifndef UFT_METRICS_H
#define UFT_METRICS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_METRICS_HD  1   /* High density (1000ns cell time) */
#define UFT_METRICS_DD  2   /* Double density (2000ns cell time) */

/*===========================================================================
 * Quality Grade Enumeration
 *===========================================================================*/

typedef enum {
    UFT_GRADE_UNKNOWN = 0,
    UFT_GRADE_EXCELLENT,
    UFT_GRADE_GOOD,
    UFT_GRADE_FAIR,
    UFT_GRADE_POOR,
    UFT_GRADE_BAD
} uft_quality_grade_t;

/*===========================================================================
 * Confidence Level Enumeration
 *===========================================================================*/

typedef enum {
    UFT_CONF_UNKNOWN = 0,
    UFT_CONF_LOW,
    UFT_CONF_MEDIUM,
    UFT_CONF_HIGH
} uft_confidence_level_t;

/*===========================================================================
 * Protection Type Enumeration
 *===========================================================================*/

typedef enum {
    UFT_PROT_NONE = 0,
    /* Amiga protections */
    UFT_PROT_COPYLOCK,
    UFT_PROT_SPEEDLOCK,
    UFT_PROT_LONGTRACK,
    /* C64 protections */
    UFT_PROT_VMAX,
    UFT_PROT_RAPIDLOK,
    UFT_PROT_VORPAL,
    /* Other */
    UFT_PROT_CUSTOM
} uft_protection_type_t;

/*===========================================================================
 * Flux Quality Metrics
 *===========================================================================*/

typedef struct {
    uint32_t min_interval;      /**< Minimum flux interval */
    uint32_t max_interval;      /**< Maximum flux interval */
    uint32_t avg_interval;      /**< Average flux interval */
    double   std_deviation;     /**< Standard deviation of intervals */
    float    jitter_percent;    /**< Jitter as percentage of cell time */
    int      valid_percent;     /**< Percentage of valid cell timings */
    int      outlier_percent;   /**< Percentage of outlier timings */
    int      overall_score;     /**< Overall quality score 0-100 */
    uft_quality_grade_t grade;  /**< Quality grade */
} uft_flux_quality_t;

/*===========================================================================
 * Sector Read Result
 *===========================================================================*/

typedef struct {
    uint8_t  track;             /**< Track number */
    uint8_t  sector;            /**< Sector number */
    bool     crc_valid;         /**< CRC check passed */
    bool     deleted;           /**< Deleted data mark */
    bool     weak_bits;         /**< Contains weak bits */
    int      retry_count;       /**< Number of retries needed */
    uint8_t *data;              /**< Sector data */
    size_t   data_size;         /**< Data size */
} uft_sector_read_t;

/*===========================================================================
 * Sector Quality Metrics
 *===========================================================================*/

typedef struct {
    size_t  total_sectors;      /**< Total sector count */
    size_t  good_sectors;       /**< Sectors with valid CRC */
    size_t  bad_sectors;        /**< Sectors with CRC errors */
    size_t  deleted_sectors;    /**< Sectors with deleted marks */
    size_t  weak_sectors;       /**< Sectors with weak bits */
    size_t  total_retries;      /**< Total retry count */
    int     good_percent;       /**< Percentage of good sectors */
    int     bad_percent;        /**< Percentage of bad sectors */
    int     overall_score;      /**< Overall quality score 0-100 */
    uft_quality_grade_t grade;  /**< Quality grade */
} uft_sector_quality_t;

/*===========================================================================
 * Track Read Result
 *===========================================================================*/

typedef struct {
    uint8_t  track;             /**< Track number */
    uint8_t  side;              /**< Side (0 or 1) */
    int      encoding;          /**< Encoding type (HD/DD) */
    size_t   sector_count;      /**< Number of sectors */
    uft_sector_read_t *sectors; /**< Sector array */
    uint32_t *flux_times;       /**< Raw flux transition times */
    size_t   flux_count;        /**< Number of flux transitions */
} uft_track_read_t;

/*===========================================================================
 * Track Quality Metrics
 *===========================================================================*/

typedef struct {
    uint8_t track;              /**< Track number */
    uint8_t side;               /**< Side (0 or 1) */
    size_t  sector_count;       /**< Number of sectors */
    int     sector_score;       /**< Sector quality score */
    int     flux_score;         /**< Flux quality score */
    int     overall_score;      /**< Overall quality score 0-100 */
    uft_quality_grade_t grade;  /**< Quality grade */
} uft_track_quality_t;

/*===========================================================================
 * Revolution Comparison Result
 *===========================================================================*/

typedef struct {
    size_t  rev1_flux;          /**< Revolution 1 flux count */
    size_t  rev2_flux;          /**< Revolution 2 flux count */
    int     matching_cells;     /**< Number of matching cells */
    int     different_cells;    /**< Number of different cells */
    uint64_t avg_difference;    /**< Average timing difference */
    int     similarity_percent; /**< Percentage of matching intervals */
    int     weak_bit_count;     /**< Number of potential weak bits */
} uft_rev_compare_t;

/*===========================================================================
 * Protection Detection Result
 *===========================================================================*/

typedef struct {
    uft_protection_type_t type; /**< Protection type detected */
    float   confidence;         /**< Detection confidence 0.0-1.0 */
    uint8_t track;              /**< Track where detected */
    uint8_t side;               /**< Side where detected */
    char    name[64];           /**< Protection scheme name */
} uft_protection_detect_t;

/*===========================================================================
 * Protection Confidence Summary
 *===========================================================================*/

typedef struct {
    size_t  detection_count;    /**< Total detections */
    uft_protection_type_t primary_type; /**< Primary protection type */
    float   max_confidence;     /**< Maximum confidence value */
    int     amiga_count;        /**< Amiga protection count */
    int     c64_count;          /**< C64 protection count */
    int     other_count;        /**< Other protection count */
    uft_confidence_level_t overall; /**< Overall confidence level */
} uft_protection_conf_t;

/*===========================================================================
 * Disk Summary Metrics
 *===========================================================================*/

typedef struct {
    size_t  total_tracks;       /**< Total track count */
    size_t  good_tracks;        /**< Good quality tracks */
    size_t  fair_tracks;        /**< Fair quality tracks */
    size_t  bad_tracks;         /**< Bad quality tracks */
    int     avg_sector_score;   /**< Average sector quality */
    int     avg_flux_score;     /**< Average flux quality */
    int     overall_score;      /**< Overall disk score */
    uft_quality_grade_t grade;  /**< Overall quality grade */
} uft_disk_summary_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Calculate flux quality metrics
 */
int uft_metrics_flux_quality(const uint32_t *flux_times, size_t count,
                             uint32_t expected_cell_ns,
                             uft_flux_quality_t *quality);

/**
 * @brief Calculate sector quality metrics
 */
int uft_metrics_sector_quality(const uft_sector_read_t *reads, size_t count,
                               uft_sector_quality_t *quality);

/**
 * @brief Calculate track quality metrics
 */
int uft_metrics_track_quality(const uft_track_read_t *track,
                              uft_track_quality_t *quality);

/**
 * @brief Compare two revolutions for weak bits
 */
int uft_metrics_revolution_compare(const uint32_t *rev1, size_t count1,
                                   const uint32_t *rev2, size_t count2,
                                   uft_rev_compare_t *result);

/**
 * @brief Calculate protection detection confidence
 */
int uft_metrics_protection_confidence(const uft_protection_detect_t *detections,
                                      size_t count,
                                      uft_protection_conf_t *confidence);

/**
 * @brief Calculate disk summary metrics
 */
int uft_metrics_disk_summary(const uft_track_quality_t *tracks, size_t count,
                             uft_disk_summary_t *summary);

/**
 * @brief Get grade name string
 */
const char *uft_metrics_grade_name(uft_quality_grade_t grade);

/**
 * @brief Generate JSON report
 */
int uft_metrics_report_json(const uft_disk_summary_t *summary,
                            char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_METRICS_H */
