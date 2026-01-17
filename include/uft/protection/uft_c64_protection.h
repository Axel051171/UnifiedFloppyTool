/**
 * @file uft_c64_protection.h
 * @brief C64/1541 Copy Protection Detection Module
 * @version 4.1.5
 * 
 * Comprehensive copy protection detection for Commodore 64 disk images.
 * Based on Super-Kit 1541 V2.0 documentation and extensive research.
 * 
 * Features:
 * - 1541 Drive Error Code Analysis
 * - Known Protection Scheme Detection
 * - Track 36-40 Extended Analysis
 * - GCR Anomaly Detection
 * - Signature Database (400+ titles)
 * - BAM Anomaly Detection
 * - Half-Track Detection
 * - Sync Mark Analysis
 */

#ifndef UFT_C64_PROTECTION_H
#define UFT_C64_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 1541 Drive Error Codes (Job Queue Results)
 * From Super-Kit 1541 V2.0 Errata Sheet
 * ============================================================================ */

typedef enum {
    C64_ERR_OK              = 0x01,  /* No error */
    C64_ERR_HEADER_NOT_FOUND = 0x02, /* Error 20: Header block not found */
    C64_ERR_NO_SYNC         = 0x03,  /* Error 21: No sync found (unformatted) */
    C64_ERR_DATA_NOT_FOUND  = 0x04,  /* Error 22: Data block not found */
    C64_ERR_CHECKSUM        = 0x05,  /* Error 23: Data block checksum error */
    C64_ERR_VERIFY          = 0x07,  /* Error 25: Verify error after write */
    C64_ERR_WRITE_PROTECT   = 0x08,  /* Error 26: Write protect error */
    C64_ERR_HEADER_CHECKSUM = 0x09,  /* Error 27: Header checksum error */
    C64_ERR_LONG_DATA       = 0x0A,  /* Error 28: Long data block */
    C64_ERR_ID_MISMATCH     = 0x0B,  /* Error 29: Disk ID mismatch */
} c64_error_code_t;

/* DOS Error to Job Queue mapping */
#define C64_DOS_ERR_20  C64_ERR_HEADER_NOT_FOUND
#define C64_DOS_ERR_21  C64_ERR_NO_SYNC
#define C64_DOS_ERR_22  C64_ERR_DATA_NOT_FOUND
#define C64_DOS_ERR_23  C64_ERR_CHECKSUM
#define C64_DOS_ERR_25  C64_ERR_VERIFY
#define C64_DOS_ERR_26  C64_ERR_WRITE_PROTECT
#define C64_DOS_ERR_27  C64_ERR_HEADER_CHECKSUM
#define C64_DOS_ERR_28  C64_ERR_LONG_DATA
#define C64_DOS_ERR_29  C64_ERR_ID_MISMATCH

/* ============================================================================
 * Known Copy Protection Schemes
 * ============================================================================ */

