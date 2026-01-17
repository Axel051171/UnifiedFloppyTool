/**
 * @file uft_track_align.h
 * @brief C64/1541 Track Alignment Module for Copy Protection Handling
 * 
 * This module provides track alignment functions essential for mastering
 * protected disks. Based on nibtools by Pete Rittwage and Markus Brenner.
 * 
 * Supported protections:
 * - V-MAX! (multiple variants including Cinemaware)
 * - Pirate Slayer (v1, v2)
 * - RapidLok (all versions)
 * - Fat tracks
 * - Bitshifted tracks (Kryoflux/SCP repair)
 * 
 * Reference: nibtools (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_TRACK_ALIGN_H
#define UFT_TRACK_ALIGN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum track length for 1541 (8KB) */
#define ALIGN_TRACK_LENGTH      0x2000

/** Maximum halftracks for 1541 */
#define ALIGN_MAX_HALFTRACKS    84

/** Maximum tracks for 1541 */
#define ALIGN_MAX_TRACKS        42

/** Minimum sync length in bytes */
#define ALIGN_MIN_SYNC_LENGTH   5

/** Maximum sync offset before timeout */
#define ALIGN_MAX_SYNC_OFFSET   0x1500

/** Minimum good GCR run for formatted track */
#define ALIGN_MIN_FORMATTED_GCR 16

/* ============================================================================
 * Alignment Types
 * ============================================================================ */

/**
 * @brief Track alignment method
 */
typedef enum {
    ALIGN_NONE          = 0x00,     /**< No alignment */
    ALIGN_GAP           = 0x01,     /**< Align to inter-sector gap */
    ALIGN_SECTOR0       = 0x02,     /**< Align to sector 0 header */
    ALIGN_LONGSYNC      = 0x03,     /**< Align to longest sync mark */
    ALIGN_BADGCR        = 0x04,     /**< Align to bad GCR run (mastering artifact) */
    ALIGN_VMAX          = 0x05,     /**< V-MAX! protection alignment */
    ALIGN_AUTOGAP       = 0x06,     /**< Auto-detect gap and align */
    ALIGN_VMAX_CW       = 0x07,     /**< V-MAX! Cinemaware variant */
    ALIGN_RAW           = 0x08,     /**< Raw alignment (no processing) */
    ALIGN_PIRATESLAYER  = 0x09,     /**< Pirate Slayer protection */
    ALIGN_RAPIDLOK      = 0x0A,     /**< RapidLok protection */
    ALIGN_SYNC          = 0x0B,     /**< Sync-align bitshifted tracks */
} align_method_t;

/**
 * @brief Track density zones
 */
typedef enum {
    DENSITY_0 = 0,      /**< Tracks 31-42: 17 sectors, 6250 bytes/track */
    DENSITY_1 = 1,      /**< Tracks 25-30: 18 sectors, 6666 bytes/track */
    DENSITY_2 = 2,      /**< Tracks 18-24: 19 sectors, 7142 bytes/track */
    DENSITY_3 = 3,      /**< Tracks 1-17:  21 sectors, 7692 bytes/track */
} track_density_t;

/**
 * @brief Track flags (from nibtools)
 */
typedef enum {
    TRACK_FLAG_MATCH    = 0x10,     /**< Track matched (legacy) */
    TRACK_FLAG_NO_CYCLE = 0x20,     /**< No track cycle found */
    TRACK_FLAG_NO_SYNC  = 0x40,     /**< No sync found on track */
    TRACK_FLAG_KILLER   = 0x80,     /**< Killer track (all 0xFF) */
} track_flags_t;

/* ============================================================================
 * V-MAX! Marker Bytes
 * ============================================================================ */

/** Standard V-MAX! duplicator marker bytes */
#define VMAX_MARKER_4B      0x4B
#define VMAX_MARKER_69      0x69
#define VMAX_MARKER_49      0x49
#define VMAX_MARKER_5A      0x5A
#define VMAX_MARKER_A5      0xA5

/** Cinemaware V-MAX! signature: 0x64 0xA5 0xA5 0xA5 */
#define VMAX_CW_MARKER      0x64

/* ============================================================================
 * Pirate Slayer Signatures
 * ============================================================================ */

/** Pirate Slayer v1/v2: D7 D7 EB CC AD */
#define PSLAYER_SIG_0       0xD7
#define PSLAYER_SIG_1       0xD7
#define PSLAYER_SIG_2       0xEB
#define PSLAYER_SIG_3       0xCC
#define PSLAYER_SIG_4       0xAD

/** Pirate Slayer v1 secondary: EB D7 AA 55 */
#define PSLAYER_V1_SEC_0    0xEB
#define PSLAYER_V1_SEC_1    0xD7
#define PSLAYER_V1_SEC_2    0xAA
#define PSLAYER_V1_SEC_3    0x55

