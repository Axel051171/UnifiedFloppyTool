/**
 * @file uft_amiga_protection.h
 * @brief Amiga Copy Protection Analysis - Ported from XCopy Pro
 * 
 * Algorithms ported from XCopy Pro (1989-2011) 68000 Assembly to C99:
 * - Multi-Sync pattern detection with bit rotation
 * - Track structure analysis (GAP detection)
 * - Copy protection identification
 * - Bruchstellen (break point) detection (Neuhaus algorithm)
 * - Track length measurement
 * - Long track detection
 * 
 * @author UFT Team (C99 Port)
 * @author Frank (Original XCopy Pro 1989)
 * @version 1.0.0
 * @date 2025
 */

#ifndef UFT_AMIGA_PROTECTION_H
#define UFT_AMIGA_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants - From XCopy Pro
 *===========================================================================*/

/* Standard Amiga sync pattern */
#define AMIGA_SYNC_STANDARD     0x4489

/* Known copy protection sync patterns (from XCopy Pro synctab) */
#define AMIGA_SYNC_ARKANOID     0x9521  /* Arkanoid */
#define AMIGA_SYNC_BTIP         0xA245  /* Beyond the Ice Palace */
#define AMIGA_SYNC_MERCENARY    0xA89A  /* Mercenary, Backlash */
#define AMIGA_SYNC_ALT1         0x448A  /* Alternative sync */
#define AMIGA_SYNC_INDEX        0xF8BC  /* Index copy marker */

/* Track length constants (from XCopy Pro) */
#define AMIGA_TRACKLEN_DEFAULT  0x30C0  /* 12480 bytes - Standard DD */
#define AMIGA_TRACKLEN_MAX      0x3400  /* 13312 bytes - Max read length */
#define AMIGA_TRACKLEN_LONG     0x3300  /* Long track threshold */

/* Sector sizes */
#define AMIGA_SECTOR_MFM_SIZE   0x0440  /* 1088 bytes MFM per sector */
#define AMIGA_SECTORS_DD        11
#define AMIGA_SECTORS_HD        22

/* Analysis limits */
#define MAX_SYNC_POSITIONS      24      /* Max sectors to track */
#define MAX_BREAKPOINTS         5       /* Max breakpoints for Neuhaus */
#define SECTOR_LEN_TOLERANCE    0x20    /* ±32 bytes tolerance */

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Known sync pattern types
 */
typedef enum {
    SYNC_TYPE_UNKNOWN = 0,
    SYNC_TYPE_AMIGA_DOS,        /* Standard $4489 */
    SYNC_TYPE_ARKANOID,         /* $9521 */
    SYNC_TYPE_BTIP,             /* $A245 - Beyond the Ice Palace */
    SYNC_TYPE_MERCENARY,        /* $A89A */
    SYNC_TYPE_INDEX_COPY,       /* $F8BC */
    SYNC_TYPE_CUSTOM            /* Other non-standard */
} uft_amiga_sync_type_t;

/**
 * @brief Track analysis result classification
 */
typedef enum {
    TRACK_CLASS_DOS = 0,        /* Standard AmigaDOS track (GREEN in XCopy) */
    TRACK_CLASS_NIBBLE,         /* Non-DOS, needs nibble copy (BLUE) */
    TRACK_CLASS_LONG,           /* Long track protection (BLACK) */
    TRACK_CLASS_BREAKPOINT,     /* Breakpoint protection (GREY) */
    TRACK_CLASS_NO_SYNC,        /* No sync found (RED) */
    TRACK_CLASS_ERROR           /* Analysis error */
} uft_amiga_track_class_t;

/**
 * @brief Detected sync position
 */
typedef struct {
    size_t position;            /* Byte offset in track */
    size_t bit_offset;          /* Bit rotation offset (0-15) */
    uint16_t pattern;           /* Actual sync pattern found */
    uft_amiga_sync_type_t type; /* Classification */
} uft_amiga_sync_pos_t;

/**
 * @brief Sector length entry for GAP analysis
 */
typedef struct {
    uint16_t length;            /* Sector length in bytes */
    uint16_t count;             /* How many sectors have this length */
} uft_amiga_sector_len_t;

