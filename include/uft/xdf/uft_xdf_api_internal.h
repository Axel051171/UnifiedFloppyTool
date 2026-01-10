/**
 * @file uft_xdf_api_internal.h
 * @brief XDF API Internal Structures
 * 
 * Internal header for XDF API implementation files.
 * Contains struct definitions that need to be shared between
 * uft_xdf_api.c and uft_xdf_api_impl.c
 */

#ifndef UFT_XDF_API_INTERNAL_H
#define UFT_XDF_API_INTERNAL_H

#include "uft_xdf_api.h"
#include "uft_xdf_core.h"
#include <stdarg.h>
#include <stdio.h>    /* vsnprintf */
#include <string.h>   /* strrchr, strcmp, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Internal Constants
 *===========================================================================*/

#define XDF_MAX_FORMATS     64
#define XDF_ERROR_BUF_SIZE  512
#define BATCH_MAX_FILES     1024

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief XDF API context structure
 */
struct xdf_api_s {
    xdf_api_config_t config;
    
    /* Format registry */
    xdf_format_desc_t formats[XDF_MAX_FORMATS];
    size_t format_count;
    
    /* Current disk */
    xdf_context_t *context;
    char *current_path;
    char *current_format;
    bool is_open;
    bool analyzed;
    
    /* Results cache */
    xdf_pipeline_result_t last_result;
    
    /* Error handling */
    char error_msg[XDF_ERROR_BUF_SIZE];
    int error_code;
    
    /* Thread safety */
    void *mutex;  /* Platform-specific mutex */
};

/**
 * @brief Batch processor structure
 */
struct xdf_batch_s {
    xdf_api_t *api;
    
    /* File list */
    char *files[BATCH_MAX_FILES];
    size_t file_count;
    
    /* Results */
    xdf_batch_result_t results[BATCH_MAX_FILES];
    size_t result_count;
    
    /* Options */
    bool analyze_all;
    bool export_xdf;
    bool export_classic;
    char *output_dir;
};

/*===========================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Set error message
 */
static inline void set_error(xdf_api_t *api, int code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(api->error_msg, XDF_ERROR_BUF_SIZE, fmt, args);
    va_end(args);
    api->error_code = code;
}

/**
 * @brief Get file extension
 */
static inline const char* get_extension(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : NULL;
}

/**
 * @brief Detect platform from format descriptor
 */
static inline xdf_platform_t detect_platform_from_format(const xdf_format_desc_t *fmt) {
    return fmt ? fmt->platform : XDF_PLATFORM_UNKNOWN;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_API_INTERNAL_H */
