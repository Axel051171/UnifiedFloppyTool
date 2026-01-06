/*
 * uft_c64_track_format.c
 *
 * C64/1541 track format tables and functions.
 * Based on nibtools by Pete Rittwage and Markus Brenner.
 *
 * Original Source:
 * - nibtools gcr.c (C) Markus Brenner <markus(at)brenner.de>
 * - nibtools gcr.h (C) Pete Rittwage <peter(at)rittwage.com>
 *
 * License: GPL (from nibtools)
 */

#include "c64/uft_c64_track_format.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * STATIC TABLES (from nibtools)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Sectors per track table.
 * Index 0 unused, tracks 1-42.
 */
static const uint8_t sector_map[UFT_C64_MAX_TRACKS_1541 + 1] = {
    0,                                      /* Track 0 (unused) */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, /* Tracks  1-10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19, /* Tracks 11-20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18, /* Tracks 21-30 */
    17, 17, 17, 17, 17,                     /* Tracks 31-35 */
    17, 17, 17, 17, 17, 17, 17              /* Tracks 36-42 (non-standard) */
};

/*
 * Speed zone table.
 * Index 0 unused, tracks 1-42.
 */
static const uint8_t speed_map[UFT_C64_MAX_TRACKS_1541 + 1] = {
    0,                                      /* Track 0 (unused) */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,           /* Tracks  1-10: Zone 3 */
    3, 3, 3, 3, 3, 3, 3, 2, 2, 2,           /* Tracks 11-20: Zone 3, then 2 */
    2, 2, 2, 2, 1, 1, 1, 1, 1, 1,           /* Tracks 21-30: Zone 2, then 1 */
    0, 0, 0, 0, 0,                          /* Tracks 31-35: Zone 0 */
    0, 0, 0, 0, 0, 0, 0                     /* Tracks 36-42 (non-standard) */
};

/*
 * Inter-sector gap length table.
 * Index 0 unused, tracks 1-42.
 */
static const uint8_t gap_map[UFT_C64_MAX_TRACKS_1541 + 1] = {
    0,                                      /* Track 0 (unused) */
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, /* Tracks  1-10 */
    10, 10, 10, 10, 10, 10, 10, 17, 17, 17, /* Tracks 11-20 */
    17, 17, 17, 17, 11, 11, 11, 11, 11, 11, /* Tracks 21-30 */
     8,  8,  8,  8,  8,                     /* Tracks 31-35 */
     8,  8,  8,  8,  8,  8,  8              /* Tracks 36-42 (non-standard) */
};

/*
 * Data rate per zone (bytes per minute).
 * Formula: (4,000,000 / (divisor * 8)) * 60 / 8
 * Where divisor is 13, 14, 15, 16 for zones 3, 2, 1, 0
 */
static const uint32_t density_bpm[4] = {
    UFT_C64_DENSITY_0,  /* Zone 0: 1875000 */
    UFT_C64_DENSITY_1,  /* Zone 1: 2000000 */
    UFT_C64_DENSITY_2,  /* Zone 2: 2142857 */
    UFT_C64_DENSITY_3   /* Zone 3: 2307692 */
};

/*
 * Track capacity at 300 RPM.
 * capacity = density_bpm / 300
 */
static const size_t capacity_300rpm[4] = {
    6250,   /* Zone 0 */
    6667,   /* Zone 1 */
    7143,   /* Zone 2 */
    7692    /* Zone 3 */
};

/*
 * Track capacity min/max (accounting for RPM variation ~295-305).
 */
static const size_t capacity_min[4] = {
    6147,   /* Zone 0 at 305 RPM */
    6557,   /* Zone 1 */
    7024,   /* Zone 2 */
    7566    /* Zone 3 */
};

static const size_t capacity_max[4] = {
    6356,   /* Zone 0 at 295 RPM */
    6780,   /* Zone 1 */
    7264,   /* Zone 2 */
    7822    /* Zone 3 */
};

/*
 * Block offset table (cumulative sectors before each track).
 */
