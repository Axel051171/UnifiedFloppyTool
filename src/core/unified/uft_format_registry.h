/**
 * @file uft_format_registry.h
 * @brief Unified Format Registry - Combines UFT parsers with FluxEngine capabilities
 * 
 * Based on FluxEngine's architecture for format detection and decoding.
 * Provides:
 * - Score-based auto-detection
 * - Format profiles with encoding/decoding settings
 * - Unified interface for all supported formats
 * 
 * @version 3.8.0
 * @date 2026-01-13
 */

#ifndef UFT_FORMAT_REGISTRY_H
#define UFT_FORMAT_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * ENCODING TYPES
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_FM,          /* Frequency Modulation (single density) */
    UFT_ENC_MFM,         /* Modified FM (double density) */
    UFT_ENC_M2FM,        /* Modified MFM (Intel) */
    UFT_ENC_GCR_APPLE,   /* Apple II/Mac GCR (6-and-2, 5-and-3) */
    UFT_ENC_GCR_C64,     /* Commodore GCR */
    UFT_ENC_GCR_VICTOR,  /* Victor 9000 GCR */
    UFT_ENC_GCR_BROTHER, /* Brother word processor GCR */
    UFT_ENC_RLL,         /* Run Length Limited */
    UFT_ENC_RAW,         /* Raw flux, no encoding */
} uft_encoding_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT CATEGORIES
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_CAT_UNKNOWN = 0,
    UFT_CAT_IBM_PC,      /* IBM PC compatible (DOS, Windows) */
    UFT_CAT_COMMODORE,   /* Commodore (C64, C128, Amiga) */
    UFT_CAT_APPLE,       /* Apple (II, Mac) */
    UFT_CAT_ATARI,       /* Atari (ST, 8-bit) */
    UFT_CAT_ACORN,       /* Acorn (BBC, Archimedes) */
    UFT_CAT_CPM,         /* CP/M systems */
    UFT_CAT_JAPANESE,    /* Japanese systems (PC-88, PC-98, X68000) */
    UFT_CAT_WORD_PROC,   /* Word processors (Brother, etc.) */
    UFT_CAT_INDUSTRIAL,  /* Industrial/scientific equipment */
    UFT_CAT_OTHER,       /* Other/exotic formats */
} uft_format_category_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT IDs (FluxEngine-compatible profile names)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_FMT_UNKNOWN = 0,
    
    /* IBM PC family */
    UFT_FMT_IBM_PC,          /* Generic IBM PC */
    UFT_FMT_IBM_180,         /* 180KB 5.25" SSDD */
    UFT_FMT_IBM_360,         /* 360KB 5.25" DSDD */
    UFT_FMT_IBM_720,         /* 720KB 3.5" DSDD */
    UFT_FMT_IBM_1200,        /* 1.2MB 5.25" DSHD */
    UFT_FMT_IBM_1440,        /* 1.44MB 3.5" DSHD */
    UFT_FMT_IBM_2880,        /* 2.88MB 3.5" DSED */
    
    /* Commodore */
    UFT_FMT_C64_1541,        /* C64 1541 (170KB) */
    UFT_FMT_C64_1571,        /* C64 1571 (340KB) */
    UFT_FMT_C64_1581,        /* C64 1581 (800KB) */
    UFT_FMT_C64_8050,        /* CBM 8050 (500KB) */
    UFT_FMT_AMIGA_DD,        /* Amiga DD (880KB) */
    UFT_FMT_AMIGA_HD,        /* Amiga HD (1.76MB) */
    
    /* Apple */
    UFT_FMT_APPLE2_DOS32,    /* Apple II DOS 3.2 (113KB, 5-and-3) */
    UFT_FMT_APPLE2_DOS33,    /* Apple II DOS 3.3 (140KB, 6-and-2) */
    UFT_FMT_APPLE2_PRODOS,   /* Apple II ProDOS */
    UFT_FMT_MAC_400,         /* Mac 400KB GCR */
    UFT_FMT_MAC_800,         /* Mac 800KB GCR */
    UFT_FMT_MAC_1440,        /* Mac 1.44MB MFM (PC compatible) */
    
    /* Atari */
    UFT_FMT_ATARI_ST,        /* Atari ST (various) */
    UFT_FMT_ATARI_8BIT,      /* Atari 8-bit */
    
    /* Acorn */
    UFT_FMT_ACORN_DFS,       /* Acorn DFS */
    UFT_FMT_ACORN_ADFS,      /* Acorn ADFS */
    
    /* CP/M */
    UFT_FMT_CPM_GENERIC,     /* Generic CP/M */
    UFT_FMT_CPM_AMPRO,       /* Ampro Little Board */
    UFT_FMT_CPM_EPSON,       /* Epson PF-10 */
    UFT_FMT_CPM_TARTU,       /* Tartu */
    
    /* Japanese */
    UFT_FMT_PC88,            /* NEC PC-88 */
    UFT_FMT_PC98,            /* NEC PC-98 */
    UFT_FMT_X68000,          /* Sharp X68000 */
    UFT_FMT_FM_TOWNS,        /* Fujitsu FM Towns */
    
    /* Word processors */
    UFT_FMT_BROTHER_120,     /* Brother 120KB */
    UFT_FMT_BROTHER_240,     /* Brother 240KB */
    
    /* Other */
    UFT_FMT_VICTOR_9000,     /* Victor 9000 / Sirius One */
    UFT_FMT_NORTHSTAR,       /* Northstar */
    UFT_FMT_MICROPOLIS,      /* Micropolis */
    UFT_FMT_HP_LIF,          /* HP LIF */
    UFT_FMT_TI99,            /* TI-99/4A */
    UFT_FMT_ROLAND_D20,      /* Roland D20 synthesizer */
    UFT_FMT_ZILOG_MCZ,       /* Zilog MCZ */
    UFT_FMT_BK,              /* Soviet BK */
    UFT_FMT_AGAT,            /* Soviet Agat (Apple II clone) */
    
    /* Raw/flux formats */
    UFT_FMT_RAW_FLUX,        /* Raw flux capture */
    UFT_FMT_SCP,             /* SuperCard Pro */
    UFT_FMT_KF_STREAM,       /* KryoFlux stream */
    UFT_FMT_A2R,             /* Applesauce A2R */
    
    UFT_FMT_COUNT            /* Number of formats */
} uft_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * DISK GEOMETRY
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int cylinders;           /* Number of cylinders (tracks) */
    int heads;               /* Number of heads (sides) */
    int sectors_per_track;   /* Sectors per track (0 = variable) */
    int sector_size;         /* Bytes per sector */
    int rpm;                 /* Rotational speed */
    int data_rate;           /* Data rate in kbps */
    bool variable_sectors;   /* Variable sectors per track (C64, Mac) */
    int first_sector;        /* First sector number (0 or 1) */
} uft_disk_geometry_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT PROFILE
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_format_id_t id;
    const char *name;            /* Short name (FluxEngine profile) */
    const char *description;     /* Human-readable description */
    uft_format_category_t category;
    uft_encoding_t encoding;
    uft_disk_geometry_t geometry;
    
    /* File extensions */
    const char *image_ext;       /* Primary image extension (.d64, .adf, .img) */
    const char *flux_ext;        /* Flux file extension */
    
    /* Detection */
    const uint8_t *magic_bytes;  /* Magic bytes for detection */
    size_t magic_len;
    size_t magic_offset;
    int file_sizes[8];           /* Valid file sizes (0-terminated) */
    
    /* Sync patterns */
    uint32_t sync_pattern;       /* Sync mark pattern */
    int sync_bits;               /* Sync pattern length in bits */
    
    /* Clock settings */
    double clock_period_us;      /* Nominal clock period in microseconds */
    double clock_tolerance;      /* Clock tolerance (0.0-0.5) */
    
    /* Read/write support */
    bool can_read;
    bool can_write;
    bool has_filesystem;         /* Direct filesystem access */
    const char *filesystem_name; /* VFS name (cbmfs, amigaffs, etc.) */
} uft_format_profile_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * DETECTION RESULT
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_format_id_t format;
    int confidence;              /* 0-100 confidence score */
    const char *reason;          /* Detection reason */
    uft_disk_geometry_t geometry; /* Detected geometry */
} uft_detection_result_t;

