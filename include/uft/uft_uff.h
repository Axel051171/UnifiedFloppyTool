// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_uff.h
 * @brief UFF - UFT Universal Flux Format
 * @version 1.0.0
 * @date 2025-01-02
 *
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║              UFF - UFT UNIVERSAL FLUX FORMAT                                 ║
 * ║                   "Kein Bit geht verloren"                                   ║
 * ╠══════════════════════════════════════════════════════════════════════════════╣
 * ║ Das UFF Format ist das neue Standard-Format für verlustfreie Floppy-        ║
 * ║ Preservation. Es kombiniert die besten Features aller existierenden         ║
 * ║ Flux-Formate mit forensischen Fähigkeiten.                                  ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FEATURES:
 * =========
 * • Multi-Resolution Flux (10ns - 1µs konfigurierbar)
 * • Multi-Revolution mit Confidence-Scoring
 * • Weak-Bit-Maps mit Confidence-Level
 * • Splice-Point Markierung
 * • Per-Track SHA-256 Integrity Hashes
 * • Forensische Metadaten (Chain of Custody)
 * • JSON Extension Block für Erweiterbarkeit
 * • Optionale LZ4/ZSTD Kompression
 * • Bidirektionale Konvertierung zu/von allen Formaten
 *
 * FILE STRUCTURE:
 * ===============
 * ┌────────────────────────────────────────────┐
 * │ UFF Header (64 bytes)                      │
 * ├────────────────────────────────────────────┤
 * │ Track Index Table (variable)              │
 * ├────────────────────────────────────────────┤
 * │ Metadata Block (JSON, optional)           │
 * ├────────────────────────────────────────────┤
 * │ Track Data Blocks                         │
 * │   ├─ Track Header                         │
 * │   ├─ Revolution Data[]                    │
 * │   ├─ Weak Bit Map                         │
 * │   ├─ Splice Points                        │
 * │   └─ Track Hash                           │
 * ├────────────────────────────────────────────┤
 * │ Forensic Block (optional)                 │
 * ├────────────────────────────────────────────┤
 * │ File Footer + CRC64                       │
 * └────────────────────────────────────────────┘
 */

#ifndef UFT_UFF_H
#define UFT_UFF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * VERSION & MAGIC
 *============================================================================*/

#define UFF_MAGIC           "UFF\x00"
#define UFF_MAGIC_LEN       4
#define UFF_VERSION_MAJOR   1
#define UFF_VERSION_MINOR   0
#define UFF_VERSION         ((UFF_VERSION_MAJOR << 8) | UFF_VERSION_MINOR)

/*============================================================================
 * LIMITS
 *============================================================================*/

#define UFF_MAX_TRACKS          168     /* 84 cylinders × 2 heads */
#define UFF_MAX_REVOLUTIONS     16      /* Max revolutions per track */
#define UFF_MAX_FLUX_PER_REV    500000  /* Max flux transitions per revolution */
#define UFF_MAX_WEAK_BITS       65536   /* Max weak bit regions per track */
#define UFF_MAX_SPLICES         256     /* Max splice points per track */
#define UFF_MAX_METADATA_SIZE   (1024*1024)  /* 1MB JSON metadata */
#define UFF_MIN_TICK_NS         10      /* Minimum resolution: 10ns */
#define UFF_DEFAULT_TICK_NS     25      /* Default: 25ns (40MHz) */

/*============================================================================
 * FLAGS
 *============================================================================*/

/* File flags */
#define UFF_FLAG_COMPRESSED     0x0001  /* Track data is compressed */
#define UFF_FLAG_ENCRYPTED      0x0002  /* Track data is encrypted (reserved) */
#define UFF_FLAG_HAS_METADATA   0x0004  /* Has JSON metadata block */
#define UFF_FLAG_HAS_FORENSIC   0x0008  /* Has forensic block */
#define UFF_FLAG_MULTI_REV      0x0010  /* Multiple revolutions per track */
#define UFF_FLAG_HAS_WEAK_BITS  0x0020  /* Has weak bit information */
#define UFF_FLAG_HAS_SPLICES    0x0040  /* Has splice point markers */
#define UFF_FLAG_HAS_HASHES     0x0080  /* Has per-track hashes */
#define UFF_FLAG_WRITE_SPLICE   0x0100  /* Write splice marker stored */
#define UFF_FLAG_INDEX_ALIGNED  0x0200  /* Tracks aligned to index pulse */