typedef enum {
    C64_PROT_NONE           = 0x0000,
    
    /* Error-Based Protection */
    C64_PROT_ERRORS_T18     = 0x0001,  /* Errors on Track 18 (directory) */
    C64_PROT_ERRORS_T36_40  = 0x0002,  /* Errors on extended tracks */
    C64_PROT_CUSTOM_ERRORS  = 0x0004,  /* Specific error patterns */
    
    /* Track-Based Protection */
    C64_PROT_EXTRA_TRACKS   = 0x0010,  /* Uses tracks 36-40 */
    C64_PROT_HALF_TRACKS    = 0x0020,  /* Uses half-tracks (36.5, etc.) */
    C64_PROT_KILLER_TRACKS  = 0x0040,  /* Tracks with no sync (unreadable) */
    
    /* Sector-Based Protection */
    C64_PROT_EXTRA_SECTORS  = 0x0100,  /* More than standard sectors */
    C64_PROT_MISSING_SECTORS = 0x0200, /* Fewer than standard sectors */
    C64_PROT_INTERLEAVE     = 0x0400,  /* Non-standard sector interleave */
    
    /* GCR-Based Protection */
    C64_PROT_GCR_TIMING     = 0x1000,  /* Non-standard bit timing */
    C64_PROT_GCR_DENSITY    = 0x2000,  /* Non-standard density zones */
    C64_PROT_GCR_SYNC       = 0x4000,  /* Non-standard sync marks */
    C64_PROT_GCR_LONG_TRACK = 0x8000,  /* Longer than normal track */
    C64_PROT_GCR_BAD_GCR    = 0x0008,  /* Invalid GCR patterns ($00, etc.) */
    
    /* Signature-Based Protection (known schemes) */
    C64_PROT_VORPAL         = 0x10000,  /* Epyx Vorpal */
    C64_PROT_V_MAX          = 0x20000,  /* V-Max! (Cinemaware/Activision) */
    C64_PROT_RAPIDLOK       = 0x40000,  /* RapidLok (Dane Final Agency) */
    C64_PROT_FAT_TRACK      = 0x80000,  /* Fat Track */
    C64_PROT_SPEEDLOCK      = 0x100000, /* Speedlock (Ocean/US Gold) */
    C64_PROT_NOVALOAD       = 0x200000, /* Novaload */
    C64_PROT_DATASOFT       = 0x400000, /* Datasoft long track protection */
    C64_PROT_SSI_RDOS       = 0x800000, /* SSI RapidDOS protection */
    C64_PROT_EA_INTERLOCK   = 0x1000000,/* EA Interlock */
    C64_PROT_ABACUS         = 0x2000000,/* Abacus protection */
    C64_PROT_RAINBIRD       = 0x4000000,/* Rainbird/Firebird protection */
    
} c64_protection_type_t;

/* ============================================================================
 * Datasoft Protection (used on Bruce Lee, Conan, Mr. Do, etc.)
 * Technical: Uses longer-than-normal tracks with extra data
 * ============================================================================ */

#define DATASOFT_LONG_TRACK_BYTES   6680  /* Max bytes per track (vs 6500 normal) */
#define DATASOFT_SIGNATURE_TRACK    18    /* Protection check on directory track */

/* ============================================================================
 * SSI RapidDOS Protection (Strategic Simulations Inc.)
 * Technical: Custom DOS with track-based key verification
 * Publishers: SSI (Pool of Radiance, Curse of Azure Bonds, War games)
 * ============================================================================ */

#define SSI_RDOS_KEY_TRACK          36    /* Key stored on track 36 */
#define SSI_RDOS_SECTORS_PER_TRACK  10    /* Non-standard 10 sectors */
#define SSI_RDOS_HEADER_MARKER      0x4B  /* Custom header marker */

/* ============================================================================
 * V-MAX! Version Detection (Pete Rittwage / Lord Crass documentation)
 * Publishers: Activision, Cinemaware, Mindscape, Origin, Sega, Taito
 * ============================================================================ */

typedef enum {
    VMAX_VERSION_UNKNOWN = 0,
    VMAX_VERSION_0,      /* Star Rank Boxing - first title, standard CBM DOS */
    VMAX_VERSION_1,      /* Activision games - standard CBM DOS, byte counting */
    VMAX_VERSION_2A,     /* Cinemaware v2a - single EOR'd track 20, standard DOS */
    VMAX_VERSION_2B,     /* Cinemaware v2b - dual EOR'd track 20, custom V-MAX sectors */
    VMAX_VERSION_3A,     /* Taito v3a - without short syncs */
    VMAX_VERSION_3B,     /* Taito v3b - with super short syncs */
    VMAX_VERSION_4,      /* Later variation - 4 marker bytes vs 7 */
} c64_vmax_version_t;

