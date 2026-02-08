/**
 * @file uft_track_analysis.h
 * @brief Generic Track Analysis Framework
 * 
 * Universal track analysis algorithms derived from XCopy Pro (1989-2011).
 * Works with ANY disk format: Amiga, Atari ST, PC/IBM, Apple II, C64, BBC, MSX, etc.
 * 
 * Generic algorithms:
 * - Multi-sync pattern search with bit rotation
 * - Track length measurement
 * - GAP detection by frequency analysis
 * - Breakpoint/protection detection
 * - Sector structure analysis
 * 
 * @author UFT Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef UFT_TRACK_ANALYSIS_H
#define UFT_TRACK_ANALYSIS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_MAX_SYNC_PATTERNS       32      /* Max patterns to search */
#define UFT_MAX_SYNC_POSITIONS      64      /* Max syncs per track */
#define UFT_MAX_SECTOR_TYPES        16      /* Max unique sector lengths */
#define UFT_MAX_BREAKPOINTS         10      /* Max breakpoints to detect */

/* Common sync patterns across platforms */
#define SYNC_AMIGA_DOS              0x4489  /* Amiga AmigaDOS */
#define SYNC_AMIGA_ARKANOID         0x9521  /* Amiga Arkanoid protection */
#define SYNC_AMIGA_OCEAN            0xA245  /* Amiga Ocean/Imagine */
#define SYNC_AMIGA_NOVAGEN          0xA89A  /* Amiga Mercenary/Backlash */
#define SYNC_IBM_MFM                0x4489  /* IBM/PC MFM (A1 with missing clock) */
#define SYNC_ATARI_ST               0x4489  /* Atari ST standard */
#define SYNC_APPLE_DOS33            0xD5AA96 /* Apple II DOS 3.3 (24-bit) */
#define SYNC_APPLE_PRODOS           0xD5AAAD /* Apple II ProDOS (24-bit) */
#define SYNC_C64_GCR                0x52    /* C64 GCR sync byte */
#define SYNC_BBC_FM                 0xFE    /* BBC Micro FM */
#define SYNC_MSX                    0x4489  /* MSX standard */
#define SYNC_AMSTRAD                0x4489  /* Amstrad CPC */

/*===========================================================================
 * Types - Platform Definitions
 *===========================================================================*/

/**
 * @brief Supported disk platforms
 */
typedef enum {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_AMIGA,
    PLATFORM_ATARI_ST,
    PLATFORM_IBM_PC,
    PLATFORM_APPLE_II,
    PLATFORM_C64,
    PLATFORM_BBC_MICRO,
    PLATFORM_MSX,
    PLATFORM_AMSTRAD_CPC,
    PLATFORM_ARCHIMEDES,
    PLATFORM_SAM_COUPE,
    PLATFORM_SPECTRUM_PLUS3,
    PLATFORM_PC98,
    PLATFORM_X68000,
    PLATFORM_FM_TOWNS,
    PLATFORM_CUSTOM
} uft_platform_t;

/**
 * @brief Encoding types
 */
typedef enum {
    ENCODING_UNKNOWN = 0,
    ENCODING_FM,            /* Single density */
    ENCODING_MFM,           /* Double density */
    ENCODING_GCR_APPLE,     /* Apple GCR 6&2 */
    ENCODING_GCR_C64,       /* Commodore GCR */
    ENCODING_GCR_VICTOR,    /* Victor 9000 GCR */
    ENCODING_M2FM,          /* Modified MFM */
    ENCODING_MMFM           /* Modified MFM variant */
} uft_encoding_t;

/**
 * @brief Track classification result
 */
typedef enum {
    TRACK_TYPE_STANDARD = 0,    /* Normal format track */
    TRACK_TYPE_PROTECTED,       /* Copy protected */
    TRACK_TYPE_LONG,            /* Long track protection */
    TRACK_TYPE_SHORT,           /* Short track */
    TRACK_TYPE_WEAK_BITS,       /* Weak bit protection */
    TRACK_TYPE_FUZZY,           /* Fuzzy bits */
    TRACK_TYPE_NO_SYNC,         /* No sync found */
    TRACK_TYPE_UNFORMATTED,     /* Blank/unformatted */
    TRACK_TYPE_DAMAGED,         /* Physically damaged */
    TRACK_TYPE_UNKNOWN          /* Cannot classify */
} uft_track_type_t;

