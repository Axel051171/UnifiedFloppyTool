/*
 * uft_g64_offsets.h
 *
 * Standard G64 track offset tables.
 *
 * These offsets are for a standard 40-track G64 image
 * as created by nibtools/nibconvert.
 *
 * Data provided by user analysis of G64 files.
 *
 * Track Size per Zone:
 * - Zone 3 (Tracks 1-17):  ~$1E00 bytes ($1BDE = 7134 bytes typical)
 * - Zone 2 (Tracks 18-24): ~$1BDE bytes ($1BDE = 7134 bytes)
 * - Zone 1 (Tracks 25-30): ~$1C00 bytes
 * - Zone 0 (Tracks 31-40): ~$1A00 bytes
 */

#ifndef UFT_G64_OFFSETS_H
#define UFT_G64_OFFSETS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Standard G64 track offsets (40 tracks).
 *
 * These are byte-swapped from the raw hex values:
 *   AC02000000000000 for Track 01 => 0x000002AC
 *
 * Note: These offsets point to the track data offset in the G64 file,
 * where each track entry is:
 *   [2 bytes: track length (LE)] [track_length bytes: GCR data]
 */

/*
 * Track data offsets (derived from user-provided G64 analysis).
 *
 * Format: 8-byte Little-Endian values from speedflag-offset table.
 * Example: AC02000000000000 → LE32: 0x000002AC = 684 decimal
 *
 * Track size pattern: ~7930 bytes between tracks in Zone 3
 * Zone 2 tracks (18-24): $1BDE (7134) bytes per track (nibtools)
 */
static const uint32_t g64_standard_track_offsets[41] = {
    0,          /* Track 0 (unused) */
    
    /* Zone 3: 21 sectors per track (~7930 bytes) */
    0x000002AC, /* Track 01 - AC02000000000000 */
    0x000021A6, /* Track 02 - A621000000000000 */
    0x000040A0, /* Track 03 - A040000000000000 */
    0x00005F9A, /* Track 04 - 9A5F000000000000 */
    0x00007E94, /* Track 05 - 947E000000000000 */
    0x00009D8E, /* Track 06 - 8E9D000000000000 */
    0x0000BC88, /* Track 07 - 88BC000000000000 */
    0x0000DB82, /* Track 08 - 82DB000000000000 */
    0x0000FA7C, /* Track 09 - 7CFA000000000000 */
    0x00011976, /* Track 10 - 7619010000000000 */
    0x00013870, /* Track 11 - 7038010000000000 */
    0x0001576A, /* Track 12 - 6A57010000000000 */
    0x00017664, /* Track 13 - 6476010000000000 */
    0x0001955E, /* Track 14 - 5E95010000000000 */
    0x0001B458, /* Track 15 - 58B4010000000000 */
    0x0001D352, /* Track 16 - 52D3010000000000 */
    0x0001F24C, /* Track 17 - 4CF2010000000000 */
    
    /* Zone 2: 19 sectors per track ($1BDE = 7134 bytes) */
    0x00021146, /* Track 18 - 4611020000000000 - nibtools: $1BDE bytes */
    0x00023040, /* Track 19 - 4030020000000000 */
    0x00024F3A, /* Track 20 - 3A4F020000000000 */
    0x00026E34, /* Track 21 - 346E020000000000 */
    0x00028D2E, /* Track 22 - 2E8D020000000000 */
    0x0002AC28, /* Track 23 - 28AC020000000000 */
    0x0002CB22, /* Track 24 - 22CB020000000000 */
    
    /* Zone 1: 18 sectors per track */
    0x0002EA1C, /* Track 25 - 1CEA020000000000 */
    0x00030916, /* Track 26 - 1609030000000000 */
    0x00032810, /* Track 27 - 1028030000000000 */
    0x0003470A, /* Track 28 - 0A47030000000000 */
    0x00036604, /* Track 29 - 0466030000000000 */
    0x000384FE, /* Track 30 - FE84030000000000 */
    
    /* Zone 0: 17 sectors per track */
    0x0003A3F8, /* Track 31 - F8A3030000000000 */
    0x0003C2F2, /* Track 32 - F2C2030000000000 */
    0x0003E1EC, /* Track 33 - ECE1030000000000 */
    0x000400E6, /* Track 34 - E600040000000000 */
    0x00041FE0, /* Track 35 - E01F040000000000 */
    0x00043EDA, /* Track 36 - DA3E040000000000 */
    0x00045DD4, /* Track 37 - D45D040000000000 */
    0x00047CCE, /* Track 38 - CE7C040000000000 */
    0x00049BC8, /* Track 39 - C89B040000000000 */
    0x0004BAC2  /* Track 40 - C2BA040000000000 */
};

/*
 * Standard track sizes in bytes.
 * nibconvert uses $1BDE (7134) bytes for Zone 2 tracks.
 */
static const uint16_t g64_standard_track_sizes[41] = {
    0,      /* Track 0 (unused) */
    
    /* Zone 3: 21 sectors (tracks 1-17) */
    0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA,  /* 1-5 */
    0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA,  /* 6-10 */
    0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA,  /* 11-15 */
    0x1EFA, 0x1EFA,                          /* 16-17 */
    
    /* Zone 2: 19 sectors (tracks 18-24) */
    0x1BDE, 0x1BDE, 0x1BDE, 0x1BDE,          /* 18-21 */
    0x1BDE, 0x1BDE, 0x1BDE,                  /* 22-24 */
    
    /* Zone 1: 18 sectors (tracks 25-30) */
    0x1EFA, 0x1EFA, 0x1EFA,                  /* 25-27 */
    0x1EFA, 0x1EFA, 0x1EFA,                  /* 28-30 */
    
    /* Zone 0: 17 sectors (tracks 31-40) */
    0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA,  /* 31-35 */
    0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA, 0x1EFA   /* 36-40 */
};

/*
 * Calculate G64 track offset from track number.
 * For standard 40-track images.
 */
static inline uint32_t g64_get_track_offset(int track)
{
    if (track < 1 || track > 40) return 0;
    return g64_standard_track_offsets[track];
}

/*
 * Calculate expected track size from track number.
 */
static inline uint16_t g64_get_track_size(int track)
{
    if (track < 1 || track > 40) return 0;
    return g64_standard_track_sizes[track];
}

/*
 * Calculate half-track offset (for half-track images).
 * Half-tracks are interleaved: 1, 1.5, 2, 2.5, ...
 */
static inline uint32_t g64_get_halftrack_offset(int halftrack)
{
    if (halftrack < 1 || halftrack > 84) return 0;
    
    /* Real tracks are at even positions in half-track numbering */
    /* Half-track 2 = Track 1, Half-track 4 = Track 2, etc. */
    int real_track = halftrack / 2;
    if (real_track < 1 || real_track > 40) return 0;
    
    /* For actual half-tracks (odd numbers), estimate position */
    if (halftrack % 2 == 1) {
        /* Between tracks - interpolate */
        if (real_track == 0) return g64_standard_track_offsets[1] / 2;
        return (g64_standard_track_offsets[real_track] + 
                g64_standard_track_offsets[real_track + 1]) / 2;
    }
    
    return g64_standard_track_offsets[real_track];
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_G64_OFFSETS_H */
