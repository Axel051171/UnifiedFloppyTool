/**
 * @file uft_protection.h
 * @brief UFT Copy Protection Detection Framework
 * @version 3.2.0.002
 * 
 * Comprehensive copy protection detection for:
 * - S-001: C64 Protection Suite (V-MAX!, RapidLok, Vorpal, Fat Tracks, GCR Timing)
 * - S-002: Apple II Protection Suite (Nibble Count, Timing Bits, Spiral Track)
 * - S-003: Atari ST Protection Suite (Macrodos, Copylock ST, Flaschel)
 * 
 * Design Philosophy:
 * - Score-based detection with confidence levels
 * - Multi-indicator correlation for accurate identification
 * - Variant differentiation (e.g., V-MAX! v1/v2/v3)
 * - Full audit trail integration
 * 
 * "Kein Bit verloren" - Every protection scheme preserved faithfully
 */

#ifndef UFT_PROTECTION_H
#define UFT_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** Maximum protection indicators per track */
#define UFT_PROT_MAX_INDICATORS     64
#define UFT_PROT_SCHEME_MAX_INDICATORS 16  /**< Per-scheme indicator limit (struct array size) */

/** Maximum protection schemes per disk */
#define UFT_PROT_MAX_SCHEMES        32

/** Maximum tracks to analyze */
#define UFT_PROT_MAX_TRACKS         168

/** Maximum custom sync patterns */
#define UFT_PROT_MAX_SYNC_PATTERNS  16

/** Maximum signature bytes */
#define UFT_PROT_MAX_SIGNATURE      64

/** Confidence threshold for positive detection */
#define UFT_PROT_CONFIDENCE_THRESHOLD  70

/*============================================================================
 * PLATFORM ENUMERATION
 *============================================================================*/

/** Supported platforms for protection detection */
typedef enum {
    UFT_PLATFORM_UNKNOWN = 0,
    
    /* Commodore */
    UFT_PLATFORM_C64,           /**< Commodore 64 */
    UFT_PLATFORM_C128,          /**< Commodore 128 */
    UFT_PLATFORM_VIC20,         /**< VIC-20 */
    UFT_PLATFORM_PLUS4,         /**< Plus/4 */
    UFT_PLATFORM_AMIGA,         /**< Amiga (all models) */
    
    /* Apple */
    UFT_PLATFORM_APPLE_II,      /**< Apple II series */
    UFT_PLATFORM_APPLE_III,     /**< Apple III */
    UFT_PLATFORM_MAC,           /**< Macintosh (early) */
    
    /* Atari */
    UFT_PLATFORM_ATARI_ST,      /**< Atari ST/STE/TT */
    UFT_PLATFORM_ATARI_8BIT,    /**< Atari 400/800/XL/XE */
    
    /* PC/DOS */
    UFT_PLATFORM_PC_DOS,        /**< IBM PC/MS-DOS */
    UFT_PLATFORM_PC_98,         /**< NEC PC-98 */
    
    /* Others */
    UFT_PLATFORM_MSX,           /**< MSX */
    UFT_PLATFORM_BBC,           /**< BBC Micro */
    UFT_PLATFORM_SPECTRUM,      /**< ZX Spectrum */
    UFT_PLATFORM_CPC,           /**< Amstrad CPC */
    UFT_PLATFORM_TRS80,         /**< TRS-80 */
    UFT_PLATFORM_TI99,          /**< TI-99/4A */
    
    UFT_PLATFORM_COUNT
} uft_platform_t;

/*============================================================================
 * PROTECTION SCHEME ENUMERATION
 *============================================================================*/

