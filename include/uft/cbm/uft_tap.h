/**
 * @file uft_tap.h
 * @brief C64/C128/VIC-20 TAP Tape Image Format Support
 * @version 1.0.0
 * 
 * TAP is the standard format for C64 tape images, storing pulse durations.
 * 
 * Format Structure:
 * - 20-byte header with magic "C64-TAPE-RAW"
 * - Version byte (0 or 1)
 * - Data size (32-bit LE)
 * - Pulse data
 * 
 * Pulse Encoding:
 * - Version 0: byte * 8 cycles, 0x00 = long pulse (undetermined)
 * - Version 1: byte * 8 cycles, 0x00 + 3 bytes = exact cycle count
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TAP_H
#define UFT_TAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** TAP file magic bytes */
#define UFT_TAP_MAGIC           "C64-TAPE-RAW"
#define UFT_TAP_MAGIC_LEN       12

/** TAP header size */
#define UFT_TAP_HEADER_SIZE     20

/** Maximum pulse value for short encoding */
#define UFT_TAP_SHORT_MAX       (255 * 8)

/** PAL C64 clock frequency (Hz) */
#define UFT_TAP_PAL_CLOCK       985248

/** NTSC C64 clock frequency (Hz) */
#define UFT_TAP_NTSC_CLOCK      1022727

/** Standard pulse thresholds (cycles) for CBM ROM loader */
#define UFT_TAP_SHORT_PULSE     352     /* 0 bit */
#define UFT_TAP_MEDIUM_PULSE    512     /* 1 bit */
#define UFT_TAP_LONG_PULSE      672     /* Sync/Header */

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TAP format status codes
 */
typedef enum uft_tap_status {
    UFT_TAP_OK = 0,             /**< Success */
    UFT_TAP_E_INVALID = 1,      /**< Invalid parameter */
    UFT_TAP_E_TRUNC = 2,        /**< Truncated data */
    UFT_TAP_E_MAGIC = 3,        /**< Invalid magic bytes */
    UFT_TAP_E_VERSION = 4,      /**< Unsupported version */
    UFT_TAP_E_EOF = 5,          /**< End of file reached */
    UFT_TAP_E_ALLOC = 6         /**< Memory allocation failed */
} uft_tap_status_t;

/**
 * @brief TAP machine type
 */
typedef enum uft_tap_machine {
    UFT_TAP_C64 = 0,            /**< C64 (default) */
    UFT_TAP_VIC20 = 1,          /**< VIC-20 */
    UFT_TAP_C16 = 2,            /**< C16/Plus4 */
    UFT_TAP_C128 = 3            /**< C128 */
} uft_tap_machine_t;

/**
 * @brief TAP video standard
 */
typedef enum uft_tap_video {
    UFT_TAP_PAL = 0,            /**< PAL (Europe) */
    UFT_TAP_NTSC = 1            /**< NTSC (USA) */
} uft_tap_video_t;

/**
 * @brief TAP file header
 */
typedef struct uft_tap_header {
    uint8_t     magic[12];      /**< "C64-TAPE-RAW" */
    uint8_t     version;        /**< 0 or 1 */
    uint8_t     machine;        /**< Machine type (extended) */
    uint8_t     video;          /**< Video standard (extended) */
    uint8_t     reserved;       /**< Reserved */
    uint32_t    data_size;      /**< Size of pulse data */
} uft_tap_header_t;

/**
 * @brief TAP file view (read-only access)
 */
typedef struct uft_tap_view {
    const uint8_t*  data;           /**< Raw TAP data */
    size_t          data_size;      /**< Total data size */
    uft_tap_header_t header;        /**< Parsed header */
    size_t          pulse_offset;   /**< Offset to first pulse */
    size_t          pulse_count;    /**< Number of pulses (cached) */
} uft_tap_view_t;

/**
 * @brief TAP pulse iterator
 */
typedef struct uft_tap_iter {
    const uft_tap_view_t*   tap;        /**< Parent TAP view */
    size_t                  position;   /**< Current byte position */
    uint32_t                pulse_num;  /**< Current pulse number */
} uft_tap_iter_t;

/**
 * @brief TAP pulse info
 */
typedef struct uft_tap_pulse {
    uint32_t    cycles;         /**< Duration in CPU cycles */
    double      microseconds;   /**< Duration in microseconds */
    bool        is_long;        /**< True if long pulse (version 0 ambiguous) */
} uft_tap_pulse_t;

/**
 * @brief TAP creation context
 */
