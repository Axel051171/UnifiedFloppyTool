/**
 * @file uft_format_params.h
 * 
 * Comprehensive parameter system for disk format encoding/decoding.
 */

#ifndef UFT_FORMAT_PARAMS_H
#define UFT_FORMAT_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ============================================================================
typedef enum {
    UFT_DATARATE_UNKNOWN = 0,
    UFT_DATARATE_125K    = 125000,   // FM single density
    UFT_DATARATE_250K    = 250000,   // MFM double density
    UFT_DATARATE_300K    = 300000,   // MFM 300rpm HD in DD drive
    UFT_DATARATE_500K    = 500000,   // MFM high density
    UFT_DATARATE_1000K   = 1000000,  // MFM extra-high density
} uft_datarate_t;

// ============================================================================
// ============================================================================
typedef enum {
    UFT_ENCODING_UNKNOWN = 0,
    UFT_ENCODING_FM,        // Frequency Modulation (SD)
    UFT_ENCODING_MFM,       // Modified FM (DD/HD)
    UFT_ENCODING_M2FM,      // M2FM (Intel)
    UFT_ENCODING_GCR,       // Group Coded Recording (Apple/C64)
    UFT_ENCODING_GCR_APPLE, // Apple II GCR (6-and-2)
    UFT_ENCODING_GCR_C64,   // Commodore GCR
    UFT_ENCODING_GCR_VICTOR,// Victor 9000 GCR
    UFT_ENCODING_GCR_BROTHER,// Brother word processor GCR
    UFT_ENCODING_AMIGA,     // Amiga MFM variant
    UFT_ENCODING_RX02,      // DEC RX02 encoding
} uft_encoding_t;

// ============================================================================
// ============================================================================
typedef enum {
    UFT_FDC_NONE = 0,
    UFT_FDC_PC,     // IBM PC (NEC 765/Intel 8272)
    UFT_FDC_WD,     // Western Digital (1770/1772/1793)
    UFT_FDC_AMIGA,  // Amiga custom
    UFT_FDC_APPLE,  // Apple IWM/SWIM
} uft_fdc_type_t;

// ============================================================================
// ============================================================================
typedef struct {
    // Track-specific overrides
    int32_t track;              // -1 = all tracks
    int32_t head;               // -1 = all heads
    
    // Sector header handling
    bool ignore_side_byte;
    bool ignore_track_byte;
    bool invert_side_byte;
    
    // Encoding
    bool use_fm;                // true=FM, false=MFM
    double target_clock_period_us;  // default 4.0 (MFM) or 8.0 (FM)
    
    // Gap sizes (bytes)
    int32_t gap0;               // Post-index gap (default 80)
    int32_t gap1;               // Post-ID gap (default 50)
    int32_t gap2;               // Pre-data gap (default 22)
    int32_t gap3;               // Post-data gap (default 80)
    
    // Sync patterns (raw 16-bit)
    uint16_t idam_byte;         // default 0x5554
    uint16_t dam_byte;          // default 0x5545
    uint16_t gap_fill_byte;     // default 0x9254
    
    // Timing
    double target_rotational_period_ms;  // default 200.0
} uft_ibm_params_t;

// ============================================================================
// ============================================================================
typedef struct {
    double clock_rate_us;           // default 2.0
    double post_index_gap_ms;       // default 0.5
} uft_amiga_params_t;

// ============================================================================
// ============================================================================
typedef enum {
    UFT_BROTHER_240 = 0,    // 78 tracks, 240KB
    UFT_BROTHER_120 = 1,    // 120 tracks
} uft_brother_format_t;

typedef struct {
    uft_brother_format_t format;
    double clock_rate_us;           // default 3.83
    double post_index_gap_ms;       // default 1.0
    double sector_spacing_ms;       // default 16.2
    double post_header_spacing_ms;  // default 0.69
} uft_brother_params_t;

// ============================================================================
// ============================================================================
typedef struct {
    int32_t min_track;
    int32_t max_track;
    int32_t head;
    
    double rotational_period_ms;
    double clock_period_us;
    double post_index_gap_us;
    
    int32_t pre_header_sync_bits;
    int32_t pre_data_sync_bits;
    int32_t post_data_gap_bits;
    int32_t post_header_gap_bits;
} uft_victor9k_track_params_t;

typedef struct {
    uft_victor9k_track_params_t zones[5];  // 5 speed zones
} uft_victor9k_params_t;

// ============================================================================
// ============================================================================
typedef enum {
    UFT_MICROPOLIS_CHECKSUM_AUTO = 0,
    UFT_MICROPOLIS_CHECKSUM_STANDARD = 1,
    UFT_MICROPOLIS_CHECKSUM_MZOS = 2,
} uft_micropolis_checksum_t;

typedef enum {
    UFT_MICROPOLIS_ECC_NONE = 0,
    UFT_MICROPOLIS_ECC_VECTOR = 1,
} uft_micropolis_ecc_t;

typedef struct {
    int32_t sector_output_size;         // 256 or 275
    uft_micropolis_checksum_t checksum_type;
    uft_micropolis_ecc_t ecc_type;
    double clock_period_us;             // default 2.0
    double rotational_period_ms;        // default 200.0
} uft_micropolis_params_t;

