/**
 * @file uft_pauline.h
 * @brief Pauline Floppy Controller Support
 *
 * Pauline is a professional-grade floppy disk controller/analyzer
 * by Jean-François DEL NERO (HxC Floppy Emulator developer).
 *
 * Architecture (from github.com/jfdelnero/Pauline):
 * - DE10-nano FPGA board with custom floppy controller IP
 * - Embedded Linux with SSH / HTTP / FTP servers
 * - HxCFloppyEmulator library for format handling
 * - 50 MHz or 25 MHz capture resolution
 * - Up to 16 channels, 4 drive emulation
 *
 * Control: HTTP web server + SSH commands (NOT binary serial protocol)
 *
 * @see https://github.com/jfdelnero/Pauline
 * @copyright MIT License
 */

#ifndef UFT_PAULINE_H
#define UFT_PAULINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_PAULINE_DEFAULT_PORT        80       /* HTTP web server port */
#define UFT_PAULINE_MAX_TRACKS          84
#define UFT_PAULINE_MAX_HEADS           2
#define UFT_PAULINE_MAX_BUFFER          (16 * 1024 * 1024)

#define UFT_PAULINE_SAMPLE_RATE_25MHZ   25000000
#define UFT_PAULINE_SAMPLE_RATE_50MHZ   50000000

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct uft_pauline_device_s uft_pauline_device_t;

typedef struct {
    char device_name[64];
    char firmware_version[32];
    int  sample_rate;
    int  max_tracks;
    int  max_heads;
} uft_pauline_info_t;

typedef struct {
    bool connected;
    bool motor_on;
    bool write_protected;
    bool disk_present;
    int  current_track;
    int  current_head;
    int  current_drive;
} uft_pauline_status_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

uft_pauline_device_t* uft_pauline_create(void);
void uft_pauline_destroy(uft_pauline_device_t *dev);

/**
 * @brief Connect to Pauline via network
 * @param hostname IP address or hostname of Pauline device
 * @param port     HTTP port (0 for default 80)
 */
int  uft_pauline_open(uft_pauline_device_t *dev, const char *hostname, int port);
void uft_pauline_close(uft_pauline_device_t *dev);
bool uft_pauline_is_connected(const uft_pauline_device_t *dev);

/** Set SSH credentials (default: root with no key) */
int  uft_pauline_set_ssh_credentials(uft_pauline_device_t *dev,
                                     const char *user, const char *keyfile);

/* ============================================================================
 * Device Info / Status
 * ============================================================================ */

int uft_pauline_get_info(uft_pauline_device_t *dev, uft_pauline_info_t *info);
int uft_pauline_get_status(uft_pauline_device_t *dev, uft_pauline_status_t *status);

/* ============================================================================
 * Drive Control
 * ============================================================================ */

int uft_pauline_set_drive(uft_pauline_device_t *dev, int drive, int type);
int uft_pauline_motor_on(uft_pauline_device_t *dev);
int uft_pauline_motor_off(uft_pauline_device_t *dev);
int uft_pauline_seek(uft_pauline_device_t *dev, int track);
int uft_pauline_recalibrate(uft_pauline_device_t *dev);
int uft_pauline_select_head(uft_pauline_device_t *dev, int head);
int uft_pauline_set_sample_rate(uft_pauline_device_t *dev, int rate_hz);

/* ============================================================================
 * Capture / Write
 * ============================================================================ */

/**
 * @brief Read track flux as HxC Stream data
 * @param stream_data  Output: raw stream bytes (caller frees)
 * @param stream_len   Output: byte count
 */
int uft_pauline_read_track(uft_pauline_device_t *dev,
                           int track, int head, int revolutions,
                           uint8_t **stream_data, size_t *stream_len);

int uft_pauline_read_index(uft_pauline_device_t *dev,
                           uint32_t *index_times, size_t max_indices,
                           size_t *index_count);

int uft_pauline_write_track(uft_pauline_device_t *dev,
                            int track, int head,
                            const uint8_t *stream_data, size_t stream_len);

/* ============================================================================
 * Utilities
 * ============================================================================ */

double   uft_pauline_ticks_to_ns(const uft_pauline_device_t *dev, uint32_t ticks);
uint32_t uft_pauline_ns_to_ticks(const uft_pauline_device_t *dev, double ns);
const char* uft_pauline_get_error(const uft_pauline_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PAULINE_H */
