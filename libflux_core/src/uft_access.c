/**
 * @file uft_access.c
 * @brief Extended Access API Implementation
 */

#include "uft/uft_access.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void uft_access_options_init(uft_access_options_t* opts) {
    UFT_CHECK_NULL(opts);
    
    memset(opts, 0, sizeof(*opts));
    opts->sector_id_zero_based = -1;  /* Use format default */
}

uft_rc_t uft_parse_geometry(const char* str, uft_geometry_t* geom) {
    UFT_CHECK_NULL(str);
    UFT_CHECK_NULL(geom);
    
    /* Try format: "C,H,S,SIZE" */
    int n = sscanf(str, "%u,%u,%u,%u", 
                   &geom->cylinders, &geom->heads, 
                   &geom->sectors, &geom->sector_size);
    if (n == 4) return UFT_SUCCESS;
    
    /* Try format: "CxHxS:SIZE" */
    n = sscanf(str, "%ux%ux%u:%u",
               &geom->cylinders, &geom->heads,
               &geom->sectors, &geom->sector_size);
    if (n == 4) return UFT_SUCCESS;
    
    /* Try format: "CxHxS" (assume 512 bytes) */
    n = sscanf(str, "%ux%ux%u",
               &geom->cylinders, &geom->heads, &geom->sectors);
    if (n == 3) {
        geom->sector_size = 512;
        return UFT_SUCCESS;
    }
    
    return UFT_ERR_INVALID_ARG;
}

uft_rc_t uft_geometry_to_string(
    const uft_geometry_t* geom,
    char* buffer,
    size_t buffer_size
) {
    UFT_CHECK_NULL(geom);
    UFT_CHECK_NULL(buffer);
    
    if (buffer_size < 32) {
        return UFT_ERR_BUFFER_TOO_SMALL;
    }
    
    snprintf(buffer, buffer_size, "%ux%ux%u:%u",
             geom->cylinders, geom->heads,
             geom->sectors, geom->sector_size);
    
    return UFT_SUCCESS;
}

