/**
 * @file uft_uft_kf_algorithms.h
 * 
 * This header contains data structures and constants reverse-engineered
 * 
 * 
 * @author UFT Development Team
 * @date 2025
 * @license MIT
 */

#ifndef UFT_UFT_UFT_KF_ALGORITHMS_H
#define UFT_UFT_UFT_KF_ALGORITHMS_H

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ============================================================================

/**
 * 
 */
typedef enum uft_kfe_error {
    KFE_OK = 0,                     ///< Success
    
    // Cell-level errors
    KFE_CELL_BAD_RPM,               ///< Cell has bad RPM
    KFE_CELL_MISSING_INDEX,         ///< Cell missing index signal
    
    // Stream-level errors
    KFE_STR_DEV_BUFFER,             ///< Device buffer error
    KFE_STR_DEV_INDEX,              ///< Device index error
    KFE_STR_INDEX_REFERENCE,        ///< Index reference error
    KFE_STR_INVALID_CODE,           ///< Invalid stream code
    KFE_STR_INVALID_OOB,            ///< Invalid OOB message
    KFE_STR_MISSING_DATA,           ///< Missing stream data
    KFE_STR_MISSING_END,            ///< Missing stream end marker
    KFE_STR_MISSING_INDEX,          ///< Missing index in stream
    KFE_STR_TRANSFER,               ///< Transfer error
    KFE_STR_WRONG_POSITION          ///< Wrong stream position
} uft_kfe_error_t;

// ============================================================================
// ============================================================================

/**
 * @brief OOB Message Type Codes
 * 
 */
typedef enum uft_c2_oob_type {
    C2_OOB_INVALID      = 0x00,     ///< Invalid
    C2_OOB_STREAM_READ  = 0x01,     ///< Stream read data
    C2_OOB_INDEX        = 0x02,     ///< Index signal
    C2_OOB_STREAM_END   = 0x03,     ///< Stream end
    C2_OOB_INFO         = 0x04,     ///< Info string
    C2_OOB_END          = 0x0D      ///< End of OOB data
} uft_c2_oob_type_t;

/**
 * @brief OOB Header Structure
 */
typedef struct uft_c2_oob_header {
    uint8_t  sign;          ///< 0x0D for OOB
    uint8_t  type;          ///< uft_c2_oob_type_t
    uint16_t size;          ///< Payload size
} uft_c2_oob_header_t;

/**
 * @brief OOB Disk Index Structure
 */
typedef struct uft_c2_oob_disk_index {
    uint32_t stream_position;   ///< Position in stream
    uint32_t timer_value;       ///< Timer value at index
} uft_c2_oob_disk_index_t;

/**
 * @brief OOB Stream Read Structure
 */
typedef struct uft_c2_oob_stream_read {
    uint32_t stream_position;   ///< Current stream position
    uint32_t transfer_time;     ///< Transfer time
} uft_c2_oob_stream_read_t;

/**
 * @brief OOB Stream End Structure
 */
typedef struct uft_c2_oob_stream_end {
    uint32_t stream_position;   ///< Final stream position
    uint32_t result_code;       ///< Result/error code
} uft_c2_oob_stream_end_t;

// ============================================================================
// ============================================================================

/**
 * @brief Cell Statistics
 * 
 * Statistics computed for a track/revolution.
 */
typedef struct uft_uft_kf_cell_stat {
    double avgbps;      ///< Average bits per second
    double avgdrift;    ///< Average drift (µs)
    double avgfr;       ///< Average flux reversals
    double avgrpm;      ///< Average RPM
    double avgrps;      ///< Average revolutions per second
} uft_uft_kf_cell_stat_t;

/**
 * @brief Cell Index
 * 
 * Index data for a cell position.
 */
typedef struct uft_uft_kf_cell_index {
    double   cellpos;   ///< Cell position
    double   rpm;       ///< Measured RPM at this position
} uft_uft_kf_cell_index_t;

/**
 * @brief Stream Index
 * 
 * Index marker position in stream.
 */