/* ============================================================================
 * RapidLok Constants
 * ============================================================================ */

/** RapidLok extra sector byte */
#define RAPIDLOK_EXTRA_BYTE 0x7B

/** RapidLok alternate extra byte */
#define RAPIDLOK_ALT_BYTE   0x4B

/** RapidLok header marker */
#define RAPIDLOK_HEADER     0x75

/** Minimum RapidLok extra sector length */
#define RAPIDLOK_MIN_EXTRA  60

/** Maximum RapidLok extra sector length */
#define RAPIDLOK_MAX_EXTRA  300

/* ============================================================================
 * Capacity Constants (bytes per minute at 300 RPM)
 * ============================================================================ */

/** Density 0: 1,875,000 bytes/min */
#define CAPACITY_DENSITY_0  1875000

/** Density 1: 2,000,000 bytes/min */
#define CAPACITY_DENSITY_1  2000000

/** Density 2: 2,142,857 bytes/min */
#define CAPACITY_DENSITY_2  2142857

/** Density 3: 2,307,692 bytes/min */
#define CAPACITY_DENSITY_3  2307692

/* ============================================================================
 * Track Alignment Result
 * ============================================================================ */

/**
 * @brief Result of track alignment operation
 */
typedef struct {
    bool            success;            /**< Alignment succeeded */
    align_method_t  method_used;        /**< Alignment method that was used */
    size_t          align_offset;       /**< Offset where alignment marker found */
    size_t          original_length;    /**< Original track length */
    size_t          aligned_length;     /**< Aligned track length */
    uint8_t         density;            /**< Track density */
    uint8_t         flags;              /**< Track flags */
    
    /* Protection-specific info */
    union {
        struct {
            int     marker_run;         /**< Length of marker run */
            uint8_t marker_byte;        /**< Marker byte found */
        } vmax;
        
        struct {
            int     version;            /**< Pirate Slayer version (1 or 2) */
            int     shift_count;        /**< Number of bit shifts needed */
        } pirateslayer;
        
        struct {
            int     version;            /**< RapidLok version (1-7) */
            int     extra_length;       /**< Extra sector length */
            int     sync_length;        /**< Sync length */
            bool    is_pal;             /**< PAL or NTSC */
        } rapidlok;
        
        struct {
            int     sync_length;        /**< Longest sync length */
        } sync;
        
        struct {
            int     gap_length;         /**< Gap length */
            uint8_t gap_byte;           /**< Gap fill byte */
        } gap;
    } info;
    
    char            description[128];   /**< Human-readable description */
} align_result_t;

/* ============================================================================
 * Track Buffer Structure
 * ============================================================================ */

/**
 * @brief Track data with metadata
 */
typedef struct {
    uint8_t     data[ALIGN_TRACK_LENGTH * 2];   /**< Track data (double buffer for alignment) */
    size_t      length;                          /**< Actual track length */
    uint8_t     density;                         /**< Track density (0-3) + flags */
    uint8_t     alignment;                       /**< Alignment method used */
    int         halftrack;                       /**< Halftrack number (2-84) */
} track_buffer_t;

/* ============================================================================
 * API Functions - Main Alignment
 * ============================================================================ */

/**
 * @brief Align track using automatic detection
 * 
 * Tries multiple alignment methods and returns the best result.
 * 
 * @param buffer Track data buffer (modified in place)
 * @param length Track length
 * @param density Track density
 * @param halftrack Halftrack number
 * @param result Output alignment result
 * @return Pointer to aligned data start, or NULL on failure
 */
uint8_t *align_track_auto(uint8_t *buffer, size_t length, 
                          uint8_t density, int halftrack,
                          align_result_t *result);

/**
 * @brief Align track using specific method
 * 
 * @param buffer Track data buffer
 * @param length Track length
 * @param method Alignment method to use
 * @param result Output alignment result (can be NULL)
 * @return Pointer to aligned data start, or NULL on failure
 */
uint8_t *align_track(uint8_t *buffer, size_t length,
                     align_method_t method, align_result_t *result);

/* ============================================================================
 * API Functions - Protection-Specific Alignment
 * ============================================================================ */

/**
 * @brief Align V-MAX! protected track
 * 
 * Searches for V-MAX! duplicator marker bytes (0x4B, 0x69, 0x49, 0x5A, 0xA5)
 * and aligns track to start of marker run.
 * 
 * @param buffer Track data
 * @param length Track length
 * @return Pointer to alignment position, or NULL if not V-MAX!
 */
uint8_t *align_vmax(uint8_t *buffer, size_t length);

/**
 * @brief Align V-MAX! protected track (new algorithm)
 * 
 * Finds the longest run of V-MAX! markers for more reliable alignment.
 * 
 * @param buffer Track data
 * @param length Track length
 * @return Pointer to alignment position, or NULL if not V-MAX!
 */