/** Known protection schemes */
typedef enum {
    UFT_PROT_NONE = 0,
    
    /*--- C64 Protection Schemes (0x0100-0x01FF) ---*/
    UFT_PROT_C64_BASE = 0x0100,
    
    /* V-MAX! Family */
    UFT_PROT_C64_VMAX_V1 = 0x0101,          /**< V-MAX! Version 1 */
    UFT_PROT_C64_VMAX_V2 = 0x0102,          /**< V-MAX! Version 2 */
    UFT_PROT_C64_VMAX_V3 = 0x0103,          /**< V-MAX! Version 3 */
    UFT_PROT_C64_VMAX_GENERIC = 0x010F,     /**< V-MAX! Unknown version */
    
    /* RapidLok Family */
    UFT_PROT_C64_RAPIDLOK_V1 = 0x0111,      /**< RapidLok Version 1 */
    UFT_PROT_C64_RAPIDLOK_V2 = 0x0112,      /**< RapidLok Version 2 */
    UFT_PROT_C64_RAPIDLOK_V3 = 0x0113,      /**< RapidLok Version 3 */
    UFT_PROT_C64_RAPIDLOK_V4 = 0x0114,      /**< RapidLok Version 4 */
    UFT_PROT_C64_RAPIDLOK_GENERIC = 0x011F, /**< RapidLok Unknown version */
    
    /* Vorpal Family */
    UFT_PROT_C64_VORPAL_V1 = 0x0121,        /**< Vorpal Version 1 */
    UFT_PROT_C64_VORPAL_V2 = 0x0122,        /**< Vorpal Version 2 */
    UFT_PROT_C64_VORPAL_GENERIC = 0x012F,   /**< Vorpal Unknown version */
    
    /* Other C64 */
    UFT_PROT_C64_PIRATESLAYER = 0x0130,     /**< PirateSlayer */
    UFT_PROT_C64_FAT_TRACK = 0x0140,        /**< Fat Track Protection */
    UFT_PROT_C64_HALF_TRACK = 0x0141,       /**< Half Track Protection */
    UFT_PROT_C64_GCR_TIMING = 0x0150,       /**< GCR Timing Variations */
    UFT_PROT_C64_CUSTOM_SYNC = 0x0160,      /**< Custom Sync Patterns */
    UFT_PROT_C64_SECTOR_GAP = 0x0170,       /**< Non-standard sector gaps */
    UFT_PROT_C64_DENSITY_MISMATCH = 0x0180, /**< Zone density mismatch */
    
    /*--- Apple II Protection Schemes (0x0200-0x02FF) ---*/
    UFT_PROT_APPLE_BASE = 0x0200,
    
    /* Nibble Count */
    UFT_PROT_APPLE_NIBBLE_COUNT = 0x0201,   /**< Nibble Count Protection */
    UFT_PROT_APPLE_TIMING_BITS = 0x0210,    /**< Timing Bit Protection */
    UFT_PROT_APPLE_SPIRAL_TRACK = 0x0220,   /**< Spiral Track Protection */
    UFT_PROT_APPLE_CROSS_TRACK = 0x0230,    /**< Cross-Track Sync */
    UFT_PROT_APPLE_CUSTOM_ADDR = 0x0240,    /**< Custom Address Marks */
    UFT_PROT_APPLE_CUSTOM_DATA = 0x0250,    /**< Custom Data Marks */
    UFT_PROT_APPLE_HALF_TRACK = 0x0260,     /**< Half Track Storage */
    UFT_PROT_APPLE_QUARTER_TRACK = 0x0261,  /**< Quarter Track Storage */
    UFT_PROT_APPLE_BIT_SLIP = 0x0270,       /**< Bit Slip Protection */
    UFT_PROT_APPLE_SYNC_FLOOD = 0x0280,     /**< Sync Byte Flooding */
    
    /*--- Atari ST Protection Schemes (0x0300-0x03FF) ---*/
    UFT_PROT_ATARI_BASE = 0x0300,
    
    /* Copylock Family */
    UFT_PROT_ATARI_COPYLOCK_V1 = 0x0301,    /**< Copylock ST Version 1 */
    UFT_PROT_ATARI_COPYLOCK_V2 = 0x0302,    /**< Copylock ST Version 2 */
    UFT_PROT_ATARI_COPYLOCK_V3 = 0x0303,    /**< Copylock ST Version 3 */
    UFT_PROT_ATARI_COPYLOCK_GENERIC = 0x030F,/**< Copylock Unknown */
    
    /* Macrodos Family */
    UFT_PROT_ATARI_MACRODOS = 0x0310,       /**< Macrodos Protection */
    UFT_PROT_ATARI_MACRODOS_PLUS = 0x0311,  /**< Macrodos+ */
    
    /* Other Atari ST */
    UFT_PROT_ATARI_FLASCHEL = 0x0320,       /**< Flaschel (FDC Bug Exploit) */
    UFT_PROT_ATARI_FUZZY_SECTOR = 0x0330,   /**< Fuzzy Sector */
    UFT_PROT_ATARI_LONG_TRACK = 0x0340,     /**< Long Track (>6250 bytes) */
    UFT_PROT_ATARI_SHORT_TRACK = 0x0341,    /**< Short Track (<6000 bytes) */
    UFT_PROT_ATARI_EXTRA_SECTOR = 0x0350,   /**< Extra Sector per Track */
    UFT_PROT_ATARI_MISSING_SECTOR = 0x0351, /**< Missing Sector */
    UFT_PROT_ATARI_SECTOR_IN_GAP = 0x0360,  /**< Sector Hidden in Gap */
    UFT_PROT_ATARI_DATA_IN_GAP = 0x0361,    /**< Data Hidden in Gap */
    UFT_PROT_ATARI_WEAK_BITS = 0x0370,      /**< Intentional Weak Bits */
    
    /*--- Amiga Protection Schemes (0x0400-0x04FF) ---*/
    UFT_PROT_AMIGA_BASE = 0x0400,
    
    UFT_PROT_AMIGA_COPYLOCK = 0x0401,       /**< Rob Northen Copylock */
    UFT_PROT_AMIGA_SPEEDLOCK = 0x0410,      /**< Speedlock */
    UFT_PROT_AMIGA_LONG_TRACK = 0x0420,     /**< Long Tracks (>12800 bytes) */
    UFT_PROT_AMIGA_SHORT_TRACK = 0x0421,    /**< Short Tracks */
    UFT_PROT_AMIGA_CUSTOM_SYNC = 0x0430,    /**< Custom MFM Sync Words */
    UFT_PROT_AMIGA_VARIABLE_SYNC = 0x0431,  /**< Variable Sync */
    UFT_PROT_AMIGA_WEAK_BITS = 0x0440,      /**< Intentional Weak Bits */
    UFT_PROT_AMIGA_CAPS_SPS = 0x0450,       /**< CAPS/SPS Special */
    
    /*--- PC Protection Schemes (0x0500-0x05FF) ---*/
    UFT_PROT_PC_BASE = 0x0500,
    
    UFT_PROT_PC_WEAK_SECTOR = 0x0501,       /**< Weak Sector */
    UFT_PROT_PC_FAT_TRICKS = 0x0510,        /**< FAT Structure Tricks */
    UFT_PROT_PC_EXTRA_SECTOR = 0x0520,      /**< Extra Sector */
    UFT_PROT_PC_LONG_SECTOR = 0x0530,       /**< Long Sector (>512 bytes) */
    
    /*--- Generic/Multi-Platform (0x0F00-0x0FFF) ---*/
    UFT_PROT_GENERIC_BASE = 0x0F00,
    
    UFT_PROT_GENERIC_WEAK_BITS = 0x0F01,    /**< Generic Weak Bits */
    UFT_PROT_GENERIC_LONG_TRACK = 0x0F10,   /**< Generic Long Track */
    UFT_PROT_GENERIC_TIMING = 0x0F20,       /**< Generic Timing Variation */
    UFT_PROT_GENERIC_CUSTOM_FORMAT = 0x0F30,/**< Custom Format Structure */
    
    UFT_PROT_MAX = 0xFFFF
} uft_protection_scheme_t;