typedef struct uft_kf_stream_index {
    uint64_t position;  ///< Stream byte position
} uft_kf_stream_index_t;

// ============================================================================
// ============================================================================

/**
 * @brief Histogram Structure for Timing Analysis
 */
typedef struct uft_kf_histogram {
    int*     counts;        ///< Bin counts
    int      num_bins;      ///< Number of bins
    double   factor;        ///< Scaling factor
    double   min_val;       ///< Minimum value
    double   max_val;       ///< Maximum value (computed lazily)
    int      idx_dir;       ///< Index direction
    bool     finished;      ///< Histogram complete
} uft_kf_histogram_t;

// ============================================================================
// ============================================================================

/**
 * @brief Timing Constants
 */
typedef struct uft_kf_timing {
    double sample_clock;    ///< Sample clock frequency
    double index_clock;     ///< Index clock frequency
} uft_kf_timing_t;

/**
 * @brief Default Timing Values
 * 
 * - Sample clock: 24027428.57 Hz (ICK/2)
 * - Index clock: 48054857.14 Hz (ICK)
 * - ICK = ((18432000 * 73) / 14) / 2
 */
#define UFT_UFT_KF_DEFAULT_SAMPLE_CLOCK  24027428.57
#define UFT_UFT_KF_DEFAULT_INDEX_CLOCK   48054857.14
#define UFT_UFT_KF_TICK_NS               41.619  // nanoseconds per tick

// ============================================================================
// ============================================================================

/**
 * @brief Cell Buffer Entry
 * 
 * Each cell entry is a (position, timing) tuple.
 */
typedef struct uft_uft_kf_cell_entry {
    double position;    ///< Cell position in track
    double timing;      ///< Timing value (µs or ticks)
} uft_uft_kf_cell_entry_t;

/**
 * @brief Cell Buffer
 * 
 * Stores decoded cell data for a track.
 */
typedef struct uft_uft_kf_cell_buffer {
    uft_uft_kf_cell_entry_t* cells;     ///< Cell entries
    size_t               count;     ///< Number of cells
    size_t               capacity;  ///< Allocated capacity
} uft_uft_kf_cell_buffer_t;

// ============================================================================
// ============================================================================

/**
 * @brief Density Type
 */
typedef enum uft_kf_density {
    UFT_KF_DENSITY_DD,      ///< Double Density
    UFT_KF_DENSITY_HD       ///< High Density
} uft_kf_density_t;

/**
 * @brief Track Result
 */
typedef enum uft_kf_track_result {
    UFT_KF_RESULT_NOT_DUMPED,   ///< Not yet read
    UFT_KF_RESULT_GOOD,         ///< All sectors OK
    UFT_KF_RESULT_BAD,          ///< Has errors
    UFT_KF_RESULT_UNKNOWN,      ///< Unknown status
    UFT_KF_RESULT_MISMATCH      ///< Format mismatch
} uft_kf_track_result_t;

/**
 * @brief Format Status
 */
typedef enum uft_kf_format_status {
    UFT_KF_FORMAT_UNKNOWN,      ///< Unknown format
    UFT_KF_FORMAT_GOOD,         ///< Format detected OK
    UFT_KF_FORMAT_BAD,          ///< Format detection failed
    UFT_KF_FORMAT_MISMATCH      ///< Format doesn't match expected
} uft_kf_format_status_t;

/**
 * @brief Sector Flags
 * 
 */
