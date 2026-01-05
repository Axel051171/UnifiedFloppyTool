/**
 * @file uft_capability.h
 * @brief Runtime Capability Matrix API
 * 
 * TICKET-007: Capability Matrix Runtime
 * Query hardware/format capabilities at runtime
 * 
 * @version 5.1.0
 * @date 2026-01-03
 */

#ifndef UFT_CAPABILITY_H
#define UFT_CAPABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Flags
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Format capabilities */
typedef enum {
    UFT_CAP_READ          = (1 << 0),   /**< Can read this format */
    UFT_CAP_WRITE         = (1 << 1),   /**< Can write this format */
    UFT_CAP_CONVERT_FROM  = (1 << 2),   /**< Can convert from this format */
    UFT_CAP_CONVERT_TO    = (1 << 3),   /**< Can convert to this format */
    UFT_CAP_ANALYZE       = (1 << 4),   /**< Can analyze/inspect */
    UFT_CAP_RECOVER       = (1 << 5),   /**< Recovery support */
    UFT_CAP_VERIFY        = (1 << 6),   /**< Verification support */
    UFT_CAP_FLUX          = (1 << 7),   /**< Flux-level access */
    UFT_CAP_PROTECTION    = (1 << 8),   /**< Copy protection detection */
    UFT_CAP_MULTI_REV     = (1 << 9),   /**< Multi-revolution support */
    UFT_CAP_WEAK_BITS     = (1 << 10),  /**< Weak bit detection */
    UFT_CAP_HALF_TRACKS   = (1 << 11),  /**< Half-track support */
    UFT_CAP_VARIABLE_RPM  = (1 << 12),  /**< Variable RPM support */
    UFT_CAP_INDEX_SYNC    = (1 << 13),  /**< Index-synchronized capture */
} uft_capability_t;

/** Hardware capabilities */
typedef enum {
    UFT_HW_CAP_READ       = (1 << 0),   /**< Can read disks */
    UFT_HW_CAP_WRITE      = (1 << 1),   /**< Can write disks */
    UFT_HW_CAP_FLUX_READ  = (1 << 2),   /**< Raw flux capture */
    UFT_HW_CAP_FLUX_WRITE = (1 << 3),   /**< Raw flux write */
    UFT_HW_CAP_MULTI_REV  = (1 << 4),   /**< Multi-revolution capture */
    UFT_HW_CAP_INDEX      = (1 << 5),   /**< Index pulse detection */
    UFT_HW_CAP_DENSITY    = (1 << 6),   /**< Density selection */
    UFT_HW_CAP_SIDE_SEL   = (1 << 7),   /**< Side selection */
    UFT_HW_CAP_MOTOR_CTRL = (1 << 8),   /**< Motor control */
    UFT_HW_CAP_ERASE      = (1 << 9),   /**< Erase capability */
    UFT_HW_CAP_PRECOMP    = (1 << 10),  /**< Write precompensation */
    UFT_HW_CAP_HD         = (1 << 11),  /**< High density support */
    UFT_HW_CAP_ED         = (1 << 12),  /**< Extra density support */
    UFT_HW_CAP_8INCH      = (1 << 13),  /**< 8" drive support */
} uft_hw_capability_t;

/** Platform support level */
typedef enum {
    UFT_PLATFORM_FULL,          /**< Full support */
    UFT_PLATFORM_PARTIAL,       /**< Partial support */
    UFT_PLATFORM_EXPERIMENTAL,  /**< Experimental */
    UFT_PLATFORM_UNSUPPORTED,   /**< Not supported */
} uft_platform_support_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Info Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format capability information
 */
typedef struct {
    uft_format_t        format;         /**< Format identifier */
    const char          *name;          /**< Format name */
    const char          *description;   /**< Description */
    const char          *extensions;    /**< File extensions (comma-separated) */
    uint32_t            capabilities;   /**< Capability flags */
    
    /* Geometry limits */
    int                 min_cylinders;
    int                 max_cylinders;
    int                 min_heads;
    int                 max_heads;
    int                 min_sectors;
    int                 max_sectors;
    
    /* Platform support */
    const char          *platforms;     /**< Supported platforms */
    
    /* Dependencies */
    const char          *requires_hw;   /**< Required hardware (NULL = any) */
    const char          *conflicts;     /**< Conflicting formats */
    
    /* Metadata */
    const char          *version;       /**< Parser version */
    const char          *author;        /**< Author/source */
    const char          *url;           /**< Documentation URL */
} uft_format_info_t;

