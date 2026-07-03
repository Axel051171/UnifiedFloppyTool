/**
 * @file uft_xum1541.h
 * @brief XUM1541/ZoomFloppy Interface for Commodore Drives
 * 
 * Supports direct communication with Commodore disk drives (1541, 1571, 1581)
 * via the XUM1541/ZoomFloppy USB adapter using the OpenCBM protocol.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Hardware abstraction (M3.2 in progress)
 *
 * This header declares 26 public functions. As of MASTER_PLAN.md §M3.2
 * partial scaffold:
 *   - 13 are REAL: uft_xum_drive_name, uft_xum_tracks_for_drive,
 *     uft_xum_sectors_for_track, uft_xum_config_create / _destroy /
 *     _is_connected, uft_xum_set_device / _track_range / _side / _retries,
 *     uft_xum_get_error, uft_xum_close (no-op safe), uft_xum_iec_write
 *     short-circuit on len==0.
 *   - 13 are HONEST STUBS: return UFT_ERR_NOT_IMPLEMENTED with a
 *     descriptive error string in cfg->last_error (uft_xum_get_error
 *     retrieves it). USB I/O + IEC bus + track read/write all wait
 *     for the libusb layer (M3.2 follow-up). Status-returning sigs are
 *     uft_error_t (matching SCP-Direct M3.1 + Applesauce M3.3 + rest
 *     of UFT).
 *
 * 16 tests in tests/test_xum1541_hal.c verify the real functions and
 * the stub honesty contract. When libusb wiring lands, the stubs flip
 * to real I/O without changing the API.
 *
 * Do NOT add new call sites to the stub functions without checking
 * the M3.2 status — they will currently always fail.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_XUM1541_H
#define UFT_XUM1541_H

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

/** Commodore drive types */
typedef enum {
    UFT_CBM_DRIVE_AUTO = 0,
    UFT_CBM_DRIVE_1541,     /**< Single-sided, 35 tracks, GCR */
    UFT_CBM_DRIVE_1541_II,  /**< Single-sided, 35 tracks, GCR */
    UFT_CBM_DRIVE_1570,     /**< Single-sided, 35 tracks */
    UFT_CBM_DRIVE_1571,     /**< Double-sided, 35 tracks, GCR */
    UFT_CBM_DRIVE_1581,     /**< Double-sided, 80 tracks, MFM, 3.5" */
    UFT_CBM_DRIVE_SFD1001,  /**< 77 tracks, 5.25" */
    UFT_CBM_DRIVE_8050,     /**< 77 tracks, dual drive */
    UFT_CBM_DRIVE_8250,     /**< 77 tracks, dual drive, DS */
} uft_cbm_drive_t;

/** IEC bus commands (Commodore serial bus, 8-bit opcodes). */
typedef enum {
    UFT_IEC_LISTEN      = 0x20,
    UFT_IEC_UNLISTEN    = 0x3F,
    UFT_IEC_TALK        = 0x40,
    UFT_IEC_UNTALK      = 0x5F,
    UFT_IEC_OPEN        = 0xF0,
    UFT_IEC_CLOSE       = 0xE0,
    UFT_IEC_DATA        = 0x60,
} uft_iec_cmd_t;

/* MF-255 (v4.1.5-hardening): XUM1541 / ZoomFloppy USB protocol.
 * VID/PID + endpoint + command codes verified against OpenCBM xum1541
 * firmware source (github.com/OpenCBM/OpenCBM xum1541_types.h). */
#define UFT_XUM1541_USB_VID           0x16D0
#define UFT_XUM1541_USB_PID           0x0504
#define UFT_XUM1541_BULK_IN_EP        0x83  /* bEndpointAddress 3 + IN bit */
#define UFT_XUM1541_BULK_OUT_EP       0x04  /* bEndpointAddress 4 + OUT bit */
#define UFT_XUM1541_MAX_XFER_SIZE     32768
#define UFT_XUM1541_CTRL_TIMEOUT_MS   1500

