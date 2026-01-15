/**
 * @file uft_flux_decoder.h
 * @brief UFT Flux Decoder - MFM/FM/GCR decoding extracted from RIDE
 * 
 * This module provides industrial-grade flux decoding algorithms ported from
 * Tomas Nestorovic's RIDE (Real and Imaginary Disk Editor).
 * 
 * Key features:
 * - Multiple decoder algorithms (Simple, PLL-Fixed, PLL-Adaptive, Keir Fraser)
 * - MFM/FM encoding support with proper sync detection
 * - Multi-revolution sector comparison and merging
 * - Track mining for data recovery from damaged media
 * - SCP/KryoFlux stream format support
 * 
 * @copyright Original RIDE code: (c) Tomas Nestorovic
 * @copyright KryoFlux support based on Simon Owen's SamDisk
 * @license MIT for this UFT port
 * 
 * Integration: UnifiedFloppyTool v3.8.0+
 * Author: UFT Team / Claude Integration
 */

#ifndef UFT_FLUX_DECODER_H
#define UFT_FLUX_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS AND TIMING DEFINITIONS
 *============================================================================*/

/** Time unit: nanoseconds */
typedef int32_t uft_logtime_t;

/** Maximum revolutions supported */
#define UFT_REVOLUTION_MAX          8

/** Maximum sectors per track */
#define UFT_SECTORS_MAX             64

/** Maximum cylinders */
#define UFT_CYLINDERS_MAX           84

/** Flux buffer default capacity */
#define UFT_FLUX_BUFFER_CAPACITY    2000000

/** KryoFlux sample clock (nanoseconds per sample) */
#define UFT_KF_SAMPLE_CLOCK_NS      41667  /* ~24 MHz */

/** Greaseweazle sample clock (nanoseconds per sample) */
#define UFT_GW_SAMPLE_CLOCK_NS      125    /* 8 MHz (F1xx) */

/*----------------------------------------------------------------------------
 * MFM Timing Constants (DD = Double Density, 250 kbit/s)
 *----------------------------------------------------------------------------*/
#define UFT_MFM_CELL_DD_NS          4000   /* 4µs per bit cell */
#define UFT_MFM_CELL_HD_NS          2000   /* 2µs per bit cell (HD) */
#define UFT_MFM_CELL_ED_NS          1000   /* 1µs per bit cell (ED) */

#define UFT_MFM_SHORT_FLUX_DD_NS    4000   /* 2T = 4µs */
#define UFT_MFM_MEDIUM_FLUX_DD_NS   6000   /* 3T = 6µs */
#define UFT_MFM_LONG_FLUX_DD_NS     8000   /* 4T = 8µs */

#define UFT_MFM_TOLERANCE_PCT       15     /* ±15% timing tolerance */

/*----------------------------------------------------------------------------
 * FM Timing Constants (SD = Single Density, 125 kbit/s)
 *----------------------------------------------------------------------------*/
#define UFT_FM_CELL_SD_NS           8000   /* 8µs per bit cell */
#define UFT_FM_SHORT_FLUX_SD_NS     8000   /* 2T = 8µs */
#define UFT_FM_LONG_FLUX_SD_NS      16000  /* 4T = 16µs */

/*----------------------------------------------------------------------------
 * Sync Patterns
 *----------------------------------------------------------------------------*/
#define UFT_MFM_SYNC_A1             0x4489 /* A1 with missing clock */
#define UFT_MFM_SYNC_C2             0x5224 /* C2 with missing clock */
#define UFT_FM_SYNC_F8F9            0xF8F9 /* FM sync pattern */

/*----------------------------------------------------------------------------
 * Address Marks
 *----------------------------------------------------------------------------*/
#define UFT_AM_IDAM                 0xFE   /* ID Address Mark */
#define UFT_AM_DAM                  0xFB   /* Data Address Mark */
#define UFT_AM_DDAM                 0xF8   /* Deleted Data Address Mark */
#define UFT_AM_IAM                  0xFC   /* Index Address Mark */

/*============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Encoding type
 */
typedef enum {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_FM,             /**< Single density (FM) */
    UFT_ENC_MFM,            /**< Double/High density (MFM) */
    UFT_ENC_GCR_APPLE,      /**< Apple II GCR */
    UFT_ENC_GCR_C64,        /**< Commodore 64 GCR */
    UFT_ENC_GCR_AMIGA,      /**< Amiga MFM variant */
    UFT_ENC_RLL             /**< Hard disk RLL */
} uft_encoding_t;

