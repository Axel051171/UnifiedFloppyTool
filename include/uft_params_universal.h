/**
 * @file uft_params_universal.h
 * @brief GOD MODE Universal Floppy Parameter System v2
 * 
 * Applies the a8rawconv parameter model to ALL floppy disk formats.
 * This creates a unified interface for:
 * - Reading/Writing any format
 * - Flux-level analysis
 * - Multi-revolution handling
 * - Recovery strategies
 * - Format conversion
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#ifndef UFT_PARAMS_UNIVERSAL_H
#define UFT_PARAMS_UNIVERSAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * UNIVERSAL CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_MAX_TRACKS          168     /* 84 tracks * 2 sides */
#define UFT_MAX_SECTORS         64      /* Max sectors per track */
#define UFT_MAX_REVOLUTIONS     16      /* Max revolutions to store */
#define UFT_MAX_SIDES           2
#define UFT_MAX_FILENAME        1024
#define UFT_MAX_FLUX_TRANSITIONS 500000

/* ═══════════════════════════════════════════════════════════════════════════
 * ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Disk platform/system types
 */
typedef enum {
    UFT_PLATFORM_UNKNOWN = 0,
    
    /* 8-bit computers */
    UFT_PLATFORM_COMMODORE_64,
    UFT_PLATFORM_COMMODORE_128,
    UFT_PLATFORM_COMMODORE_VIC20,
    UFT_PLATFORM_COMMODORE_PET,
    UFT_PLATFORM_COMMODORE_PLUS4,
    UFT_PLATFORM_ATARI_8BIT,
    UFT_PLATFORM_APPLE_II,
    UFT_PLATFORM_APPLE_III,
    UFT_PLATFORM_BBC_MICRO,
    UFT_PLATFORM_ZX_SPECTRUM,
    UFT_PLATFORM_AMSTRAD_CPC,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_TRS80,
    UFT_PLATFORM_ORIC,
    UFT_PLATFORM_THOMSON,
    UFT_PLATFORM_TI99,
    UFT_PLATFORM_DRAGON,
    UFT_PLATFORM_SAM_COUPE,
    
    /* 16/32-bit computers */
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_MACINTOSH,
    UFT_PLATFORM_PC,
    UFT_PLATFORM_PC98,
    UFT_PLATFORM_X68000,
    UFT_PLATFORM_FM_TOWNS,
    
    /* Consoles */
    UFT_PLATFORM_FAMICOM_DISK,
    
    /* Other */
    UFT_PLATFORM_GENERIC,
    
    UFT_PLATFORM_COUNT
} uft_platform_t;

/**
 * @brief Data encoding types
 */
typedef enum {
    UFT_ENCODING_UNKNOWN = 0,
    UFT_ENCODING_FM,              /* Single density */
    UFT_ENCODING_MFM,             /* Double density */
    UFT_ENCODING_GCR_COMMODORE,   /* Commodore GCR */
    UFT_ENCODING_GCR_APPLE,       /* Apple GCR (6&2, 5&3) */
    UFT_ENCODING_GCR_VICTOR,      /* Victor 9000 GCR */
    UFT_ENCODING_M2FM,            /* Modified MFM (Intel) */
    UFT_ENCODING_RLL,             /* Run Length Limited */
    UFT_ENCODING_RAW_FLUX,        /* Raw flux transitions */
    UFT_ENCODING_COUNT
} uft_encoding_t;

/**
 * @brief Disk geometry types
 */
typedef enum {
    UFT_GEOMETRY_UNKNOWN = 0,
    UFT_GEOMETRY_FIXED,           /* Fixed sectors per track */
    UFT_GEOMETRY_VARIABLE,        /* Variable (zone-based) */
    UFT_GEOMETRY_FLUX,            /* Raw flux, no geometry */
    UFT_GEOMETRY_COUNT
} uft_geometry_type_t;

/**
 * @brief Revolution selection modes
 */
typedef enum {
    UFT_REV_SELECT_FIRST = 0,     /* Use first revolution */
    UFT_REV_SELECT_BEST,          /* Use best quality revolution */
    UFT_REV_SELECT_VOTING,        /* Bit-voting across revolutions */
    UFT_REV_SELECT_MERGE,         /* Merge all revolutions */
    UFT_REV_SELECT_ALL,           /* Process all separately */
    UFT_REV_SELECT_COUNT
} uft_rev_select_t;

/**
 * @brief Recovery aggressiveness levels
 */
typedef enum {
    UFT_RECOVERY_NONE = 0,        /* No recovery, strict mode */
    UFT_RECOVERY_MINIMAL,         /* Basic error correction */
    UFT_RECOVERY_STANDARD,        /* Standard recovery */
    UFT_RECOVERY_AGGRESSIVE,      /* Try everything */
    UFT_RECOVERY_FORENSIC,        /* Preserve errors for analysis */
    UFT_RECOVERY_COUNT
} uft_recovery_level_t;

