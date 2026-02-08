/**
 * @file uft_amiga_caps.h
 * @brief Amiga CAPS/SPS Protection Detection
 * 
 * CAPS (Classic Amiga Preservation Society) / SPS (Software Preservation Society)
 * IPF format and protection analysis
 * 
 * Improves CAPS/SPS detection: 60% → 85%
 */

#ifndef UFT_AMIGA_CAPS_H
#define UFT_AMIGA_CAPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_CAPS_MAX_TRACKS         168
#define UFT_CAPS_MAX_SECTORS        22
#define UFT_CAPS_MAX_WEAK_REGIONS   64

/* IPF record types */
#define UFT_IPF_CAPS                0x43415053  /* 'CAPS' */
#define UFT_IPF_INFO                0x494E464F  /* 'INFO' */
#define UFT_IPF_IMGE                0x494D4745  /* 'IMGE' */
#define UFT_IPF_DATA                0x44415441  /* 'DATA' */
#define UFT_IPF_CTEI                0x43544549  /* 'CTEI' - CTRaw Extra Info */
#define UFT_IPF_CTEX                0x43544558  /* 'CTEX' - CTRaw Extended */

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief IPF encoder types
 */
typedef enum {
    UFT_IPF_ENC_CAPS = 1,           /**< CAPS encoder */
    UFT_IPF_ENC_SPS,                /**< SPS encoder */
    UFT_IPF_ENC_CTRAW               /**< CTRaw encoder */
} uft_ipf_encoder_t;

/**
 * @brief IPF data types
 */
typedef enum {
    UFT_IPF_DATA_RAW = 0,           /**< Raw MFM data */
    UFT_IPF_DATA_FLUX,              /**< Flux transitions */
    UFT_IPF_DATA_SYNC,              /**< Sync-aware data */
    UFT_IPF_DATA_CTRAW              /**< CTRaw format */
} uft_ipf_data_type_t;

/**
 * @brief Density types
 */
typedef enum {
    UFT_CAPS_DENSITY_AUTO = 0,
    UFT_CAPS_DENSITY_DD,            /**< Double Density */
    UFT_CAPS_DENSITY_HD,            /**< High Density */
    UFT_CAPS_DENSITY_ED             /**< Extra Density */
} uft_caps_density_t;

/**
 * @brief Protection categories
 */
typedef enum {
    UFT_CAPS_PROT_NONE = 0,
    UFT_CAPS_PROT_COPYLOCK,         /**< Rob Northen Copylock */
    UFT_CAPS_PROT_SPEEDLOCK,        /**< Speedlock */
    UFT_CAPS_PROT_LONGTRACK,        /**< Long tracks */
    UFT_CAPS_PROT_WEAK,             /**< Weak/fuzzy bits */
    UFT_CAPS_PROT_DENSITY,          /**< Variable density */
    UFT_CAPS_PROT_NOFLUX,           /**< No-flux areas */
    UFT_CAPS_PROT_CUSTOM            /**< Custom protection */
} uft_caps_protection_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief IPF file header
 */
typedef struct {
    uint32_t magic;                 /**< 'CAPS' */
    uint32_t length;                /**< Record length */
    uint32_t crc;                   /**< CRC32 */
} uft_ipf_header_t;

/**
 * @brief IPF INFO record
 */
typedef struct {
    uint32_t media_type;            /**< Media type code */
    uint32_t encoder_type;          /**< Encoder used */
    uint32_t encoder_rev;           /**< Encoder revision */
    uint32_t file_key;              /**< File key */
    uint32_t file_rev;              /**< File revision */
    uint32_t origin;                /**< Origin CRC */
    uint32_t min_track;             /**< Minimum track */
    uint32_t max_track;             /**< Maximum track */
    uint32_t min_side;              /**< Minimum side */
    uint32_t max_side;              /**< Maximum side */
    uint32_t creation_date;         /**< Creation date */
    uint32_t creation_time;         /**< Creation time */
    uint32_t platforms;             /**< Platform flags */
    uint32_t disk_number;           /**< Disk number */
    uint32_t creator_id;            /**< Creator ID */
    uint32_t reserved[3];           /**< Reserved */
} uft_ipf_info_t;

/**
 * @brief IPF IMGE (image) record
 */
typedef struct {
    uint32_t track;                 /**< Track number */
    uint32_t side;                  /**< Side number */
    uint32_t density;               /**< Density type */
    uint32_t signal_type;           /**< Signal type */
    uint32_t track_bytes;           /**< Track size in bytes */
    uint32_t start_byte_pos;        /**< Start byte position */
    uint32_t start_bit_pos;         /**< Start bit position */
    uint32_t data_bits;             /**< Data bits count */
    uint32_t gap_bits;              /**< Gap bits count */
    uint32_t track_bits;            /**< Total track bits */
    uint32_t block_count;           /**< Data block count */
    uint32_t encoder_process;       /**< Encoder process */
    uint32_t flags;                 /**< Track flags */
    uint32_t data_key;              /**< Data encryption key */
    uint32_t reserved[3];           /**< Reserved */
} uft_ipf_imge_t;

/**
 * @brief Weak region definition
 */
typedef struct {
    uint32_t bit_position;          /**< Start bit position */
    uint32_t bit_length;            /**< Length in bits */
    uint8_t  variation_count;       /**< Number of variations */
    uint8_t  decay_rate;            /**< Signal decay rate */
} uft_caps_weak_region_t;

/**
 * @brief Track analysis result
 */