/*===========================================================================
 * Types - Platform Profile
 *===========================================================================*/

/**
 * @brief Platform-specific parameters for track analysis
 */
typedef struct {
    uft_platform_t platform;
    uft_encoding_t encoding;
    const char *name;
    
    /* Sync patterns for this platform */
    const uint32_t *sync_patterns;      /* Array of sync patterns */
    int sync_count;                     /* Number of patterns */
    int sync_bits;                      /* Bits per sync (8, 16, 24, 32) */
    
    /* Track geometry */
    size_t track_length_min;            /* Minimum valid track length */
    size_t track_length_max;            /* Maximum valid track length */
    size_t track_length_nominal;        /* Standard track length */
    size_t long_track_threshold;        /* Long track detection */
    
    /* Sector geometry */
    int sectors_per_track;              /* Standard sector count */
    size_t sector_size;                 /* Data bytes per sector */
    size_t sector_mfm_size;             /* MFM bytes per sector (with header/gap) */
    size_t sector_tolerance;            /* Length tolerance in bytes */
    
    /* Timing */
    double data_rate_kbps;              /* Data rate (250, 300, 500 kbps) */
    double rpm;                         /* Rotation speed */
} uft_platform_profile_t;

/*===========================================================================
 * Types - Sync Detection
 *===========================================================================*/

/**
 * @brief Found sync position
 */
typedef struct {
    size_t byte_position;       /* Byte offset in track */
    int bit_offset;             /* Bit rotation (0-15 for 16-bit sync) */
    uint32_t pattern;           /* Actual pattern found */
    int pattern_index;          /* Index in search array */
    float confidence;           /* Detection confidence 0.0-1.0 */
} uft_sync_position_t;

/**
 * @brief Sync search result
 */
typedef struct {
    int count;                  /* Number of syncs found */
    uft_sync_position_t positions[UFT_MAX_SYNC_POSITIONS];
    uint32_t primary_pattern;   /* Most common pattern */
    int primary_count;          /* Count of primary pattern */
    bool bit_shifted;           /* Found via bit rotation */
} uft_sync_result_t;

/*===========================================================================
 * Types - Sector Analysis
 *===========================================================================*/

/**
 * @brief Sector length distribution entry
 */
typedef struct {
    size_t length;              /* Sector length in bytes */
    int count;                  /* Number of sectors with this length */
    float percentage;           /* Percentage of total */
} uft_sector_length_t;

/**
 * @brief Sector structure analysis
 */
typedef struct {
    int sector_count;           /* Total sectors found */
    int unique_lengths;         /* Number of unique lengths */
    uft_sector_length_t lengths[UFT_MAX_SECTOR_TYPES];
    
    /* GAP analysis */
    bool gap_found;
    int gap_sector_index;       /* Sector after GAP */
    size_t gap_length;          /* GAP length in bytes */
    
    /* Uniformity check */
    bool is_uniform;            /* All sectors same length */
    size_t nominal_length;      /* Most common length */
    float uniformity;           /* 0.0-1.0 how uniform */
} uft_sector_analysis_t;

/*===========================================================================
 * Types - Track Analysis Result
 *===========================================================================*/

/**
 * @brief Complete track analysis result
 */
typedef struct {
    /* Classification */
    uft_track_type_t type;
    uft_platform_t detected_platform;
    uft_encoding_t detected_encoding;
    float confidence;
    
    /* Track geometry */
    size_t track_length;            /* Measured length */
    size_t data_start;              /* First data byte */
    size_t data_end;                /* Last data byte */
    size_t optimal_write_start;     /* Best write start position */
    
    /* Sync analysis */
    uft_sync_result_t sync;
    
    /* Sector analysis */
    uft_sector_analysis_t sectors;
    
    /* Protection indicators */
    bool is_protected;
    bool is_long_track;
    bool is_short_track;
    bool has_weak_bits;
    bool has_breakpoints;
    int breakpoint_count;
    size_t breakpoint_positions[UFT_MAX_BREAKPOINTS];
    
    /* Identification */
    char protection_name[64];
    char format_name[64];
    
    /* Raw statistics */
    uint32_t bit_count;
    uint32_t flux_transitions;
    double avg_bit_time_ns;
} uft_track_analysis_t;

