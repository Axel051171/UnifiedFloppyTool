/**
 * @file c64_protection_internal.h
 * @brief Internal shared declarations for C64 protection modules
 * @version 4.1.5
 */

#ifndef C64_PROTECTION_INTERNAL_H
#define C64_PROTECTION_INTERNAL_H

#include "uft/protection/uft_c64_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * D64 Image Size Constants
 * ============================================================================ */

#define D64_35_TRACKS           174848   /* Standard 35 tracks */
#define D64_35_TRACKS_ERRORS    175531   /* 35 tracks + 683 error bytes */
#define D64_40_TRACKS           196608   /* Extended 40 tracks */
#define D64_40_TRACKS_ERRORS    197376   /* 40 tracks + 768 error bytes */

#define D64_SECTOR_SIZE         256
#define D64_SECTORS_35          683      /* Total sectors in 35 tracks */
#define D64_SECTORS_40          768      /* Total sectors in 40 tracks */

/* ============================================================================
 * Shared Internal Functions
 * ============================================================================ */

/* Get byte offset for a given track/sector in a D64 image */
size_t c64_d64_get_sector_offset(int track, int sector);

/* ============================================================================
 * Database Access (defined in c64_protection_db.c)
 * ============================================================================ */

extern const c64_known_title_t g_known_titles[];

#endif /* C64_PROTECTION_INTERNAL_H */