typedef enum uft_kf_flag {
    UFT_KF_FLAG_NONE           = 0,
    UFT_KF_FLAG_PROTECTION     = (1 << 0),  ///< P - Protection detected
    UFT_KF_FLAG_SECTOR_IGNORED = (1 << 1),  ///< N - Sector not in image
    UFT_KF_FLAG_TRUNCATED      = (1 << 2),  ///< X - Decoding stopped
    UFT_KF_FLAG_EXTRA_HEADER   = (1 << 3),  ///< H - Hidden header data
    UFT_KF_FLAG_NON_STANDARD   = (1 << 4),  ///< I - Non-standard format
    UFT_KF_FLAG_BAD_TRACK_ID   = (1 << 5),  ///< T - Wrong track number
    UFT_KF_FLAG_BAD_SIDE_ID    = (1 << 6),  ///< S - Wrong side number
    UFT_KF_FLAG_OUT_OF_RANGE   = (1 << 7),  ///< B - Sector out of range
    UFT_KF_FLAG_BAD_LENGTH     = (1 << 8),  ///< L - Non-standard length
    UFT_KF_FLAG_BAD_OFFSET     = (1 << 9),  ///< Z - Illegal offset
    UFT_KF_FLAG_UNCHECKED_CRC  = (1 << 10)  ///< C - Unchecked checksum
} uft_kf_flag_t;

/**
 * @brief Flag Severity
 */
typedef enum uft_kf_severity {
    UFT_KF_SEVERITY_INFO,       ///< Informational
    UFT_KF_SEVERITY_WARNING,    ///< Warning
    UFT_KF_SEVERITY_SERIOUS     ///< Serious error
} uft_kf_severity_t;

/**
 * @brief Get flag character for display
 */
static inline char uft_kf_flag_char(uft_kf_flag_t flag) {
    switch (flag) {
        case UFT_KF_FLAG_PROTECTION:     return 'P';
        case UFT_KF_FLAG_SECTOR_IGNORED: return 'N';
        case UFT_KF_FLAG_TRUNCATED:      return 'X';
        case UFT_KF_FLAG_EXTRA_HEADER:   return 'H';
        case UFT_KF_FLAG_NON_STANDARD:   return 'I';
        case UFT_KF_FLAG_BAD_TRACK_ID:   return 'T';
        case UFT_KF_FLAG_BAD_SIDE_ID:    return 'S';
        case UFT_KF_FLAG_OUT_OF_RANGE:   return 'B';
        case UFT_KF_FLAG_BAD_LENGTH:     return 'L';
        case UFT_KF_FLAG_BAD_OFFSET:     return 'Z';
        case UFT_KF_FLAG_UNCHECKED_CRC:  return 'C';
        default:                     return '?';
    }
}

// ============================================================================
// ============================================================================

/**
 * @brief Image Descriptor
 * 
 * Describes an image format type.
 */
typedef struct uft_kf_image_descriptor {
    const char* name;           ///< Format name (e.g., "amiga_dd")
    const char* extension;      ///< File extension (e.g., "adf")
    const char* description;    ///< Human-readable description
    bool        write_enabled;  ///< Write support available
} uft_kf_image_descriptor_t;

// ============================================================================
// ============================================================================

/**
 * @brief Basic Track Info
 */
typedef struct uft_kf_track_info_basic {
    int                   track_number;
    int                   logical_track;
    const char*           format_name;
    uft_kf_track_result_t result;
    int                   sectors_found;
    int                   sectors_expected;
    double                rpm;
    int                   transfer_rate;    // bytes/sec
} uft_kf_track_info_basic_t;

/**
 * @brief Full Track Info (includes advanced metrics)
 */
typedef struct uft_kf_track_info_full {
    uft_kf_track_info_basic_t basic;
    
    // Advanced metrics (from MetricSet2)
    int                   flux_reversals;
    double                drift_us;
    double                base_us;
    
    // Band info
    struct {
        double timing_us;
        bool   present;
    } bands[8];
    int                   num_bands;
    
    // Flags
    uint32_t              flags;        // uft_kf_flag_t bitmask
} uft_kf_track_info_full_t;

// ============================================================================
// ============================================================================

/**
 * @brief Read Error Types
 */
