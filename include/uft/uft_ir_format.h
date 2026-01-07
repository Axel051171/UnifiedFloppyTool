/**
 * @file uft_ir_format.h
 * @brief UFT Intermediate Representation (UFT-IR) Format Specification v1.0
 * 
 * The UFT-IR format is the canonical hub format for raw track data in the
 * UnifiedFloppyTool. It serves as the central interchange format between:
 * - Disk image formats (HFE, WOZ, SCP, IPF, etc.)
 * - Analysis/decode pipelines
 * - Archive/cache storage
 * 
 * Design Goals:
 * - Lossless preservation of all flux timing data
 * - Multi-revolution support (up to 16 revolutions)
 * - Comprehensive metadata (timing, quality, forensics)
 * - Efficient serialization for persistence/networking
 * - Platform-independent (little-endian, packed structures)
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_IR_FORMAT_H
#define UFT_IR_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VERSION & MAGIC
 * ═══════════════════════════════════════════════════════════════════════════ */

/** UFT-IR file magic: "UFTIR\x00\x01\x00" (8 bytes) */
#define UFT_IR_MAGIC             0x0001005249545655ULL  /* "UFTIR\0\1\0" LE */
#define UFT_IR_MAGIC_BYTES       "UFTIR\x00\x01\x00"

/** Format version (major.minor.patch encoded as 0xMMmmpp) */
#define UFT_IR_VERSION_MAJOR     1
#define UFT_IR_VERSION_MINOR     0
#define UFT_IR_VERSION_PATCH     0
#define UFT_IR_VERSION           ((UFT_IR_VERSION_MAJOR << 16) | \
                                  (UFT_IR_VERSION_MINOR << 8) | \
                                  UFT_IR_VERSION_PATCH)

/* ═══════════════════════════════════════════════════════════════════════════
 * LIMITS & CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_IR_MAX_REVOLUTIONS       16      /**< Max revolutions per track */
#define UFT_IR_MAX_TRACKS           168      /**< Max tracks (84 cylinders × 2 heads) */
#define UFT_IR_MAX_CYLINDERS         84      /**< Max cylinders */
#define UFT_IR_MAX_HEADS              2      /**< Max heads (sides) */
#define UFT_IR_MAX_FLUX_PER_REV  500000      /**< Max flux transitions per revolution */
#define UFT_IR_MAX_METADATA_SIZE   4096      /**< Max custom metadata size */
#define UFT_IR_MAX_COMMENT_LEN      256      /**< Max comment string length */
#define UFT_IR_MAX_SOURCE_LEN       128      /**< Max source identifier length */
#define UFT_IR_MAX_WEAK_REGIONS      32      /**< Max weak bit regions per track */
#define UFT_IR_MAX_PROTECTIONS        8      /**< Max protection markers per track */

/** Timing resolution: nanoseconds (standard) or sample ticks (raw) */
#define UFT_IR_TIMING_NS             1       /**< Nanosecond resolution */
#define UFT_IR_TIMING_TICKS          2       /**< Raw sample ticks */

/* ═══════════════════════════════════════════════════════════════════════════
 * ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Data representation type
 */
typedef enum uft_ir_data_type {
    UFT_IR_DATA_NONE          = 0,   /**< No data present */
    UFT_IR_DATA_FLUX_DELTA    = 1,   /**< Flux deltas (time between transitions) */
    UFT_IR_DATA_FLUX_ABSOLUTE = 2,   /**< Absolute flux timestamps */
    UFT_IR_DATA_BITSTREAM     = 3,   /**< Decoded bitstream */
    UFT_IR_DATA_BYTESTREAM    = 4,   /**< Decoded byte stream */
    UFT_IR_DATA_MFM_DECODED   = 5,   /**< MFM-decoded data */
    UFT_IR_DATA_GCR_DECODED   = 6,   /**< GCR-decoded data */
    UFT_IR_DATA_FM_DECODED    = 7,   /**< FM-decoded data */
    UFT_IR_DATA_RAW_SAMPLES   = 8,   /**< Raw ADC samples (analog capture) */
} uft_ir_data_type_t;