/* Control-transfer (bRequest) opcodes — sent via libusb_control_transfer. */
#define UFT_XUM1541_CTRL_ECHO              0
#define UFT_XUM1541_CTRL_INIT              1
#define UFT_XUM1541_CTRL_RESET             2
#define UFT_XUM1541_CTRL_SHUTDOWN          3
#define UFT_XUM1541_CTRL_ENTER_BOOTLOADER  4
#define UFT_XUM1541_CTRL_TAP_BREAK         5
#define UFT_XUM1541_CTRL_GITREV            6
#define UFT_XUM1541_CTRL_GCCVER            7
#define UFT_XUM1541_CTRL_LIBCVER           8

/* IOCTL commands (MF-301 OpenCBM re-audit): sent as 4-byte BULK-OUT
 * commands [cmd, arg1, arg2, 0] — NOT as control transfers. The device
 * answers with a 3-byte status on bulk IN. Values are XUM1541_IOCTL(16)+n
 * and were re-verified verbatim against OpenCBM xum1541_types.h. */
#define UFT_XUM1541_IOCTL_GET_EOI         23
#define UFT_XUM1541_IOCTL_CLEAR_EOI       24
#define UFT_XUM1541_IOCTL_PP_READ         25
#define UFT_XUM1541_IOCTL_PP_WRITE        26
#define UFT_XUM1541_IOCTL_IEC_POLL        27
#define UFT_XUM1541_IOCTL_IEC_WAIT        28
#define UFT_XUM1541_IOCTL_IEC_SETRELEASE  29
#define UFT_XUM1541_IOCTL_PARBURST_READ   30
#define UFT_XUM1541_IOCTL_PARBURST_WRITE  31

/* Bulk data commands (MF-301): the ONLY non-ioctl bulk opcodes the real
 * firmware knows are READ(8) and WRITE(9). The previous UFT constant
 * table (WRITE_DATA=0, TALK=1, LISTEN=2, UNLISTEN=3, UNTALK=4,
 * READ_DATA=7, OPEN=8, CLOSE=9) was FICTIONAL — worse, OPEN/CLOSE
 * collided with the real READ/WRITE numbers, so an 'open file' against
 * real silicon would have triggered a bus READ. IEC addressing
 * (talk/listen/untalk/unlisten) is NOT separate opcodes: it is a WRITE
 * with the ATN protocol flag and the raw IEC ATN bytes as payload —
 * exactly how OpenCBM's archlib.c does it. */
#define UFT_XUM1541_BULK_READ        8
#define UFT_XUM1541_BULK_WRITE       9

/* Bulk command header layout (4 bytes, verified against OpenCBM host
 * plugin xum1541.c): [opcode, proto|flags, size_lo, size_hi]. */

/* Protocol selector — upper nibble of header byte 1. */
#define UFT_XUM1541_PROTO_CBM        (1 << 4)  /* standard CBM serial  */
#define UFT_XUM1541_PROTO_S1         (2 << 4)
#define UFT_XUM1541_PROTO_S2         (3 << 4)
#define UFT_XUM1541_PROTO_PP         (4 << 4)
#define UFT_XUM1541_PROTO_P2         (5 << 4)
#define UFT_XUM1541_PROTO_NIB        (6 << 4)  /* burst nibbler        */

/* CBM-protocol flags — lower nibble of header byte 1. */
#define UFT_XUM1541_FLAG_WRITE_TALK  (1 << 0)
#define UFT_XUM1541_FLAG_WRITE_ATN   (1 << 1)

/* XUM1541 status response: 3 bytes on bulk IN (MF-301; was wrongly read
 * as 1 byte pre-audit): buf[0] = status code, buf[1..2] = little-endian
 * extended value (e.g. actual byte count for WRITE). */
#define UFT_XUM1541_STATUSBUF_SIZE  3
#define UFT_XUM1541_STATUS_BUSY   1
#define UFT_XUM1541_STATUS_READY  2
#define UFT_XUM1541_STATUS_ERROR  3
#define UFT_XUM1541_GET_STATUS(buf)     ((buf)[0])
#define UFT_XUM1541_GET_STATUS_VAL(buf) \
    ((uint16_t)(((uint16_t)(buf)[2] << 8) | (buf)[1]))

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct uft_xum_config_s uft_xum_config_t;

/**
 * @brief Track read result
 */