/**
 * @brief Media density
 */
typedef enum {
    UFT_DENSITY_UNKNOWN = 0,
    UFT_DENSITY_SD,         /**< Single Density (FM, 125 kbit/s) */
    UFT_DENSITY_DD,         /**< Double Density (MFM, 250 kbit/s) */
    UFT_DENSITY_QD,         /**< Quad Density (MFM, 500 kbit/s, 96 tpi) */
    UFT_DENSITY_HD,         /**< High Density (MFM, 500 kbit/s) */
    UFT_DENSITY_ED          /**< Extended Density (MFM, 1 Mbit/s) */
} uft_density_t;

/**
 * @brief Decoder algorithm selection
 * 
 * Based on RIDE's TDecoderMethod enumeration
 */
typedef enum {
    UFT_DECODER_NONE = 0,
    UFT_DECODER_SIMPLE,         /**< Simple threshold decoder */
    UFT_DECODER_PLL_FIXED,      /**< PLL with fixed frequency */
    UFT_DECODER_PLL_ADAPTIVE,   /**< PLL with adaptive frequency (DPLL) */
    UFT_DECODER_KEIR_FRASER,    /**< Keir Fraser's algorithm (Greaseweazle) */
    UFT_DECODER_MARK_OGDEN      /**< Mark Ogden's algorithm */
} uft_decoder_algo_t;

/**
 * @brief FDC status register bits (ST1)
 */
typedef enum {
    UFT_FDC_ST1_END_OF_CYL      = 0x80,
    UFT_FDC_ST1_DATA_ERROR      = 0x20,
    UFT_FDC_ST1_NO_DATA         = 0x04,
    UFT_FDC_ST1_NO_AM           = 0x01
} uft_fdc_st1_t;

/**
 * @brief FDC status register bits (ST2)
 */
typedef enum {
    UFT_FDC_ST2_DELETED_DAM     = 0x40,
    UFT_FDC_ST2_CRC_ERROR       = 0x20,
    UFT_FDC_ST2_NOT_DAM         = 0x01
} uft_fdc_st2_t;

/*============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief FDC status (combined ST1/ST2)
 */
typedef struct {
    uint8_t reg1;           /**< ST1 register */
    uint8_t reg2;           /**< ST2 register */
} uft_fdc_status_t;

/**
 * @brief Time interval
 */
typedef struct {
    uft_logtime_t start;    /**< Start time (ns) */
    uft_logtime_t end;      /**< End time (ns) */
} uft_time_interval_t;

/**
 * @brief Flux transition buffer
 * 
 * Stores raw flux transition times from stream files or devices.
 */
typedef struct {
    uft_logtime_t  *times;          /**< Flux transition times (ns) */
    size_t          count;          /**< Number of transitions */
    size_t          capacity;       /**< Buffer capacity */
    uft_logtime_t  *index_times;    /**< Index pulse times */
    uint8_t         index_count;    /**< Number of index pulses */
    uint32_t        sample_clock;   /**< Sample clock period (ns) */
    uft_encoding_t  detected_enc;   /**< Auto-detected encoding */
    uft_density_t   detected_den;   /**< Auto-detected density */
} uft_flux_buffer_t;

/**
 * @brief PLL (Phase-Locked Loop) state
 * 
 * Used by adaptive decoders for clock recovery.
 */
typedef struct {
    double      frequency;      /**< Current PLL frequency (normalized) */
    double      phase;          /**< Current phase */
    double      gain_p;         /**< Proportional gain */
    double      gain_i;         /**< Integral gain */
    double      error_integral; /**< Accumulated error */
    uint32_t    clock_period;   /**< Nominal clock period (ns) */
    uint32_t    window_min;     /**< Minimum inspection window */
    uint32_t    window_max;     /**< Maximum inspection window */
    uint32_t    window_current; /**< Current inspection window size */
} uft_pll_state_t;

/**
 * @brief Inspection window profile
 * 
 * From RIDE's TIwProfile structure.
 */
