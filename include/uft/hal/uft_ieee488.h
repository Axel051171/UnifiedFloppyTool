/**
 * @file uft_ieee488.h
 * @brief IEEE-488 Bus Support Interface
 * @version 4.1.0
 * 
 * Supports vintage CBM IEEE-488 drives:
 * - CBM 2031, 4040, 8050, 8250, SFD-1001
 */

#ifndef UFT_IEEE488_H
#define UFT_IEEE488_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

typedef enum {
    IEEE_DRIVE_UNKNOWN = 0,
    IEEE_DRIVE_2031,        /**< CBM 2031 LP (SS, 170KB) */
    IEEE_DRIVE_4040,        /**< CBM 4040 (DS, 340KB) */
    IEEE_DRIVE_8050,        /**< CBM 8050 (DS, 1MB) */
    IEEE_DRIVE_8250,        /**< CBM 8250 (DS, 2MB) */
    IEEE_DRIVE_SFD1001      /**< SFD-1001 (SS, 1MB) */
} ieee_drive_type_t;

typedef struct {
    bool connected;
    uint8_t address;
    uint8_t drive_number;
    ieee_drive_type_t type;
    char name[32];
    char description[64];
    int tracks;
    int sectors_max;
    int sector_size;
    size_t capacity;
} ieee_drive_info_result_t;

typedef struct {
    uint8_t address;
    ieee_drive_type_t type;
    char name[32];
} ieee_device_entry_t;

typedef struct {
    int count;
    ieee_device_entry_t devices[16];
} ieee_device_list_t;

/* Opaque context */
typedef struct uft_ieee488_ctx uft_ieee488_ctx_t;

/* ============================================================================
 * Context Management
 * ============================================================================ */

uft_ieee488_ctx_t* uft_ieee488_create(void);
void uft_ieee488_destroy(uft_ieee488_ctx_t *ctx);

/* ============================================================================
 * Connection Management
 * ============================================================================ */

int uft_ieee488_connect(uft_ieee488_ctx_t *ctx, const char *device_path,
                        uint8_t address);
int uft_ieee488_disconnect(uft_ieee488_ctx_t *ctx);

/* ============================================================================
 * Device Identification
 * ============================================================================ */

int uft_ieee488_identify(uft_ieee488_ctx_t *ctx);
int uft_ieee488_get_drive_info(uft_ieee488_ctx_t *ctx, 
                                ieee_drive_info_result_t *info);

/* ============================================================================
 * Low-Level Operations
 * ============================================================================ */

int uft_ieee488_send_command(uft_ieee488_ctx_t *ctx, const char *cmd);
int uft_ieee488_read_status(uft_ieee488_ctx_t *ctx, char *buffer, 
                            size_t buffer_size);

/* ============================================================================
 * Sector I/O
 * ============================================================================ */

int uft_ieee488_read_sector(uft_ieee488_ctx_t *ctx, int track, int sector,
                            uint8_t *buffer, size_t buffer_size);
int uft_ieee488_write_sector(uft_ieee488_ctx_t *ctx, int track, int sector,
                             const uint8_t *buffer, size_t buffer_size);

/* ============================================================================
 * Track I/O
 * ============================================================================ */

int uft_ieee488_read_track(uft_ieee488_ctx_t *ctx, int track, int head,
                           uint8_t *buffer, size_t buffer_size,
                           size_t *bytes_read);
int uft_ieee488_write_track(uft_ieee488_ctx_t *ctx, int track, int head,
                            const uint8_t *buffer, size_t buffer_size);

/* ============================================================================
 * Disk Operations
 * ============================================================================ */

int uft_ieee488_format_disk(uft_ieee488_ctx_t *ctx, const char *name,
                            const char *id);
int uft_ieee488_validate_disk(uft_ieee488_ctx_t *ctx);

/* ============================================================================
 * Device Discovery
 * ============================================================================ */

int uft_ieee488_scan_bus(ieee_device_list_t *devices);

/* ============================================================================
 * Utilities
 * ============================================================================ */

const char* uft_ieee488_drive_type_name(ieee_drive_type_t type);
int uft_ieee488_get_sectors_per_track(ieee_drive_type_t type, int track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IEEE488_H */
