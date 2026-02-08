/**
 * @file uft_xdf.h
 * @brief IBM XDF (eXtended Density Format) Support
 * 
 * XDF was developed by IBM for OS/2 to squeeze ~1.86MB onto a 3.5" HD disk.
 * Uses variable sector sizes per track - requires Track Copy mode in XCopy.
 * 
 * XDF Layout:
 *   Track 0:     4 sectors (8KB + 2KB + 1KB + 512B = 11.5KB per side)
 *   Tracks 1-79: 5 sectors (8KB + 8KB + 2KB + 1KB + 512B = 19.5KB per side)
 * 
 * Total: ~1.86MB (1,915,904 bytes)
 * 
 * XCopy Compatibility:
 *   ✗ Normal (Sector) Copy - FAILS due to variable sector sizes
 *   ✓ Track Copy          - WORKS (recommended)
 *   ✓ Nibble Copy         - WORKS
 *   ✓ Flux Copy           - WORKS (best for protected XDF)
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_XDF_H
#define UFT_XDF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * XDF Constants
 * ============================================================================ */

#define XDF_TRACKS              80
#define XDF_HEADS               2
#define XDF_TRACK0_SECTORS      4
#define XDF_STANDARD_SECTORS    5
#define XDF_MAX_SECTOR_SIZE     8192    /* 8 KB */
#define XDF_DISK_SIZE           1915904 /* ~1.86 MB */

/* XDF sector size codes (IBM N-field) */
#define XDF_SIZE_512    2   /* N=2: 128 << 2 = 512 */
#define XDF_SIZE_1024   3   /* N=3: 128 << 3 = 1024 */
#define XDF_SIZE_2048   4   /* N=4: 128 << 4 = 2048 */
#define XDF_SIZE_8192   6   /* N=6: 128 << 6 = 8192 */

/* ============================================================================
 * XDF Sector Layout
 * ============================================================================ */

/**
 * @brief XDF sector descriptor
 */
typedef struct {
    uint8_t  cylinder;      /* C - always matches physical track */
    uint8_t  head;          /* H - 0 or 1 */
    uint8_t  record;        /* R - sector number (1-based) */
    uint8_t  size_n;        /* N - size code */
    uint16_t size_bytes;    /* Actual size in bytes */
} uft_xdf_sector_t;

/**
 * @brief XDF track layout
 */
typedef struct {
    int sector_count;
    uft_xdf_sector_t sectors[8];
    size_t total_data;      /* Total data bytes on track */
    size_t raw_track_len;   /* Raw MFM track length */
} uft_xdf_track_layout_t;

/* ============================================================================
 * XDF API Functions
 * ============================================================================ */

/**
 * @brief Get XDF track layout for given track
 * 
 * @param track Track number (0-79)
 * @param head  Head number (0-1)
 * @param layout Output layout structure
 * @return 0 on success, -1 on error
 */
int uft_xdf_get_track_layout(int track, int head, uft_xdf_track_layout_t *layout);

/**
 * @brief Get number of sectors on XDF track
 * 
 * @param track Track number (0-79)
 * @return Sector count (4 for track 0, 5 for others), 0 on error
 */
int uft_xdf_sectors_for_track(int track);

/**
 * @brief Get sector size by index on XDF track
 * 
 * @param track Track number
 * @param sector_index Sector index (0-based)
 * @return Sector size in bytes, 0 on error
 */
int uft_xdf_sector_size(int track, int sector_index);

/**
 * @brief Calculate total XDF disk size
 * @return Size in bytes (~1.86MB)
 */
size_t uft_xdf_disk_size(void);

/**
 * @brief Detect if file is likely XDF format
 * 
 * @param data File data
 * @param size File size
 * @return Confidence 0-100
 */
int uft_xdf_detect(const uint8_t *data, size_t size);

/**
 * @brief Check if size matches known XDF variants
 * @param size File size
 * @return true if likely XDF
 */
bool uft_xdf_detect_by_size(size_t size);

/**
 * @brief Get recommended XCopy mode for XDF
 * 
 * XDF requires Track Copy or better due to variable sector sizes.
 * 
 * @param has_protection true if copy protection detected
 * @return Recommended copy mode (2=Track, 3=Flux)
 */
int uft_xdf_recommended_copy_mode(bool has_protection);

/**
 * @brief Validate XDF track data
 * 
 * @param track_data Raw track bytes
 * @param track_len Track length
 * @param track Track number
 * @param head Head number
 * @return 0 if valid, error code otherwise
 */
int uft_xdf_validate_track(const uint8_t *track_data, size_t track_len,
                           int track, int head);

/* ============================================================================
 * XDF ↔ XCopy Integration
 * ============================================================================ */

/**
 * @brief Check if format requires Track Copy mode
 * 
 * XDF and similar formats with variable sectors cannot use
 * normal sector copy.
 * 
 * @param format_name Format identifier
 * @return true if Track/Nibble/Flux copy required
 */
bool uft_format_requires_track_copy(const char *format_name);

/**
 * @brief Get XDF-aware copy recommendation
 * 
 * For use by AnalyzerToolbar to suggest correct mode.
 * 
 * @param track_data Track data
 * @param track_len Track length  
 * @param out_mode Output: recommended mode (0-3)
 * @param out_reason Output: reason string
 * @return 0 on success
 */
int uft_xdf_analyze_for_copy(const uint8_t *track_data, size_t track_len,
                             int *out_mode, const char **out_reason);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_H */