uint8_t *align_vmax_new(uint8_t *buffer, size_t length);

/**
 * @brief Align V-MAX! Cinemaware variant
 * 
 * Cinemaware titles use signature: 0x64 0xA5 0xA5 0xA5
 * 
 * @param buffer Track data
 * @param length Track length
 * @return Pointer to alignment position, or NULL if not found
 */
uint8_t *align_vmax_cinemaware(uint8_t *buffer, size_t length);

/**
 * @brief Align Pirate Slayer protected track
 * 
 * Searches for Pirate Slayer signature with bit-shifting if needed.
 * Signature v1/v2: D7 D7 EB CC AD
 * 
 * @param buffer Track data (may be modified by bit shifting)
 * @param length Track length
 * @return Pointer to alignment position, or NULL if not found
 */
uint8_t *align_pirateslayer(uint8_t *buffer, size_t length);

/**
 * @brief Align RapidLok protected track
 * 
 * Complex alignment for RapidLok protection. Handles:
 * - Track headers with 0x75 sector IDs
 * - Extra sectors with 0x7B bytes
 * - DOS sectors with standard format
 * - Key sectors at track end
 * 
 * @param buffer Track data
 * @param length Track length
 * @param result Detailed result with RapidLok version info
 * @return Pointer to alignment position, or NULL if not RapidLok
 */
uint8_t *align_rapidlok(uint8_t *buffer, size_t length, align_result_t *result);

/* ============================================================================
 * API Functions - Generic Alignment
 * ============================================================================ */

/**
 * @brief Align to longest gap mark
 * 
 * Finds the longest run of identical bytes (gap) and aligns to its end.
 * 
 * @param buffer Track data
 * @param length Track length
 * @return Pointer to alignment position
 */
uint8_t *align_auto_gap(uint8_t *buffer, size_t length);

/**
 * @brief Align to bad GCR run
 * 
 * Bad GCR commonly occurs at track ends during mastering.
 * Aligns to the end of the longest bad GCR run.
 * 
 * @param buffer Track data
 * @param length Track length
 * @return Pointer to alignment position, or NULL if no bad GCR
 */
uint8_t *align_bad_gcr(uint8_t *buffer, size_t length);

/**
 * @brief Align to longest sync mark
 * 
 * @param buffer Track data
 * @param length Track length
 * @return Pointer to start of longest sync run
 */
uint8_t *align_long_sync(uint8_t *buffer, size_t length);

/**
 * @brief Align to sector 0 header
 * 
 * @param buffer Track data
 * @param length Track length
 * @param sector_length Output: length of sector (can be NULL)
 * @return Pointer to sector 0 position
 */
uint8_t *align_sector0(uint8_t *buffer, size_t length, size_t *sector_length);

/* ============================================================================
 * API Functions - Bitshift Repair
 * ============================================================================ */

/**
 * @brief Check if track is bitshifted
 * 
 * Bitshifted tracks have sectors that don't start on byte boundaries.
 * Common in Kryoflux and SCP captures.
 * 
 * @param buffer Track data
 * @param length Track length
 * @return true if track is bitshifted
 */
bool is_track_bitshifted(const uint8_t *buffer, size_t length);

/**
 * @brief Sync-align bitshifted track
 * 
 * Inserts pad bits before syncs so data sectors start on byte boundaries.
 * Used for Kryoflux G64 files.
 * 
 * @param buffer Track data (modified in place)
 * @param length Track length
 * @return New track length, or 0 on failure
 */
size_t sync_align_track(uint8_t *buffer, size_t length);

/**
 * @brief Align bitshifted track from Kryoflux
 * 
 * @param track_start Input track data
 * @param track_length Input track length
 * @param aligned_start Output: pointer to aligned data
 * @param aligned_length Output: aligned track length
 * @return 1 on success, 0 if no sync, -1 if empty
 */
int align_bitshifted_track(uint8_t *track_start, size_t track_length,
                           uint8_t **aligned_start, size_t *aligned_length);

/* ============================================================================
 * API Functions - Buffer Manipulation
 * ============================================================================ */

/**
 * @brief Shift buffer left by n bits
 * 
 * @param buffer Data buffer
 * @param length Buffer length
 * @param bits Number of bits to shift (1-7)
 */
void shift_buffer_left(uint8_t *buffer, size_t length, int bits);

/**
 * @brief Shift buffer right by n bits
 * 
 * @param buffer Data buffer
 * @param length Buffer length
 * @param bits Number of bits to shift (1-7)
 */
void shift_buffer_right(uint8_t *buffer, size_t length, int bits);

/**
 * @brief Rotate track data to new start position
 * 
 * @param buffer Track buffer (must be at least 2x track length)
 * @param length Track length
 * @param new_start Pointer to new start position within buffer
 * @return 0 on success
 */