/*============================================================================
 * INDICATOR TYPES
 *============================================================================*/

/** Types of protection indicators */
typedef enum {
    UFT_IND_NONE = 0,
    
    /* Structural Indicators */
    UFT_IND_TRACK_LENGTH,           /**< Non-standard track length */
    UFT_IND_SECTOR_COUNT,           /**< Non-standard sector count */
    UFT_IND_SECTOR_SIZE,            /**< Non-standard sector size */
    UFT_IND_SECTOR_GAP,             /**< Non-standard sector gaps */
    UFT_IND_HALF_TRACK,             /**< Half-track data present */
    UFT_IND_QUARTER_TRACK,          /**< Quarter-track data present */
    
    /* Sync/Encoding Indicators */
    UFT_IND_CUSTOM_SYNC,            /**< Non-standard sync pattern */
    UFT_IND_SYNC_LENGTH,            /**< Non-standard sync length */
    UFT_IND_SYNC_POSITION,          /**< Unusual sync position */
    UFT_IND_ADDRESS_MARK,           /**< Custom address mark */
    UFT_IND_DATA_MARK,              /**< Custom data mark */
    UFT_IND_ENCODING_MIX,           /**< Mixed encoding (MFM/FM) */
    
    /* Timing Indicators */
    UFT_IND_TIMING_VARIATION,       /**< Intentional timing variation */
    UFT_IND_BITCELL_DEVIATION,      /**< Bitcell timing deviation */
    UFT_IND_DENSITY_ZONE,           /**< Zone density mismatch */
    UFT_IND_RPM_VARIATION,          /**< RPM variation */
    
    /* Data Integrity Indicators */
    UFT_IND_WEAK_BITS,              /**< Weak/unstable bits */
    UFT_IND_CRC_ERROR,              /**< Intentional CRC error */
    UFT_IND_CHECKSUM_ERROR,         /**< Intentional checksum error */
    UFT_IND_DATA_PATTERN,           /**< Specific data pattern */
    
    /* Position Indicators */
    UFT_IND_TRACK_POSITION,         /**< Data at specific track */
    UFT_IND_SECTOR_POSITION,        /**< Data at specific sector */
    UFT_IND_GAP_DATA,               /**< Data hidden in gaps */
    UFT_IND_INDEX_POSITION,         /**< Index-relative position */
    
    /* Signature Indicators */
    UFT_IND_CODE_SIGNATURE,         /**< Known code signature */
    UFT_IND_STRING_SIGNATURE,       /**< Known string signature */
    UFT_IND_PATTERN_SIGNATURE,      /**< Known byte pattern */
    
    UFT_IND_TYPE_COUNT
} uft_indicator_type_t;

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief Single protection indicator
 */