typedef struct {
    uft_logtime_t   iw_default;     /**< Default inspection window size */
    uft_logtime_t   iw_current;     /**< Current inspection window size */
    uft_logtime_t   iw_min;         /**< Minimum inspection window */
    uft_logtime_t   iw_max;         /**< Maximum inspection window */
    uint8_t         tolerance_pct;  /**< Tolerance percentage */
} uft_iw_profile_t;

/**
 * @brief Sector ID (CHRN)
 */
typedef struct {
    uint8_t cylinder;       /**< Cylinder number */
    uint8_t head;           /**< Head/side number */
    uint8_t sector;         /**< Sector number */
    uint8_t size_code;      /**< Size code (0=128, 1=256, 2=512...) */
} uft_sector_id_t;

/**
 * @brief Sector header information
 */
typedef struct {
    uft_sector_id_t     id;             /**< Sector ID (CHRN) */
    uint16_t            header_crc;     /**< Header CRC */
    bool                header_crc_ok;  /**< Header CRC valid */
    uft_logtime_t       id_end_time;    /**< Time at end of ID field */
    uft_logtime_t       gap2_time;      /**< Gap2 duration */
} uft_sector_header_t;

/**
 * @brief Complete sector information
 */
typedef struct {
    uft_sector_header_t header;         /**< Sector header */
    uint8_t             data_mark;      /**< DAM/DDAM */
    uint16_t            data_crc;       /**< Data CRC */
    bool                data_crc_ok;    /**< Data CRC valid */
    uft_logtime_t       data_start;     /**< Data field start time */
    uft_logtime_t       data_end;       /**< Data field end time */
    uint16_t            data_size;      /**< Data size in bytes */
    uint8_t            *data;           /**< Sector data (allocated) */
    uint8_t             revolution;     /**< Source revolution */
    float               confidence;     /**< Quality confidence (0.0-1.0) */
    uft_fdc_status_t    fdc_status;     /**< FDC status */
} uft_sector_t;

/**
 * @brief Track information
 */
typedef struct {
    uint8_t             cylinder;       /**< Physical cylinder */
    uint8_t             head;           /**< Physical head */
    uft_encoding_t      encoding;       /**< Track encoding */
    uft_density_t       density;        /**< Track density */
    uint32_t            bit_rate;       /**< Bit rate (bits/sec) */
    uint32_t            track_length;   /**< Track length in bits */
    uft_logtime_t       index_time;     /**< Index-to-index time */
    uint8_t             sector_count;   /**< Number of sectors found */
    uft_sector_t        sectors[UFT_SECTORS_MAX]; /**< Sector array */
    uint8_t             revolution_count; /**< Revolutions decoded */
    bool                consistent;     /**< All revolutions match */
    uint8_t             healthy_sectors; /**< Sectors with good CRC */
    uint8_t             bad_sectors;    /**< Sectors with errors */
    bool                modified;       /**< Track has been modified */
} uft_track_t;

/**
 * @brief Decoder configuration
 */
typedef struct {
    uft_decoder_algo_t  algorithm;      /**< Decoder algorithm */
    uft_encoding_t      encoding;       /**< Expected encoding */
    uft_density_t       density;        /**< Expected density */
    bool                reset_on_index; /**< Reset decoder at index */
    uint8_t             revolutions;    /**< Revolutions to read */
    uint32_t            clock_period;   /**< Override clock period (0=auto) */
    float               pll_gain_p;     /**< PLL proportional gain */
    float               pll_gain_i;     /**< PLL integral gain */
    bool                strict_timing;  /**< Strict timing mode */
} uft_decoder_config_t;

/**
 * @brief Mining target configuration
 */
typedef struct {
    uint8_t     min_healthy;            /**< Minimum healthy sectors required */
    bool        require_all_headers;    /**< All headers must be readable */
    bool        require_all_data;       /**< All data must be readable */
    uint32_t    timeout_ms;             /**< Mining timeout (milliseconds) */
    uint32_t    max_retries;            /**< Maximum read retries */
    uint8_t     head_calibration_every; /**< Head calibration interval */
    bool        stop_on_success;        /**< Stop when target reached */
} uft_mining_target_t;

/**
 * @brief Mining result
 */
typedef struct {
    bool        success;                /**< Target achieved */
    uint32_t    attempts;               /**< Number of attempts */
    uint32_t    elapsed_ms;             /**< Time spent */
    uint8_t     best_healthy;           /**< Best healthy sector count */
    uint8_t     best_revolution;        /**< Best revolution index */
    uft_track_t best_track;             /**< Best track result */
} uft_mining_result_t;