/* V-MAX! sector format constants */
#define VMAX_V2_SECTORS_ZONE1       22   /* Tracks 1-17 */
#define VMAX_V2_SECTORS_ZONE2       20   /* Tracks 18-38 */
#define VMAX_V2_SECTOR_SIZE         0x140 /* $140 bytes per sector */
#define VMAX_LOADER_TRACK           20   /* V-MAX loader track */
#define VMAX_RECOVERY_TRACK         19   /* V-MAX v3 recovery sector track */
#define VMAX_V3_MAX_SECTOR_SIZE     0x118 /* Max $118 GCR bytes in v3 */

/* V-MAX! GCR encoding ratios */
#define VMAX_GCR_RATIO_SECTOR       34   /* 3:4 ratio for sector data */
#define VMAX_GCR_RATIO_LOADER       23   /* 2:3 ratio for track 20 loader */

/* V-MAX! marker bytes */
#define VMAX_V2_MARKER_64           0x64
#define VMAX_V2_MARKER_46           0x46 /* Problematic - 3 zero bits in row */
#define VMAX_V2_MARKER_4E           0x4E
#define VMAX_V3_HEADER_MARKER       0x49 /* $49 bytes at sector start */
#define VMAX_V3_HEADER_END          0xEE /* End of header marker */
#define VMAX_END_OF_SECTOR          0x7F /* End-of-sector marker */

/* V-MAX! detection signatures */
#define VMAX_DIR_ENTRY_EXCLAIM      "!"  /* V2 disks have only "!" in directory */

/* ============================================================================
 * RapidLok Version Detection (Dane Final Agency / Pete Rittwage / Banguibob)
 * Publishers: MicroProse (Pirates!, F-15, Gunship, Kennedy Approach, etc.)
 * ============================================================================ */

typedef enum {
    RAPIDLOK_VERSION_UNKNOWN = 0,
    RAPIDLOK_VERSION_1,      /* v1: Patch keycheck works */
    RAPIDLOK_VERSION_2,      /* v2: Patch keycheck works */
    RAPIDLOK_VERSION_3,      /* v3: Patch keycheck works */
    RAPIDLOK_VERSION_4,      /* v4: Patch keycheck works */
    RAPIDLOK_VERSION_5,      /* v5: Intermittent failures in VICE */
    RAPIDLOK_VERSION_6,      /* v6: Intermittent failures in VICE */
    RAPIDLOK_VERSION_7,      /* v7: Requires additional crack work */
} c64_rapidlok_version_t;

/* RapidLok structure constants */
#define RAPIDLOK_KEY_TRACK          36   /* Track 36: encrypted key sector */
#define RAPIDLOK_SECTORS_ZONE1      12   /* Tracks 1-17: 12 sectors */
#define RAPIDLOK_SECTORS_ZONE2      11   /* Tracks 19-35: 11 sectors */
#define RAPIDLOK_BITRATE_ZONE1      11   /* 307692 Bit/s */
#define RAPIDLOK_BITRATE_ZONE2      10   /* 285714 Bit/s */

/* RapidLok sync lengths (bits) */
#define RAPIDLOK_TRACK_SYNC_BITS    320  /* Track start sync */
#define RAPIDLOK_SECTOR0_SYNC_BITS  480  /* Sector 0 sync */
#define RAPIDLOK_NORMAL_SYNC_BITS   40   /* Standard sync */
#define RAPIDLOK_DATA_SYNC_BITS     62   /* First data sector sync (62x$FF) */
#define RAPIDLOK_HEADER_SYNC_BITS   5    /* Other sector header syncs */

/* RapidLok marker bytes */
#define RAPIDLOK_EXTRA_SECTOR       0x7B /* $7B 'Extra sector' marker */
#define RAPIDLOK_EXTRA_START        0x55 /* Extra sector start byte */
#define RAPIDLOK_DOS_REF_HEADER     0x52 /* DOS reference header marker */
#define RAPIDLOK_SECTOR_HEADER      0x75 /* RapidLok sector header marker */
#define RAPIDLOK_DATA_BLOCK         0x6B /* RapidLok data block marker */
#define RAPIDLOK_BAD_GCR            0x00 /* Bad GCR in gaps (not checked) */

