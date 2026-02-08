/**
 * @file uft_tap.h
 * @brief TAP Raw Tape Image Format Support
 * 
 * Complete TAP format handling for C64/VIC-20/C16 Datasette:
 * - Parse TAP header and pulse data
 * - Extract pulse timings
 * - Create TAP from pulse data
 * - Analyze tape structure (pilot, sync, data blocks)
 * - Convert between TAP versions (v0, v1, v2)
 * 
 * TAP Format:
 * - 20-byte header (magic, version, platform, size)
 * - Pulse data (1 byte per pulse, or 4 bytes for long pulses)
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_TAP_H
#define UFT_TAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** TAP magic signature */
#define TAP_MAGIC               "C64-TAPE-RAW"
#define TAP_MAGIC_LEN           12

/** TAP header size */
#define TAP_HEADER_SIZE         20

/** TAP versions */
#define TAP_VERSION_0           0       /**< Original (no half-waves) */
#define TAP_VERSION_1           1       /**< Half-wave support */
#define TAP_VERSION_2           2       /**< Extended timing */

/** Platform/machine types */
#define TAP_MACHINE_C64         0       /**< C64 */
#define TAP_MACHINE_VIC20       1       /**< VIC-20 */
#define TAP_MACHINE_C16         2       /**< C16/Plus4 */

/** Pulse timing constants (PAL, in CPU cycles at 985248 Hz) */
#define TAP_CYCLES_PER_SECOND   985248  /**< C64 PAL clock */
#define TAP_SHORT_PULSE         0x30    /**< ~352 cycles (358µs) */
#define TAP_MEDIUM_PULSE        0x42    /**< ~480 cycles (487µs) */
#define TAP_LONG_PULSE          0x56    /**< ~624 cycles (633µs) */

/** Pulse type thresholds */
#define TAP_THRESHOLD_SHORT     0x38    /**< Below = short */
#define TAP_THRESHOLD_MEDIUM    0x4C    /**< Below = medium */

/** Special pulse values */
#define TAP_OVERFLOW_MARKER     0x00    /**< Overflow marker (v1+) */

/** Block types */
#define TAP_BLOCK_HEADER        0x01    /**< File header block */
#define TAP_BLOCK_DATA          0x02    /**< Data block */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief TAP file header
 */
typedef struct {
    char        magic[12];          /**< "C64-TAPE-RAW" */
    uint8_t     version;            /**< TAP version (0, 1, or 2) */
    uint8_t     machine;            /**< Machine type */
    uint8_t     video_standard;     /**< 0=PAL, 1=NTSC */
    uint8_t     reserved;           /**< Reserved (0) */
    uint32_t    data_size;          /**< Size of pulse data */
} tap_header_t;

/**
 * @brief Pulse types
 */
typedef enum {
    PULSE_SHORT = 0,                /**< Short pulse (bit 0) */
    PULSE_MEDIUM = 1,               /**< Medium pulse (bit 1) */
    PULSE_LONG = 2,                 /**< Long pulse (new data byte) */
    PULSE_PAUSE = 3                 /**< Long pause/silence */
} tap_pulse_type_t;

/**
 * @brief Single pulse info
 */
typedef struct {
    uint32_t    cycles;             /**< Duration in CPU cycles */
    tap_pulse_type_t type;          /**< Pulse type */
    size_t      file_offset;        /**< Offset in TAP file */
} tap_pulse_t;

/**
 * @brief Tape block info
 */
typedef struct {
    uint8_t     block_type;         /**< Block type (header/data) */
    size_t      start_offset;       /**< Start offset in TAP */
    size_t      end_offset;         /**< End offset in TAP */
    size_t      pilot_pulses;       /**< Number of pilot pulses */
    uint8_t     *data;              /**< Decoded data */
    size_t      data_size;          /**< Data size */
    uint8_t     checksum;           /**< Block checksum */
    bool        checksum_valid;     /**< Checksum matches */
} tap_block_t;

/**
 * @brief TAP analysis results
 */
typedef struct {
    int         num_blocks;         /**< Number of detected blocks */
    tap_block_t *blocks;            /**< Block array */
    size_t      total_pulses;       /**< Total pulse count */
    double      duration_seconds;   /**< Total duration */
    int         short_count;        /**< Short pulse count */
    int         medium_count;       /**< Medium pulse count */
    int         long_count;         /**< Long pulse count */
    int         pause_count;        /**< Pause count */
} tap_analysis_t;

/**
 * @brief TAP image context
 */
typedef struct {
    uint8_t     *data;              /**< TAP file data */
    size_t      size;               /**< Total file size */
    tap_header_t header;            /**< Parsed header */
    uint8_t     *pulses;            /**< Pulse data pointer */
    size_t      pulse_data_size;    /**< Pulse data size */
    bool        owns_data;          /**< true if we allocated data */
} tap_image_t;

/* ============================================================================
 * API Functions - Image Management
 * ============================================================================ */

/**
 * @brief Open TAP image from data
 * @param data TAP file data
 * @param size Data size
 * @param image Output image context
 * @return 0 on success
 */
int tap_open(const uint8_t *data, size_t size, tap_image_t *image);

/**
 * @brief Load TAP from file
 * @param filename File path
 * @param image Output image context
 * @return 0 on success
 */
int tap_load(const char *filename, tap_image_t *image);

/**
 * @brief Save TAP to file
 * @param image TAP image
 * @param filename Output path
 * @return 0 on success
 */
int tap_save(const tap_image_t *image, const char *filename);

/**
 * @brief Close TAP image
 * @param image Image to close
 */
void tap_close(tap_image_t *image);

/**
 * @brief Validate TAP format
 * @param data TAP data
 * @param size Data size
 * @return true if valid
 */