typedef struct {
    int track;              /**< Track (1-35/40/77) */
    int side;               /**< Side (0 or 1, 0 for 1541) */
    uint8_t *gcr_data;      /**< Raw GCR data */
    size_t gcr_size;        /**< GCR data size */
    uint8_t *decoded;       /**< Decoded sector data (optional) */
    size_t decoded_size;    /**< Decoded size */
    int sector_count;       /**< Number of sectors */
    uint8_t sector_errors[21]; /**< Error codes per sector */
    bool success;
    const char *error;
} uft_xum_track_t;

typedef int (*uft_xum_callback_t)(const uft_xum_track_t *track, void *user);

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_xum_config_t* uft_xum_config_create(void);
void uft_xum_config_destroy(uft_xum_config_t *cfg);

uft_error_t uft_xum_open(uft_xum_config_t *cfg, int device_num);
void        uft_xum_close(uft_xum_config_t *cfg);
bool        uft_xum_is_connected(const uft_xum_config_t *cfg);

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

uft_error_t uft_xum_detect(int *device_count);
uft_error_t uft_xum_identify_drive(uft_xum_config_t *cfg, uft_cbm_drive_t *type);
uft_error_t uft_xum_get_status(uft_xum_config_t *cfg, char *status, size_t max_len);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

uft_error_t uft_xum_set_device(uft_xum_config_t *cfg, int device_num);
uft_error_t uft_xum_set_track_range(uft_xum_config_t *cfg, int start, int end);
uft_error_t uft_xum_set_side(uft_xum_config_t *cfg, int side);
uft_error_t uft_xum_set_retries(uft_xum_config_t *cfg, int count);

/*============================================================================
 * CAPTURE
 *============================================================================*/

/**
 * @brief Read track (raw GCR)
 */
uft_error_t uft_xum_read_track_gcr(uft_xum_config_t *cfg, int track, int side,
                                    uint8_t **gcr, size_t *size);

/**
 * @brief Read track (decoded sectors)
 */
uft_error_t uft_xum_read_track(uft_xum_config_t *cfg, int track, int side,
                                uint8_t *sectors, int *sector_count,
                                uint8_t *errors);

/**
 * @brief Read entire disk
 */
uft_error_t uft_xum_read_disk(uft_xum_config_t *cfg, uft_xum_callback_t callback,
                               void *user);

/**
 * @brief Write track
 */
uft_error_t uft_xum_write_track(uft_xum_config_t *cfg, int track, int side,
                                 const uint8_t *data, size_t size);

/*============================================================================
 * LOW-LEVEL IEC
 *============================================================================*/

uft_error_t uft_xum_iec_listen(uft_xum_config_t *cfg, int device, int secondary);
uft_error_t uft_xum_iec_talk(uft_xum_config_t *cfg, int device, int secondary);
uft_error_t uft_xum_iec_unlisten(uft_xum_config_t *cfg);
uft_error_t uft_xum_iec_untalk(uft_xum_config_t *cfg);
uft_error_t uft_xum_iec_write(uft_xum_config_t *cfg, const uint8_t *data, size_t len);

/**
 * @brief Read IEC data (CBM serial) into @p data.
 *
 * @param bytes_read  Out: actual bytes received. MANDATORY (MF-301) —
 *        the XUM1541 legitimately ends transfers early on IEC EOI, and
 *        a caller that cannot distinguish a full from an EOI-shortened
 *        read loses forensically relevant length information.
 */
uft_error_t uft_xum_iec_read(uft_xum_config_t *cfg, uint8_t *data,
                              size_t max_len, size_t *bytes_read);

/**
 * @brief Poll the IEC bus line states (IOCTL 27, MF-301).
 * @param lines_out  Out: line bitmask (DATA/CLK/ATN/RESET per OpenCBM
 *                   iec_poll semantics), from the status extended value.
 */
uft_error_t uft_xum_iec_poll(uft_xum_config_t *cfg, uint8_t *lines_out);

/*============================================================================
 * UTILITIES
 *============================================================================*/

const char* uft_xum_get_error(const uft_xum_config_t *cfg);
const char* uft_xum_drive_name(uft_cbm_drive_t type);
int uft_xum_tracks_for_drive(uft_cbm_drive_t type);
int uft_xum_sectors_for_track(uft_cbm_drive_t type, int track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XUM1541_H */