/**
 * @brief Parse event types (from RIDE)
 */
typedef enum {
    UFT_PE_GAP,
    UFT_PE_SYNC,
    UFT_PE_IDAM,
    UFT_PE_ID_FIELD,
    UFT_PE_GAP2,
    UFT_PE_DAM,
    UFT_PE_DDAM,
    UFT_PE_DATA_FIELD,
    UFT_PE_GAP3,
    UFT_PE_CRC_OK,
    UFT_PE_CRC_BAD,
    UFT_PE_FUZZY,
    UFT_PE_UNKNOWN
} uft_parse_event_type_t;

/**
 * @brief Parse event for track analysis
 */
typedef struct {
    uft_parse_event_type_t  type;
    uft_time_interval_t     interval;
    uint8_t                 data[16];   /**< Event-specific data */
    uint16_t                data_len;
} uft_parse_event_t;

/*============================================================================
 * FLUX BUFFER MANAGEMENT
 *============================================================================*/

/**
 * @brief Create a new flux buffer
 * @param capacity Maximum number of flux transitions
 * @return Allocated flux buffer or NULL on error
 */
uft_flux_buffer_t *uft_flux_create(size_t capacity);

/**
 * @brief Free a flux buffer
 * @param flux Flux buffer to free
 */
void uft_flux_free(uft_flux_buffer_t *flux);

/**
 * @brief Add a flux transition
 * @param flux Flux buffer
 * @param time_ns Transition time in nanoseconds
 * @return true on success
 */
bool uft_flux_add_transition(uft_flux_buffer_t *flux, uft_logtime_t time_ns);

/**
 * @brief Mark an index pulse
 * @param flux Flux buffer
 * @param time_ns Index pulse time in nanoseconds
 */
void uft_flux_mark_index(uft_flux_buffer_t *flux, uft_logtime_t time_ns);

/**
 * @brief Get flux transition count
 */
size_t uft_flux_count(const uft_flux_buffer_t *flux);

/**
 * @brief Get revolution count
 */
uint8_t uft_flux_revolutions(const uft_flux_buffer_t *flux);

/**
 * @brief Get total track time
 */
uft_logtime_t uft_flux_total_time(const uft_flux_buffer_t *flux);

/*============================================================================
 * STREAM FILE I/O
 *============================================================================*/

/**
 * @brief Load flux from SCP file
 * @param filename SCP file path
 * @param cylinder Cylinder to load
 * @param head Head to load
 * @return Flux buffer or NULL on error
 */
uft_flux_buffer_t *uft_flux_load_scp(const char *filename, 
                                      uint8_t cylinder, uint8_t head);

/**
 * @brief Load flux from KryoFlux stream files
 * @param base_path Base path (without extension)
 * @param cylinder Cylinder to load
 * @param head Head to load
 * @return Flux buffer or NULL on error
 */
uft_flux_buffer_t *uft_flux_load_kryoflux(const char *base_path,
                                           uint8_t cylinder, uint8_t head);

/**
 * @brief Load flux from Greaseweazle stream
 * @param data Raw stream data
 * @param size Data size
 * @param sample_clock Sample clock period (ns)
 * @return Flux buffer or NULL on error
 */
uft_flux_buffer_t *uft_flux_load_greaseweazle(const uint8_t *data, size_t size,
                                               uint32_t sample_clock);

/**
 * @brief Save flux to SCP file
 * @param flux Flux buffer
 * @param filename Output filename
 * @return true on success
 */
bool uft_flux_save_scp(const uft_flux_buffer_t *flux, const char *filename);

/*============================================================================
 * PLL FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize PLL state
 * @param pll PLL state structure
 * @param clock_period Nominal clock period in nanoseconds
 */
void uft_pll_init(uft_pll_state_t *pll, uint32_t clock_period);

/**
 * @brief Process a flux transition through PLL
 * @param pll PLL state
 * @param flux_time Flux transition time (delta from last)
 * @param bits_out Output bit buffer (must hold at least 4 bytes)
 * @return Number of bits decoded
 */
int uft_pll_process(uft_pll_state_t *pll, uft_logtime_t flux_time, 
                    uint8_t *bits_out);

/**
 * @brief Reset PLL to initial state
 * @param pll PLL state
 */
void uft_pll_reset(uft_pll_state_t *pll);