/* RapidLok data block size */
#define RAPIDLOK_DATA_SIZE          0x255 /* ~$255 bytes with parity */

/* ============================================================================
 * Protection Categories (Publishers)
 * ============================================================================ */

typedef enum {
    C64_PUB_UNKNOWN         = 0,
    C64_PUB_ACCOLADE,
    C64_PUB_ACTIVISION,
    C64_PUB_BRODERBUND,
    C64_PUB_CINEMAWARE,      /* V-MAX! v2 */
    C64_PUB_DANE_FINAL,      /* RapidLok creator */
    C64_PUB_DATASOFT,
    C64_PUB_ELECTRONIC_ARTS,
    C64_PUB_EPYX,            /* Vorpal protection */
    C64_PUB_MICROPROSE,      /* RapidLok user */
    C64_PUB_MINDSCAPE,       /* V-MAX! user */
    C64_PUB_OCEAN,           /* Speedlock/Novaload */
    C64_PUB_ORIGIN,          /* V-MAX! user */
    C64_PUB_SEGA,            /* V-MAX! user */
    C64_PUB_SSI,
    C64_PUB_SUBLOGIC,
    C64_PUB_TAITO,           /* V-MAX! v3 */
    C64_PUB_THUNDER_MOUNTAIN, /* V-MAX! user */
    C64_PUB_US_GOLD,         /* Speedlock */
    C64_PUB_OTHER,
} c64_publisher_t;

/* ============================================================================
 * Track Geometry (Standard 1541)
 * ============================================================================ */

#define C64_TRACKS_STANDARD     35
#define C64_TRACKS_EXTENDED     40
#define C64_TRACKS_WITH_HALF    84  /* 42 full tracks * 2 for half-tracks */

#define C64_DIR_TRACK           18
#define C64_BAM_TRACK           18
#define C64_BAM_SECTOR          0

/* Sectors per track (1541 density zones) */
static const int c64_sectors_per_track[] = {
    0,                          /* Track 0 doesn't exist */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                         /* 18-24 */
    18, 18, 18, 18, 18, 18,                                             /* 25-30 */
    17, 17, 17, 17, 17,                                                 /* 31-35 */
    17, 17, 17, 17, 17,                                                 /* 36-40 (extended) */
};

/* ============================================================================
 * Protection Analysis Results
 * ============================================================================ */

typedef struct {
    /* Basic info */
    char title[64];                     /* Detected title (if known) */
    c64_publisher_t publisher;          /* Detected publisher */
    uint32_t protection_flags;          /* Combination of c64_protection_type_t */
    int confidence;                     /* Detection confidence (0-100) */
    
    /* V-MAX! specific (if detected) */
    c64_vmax_version_t vmax_version;    /* V-MAX! version (0-4) */
    bool vmax_custom_sectors;           /* Uses custom V-MAX sectors vs CBM DOS */
    int vmax_loader_blocks;             /* Number of loader blocks on track 20 */
    uint8_t vmax_marker_bytes[2];       /* Header marker bytes detected */
    
    /* RapidLok specific (if detected) */
    c64_rapidlok_version_t rapidlok_version; /* RapidLok version (1-7) */
    bool rapidlok_key_valid;            /* Track 36 key sector valid */
    int rapidlok_7b_counts[36];         /* $7B byte counts per track (key table) */
    int rapidlok_sync_track_start;      /* Measured track start sync bits */
    int rapidlok_sync_sector0;          /* Measured sector 0 sync bits */
    
    /* Error analysis */
    int total_errors;                   /* Total error sectors found */
    int error_counts[16];               /* Count per error type */
    uint8_t error_tracks[41];           /* Bitmap of tracks with errors */
    
    /* Track analysis */
    int tracks_used;                    /* Highest track number used */
    bool uses_track_36_40;              /* Extended tracks detected */
    bool uses_half_tracks;              /* Half-tracks detected */
    int half_track_count;               /* Number of half-tracks */
    
    /* Sector analysis */
    int total_sectors;                  /* Total sectors in image */
    int non_standard_sectors;           /* Tracks with non-standard sector count */
    
    /* GCR analysis (for G64/NIB) */
    bool has_gcr_data;                  /* GCR data available */
    int sync_anomalies;                 /* Non-standard sync marks */
    int density_anomalies;              /* Non-standard density zones */
    int timing_anomalies;               /* Bit timing issues */
    
    /* BAM analysis */
    bool bam_valid;                     /* BAM checksum valid */
    int bam_free_blocks;                /* Blocks marked as free */
    int bam_allocated_blocks;           /* Blocks marked as used */
    bool bam_track_36_40;               /* BAM references extended tracks */
    
    /* Signature matches */
    char protection_name[64];           /* Name of detected protection */
    char notes[256];                    /* Additional notes */
    
} c64_protection_analysis_t;