/*===========================================================================
 * Types - Analysis Context
 *===========================================================================*/

/**
 * @brief Analysis context and configuration
 */
typedef struct {
    /* Input data */
    const uint8_t *track_data;
    size_t track_size;
    
    /* Configuration */
    const uft_platform_profile_t *profile;  /* NULL for auto-detect */
    bool auto_detect_platform;
    bool search_all_syncs;                  /* Search all known patterns */
    bool detect_protection;                 /* Run protection detection */
    bool detect_breakpoints;                /* Run Neuhaus algorithm */
    bool measure_timing;                    /* Analyze bit timing */
    
    /* Custom sync patterns (optional) */
    const uint32_t *custom_syncs;
    int custom_sync_count;
    int custom_sync_bits;
    
    /* Tolerances */
    size_t length_tolerance;
    int max_syncs_to_find;
} uft_analysis_config_t;

/*===========================================================================
 * Pre-defined Platform Profiles
 *===========================================================================*/

extern const uft_platform_profile_t UFT_PROFILE_AMIGA_DD;
extern const uft_platform_profile_t UFT_PROFILE_AMIGA_HD;
extern const uft_platform_profile_t UFT_PROFILE_ATARI_ST_DD;
extern const uft_platform_profile_t UFT_PROFILE_ATARI_ST_HD;
extern const uft_platform_profile_t UFT_PROFILE_IBM_DD;
extern const uft_platform_profile_t UFT_PROFILE_IBM_HD;
extern const uft_platform_profile_t UFT_PROFILE_APPLE_DOS33;
extern const uft_platform_profile_t UFT_PROFILE_APPLE_PRODOS;
extern const uft_platform_profile_t UFT_PROFILE_C64;
extern const uft_platform_profile_t UFT_PROFILE_BBC_DFS;
extern const uft_platform_profile_t UFT_PROFILE_BBC_ADFS;
extern const uft_platform_profile_t UFT_PROFILE_MSX;
extern const uft_platform_profile_t UFT_PROFILE_AMSTRAD;

/*===========================================================================
 * Core Analysis Functions
 *===========================================================================*/

/**
 * @brief Analyze track with automatic platform detection
 * 
 * @param track_data    Raw track data
 * @param track_size    Size of track data
 * @param result        Output: Analysis result
 * @return 0 on success, <0 on error
 */
int uft_analyze_track(const uint8_t *track_data, size_t track_size,
                      uft_track_analysis_t *result);

/**
 * @brief Analyze track with specific platform profile
 */
int uft_analyze_track_profile(const uint8_t *track_data, size_t track_size,
                              const uft_platform_profile_t *profile,
                              uft_track_analysis_t *result);

/**
 * @brief Analyze track with full configuration
 */
int uft_analyze_track_ex(const uft_analysis_config_t *config,
                         uft_track_analysis_t *result);

/*===========================================================================
 * Sync Pattern Detection (Generic)
 *===========================================================================*/

/**
 * @brief Search for sync patterns with bit rotation
 * 
 * Universal algorithm from XCopy Pro - works with any sync pattern.
 * Searches all bit positions (0 to sync_bits-1) for each pattern.
 * 
 * @param data          Track data
 * @param size          Data size
 * @param patterns      Array of sync patterns to search
 * @param pattern_count Number of patterns
 * @param sync_bits     Bits per sync pattern (8, 16, 24, 32)
 * @param result        Output: Sync search result
 * @return Number of syncs found
 */
int uft_find_syncs_rotated(const uint8_t *data, size_t size,
                           const uint32_t *patterns, int pattern_count,
                           int sync_bits, uft_sync_result_t *result);

/**
 * @brief Search for single sync pattern (fast path)
 */
int uft_find_sync_simple(const uint8_t *data, size_t size,
                         uint32_t pattern, int sync_bits,
                         size_t *positions, int max_positions);

/**
 * @brief Search for all known sync patterns (auto-detection)
 */