/**
 * @brief Track analysis result
 */
typedef struct {
    /* Classification */
    uft_amiga_track_class_t classification;
    
    /* Sync analysis */
    uint16_t sync_pattern;                          /* Primary sync pattern */
    uft_amiga_sync_type_t sync_type;
    int sync_count;                                 /* Number of syncs found */
    uft_amiga_sync_pos_t sync_positions[MAX_SYNC_POSITIONS];
    
    /* Track geometry */
    size_t track_length;                            /* Measured track length */
    size_t read_length;                             /* Bytes actually read */
    size_t write_start_offset;                      /* Optimal write start */
    int sector_count;                               /* Number of sectors */
    
    /* GAP analysis */
    int gap_sector_index;                           /* Sector after GAP */
    size_t gap_length;                              /* GAP length in bytes */
    
    /* Sector lengths */
    int unique_lengths;                             /* Number of unique lengths */
    uft_amiga_sector_len_t sector_lengths[MAX_SYNC_POSITIONS];
    
    /* Protection indicators */
    bool is_long_track;                             /* Track > LONG threshold */
    bool has_breakpoints;                           /* Neuhaus breakpoints */
    int breakpoint_count;                           /* Number of breakpoints */
    bool is_pseudo_dos;                             /* Looks like DOS but isn't */
    
    /* Confidence */
    float confidence;                               /* 0.0 - 1.0 */
    
    /* Debug info */
    char protection_name[32];                       /* e.g., "Arkanoid", "Rob Northen" */
} uft_amiga_track_analysis_t;

/**
 * @brief Context for track analysis
 */
typedef struct {
    const uint8_t *track_data;
    size_t track_size;
    
    /* Configuration */
    bool detect_custom_sync;                        /* Search for non-$4489 sync */
    bool detect_breakpoints;                        /* Run Neuhaus algorithm */
    uint16_t force_sync;                            /* Force specific sync, 0=auto */
    
    /* Working buffers (internal) */
    uint16_t sector_lengths[MAX_SYNC_POSITIONS];
    size_t sync_pos_buffer[MAX_SYNC_POSITIONS + 1];
} uft_amiga_analysis_ctx_t;

/*===========================================================================
 * Core Analysis Functions
 *===========================================================================*/

/**
 * @brief Analyze an Amiga track for copy protection
 * 
 * Port of XCopy Pro's Analyse routine. Performs:
 * 1. Track length measurement
 * 2. Multi-sync pattern search with bit rotation
 * 3. Sector length analysis
 * 4. GAP detection
 * 5. Protection classification
 * 
 * @param track_data    Raw MFM track data (2 rotations recommended)
 * @param track_size    Size of track data in bytes
 * @param result        Output: Analysis results
 * @return 0 on success, <0 on error
 */
int uft_amiga_analyze_track(const uint8_t *track_data, size_t track_size,
                            uft_amiga_track_analysis_t *result);

/**
 * @brief Extended analysis with context
 */
int uft_amiga_analyze_track_ex(uft_amiga_analysis_ctx_t *ctx,
                               uft_amiga_track_analysis_t *result);

/*===========================================================================
 * Sync Pattern Detection
 *===========================================================================*/

/**
 * @brief Search for sync patterns with bit rotation
 * 
 * Port of XCopy Pro's sync search with ROL.L instruction.
 * Searches all 16 bit positions for each sync pattern.
 * 
 * @param data          Track data
 * @param size          Data size
 * @param patterns      Array of sync patterns to search
 * @param pattern_count Number of patterns
 * @param positions     Output: Found positions
 * @param max_positions Maximum positions to return
 * @return Number of syncs found
 */
int uft_amiga_find_syncs_rotated(const uint8_t *data, size_t size,
                                 const uint16_t *patterns, int pattern_count,
                                 uft_amiga_sync_pos_t *positions, int max_positions);

/**
 * @brief Find standard Amiga sync ($4489) only
 */
int uft_amiga_find_sync_standard(const uint8_t *data, size_t size,
                                 size_t *positions, int max_positions);

/**
 * @brief Identify sync pattern type
 */