typedef enum uft_kf_read_error {
    UFT_KF_ERR_NONE = 0,
    UFT_KF_ERR_BAD_SECTOR,              ///< Bad sector found
    UFT_KF_ERR_READ_FAILED,             ///< Read operation failed
    UFT_KF_ERR_STREAM_FILE_OPEN,        ///< Can't open stream file
    UFT_KF_ERR_BUFFERING,               ///< Buffering error
    UFT_KF_ERR_STREAM_READ,             ///< Error reading stream
    UFT_KF_ERR_STREAM_POSITION,         ///< Bad stream position
    UFT_KF_ERR_NO_DISK,                 ///< No disk in drive
    UFT_KF_ERR_COMMAND_REJECTED         ///< Command rejected
} uft_kf_read_error_t;

/**
 * @brief Hardware Error Types
 */
typedef enum uft_kf_hw_error {
    UFT_KF_HW_OK = 0,
    UFT_KF_HW_DEVICE_NOT_FOUND,         ///< Device not found
    UFT_KF_HW_DRIVE_NOT_FOUND,          ///< Drive not found
    UFT_KF_HW_DISCONNECT_TIMEOUT,       ///< Disconnect timeout
    UFT_KF_HW_MODE_FAILED,              ///< Mode command failed
    UFT_KF_HW_STATUS_FAILED,            ///< Status command failed
    UFT_KF_HW_IN_USE,                   ///< Device in use
    UFT_KF_HW_USB_ENDPOINT              ///< USB endpoint not found
} uft_kf_hw_error_t;

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Initialize default timing
 */
static inline uft_kf_timing_t uft_kf_default_timing(void) {
    return (uft_kf_timing_t){
        .sample_clock = UFT_UFT_KF_DEFAULT_SAMPLE_CLOCK,
        .index_clock = UFT_UFT_KF_DEFAULT_INDEX_CLOCK
    };
}

/**
 * @brief Convert ticks to microseconds
 */
static inline double uft_kf_ticks_to_us(uint32_t ticks, const uft_kf_timing_t* timing) {
    return (double)ticks * 1000000.0 / timing->sample_clock;
}

/**
 * @brief Convert microseconds to ticks
 */
static inline uint32_t uft_kf_us_to_ticks(double us, const uft_kf_timing_t* timing) {
    return (uint32_t)(us * timing->sample_clock / 1000000.0);
}

/**
 * @brief Calculate RPM from index timing
 * 
 * @param index_ticks Time between two index pulses in sample clock ticks
 * @param timing Timing configuration
 * @return RPM value
 */
static inline double uft_uft_kf_calc_rpm(uint32_t index_ticks, const uft_kf_timing_t* timing) {
    double seconds = (double)index_ticks / timing->sample_clock;
    return 60.0 / seconds;
}

/**
 * @brief Calculate expected index time for given RPM
 * 
 * @param rpm Target RPM
 * @param timing Timing configuration
 * @return Index time in sample clock ticks
 */
static inline uint32_t uft_kf_rpm_to_ticks(double rpm, const uft_kf_timing_t* timing) {
    double seconds = 60.0 / rpm;
    return (uint32_t)(seconds * timing->sample_clock);
}

/**
 * @brief Initialize histogram
 */
static inline void uft_kf_hist_init(uft_kf_histogram_t* hist, int num_bins, 
                                    double min_val, double factor) {
    hist->num_bins = num_bins;
    hist->min_val = min_val;
    hist->factor = factor;
    hist->max_val = 0;
    hist->idx_dir = 0;
    hist->finished = false;
    // Caller must allocate hist->counts
}

/**
 * @brief Add value to histogram
 */
static inline void uft_kf_hist_add(uft_kf_histogram_t* hist, double value) {
    if (hist->finished || !hist->counts) return;
    
    int bin = (int)((value - hist->min_val) * hist->factor);
    if (bin >= 0 && bin < hist->num_bins) {
        hist->counts[bin]++;
    }
}

// ============================================================================
// ============================================================================

/**
 * 
 * These are derived from the CDiskEncoding class in dtc binary.
 */