/* ============================================================================
 * Known Title Database Entry
 * ============================================================================ */

typedef struct {
    const char *title;
    c64_publisher_t publisher;
    uint32_t protection_flags;
    const char *protection_name;
} c64_known_title_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Analyze a D64 image for copy protection
 * @param data Raw D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return 0 on success, error code on failure
 */
int c64_analyze_d64(const uint8_t *data, size_t size, c64_protection_analysis_t *result);

/**
 * @brief Analyze a G64 image for copy protection (GCR-level)
 * @param data Raw G64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return 0 on success, error code on failure
 */
int c64_analyze_g64(const uint8_t *data, size_t size, c64_protection_analysis_t *result);

/**
 * @brief Analyze D64 with error info (.d64 with error bytes)
 * @param data Raw D64 image data
 * @param size Size of image data (must be 175531 or 197376 for error info)
 * @param result Output analysis results
 * @return 0 on success, error code on failure
 */
int c64_analyze_d64_errors(const uint8_t *data, size_t size, c64_protection_analysis_t *result);

/**
 * @brief Check if a title is in the known protection database
 * @param title Title to search for
 * @param entry Output entry if found
 * @return true if found, false otherwise
 */
bool c64_lookup_title(const char *title, c64_known_title_t *entry);

/**
 * @brief Get human-readable error description
 * @param error_code 1541 error code
 * @return Error description string
 */
const char *c64_error_to_string(c64_error_code_t error_code);

/**
 * @brief Get human-readable protection type description
 * @param protection_type Protection type flags
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written
 */
int c64_protection_to_string(uint32_t protection_type, char *buffer, size_t buffer_size);

/**
 * @brief Generate detailed analysis report
 * @param analysis Analysis results
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written
 */
int c64_generate_report(const c64_protection_analysis_t *analysis, char *buffer, size_t buffer_size);

/**
 * @brief Get number of known titles in database
 * @return Number of known titles
 */
int c64_get_known_titles_count(void);

/**
 * @brief Get known title by index
 * @param index Index into database
 * @return Pointer to title entry or NULL
 */
const c64_known_title_t *c64_get_known_title(int index);

/**
 * @brief Detect V-MAX! version from G64 data
 * Based on Pete Rittwage and Lord Crass documentation
 * @param data G64 image data
 * @param size Size of image data
 * @param result Output analysis results (updates vmax_* fields)
 * @return V-MAX version detected or VMAX_VERSION_UNKNOWN
 */
c64_vmax_version_t c64_detect_vmax_version(const uint8_t *data, size_t size,
                                            c64_protection_analysis_t *result);

/**
 * @brief Detect RapidLok version from G64 data
 * Based on Pete Rittwage, Banguibob, and Kracker Jax documentation
 * @param data G64 image data
 * @param size Size of image data
 * @param result Output analysis results (updates rapidlok_* fields)
 * @return RapidLok version detected or RAPIDLOK_VERSION_UNKNOWN
 */
