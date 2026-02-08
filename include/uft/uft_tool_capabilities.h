/**
 * @file uft_tool_capabilities.h
 * @brief Tool Capability Matrix & I/O Abstraction
 * 
 * TOOL-KATEGORIEN:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 * FLUX TOOLS (Hardware-basiert):
 *   Input:  Hardware (Floppy Drive)
 *   Output: Flux (SCP, HFE, Kryoflux raw)
 *   
 * SECTOR TOOLS (Format-spezifisch):
 *   Input:  Hardware ODER Sector-Images
 *   Output: Sector-Images (D64, ADF, IMG)
 *   
 * CONVERTER TOOLS (Software):
 *   Input:  Flux/Bitstream/Sector Images
 *   Output: Verschiedene Formate
 *   
 * ANALYZER TOOLS:
 *   disk-analyse -infos
 *   Input:  Alle Formate
 *   Output: Analyse-Reports
 */

#ifndef UFT_TOOL_CAPABILITIES_H
#define UFT_TOOL_CAPABILITIES_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Data Layer Types
// ============================================================================

typedef enum uft_data_layer {
    UFT_LAYER_FLUX,         // Raw flux timing data
    UFT_LAYER_BITSTREAM,    // Encoded bitstream (MFM/GCR)
    UFT_LAYER_SECTOR,       // Decoded sector data
    UFT_LAYER_FILESYSTEM,   // Filesystem level (files/dirs)
} uft_data_layer_t;

// ============================================================================
// Tool Input/Output Capabilities
// ============================================================================

typedef struct uft_tool_io_caps {
    // Input capabilities
    struct {
        bool    from_hardware;      // Can read from physical drive
        bool    from_flux;          // Can process flux files
        bool    from_bitstream;     // Can process bitstream files
        bool    from_sector;        // Can process sector images
        bool    from_filesystem;    // Can process mounted filesystems
        
        // Supported input formats
        uint32_t flux_formats;      // Bitmask: SCP, Kryoflux, A2R
        uint32_t bitstream_formats; // Bitmask: HFE, G64, WOZ
        uint32_t sector_formats;    // Bitmask: D64, ADF, IMG
    } input;
    
    // Output capabilities
    struct {
        bool    to_hardware;        // Can write to physical drive
        bool    to_flux;            // Can output flux data
        bool    to_bitstream;       // Can output bitstream
        bool    to_sector;          // Can output sector data
        bool    to_filesystem;      // Can create filesystem content
        
        // Supported output formats
        uint32_t flux_formats;
        uint32_t bitstream_formats;
        uint32_t sector_formats;
    } output;
    
    // Processing capabilities
    struct {
        bool    can_convert;        // Format conversion
        bool    can_analyze;        // Disk analysis
        bool    can_verify;         // Read verification
        bool    can_format;         // Disk formatting
        bool    can_repair;         // Error correction
    } processing;
} uft_tool_io_caps_t;

// ============================================================================
// Tool Limitations
// ============================================================================

typedef struct uft_tool_limits {
    // Track limits
    int     max_cylinders;          // 0 = unlimited
    int     max_heads;
    int     max_sectors_per_track;
    
    // Data limits
    size_t  max_track_size;         // Bytes per track
    size_t  max_image_size;         // Total image size
    int     max_revolutions;        // For flux capture
    
    // Platform restrictions
    bool    linux_only;
    bool    windows_only;
    bool    macos_only;
    bool    requires_root;          // Needs elevated privileges
    
    // Hardware restrictions
    bool    requires_usb;           // USB device required
    bool    requires_parallel;      // Parallel port required
    const char* supported_hardware[8]; // List of supported adapters
    
    // Format restrictions
    const char* format_notes;       // Special format handling notes
} uft_tool_limits_t;

// ============================================================================
// Tool Flags/Options
// ============================================================================

typedef struct uft_tool_option {
    const char* name;           // "--revs", "-r"
    const char* long_name;      // "revolutions"
    const char* description;
    
    enum {
        OPT_FLAG,               // Boolean flag
        OPT_INT,                // Integer value
        OPT_FLOAT,              // Float value
        OPT_STRING,             // String value
        OPT_ENUM,               // Enumerated value
    } type;
    
    union {
        struct { int min, max, def; } int_range;
        struct { double min, max, def; } float_range;
        struct { const char** values; int count; int def; } enum_vals;
    };
    
    bool        required;
    const char* category;       // "capture", "output", "advanced"
} uft_tool_option_t;

// ============================================================================
// Complete Tool Profile
// ============================================================================

typedef struct uft_tool_profile {
    const char*             name;
    const char*             version;
    const char*             description;
    const char*             homepage;
    const char*             executable;     // Binary name
    
    uft_tool_io_caps_t      io_caps;
    uft_tool_limits_t       limits;
    
    // Platform support
    struct {
        const char* os;                     // "linux", "windows", "macos", "all"
        const char* arch;                   // "x86_64", "arm64", "all"
        const char* min_version;            // OS minimum version
    } platforms[4];
    int platform_count;
    
    // Options
    const uft_tool_option_t* options;
    int option_count;
    
    // CLI patterns
    const char* read_pattern;       // "gw read --revs={revs} -d {device} {output}"
    const char* write_pattern;      // "gw write {input} -d {device}"
    const char* convert_pattern;    // For conversion tools
    const char* analyze_pattern;    // For analysis
} uft_tool_profile_t;

// ============================================================================
// Built-in Tool Profiles
// ============================================================================

extern const uft_tool_profile_t UFT_TOOL_GREASEWEAZLE;
extern const uft_tool_profile_t UFT_TOOL_FLUXENGINE;
extern const uft_tool_profile_t UFT_TOOL_KRYOFLUX;
extern const uft_tool_profile_t UFT_TOOL_NIBTOOLS;
extern const uft_tool_profile_t UFT_TOOL_ADFTOOLS;
extern const uft_tool_profile_t UFT_TOOL_LIBFLUX;
extern const uft_tool_profile_t UFT_TOOL_DISK_ANALYSE;
extern const uft_tool_profile_t UFT_TOOL_SAMDISK;
extern const uft_tool_profile_t UFT_TOOL_MTOOLS;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Get all tool profiles
 */
int uft_tool_get_profiles(const uft_tool_profile_t** profiles, int max);

/**
 * @brief Find tool by name
 */
const uft_tool_profile_t* uft_tool_find_profile(const char* name);

/**
 * @brief Find best tool for operation
 */
const uft_tool_profile_t* uft_tool_find_for_io(
    uft_data_layer_t input_layer,
    uft_data_layer_t output_layer,
    uft_format_t input_format,
    uft_format_t output_format);

/**
 * @brief Check if tool can handle format
 */
bool uft_tool_supports_format(const uft_tool_profile_t* tool,
                               uft_format_t format,
                               bool as_input);

/**
 * @brief Print capability matrix
 */
void uft_tool_print_matrix(void);

#ifdef __cplusplus
}
#endif

#endif // UFT_TOOL_CAPABILITIES_H
