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

#ifndef UFT_APPLESAUCE_H
#define UFT_APPLESAUCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

int uft_as_open(uft_as_config_t *cfg, const char *port);
void uft_as_close(uft_as_config_t *cfg);
bool uft_as_is_connected(const uft_as_config_t *cfg);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_as_set_format(uft_as_config_t *cfg, uft_as_format_t format);
int uft_as_set_drive(uft_as_config_t *cfg, uft_as_drive_t drive);
int uft_as_set_track_range(uft_as_config_t *cfg, int start, int end);
int uft_as_set_side(uft_as_config_t *cfg, int side);
int uft_as_set_revolutions(uft_as_config_t *cfg, int revs);

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_as_detect(char ports[][64], int max_ports);
int uft_as_get_firmware_version(uft_as_config_t *cfg, char *version, size_t max_len);

/*============================================================================
 * CAPTURE
 *============================================================================*/

int uft_as_read_track(uft_as_config_t *cfg, int track, int side,
                       uint32_t **flux, size_t *count);
int uft_as_read_disk(uft_as_config_t *cfg, uft_as_callback_t callback, void *user);
int uft_as_write_track(uft_as_config_t *cfg, int track, int side,
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

#endif /* UFT_APPLESAUCE_H */