typedef enum uft_kf_encoding_type {
    // FM (Single Density)
    UFT_KF_ENC_FM,
    
    // MFM (Double Density)
    UFT_KF_ENC_MFM,
    
    // GCR C64/CBM
    UFT_KF_ENC_GCR_CBM,         ///< Standard C64 GCR
    UFT_KF_ENC_GCR_CBM_S,       ///< C64 GCR Short
    
    // GCR Apple
    UFT_KF_ENC_GCR_APPLE_H,     ///< Apple Header
    UFT_KF_ENC_GCR_APPLE_5,     ///< Apple 5.25"
    UFT_KF_ENC_GCR_APPLE_6,     ///< Apple 6-and-2
    
    // GCR Copy Protection (C64)
    UFT_KF_ENC_GCR_VORPAL,      ///< Vorpal protection
    UFT_KF_ENC_GCR_VORPAL2,     ///< Vorpal 2 protection
    UFT_KF_ENC_GCR_VMAX,        ///< V-Max protection
    UFT_KF_ENC_GCR_VMAX_OLD,    ///< V-Max old version
    UFT_KF_ENC_GCR_BIGFIVE,     ///< Big Five protection
    UFT_KF_ENC_GCR_OZISOFT,     ///< Ozisoft protection
    UFT_KF_ENC_GCR_TEQUE,       ///< Teque protection
    
    // GCR 4-bit
    UFT_KF_ENC_GCR_4BIT         ///< 4-bit GCR
} uft_kf_encoding_type_t;

/**
 * @brief Encoding function pointer types
 */
typedef void (*uft_kf_encode_fn)(const uint8_t* in, uint8_t* out, size_t len);
typedef void (*uft_kf_decode_fn)(const uint8_t* in, uint8_t* out, size_t len);
typedef void (*uft_kf_init_fn)(const uint8_t* table, int flags);

/**
 * @brief Encoding method structure
 */
typedef struct uft_kf_encoding_method {
    uft_kf_encoding_type_t type;
    const char*           name;
    uft_kf_encode_fn      encode;
    uft_kf_decode_fn      decode;
    uft_kf_init_fn        init;
} uft_kf_encoding_method_t;

/**
 * 
 * These correspond to the -i parameter values.
 * Format: iN where N is the format code.
 */
typedef enum uft_kf_image_type {
    // Stream formats
    UFT_KF_IMG_CT_RAW           = 0,    ///< CT Raw (preservation)
    UFT_KF_IMG_UFT_KF_STREAM        = 1,    ///< KryoFlux Stream
    
    // Sector image formats
    UFT_KF_IMG_GENERIC_MFM      = 2,    ///< Generic MFM
    UFT_KF_IMG_GENERIC_FM       = 3,    ///< Generic FM
    
    // Specific platform formats
    UFT_KF_IMG_AMIGA_DD         = 4,    ///< Amiga DD (880KB)
    UFT_KF_IMG_AMIGA_HD         = 5,    ///< Amiga HD (1.76MB)
    UFT_KF_IMG_ATARI_ST_SS      = 6,    ///< Atari ST SS
    UFT_KF_IMG_ATARI_ST_DS      = 7,    ///< Atari ST DS
    UFT_KF_IMG_ATARI_ST_HD      = 8,    ///< Atari ST HD
    UFT_KF_IMG_APPLE_DOS        = 9,    ///< Apple DOS 3.x
    UFT_KF_IMG_APPLE_PRODOS     = 10,   ///< Apple ProDOS
    UFT_KF_IMG_APPLE_400K       = 11,   ///< Apple 400K
    UFT_KF_IMG_APPLE_800K       = 12,   ///< Apple 800K
    UFT_KF_IMG_CBM_1541         = 13,   ///< Commodore 1541
    UFT_KF_IMG_CBM_1571         = 14,   ///< Commodore 1571
    UFT_KF_IMG_CBM_1581         = 15,   ///< Commodore 1581
    UFT_KF_IMG_IBM_PC_DD        = 16,   ///< IBM PC DD
    UFT_KF_IMG_IBM_PC_HD        = 17,   ///< IBM PC HD
    UFT_KF_IMG_TRS80            = 18,   ///< TRS-80
    UFT_KF_IMG_SPECTRUM         = 19,   ///< ZX Spectrum
    
    // Extended formats (may vary by DTC version)
    UFT_KF_IMG_AMSTRAD          = 20,   ///< Amstrad CPC
    UFT_KF_IMG_MSX              = 21,   ///< MSX
    UFT_KF_IMG_BBC              = 22,   ///< BBC Micro
    UFT_KF_IMG_SAM_COUPE        = 23,   ///< Sam Coupe
    
    UFT_KF_IMG_MAX              = 64    ///< Maximum format number
} uft_kf_image_type_t;