/* Compression types */
#define UFF_COMPRESS_NONE       0x00
#define UFF_COMPRESS_LZ4        0x01
#define UFF_COMPRESS_ZSTD       0x02
#define UFF_COMPRESS_DELTA_LZ4  0x03    /* Delta encoding + LZ4 */

/* Track flags */
#define UFF_TRACK_VALID         0x01
#define UFF_TRACK_DAMAGED       0x02
#define UFF_TRACK_PROTECTED     0x04
#define UFF_TRACK_EMPTY         0x08
#define UFF_TRACK_HALF          0x10    /* Half-track */
#define UFF_TRACK_QUARTER       0x20    /* Quarter-track */

/* Encoding types */
#define UFF_ENCODING_UNKNOWN    0x00
#define UFF_ENCODING_FM         0x01
#define UFF_ENCODING_MFM        0x02
#define UFF_ENCODING_GCR_C64    0x03
#define UFF_ENCODING_GCR_APPLE  0x04
#define UFF_ENCODING_AMIGA      0x05
#define UFF_ENCODING_MIXED      0xFF

/* Source platform hints */
#define UFF_PLATFORM_UNKNOWN    0x00
#define UFF_PLATFORM_C64        0x01
#define UFF_PLATFORM_AMIGA      0x02
#define UFF_PLATFORM_ATARI_ST   0x03
#define UFF_PLATFORM_ATARI_8BIT 0x04
#define UFF_PLATFORM_APPLE_II   0x05
#define UFF_PLATFORM_APPLE_MAC  0x06
#define UFF_PLATFORM_IBM_PC     0x07
#define UFF_PLATFORM_TRS80      0x08
#define UFF_PLATFORM_BBC        0x09
#define UFF_PLATFORM_AMSTRAD    0x0A
#define UFF_PLATFORM_PC98       0x0B
#define UFF_PLATFORM_MSX        0x0C

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief UFF File Header (64 bytes)
 */
typedef struct {
    char        magic[4];           /* "UFF\0" */
    uint16_t    version;            /* Format version (major.minor) */
    uint16_t    flags;              /* File flags */
    
    /* Disk geometry */
    uint8_t     cylinders;          /* Number of cylinders (1-84) */
    uint8_t     heads;              /* Number of heads (1-2) */
    uint8_t     start_track;        /* First track number */
    uint8_t     end_track;          /* Last track number */
    
    /* Timing */
    uint16_t    tick_ns;            /* Tick resolution in nanoseconds */
    uint16_t    rpm;                /* Drive RPM (300 or 360) */
    
    /* Encoding hints */
    uint8_t     encoding;           /* Primary encoding type */
    uint8_t     platform;           /* Source platform hint */
    uint8_t     revolutions;        /* Revolutions per track */
    uint8_t     compression;        /* Compression type */
    
    /* Offsets */
    uint32_t    index_offset;       /* Offset to track index table */
    uint32_t    metadata_offset;    /* Offset to JSON metadata (0 if none) */
    uint32_t    forensic_offset;    /* Offset to forensic block (0 if none) */
    uint32_t    data_offset;        /* Offset to first track data */
    
    /* Integrity */
    uint64_t    file_size;          /* Total file size */
    uint32_t    track_count;        /* Total number of tracks stored */
    uint32_t    header_crc;         /* CRC32 of header (excluding this field) */
    
    /* Reserved for future use */
    uint8_t     reserved[8];
} uff_header_t;

/**
 * @brief Track Index Entry (24 bytes per track)
 */
typedef struct {
    uint8_t     cylinder;           /* Physical cylinder (0-83) */
    uint8_t     head;               /* Head (0-1) */
    uint8_t     flags;              /* Track flags */
    uint8_t     encoding;           /* Track encoding type */
    
    uint32_t    offset;             /* Offset to track data */
    uint32_t    compressed_size;    /* Compressed size (or raw if uncompressed) */
    uint32_t    uncompressed_size;  /* Uncompressed size */
    
    uint16_t    revolutions;        /* Number of revolutions stored */
    uint16_t    weak_regions;       /* Number of weak bit regions */
    
    uint32_t    crc32;              /* CRC32 of uncompressed track data */
} uff_track_index_t;