/**
 * @brief Hardware capability information
 */
typedef struct {
    uft_hardware_t      hardware;       /**< Hardware identifier */
    const char          *name;          /**< Hardware name */
    const char          *description;   /**< Description */
    const char          *vendor;        /**< Vendor name */
    uint32_t            capabilities;   /**< Capability flags */
    
    /* Timing */
    uint32_t            min_sample_rate; /**< Min sample rate (Hz) */
    uint32_t            max_sample_rate; /**< Max sample rate (Hz) */
    uint32_t            sample_resolution; /**< Resolution (ns) */
    
    /* Drive support */
    int                 max_drives;     /**< Max drives supported */
    const char          *drive_types;   /**< Supported drive types */
    
    /* Platform support */
    uft_platform_support_t linux_support;
    uft_platform_support_t macos_support;
    uft_platform_support_t windows_support;
    
    /* Connection */
    const char          *connection;    /**< USB/Serial/etc */
    const char          *driver;        /**< Required driver */
    
    /* Metadata */
    const char          *firmware_url;  /**< Firmware update URL */
    const char          *manual_url;    /**< User manual URL */
} uft_hardware_info_t;

/**
 * @brief Compatibility matrix entry
 */
typedef struct {
    uft_format_t        format;
    uft_hardware_t      hardware;
    uint32_t            capabilities;   /**< What's possible with this combo */
    int                 quality;        /**< Quality rating 0-100 */
    const char          *notes;         /**< Compatibility notes */
    const char          *limitations;   /**< Known limitations */
} uft_compat_entry_t;

/**
 * @brief Query result
 */
typedef struct {
    bool                supported;
    uint32_t            capabilities;
    int                 quality;
    const char          *message;
    const char          *suggestion;
} uft_capability_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Query API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Query if a specific capability is supported
 * @param format Format to check
 * @param capability Capability flag to check
 * @return true if supported
 */
bool uft_capability_check(uft_format_t format, uft_capability_t capability);

/**
 * @brief Query all capabilities for a format
 * @param format Format to query
 * @return Capability flags (OR'd together)
 */
uint32_t uft_capability_get(uft_format_t format);

/**
 * @brief Query hardware capabilities
 * @param hardware Hardware to query
 * @return Hardware capability flags
 */
uint32_t uft_hw_capability_get(uft_hardware_t hardware);

/**
 * @brief Check format+hardware compatibility
 * @param format Format to use
 * @param hardware Hardware to use
 * @param result Output result (can be NULL)
 * @return true if compatible
 */
bool uft_capability_compatible(uft_format_t format, uft_hardware_t hardware,
                                uft_capability_result_t *result);

/**
 * @brief Query full capability result with recommendations
 * @param format Format to query
 * @param hardware Hardware to query (UFT_HW_NONE for format-only)
 * @param operation Operation type (read/write/convert)
 * @return Result structure (caller should not free)
 */
uft_capability_result_t uft_capability_query(uft_format_t format,
                                              uft_hardware_t hardware,
                                              uft_capability_t operation);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Information API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get format information
 * @param format Format identifier
 * @return Format info (NULL if unknown)
 */
const uft_format_info_t *uft_format_get_info(uft_format_t format);

/**
 * @brief Get format by name
 * @param name Format name (case-insensitive)
 * @return Format identifier (UFT_FORMAT_UNKNOWN if not found)
 */
uft_format_t uft_format_by_name(const char *name);

/**
 * @brief Get format by file extension
 * @param extension File extension (with or without dot)
 * @return Format identifier (UFT_FORMAT_UNKNOWN if not found)
 */
uft_format_t uft_format_by_extension(const char *extension);

/**
 * @brief List all supported formats
 * @param count Output: number of formats
 * @return Array of format identifiers (do not free)
 */
const uft_format_t *uft_format_list_all(int *count);

/**
 * @brief List formats with specific capability
 * @param capability Required capability
 * @param count Output: number of matching formats
 * @return Array of format identifiers (caller must free)
 */
uft_format_t *uft_format_list_by_capability(uft_capability_t capability, int *count);

