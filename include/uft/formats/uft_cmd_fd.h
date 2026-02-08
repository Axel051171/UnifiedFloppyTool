/**
 * @file uft_cmd_fd.h
 * @brief CMD FD2000/FD4000 Disk Image Format Interface
 * @version 4.1.0
 * 
 * D1M: 720KB DD disk (80 tracks, 2 sides, 9 sectors)
 * D2M: 1.44MB HD disk (80 tracks, 2 sides, 18 sectors)
 * D4M: 2.88MB ED disk (80 tracks, 2 sides, 36 sectors)
 */

#ifndef UFT_CMD_FD_H
#define UFT_CMD_FD_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

typedef enum {
    CMD_FD_UNKNOWN = 0,
    CMD_FD_D1M,     /**< 720KB DD */
    CMD_FD_D2M,     /**< 1.44MB HD */
    CMD_FD_D4M      /**< 2.88MB ED */
} cmd_fd_type_t;

typedef struct {
    cmd_fd_type_t   type;
    int             tracks;
    int             sides;
    int             sectors;
    int             sector_size;
    size_t          total_size;
    const char*     name;
    const char*     description;
} cmd_fd_geometry_t;

/* ============================================================================
 * Probe Functions
 * ============================================================================ */

bool uft_d1m_probe(const uint8_t *data, size_t size, int *confidence);
bool uft_d2m_probe(const uint8_t *data, size_t size, int *confidence);
bool uft_d4m_probe(const uint8_t *data, size_t size, int *confidence);

/* ============================================================================
 * Geometry Functions
 * ============================================================================ */

int uft_cmd_fd_get_geometry(cmd_fd_type_t type, cmd_fd_geometry_t *geom);

/* ============================================================================
 * Read Functions
 * ============================================================================ */

int uft_cmd_fd_read(const char *path, cmd_fd_type_t expected_type, 
                    uft_disk_image_t **out);
int uft_d1m_read(const char *path, uft_disk_image_t **out);
int uft_d2m_read(const char *path, uft_disk_image_t **out);
int uft_d4m_read(const char *path, uft_disk_image_t **out);

/* ============================================================================
 * Write Functions
 * ============================================================================ */

int uft_cmd_fd_write(const char *path, const uft_disk_image_t *disk,
                     cmd_fd_type_t type);
int uft_d1m_write(const char *path, const uft_disk_image_t *disk);
int uft_d2m_write(const char *path, const uft_disk_image_t *disk);
int uft_d4m_write(const char *path, const uft_disk_image_t *disk);

/* ============================================================================
 * Create Functions
 * ============================================================================ */

int uft_cmd_fd_create_blank(cmd_fd_type_t type, uft_disk_image_t **out);

/* ============================================================================
 * Info Functions
 * ============================================================================ */

int uft_cmd_fd_get_info(const char *path, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CMD_FD_H */