/**
 * @brief Track Data Header (32 bytes)
 */
typedef struct {
    char        magic[4];           /* "TRK\0" */
    uint8_t     cylinder;
    uint8_t     head;
    uint8_t     flags;
    uint8_t     encoding;
    
    uint32_t    revolution_count;   /* Number of revolutions */
    uint32_t    flux_count_total;   /* Total flux transitions */
    
    uint32_t    weak_map_offset;    /* Offset to weak bit map (from track start) */
    uint32_t    splice_offset;      /* Offset to splice data */
    uint32_t    hash_offset;        /* Offset to SHA-256 hash */
    
    uint8_t     reserved[4];
} uff_track_header_t;

/**
 * @brief Revolution Header (16 bytes per revolution)
 */
typedef struct {
    uint32_t    index_time;         /* Index-to-index time in ticks */
    uint32_t    flux_count;         /* Number of flux transitions */
    uint32_t    data_offset;        /* Offset to flux data (from revolution start) */
    uint8_t     confidence;         /* Overall confidence (0-100) */
    uint8_t     quality;            /* Quality score (0-255) */
    uint16_t    flags;              /* Revolution flags */
} uff_revolution_t;

/**
 * @brief Weak Bit Region (12 bytes per region)
 */
typedef struct {
    uint32_t    bit_offset;         /* Bit position in decoded track */
    uint16_t    bit_count;          /* Number of weak bits */
    uint8_t     confidence;         /* Confidence that these are truly weak (0-100) */
    uint8_t     pattern;            /* Detected pattern (0=random, 1-255=pattern) */
    uint32_t    flux_offset;        /* Corresponding flux position */
} uff_weak_region_t;

/**
 * @brief Splice Point (8 bytes per splice)
 */
typedef struct {
    uint32_t    bit_offset;         /* Bit position of splice */
    uint16_t    flags;              /* Splice type flags */
    uint8_t     confidence;         /* Confidence (0-100) */
    uint8_t     reserved;
} uff_splice_point_t;

/**
 * @brief Forensic Metadata Block Header
 */
typedef struct {
    char        magic[4];           /* "FOR\0" */
    uint32_t    size;               /* Total size of forensic block */
    
    /* Capture info */
    uint64_t    capture_timestamp;  /* Unix timestamp of capture */
    char        capture_device[32]; /* Device used (e.g., "Greaseweazle F7") */
    char        capture_software[32]; /* Software version */
    
    /* Source media */
    char        media_label[64];    /* Disk label if readable */
    char        media_serial[32];   /* Media serial if available */
    uint8_t     media_condition;    /* Physical condition (0-10) */
    uint8_t     write_protect;      /* Write protect state */
    uint16_t    reserved;
    
    /* Chain of custody */
    char        examiner[64];       /* Examiner name */
    char        case_number[32];    /* Case reference */
    char        notes[256];         /* Additional notes */
    
    /* Hashes */
    uint8_t     source_sha256[32];  /* SHA-256 of entire source media */
    uint8_t     content_sha256[32]; /* SHA-256 of decoded content */
    
    /* Reserved */
    uint8_t     reserved2[64];
} uff_forensic_t;

/**
 * @brief File Footer (16 bytes)
 */
typedef struct {
    char        magic[4];           /* "END\0" */
    uint32_t    track_count;        /* Verification: track count */
    uint64_t    file_crc64;         /* CRC64 of entire file (excluding footer) */
} uff_footer_t;

#pragma pack(pop)

/*============================================================================
 * UFF FILE CONTEXT
 *============================================================================*/

/**
 * @brief UFF File Handle
 */
typedef struct uff_file {
    /* File info */
    char*               path;
    void*               handle;         /* FILE* or memory mapped */
    bool                is_write;
    bool                is_memory;
    
    /* Header */
    uff_header_t        header;
    
    /* Track index */
    uff_track_index_t*  track_index;
    size_t              track_index_count;
    
    /* Metadata */
    char*               json_metadata;
    size_t              metadata_size;
    
    /* Forensic */
    uff_forensic_t*     forensic;
    
    /* Current state */
    int                 current_track;
    int                 error_code;
    char                error_msg[256];
    
    /* Statistics */
    uint64_t            bytes_read;
    uint64_t            bytes_written;
    uint32_t            tracks_processed;
    
} uff_file_t;