/**
 * @brief Encoding type detected/expected
 */
typedef enum uft_ir_encoding {
    UFT_IR_ENC_UNKNOWN        = 0,   /**< Unknown/undetected */
    UFT_IR_ENC_FM             = 1,   /**< FM (Single Density) */
    UFT_IR_ENC_MFM            = 2,   /**< MFM (Double Density) */
    UFT_IR_ENC_M2FM           = 3,   /**< M2FM (Modified MFM) */
    UFT_IR_ENC_GCR_COMMODORE  = 4,   /**< GCR Commodore (C64/1541) */
    UFT_IR_ENC_GCR_APPLE      = 5,   /**< GCR Apple II */
    UFT_IR_ENC_GCR_APPLE_35   = 6,   /**< GCR Apple 3.5" */
    UFT_IR_ENC_GCR_VICTOR     = 7,   /**< GCR Victor 9000 */
    UFT_IR_ENC_AMIGA_MFM      = 8,   /**< Amiga-style MFM */
    UFT_IR_ENC_RLL            = 9,   /**< RLL encoding */
    UFT_IR_ENC_MIXED          = 10,  /**< Mixed encodings on track */
    UFT_IR_ENC_CUSTOM         = 255, /**< Custom/proprietary */
} uft_ir_encoding_t;

/**
 * @brief Track quality assessment
 */
typedef enum uft_ir_quality {
    UFT_IR_QUALITY_UNKNOWN    = 0,   /**< Not assessed */
    UFT_IR_QUALITY_PERFECT    = 1,   /**< All sectors OK, no errors */
    UFT_IR_QUALITY_GOOD       = 2,   /**< Minor issues, all data recovered */
    UFT_IR_QUALITY_DEGRADED   = 3,   /**< Some sectors with corrections */
    UFT_IR_QUALITY_MARGINAL   = 4,   /**< Significant issues, data uncertain */
    UFT_IR_QUALITY_BAD        = 5,   /**< Major errors, incomplete recovery */
    UFT_IR_QUALITY_UNREADABLE = 6,   /**< Cannot decode track */
    UFT_IR_QUALITY_EMPTY      = 7,   /**< No flux detected (unformatted) */
    UFT_IR_QUALITY_PROTECTED  = 8,   /**< Copy protection detected */
} uft_ir_quality_t;

/* Short aliases for quality levels (backward compatibility) */
#define UFT_IR_QUAL_PERFECT   UFT_IR_QUALITY_PERFECT
#define UFT_IR_QUAL_GOOD      UFT_IR_QUALITY_GOOD
#define UFT_IR_QUAL_DEGRADED  UFT_IR_QUALITY_DEGRADED
#define UFT_IR_QUAL_MARGINAL  UFT_IR_QUALITY_MARGINAL
#define UFT_IR_QUAL_BAD       UFT_IR_QUALITY_BAD

/**
 * @brief Weak bit pattern classification
 */
typedef enum uft_ir_weak_pattern {
    UFT_IR_WEAK_RANDOM    = 0,   /**< Truly random (no flux) */
    UFT_IR_WEAK_STUCK_0   = 1,   /**< Stuck at 0 / biased toward 0 */
    UFT_IR_WEAK_STUCK_1   = 2,   /**< Stuck at 1 / biased toward 1 */
    UFT_IR_WEAK_PERIODIC  = 3,   /**< Periodic pattern */
    UFT_IR_WEAK_DEGRADED  = 4,   /**< Media degradation */
} uft_ir_weak_pattern_t;

/**
 * @brief Hardware source type
 */
