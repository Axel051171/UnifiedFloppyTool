/**
 * @file uft_xa1541.h
 * @brief XA1541/X1541 Series Legacy Parallel Port Adapter Interface
 * @version 4.1.1
 */

#ifndef UFT_XA1541_H
#define UFT_XA1541_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define UFT_ERR_PERMISSION     -101
#define UFT_ERR_NOT_CONNECTED  -102
#define UFT_ERR_NOT_SUPPORTED  -103

/* Cable types */
typedef enum {
    XA1541_TYPE_UNKNOWN = 0,
    XA1541_TYPE_X1541,      /**< Original passive cable */
    XA1541_TYPE_XE1541,     /**< Extended passive cable */
    XA1541_TYPE_XM1541,     /**< Multitask cable */
    XA1541_TYPE_XA1541,     /**< Active cable (recommended) */
    XA1541_TYPE_XAP1541,    /**< Active + Parallel cable */
    XA1541_TYPE_XA1541F,    /**< Active cable with FETs */
} xa1541_cable_type_t;

/* Transfer speed */
typedef enum {
    XA1541_SPEED_SLOW = 0,
    XA1541_SPEED_NORMAL,
    XA1541_SPEED_TURBO,
} xa1541_speed_t;

/* Output format */
typedef enum {
    XA1541_FORMAT_D64 = 0,
    XA1541_FORMAT_G64,
    XA1541_FORMAT_NIB,
} xa1541_format_t;

/* Cable info */
typedef struct {
    xa1541_cable_type_t type;
    const char *name;
    const char *description;
    bool active;
    bool parallel_capable;
    bool requires_bidi;
    xa1541_speed_t max_speed;
    const char *compatibility;
} xa1541_cable_info_t;

/* Parallel port info */
typedef struct {
    int port_number;
    uint16_t base_address;
    char device_path[64];
    char name[64];
    bool accessible;
} xa1541_port_info_t;

/* Device info */
typedef struct {
    bool connected;
    xa1541_cable_type_t cable_type;
    int port_number;
    uint16_t port_base;
    uint8_t drive_number;
    bool drive_detected;
    bool parallel_enabled;
    bool warp_mode;
    char drive_id[64];
    char cable_name[32];
    char cable_desc[64];
} xa1541_device_info_t;

/* Progress callback */
typedef bool (*xa1541_progress_cb)(uint32_t current, uint32_t total, void *user_data);

/* Opaque context */
typedef struct uft_xa1541_ctx uft_xa1541_ctx_t;

/* Port detection */
int uft_xa1541_detect_ports(xa1541_port_info_t *ports, int max_ports);

/* Context management */
uft_xa1541_ctx_t* uft_xa1541_create(void);
void uft_xa1541_destroy(uft_xa1541_ctx_t *ctx);

/* Connection */
int uft_xa1541_connect(uft_xa1541_ctx_t *ctx, int port_number,
                        xa1541_cable_type_t cable_type);
int uft_xa1541_disconnect(uft_xa1541_ctx_t *ctx);

/* Cable type */
int uft_xa1541_set_cable_type(uft_xa1541_ctx_t *ctx, xa1541_cable_type_t type);
const xa1541_cable_info_t* uft_xa1541_get_cable_info(xa1541_cable_type_t type);
int uft_xa1541_list_cable_types(xa1541_cable_info_t *cables, int max_cables);

/* Drive detection */
int uft_xa1541_detect_drive(uft_xa1541_ctx_t *ctx, uint8_t drive_num);
int uft_xa1541_get_drive_status(uft_xa1541_ctx_t *ctx, char *status,
                                 size_t status_size);

/* Disk operations */
int uft_xa1541_read_disk(uft_xa1541_ctx_t *ctx, const char *output_path,
                          xa1541_format_t format,
                          xa1541_progress_cb progress, void *user_data);
int uft_xa1541_write_disk(uft_xa1541_ctx_t *ctx, const char *input_path,
                           xa1541_progress_cb progress, void *user_data);

/* Configuration */
int uft_xa1541_set_parallel_mode(uft_xa1541_ctx_t *ctx, bool enable);
int uft_xa1541_set_warp_mode(uft_xa1541_ctx_t *ctx, bool enable);

/* Status */
int uft_xa1541_get_info(uft_xa1541_ctx_t *ctx, xa1541_device_info_t *info);
void uft_xa1541_get_statistics(uft_xa1541_ctx_t *ctx,
                                uint32_t *bytes, uint32_t *errors);
void uft_xa1541_reset_statistics(uft_xa1541_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XA1541_H */