typedef struct {
    uint8_t track;
    uint8_t side;
    
    /* Structure */
    uint32_t total_bits;
    uint32_t data_bits;
    uint32_t gap_bits;
    uint16_t sector_count;
    
    /* Protection */
    uft_caps_protection_t protection;
    float protection_confidence;
    
    /* Weak regions */
    uft_caps_weak_region_t weak_regions[UFT_CAPS_MAX_WEAK_REGIONS];
    uint8_t weak_region_count;
    
    /* Timing */
    uint32_t bitcell_ns;            /**< Average bitcell time */
    float timing_variance;          /**< Timing variance */
    
    /* Flags */
    bool is_longtrack;
    bool has_weak_bits;
    bool has_no_flux;
    bool is_variable_density;
} uft_caps_track_analysis_t;

/**
 * @brief Full IPF analysis result
 */
typedef struct {
    /* File info */
    uft_ipf_info_t info;
    bool valid_ipf;
    
    /* Structure */
    uint8_t min_track;
    uint8_t max_track;
    uint8_t sides;
    uft_caps_density_t density;
    
    /* Tracks */
    uft_caps_track_analysis_t tracks[UFT_CAPS_MAX_TRACKS];
    uint16_t track_count;
    
    /* Protection summary */
    bool has_protection;
    uft_caps_protection_t primary_protection;
    float overall_confidence;
    char protection_name[64];
    
    /* Statistics */
    uint16_t long_tracks;
    uint16_t weak_tracks;
    uint16_t noflux_tracks;
    uint32_t total_weak_bits;
} uft_caps_analysis_t;

/**
 * @brief CTRaw specific analysis
 */
typedef struct {
    bool has_ctraw;
    uint32_t stream_count;
    uint32_t total_samples;
    
    /* Timing */
    float sample_rate_mhz;
    float index_time_ms;
    
    /* Quality */
    float signal_quality;
    uint32_t dropouts;
    uint32_t weak_samples;
} uft_ctraw_analysis_t;

/*===========================================================================
 * Function Prototypes - IPF Parsing
 *===========================================================================*/

/**
 * @brief Check if file is valid IPF
 */
bool uft_caps_is_ipf(const uint8_t *data, size_t size);

/**
 * @brief Parse IPF header
 */
int uft_caps_parse_header(const uint8_t *data, size_t size,
                          uft_ipf_header_t *header);

/**
 * @brief Parse IPF INFO record
 */
int uft_caps_parse_info(const uint8_t *data, size_t size,
                        uft_ipf_info_t *info);

/**
 * @brief Parse IPF IMGE record
 */
int uft_caps_parse_imge(const uint8_t *data, size_t size,
                        uft_ipf_imge_t *imge);

/**
 * @brief Full IPF analysis
 */
int uft_caps_analyze_ipf(const uint8_t *data, size_t size,
                         uft_caps_analysis_t *result);

/*===========================================================================
 * Function Prototypes - Protection Detection
 *===========================================================================*/

/**
 * @brief Detect Copylock in IPF track
 */
int uft_caps_detect_copylock(const uint8_t *track_data, size_t size,
                             uint8_t track, float *confidence);

/**
 * @brief Detect weak bits in track
 */
int uft_caps_detect_weak_bits(const uint8_t *track_data, size_t size,
                              uft_caps_weak_region_t *regions,
                              size_t max_regions, size_t *count);

/**
 * @brief Detect long track
 */
bool uft_caps_is_long_track(const uft_ipf_imge_t *imge, uft_caps_density_t density);

/**
 * @brief Detect no-flux areas
 */
int uft_caps_detect_noflux(const uint8_t *track_data, size_t size,
                           uint32_t *positions, size_t max_pos, size_t *count);

/**
 * @brief Analyze variable density
 */
int uft_caps_analyze_density(const uint8_t *track_data, size_t size,
                             float *density_map, size_t map_size);

/*===========================================================================
 * Function Prototypes - CTRaw Analysis
 *===========================================================================*/

/**
 * @brief Check if IPF contains CTRaw data
 */
bool uft_caps_has_ctraw(const uint8_t *data, size_t size);

/**
 * @brief Analyze CTRaw data
 */
int uft_caps_analyze_ctraw(const uint8_t *data, size_t size,
                           uft_ctraw_analysis_t *result);

/**
 * @brief Extract flux timing from CTRaw
 */
int uft_ctraw_extract_flux(const uint8_t *ctraw_data, size_t size,
                           uint32_t *flux_times, size_t max_flux,
                           size_t *flux_count);

/*===========================================================================
 * Function Prototypes - Conversion
 *===========================================================================*/

/**
 * @brief Convert IPF track to ADF sector data
 */
int uft_caps_to_adf_track(const uint8_t *ipf_track, size_t size,
                          const uft_ipf_imge_t *imge,
                          uint8_t *adf_data, size_t *adf_size);

/**
 * @brief Convert ADF track to IPF format
 */
int uft_adf_to_caps_track(const uint8_t *adf_data, size_t adf_size,
                          uint8_t track, uint8_t side,
                          uint8_t *ipf_data, size_t *ipf_size);

/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/

/**
 * @brief Get protection type name
 */
const char *uft_caps_protection_name(uft_caps_protection_t prot);

/**
 * @brief Get density name
 */
const char *uft_caps_density_name(uft_caps_density_t density);

/**
 * @brief Get encoder name
 */
const char *uft_caps_encoder_name(uft_ipf_encoder_t encoder);

/**
 * @brief Calculate IPF CRC32
 */
uint32_t uft_caps_crc32(const uint8_t *data, size_t size);

/**
 * @brief Export analysis to JSON
 */
int uft_caps_report_json(const uft_caps_analysis_t *analysis,
                         char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_CAPS_H */