typedef enum uft_ir_source {
    UFT_IR_SRC_UNKNOWN        = 0,   /**< Unknown source */
    UFT_IR_SRC_GREASEWEAZLE   = 1,   /**< Greaseweazle */
    UFT_IR_SRC_FLUXENGINE     = 2,   /**< FluxEngine */
    UFT_IR_SRC_KRYOFLUX       = 3,   /**< KryoFlux */
    UFT_IR_SRC_FC5025         = 4,   /**< FC5025 */
    UFT_IR_SRC_XUM1541        = 5,   /**< XUM1541 */
    UFT_IR_SRC_SUPERCARD_PRO  = 6,   /**< SuperCard Pro */
    UFT_IR_SRC_PAULINE        = 7,   /**< Pauline */
    UFT_IR_SRC_APPLESAUCE     = 8,   /**< Applesauce */
    UFT_IR_SRC_CONVERTED      = 100, /**< Converted from image file */
    UFT_IR_SRC_SYNTHETIC      = 101, /**< Synthetically generated */
    UFT_IR_SRC_EMULATOR       = 102, /**< From emulator */
} uft_ir_source_t;

/**
 * @brief Compression type for serialized data
 */
typedef enum uft_ir_compression {
    UFT_IR_COMP_NONE          = 0,   /**< No compression */
    UFT_IR_COMP_ZLIB          = 1,   /**< zlib/deflate */
    UFT_IR_COMP_LZ4           = 2,   /**< LZ4 */
    UFT_IR_COMP_ZSTD          = 3,   /**< Zstandard */
    UFT_IR_COMP_RLE           = 4,   /**< Simple RLE */
    UFT_IR_COMP_DELTA         = 5,   /**< Delta encoding */
} uft_ir_compression_t;

/**
 * @brief Track flags (bitfield)
 */
typedef enum uft_ir_track_flags {
    UFT_IR_TF_NONE            = 0,
    UFT_IR_TF_INDEXED         = (1 << 0),  /**< Index-aligned revolutions */
    UFT_IR_TF_WEAK_BITS       = (1 << 1),  /**< Contains weak/random bits */
    UFT_IR_TF_PROTECTED       = (1 << 2),  /**< Copy protection detected */
    UFT_IR_TF_LONG_TRACK      = (1 << 3),  /**< Longer than standard */
    UFT_IR_TF_SHORT_TRACK     = (1 << 4),  /**< Shorter than standard */
    UFT_IR_TF_DENSITY_VARIED  = (1 << 5),  /**< Variable density zones */
    UFT_IR_TF_HALF_TRACK      = (1 << 6),  /**< Half-track position */
    UFT_IR_TF_QUARTER_TRACK   = (1 << 7),  /**< Quarter-track position */
    UFT_IR_TF_MULTI_REV_FUSED = (1 << 8),  /**< Multi-rev fusion applied */
    UFT_IR_TF_CRC_CORRECTED   = (1 << 9),  /**< CRC corrections applied */
    UFT_IR_TF_SYNTHESIZED     = (1 << 10), /**< Partially synthesized */
    UFT_IR_TF_INCOMPLETE      = (1 << 11), /**< Incomplete read */
    UFT_IR_TF_VERIFIED        = (1 << 12), /**< Verified against source */
} uft_ir_track_flags_t;

/**
 * @brief Revolution flags (bitfield)
 */
typedef enum uft_ir_rev_flags {
    UFT_IR_RF_NONE            = 0,
    UFT_IR_RF_INDEX_START     = (1 << 0),  /**< Starts at index pulse */
    UFT_IR_RF_INDEX_END       = (1 << 1),  /**< Ends at index pulse */
    UFT_IR_RF_COMPLETE        = (1 << 2),  /**< Full revolution captured */
    UFT_IR_RF_OVERFLOW        = (1 << 3),  /**< Buffer overflow occurred */
    UFT_IR_RF_BEST_QUALITY    = (1 << 4),  /**< Best quality revolution */
    UFT_IR_RF_REFERENCE       = (1 << 5),  /**< Reference revolution for fusion */
} uft_ir_rev_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * CORE STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Flux timing statistics
 */