typedef struct {
    uft_detection_result_t results[10]; /* Top 10 candidates */
    int count;
} uft_detection_results_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT REGISTRY API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize format registry
 */
void uft_format_registry_init(void);

/**
 * @brief Get format profile by ID
 */
const uft_format_profile_t* uft_format_get_profile(uft_format_id_t id);

/**
 * @brief Get format profile by name
 */
const uft_format_profile_t* uft_format_get_by_name(const char *name);

/**
 * @brief Get all formats in a category
 */
int uft_format_get_by_category(uft_format_category_t category,
                                uft_format_id_t *out_ids, int max_count);

/**
 * @brief Auto-detect format from file
 * @param filename Path to disk image or flux file
 * @param results Output detection results (sorted by confidence)
 * @return Number of candidates found
 */
int uft_format_detect_file(const char *filename, uft_detection_results_t *results);

/**
 * @brief Auto-detect format from raw flux data
 * @param flux_data Raw flux transition data
 * @param flux_len Length of flux data
 * @param results Output detection results
 * @return Number of candidates found
 */
int uft_format_detect_flux(const uint8_t *flux_data, size_t flux_len,
                           uft_detection_results_t *results);

/**
 * @brief Auto-detect format from sector data
 * @param sector_data Raw sector data from track
 * @param sector_len Length of sector data
 * @param results Output detection results
 * @return Number of candidates found
 */
int uft_format_detect_sectors(const uint8_t *sector_data, size_t sector_len,
                              uft_detection_results_t *results);

/**
 * @brief Get FluxEngine profile name for format
 */
const char* uft_format_get_fluxengine_profile(uft_format_id_t id);

/**
 * @brief Get list of all supported formats
 */
int uft_format_list_all(uft_format_id_t *out_ids, int max_count);

/**
 * @brief Check if format supports reading
 */
bool uft_format_can_read(uft_format_id_t id);

/**
 * @brief Check if format supports writing
 */
bool uft_format_can_write(uft_format_id_t id);

/**
 * @brief Check if format has filesystem support
 */
bool uft_format_has_filesystem(uft_format_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_REGISTRY_H */