static const int block_offset[UFT_C64_MAX_TRACKS_1541 + 1] = {
    0,                                      /* Track 0 */
    0,   21,  42,  63,  84, 105, 126, 147, 168, 189,  /* Tracks  1-10 */
    210, 231, 252, 273, 294, 315, 336, 357, 376, 395, /* Tracks 11-20 */
    414, 433, 452, 471, 490, 508, 526, 544, 562, 580, /* Tracks 21-30 */
    598, 615, 632, 649, 666,                          /* Tracks 31-35 */
    683, 700, 717, 734, 751, 768, 785                 /* Tracks 36-42 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK FORMAT FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

uint8_t uft_c64_sectors_per_track(int track)
{
    if (track < 1 || track > UFT_C64_MAX_TRACKS_1541) {
        return 0;
    }
    return sector_map[track];
}

uft_c64_speed_zone_t uft_c64_speed_zone(int track)
{
    if (track < 1 || track > UFT_C64_MAX_TRACKS_1541) {
        return UFT_C64_SPEED_ZONE_0;
    }
    return (uft_c64_speed_zone_t)speed_map[track];
}

uint8_t uft_c64_sector_gap_length(int track)
{
    if (track < 1 || track > UFT_C64_MAX_TRACKS_1541) {
        return 8;  /* Default to zone 0 gap */
    }
    return gap_map[track];
}

uint32_t uft_c64_zone_data_rate(uft_c64_speed_zone_t zone)
{
    if (zone > UFT_C64_SPEED_ZONE_3) {
        return density_bpm[0];
    }
    return density_bpm[zone];
}

size_t uft_c64_track_capacity(uft_c64_speed_zone_t zone, int rpm)
{
    if (zone > UFT_C64_SPEED_ZONE_3) {
        zone = UFT_C64_SPEED_ZONE_0;
    }
    if (rpm <= 0) {
        rpm = 300;
    }
    return density_bpm[zone] / (uint32_t)rpm;
}

size_t uft_c64_track_capacity_min(uft_c64_speed_zone_t zone)
{
    if (zone > UFT_C64_SPEED_ZONE_3) {
        return capacity_min[0];
    }
    return capacity_min[zone];
}

size_t uft_c64_track_capacity_max(uft_c64_speed_zone_t zone)
{
    if (zone > UFT_C64_SPEED_ZONE_3) {
        return capacity_max[0];
    }
    return capacity_max[zone];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * D64 BLOCK ADDRESSING
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_c64_ts_to_block(int track, int sector)
{
    if (track < 1 || track > 40) {
        return -1;
    }
    if (sector < 0 || sector >= sector_map[track]) {
        return -1;
    }
    return block_offset[track] + sector;
}

bool uft_c64_block_to_ts(int block, int *track_out, int *sector_out)
{
    if (block < 0 || block >= UFT_C64_MAX_BLOCKS) {
        return false;
    }
    
    /* Binary search for track */
    int track = 1;
    while (track <= 40 && block_offset[track + 1] <= block) {
        track++;
    }
    
    int sector = block - block_offset[track];
    
    if (track_out) *track_out = track;
    if (sector_out) *sector_out = sector;
    
    return true;
}

int uft_c64_d64_offset(int track, int sector)
{
    int block = uft_c64_ts_to_block(track, sector);
    if (block < 0) {
        return -1;
    }
    return block * 256;  /* 256 bytes per sector */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DIRECTORY TRACK
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_is_dir_sector(int track, int sector)
{
    if (track != UFT_C64_DIR_TRACK) {
        return false;
    }
    /* BAM is sector 0, directory chain starts at sector 1 */
    return (sector >= 0 && sector < sector_map[UFT_C64_DIR_TRACK]);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_is_standard_track(int track)
{
    return (track >= 1 && track <= UFT_C64_STANDARD_TRACKS);
}

bool uft_c64_is_extended_track(int track)
{
    return (track >= 1 && track <= 40);
}

bool uft_c64_is_halftrack_data(int halftrack)
{
    /* Standard tracks are even halftracks (2, 4, 6, ..., 70 for tracks 1-35) */
    /* Odd halftracks (1, 3, 5, ...) are "between" tracks and normally empty */
    /* But protection schemes use them */
    
    if (halftrack < 0 || halftrack > UFT_C64_MAX_HALFTRACKS_1541) {
        return false;
    }
    
    /* Halftracks 1-84 could potentially contain data */
    /* Standard data is on even halftracks */
    /* Return true for odd halftracks (potential protection data) */
    return (halftrack & 1) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RAW TABLE ACCESS
 * ═══════════════════════════════════════════════════════════════════════════ */

const uint8_t *uft_c64_get_sector_map(void)
{
    return sector_map;
}

const uint8_t *uft_c64_get_speed_map(void)
{
    return speed_map;
}

const uint8_t *uft_c64_get_gap_map(void)
{
    return gap_map;
}