typedef struct uft_ir_flux_stats {
    uint32_t    total_transitions;   /**< Total flux transitions */
    uint32_t    min_delta_ns;        /**< Minimum delta time (ns) */
    uint32_t    max_delta_ns;        /**< Maximum delta time (ns) */
    uint32_t    mean_delta_ns;       /**< Mean delta time (ns) */
    uint32_t    stddev_delta_ns;     /**< Std deviation (ns) */
    uint32_t    clock_period_ns;     /**< Detected clock period (ns) */
    uint32_t    index_to_index_ns;   /**< Index-to-index time (ns) */
    uint16_t    histogram_1us[64];   /**< 1µs bucket histogram (0-63µs) */
} uft_ir_flux_stats_t;

/**
 * @brief Weak bit region descriptor
 */
typedef struct uft_ir_weak_region {
    uint32_t    start_bit;           /**< Start bit position */
    uint32_t    length_bits;         /**< Length in bits */
    uint8_t     pattern;             /**< Pattern type (0=random, 1=stuck0, 2=stuck1) */
    uint8_t     confidence;          /**< Detection confidence (0-255) */
    uint16_t    reserved;
} uft_ir_weak_region_t;

/**
 * @brief Copy protection marker
 */
typedef struct uft_ir_protection {
    uint32_t    scheme_id;           /**< Protection scheme identifier */
    uint32_t    location_bit;        /**< Location in bitstream */
    uint32_t    length_bits;         /**< Affected region length */
    uint8_t     severity;            /**< Impact level (0-255) */
    uint8_t     confirmed;           /**< Analysis confirmed (bool) */
    uint16_t    signature_crc;       /**< Signature CRC for identification */
    char        name[32];            /**< Scheme name (e.g., "V-MAX!", "CopyLock") */
} uft_ir_protection_t;

/**
 * @brief Single revolution data
 */
typedef struct uft_ir_revolution {
    /** Metadata */
    uint32_t    rev_index;           /**< Revolution number (0-based) */
    uint32_t    flags;               /**< uft_ir_rev_flags_t */
    uint32_t    duration_ns;         /**< Total revolution time (ns) */
    uint32_t    index_offset_ns;     /**< Offset from index pulse (ns) */
    
    /** Flux data */
    uft_ir_data_type_t data_type;    /**< Type of data stored */
    uint32_t    flux_count;          /**< Number of flux transitions */
    uint32_t    data_size;           /**< Size of data array in bytes */
    uint32_t*   flux_deltas;         /**< Flux delta times (ns or ticks) */
    
    /** Optional confidence data (forensic mode) */
    uint8_t*    flux_confidence;     /**< Per-transition confidence (0-255) */
    
    /** Statistics */
    uft_ir_flux_stats_t stats;       /**< Computed statistics */
    
    /** Quality */
    uint8_t     quality_score;       /**< Overall quality (0-100) */
    uint8_t     reserved[3];
} uft_ir_revolution_t;

/**
 * @brief Complete track data (central hub structure)
 */