/**
 * @brief List formats compatible with hardware
 * @param hardware Hardware to check
 * @param count Output: number of compatible formats
 * @return Array of format identifiers (caller must free)
 */
uft_format_t *uft_format_list_by_hardware(uft_hardware_t hardware, int *count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hardware Information API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get hardware information
 * @param hardware Hardware identifier
 * @return Hardware info (NULL if unknown)
 */
const uft_hardware_info_t *uft_hardware_get_info(uft_hardware_t hardware);

/**
 * @brief Get hardware by name
 * @param name Hardware name (case-insensitive)
 * @return Hardware identifier (UFT_HW_NONE if not found)
 */
uft_hardware_t uft_hardware_by_name(const char *name);

/**
 * @brief List all supported hardware
 * @param count Output: number of hardware types
 * @return Array of hardware identifiers (do not free)
 */
const uft_hardware_t *uft_hardware_list_all(int *count);

/**
 * @brief List hardware with specific capability
 * @param capability Required capability
 * @param count Output: number of matching hardware
 * @return Array of hardware identifiers (caller must free)
 */
uft_hardware_t *uft_hardware_list_by_capability(uft_hw_capability_t capability, int *count);

/**
 * @brief Check platform support for hardware
 * @param hardware Hardware to check
 * @return Platform support level for current platform
 */
uft_platform_support_t uft_hardware_platform_support(uft_hardware_t hardware);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compatibility Matrix API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get compatibility entry
 * @param format Format to check
 * @param hardware Hardware to check
 * @return Compatibility entry (NULL if no specific entry)
 */
const uft_compat_entry_t *uft_compat_get(uft_format_t format, uft_hardware_t hardware);

/**
 * @brief Find best hardware for a format
 * @param format Format to use
 * @param operation Desired operation
 * @return Best hardware (UFT_HW_NONE if none suitable)
 */
uft_hardware_t uft_compat_best_hardware(uft_format_t format, uft_capability_t operation);

/**
 * @brief Find best format for conversion
 * @param source Source format
 * @param preserve_caps Capabilities to preserve
 * @return Best target format
 */
uft_format_t uft_compat_best_target(uft_format_t source, uint32_t preserve_caps);

/**
 * @brief Get conversion path between formats
 * @param source Source format
 * @param target Target format
 * @param path Output array for intermediate formats (can be NULL)
 * @param max_steps Maximum path length
 * @return Number of steps (-1 if no path)
 */
int uft_compat_conversion_path(uft_format_t source, uft_format_t target,
                                uft_format_t *path, int max_steps);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Discovery API (for GUI)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Discover available features
 * @param detected_hw Detected hardware (can be UFT_HW_NONE)
 * @param source_format Source file format (can be UFT_FORMAT_UNKNOWN)
 * @return JSON string describing available operations (caller must free)
 */
char *uft_capability_discover(uft_hardware_t detected_hw, uft_format_t source_format);

/**
 * @brief Get feature suggestions for user
 * @param current_caps Current capabilities
 * @param desired_caps Desired capabilities
 * @return JSON string with suggestions (caller must free)
 */
char *uft_capability_suggest(uint32_t current_caps, uint32_t desired_caps);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Export API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Export full capability matrix as JSON
 * @param pretty Pretty-print output
 * @return JSON string (caller must free)
 */
char *uft_capability_export_json(bool pretty);

/**
 * @brief Export capability matrix as Markdown table
 * @return Markdown string (caller must free)
 */
char *uft_capability_export_markdown(void);

/**
 * @brief Export capability matrix as HTML
 * @return HTML string (caller must free)
 */
char *uft_capability_export_html(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Get capability name */
const char *uft_capability_name(uft_capability_t cap);

/** Get hardware capability name */
const char *uft_hw_capability_name(uft_hw_capability_t cap);

/** Get platform support name */
const char *uft_platform_support_name(uft_platform_support_t level);

/** Format capability flags as string */
char *uft_capability_flags_string(uint32_t caps);

/** Parse capability flags from string */
uint32_t uft_capability_flags_parse(const char *str);

/** Print capability summary */
void uft_capability_print_summary(void);

/** Print format info */
void uft_format_print_info(uft_format_t format);

/** Print hardware info */
void uft_hardware_print_info(uft_hardware_t hardware);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CAPABILITY_H */