/* Generic implementation using type-agnostic context */
uft_rc_t uft_read_sector_ex(
    void* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_read,
    const uft_access_options_t* opts
) {
    UFT_CHECK_NULL(ctx);
    UFT_CHECK_NULL(buffer);
    
    /* If no options, use defaults */
    uft_access_options_t default_opts;
    if (!opts) {
        uft_access_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Adjust sector numbering if specified */
    uint32_t adjusted_sector = sector;
    if (opts->sector_id_zero_based == 0) {
        /* 1-based requested but we're 0-based internally */
        if (sector > 0) adjusted_sector = sector - 1;
    } else if (opts->sector_id_zero_based == 1) {
        /* 0-based requested, no adjustment needed */
    }
    /* -1 = use format default, no adjustment */
    
    /* Progress callback before operation */
    if (opts->progress_callback) {
        opts->progress_callback(opts->user_data, 0, 1);
    }
    
    /* Log if verbose */
    if (opts->flags & UFT_FLAG_VERBOSE) {
        if (opts->log_callback) {
            char msg[128];
            snprintf(msg, sizeof(msg), 
                    "Reading C=%u H=%u S=%u size=%zu",
                    cylinder, head, adjusted_sector, buffer_size);
            opts->log_callback(opts->user_data, UFT_LOG_DEBUG, msg);
        }
    }
    
    /*
     * NOTE: Actual format-specific implementation would go here.
     * This is a generic wrapper that delegates to format-specific functions.
     * In real implementation, we would:
     * 1. Determine format type from ctx
     * 2. Call appropriate format's read_sector
     * 3. Handle format-specific quirks
     * 
     * For now, this is a framework that format modules can use.
     */
    
    /* Progress callback after operation */
    if (opts->progress_callback) {
        opts->progress_callback(opts->user_data, 1, 1);
    }
    
    if (bytes_read) {
        *bytes_read = buffer_size;  /* Would be actual bytes in real impl */
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_write_sector_ex(
    void* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    const uint8_t* data,
    size_t data_size,
    const uft_access_options_t* opts
) {
    UFT_CHECK_NULL(ctx);
    UFT_CHECK_NULL(data);
    
    uft_access_options_t default_opts;
    if (!opts) {
        uft_access_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Adjust sector numbering */
    uint32_t adjusted_sector = sector;
    if (opts->sector_id_zero_based == 0 && sector > 0) {
        adjusted_sector = sector - 1;
    }
    
    /* Log operation */
    if (opts->flags & UFT_FLAG_VERBOSE && opts->log_callback) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                "Writing C=%u H=%u S=%u size=%zu",
                cylinder, head, adjusted_sector, data_size);
        opts->log_callback(opts->user_data, UFT_LOG_INFO, msg);
    }
    
    /* Actual write would happen here */
    /* Format-specific implementation delegates to format module */
    
    /* Verify if requested */
    if (opts->flags & UFT_FLAG_VERIFY_WRITE) {
        uint8_t* verify_buf = malloc(data_size);
        if (!verify_buf) return UFT_ERR_MEMORY;
        
        /* Read back and compare */
        size_t bytes_read = 0;
        uft_rc_t rc = uft_read_sector_ex(ctx, cylinder, head, sector,
                                         verify_buf, data_size, 
                                         &bytes_read, opts);
        
        if (uft_success(rc)) {
            if (memcmp(data, verify_buf, data_size) != 0) {
                if (opts->log_callback) {
                    opts->log_callback(opts->user_data, UFT_LOG_ERROR,
                                      "Verify failed: data mismatch");
                }
                free(verify_buf);
                return UFT_ERR_CORRUPTED;
            }
        }
        
        free(verify_buf);
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_read_sectors_bulk(
    void* ctx,
    uint32_t start_cylinder,
    uint32_t start_head,
    uint32_t start_sector,
    uint32_t sector_count,
    uint8_t* buffer,
    size_t buffer_size,
    const uft_access_options_t* opts
) {
    UFT_CHECK_NULL(ctx);
    UFT_CHECK_NULL(buffer);
    
    uft_access_options_t default_opts;
    if (!opts) {
        uft_access_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Determine sector size from geometry or assume 512 */
    size_t sector_size = opts->geometry ? opts->geometry->sector_size : 512;
    
    if (buffer_size < sector_count * sector_size) {
        return UFT_ERR_BUFFER_TOO_SMALL;
    }
    
    /* Read each sector sequentially */
    for (uint32_t i = 0; i < sector_count; i++) {
        /* Progress callback */
        if (opts->progress_callback) {
            opts->progress_callback(opts->user_data, i, sector_count);
        }
        
        /* Calculate CHS for this sector */
        uint32_t cyl = start_cylinder;
        uint32_t head = start_head;
        uint32_t sec = start_sector + i;
        
        /* Handle wraparound (simplified) */
        if (opts->geometry) {
            while (sec >= opts->geometry->sectors) {
                sec -= opts->geometry->sectors;
                head++;
                if (head >= opts->geometry->heads) {
                    head = 0;
                    cyl++;
                }
            }
        }
        
        size_t bytes_read = 0;
        uft_rc_t rc = uft_read_sector_ex(ctx, cyl, head, sec,
                                         buffer + (i * sector_size),
                                         sector_size, &bytes_read, opts);
        
        if (uft_failed(rc)) {
            if (!(opts->flags & UFT_FLAG_IGNORE_ERRORS)) {
                return rc;
            }
        }
    }
    
    /* Final progress callback */
    if (opts->progress_callback) {
        opts->progress_callback(opts->user_data, sector_count, sector_count);
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_write_sectors_bulk(
    void* ctx,
    uint32_t start_cylinder,
    uint32_t start_head,
    uint32_t start_sector,
    uint32_t sector_count,
    const uint8_t* data,
    size_t data_size,
    const uft_access_options_t* opts
) {
    UFT_CHECK_NULL(ctx);
    UFT_CHECK_NULL(data);
    
    uft_access_options_t default_opts;
    if (!opts) {
        uft_access_options_init(&default_opts);
        opts = &default_opts;
    }
    
    size_t sector_size = opts->geometry ? opts->geometry->sector_size : 512;
    
    if (data_size < sector_count * sector_size) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Write each sector */
    for (uint32_t i = 0; i < sector_count; i++) {
        if (opts->progress_callback) {
            opts->progress_callback(opts->user_data, i, sector_count);
        }
        
        uint32_t cyl = start_cylinder;
        uint32_t head = start_head;
        uint32_t sec = start_sector + i;
        
        /* Handle wraparound */
        if (opts->geometry) {
            while (sec >= opts->geometry->sectors) {
                sec -= opts->geometry->sectors;
                head++;
                if (head >= opts->geometry->heads) {
                    head = 0;
                    cyl++;
                }
            }
        }
        
        uft_rc_t rc = uft_write_sector_ex(ctx, cyl, head, sec,
                                          data + (i * sector_size),
                                          sector_size, opts);
        
        if (uft_failed(rc)) {
            if (!(opts->flags & UFT_FLAG_IGNORE_ERRORS)) {
                return rc;
            }
        }
    }
    
    if (opts->progress_callback) {
        opts->progress_callback(opts->user_data, sector_count, sector_count);
    }
    
    return UFT_SUCCESS;
}