// ============================================================================
// ============================================================================
typedef struct {
    // Geometry
    int32_t cylinders;          // default 80
    int32_t heads;              // default 2
    int32_t sectors;            // sectors per track
    int32_t size;               // sector size code (0=128, 1=256, 2=512, 3=1024)
    
    // Timing/Encoding
    uft_fdc_type_t fdc;
    uft_datarate_t datarate;
    uft_encoding_t encoding;
    
    // Sector layout
    int32_t base;               // base sector number (usually 1)
    int32_t offset;             // offset into cyl 0 head 0
    int32_t interleave;         // sector interleave (1=sequential)
    int32_t skew;               // track skew
    
    // Head mapping
    int32_t head0;              // head 0 value in sector ID
    int32_t head1;              // head 1 value in sector ID
    
    // Formatting
    int32_t gap3;               // inter-sector gap
    uint8_t fill;               // fill byte for formatting
    
    // Flags
    bool cyls_first;            // true = all cyls on head 0 before head 1
} uft_format_params_t;

// ============================================================================
// ============================================================================
typedef enum {
    UFT_PRESET_CUSTOM = 0,
    
    // PC Formats
    UFT_PRESET_PC_160K,     // 40×1×8×512
    UFT_PRESET_PC_180K,     // 40×1×9×512
    UFT_PRESET_PC_320K,     // 40×2×8×512
    UFT_PRESET_PC_360K,     // 40×2×9×512
    UFT_PRESET_PC_640K,     // 80×2×8×512
    UFT_PRESET_PC_720K,     // 80×2×9×512
    UFT_PRESET_PC_1200K,    // 80×2×15×512
    UFT_PRESET_PC_1232K,    // 77×2×8×1024 (PC-98)
    UFT_PRESET_PC_1440K,    // 80×2×18×512
    UFT_PRESET_PC_2880K,    // 80×2×36×512
    
    // SAM Coupé
    UFT_PRESET_MGT,         // 80×2×10×512 (800K)
    UFT_PRESET_D2M,         // 80×2×10×512 (800K MGT D2M)
    UFT_PRESET_D4M,         // 80×2×20×512 (1.6MB MGT D4M)
    
    // Amstrad
    UFT_PRESET_CPC_DATA,    // 40×1×9×512
    UFT_PRESET_CPC_SYSTEM,  // 40×1×9×512 + boot
    
    // Spectrum
    UFT_PRESET_TRDOS,       // 80×2×16×256
    UFT_PRESET_OPUS,        // Opus Discovery
    UFT_PRESET_QDOS,        // Sinclair QL
    
    // Amiga
    UFT_PRESET_AMIGA_DD,    // 80×2×11×512 (880K)
    UFT_PRESET_AMIGA_HD,    // 80×2×22×512 (1.76MB)
    
    // Atari ST
    UFT_PRESET_ATARIST_SS,  // 80×1×9×512 (360K)
    UFT_PRESET_ATARIST_DS,  // 80×2×9×512 (720K)
    UFT_PRESET_ATARIST_HD,  // 80×2×18×512 (1.44MB)
    
    // Commodore
    UFT_PRESET_C64_1541,    // 35 tracks, GCR
    UFT_PRESET_C64_1571,    // 70 tracks, GCR
    UFT_PRESET_C64_1581,    // 80×2×10×512 MFM
    
    // Apple
    UFT_PRESET_APPLE2_DOS,  // 35×1×16×256 GCR
    UFT_PRESET_APPLE2_PRODOS,// 35×1×16×256 GCR
    UFT_PRESET_MAC_400K,    // 80×1×GCR (variable)
    UFT_PRESET_MAC_800K,    // 80×2×GCR (variable)
    
    // HP
    UFT_PRESET_LIF,         // 77×2×16×256
    
    // Thomson
    UFT_PRESET_SAP,         // 80×16×256
    
    // Pro-DOS
    UFT_PRESET_PRODOS,      // 80×2×9×512
    
    // Commodore D80/D82
    UFT_PRESET_D80,         // 8050
    UFT_PRESET_D81,         // 1581
    
    UFT_PRESET_COUNT
} uft_format_preset_t;

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Get default parameters for a format preset
 */
uft_format_params_t uft_format_get_preset(uft_format_preset_t preset);

/**
 * @brief Calculate sector size from size code
 */
static inline int32_t uft_sector_size_from_code(int32_t code) {
    return 128 << code;  // 0=128, 1=256, 2=512, 3=1024
}

/**
 * @brief Calculate size code from sector size
 */
static inline int32_t uft_sector_size_to_code(int32_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        default:   return 2;  // Default to 512
    }
}

/**
 * @brief Calculate track size in bytes
 */
static inline int32_t uft_track_size(const uft_format_params_t* fmt) {
    return fmt->sectors * uft_sector_size_from_code(fmt->size);
}

/**
 * @brief Calculate side size in bytes
 */
static inline int32_t uft_side_size(const uft_format_params_t* fmt) {
    return fmt->cylinders * uft_track_size(fmt);
}

/**
 * @brief Calculate disk size in bytes
 */
static inline int32_t uft_disk_size(const uft_format_params_t* fmt) {
    return fmt->heads * uft_side_size(fmt);
}

/**
 * @brief Calculate total sectors
 */
static inline int32_t uft_total_sectors(const uft_format_params_t* fmt) {
    return fmt->cylinders * fmt->heads * fmt->sectors;
}

/**
 * @brief Validate format parameters
 */
bool uft_format_validate(const uft_format_params_t* fmt);

/**
 * @brief Get data rate in bits per second
 */
static inline uint32_t uft_datarate_bps(uft_datarate_t rate) {
    return (uint32_t)rate;
}

/**
 * @brief Get encoding name string
 */
const char* uft_encoding_name(uft_encoding_t encoding);

/**
 * @brief Get FDC type name string
 */
const char* uft_fdc_name(uft_fdc_type_t fdc);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_PARAMS_H
