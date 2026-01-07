/*
 * uft_c64_track_align.h
 *
 * Track alignment functions for C64/1541 disk preservation.
 *
 * Purpose:
 * - Align track data to optimal starting points for preservation
 * - Detect and handle various copy protection schemes
 * - Support fat tracks, half tracks, and non-standard formats
 *
 * Protection Schemes Supported:
 * - V-MAX! (Cinemaware, Mastertronic)
 * - PirateSlayer/EA (Electronic Arts)
 * - Rapidlok (various versions 1-7)
 * - Fat Tracks (wide tracks covering multiple halftracks)
 * - Custom sync/gap alignments
 *
 * Original Source:
 * - GCR routines (C) Markus Brenner <markus(at)brenner.de>
 *
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_track_align.c
 */

#ifndef UFT_C64_TRACK_ALIGN_H
#define UFT_C64_TRACK_ALIGN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Maximum track buffer size (NIB format) */
#define UFT_NIB_TRACK_LENGTH    0x2000   /* 8192 bytes */

/* Maximum tracks for 1541/1571 */
#define UFT_MAX_TRACKS_1541     42
#define UFT_MAX_HALFTRACKS_1541 (UFT_MAX_TRACKS_1541 * 2)

/* Track alignment methods */
typedef enum uft_c64_align_method {
    UFT_C64_ALIGN_NONE       = 0x0,   /* No alignment */
    UFT_C64_ALIGN_GAP        = 0x1,   /* Align to sector gap */
    UFT_C64_ALIGN_SEC0       = 0x2,   /* Align to sector 0 */
    UFT_C64_ALIGN_LONGSYNC   = 0x3,   /* Align to longest sync run */
    UFT_C64_ALIGN_BADGCR     = 0x4,   /* Align to bad GCR region */
    UFT_C64_ALIGN_VMAX       = 0x5,   /* V-MAX! protection alignment */
    UFT_C64_ALIGN_AUTOGAP    = 0x6,   /* Auto-detect gap alignment */
    UFT_C64_ALIGN_VMAX_CW    = 0x7,   /* V-MAX Cinemaware variant */
    UFT_C64_ALIGN_RAW        = 0x8,   /* Raw (no processing) */
    UFT_C64_ALIGN_PSLAYER    = 0x9,   /* EA PirateSlayer alignment */
    UFT_C64_ALIGN_RAPIDLOK   = 0xA    /* Rapidlok alignment */
} uft_c64_align_method_t;

/* V-MAX marker bytes (duplicator signatures) */
#define UFT_VMAX_MARKER_4B  0x4B
#define UFT_VMAX_MARKER_49  0x49
#define UFT_VMAX_MARKER_69  0x69
#define UFT_VMAX_MARKER_5A  0x5A
#define UFT_VMAX_MARKER_A5  0xA5

/* Rapidlok TV standards */
typedef enum uft_rapidlok_tv {
    UFT_RL_TV_UNKNOWN = 0,
    UFT_RL_TV_NTSC    = 1,
    UFT_RL_TV_PAL     = 2
} uft_rapidlok_tv_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * ALIGNMENT RESULT STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_c64_align_result {
    uft_c64_align_method_t method;    /* Method used */
    size_t                 offset;    /* Offset to align point */
    bool                   found;     /* Alignment point found? */
    
    /* V-MAX specific */
    size_t                 marker_run;     /* Length of marker run */
    
    /* Rapidlok specific */
    int                    rl_version;     /* Rapidlok version (1-7) */
    uft_rapidlok_tv_t      rl_tv;          /* TV standard */
    size_t                 rl_th_length;   /* Track header length */
    
    /* PirateSlayer specific */
    int                    ps_version;     /* PirateSlayer version */
    
    /* Fat track detection */
    bool                   is_fat_track;   /* Track is a fat track? */
    int                    fat_track_num;  /* Which halftrack is duplicated */
} uft_c64_align_result_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * CORE ALIGNMENT FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Align track to V-MAX marker bytes.
 *
 * V-MAX protection uses marker sequences like $4B, $69, $49, $5A, $A5
 * at track boundaries. This function finds the longest run.
 *
 * @param buffer     Track data buffer
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Offset to alignment point, or 0 if not found
 */
size_t uft_c64_align_vmax(const uint8_t *buffer, size_t length,
                          uft_c64_align_result_t *result);

/**
 * Align track to V-MAX Cinemaware variant.
 *
 * Cinemaware titles use marker sequence $64 $A5 $A5 $A5.
 *
 * @param buffer     Track data buffer
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Offset to alignment point, or 0 if not found
 */
size_t uft_c64_align_vmax_cw(const uint8_t *buffer, size_t length,
                             uft_c64_align_result_t *result);

/**
 * Align track to EA PirateSlayer signature.
 *
 * PirateSlayer uses signatures like:
 * - Version 1/2: $D7 $D7 $EB $CC $AD
 * - Alternate: $EB $D7 $AA $55
 *
 * @param buffer     Track data buffer (will be shifted during search!)
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Offset to alignment point, or 0 if not found
 */
size_t uft_c64_align_pirateslayer(uint8_t *buffer, size_t length,
                                  uft_c64_align_result_t *result);

/**
 * Align track to Rapidlok track header.
 *
 * Rapidlok uses special track headers with:
 * - 14-24 sync bytes ($FF)
 * - $55 ID byte
 * - 60-300 $7B/$4B bytes
 *
 * Also detects Rapidlok version (1-7) and TV standard (NTSC/PAL).
 *
 * @param buffer     Track data buffer
 * @param length     Track length in bytes
 * @param result     Output alignment result (includes RL version/TV)
 * @return           Offset to alignment point, or 0 if not found
 */