/**
 * @brief Sector status flags
 */
typedef enum {
    UFT_SECTOR_OK           = 0x00,
    UFT_SECTOR_CRC_ERROR    = 0x01,
    UFT_SECTOR_MISSING      = 0x02,
    UFT_SECTOR_DELETED      = 0x04,
    UFT_SECTOR_WEAK         = 0x08,
    UFT_SECTOR_DUPLICATE    = 0x10,
    UFT_SECTOR_ID_CRC_ERROR = 0x20,
    UFT_SECTOR_NO_DAM       = 0x40,
    UFT_SECTOR_RECOVERED    = 0x80
} uft_sector_status_t;

/**
 * @brief Track status flags
 */
typedef enum {
    UFT_TRACK_OK            = 0x00,
    UFT_TRACK_UNFORMATTED   = 0x01,
    UFT_TRACK_PROTECTED     = 0x02,
    UFT_TRACK_WEAK_BITS     = 0x04,
    UFT_TRACK_TIMING_DATA   = 0x08,
    UFT_TRACK_FUZZY         = 0x10,
    UFT_TRACK_NONSTANDARD   = 0x20
} uft_track_status_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * FILE/IMAGE PARAMETERS (Pflicht)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Image file parameters
 */
typedef struct {
    char path[UFT_MAX_FILENAME];      /* Input/output file path */
    char format[32];                   /* Format name (auto-detected or specified) */
    size_t file_size;                  /* File size in bytes */
    uint32_t checksum;                 /* File checksum (CRC32) */
    bool read_only;                    /* Open read-only */
    bool extended_mode;                /* Extended header mode (SCP, etc.) */
} uft_file_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HEADER METADATA (Read-Only from file)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Universal header metadata
 * Populated from format-specific headers
 */
typedef struct {
    /* Format identification */
    char signature[16];               /* Format signature/magic */
    uint8_t version_major;            /* Format version major */
    uint8_t version_minor;            /* Format version minor */
    char creator[64];                 /* Creator tool name */
    
    /* Disk type info */
    uft_platform_t platform;          /* Target platform */
    uft_encoding_t encoding;          /* Data encoding */
    uint8_t disk_type;                /* Format-specific disk type */
    
    /* Geometry */
    uint8_t num_tracks;               /* Number of tracks */
    uint8_t num_sides;                /* Number of sides */
    uint8_t start_track;              /* First track number */
    uint8_t end_track;                /* Last track number */
    uint8_t sectors_per_track;        /* Sectors per track (if fixed) */
    uint16_t sector_size;             /* Bytes per sector (if fixed) */
    
    /* Flux-specific */
    uint8_t num_revolutions;          /* Revolutions per track */
    uint8_t bitcell_encoding;         /* Bitcell encoding type */
    uint32_t bit_rate;                /* Nominal bit rate (bps) */
    uint32_t sample_rate;             /* Flux sample rate (Hz) */
    
    /* Flags */
    uint8_t flags;                    /* Format-specific flags */
    bool write_protected;             /* Write protection status */
    bool single_sided;                /* Single-sided disk */
    bool double_step;                 /* Double-step drive required */
    
    /* Integrity */
    uint32_t header_checksum;         /* Header checksum */
    bool checksum_valid;              /* Checksum verification result */
    
    /* Optional metadata */
    char disk_name[64];               /* Disk label/name */
    char comment[256];                /* User comment */
    uint32_t creation_time;           /* Creation timestamp */
} uft_header_metadata_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track location/addressing
 */
typedef struct {
    uint8_t track_number;             /* Physical track (0-83+) */
    uint8_t side;                     /* Side (0-1) */
    uint8_t track_index;              /* Linear index (0-167) */
    uint32_t file_offset;             /* Absolute file offset */
    bool present;                     /* Track exists in image */
} uft_track_location_t;

/**
 * @brief Track geometry (per-track, for variable formats)
 */
typedef struct {
    uint8_t sectors;                  /* Sectors on this track */
    uint16_t sector_size;             /* Sector size (bytes) */
    uint16_t track_length;            /* Raw track length (bytes) */
    uint32_t bit_length;              /* Track length in bits */
    uint8_t interleave;               /* Sector interleave */
    uint8_t skew;                     /* Track skew */
    uint8_t gap3_length;              /* Gap 3 length */
    uft_encoding_t encoding;          /* Encoding for this track */
} uft_track_geometry_t;

/**
 * @brief Track timing data
 */