/**
 * @brief Set PLL gains
 * @param pll PLL state
 * @param gain_p Proportional gain (typical: 0.5-2.0)
 * @param gain_i Integral gain (typical: 0.0-0.5)
 */
void uft_pll_set_gains(uft_pll_state_t *pll, float gain_p, float gain_i);

/*============================================================================
 * TRACK DECODING
 *============================================================================*/

/**
 * @brief Initialize decoder configuration with defaults
 * @param config Configuration structure to initialize
 */
void uft_decoder_init_default(uft_decoder_config_t *config);

/**
 * @brief Decode a track from flux data
 * @param flux Flux buffer
 * @param config Decoder configuration
 * @param track Output track structure
 * @return true on success (may still have bad sectors)
 */
bool uft_decode_track(const uft_flux_buffer_t *flux,
                      const uft_decoder_config_t *config,
                      uft_track_t *track);

/**
 * @brief Decode MFM bits to bytes
 * @param bits MFM bit stream
 * @param bit_count Number of bits
 * @param data Output data buffer
 * @param data_size Data buffer size
 * @return Number of bytes decoded
 */
size_t uft_decode_mfm_bits(const uint8_t *bits, size_t bit_count,
                           uint8_t *data, size_t data_size);

/**
 * @brief Encode bytes to MFM bits
 * @param data Input data
 * @param data_size Data size
 * @param bits Output bit buffer
 * @param max_bits Maximum bits to write
 * @return Number of bits encoded
 */
size_t uft_encode_mfm_bits(const uint8_t *data, size_t data_size,
                           uint8_t *bits, size_t max_bits);

/**
 * @brief Decode FM bits to bytes
 * @param bits FM bit stream
 * @param bit_count Number of bits
 * @param data Output data buffer
 * @param data_size Data buffer size
 * @return Number of bytes decoded
 */
size_t uft_decode_fm_bits(const uint8_t *bits, size_t bit_count,
                          uint8_t *data, size_t data_size);

/**
 * @brief Calculate CRC-CCITT
 * @param data Data buffer
 * @param len Data length
 * @return CRC-CCITT value
 */
uint16_t uft_crc_ccitt(const uint8_t *data, size_t len);

/**
 * @brief Verify sector CRC
 * @param sector Sector to verify
 * @return true if CRC is valid
 */
bool uft_sector_verify(const uft_sector_t *sector);

/**
 * @brief Free sector data
 * @param sector Sector to free
 */
void uft_sector_free(uft_sector_t *sector);

/**
 * @brief Free track data
 * @param track Track to free
 */
void uft_track_free(uft_track_t *track);

/*============================================================================
 * TRACK MINING (DATA RECOVERY)
 *============================================================================*/

/**
 * @brief Initialize mining target with defaults
 * @param target Mining target structure
 */
void uft_mining_init_default(uft_mining_target_t *target);

/**
 * @brief Mine a track for data recovery
 * @param device Device handle (implementation-specific)
 * @param cyl Cylinder number
 * @param head Head number
 * @param target Mining target parameters
 * @param config Decoder configuration
 * @param result Mining result (output)
 * @param progress_cb Progress callback (may be NULL)
 * @return true if target achieved
 * 
 * Mining repeatedly reads a track until the target conditions are met
 * or timeout/max_retries is reached.
 */
bool uft_mine_track(void *device, uint8_t cyl, uint8_t head,
                    const uft_mining_target_t *target,
                    const uft_decoder_config_t *config,
                    uft_mining_result_t *result,
                    void (*progress_cb)(int attempt, int best_healthy, void *ctx),
                    void *cb_ctx);

/**
 * @brief Cancel ongoing mining operation
 */
void uft_mine_cancel(void);

/**
 * @brief Merge multiple revolutions to recover best sectors
 * @param tracks Array of track structures (one per revolution)
 * @param count Number of revolutions
 * @param output Merged output track
 * @return true on success
 */
bool uft_merge_revolutions(const uft_track_t *tracks, size_t count,
                           uft_track_t *output);

/*============================================================================
 * ENCODING/DENSITY DETECTION
 *============================================================================*/

/**
 * @brief Auto-detect encoding from flux data
 * @param flux Flux buffer
 * @return Detected encoding type
 */
uft_encoding_t uft_detect_encoding(const uft_flux_buffer_t *flux);

