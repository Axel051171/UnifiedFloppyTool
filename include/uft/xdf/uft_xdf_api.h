/**
 * @file uft_xdf_api.h
 * @brief XDF API - Universal Disk Forensics Interface
 * 
 * Der "Booster" - Eine einheitliche API für alle Disk-Operationen.
 * 
 * Konzept:
 * - EIN Interface für ALLE Formate (Amiga/C64/PC/Atari/Spectrum/...)
 * - Format-agnostisch: API weiß nicht, welches Format - XDF entscheidet
 * - Plugin-System für neue Formate
 * - Callbacks für Progress/Events
 * - Thread-safe für parallele Operationen
 * - Optional: REST/gRPC für externe Tools
 * 
 * Usage:
 *   xdf_api_t *api = xdf_api_create();
 *   xdf_api_open(api, "game.adf");           // Auto-detect
 *   xdf_api_analyze(api);                     // Full pipeline
 *   xdf_api_export(api, "game.axdf", NULL);  // Save with metadata
 *   xdf_api_destroy(api);
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_API_H
#define UFT_XDF_API_H

#include "uft_xdf_core.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * API Version
 *===========================================================================*/

#define XDF_API_VERSION_MAJOR   1
#define XDF_API_VERSION_MINOR   0
#define XDF_API_VERSION_PATCH   0

#define XDF_API_VERSION_STRING  "1.0.0"

/*===========================================================================
 * Opaque Types
 *===========================================================================*/

typedef struct xdf_api_s xdf_api_t;
typedef struct xdf_disk_s xdf_disk_t;
typedef struct xdf_batch_s xdf_batch_t;

/*===========================================================================
 * Event Types
 *===========================================================================*/

typedef enum {
    XDF_EVENT_NONE = 0,
    
    /* Progress events */
    XDF_EVENT_PROGRESS,         /**< General progress update */
    XDF_EVENT_PHASE_START,      /**< Pipeline phase started */
    XDF_EVENT_PHASE_END,        /**< Pipeline phase ended */
    XDF_EVENT_TRACK_START,      /**< Track processing started */
    XDF_EVENT_TRACK_END,        /**< Track processing ended */
    
    /* Analysis events */
    XDF_EVENT_FORMAT_DETECTED,  /**< Format auto-detected */
    XDF_EVENT_PROTECTION_FOUND, /**< Protection detected */
    XDF_EVENT_WEAK_BITS,        /**< Weak bits found */
    XDF_EVENT_ERROR_FOUND,      /**< Error detected */
    
    /* Repair events */
    XDF_EVENT_REPAIR_START,     /**< Repair attempt started */
    XDF_EVENT_REPAIR_SUCCESS,   /**< Repair successful */
    XDF_EVENT_REPAIR_FAILED,    /**< Repair failed */
    
    /* Validation events */
    XDF_EVENT_VALIDATION_WARN,  /**< Validation warning */
    XDF_EVENT_VALIDATION_ERROR, /**< Validation error */
    
    /* I/O events */
    XDF_EVENT_FILE_OPEN,        /**< File opened */
    XDF_EVENT_FILE_CLOSE,       /**< File closed */
    XDF_EVENT_EXPORT_START,     /**< Export started */
    XDF_EVENT_EXPORT_END,       /**< Export completed */
} xdf_event_type_t;

/**
 * @brief Event data structure
 */
typedef struct {
    xdf_event_type_t type;
    
    /* Context */
    const char *source;         /**< Source identifier */
    int track;                  /**< Track number (-1 if N/A) */
    int sector;                 /**< Sector number (-1 if N/A) */
    int phase;                  /**< Pipeline phase (1-7) */
    
    /* Progress */
    int current;                /**< Current item */
    int total;                  /**< Total items */
    float percent;              /**< 0.0 - 100.0 */
    
    /* Details */
    const char *message;        /**< Human-readable message */
    xdf_confidence_t confidence;/**< Confidence if applicable */
    uint32_t flags;             /**< Event-specific flags */
    
    /* Data pointer (event-specific) */
    void *data;
    size_t data_size;
} xdf_event_t;

/**
 * @brief Event callback
 * @return true to continue, false to cancel
 */