typedef struct {
    uint32_t rotation_time_ns;        /* Track rotation time (ns) */
    uint32_t bit_rate;                /* Effective bit rate */
    uint32_t write_splice;            /* Write splice position */
    bool has_timing;                  /* Per-byte timing available */
    uint16_t* byte_timing;            /* Per-byte timing (if available) */
    size_t timing_count;              /* Number of timing entries */
} uft_track_timing_t;

/**
 * @brief Revolution data (for flux formats)
 */
typedef struct {
    uint8_t rev_index;                /* Revolution index (0..N-1) */
    uint32_t time_duration;           /* Duration in sample units */
    uint32_t data_length;             /* Number of flux values */
    uint32_t data_offset;             /* Offset within track block */
    uint32_t index_time;              /* Time of index pulse */
    float quality_score;              /* Quality metric (0-1) */
    bool valid;                       /* Revolution is valid */
} uft_revolution_t;

/**
 * @brief Complete track parameters
 */
typedef struct {
    uft_track_location_t location;
    uft_track_geometry_t geometry;
    uft_track_timing_t timing;
    uft_track_status_t status;
    
    /* Revolutions (flux formats) */
    uint8_t num_revolutions;
    uft_revolution_t revolutions[UFT_MAX_REVOLUTIONS];
    
    /* Protection info */
    bool has_weak_bits;
    bool has_fuzzy_mask;
    uint8_t* fuzzy_mask;
    size_t fuzzy_mask_size;
    
    /* Raw data */
    uint8_t* raw_data;
    size_t raw_data_size;
} uft_track_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector ID field (IDAM)
 */
typedef struct {
    uint8_t track;                    /* Track from ID field */
    uint8_t side;                     /* Side from ID field */
    uint8_t sector;                   /* Sector number */
    uint8_t size_code;                /* Size code (0=128, 1=256, ...) */
    uint16_t crc;                     /* ID field CRC */
    bool crc_valid;                   /* CRC verification result */
} uft_sector_id_t;

/**
 * @brief Sector data parameters
 */
typedef struct {
    uft_sector_id_t id;               /* Sector ID */
    uft_sector_status_t status;       /* Sector status */
    
    /* Position */
    uint32_t bit_position;            /* Position in track (bits) */
    uint32_t byte_offset;             /* Offset in track data */
    uint16_t read_time;               /* Time to read sector */
    
    /* Data */
    uint8_t* data;                    /* Sector data */
    size_t data_size;                 /* Actual data size */
    uint16_t data_crc;                /* Data CRC */
    bool data_crc_valid;              /* Data CRC verification */
    
    /* Data Address Mark */
    uint8_t dam_type;                 /* DAM type (normal/deleted) */
    bool has_dam;                     /* DAM present */
    
    /* FDC status (for formats that preserve it) */
    uint8_t fdc_status;               /* FDC status register */
    
    /* Duplicate tracking */
    uint8_t copy_number;              /* Copy number if duplicate */
    uint8_t total_copies;             /* Total copies found */
} uft_sector_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX OUTPUT PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Flux transition data
 */
typedef struct {
    uint32_t* transitions;            /* Cumulative flux times */
    size_t count;                     /* Number of transitions */
    size_t capacity;                  /* Allocated capacity */
    uint32_t total_time;              /* Total time span */
    uint32_t sample_rate;             /* Sample rate (Hz) */
    
    /* Statistics */
    uint32_t min_interval;            /* Shortest interval */
    uint32_t max_interval;            /* Longest interval */
    double mean_interval;             /* Mean interval */
    double stddev_interval;           /* Standard deviation */
} uft_flux_data_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * CLI/GUI PARAMETERS (User-settable)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Input/Output parameters
 */
typedef struct {
    char input_file[UFT_MAX_FILENAME];
    char output_file[UFT_MAX_FILENAME];
    char format_override[32];         /* Force specific format */
    bool auto_detect;                 /* Auto-detect format */
} uft_io_params_t;

/**
 * @brief Analysis/Display parameters
 */
typedef struct {
    bool show_summary;                /* Show header + track overview */
    bool show_catalog;                /* Show file catalog (if applicable) */
    bool show_sectors;                /* Show sector details */
    bool show_flux;                   /* Show flux statistics */
    bool verbose;                     /* Verbose output */
    bool quiet;                       /* Suppress non-essential output */
    char catalog_output[UFT_MAX_FILENAME]; /* JSON catalog output */
} uft_analysis_params_t;

/**
 * @brief Flux dump parameters
 */