/**
 * @brief Auto-detect density from flux data
 * @param flux Flux buffer
 * @return Detected density
 */
uft_density_t uft_detect_density(const uft_flux_buffer_t *flux);

/**
 * @brief Create flux histogram
 * @param flux Flux buffer
 * @param bins Output histogram bins
 * @param bin_count Number of bins
 * @param bin_width Bin width in nanoseconds
 */
void uft_flux_histogram(const uft_flux_buffer_t *flux,
                        uint32_t *bins, size_t bin_count, uint32_t bin_width);

/*============================================================================
 * TRACK ANALYSIS
 *============================================================================*/

/**
 * @brief Analyze sector timing
 * @param track Track to analyze
 * @param sector_times Output sector times (array of sector_count)
 * @param gap_times Output gap times (array of sector_count)
 */
void uft_analyze_timing(const uft_track_t *track,
                        uint32_t *sector_times, uint32_t *gap_times);

/**
 * @brief Compare two tracks
 * @param a First track
 * @param b Second track
 * @return 0 if identical, negative if a<b, positive if a>b
 */
int uft_compare_tracks(const uft_track_t *a, const uft_track_t *b);

/**
 * @brief Scan track and extract parse events
 * @param flux Flux buffer
 * @param config Decoder config
 * @param events Output event array
 * @param max_events Maximum events
 * @return Number of events found
 */
size_t uft_scan_parse_events(const uft_flux_buffer_t *flux,
                              const uft_decoder_config_t *config,
                              uft_parse_event_t *events, size_t max_events);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get sector size from size code
 * @param code Size code (0-7)
 * @return Sector size in bytes (128 << code)
 */
uint16_t uft_sector_size_from_code(uint8_t code);

/**
 * @brief Get size code from sector size
 * @param size Sector size in bytes
 * @return Size code (0-7)
 */
uint8_t uft_size_code_from_size(uint16_t size);

/**
 * @brief Get encoding name string
 * @param enc Encoding type
 * @return Static string
 */
const char *uft_encoding_name(uft_encoding_t enc);

/**
 * @brief Get density name string
 * @param den Density type
 * @return Static string
 */
const char *uft_density_name(uft_density_t den);

/**
 * @brief Get decoder algorithm name
 * @param algo Algorithm type
 * @return Static string
 */
const char *uft_decoder_name(uft_decoder_algo_t algo);

/**
 * @brief Format sector status string
 * @param sector Sector to describe
 * @param buffer Output buffer
 * @param size Buffer size
 */
void uft_sector_status_string(const uft_sector_t *sector,
                               char *buffer, size_t size);

/**
 * @brief Format track summary string
 * @param track Track to describe
 * @param buffer Output buffer
 * @param size Buffer size
 */
void uft_track_summary_string(const uft_track_t *track,
                               char *buffer, size_t size);

/**
 * @brief Check if FDC status indicates error
 * @param status FDC status
 * @return true if any error bit set
 */
bool uft_fdc_has_error(const uft_fdc_status_t *status);

/**
 * @brief Check if FDC status indicates missing ID
 * @param status FDC status
 * @return true if ID not found
 */
bool uft_fdc_missing_id(const uft_fdc_status_t *status);

/**
 * @brief Check if FDC status indicates CRC error
 * @param status FDC status
 * @return true if CRC error in ID or data
 */
bool uft_fdc_crc_error(const uft_fdc_status_t *status);

/*============================================================================
 * SCP/KRYOFLUX EXTENDED API
 *============================================================================*/

/**
 * @brief SCP file information structure
 */
typedef struct {
    uint8_t  version;           /**< SCP version */
    uint8_t  disk_type;         /**< Disk type code */
    uint8_t  revolutions;       /**< Revolutions per track */
    uint8_t  start_track;       /**< First track */
    uint8_t  end_track;         /**< Last track */
    uint8_t  flags;             /**< SCP flags */
    uint8_t  heads;             /**< 0=both, 1=side0, 2=side1 */
    uint32_t resolution_ns;     /**< Time resolution in ns */
    char     disk_type_str[32]; /**< Human-readable disk type */
} uft_scp_info_t;

/**
 * @brief Get SCP file information
 * @param path Path to SCP file
 * @param info Output info structure
 * @return 0 on success, -1 on error
 */
int uft_scp_get_info(const char *path, uft_scp_info_t *info);