typedef struct uft_ir_track {
    /** Position */
    uint8_t     cylinder;            /**< Physical cylinder (0-83) */
    uint8_t     head;                /**< Head/side (0-1) */
    uint16_t    flags;               /**< uft_ir_track_flags_t */
    
    /** Fractional position (for half/quarter tracks) */
    int8_t      cyl_offset_quarters; /**< Offset in quarter-tracks (-2 to +2) */
    uint8_t     reserved1[3];
    
    /** Encoding & Format */
    uft_ir_encoding_t encoding;      /**< Detected/expected encoding */
    uint8_t     sectors_expected;    /**< Expected sector count */
    uint8_t     sectors_found;       /**< Actually found sectors */
    uint8_t     sectors_good;        /**< Sectors with valid CRC */
    
    /** Timing parameters */
    uint32_t    bitcell_ns;          /**< Nominal bitcell time (ns) */
    uint32_t    rpm_measured;        /**< Measured RPM × 100 (e.g., 30000 = 300.00) */
    uint32_t    write_splice_ns;     /**< Write splice location (ns from index) */
    
    /** Multi-revolution data */
    uint8_t     revolution_count;    /**< Number of revolutions stored */
    uint8_t     best_revolution;     /**< Index of best quality revolution */
    uint8_t     reserved2[2];
    uft_ir_revolution_t* revolutions[UFT_IR_MAX_REVOLUTIONS];
    
    /** Quality assessment */
    uft_ir_quality_t quality;        /**< Overall track quality */
    uint8_t     quality_score;       /**< Numeric score (0-100) */
    uint8_t     reserved3[2];
    
    /** Weak bits */
    uint16_t    weak_region_count;
    uint16_t    reserved4;
    uft_ir_weak_region_t* weak_regions;
    
    /** Copy protection */
    uint16_t    protection_count;
    uint16_t    reserved5;
    uft_ir_protection_t* protections;
    
    /** Decoded data (optional) */
    uint32_t    decoded_size;        /**< Size of decoded data */
    uint8_t*    decoded_data;        /**< Decoded sector data */
    
    /** Timing */
    uint64_t    capture_timestamp;   /**< Unix timestamp of capture */
    uint32_t    capture_duration_ms; /**< Time to capture this track */
    uint32_t    reserved6;
    
    /** User data */
    char        comment[UFT_IR_MAX_COMMENT_LEN];
} uft_ir_track_t;

/**
 * @brief Disk geometry descriptor
 */
typedef struct uft_ir_geometry {
    uint8_t     cylinders;           /**< Total cylinders */
    uint8_t     heads;               /**< Number of heads (1 or 2) */
    uint8_t     sectors_per_track;   /**< Sectors per track (if uniform) */
    uint8_t     sector_size_shift;   /**< Sector size = 128 << shift */
    uint32_t    total_sectors;       /**< Total sectors on disk */
    uint32_t    rpm;                 /**< Drive RPM (300 or 360 typical) */
    uint32_t    data_rate_kbps;      /**< Data rate in kbit/s */
    uint8_t     density;             /**< 0=SD, 1=DD, 2=HD, 3=ED */
    uint8_t     interleave;          /**< Sector interleave */
    uint8_t     track_skew;          /**< Track-to-track skew */
    uint8_t     head_skew;           /**< Head-to-head skew */
} uft_ir_geometry_t;

/**
 * @brief Disk image metadata
 */
typedef struct uft_ir_metadata {
    /** Source identification */
    uft_ir_source_t source_type;     /**< Hardware/software source */
    char        source_name[UFT_IR_MAX_SOURCE_LEN];   /**< e.g., "Greaseweazle F7" */
    char        source_version[32];  /**< Source firmware/software version */
    
    /** Image identification */
    char        title[128];          /**< Disk title/name */
    char        platform[32];        /**< Target platform (e.g., "Amiga", "C64") */
    char        creator[64];         /**< Who created this image */
    char        comment[UFT_IR_MAX_COMMENT_LEN];
    
    /** Timestamps */
    uint64_t    creation_time;       /**< Unix timestamp */
    uint64_t    modification_time;   /**< Last modification */
    uint64_t    original_date;       /**< Original disk date (if known) */
    
    /** Checksums */
    uint32_t    crc32;               /**< CRC32 of all track data */
    uint8_t     md5[16];             /**< MD5 hash */
    uint8_t     sha256[32];          /**< SHA-256 hash */
    
    /** Custom metadata */
    uint32_t    custom_size;
    uint8_t*    custom_data;
} uft_ir_metadata_t;

/**
 * @brief Complete disk image in UFT-IR format
 */