typedef struct {
    uft_indicator_type_t type;      /**< Indicator type */
    uint8_t             cylinder;   /**< Cylinder (track) where found */
    uint8_t             head;       /**< Head (side) where found */
    uint8_t             sector;     /**< Sector (0xFF if track-level) */
    uint8_t             confidence; /**< Confidence 0-100 */
    
    /* Indicator-specific data */
    union {
        struct {
            uint32_t    expected;   /**< Expected value */
            uint32_t    actual;     /**< Actual value */
        } length;
        
        struct {
            uint8_t     pattern[8]; /**< Sync pattern bytes */
            uint8_t     length;     /**< Pattern length */
        } sync;
        
        struct {
            uint32_t    position;   /**< Bit position */
            uint16_t    count;      /**< Number of affected bits */
            uint8_t     stability;  /**< Stability score 0-100 */
        } weak;
        
        struct {
            int16_t     deviation_ns;/**< Timing deviation in ns */
            uint16_t    sample_count;/**< Number of samples */
        } timing;
        
        struct {
            uint8_t     signature[UFT_PROT_MAX_SIGNATURE];
            uint8_t     length;     /**< Signature length */
            uint32_t    offset;     /**< Offset where found */
        } signature;
        
        uint8_t         raw[64];    /**< Raw indicator data */
    } data;
    
    const char*         description;/**< Human-readable description */
} uft_prot_indicator_t;

/**
 * @brief Track-level protection info
 */
typedef struct {
    uint8_t             cylinder;
    uint8_t             head;
    
    /* Track metrics */
    uint32_t            raw_length_bits;    /**< Raw track length in bits */
    uint32_t            expected_length;    /**< Expected length */
    uint16_t            sector_count;       /**< Actual sector count */
    uint16_t            expected_sectors;   /**< Expected sector count */
    
    /* Timing metrics */
    uint32_t            bitcell_avg_ns;     /**< Average bitcell time */
    uint32_t            bitcell_stddev_ns;  /**< Bitcell standard deviation */
    int16_t             timing_offset_ns;   /**< Overall timing offset */
    
    /* Weak bit info */
    uint16_t            weak_region_count;
    uint32_t            weak_bit_total;
    
    /* Indicators found on this track */
    uft_prot_indicator_t indicators[UFT_PROT_MAX_INDICATORS];
    uint8_t             indicator_count;
    
    /* Track flags */
    uint32_t            flags;
    #define UFT_TRACK_FLAG_LONG         0x0001  /**< Longer than expected */
    #define UFT_TRACK_FLAG_SHORT        0x0002  /**< Shorter than expected */
    #define UFT_TRACK_FLAG_HALF         0x0004  /**< Half-track */
    #define UFT_TRACK_FLAG_QUARTER      0x0008  /**< Quarter-track */
    #define UFT_TRACK_FLAG_WEAK         0x0010  /**< Contains weak bits */
    #define UFT_TRACK_FLAG_TIMING       0x0020  /**< Timing anomalies */
    #define UFT_TRACK_FLAG_CUSTOM_SYNC  0x0040  /**< Custom sync */
    #define UFT_TRACK_FLAG_PROTECTED    0x0080  /**< Likely protected */
    #define UFT_TRACK_FLAG_UNREADABLE   0x0100  /**< Partially unreadable */
} uft_prot_track_t;

