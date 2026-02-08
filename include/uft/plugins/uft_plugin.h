/**
 * @file uft_plugin.h
 * @brief UFT Plugin System Interface
 * @version 4.1.0
 * 
 * Provides infrastructure for community-developed format handlers,
 * hardware drivers, and tool extensions.
 * 
 * Plugin Types:
 * - FORMAT_PLUGIN: Disk image format handlers
 * - HARDWARE_PLUGIN: Hardware device drivers
 * - TOOL_PLUGIN: Analysis/conversion tools
 * - FILESYSTEM_PLUGIN: Filesystem handlers
 */

#ifndef UFT_PLUGIN_H
#define UFT_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Plugin Version and ABI
 * ============================================================================ */

#define UFT_PLUGIN_ABI_VERSION  1
#define UFT_PLUGIN_API_VERSION  "4.1.0"

/* ============================================================================
 * Plugin Types
 * ============================================================================ */

typedef enum {
    UFT_PLUGIN_FORMAT     = 0x01,  /**< Disk image format handler */
    UFT_PLUGIN_HARDWARE   = 0x02,  /**< Hardware device driver */
    UFT_PLUGIN_TOOL       = 0x04,  /**< Analysis/conversion tool */
    UFT_PLUGIN_FILESYSTEM = 0x08,  /**< Filesystem handler */
    UFT_PLUGIN_DECODER    = 0x10,  /**< Track decoder */
    UFT_PLUGIN_ENCODER    = 0x20,  /**< Track encoder */
} uft_plugin_type_t;

/* ============================================================================
 * Plugin Capability Flags
 * ============================================================================ */

typedef enum {
    UFT_PCAP_READ         = 0x0001,  /**< Can read */
    UFT_PCAP_WRITE        = 0x0002,  /**< Can write */
    UFT_PCAP_CONVERT      = 0x0004,  /**< Can convert */
    UFT_PCAP_ANALYZE      = 0x0008,  /**< Can analyze */
    UFT_PCAP_REPAIR       = 0x0010,  /**< Can repair */
    UFT_PCAP_VERIFY       = 0x0020,  /**< Can verify */
    UFT_PCAP_STREAM       = 0x0040,  /**< Streaming support */
    UFT_PCAP_ASYNC        = 0x0080,  /**< Async operation support */
    UFT_PCAP_GUI          = 0x0100,  /**< Has GUI components */
    UFT_PCAP_CLI          = 0x0200,  /**< Has CLI commands */
} uft_plugin_caps_t;

/* ============================================================================
 * Plugin Info Structure
 * ============================================================================ */

typedef struct {
    uint32_t            abi_version;     /**< Must be UFT_PLUGIN_ABI_VERSION */
    const char*         api_version;     /**< API version string */
    const char*         name;            /**< Plugin name */
    const char*         version;         /**< Plugin version */
    const char*         author;          /**< Author name */
    const char*         description;     /**< Description */
    const char*         license;         /**< License (MIT, GPL, etc.) */
    const char*         url;             /**< Project URL */
    uft_plugin_type_t   type;            /**< Plugin type */
    uint32_t            capabilities;    /**< Capability flags */
} uft_plugin_info_t;

/* ============================================================================
 * Plugin Context
 * ============================================================================ */

typedef struct uft_plugin_context uft_plugin_context_t;

/* ============================================================================
 * Format Plugin Interface
 * ============================================================================ */

typedef struct {
    const char*  extension;         /**< File extension (without dot) */
    const char*  description;       /**< Format description */
    const char*  platform;          /**< Target platform */
    
    /** Probe function - returns confidence (0-100) */
    int (*probe)(const uint8_t *data, size_t size);
    
    /** Read image from file */
    int (*read)(const char *path, void **disk_out);
    
    /** Write image to file */
    int (*write)(const char *path, const void *disk);
    
    /** Get format info */
    int (*get_info)(const void *disk, char *buf, size_t buf_size);
    
    /** Convert to another format */
    int (*convert)(const void *src, const char *target_format, void **dst);
    
} uft_format_plugin_t;

/* ============================================================================
 * Hardware Plugin Interface
 * ============================================================================ */

typedef struct {
    const char*  device_name;       /**< Device name */
    const char*  description;       /**< Device description */
    uint16_t     vendor_id;         /**< USB vendor ID (0 for serial) */
    uint16_t     product_id;        /**< USB product ID */
    
    /** Detect devices */
    int (*detect)(char **devices, int max_devices);
    
    /** Open device */
    int (*open)(const char *path, void **handle);
    
    /** Close device */
    void (*close)(void *handle);
    
    /** Read track */
    int (*read_track)(void *handle, int cyl, int head, uint8_t *data, size_t *size);
    
    /** Write track */
    int (*write_track)(void *handle, int cyl, int head, const uint8_t *data, size_t size);
    
    /** Get device info */
    int (*get_info)(void *handle, char *buf, size_t buf_size);
    
} uft_hardware_plugin_t;

/* ============================================================================
 * Tool Plugin Interface
 * ============================================================================ */

typedef struct {
    const char*  tool_name;         /**< Tool name */
    const char*  description;       /**< Tool description */
    const char*  category;          /**< Category (Analysis, Conversion, etc.) */
    
    /** Execute tool */
    int (*execute)(int argc, const char **argv, char *output, size_t output_size);
    
    /** Get help text */
    const char* (*get_help)(void);
    
} uft_tool_plugin_t;

/* ============================================================================
 * Plugin Entry Point
 * ============================================================================ */

/**
 * @brief Plugin initialization function
 * 
 * Every plugin must export this function:
 *   uft_plugin_info_t* uft_plugin_init(void);
 * 
 * @return Plugin info structure, or NULL on failure
 */
typedef uft_plugin_info_t* (*uft_plugin_init_fn)(void);

/**
 * @brief Plugin cleanup function
 * 
 * Optional cleanup function:
 *   void uft_plugin_cleanup(void);
 */
typedef void (*uft_plugin_cleanup_fn)(void);

/**
 * @brief Get plugin interface
 * 
 * Returns the plugin's interface structure based on type:
 *   void* uft_plugin_get_interface(uft_plugin_type_t type);
 */
typedef void* (*uft_plugin_get_interface_fn)(uft_plugin_type_t type);

/* ============================================================================
 * Plugin Manager API
 * ============================================================================ */

/** Load plugin from file */
int uft_plugin_load(const char *path);

/** Unload plugin */
int uft_plugin_unload(const char *name);

/** Get loaded plugin count */
int uft_plugin_count(void);

/** Get plugin info by index */
const uft_plugin_info_t* uft_plugin_get_info(int index);

/** Find plugin by name */
const uft_plugin_info_t* uft_plugin_find(const char *name);

/** List all plugins */
void uft_plugin_list(void);

/** Scan directory for plugins */
int uft_plugin_scan_dir(const char *dir);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLUGIN_H */