typedef struct {
    int track;                        /* Track to dump (-1 = all) */
    int side;                         /* Side to dump (-1 = all) */
    int revolution;                   /* Revolution to dump (-1 = best) */
    char output_file[UFT_MAX_FILENAME]; /* Output file (CSV/binary) */
    size_t max_transitions;           /* Safety limit */
    bool binary_output;               /* Binary instead of CSV */
    bool include_timing;              /* Include timing data */
} uft_flux_dump_params_t;

/**
 * @brief Recovery parameters (strategic)
 */
typedef struct {
    uft_rev_select_t rev_selection;   /* Revolution selection mode */
    uft_recovery_level_t level;       /* Recovery aggressiveness */
    
    /* Multi-revolution handling */
    bool merge_revolutions;           /* Merge data from multiple revs */
    uint8_t max_revs_to_use;          /* Max revolutions to process */
    bool ignore_short_revs;           /* Skip damaged/short revolutions */
    
    /* PLL/Decoding */
    bool normalize_timebase;          /* Normalize PLL timing */
    float pll_bandwidth;              /* PLL bandwidth (0.0-1.0) */
    uint32_t bitcell_tolerance;       /* Bitcell timing tolerance (%) */
    
    /* CRC handling */
    bool allow_crc_errors;            /* Accept sectors with CRC errors */
    bool attempt_crc_recovery;        /* Try to correct CRC errors */
    uint8_t max_correction_bits;      /* Max bits to try correcting */
    
    /* Weak bits */
    bool detect_weak_bits;            /* Detect weak/fuzzy bits */
    uint8_t weak_bit_threshold;       /* Threshold for weak detection */
    
    /* Scoring weights for best-rev selection */
    float score_crc_weight;           /* Weight for CRC validity */
    float score_timing_weight;        /* Weight for timing consistency */
    float score_complete_weight;      /* Weight for sector completeness */
} uft_recovery_params_t;

/**
 * @brief Conversion parameters
 */
typedef struct {
    char target_format[32];           /* Target format name */
    bool preserve_errors;             /* Keep error sectors as-is */
    bool preserve_timing;             /* Keep timing information */
    bool preserve_protection;         /* Keep copy protection */
    bool fill_missing;                /* Fill missing sectors with pattern */
    uint8_t fill_byte;                /* Fill pattern */
    
    /* Geometry override */
    bool override_geometry;
    uint8_t target_tracks;
    uint8_t target_sides;
    uint8_t target_sectors;
    uint16_t target_sector_size;
} uft_conversion_params_t;

/**
 * @brief Verification parameters
 */
typedef struct {
    bool verify_checksums;            /* Verify all checksums */
    bool verify_structure;            /* Verify format structure */
    bool verify_filesystem;           /* Verify filesystem integrity */
    bool hash_output;                 /* Generate hash report */
    char hash_algorithm[16];          /* "MD5", "SHA1", "SHA256" */
} uft_verify_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MASTER PARAMETER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Complete parameter set for any operation
 */
typedef struct {
    /* File parameters */
    uft_file_params_t file;
    
    /* Metadata (read from file) */
    uft_header_metadata_t metadata;
    
    /* User-settable parameters */
    uft_io_params_t io;
    uft_analysis_params_t analysis;
    uft_flux_dump_params_t flux_dump;
    uft_recovery_params_t recovery;
    uft_conversion_params_t conversion;
    uft_verify_params_t verify;
    
    /* Operation mode */
    enum {
        UFT_OP_READ,
        UFT_OP_WRITE,
        UFT_OP_CONVERT,
        UFT_OP_ANALYZE,
        UFT_OP_VERIFY,
        UFT_OP_REPAIR
    } operation;
    
    /* Status */
    bool initialized;
    char error[256];
} uft_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * PARAMETER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize parameters with defaults
 */
void uft_params_init(uft_params_t* params);

/**
 * @brief Set defaults for specific platform
 */
void uft_params_set_platform_defaults(uft_params_t* params, uft_platform_t platform);

/**
 * @brief Parse command line arguments
 */
bool uft_params_parse_cli(uft_params_t* params, int argc, char** argv);

/**
 * @brief Load parameters from JSON config
 */
bool uft_params_load_json(uft_params_t* params, const char* json_path);

/**
 * @brief Save parameters to JSON config
 */
bool uft_params_save_json(const uft_params_t* params, const char* json_path);

/**
 * @brief Validate parameter consistency
 */
bool uft_params_validate(const uft_params_t* params, char* error, size_t error_size);

/**
 * @brief Get human-readable platform name
 */
const char* uft_platform_name(uft_platform_t platform);

/**
 * @brief Get human-readable encoding name
 */
const char* uft_encoding_name(uft_encoding_t encoding);

/**
 * @brief Get human-readable recovery level name
 */
const char* uft_recovery_level_name(uft_recovery_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARAMS_UNIVERSAL_H */