/**
 * @brief Detected protection scheme with confidence
 */
typedef struct {
    uft_protection_scheme_t scheme;     /**< Protection scheme ID */
    uint8_t                 confidence; /**< Confidence 0-100 */
    uint8_t                 variant;    /**< Specific variant (if known) */
    
    /* Location info */
    uint8_t                 key_track;      /**< Primary protection track */
    uint8_t                 key_sector;     /**< Primary protection sector */
    
    /* Indicator summary */
    uint8_t                 indicator_count;/**< Number of indicators */
    uint16_t                indicator_mask; /**< Bitmask of indicator types */
    
    /* Scheme-specific data */
    union {
        /* V-MAX! specifics */
        struct {
            uint8_t         sync_pattern[8];
            uint8_t         key_byte;
            uint8_t         loader_track;
        } vmax;
        
        /* RapidLok specifics */
        struct {
            uint8_t         sector_sequence[8];
            uint16_t        timing_offset;
            uint8_t         decode_key;
        } rapidlok;
        
        /* Vorpal specifics */
        struct {
            uint8_t         interleave;
            uint8_t         track_map[40];
            uint8_t         half_track_count;
        } vorpal;
        
        /* Apple II Nibble Count */
        struct {
            uint16_t        nibble_count;
            uint16_t        expected;
            uint8_t         threshold;
        } nibble;
        
        /* Apple II Timing */
        struct {
            uint32_t        bit_positions[8];
            uint8_t         bit_count;
        } timing_bits;
        
        /* Atari ST Copylock */
        struct {
            uint8_t         serial[16];
            uint32_t        signature;
            uint8_t         fuzzy_sector;
        } copylock;
        
        /* Atari ST Flaschel */
        struct {
            uint8_t         exploit_sector;
            uint16_t        fdc_command;
            uint8_t         trigger_byte;
        } flaschel;
        
        /* Generic */
        uint8_t             raw[64];
    } details;
    
    uint32_t                id;             /**< Unique scheme ID */
    uft_platform_t          platform;       /**< Target platform */
    uft_prot_indicator_t    indicators[16]; /**< Protection indicators */
    char                    notes[256];     /**< Detection notes */
    
    const char*             name;           /**< Human-readable name */
    const char*             description;    /**< Detailed description */
} uft_prot_scheme_t;

/**
 * @brief Complete disk protection analysis result
 */
typedef struct {
    /* Platform detection */
    uft_platform_t          platform;           /**< Detected platform */
    uint8_t                 platform_confidence;/**< Platform confidence */
    
    /* Detected schemes */
    uft_prot_scheme_t       schemes[UFT_PROT_MAX_SCHEMES];
    uint8_t                 scheme_count;
    
    /* Per-track analysis */
    uft_prot_track_t        tracks[UFT_PROT_MAX_TRACKS * 2];  /**< [cyl][head] */
    uint8_t                 cylinder_count;
    uint8_t                 head_count;
    
    /* Summary statistics */
    uint32_t                total_indicators;
    uint16_t                protected_track_count;
    uint16_t                weak_track_count;
    uint16_t                timing_anomaly_count;
    
    /* Analysis metadata */
    uint64_t                analysis_time_us;   /**< Analysis duration */
    uint32_t                flags;
    
    #define UFT_PROT_FLAG_COMPLETE      0x0001  /**< Full analysis done */
    #define UFT_PROT_FLAG_PROTECTED     0x0002  /**< Protection detected */
    #define UFT_PROT_FLAG_MULTIPLE      0x0004  /**< Multiple schemes */
    #define UFT_PROT_FLAG_UNCERTAIN     0x0008  /**< Low confidence */
    #define UFT_PROT_FLAG_VARIANT_KNOWN 0x0010  /**< Specific variant ID'd */
    #define UFT_PROT_FLAG_PRESERVABLE   0x0020  /**< Can be preserved */
    
    /* Preservation notes */
    char                    notes[1024];        /**< Analysis notes */
} uft_prot_result_t;