typedef bool (*xdf_event_callback_t)(const xdf_event_t *event, void *user);

/*===========================================================================
 * Format Registration (Plugin System)
 *===========================================================================*/

/**
 * @brief Format probe function
 * @return Confidence (0-10000) that this format matches
 */
typedef xdf_confidence_t (*xdf_format_probe_t)(
    const uint8_t *data, 
    size_t size,
    const char *filename
);

/**
 * @brief Format import function
 */
typedef int (*xdf_format_import_t)(
    xdf_context_t *ctx,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Format export function
 */
typedef int (*xdf_format_export_t)(
    xdf_context_t *ctx,
    uint8_t **data,
    size_t *size
);

/**
 * @brief Format descriptor
 */
typedef struct {
    const char *name;           /**< Format name (e.g., "ADF") */
    const char *description;    /**< Human-readable description */
    const char *extensions;     /**< File extensions (comma-separated) */
    xdf_platform_t platform;    /**< Platform */
    
    /* Functions */
    xdf_format_probe_t probe;   /**< Probe function */
    xdf_format_import_t import; /**< Import function */
    xdf_format_export_t export; /**< Export function */
    
    /* Capabilities */
    bool can_read;
    bool can_write;
    bool preserves_protection;
    bool supports_flux;
} xdf_format_desc_t;

/*===========================================================================
 * API Configuration
 *===========================================================================*/

typedef struct {
    /* Pipeline options */
    xdf_options_t pipeline;     /**< Pipeline options */
    
    /* API behavior */
    bool auto_detect;           /**< Auto-detect format on open */
    bool lazy_load;             /**< Lazy-load track data */
    bool thread_safe;           /**< Enable thread-safe mode */
    int max_threads;            /**< Max worker threads (0=auto) */
    
    /* Caching */
    bool enable_cache;          /**< Enable disk cache */
    size_t cache_size_mb;       /**< Cache size in MB */
    
    /* Events */
    xdf_event_callback_t callback;
    void *callback_user;
    uint32_t event_mask;        /**< Which events to report */
    
    /* Logging */
    int log_level;              /**< 0=off, 1=error, 2=warn, 3=info, 4=debug */
    const char *log_file;       /**< Log file path (NULL=stderr) */
} xdf_api_config_t;

/*===========================================================================
 * Core API Functions
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * Lifecycle
 *---------------------------------------------------------------------------*/

/**
 * @brief Create API instance with default config
 */
xdf_api_t* xdf_api_create(void);

/**
 * @brief Create API instance with custom config
 */
xdf_api_t* xdf_api_create_with_config(const xdf_api_config_t *config);

/**
 * @brief Destroy API instance
 */
void xdf_api_destroy(xdf_api_t *api);

/**
 * @brief Get default configuration
 */
xdf_api_config_t xdf_api_default_config(void);

/**
 * @brief Update configuration (some options require reopen)
 */
int xdf_api_set_config(xdf_api_t *api, const xdf_api_config_t *config);

/*---------------------------------------------------------------------------
 * Format Registration
 *---------------------------------------------------------------------------*/

/**
 * @brief Register a format handler
 */
int xdf_api_register_format(xdf_api_t *api, const xdf_format_desc_t *format);

/**
 * @brief Unregister a format handler
 */
int xdf_api_unregister_format(xdf_api_t *api, const char *name);

/**
 * @brief Get list of registered formats
 */
int xdf_api_list_formats(xdf_api_t *api, const char ***names, size_t *count);

/**
 * @brief Get format descriptor
 */
const xdf_format_desc_t* xdf_api_get_format(xdf_api_t *api, const char *name);

/*---------------------------------------------------------------------------
 * File Operations (Single Disk)
 *---------------------------------------------------------------------------*/

/**
 * @brief Open disk image (auto-detect format)
 */
int xdf_api_open(xdf_api_t *api, const char *path);

/**
 * @brief Open with explicit format
 */
int xdf_api_open_as(xdf_api_t *api, const char *path, const char *format);

/**
 * @brief Open from memory buffer
 */
int xdf_api_open_memory(xdf_api_t *api, const uint8_t *data, size_t size,
                         const char *format);

/**
 * @brief Close current disk
 */
int xdf_api_close(xdf_api_t *api);

/**
 * @brief Check if disk is open
 */
bool xdf_api_is_open(const xdf_api_t *api);

/**
 * @brief Get detected format name
 */
const char* xdf_api_get_format_name(const xdf_api_t *api);

/**
 * @brief Get detected platform
 */
xdf_platform_t xdf_api_get_platform(const xdf_api_t *api);

/*---------------------------------------------------------------------------
 * Analysis (The Booster!)
 *---------------------------------------------------------------------------*/

/**
 * @brief Run full 7-phase analysis pipeline
 * 
 * This is the main "booster" function. It:
 * 1. Reads and captures data
 * 2. Compares multiple reads
 * 3. Analyzes structure and zones
 * 4. Matches against known patterns
 * 5. Validates and scores confidence
 * 6. Repairs if enabled
 * 7. Prepares for export
 */
int xdf_api_analyze(xdf_api_t *api);

/**
 * @brief Run specific pipeline phase
 */
int xdf_api_run_phase(xdf_api_t *api, int phase);

/**
 * @brief Quick analysis (phases 1, 3, 5 only)
 */
int xdf_api_quick_analyze(xdf_api_t *api);

/**
 * @brief Get analysis results
 */
int xdf_api_get_results(xdf_api_t *api, xdf_pipeline_result_t *result);

/*---------------------------------------------------------------------------
 * Query Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Get overall confidence
 */
xdf_confidence_t xdf_api_get_confidence(const xdf_api_t *api);

/**
 * @brief Get disk info summary
 */
typedef struct {
    xdf_platform_t platform;
    const char *format;
    int cylinders;
    int heads;
    int sectors_per_track;
    int sector_size;
    size_t total_size;
    xdf_confidence_t confidence;
    bool has_protection;
    bool has_errors;
    bool was_repaired;
} xdf_disk_info_t;

int xdf_api_get_disk_info(xdf_api_t *api, xdf_disk_info_t *info);

/**
 * @brief Get track info
 */
int xdf_api_get_track_info(xdf_api_t *api, int cyl, int head, 
                            xdf_track_t *info);

/**
 * @brief Get sector data
 */
int xdf_api_read_sector(xdf_api_t *api, int cyl, int head, int sector,
                         uint8_t *buffer, size_t size);

/**
 * @brief Get raw track data
 */
int xdf_api_read_track(xdf_api_t *api, int cyl, int head,
                        uint8_t *buffer, size_t *size);

/**
 * @brief Get protection info
 */
int xdf_api_get_protection(xdf_api_t *api, xdf_protection_t *prot);

/**
 * @brief Get repair log
 */
int xdf_api_get_repairs(xdf_api_t *api, xdf_repair_entry_t **repairs,
                         size_t *count);

/*---------------------------------------------------------------------------
 * Export
 *---------------------------------------------------------------------------*/

/**
 * @brief Export to XDF format (preserves all metadata)
 */
int xdf_api_export_xdf(xdf_api_t *api, const char *path);

/**
 * @brief Export to classic format (ADF/D64/IMG/etc.)
 */
int xdf_api_export_classic(xdf_api_t *api, const char *path);

/**
 * @brief Export to specific format
 */
int xdf_api_export_as(xdf_api_t *api, const char *path, const char *format);

/**
 * @brief Export to memory buffer
 */
int xdf_api_export_memory(xdf_api_t *api, const char *format,
                           uint8_t **data, size_t *size);

/**
 * @brief Free exported memory buffer
 */
void xdf_api_free_buffer(uint8_t *buffer);

/*---------------------------------------------------------------------------
 * Modification
 *---------------------------------------------------------------------------*/

/**
 * @brief Write sector
 */
int xdf_api_write_sector(xdf_api_t *api, int cyl, int head, int sector,
                          const uint8_t *data, size_t size);

/**
 * @brief Apply repair
 */
int xdf_api_apply_repair(xdf_api_t *api, int cyl, int head, int sector,
                          xdf_repair_action_t action);

/**
 * @brief Undo last repair
 */
int xdf_api_undo_repair(xdf_api_t *api);

/**
 * @brief Undo all repairs
 */
int xdf_api_undo_all_repairs(xdf_api_t *api);

/*===========================================================================
 * Batch Processing API
 *===========================================================================*/

/**
 * @brief Create batch processor
 */
xdf_batch_t* xdf_api_batch_create(xdf_api_t *api);

/**
 * @brief Add file to batch
 */
int xdf_api_batch_add(xdf_batch_t *batch, const char *path);

/**
 * @brief Add directory to batch (recursive)
 */
int xdf_api_batch_add_dir(xdf_batch_t *batch, const char *path, 
                           const char *pattern);

/**
 * @brief Process all files in batch
 */
int xdf_api_batch_process(xdf_batch_t *batch);

/**
 * @brief Get batch results
 */
typedef struct {
    const char *path;
    bool success;
    xdf_confidence_t confidence;
    const char *error;
} xdf_batch_result_t;

int xdf_api_batch_get_results(xdf_batch_t *batch, 
                               xdf_batch_result_t **results, size_t *count);

/**
 * @brief Destroy batch processor
 */
void xdf_api_batch_destroy(xdf_batch_t *batch);

/*===========================================================================
 * Comparison API
 *===========================================================================*/

/**
 * @brief Compare two disk images
 */
typedef struct {
    bool identical;             /**< Bit-identical? */
    bool logically_equal;       /**< Same content? */
    int different_tracks;       /**< Number of different tracks */
    int different_sectors;      /**< Number of different sectors */
    size_t different_bytes;     /**< Total different bytes */
    xdf_confidence_t similarity;/**< Similarity score */
    
    /* Details (caller must free) */
    struct {
        int cyl;
        int head;
        int sector;
        const char *difference;
    } *differences;
    size_t diff_count;
} xdf_compare_result_t;

int xdf_api_compare(xdf_api_t *api, const char *path1, const char *path2,
                     xdf_compare_result_t *result);

void xdf_api_free_compare_result(xdf_compare_result_t *result);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get API version
 */
void xdf_api_get_version(int *major, int *minor, int *patch);

/**
 * @brief Get version string
 */
const char* xdf_api_version_string(void);

/**
 * @brief Get last error message
 */
const char* xdf_api_get_error(const xdf_api_t *api);

/**
 * @brief Get last error code
 */
int xdf_api_get_error_code(const xdf_api_t *api);

/**
 * @brief Clear error state
 */
void xdf_api_clear_error(xdf_api_t *api);

/**
 * @brief Convert platform to string
 */
const char* xdf_api_platform_name(xdf_platform_t platform);

/**
 * @brief Detect format from file
 */
int xdf_api_detect_format(const char *path, char *format, size_t size,
                           xdf_confidence_t *confidence);

/**
 * @brief Validate file integrity
 */
int xdf_api_validate_file(const char *path, int *errors, char **messages);

/*===========================================================================
 * JSON API (for REST/IPC)
 *===========================================================================*/

/**
 * @brief Get disk info as JSON
 */
char* xdf_api_to_json(xdf_api_t *api);

/**
 * @brief Get track grid as JSON
 */
char* xdf_api_track_grid_json(xdf_api_t *api);

/**
 * @brief Get repair log as JSON
 */
char* xdf_api_repairs_json(xdf_api_t *api);

/**
 * @brief Process JSON command
 * 
 * Commands: "open", "analyze", "export", "compare", etc.
 */
char* xdf_api_process_json(xdf_api_t *api, const char *json_command);

/**
 * @brief Free JSON string
 */
void xdf_api_free_json(char *json);

/*===========================================================================
 * Hardware Integration (Optional)
 *===========================================================================*/

#ifdef XDF_API_HARDWARE

/**
 * @brief List connected hardware
 */
int xdf_api_list_hardware(xdf_api_t *api, char ***devices, size_t *count);

/**
 * @brief Read from hardware
 */
int xdf_api_read_hardware(xdf_api_t *api, const char *device);

/**
 * @brief Write to hardware
 */
int xdf_api_write_hardware(xdf_api_t *api, const char *device);

#endif /* XDF_API_HARDWARE */

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_API_H */