bool tap_validate(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is TAP format
 * @param data Data to check
 * @param size Data size
 * @return true if TAP
 */
bool tap_detect(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Pulse Operations
 * ============================================================================ */

/**
 * @brief Get pulse count
 * @param image TAP image
 * @return Number of pulses
 */
size_t tap_get_pulse_count(const tap_image_t *image);

/**
 * @brief Get pulse at index
 * @param image TAP image
 * @param index Pulse index
 * @param pulse Output pulse info
 * @return 0 on success
 */
int tap_get_pulse(const tap_image_t *image, size_t index, tap_pulse_t *pulse);

/**
 * @brief Get pulse cycles (duration)
 * @param image TAP image
 * @param offset Byte offset in pulse data
 * @param cycles Output: cycles
 * @param bytes_consumed Output: bytes used (1 or 4)
 * @return 0 on success
 */
int tap_read_pulse_cycles(const tap_image_t *image, size_t offset,
                          uint32_t *cycles, int *bytes_consumed);

/**
 * @brief Classify pulse by duration
 * @param cycles Pulse duration in cycles
 * @return Pulse type
 */
tap_pulse_type_t tap_classify_pulse(uint32_t cycles);

/**
 * @brief Get total duration
 * @param image TAP image
 * @return Duration in seconds
 */
double tap_get_duration(const tap_image_t *image);

/* ============================================================================
 * API Functions - Analysis
 * ============================================================================ */

/**
 * @brief Analyze TAP structure
 * @param image TAP image
 * @param analysis Output analysis
 * @return 0 on success
 */
int tap_analyze(const tap_image_t *image, tap_analysis_t *analysis);

/**
 * @brief Free analysis results
 * @param analysis Analysis to free
 */
void tap_free_analysis(tap_analysis_t *analysis);

/**
 * @brief Find pilot tone
 * @param image TAP image
 * @param start_offset Starting offset
 * @param pilot_start Output: pilot start
 * @param pilot_end Output: pilot end
 * @param pilot_pulses Output: pulse count
 * @return 0 if found
 */
int tap_find_pilot(const tap_image_t *image, size_t start_offset,
                   size_t *pilot_start, size_t *pilot_end, size_t *pilot_pulses);

/**
 * @brief Decode data block
 * @param image TAP image
 * @param start_offset Start of data
 * @param data Output buffer
 * @param max_size Maximum bytes to decode
 * @param decoded Output: bytes decoded
 * @return 0 on success
 */
int tap_decode_block(const tap_image_t *image, size_t start_offset,
                     uint8_t *data, size_t max_size, size_t *decoded);

/**
 * @brief Get pulse statistics
 * @param image TAP image
 * @param short_count Output: short pulses
 * @param medium_count Output: medium pulses
 * @param long_count Output: long pulses
 * @param pause_count Output: pauses
 */
void tap_get_statistics(const tap_image_t *image,
                        int *short_count, int *medium_count,
                        int *long_count, int *pause_count);

/* ============================================================================
 * API Functions - TAP Creation
 * ============================================================================ */

/**
 * @brief Create new TAP image
 * @param version TAP version (0, 1, or 2)
 * @param machine Machine type
 * @param image Output image
 * @return 0 on success
 */
int tap_create(uint8_t version, uint8_t machine, tap_image_t *image);

/**
 * @brief Add pulse to TAP
 * @param image TAP image
 * @param cycles Pulse duration in cycles
 * @return 0 on success
 */
int tap_add_pulse(tap_image_t *image, uint32_t cycles);

/**
 * @brief Add pilot tone
 * @param image TAP image
 * @param num_pulses Number of pulses
 * @param pulse_cycles Cycles per pulse
 * @return 0 on success
 */
int tap_add_pilot(tap_image_t *image, size_t num_pulses, uint32_t pulse_cycles);

/**
 * @brief Add sync sequence
 * @param image TAP image
 * @return 0 on success
 */
int tap_add_sync(tap_image_t *image);

/**
 * @brief Add data byte
 * @param image TAP image
 * @param byte Data byte to encode
 * @return 0 on success
 */
int tap_add_data_byte(tap_image_t *image, uint8_t byte);

/**
 * @brief Add data block with checksum
 * @param image TAP image
 * @param data Data to add
 * @param size Data size
 * @return 0 on success
 */
int tap_add_data_block(tap_image_t *image, const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Conversion
 * ============================================================================ */

/**
 * @brief Convert TAP version
 * @param image TAP image
 * @param new_version Target version
 * @return 0 on success
 */
int tap_convert_version(tap_image_t *image, uint8_t new_version);

/**
 * @brief Normalize pulse timings
 * @param image TAP image
 * @return Number of pulses modified
 */
int tap_normalize_pulses(tap_image_t *image);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get version name
 * @param version Version number
 * @return Version string
 */
const char *tap_version_name(uint8_t version);

/**
 * @brief Get machine name
 * @param machine Machine type
 * @return Machine string
 */
const char *tap_machine_name(uint8_t machine);

/**
 * @brief Convert cycles to microseconds
 * @param cycles CPU cycles
 * @return Microseconds
 */
double tap_cycles_to_us(uint32_t cycles);

/**
 * @brief Convert microseconds to cycles
 * @param us Microseconds
 * @return CPU cycles
 */
uint32_t tap_us_to_cycles(double us);

/**
 * @brief Print TAP info
 * @param image TAP image
 * @param fp Output file
 */
void tap_print_info(const tap_image_t *image, FILE *fp);

/**
 * @brief Print pulse histogram
 * @param image TAP image
 * @param fp Output file
 */
void tap_print_histogram(const tap_image_t *image, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TAP_H */