/*============================================================================
 * CALLBACK TYPES
 *============================================================================*/

/**
 * @brief Progress callback for protection analysis
 * @param current Current track being analyzed
 * @param total Total tracks to analyze
 * @param scheme Current best guess protection scheme (may change)
 * @param user_data User-provided context
 * @return Non-zero to abort analysis
 */
typedef int (*uft_prot_progress_cb)(
    uint16_t current,
    uint16_t total,
    uft_protection_scheme_t scheme,
    void* user_data
);

/*============================================================================
 * ANALYSIS CONTEXT
 *============================================================================*/

/**
 * @brief Analysis configuration
 */
typedef struct {
    /* Analysis depth */
    uint32_t                flags;
    
    #define UFT_PROT_ANAL_QUICK         0x0001  /**< Quick scan only */
    #define UFT_PROT_ANAL_DEEP          0x0002  /**< Deep analysis */
    #define UFT_PROT_ANAL_TIMING        0x0004  /**< Analyze timing */
    #define UFT_PROT_ANAL_WEAK_BITS     0x0008  /**< Detect weak bits */
    #define UFT_PROT_ANAL_HALF_TRACKS   0x0010  /**< Check half-tracks */
    #define UFT_PROT_ANAL_SIGNATURES    0x0020  /**< Check code signatures */
    #define UFT_PROT_ANAL_ALL           0x003F  /**< Full analysis */
    
    /* Platform hint (optional) */
    uft_platform_t          platform_hint;
    
    /* Track range (0 for all) */
    uint8_t                 start_cylinder;
    uint8_t                 end_cylinder;
    
    /* Callback */
    uft_prot_progress_cb    progress_cb;
    void*                   user_data;
    
    /* Thresholds */
    uint8_t                 confidence_threshold;   /**< Min confidence (def: 70) */
    uint16_t                timing_tolerance_ns;    /**< Timing tolerance (def: 500) */
    uint8_t                 weak_bit_threshold;     /**< Weak bit stability (def: 50) */
} uft_prot_config_t;

/*============================================================================
 * SIGNATURE DATABASE (Compile-time)
 *============================================================================*/

/**
 * @brief Known protection signature
 */
typedef struct {
    uft_protection_scheme_t scheme;
    uint32_t                id;             /**< Unique scheme ID */
    uft_platform_t          platform;       /**< Target platform */
    uft_prot_indicator_t    indicators[16]; /**< Protection indicators */
    char                    notes[256];     /**< Detection notes */
    
    const char*             name;
    
    /* Signature data */
    const uint8_t*          signature;
    size_t                  signature_len;
    
    /* Location hints */
    uint8_t                 typical_track;
    uint8_t                 typical_sector;
    uint16_t                typical_offset;
    
    /* Characteristics */
    uint32_t                characteristic_flags;
    
    /* Scoring weights */
    uint8_t                 base_confidence;
    uint8_t                 signature_weight;
} uft_prot_signature_db_t;

/*============================================================================
 * PUBLIC API - Initialization
 *============================================================================*/

/**
 * @brief Initialize protection analysis configuration with defaults
 * @param config Configuration to initialize
 */
void uft_prot_config_init(uft_prot_config_t* config);

/**
 * @brief Initialize protection result structure
 * @param result Result structure to initialize
 */
void uft_prot_result_init(uft_prot_result_t* result);

/**
 * @brief Free resources in protection result
 * @param result Result structure to free
 */
void uft_prot_result_free(uft_prot_result_t* result);

/*============================================================================
 * PUBLIC API - Analysis Functions
 *============================================================================*/

/**
 * @brief Analyze disk for copy protection
 * @param flux_data Raw flux data
 * @param flux_size Size of flux data
 * @param config Analysis configuration
 * @param result Result structure (output)
 * @return 0 on success, negative on error
 */