typedef struct uft_ir_disk {
    /** Header */
    uint64_t    magic;               /**< UFT_IR_MAGIC */
    uint32_t    version;             /**< UFT_IR_VERSION */
    uint32_t    header_size;         /**< Size of this header */
    
    /** Geometry */
    uft_ir_geometry_t geometry;
    
    /** Metadata */
    uft_ir_metadata_t metadata;
    
    /** Tracks */
    uint16_t    track_count;         /**< Number of tracks present */
    uint16_t    reserved;
    uft_ir_track_t** tracks;         /**< Array of track pointers */
    
    /** Quality summary */
    uint16_t    tracks_perfect;      /**< Tracks with perfect quality */
    uint16_t    tracks_good;         /**< Tracks with good quality */
    uint16_t    tracks_degraded;     /**< Tracks with degraded quality */
    uint16_t    tracks_bad;          /**< Tracks with bad/unreadable */
    
    /** Global flags */
    uint32_t    disk_flags;          /**< Aggregate of track flags */
    
    /** Write protection */
    uint8_t     write_protected;     /**< Original disk was write-protected */
    uint8_t     reserved2[3];
} uft_ir_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SERIALIZATION STRUCTURES (for file I/O)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief File header (on-disk format)
 */
#pragma pack(push, 1)
typedef struct uft_ir_file_header {
    uint8_t     magic[8];            /**< "UFTIR\0\1\0" */
    uint32_t    version;             /**< Format version */
    uint32_t    header_size;         /**< Total header size */
    uint32_t    flags;               /**< File flags */
    uft_ir_compression_t compression;/**< Compression type */
    uint8_t     reserved[3];
    uint64_t    uncompressed_size;   /**< Total uncompressed size */
    uint64_t    compressed_size;     /**< Compressed size (if applicable) */
    uint32_t    track_count;         /**< Number of tracks */
    uint32_t    crc32;               /**< Header CRC32 */
} uft_ir_file_header_t;

/**
 * @brief Track header (on-disk format)
 */
typedef struct uft_ir_track_header {
    uint8_t     cylinder;
    uint8_t     head;
    uint16_t    flags;
    uint32_t    data_offset;         /**< Offset to track data */
    uint32_t    data_size;           /**< Size of track data */
    uint32_t    uncompressed_size;   /**< Uncompressed track size */
    uint8_t     revolution_count;
    uint8_t     encoding;
    uint8_t     quality;
    uint8_t     compression;         /**< Compression type (uft_ir_compression_t) */
    uint32_t    crc32;               /**< Track data CRC32 */
} uft_ir_track_header_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════
 * API: LIFECYCLE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new empty disk image
 * @param cylinders Number of cylinders
 * @param heads Number of heads (1 or 2)
 * @return New disk image or NULL on error
 */
uft_ir_disk_t* uft_ir_disk_create(uint8_t cylinders, uint8_t heads);

/**
 * @brief Free disk image and all associated data
 * @param disk Disk to free (can be NULL)
 */
void uft_ir_disk_free(uft_ir_disk_t* disk);

/**
 * @brief Create a new track
 * @param cylinder Cylinder number
 * @param head Head number
 * @return New track or NULL on error
 */
uft_ir_track_t* uft_ir_track_create(uint8_t cylinder, uint8_t head);

/**
 * @brief Free track and all associated data
 * @param track Track to free (can be NULL)
 */
void uft_ir_track_free(uft_ir_track_t* track);

/**
 * @brief Create a new revolution
 * @param flux_count Number of flux transitions
 * @return New revolution or NULL on error
 */
uft_ir_revolution_t* uft_ir_revolution_create(uint32_t flux_count);

/**
 * @brief Free revolution and associated data
 * @param rev Revolution to free (can be NULL)
 */
void uft_ir_revolution_free(uft_ir_revolution_t* rev);

/**
 * @brief Clone a track (deep copy)
 * @param src Source track
 * @return Cloned track or NULL on error
 */
uft_ir_track_t* uft_ir_track_clone(const uft_ir_track_t* src);

/**
 * @brief Clone a revolution (deep copy)
 * @param src Source revolution
 * @return Cloned revolution or NULL on error
 */