typedef struct uft_tap_writer {
    uint8_t*    buffer;         /**< Output buffer */
    size_t      capacity;       /**< Buffer capacity */
    size_t      position;       /**< Current write position */
    uint8_t     version;        /**< TAP version to create */
    uint8_t     machine;        /**< Machine type */
    uint8_t     video;          /**< Video standard */
} uft_tap_writer_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data is a TAP file
 * 
 * @param data      Data to check
 * @param size      Data size
 * @return          true if data appears to be a TAP file
 */
bool uft_tap_detect(const uint8_t* data, size_t size);

/**
 * @brief Get confidence score for TAP detection
 * 
 * @param data      Data to check
 * @param size      Data size
 * @return          Confidence 0-100
 */
int uft_tap_detect_confidence(const uint8_t* data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Reading Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open a TAP file for reading
 * 
 * @param data      TAP file data
 * @param size      Data size
 * @param view      Output view
 * @return          Status code
 */
uft_tap_status_t uft_tap_open(const uint8_t* data, size_t size,
                              uft_tap_view_t* view);

/**
 * @brief Get header information
 * 
 * @param view      TAP view
 * @return          Pointer to header
 */
const uft_tap_header_t* uft_tap_get_header(const uft_tap_view_t* view);

/**
 * @brief Get estimated pulse count
 * 
 * @param view      TAP view
 * @return          Number of pulses
 */
size_t uft_tap_get_pulse_count(const uft_tap_view_t* view);

/**
 * @brief Initialize pulse iterator
 * 
 * @param view      TAP view
 * @param iter      Output iterator
 * @return          Status code
 */
uft_tap_status_t uft_tap_iter_begin(const uft_tap_view_t* view,
                                    uft_tap_iter_t* iter);

/**
 * @brief Get next pulse
 * 
 * @param iter      Iterator
 * @param pulse     Output pulse info
 * @return          UFT_TAP_OK on success, UFT_TAP_E_EOF at end
 */
uft_tap_status_t uft_tap_iter_next(uft_tap_iter_t* iter,
                                   uft_tap_pulse_t* pulse);

/**
 * @brief Check if iterator has more pulses
 * 
 * @param iter      Iterator
 * @return          true if more pulses available
 */
bool uft_tap_iter_has_next(const uft_tap_iter_t* iter);

/**
 * @brief Get pulse at specific index
 * 
 * @param view      TAP view
 * @param index     Pulse index
 * @param pulse     Output pulse info
 * @return          Status code
 */
uft_tap_status_t uft_tap_get_pulse(const uft_tap_view_t* view,
                                   uint32_t index,
                                   uft_tap_pulse_t* pulse);

/* ═══════════════════════════════════════════════════════════════════════════
 * Writing Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize TAP writer
 * 
 * @param writer    Writer context
 * @param version   TAP version (0 or 1)
 * @return          Status code
 */
uft_tap_status_t uft_tap_writer_init(uft_tap_writer_t* writer,
                                     uint8_t version);

/**
 * @brief Add pulse to TAP
 * 
 * @param writer    Writer context
 * @param cycles    Pulse duration in CPU cycles
 * @return          Status code
 */
uft_tap_status_t uft_tap_writer_add_pulse(uft_tap_writer_t* writer,
                                          uint32_t cycles);

/**
 * @brief Finalize and get TAP data
 * 
 * @param writer    Writer context
 * @param out_data  Output data pointer (caller must free)
 * @param out_size  Output data size
 * @return          Status code
 */
uft_tap_status_t uft_tap_writer_finish(uft_tap_writer_t* writer,
                                       uint8_t** out_data,
                                       size_t* out_size);

/**
 * @brief Free writer resources
 * 
 * @param writer    Writer context
 */
void uft_tap_writer_free(uft_tap_writer_t* writer);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert cycles to microseconds
 * 
 * @param cycles    CPU cycles
 * @param video     Video standard
 * @return          Duration in microseconds
 */
double uft_tap_cycles_to_us(uint32_t cycles, uft_tap_video_t video);

/**
 * @brief Convert microseconds to cycles
 * 
 * @param us        Duration in microseconds
 * @param video     Video standard
 * @return          CPU cycles
 */
uint32_t uft_tap_us_to_cycles(double us, uft_tap_video_t video);

/**
 * @brief Classify pulse as short/medium/long
 * 
 * @param cycles    Pulse duration in cycles
 * @return          0=short, 1=medium, 2=long, -1=invalid
 */
int uft_tap_classify_pulse(uint32_t cycles);

/**
 * @brief Get machine name string
 * 
 * @param machine   Machine type
 * @return          Name string
 */
const char* uft_tap_machine_name(uft_tap_machine_t machine);

/**
 * @brief Get video standard name
 * 
 * @param video     Video standard
 * @return          Name string
 */
const char* uft_tap_video_name(uft_tap_video_t video);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TAP_H */