int uft_prot_analyze_flux(
    const uint8_t* flux_data,
    size_t flux_size,
    const uft_prot_config_t* config,
    uft_prot_result_t* result
);

/**
 * @brief Analyze decoded track data for copy protection
 * @param bitstream Decoded bitstream
 * @param bit_count Number of bits
 * @param cylinder Cylinder number
 * @param head Head number
 * @param config Analysis configuration
 * @param track_result Track result (output)
 * @return 0 on success, negative on error
 */
int uft_prot_analyze_track(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t cylinder,
    uint8_t head,
    const uft_prot_config_t* config,
    uft_prot_track_t* track_result
);

/**
 * @brief Detect platform from disk characteristics
 * @param result Protection analysis result
 * @return Detected platform (UFT_PLATFORM_UNKNOWN if unsure)
 */
uft_platform_t uft_prot_detect_platform(const uft_prot_result_t* result);

/*============================================================================
 * PUBLIC API - C64 Protection Suite (S-001)
 *============================================================================*/

/**
 * @brief Detect V-MAX! protection
 * @param bitstream GCR-encoded bitstream
 * @param bit_count Number of bits
 * @param track Track number
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_c64_detect_vmax(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect RapidLok protection
 * @param bitstream GCR-encoded bitstream
 * @param bit_count Number of bits
 * @param track Track number
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_c64_detect_rapidlok(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Vorpal protection
 * @param bitstream GCR-encoded bitstream
 * @param bit_count Number of bits
 * @param track Track number
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_c64_detect_vorpal(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Fat Track protection
 * @param flux_samples Flux timing samples
 * @param sample_count Number of samples
 * @param track_count Total tracks (>35 indicates fat tracks)
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_c64_detect_fat_track(
    const uint32_t* flux_samples,
    size_t sample_count,
    uint8_t track_count,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect GCR timing variations
 * @param flux_samples Flux timing samples
 * @param sample_count Number of samples
 * @param nominal_bitcell_ns Nominal bitcell time
 * @param indicators Output indicators array
 * @param max_indicators Maximum indicators to return
 * @return Number of indicators found
 */
uint8_t uft_prot_c64_detect_gcr_timing(
    const uint32_t* flux_samples,
    size_t sample_count,
    uint32_t nominal_bitcell_ns,
    uft_prot_indicator_t* indicators,
    uint8_t max_indicators
);

/*============================================================================
 * PUBLIC API - Apple II Protection Suite (S-002)
 *============================================================================*/

/**
 * @brief Detect Nibble Count protection
 * @param track_data Raw track data
 * @param track_size Track size in bytes
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_apple_detect_nibble_count(
    const uint8_t* track_data,
    size_t track_size,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Timing Bit protection
 * @param flux_samples Flux timing samples
 * @param sample_count Number of samples
 * @param track Track number
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_apple_detect_timing_bits(
    const uint32_t* flux_samples,
    size_t sample_count,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Spiral Track protection
 * @param quarter_track_data Data from multiple quarter-tracks
 * @param track_count Number of quarter-tracks
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_apple_detect_spiral(
    const uint8_t** quarter_track_data,
    size_t* track_sizes,
    uint8_t track_count,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Cross-Track Sync protection
 * @param track_data Array of track data
 * @param track_sizes Array of track sizes
 * @param track_count Number of tracks
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_apple_detect_cross_track(
    const uint8_t** track_data,
    const size_t* track_sizes,
    uint8_t track_count,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect custom address/data marks
 * @param track_data Track data
 * @param track_size Track size
 * @param indicators Output indicators
 * @param max_indicators Maximum indicators
 * @return Number of indicators found
 */
uint8_t uft_prot_apple_detect_custom_marks(
    const uint8_t* track_data,
    size_t track_size,
    uft_prot_indicator_t* indicators,
    uint8_t max_indicators
);

/*============================================================================
 * PUBLIC API - Atari ST Protection Suite (S-003)
 *============================================================================*/