int rotate_track(uint8_t *buffer, size_t length, uint8_t *new_start);

/* ============================================================================
 * API Functions - Fat Track Detection
 * ============================================================================ */

/**
 * @brief Search for fat (wide) tracks
 * 
 * Fat tracks span two halftracks. This function detects them by comparing
 * adjacent tracks for similarity.
 * 
 * @param track_buffer Full disk track buffer
 * @param track_density Density array
 * @param track_length Length array
 * @param fat_track Output: first fat track found (0 if none)
 * @return Number of fat tracks found
 */
int search_fat_tracks(uint8_t *track_buffer, uint8_t *track_density,
                      size_t *track_length, int *fat_track);

/**
 * @brief Check if specific track is fat
 * 
 * @param track_buffer Full disk track buffer
 * @param track_length Length array
 * @param halftrack Halftrack to check
 * @return Difference score (low = likely fat track)
 */
size_t check_fat_track(uint8_t *track_buffer, size_t *track_length, int halftrack);

/* ============================================================================
 * API Functions - Track Comparison
 * ============================================================================ */

/**
 * @brief Compare two tracks
 * 
 * @param track1 First track data
 * @param track2 Second track data
 * @param length1 First track length
 * @param length2 Second track length
 * @param same_disk true if tracks are from same disk
 * @param output Output string for differences (can be NULL)
 * @return Number of differences found
 */
size_t compare_tracks(const uint8_t *track1, const uint8_t *track2,
                      size_t length1, size_t length2, 
                      bool same_disk, char *output);

/**
 * @brief Compare sectors between two tracks
 * 
 * @param track1 First track data
 * @param track2 Second track data
 * @param length1 First track length
 * @param length2 Second track length
 * @param id1 First disk ID
 * @param id2 Second disk ID
 * @param track Track number
 * @param output Output string for differences (can be NULL)
 * @return Number of sector differences
 */
size_t compare_sectors(const uint8_t *track1, const uint8_t *track2,
                       size_t length1, size_t length2,
                       const uint8_t *id1, const uint8_t *id2,
                       int track, char *output);

/* ============================================================================
 * API Functions - GCR Helpers
 * ============================================================================ */

/**
 * @brief Find sync mark in GCR data
 * 
 * @param gcr_ptr Pointer to current position (updated on return)
 * @param gcr_end End of GCR data
 * @return true if sync found
 */
bool find_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end);

/**
 * @brief Find bitshifted sync mark
 * 
 * @param gcr_ptr Pointer to current position (updated on return)
 * @param gcr_end End of GCR data
 * @return Bit position (1-8) of sync start, or 0 if not found
 */
int find_bitshifted_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end);

/**
 * @brief Find end of sync mark
 * 
 * @param gcr_ptr Pointer to sync position (updated to end of sync)
 * @param gcr_end End of GCR data
 * @return Number of trailing 1 bits in last sync byte
 */
int find_end_of_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end);

/**
 * @brief Check if byte position contains bad GCR
 * 
 * @param buffer GCR data
 * @param length Buffer length
 * @param pos Position to check
 * @return true if bad GCR at position
 */
bool is_bad_gcr(const uint8_t *buffer, size_t length, size_t pos);

/**
 * @brief Fix first bad GCR byte in a run
 * 
 * @param buffer GCR data
 * @param length Buffer length
 * @param pos Position of bad GCR
 */
void fix_first_gcr(uint8_t *buffer, size_t length, size_t pos);

/**
 * @brief Fix last bad GCR byte in a run
 * 
 * @param buffer GCR data
 * @param length Buffer length
 * @param pos Position of bad GCR
 */
void fix_last_gcr(uint8_t *buffer, size_t length, size_t pos);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get alignment method name
 * 
 * @param method Alignment method
 * @return Human-readable name
 */
const char *align_method_name(align_method_t method);

/**
 * @brief Get expected track capacity for density
 * 
 * @param density Track density (0-3)
 * @return Expected capacity in bytes
 */
size_t get_track_capacity(int density);

/**
 * @brief Get minimum track capacity for density
 * 
 * @param density Track density (0-3)
 * @return Minimum capacity in bytes
 */
size_t get_track_capacity_min(int density);

/**
 * @brief Get maximum track capacity for density
 * 
 * @param density Track density (0-3)
 * @return Maximum capacity in bytes
 */
size_t get_track_capacity_max(int density);

/**
 * @brief Get sectors per track for track number
 * 
 * @param track Track number (1-42)
 * @return Number of sectors
 */
int get_sectors_per_track(int track);

/**
 * @brief Get density for track number
 * 
 * @param track Track number (1-42)
 * @return Density (0-3)
 */
int get_track_density(int track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_ALIGN_H */
