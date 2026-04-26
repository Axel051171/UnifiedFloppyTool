/**
 * @file uft_applesauce.h
 * @brief Applesauce Floppy Controller Interface
 * 
 * Applesauce is a premium Apple II/III disk controller that captures
 * flux data with high precision. Supports all Apple disk formats.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Hardware abstraction (M3.3 in progress)
 *
 * This header declares 20 public functions. As of MASTER_PLAN.md §M3.3
 * partial scaffold:
 *   - 13 are REAL: format_name, ticks_to_ns / ns_to_ticks /
 *     get_sample_clock (pure-math conversions), config_create / _destroy /
 *     _is_connected / _close (no-op safe), set_format / _drive /
 *     _track_range / _side / _revolutions (input-validating), get_error.
 *   - 7 are HONEST STUBS: return UFT_ERR_NOT_IMPLEMENTED with a
 *     descriptive error string in cfg->last_error. Serial-port I/O
 *     (115200 8N1, text protocol) and capture functions wait for the
 *     M3.3 follow-up. Status-returning sigs are uft_error_t (matching
 *     SCP-Direct M3.1 + XUM1541 M3.2 + rest of UFT).
 *
 * 17 tests in tests/test_applesauce_hal.c verify the real functions
 * and the stub honesty contract. When the serial layer lands, the
 * stubs flip to real I/O without changing the API.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_HAL_APPLESAUCE_H
#define UFT_HAL_APPLESAUCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** Applesauce sample clock: 8 MHz (125ns resolution) */
#define UFT_AS_SAMPLE_CLOCK     8000000

/** Supported Apple formats */
typedef enum {
    UFT_AS_FMT_AUTO = 0,
    UFT_AS_FMT_DOS32,           /**< Apple DOS 3.2 (13-sector) */
    UFT_AS_FMT_DOS33,           /**< Apple DOS 3.3 (16-sector) */
    UFT_AS_FMT_PRODOS,          /**< Apple ProDOS */
    UFT_AS_FMT_PASCAL,          /**< Apple Pascal */
    UFT_AS_FMT_CPM,             /**< Apple CP/M */
    UFT_AS_FMT_LISA,            /**< Apple Lisa */
    UFT_AS_FMT_MAC_400K,        /**< Macintosh 400K GCR */
    UFT_AS_FMT_MAC_800K,        /**< Macintosh 800K GCR */
    UFT_AS_FMT_APPLE3,          /**< Apple III SOS */
    UFT_AS_FMT_RAW,             /**< Raw flux capture */
} uft_as_format_t;

/** Drive configurations */
typedef enum {
    UFT_AS_DRIVE_525,           /**< 5.25" (Apple II) */
    UFT_AS_DRIVE_35,            /**< 3.5" (Macintosh, Apple IIgs) */
} uft_as_drive_t;

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct uft_as_config_s uft_as_config_t;

typedef struct {
    int track;
    int side;
    uint32_t *flux;
    size_t flux_count;
    uint8_t *nibbles;       /**< Decoded nibble data */
    size_t nibble_count;
    bool success;
    const char *error;
} uft_as_track_t;

typedef int (*uft_as_callback_t)(const uft_as_track_t *track, void *user);

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_as_config_t* uft_as_config_create(void);
void uft_as_config_destroy(uft_as_config_t *cfg);

uft_error_t uft_as_open(uft_as_config_t *cfg, const char *port);
void uft_as_close(uft_as_config_t *cfg);
bool uft_as_is_connected(const uft_as_config_t *cfg);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

uft_error_t uft_as_set_format(uft_as_config_t *cfg, uft_as_format_t format);
uft_error_t uft_as_set_drive(uft_as_config_t *cfg, uft_as_drive_t drive);
uft_error_t uft_as_set_track_range(uft_as_config_t *cfg, int start, int end);
uft_error_t uft_as_set_side(uft_as_config_t *cfg, int side);
uft_error_t uft_as_set_revolutions(uft_as_config_t *cfg, int revs);

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

uft_error_t uft_as_detect(char ports[][64], int max_ports);
uft_error_t uft_as_get_firmware_version(uft_as_config_t *cfg, char *version, size_t max_len);

/*============================================================================
 * CAPTURE
 *============================================================================*/

uft_error_t uft_as_read_track(uft_as_config_t *cfg, int track, int side,
                       uint32_t **flux, size_t *count);
uft_error_t uft_as_read_disk(uft_as_config_t *cfg, uft_as_callback_t callback, void *user);
uft_error_t uft_as_write_track(uft_as_config_t *cfg, int track, int side,
                        const uint32_t *flux, size_t count);

/*============================================================================
 * UTILITIES
 *============================================================================*/

double uft_as_ticks_to_ns(uint32_t ticks);
uint32_t uft_as_ns_to_ticks(double ns);
double uft_as_get_sample_clock(void);
const char* uft_as_get_error(const uft_as_config_t *cfg);
const char* uft_as_format_name(uft_as_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_APPLESAUCE_H */