uft_amiga_sync_type_t uft_amiga_identify_sync(uint16_t pattern);

/**
 * @brief Get human-readable sync name
 */
const char *uft_amiga_sync_name(uft_amiga_sync_type_t type);

/*===========================================================================
 * Track Length Measurement
 *===========================================================================*/

/**
 * @brief Measure actual track length
 * 
 * Port of XCopy Pro's getracklen routine.
 * Finds the end of valid data in a 2-rotation read.
 * 
 * @param data      Track data (should be 2 rotations)
 * @param size      Buffer size
 * @param end_pos   Output: Position of track end
 * @return Track length in bytes, or 0 on error
 */
size_t uft_amiga_measure_track_length(const uint8_t *data, size_t size,
                                      size_t *end_pos);

/**
 * @brief Check if track is "long track" protection
 */
bool uft_amiga_is_long_track(size_t track_length);

/*===========================================================================
 * Breakpoint Detection (Neuhaus Algorithm)
 *===========================================================================*/

/**
 * @brief Detect breakpoints in track data
 * 
 * Port of XCopy Pro's Neuhaus routine.
 * Detects "Bruchstellen" - sudden value changes used for copy protection.
 * 
 * @param data              Track data
 * @param size              Data size
 * @param breakpoint_count  Output: Number of breakpoints found
 * @param positions         Output: Breakpoint positions (optional)
 * @param max_positions     Maximum positions to return
 * @return true if valid breakpoint pattern detected (≤5 breakpoints)
 */
bool uft_amiga_detect_breakpoints(const uint8_t *data, size_t size,
                                  int *breakpoint_count,
                                  size_t *positions, int max_positions);

/*===========================================================================
 * GAP Analysis
 *===========================================================================*/

/**
 * @brief Analyze sector lengths and find GAP
 * 
 * Port of XCopy Pro's gapsearch routine.
 * GAP is identified as the sector with minimum occurrence count.
 * 
 * @param sync_positions    Array of sync positions
 * @param sync_count        Number of syncs
 * @param gap_index         Output: Index of GAP sector
 * @param gap_length        Output: GAP length
 * @return true if GAP found
 */
bool uft_amiga_find_gap(const size_t *sync_positions, int sync_count,
                        int *gap_index, size_t *gap_length);

/**
 * @brief Calculate optimal write start position
 * 
 * @param track_data        Track data
 * @param track_length      Measured track length
 * @param gap_sector_index  GAP sector index from find_gap
 * @param sync_positions    Sync positions
 * @return Optimal write start offset
 */
size_t uft_amiga_calc_write_start(const uint8_t *track_data, size_t track_length,
                                  int gap_sector_index, const size_t *sync_positions);

/*===========================================================================
 * DOS Track Validation
 *===========================================================================*/

/**
 * @brief Check if track is standard AmigaDOS format
 * 
 * Port of XCopy Pro's dostest.
 * Validates: sync=$4489, 11 sectors, each $440 bytes.
 * 
 * @param result    Track analysis result
 * @return true if valid DOS track
 */
bool uft_amiga_is_dos_track(const uft_amiga_track_analysis_t *result);

/**
 * @brief Check if track is "pseudo-DOS" (looks like DOS but has protection)
 */
bool uft_amiga_is_pseudo_dos(const uft_amiga_track_analysis_t *result);

/*===========================================================================
 * Protection Identification
 *===========================================================================*/

/**
 * @brief Identify copy protection scheme
 * 
 * Based on sync patterns, track structure, and other indicators.
 * 
 * @param result    Track analysis result
 * @param name      Output: Protection name (if known)
 * @param name_size Size of name buffer
 * @return true if protection identified
 */
bool uft_amiga_identify_protection(const uft_amiga_track_analysis_t *result,
                                   char *name, size_t name_size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get classification name
 */
const char *uft_amiga_class_name(uft_amiga_track_class_t cls);

/**
 * @brief Print analysis report
 */
void uft_amiga_print_analysis(const uft_amiga_track_analysis_t *result);

/**
 * @brief Generate JSON report
 */
int uft_amiga_analysis_to_json(const uft_amiga_track_analysis_t *result,
                               char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_PROTECTION_H */