/**
 * @brief Build KryoFlux track filename
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param base_path Directory containing raw files
 * @param track Track number
 * @param side Side (0 or 1)
 */
void uft_kryoflux_build_filename(char *buf, size_t buf_size,
                                  const char *base_path,
                                  int track, int side);

/**
 * @brief Load single KryoFlux raw file
 * @param path Full path to .raw file
 * @return Flux buffer or NULL on error
 */
uft_flux_buffer_t *uft_flux_load_kryoflux_file(const char *path);

/**
 * @brief Auto-detect flux encoding from timing histogram
 * @param flux Flux buffer to analyze (encoding/density updated)
 */
void uft_flux_detect_encoding(uft_flux_buffer_t *flux);

/**
 * @brief Convert flux buffer to bitstream
 * @param flux Source flux buffer
 * @param bits Output bit buffer
 * @param bits_capacity Capacity in bytes
 * @param bits_written Actual bytes written
 * @return 0 on success
 */
int uft_flux_to_bitstream(const uft_flux_buffer_t *flux,
                           uint8_t *bits, size_t bits_capacity,
                           size_t *bits_written);

/**
 * @brief Get revolution data from flux buffer
 * @param flux Flux buffer
 * @param rev Revolution index (0-based)
 * @param start_idx Output: first flux index
 * @param end_idx Output: last flux index
 * @param duration_ns Output: revolution duration
 * @return 0 on success
 */
int uft_flux_get_revolution(const uft_flux_buffer_t *flux, int rev,
                             size_t *start_idx, size_t *end_idx,
                             uft_logtime_t *duration_ns);

/*============================================================================
 * GCR DECODER API
 *============================================================================*/

/**
 * @brief Decode Apple II sector from GCR nibbles
 */
int uft_gcr_decode_apple2_sector(const uint8_t *nibbles, size_t count,
                                  uint8_t *data, size_t data_size,
                                  uft_sector_t *sector);

/**
 * @brief Get sectors per track for C64 track number
 */
int uft_c64_sectors_per_track(int track);

/**
 * @brief Decode C64 sector from GCR data
 */
int uft_gcr_decode_c64_sector(const uint8_t *gcr_data, size_t gcr_len,
                               uint8_t *data, size_t data_size,
                               uft_sector_t *sector);

/**
 * @brief Decode entire Apple II track
 */
int uft_gcr_decode_apple2_track(const uft_flux_buffer_t *flux,
                                 uft_track_t *track);

/**
 * @brief Decode entire C64 track
 */
int uft_gcr_decode_c64_track(const uft_flux_buffer_t *flux,
                              int track_num,
                              uft_track_t *track);

/*============================================================================
 * HFE FORMAT API
 *============================================================================*/

/**
 * @brief HFE file information
 */
typedef struct {
    uint8_t  version;           /**< Format version (1 or 3) */
    uint8_t  tracks;            /**< Number of tracks */
    uint8_t  sides;             /**< Number of sides */
    uint8_t  encoding;          /**< Track encoding */
    uint16_t bitrate;           /**< Bitrate in kbit/s */
    uint16_t rpm;               /**< Rotation speed */
    uint8_t  interface_mode;    /**< Interface mode */
    bool     write_allowed;     /**< Write allowed */
    char     encoding_str[16];  /**< Encoding name */
    char     interface_str[32]; /**< Interface name */
} uft_hfe_info_t;

/**
 * @brief Get HFE file information
 */
int uft_hfe_get_info(const char *path, uft_hfe_info_t *info);

/**
 * @brief Load HFE track into flux buffer
 */
uft_flux_buffer_t *uft_flux_load_hfe(const char *path, int track, int side);

/**
 * @brief Write flux buffer to HFE format
 */
int uft_flux_write_hfe(const char *path, 
                        uft_flux_buffer_t **tracks,
                        int num_tracks, int num_sides);

/*============================================================================
 * UDI (ULTRA DISK IMAGE) FORMAT API
 *============================================================================*/

/**
 * @brief UDI file information
 */
typedef struct {
    uint8_t  version;           /**< Format version */
    uint8_t  cylinders;         /**< Number of cylinders */
    uint8_t  heads;             /**< Number of heads (1 or 2) */
    uint32_t file_size;         /**< File size */
    bool     crc_valid;         /**< CRC validation result */
    uint32_t stored_crc;        /**< CRC from file */
    uint32_t calculated_crc;    /**< Calculated CRC */
} uft_udi_info_t;