c64_rapidlok_version_t c64_detect_rapidlok_version(const uint8_t *data, size_t size,
                                                    c64_protection_analysis_t *result);

/**
 * @brief Get V-MAX! version string
 * @param version V-MAX! version enum
 * @return Human-readable version string
 */
const char *c64_vmax_version_string(c64_vmax_version_t version);

/**
 * @brief Get RapidLok version string
 * @param version RapidLok version enum
 * @return Human-readable version string
 */
const char *c64_rapidlok_version_string(c64_rapidlok_version_t version);

/**
 * @brief Extract RapidLok Track 36 key table
 * @param data G64 image data
 * @param size Size of image data
 * @param key_table Output array for 35 key values (bytes expected per track)
 * @return true if key extracted successfully
 */
bool c64_extract_rapidlok_key(const uint8_t *data, size_t size, uint8_t *key_table);

/**
 * @brief Check for V-MAX! "!" directory signature
 * @param data D64 or G64 image data
 * @param size Size of image data
 * @return true if V-MAX v2 directory signature found
 */
bool c64_check_vmax_directory(const uint8_t *data, size_t size);

/* ============================================================================
 * Additional Protection Detectors (v4.1.6)
 * ============================================================================ */

/**
 * @brief Detect Datasoft long track protection
 * Used by: Bruce Lee, Mr. Do!, Dig Dug, Pac-Man, Conan, etc.
 * Technical: Uses tracks with > 6680 bytes (vs ~6500 normal)
 * @param data G64 or D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if Datasoft protection detected
 */
bool c64_detect_datasoft(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result);

/**
 * @brief Detect Datasoft protection from D64 (name-based)
 * @param data D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if Datasoft protection detected
 */
bool c64_detect_datasoft_d64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result);

/**
 * @brief Detect SSI RapidDOS protection
 * Used by: Pool of Radiance, Curse of Azure Bonds, War games
 * Technical: Custom DOS with track 36 key, 10 sectors per track
 * @param data G64 or D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if SSI RapidDOS protection detected
 */
bool c64_detect_ssi_rdos(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result);

/**
 * @brief Detect SSI RapidDOS from G64 data
 * @param data G64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if SSI RapidDOS protection detected
 */
bool c64_detect_ssi_rdos_g64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result);

/**
 * @brief Detect SSI RapidDOS from D64 data
 * @param data D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if SSI RapidDOS protection detected
 */
bool c64_detect_ssi_rdos_d64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result);

/**
 * @brief Detect EA Interlock protection
 * Used by: Bard's Tale, Archon, Seven Cities of Gold, etc.
 * Technical: Custom DOS with interleave and specific boot sequence
 * @param data D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if EA Interlock protection detected
 */
bool c64_detect_ea_interlock(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result);

/**
 * @brief Detect Novaload protection (disk variant)
 * Used by: Combat School, Target Renegade, Gryzor, etc. (Ocean/Imagine)
 * Technical: Fast loader with anti-tampering, stack manipulation
 * @param data D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if Novaload protection detected
 */
bool c64_detect_novaload(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result);

/**
 * @brief Detect Speedlock protection
 * Used by: Many Ocean, US Gold titles (Gauntlet, Commando, etc.)
 * Technical: Custom loader with encrypted code, timing checks
 * @param data D64 image data
 * @param size Size of image data
 * @param result Output analysis results
 * @return true if Speedlock protection detected
 */
bool c64_detect_speedlock(const uint8_t *data, size_t size,
                           c64_protection_analysis_t *result);

/**
 * @brief Run all protection detectors on an image
 * Performs comprehensive analysis using all available detectors
 * @param data G64 or D64 image data
 * @param size Size of image data
 * @param result Output analysis results with all detected protections
 */
void c64_detect_all_protections(const uint8_t *data, size_t size,
                                 c64_protection_analysis_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_PROTECTION_H */