uft_ir_revolution_t* uft_ir_revolution_clone(const uft_ir_revolution_t* src);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: TRACK MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Add track to disk image
 * @param disk Target disk
 * @param track Track to add (ownership transferred)
 * @return 0 on success, error code on failure
 */
int uft_ir_disk_add_track(uft_ir_disk_t* disk, uft_ir_track_t* track);

/**
 * @brief Get track from disk
 * @param disk Source disk
 * @param cylinder Cylinder number
 * @param head Head number
 * @return Track pointer or NULL if not found
 */
uft_ir_track_t* uft_ir_disk_get_track(const uft_ir_disk_t* disk, 
                                       uint8_t cylinder, uint8_t head);

/**
 * @brief Remove track from disk
 * @param disk Target disk
 * @param cylinder Cylinder number
 * @param head Head number
 * @return Removed track (caller must free) or NULL if not found
 */
uft_ir_track_t* uft_ir_disk_remove_track(uft_ir_disk_t* disk,
                                          uint8_t cylinder, uint8_t head);

/**
 * @brief Add revolution to track
 * @param track Target track
 * @param rev Revolution to add (ownership transferred)
 * @return Revolution index or -1 on error
 */
int uft_ir_track_add_revolution(uft_ir_track_t* track, uft_ir_revolution_t* rev);

/**
 * @brief Set flux data for revolution
 * @param rev Target revolution
 * @param deltas Flux delta array (copied)
 * @param count Number of deltas
 * @param data_type Type of data
 * @return 0 on success, error code on failure
 */
int uft_ir_revolution_set_flux(uft_ir_revolution_t* rev,
                                const uint32_t* deltas, uint32_t count,
                                uft_ir_data_type_t data_type);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: ANALYSIS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate statistics for a revolution
 * @param rev Revolution to analyze
 * @return 0 on success, error code on failure
 */
int uft_ir_revolution_calc_stats(uft_ir_revolution_t* rev);

/**
 * @brief Detect encoding type from flux data
 * @param rev Revolution to analyze
 * @param confidence Output: detection confidence (0-100)
 * @return Detected encoding type
 */
uft_ir_encoding_t uft_ir_detect_encoding(const uft_ir_revolution_t* rev,
                                          uint8_t* confidence);

/**
 * @brief Detect weak bit regions
 * @param track Track to analyze (uses all revolutions)
 * @return Number of weak regions found
 */
int uft_ir_detect_weak_bits(uft_ir_track_t* track);

/**
 * @brief Calculate track quality score
 * @param track Track to assess
 * @return Quality score (0-100)
 */
uint8_t uft_ir_calc_quality(uft_ir_track_t* track);

/**
 * @brief Find best revolution in track
 * @param track Track to search
 * @return Index of best revolution or -1 if none
 */
int uft_ir_find_best_revolution(const uft_ir_track_t* track);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: SERIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Save disk image to file
 * @param disk Disk to save
 * @param path Output file path
 * @param compression Compression type
 * @return 0 on success, error code on failure
 */
int uft_ir_disk_save(const uft_ir_disk_t* disk, const char* path,
                      uft_ir_compression_t compression);

/**
 * @brief Load disk image from file
 * @param path Input file path
 * @param disk Output: loaded disk (caller must free)
 * @return 0 on success, error code on failure
 */
int uft_ir_disk_load(const char* path, uft_ir_disk_t** disk);

/**
 * @brief Save single track to buffer
 * @param track Track to serialize
 * @param buffer Output buffer (allocated by function)
 * @param size Output: buffer size
 * @param compression Compression type
 * @return 0 on success, error code on failure
 */
int uft_ir_track_serialize(const uft_ir_track_t* track,
                            uint8_t** buffer, size_t* size,
                            uft_ir_compression_t compression);

/**
 * @brief Load track from buffer
 * @param buffer Input buffer
 * @param size Buffer size
 * @param track Output: loaded track (caller must free)
 * @return 0 on success, error code on failure
 */