/**
 * @brief Track Data (decoded)
 */
typedef struct {
    uint8_t             cylinder;
    uint8_t             head;
    uint8_t             flags;
    uint8_t             encoding;
    
    /* Revolutions */
    uint32_t            revolution_count;
    uff_revolution_t*   revolutions;
    
    /* Flux data per revolution */
    uint32_t**          flux_data;          /* [rev][flux] */
    uint32_t*           flux_counts;        /* Per revolution */
    
    /* Fused/best flux */
    uint32_t*           fused_flux;
    uint32_t            fused_count;
    float*              fused_confidence;   /* Confidence per flux */
    
    /* Weak bits */
    uff_weak_region_t*  weak_regions;
    uint32_t            weak_count;
    
    /* Splices */
    uff_splice_point_t* splices;
    uint32_t            splice_count;
    
    /* Integrity */
    uint8_t             sha256[32];
    uint32_t            crc32;
    
} uff_track_data_t;

/*============================================================================
 * API - FILE OPERATIONS
 *============================================================================*/

/**
 * @brief Open UFF file for reading
 * @param path File path
 * @return File handle or NULL on error
 */
uff_file_t* uff_open(const char* path);

/**
 * @brief Create new UFF file for writing
 * @param path File path
 * @param cylinders Number of cylinders
 * @param heads Number of heads
 * @param tick_ns Tick resolution (0 for default 25ns)
 * @return File handle or NULL on error
 */
uff_file_t* uff_create(const char* path, uint8_t cylinders, uint8_t heads, 
                       uint16_t tick_ns);

/**
 * @brief Close UFF file
 * @param uff File handle
 */
void uff_close(uff_file_t* uff);

/**
 * @brief Get last error message
 * @param uff File handle
 * @return Error message
 */
const char* uff_get_error(uff_file_t* uff);

/*============================================================================
 * API - TRACK OPERATIONS
 *============================================================================*/

/**
 * @brief Read track data
 * @param uff File handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param track Output track data (caller allocates)
 * @return 0 on success, error code on failure
 */
int uff_read_track(uff_file_t* uff, uint8_t cylinder, uint8_t head,
                   uff_track_data_t* track);

/**
 * @brief Write track data
 * @param uff File handle
 * @param track Track data to write
 * @return 0 on success, error code on failure
 */
int uff_write_track(uff_file_t* uff, const uff_track_data_t* track);

/**
 * @brief Free track data
 * @param track Track data to free
 */
void uff_free_track(uff_track_data_t* track);

/**
 * @brief Get track info without reading full data
 * @param uff File handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param index Output index entry
 * @return 0 on success
 */
int uff_get_track_info(uff_file_t* uff, uint8_t cylinder, uint8_t head,
                       uff_track_index_t* index);

/*============================================================================
 * API - METADATA
 *============================================================================*/

/**
 * @brief Set JSON metadata
 * @param uff File handle
 * @param json JSON string
 * @return 0 on success
 */
int uff_set_metadata(uff_file_t* uff, const char* json);

/**
 * @brief Get JSON metadata
 * @param uff File handle
 * @return JSON string or NULL
 */
const char* uff_get_metadata(uff_file_t* uff);

/**
 * @brief Set forensic information
 * @param uff File handle
 * @param forensic Forensic data
 * @return 0 on success
 */
int uff_set_forensic(uff_file_t* uff, const uff_forensic_t* forensic);

/**
 * @brief Get forensic information
 * @param uff File handle
 * @return Forensic data or NULL
 */
const uff_forensic_t* uff_get_forensic(uff_file_t* uff);

/*============================================================================
 * API - FLUX OPERATIONS
 *============================================================================*/

/**
 * @brief Fuse multiple revolutions using confidence weighting
 * @param track Track with multiple revolutions
 * @return 0 on success
 */
int uff_fuse_revolutions(uff_track_data_t* track);

/**
 * @brief Detect weak bits by comparing revolutions
 * @param track Track with multiple revolutions
 * @return Number of weak regions detected
 */
int uff_detect_weak_bits(uff_track_data_t* track);

/**
 * @brief Detect splice points in track
 * @param track Track data
 * @return Number of splice points detected
 */
