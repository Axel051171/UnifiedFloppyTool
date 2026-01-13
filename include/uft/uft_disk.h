/**
 * @file uft_disk.h
 * @brief Disk Handle Definition
 * 
 * P0-002: Complete disk structure with writer backend support
 */

#ifndef UFT_DISK_H
#define UFT_DISK_H

#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct uft_writer_backend uft_writer_backend_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Complete disk handle
 */
struct uft_disk {
    /* Identity */
    char                path[512];          /* File path or device path */
    uft_format_t        format;             /* Image format */
    uft_encoding_t      encoding;           /* Data encoding */
    
    /* Geometry */
    uft_geometry_t      geometry;           /* Disk geometry */
    
    /* State */
    bool                is_open;
    bool                is_modified;
    bool                is_readonly;
    
    /* Backends */
    void                *reader_backend;    /* For reading */
    uft_writer_backend_t *writer_backend;   /* For writing */
    void                *hw_provider;       /* Hardware provider (if physical) */
    
    /* Data */
    uft_track_t         **tracks;           /* Track array */
    int                 track_count;
    
    /* Image buffer (for file-based images) */
    uint8_t             *image_data;
    size_t              image_size;
    
    /* Callbacks */
    uft_progress_fn     progress;
    void                *progress_user;
    uft_log_fn          log;
    void                *log_user;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create new disk handle
 */
uft_disk_t* uft_disk_create(void);

/**
 * @brief Open disk from file
 */
uft_error_t uft_disk_open(uft_disk_t *disk, const char *path, bool readonly);

/**
 * @brief Close disk
 */
void uft_disk_close(uft_disk_t *disk);

/**
 * @brief Free disk handle
 */
void uft_disk_free(uft_disk_t *disk);

/**
 * @brief Get disk geometry
 */
uft_error_t uft_disk_get_geometry(const uft_disk_t *disk, uft_geometry_t *geom);

/**
 * @brief Set writer backend
 */
void uft_disk_set_writer(uft_disk_t *disk, uft_writer_backend_t *backend);

/**
 * @brief Get writer backend
 */
uft_writer_backend_t* uft_disk_get_writer(uft_disk_t *disk);

/**
 * @brief Save disk to file
 */
uft_error_t uft_disk_save(uft_disk_t *disk, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_H */