/**
 * @brief Detect Copylock ST protection
 * @param bitstream MFM-encoded bitstream
 * @param bit_count Number of bits
 * @param track Track number
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_atari_detect_copylock(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Macrodos protection
 * @param bitstream MFM-encoded bitstream
 * @param bit_count Number of bits
 * @param track Track number
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_atari_detect_macrodos(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint8_t track,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Flaschel protection (FDC bug exploit)
 * @param sector_data Sector data array
 * @param sector_count Number of sectors
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_atari_detect_flaschel(
    const uint8_t** sector_data,
    uint8_t sector_count,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Fuzzy Sector protection
 * @param sector_reads Multiple reads of same sector
 * @param read_count Number of reads
 * @param sector_size Sector size
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_atari_detect_fuzzy(
    const uint8_t** sector_reads,
    uint8_t read_count,
    size_t sector_size,
    uft_prot_scheme_t* scheme
);

/**
 * @brief Detect Long Track protection
 * @param track_length Actual track length in bytes
 * @param expected_length Expected length (typically 6250)
 * @param scheme Output scheme details
 * @return Confidence 0-100
 */
uint8_t uft_prot_atari_detect_long_track(
    uint32_t track_length,
    uint32_t expected_length,
    uft_prot_scheme_t* scheme
);

/*============================================================================
 * PUBLIC API - Utility Functions
 *============================================================================*/

/**
 * @brief Get protection scheme name
 * @param scheme Protection scheme ID
 * @return Human-readable name
 */
const char* uft_prot_scheme_name(uft_protection_scheme_t scheme);

/**
 * @brief Get platform name
 * @param platform Platform ID
 * @return Human-readable name
 */
const char* uft_prot_platform_name(uft_platform_t platform);

/**
 * @brief Get indicator type name
 * @param type Indicator type
 * @return Human-readable name
 */
const char* uft_prot_indicator_name(uft_indicator_type_t type);

/**
 * @brief Export protection analysis to JSON
 * @param result Analysis result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or negative on error
 */
int uft_prot_export_json(
    const uft_prot_result_t* result,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Export protection analysis to Markdown report
 * @param result Analysis result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or negative on error
 */
int uft_prot_export_markdown(
    const uft_prot_result_t* result,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Print protection analysis summary
 * @param result Analysis result
 */
void uft_prot_print_summary(const uft_prot_result_t* result);

/**
 * @brief Check if protection scheme is preservable with current tools
 * @param scheme Protection scheme
 * @return true if preservable
 */
bool uft_prot_is_preservable(uft_protection_scheme_t scheme);

/**
 * @brief Get preservation recommendations for a scheme
 * @param scheme Protection scheme
 * @return Recommendation string
 */
const char* uft_prot_preservation_notes(uft_protection_scheme_t scheme);

/*============================================================================
 * INLINE HELPERS
 *============================================================================*/

/**
 * @brief Check if scheme is a C64 protection
 */
static inline bool uft_prot_is_c64(uft_protection_scheme_t scheme) {
    return (scheme & 0xFF00) == UFT_PROT_C64_BASE;
}

/**
 * @brief Check if scheme is an Apple II protection
 */
static inline bool uft_prot_is_apple(uft_protection_scheme_t scheme) {
    return (scheme & 0xFF00) == UFT_PROT_APPLE_BASE;
}

/**
 * @brief Check if scheme is an Atari ST protection
 */
static inline bool uft_prot_is_atari_st(uft_protection_scheme_t scheme) {
    return (scheme & 0xFF00) == UFT_PROT_ATARI_BASE;
}

/**
 * @brief Check if scheme is an Amiga protection
 */
static inline bool uft_prot_is_amiga(uft_protection_scheme_t scheme) {
    return (scheme & 0xFF00) == UFT_PROT_AMIGA_BASE;
}

/**
 * @brief Get platform from protection scheme
 */
static inline uft_platform_t uft_prot_scheme_platform(uft_protection_scheme_t scheme) {
    switch (scheme & 0xFF00) {
        case UFT_PROT_C64_BASE:   return UFT_PLATFORM_C64;
        case UFT_PROT_APPLE_BASE: return UFT_PLATFORM_APPLE_II;
        case UFT_PROT_ATARI_BASE: return UFT_PLATFORM_ATARI_ST;
        case UFT_PROT_AMIGA_BASE: return UFT_PLATFORM_AMIGA;
        case UFT_PROT_PC_BASE:    return UFT_PLATFORM_PC_DOS;
        default:                  return UFT_PLATFORM_UNKNOWN;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_H */