int uft_ir_track_deserialize(const uint8_t* buffer, size_t size,
                              uft_ir_track_t** track);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: CONVERSION HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert ticks to nanoseconds
 * @param ticks Tick count
 * @return Time in nanoseconds
 */
static inline uint32_t uft_ir_ticks_to_ns(uint32_t ticks, uint32_t tick_rate) {
    return (uint32_t)(((uint64_t)ticks * 1000000000ULL) / tick_rate);
}

/**
 * @brief Convert nanoseconds to ticks
 * @param ns Time in nanoseconds
 * @param tick_rate Tick rate in Hz
 * @return Tick count
 */
static inline uint32_t uft_ir_ns_to_ticks(uint32_t ns, uint32_t tick_rate) {
    return (uint32_t)(((uint64_t)ns * tick_rate) / 1000000000ULL);
}

/**
 * @brief Get RPM from revolution duration
 * @param duration_ns Revolution time in nanoseconds
 * @return RPM × 100 (e.g., 30000 = 300.00 RPM)
 */
static inline uint32_t uft_ir_duration_to_rpm(uint64_t duration_ns) {
    if (duration_ns == 0) return 0;
    return (uint32_t)((60000000000ULL * 100) / duration_ns);
}

/**
 * @brief Get nominal bitcell time for encoding/density
 * @param encoding Encoding type
 * @param rpm Drive RPM
 * @return Bitcell time in nanoseconds
 */
uint32_t uft_ir_get_nominal_bitcell(uft_ir_encoding_t encoding, uint32_t rpm);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate disk image structure
 * @param disk Disk to validate
 * @param errors Output: error messages (can be NULL)
 * @param max_errors Maximum errors to collect
 * @return Number of errors found
 */
int uft_ir_disk_validate(const uft_ir_disk_t* disk, 
                          char** errors, int max_errors);

/**
 * @brief Validate track data integrity
 * @param track Track to validate
 * @return 0 if valid, error code if invalid
 */
int uft_ir_track_validate(const uft_ir_track_t* track);

/**
 * @brief Check if file is UFT-IR format
 * @param path File path
 * @return 1 if UFT-IR, 0 if not, -1 on error
 */
int uft_ir_is_uft_ir_file(const char* path);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: EXPORT/REPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Export disk information to JSON
 * @param disk Disk to export
 * @param include_flux Include raw flux data
 * @return JSON string (caller must free) or NULL on error
 */
char* uft_ir_disk_to_json(const uft_ir_disk_t* disk, bool include_flux);

/**
 * @brief Export track information to JSON
 * @param track Track to export
 * @param include_flux Include raw flux data
 * @return JSON string (caller must free) or NULL on error
 */
char* uft_ir_track_to_json(const uft_ir_track_t* track, bool include_flux);

/**
 * @brief Generate text summary of disk
 * @param disk Disk to summarize
 * @return Text summary (caller must free) or NULL on error
 */
char* uft_ir_disk_summary(const uft_ir_disk_t* disk);

/**
 * @brief Generate text summary of track
 * @param track Track to summarize
 * @return Text summary (caller must free) or NULL on error
 */
char* uft_ir_track_summary(const uft_ir_track_t* track);

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_IR_OK                    0
#define UFT_IR_ERR_NOMEM            -1
#define UFT_IR_ERR_INVALID          -2
#define UFT_IR_ERR_OVERFLOW         -3
#define UFT_IR_ERR_IO               -4
#define UFT_IR_ERR_FORMAT           -5
#define UFT_IR_ERR_VERSION          -6
#define UFT_IR_ERR_CHECKSUM         -7
#define UFT_IR_ERR_COMPRESSION      -8
#define UFT_IR_ERR_NOT_FOUND        -9
#define UFT_IR_ERR_DUPLICATE       -10
#define UFT_IR_ERR_CORRUPT         -11

/**
 * @brief Get error message for error code
 * @param err Error code
 * @return Static error message string
 */
const char* uft_ir_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IR_FORMAT_H */