int uff_detect_splices(uff_track_data_t* track);

/**
 * @brief Calculate SHA-256 hash of track
 * @param track Track data
 * @return 0 on success
 */
int uff_hash_track(uff_track_data_t* track);

/*============================================================================
 * API - CONVERSION
 *============================================================================*/

/**
 * @brief Import from SCP file
 * @param uff Target UFF file
 * @param scp_path Source SCP path
 * @return 0 on success
 */
int uff_import_scp(uff_file_t* uff, const char* scp_path);

/**
 * @brief Import from HFE file
 * @param uff Target UFF file
 * @param hfe_path Source HFE path
 * @return 0 on success
 */
int uff_import_hfe(uff_file_t* uff, const char* hfe_path);

/**
 * @brief Import from G64 file
 * @param uff Target UFF file
 * @param g64_path Source G64 path
 * @return 0 on success
 */
int uff_import_g64(uff_file_t* uff, const char* g64_path);

/**
 * @brief Import from KryoFlux stream
 * @param uff Target UFF file
 * @param kf_dir Source KryoFlux directory
 * @return 0 on success
 */
int uff_import_kryoflux(uff_file_t* uff, const char* kf_dir);

/**
 * @brief Export to SCP file
 * @param uff Source UFF file
 * @param scp_path Target SCP path
 * @param revolutions Number of revolutions to export (0 = all)
 * @return 0 on success
 */
int uff_export_scp(uff_file_t* uff, const char* scp_path, int revolutions);

/**
 * @brief Export to HFE file
 * @param uff Source UFF file
 * @param hfe_path Target HFE path
 * @param version HFE version (1, 2, or 3)
 * @return 0 on success
 */
int uff_export_hfe(uff_file_t* uff, const char* hfe_path, int version);

/**
 * @brief Export decoded data to sector image
 * @param uff Source UFF file
 * @param path Target path
 * @param format Target format ("d64", "adf", "img", etc.)
 * @return 0 on success
 */
int uff_export_decoded(uff_file_t* uff, const char* path, const char* format);

/*============================================================================
 * API - UTILITIES
 *============================================================================*/

/**
 * @brief Verify UFF file integrity
 * @param uff File handle
 * @param verbose Print details
 * @return 0 if valid, error code otherwise
 */
int uff_verify(uff_file_t* uff, bool verbose);

/**
 * @brief Get format statistics
 * @param uff File handle
 * @param stats Output buffer (at least 1024 bytes)
 * @return 0 on success
 */
int uff_get_stats(uff_file_t* uff, char* stats);

/**
 * @brief Get human-readable format info
 * @param uff File handle
 * @param info Output buffer (at least 2048 bytes)
 * @return 0 on success
 */
int uff_get_info(uff_file_t* uff, char* info);

/*============================================================================
 * ERROR CODES
 *============================================================================*/

#define UFF_OK                  0
#define UFF_ERR_FILE            -1      /* File I/O error */
#define UFF_ERR_MAGIC           -2      /* Invalid magic */
#define UFF_ERR_VERSION         -3      /* Unsupported version */
#define UFF_ERR_CORRUPT         -4      /* Data corruption detected */
#define UFF_ERR_MEMORY          -5      /* Memory allocation failed */
#define UFF_ERR_PARAM           -6      /* Invalid parameter */
#define UFF_ERR_NO_TRACK        -7      /* Track not found */
#define UFF_ERR_COMPRESS        -8      /* Compression error */
#define UFF_ERR_HASH            -9      /* Hash mismatch */
#define UFF_ERR_UNSUPPORTED     -10     /* Unsupported feature */

/*============================================================================
 * PLATFORM NAMES (for GUI)
 *============================================================================*/

static const char* UFF_PLATFORM_NAMES[] = {
    "Unknown",
    "Commodore 64/128",
    "Amiga",
    "Atari ST",
    "Atari 8-bit",
    "Apple II",
    "Macintosh",
    "IBM PC",
    "TRS-80",
    "BBC Micro",
    "Amstrad CPC",
    "PC-98",
    "MSX"
};

static const char* UFF_ENCODING_NAMES[] = {
    "Unknown",
    "FM",
    "MFM",
    "GCR (C64)",
    "GCR (Apple)",
    "Amiga MFM",
    "Mixed"
};

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFF_H */