/**
 * @brief UDI track data for writing
 */
typedef struct {
    uint8_t *data;          /**< Track data */
    size_t   length;        /**< Data length */
    uint8_t *sync_map;      /**< Sync byte bitmap */
    size_t   sync_len;      /**< Sync map length */
} uft_udi_track_data_t;

/**
 * @brief Get UDI file information
 * @param path Path to UDI file
 * @param info Output info structure
 * @return 0 on success, -1 on error
 */
int uft_udi_get_info(const char *path, uft_udi_info_t *info);

/**
 * @brief Load UDI track data
 * @param path Path to UDI file
 * @param cylinder Cylinder to load
 * @param head Head to load
 * @param track_data Output track data (caller must free)
 * @param track_len Output track length
 * @param sync_map Output sync byte bitmap (caller must free, may be NULL)
 * @param sync_len Output sync map length
 * @return 0 on success, -1 on error
 */
int uft_udi_load_track(const char *path, int cylinder, int head,
                        uint8_t **track_data, size_t *track_len,
                        uint8_t **sync_map, size_t *sync_len);

/**
 * @brief Write UDI file from track data
 * @param path Output file path
 * @param tracks Array of track data [cylinder][head]
 * @param num_cylinders Number of cylinders
 * @param num_heads Number of heads (1 or 2)
 * @return 0 on success, -1 on error
 */
int uft_udi_write(const char *path, 
                   uft_udi_track_data_t tracks[][2],
                   int num_cylinders, int num_heads);

/**
 * @brief Convert MFM bitstream to UDI track format
 * @param mfm_data MFM bitstream data
 * @param mfm_bits Number of bits
 * @param track_data Output track data (caller must free)
 * @param track_len Output track length
 * @param sync_map Output sync bitmap (caller must free)
 * @param sync_len Output sync map length
 * @return 0 on success, -1 on error
 */
int uft_mfm_to_udi_track(const uint8_t *mfm_data, size_t mfm_bits,
                          uint8_t **track_data, size_t *track_len,
                          uint8_t **sync_map, size_t *sync_len);

/**
 * @brief Extract sectors from UDI track data
 * @param track_data UDI track data
 * @param track_len Track data length
 * @param sync_map Sync byte bitmap (may be NULL)
 * @param sectors Output sector array (caller allocated)
 * @param max_sectors Maximum sectors to extract
 * @return Number of sectors found, or -1 on error
 */
int uft_udi_extract_sectors(const uint8_t *track_data, size_t track_len,
                             const uint8_t *sync_map,
                             uft_sector_t *sectors, int max_sectors);

/*============================================================================
 * FORMAT VERIFICATION API
 *============================================================================*/

/**
 * @brief Unified format verification result
 */
typedef struct {
    const char *format_name;
    bool valid;
    int error_code;
    char details[256];
} uft_verify_result_t;

/**
 * @brief Verify disk image file (auto-detect format)
 */
int uft_verify_image(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify UDI file integrity
 * @param path Path to UDI file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_udi(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify HFE file integrity
 * 
 * Validates HFE v1/v3 disk images by checking:
 * - Magic signature
 * - Header field consistency
 * - Track list validity
 * - File size consistency
 * 
 * @param path Path to HFE file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_hfe(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify IMG/IMA raw sector image
 * 
 * Validates by checking file size matches known geometry
 * and optionally verifying boot sector structure.
 * 
 * @param path Path to IMG/IMA file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_img(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify Commodore D71 (1571) disk image
 * 
 * Validates BAM structure, DOS version, and directory chain.
 * 
 * @param path Path to D71 file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_d71(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify Commodore D81 (1581) disk image
 * 
 * Validates header, BAM structure, and DOS version.
 * 
 * @param path Path to D81 file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_d81(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify Atari ST raw disk image
 * 
 * Validates file size against known ST geometries and
 * checks boot sector BPB structure.
 * 
 * @param path Path to ST file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_st(const char *path, uft_verify_result_t *result);

/**
 * @brief Verify Atari ST MSA compressed image
 * 
 * Validates MSA magic and header parameters.
 * 
 * @param path Path to MSA file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_msa(const char *path, uft_verify_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_DECODER_H */