size_t uft_c64_align_rapidlok(const uint8_t *buffer, size_t length,
                              uft_c64_align_result_t *result);

/**
 * Align track to longest sync run.
 *
 * Finds the longest sequence of $FF bytes and returns its start.
 *
 * @param buffer     Track data buffer
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Offset to start of longest sync, or 0 if not found
 */
size_t uft_c64_align_longsync(const uint8_t *buffer, size_t length,
                              uft_c64_align_result_t *result);

/**
 * Align track to longest gap region.
 *
 * Finds the longest run of any single repeated byte (typically $55).
 *
 * @param buffer     Track data buffer
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Offset to end of gap region, or 0 if not found
 */
size_t uft_c64_align_autogap(const uint8_t *buffer, size_t length,
                             uft_c64_align_result_t *result);

/**
 * Align track to bad GCR region.
 *
 * Bad GCR often occurs at track boundaries during mastering.
 * This finds the longest run of invalid GCR bytes.
 *
 * @param buffer     Track data buffer
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Offset to end of bad GCR run, or 0 if not found
 */
size_t uft_c64_align_badgcr(const uint8_t *buffer, size_t length,
                            uft_c64_align_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC ALIGNMENT (Kryoflux/Raw Stream Support)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Perform sync alignment on non-sync-aligned images.
 *
 * This handles images created from raw Kryoflux streams that may not
 * start at a sync boundary. The function:
 * 1. Shuffles buffer to start at a sync mark
 * 2. Bit-shifts data between sync marks to proper byte boundaries
 *
 * WARNING: This modifies the buffer in place and may destroy
 * non-standard sync lengths or custom protection data.
 *
 * @param buffer     Track data buffer (modified in place)
 * @param length     Track length in bytes
 * @return           1 on success, 0 if no sync found
 */
int uft_c64_sync_align(uint8_t *buffer, size_t length);

/* ═══════════════════════════════════════════════════════════════════════════
 * FAT TRACK DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Search for fat tracks in a disk image.
 *
 * Fat tracks are tracks written with a wider-than-normal write head,
 * causing them to overwrite adjacent halftracks. Common in:
 * - Early EA games
 * - Activision titles
 * - XEMAG protection
 *
 * @param track_buffer   Full disk buffer (all halftracks)
 * @param track_density  Density array for each halftrack
 * @param track_length   Length array for each halftrack
 * @param fat_track_out  Output: halftrack number of fat track (or -1)
 * @return               Number of fat tracks found
 */
int uft_c64_search_fat_tracks(const uint8_t *track_buffer,
                              const uint8_t *track_density,
                              const size_t *track_length,
                              int *fat_track_out);

/* ═══════════════════════════════════════════════════════════════════════════
 * BUFFER MANIPULATION UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Shift buffer left by n bits.
 *
 * @param buffer     Data buffer (modified in place)
 * @param length     Buffer length in bytes
 * @param bits       Number of bits to shift (0-7)
 */
void uft_c64_shift_buffer_left(uint8_t *buffer, size_t length, int bits);

/**
 * Shift buffer right by n bits.
 *
 * @param buffer     Data buffer (modified in place)
 * @param length     Buffer length in bytes
 * @param bits       Number of bits to shift (0-7)
 */
void uft_c64_shift_buffer_right(uint8_t *buffer, size_t length, int bits);

/**
 * Rotate buffer by offset bytes (shuffle).
 *
 * Moves data so that byte at 'offset' becomes byte 0.
 *
 * @param buffer     Data buffer (modified in place)
 * @param length     Buffer length in bytes
 * @param offset     Rotation offset
 */
void uft_c64_rotate_buffer(uint8_t *buffer, size_t length, size_t offset);

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR VALIDATION HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Check if byte at position is invalid GCR.
 *
 * Invalid GCR occurs when the 5-bit GCR symbols don't correspond
 * to valid nibble values (>=32 values map to nothing).
 *
 * @param buffer     Track data buffer
 * @param length     Buffer length
 * @param pos        Position to check
 * @return           true if bad GCR detected at position
 */
bool uft_c64_is_bad_gcr(const uint8_t *buffer, size_t length, size_t pos);

/**
 * Count total bad GCR bytes in buffer.
 *
 * @param buffer     Track data buffer
 * @param length     Buffer length
 * @return           Number of bad GCR positions found
 */
size_t uft_c64_count_bad_gcr(const uint8_t *buffer, size_t length);

/**
 * Find next sync mark in buffer.
 *
 * Sync is defined as 10+ consecutive '1' bits, typically appearing
 * as $FF followed by a byte with MSB set.
 *
 * @param buffer     Track data buffer
 * @param length     Buffer length
 * @param start      Starting position
 * @return           Position of sync mark, or length if not found
 */
size_t uft_c64_find_sync(const uint8_t *buffer, size_t length, size_t start);

/* ═══════════════════════════════════════════════════════════════════════════
 * HIGH-LEVEL AUTO-ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Auto-detect and apply best alignment for a track.
 *
 * Tries alignment methods in priority order:
 * 1. V-MAX markers
 * 2. PirateSlayer signature
 * 3. Rapidlok track header
 * 4. Longest sync run
 * 5. Auto gap detection
 *
 * @param buffer     Track data buffer (may be modified)
 * @param length     Track length in bytes
 * @param result     Output alignment result
 * @return           Alignment method used
 */
uft_c64_align_method_t uft_c64_auto_align(uint8_t *buffer, size_t length,
                                          uft_c64_align_result_t *result);

/**
 * Get human-readable name for alignment method.
 *
 * @param method     Alignment method enum
 * @return           Static string with method name
 */
const char *uft_c64_align_method_name(uft_c64_align_method_t method);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_TRACK_ALIGN_H */