/**
 * @brief DTC Command Line Options
 * 
 * Common options extracted from dtc binary.
 */
typedef struct uft_kf_dtc_options {
    // Track range
    int  start_track;       ///< -s<trk>
    int  end_track;         ///< -e<trk>
    
    // Drive settings
    int  drive_id;          ///< -d<id>
    int  side;              ///< -g<side>
    int  density_line;      ///< -dd<val>
    
    // Read settings
    int  retries;           ///< -t<try>
    double target_rpm;      ///< -v<rpm>
    int  calibration_mode;  ///< -c<mode>
    
    // Output
    int  output_level;      ///< -l<mask>
    int  image_type;        ///< -i<type>
    
    // Track 0 positions
    int  track0_side_a;     ///< -a<trk>
    int  track0_side_b;     ///< -b<trk>
    
    // Write settings
    bool write_mode;        ///< -w
    int  write_side;        ///< -g<side> for write
    int  precomp_ns;        ///< -ww<ns>
    int  erase_mode;        ///< -we<mode>
    
    // Plot settings
    int  plot_type;         ///< -pg<type>
    int  plot_height;       ///< -ph<size>
    double plot_x_origin;   ///< -px<fval>
    double plot_domain;     ///< -pd<fval>
    int  band_threshold;    ///< -ot<pct>
} uft_kf_dtc_options_t;

/**
 * @brief Default DTC options
 */
static inline uft_kf_dtc_options_t uft_kf_default_dtc_options(void) {
    return (uft_kf_dtc_options_t){
        .start_track = 0,
        .end_track = -1,        // Auto
        .drive_id = 0,
        .side = -1,             // Both
        .density_line = 0,
        .retries = 5,
        .target_rpm = 0,        // By image type
        .calibration_mode = 0,
        .output_level = 62,
        .image_type = 0,
        .track0_side_a = 0,
        .track0_side_b = 0,
        .write_mode = false,
        .write_side = -1,
        .precomp_ns = 0,        // Auto
        .erase_mode = 0,        // By bias
        .plot_type = 0,
        .plot_height = 600,
        .plot_x_origin = 0.0,
        .plot_domain = 0.0,     // Entire track
        .band_threshold = 30
    };
}

// ============================================================================
// Firmware Commands (from firmware_kf_usb_rosalie.bin)
// ============================================================================

/**
 */
typedef enum uft_kf_fw_command {
    UFT_KF_FW_CMD_STATUS,       ///< Get status
    UFT_KF_FW_CMD_INFO,         ///< Get info
    UFT_KF_FW_CMD_RESULT,       ///< Get result
    UFT_KF_FW_CMD_DATA,         ///< Data transfer
    UFT_KF_FW_CMD_INDEX,        ///< Index signal
    UFT_KF_FW_CMD_RESET,        ///< Reset device
    UFT_KF_FW_CMD_DEVICE,       ///< Device control
    UFT_KF_FW_CMD_MOTOR,        ///< Motor control
    UFT_KF_FW_CMD_DENSITY,      ///< Density select
    UFT_KF_FW_CMD_SIDE          ///< Side select
} uft_kf_fw_command_t;

#ifdef __cplusplus
}
#endif

#endif // UFT_UFT_UFT_KF_ALGORITHMS_H