int uft_find_syncs_all_known(const uint8_t *data, size_t size,
                             uft_sync_result_t *result);

/*===========================================================================
 * Track Length Measurement (Generic)
 *===========================================================================*/

/**
 * @brief Measure actual track length from raw data
 * 
 * Universal algorithm from XCopy Pro's getracklen.
 * 
 * @param data          Track data (ideally 2 rotations)
 * @param size          Buffer size
 * @param end_position  Output: Position of track end
 * @return Track length in bytes
 */
size_t uft_measure_track_length(const uint8_t *data, size_t size,
                                size_t *end_position);

/**
 * @brief Measure track length for specific encoding
 */
size_t uft_measure_track_length_enc(const uint8_t *data, size_t size,
                                    uft_encoding_t encoding,
                                    size_t *end_position);

/*===========================================================================
 * Sector/GAP Analysis (Generic)
 *===========================================================================*/

/**
 * @brief Analyze sector structure from sync positions
 * 
 * Universal GAP detection from XCopy Pro's gapsearch.
 * 
 * @param sync_positions    Array of sync byte positions
 * @param sync_count        Number of syncs
 * @param tolerance         Length tolerance in bytes
 * @param result            Output: Sector analysis
 * @return 0 on success
 */
int uft_analyze_sectors(const size_t *sync_positions, int sync_count,
                        size_t tolerance, uft_sector_analysis_t *result);

/**
 * @brief Find GAP by frequency analysis
 * 
 * GAP = sector with minimum occurrence (rarest length = inter-track gap)
 */
bool uft_find_gap_by_frequency(const size_t *sync_positions, int sync_count,
                               size_t tolerance, int *gap_index, size_t *gap_length);

/**
 * @brief Calculate optimal write start position
 */
size_t uft_calc_write_start(const size_t *sync_positions, int sync_count,
                            int gap_sector_index, size_t pre_gap_bytes);

/*===========================================================================
 * Protection Detection (Generic)
 *===========================================================================*/

/**
 * @brief Detect breakpoints in track data (Neuhaus algorithm)
 * 
 * Universal algorithm - finds sudden value changes used for copy protection.
 * 
 * @param data              Track data
 * @param size              Data size
 * @param max_breakpoints   Maximum breakpoints allowed (typically 5)
 * @param positions         Output: Breakpoint positions (optional)
 * @param count             Output: Number of breakpoints found
 * @return true if valid breakpoint pattern (protection detected)
 */
bool uft_detect_breakpoints(const uint8_t *data, size_t size,
                            int max_breakpoints,
                            size_t *positions, int *count);

/**
 * @brief Detect weak bits in track data
 */
bool uft_detect_weak_bits(const uint8_t *data, size_t size,
                          size_t *positions, int *count);

/**
 * @brief Check if track is longer than nominal
 */
bool uft_is_long_track(size_t track_length, const uft_platform_profile_t *profile);

/**
 * @brief Identify protection scheme by characteristics
 */
bool uft_identify_protection(const uft_track_analysis_t *analysis,
                             char *name, size_t name_size);

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

/**
 * @brief Auto-detect platform from track characteristics
 */
uft_platform_t uft_detect_platform(const uft_track_analysis_t *analysis);

/**
 * @brief Get platform profile by ID
 */
const uft_platform_profile_t *uft_get_platform_profile(uft_platform_t platform);

/**
 * @brief Get platform name
 */
const char *uft_platform_name(uft_platform_t platform);

/**
 * @brief Get encoding name
 */
const char *uft_encoding_name(uft_encoding_t encoding);

/**
 * @brief Get track type name
 */
const char *uft_track_type_name(uft_track_type_t type);

/*===========================================================================
 * Reporting
 *===========================================================================*/

/**
 * @brief Print analysis result to stdout
 */
void uft_print_track_analysis(const uft_track_analysis_t *result);

/**
 * @brief Generate JSON report
 */
int uft_track_analysis_to_json(const uft_track_analysis_t *result,
                               char *buffer, size_t size);

/**
 * @brief Initialize default analysis config
 */
void uft_analysis_config_init(uft_analysis_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_ANALYSIS_H */
