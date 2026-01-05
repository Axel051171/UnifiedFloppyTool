/**
 * @file uft_web_viewer.h
 * @brief UFT Web-Based Disk Image Viewer
 * 
 * C-001: Browser-based viewer for disk images using WebAssembly
 * 
 * Features:
 * - WASM-compiled core library
 * - HTML5 Canvas track visualization
 * - Sector hex viewer
 * - Protection report display
 * - File format detection and preview
 */

#ifndef UFT_WEB_VIEWER_H
#define UFT_WEB_VIEWER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WASM export markers */
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define UFT_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define UFT_WASM_EXPORT
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Viewer limits */
#define UFT_WEB_MAX_TRACKS          168     /**< Maximum tracks (84 * 2 sides) */
#define UFT_WEB_MAX_SECTORS         30      /**< Maximum sectors per track */
#define UFT_WEB_MAX_FILE_SIZE       (64*1024*1024)  /**< 64MB max */
#define UFT_WEB_CANVAS_WIDTH        800
#define UFT_WEB_CANVAS_HEIGHT       600

/** Color palette (RGBA) */
#define UFT_COLOR_GOOD              0x00FF00FF  /**< Green */
#define UFT_COLOR_BAD               0xFF0000FF  /**< Red */
#define UFT_COLOR_WEAK              0xFFFF00FF  /**< Yellow */
#define UFT_COLOR_EMPTY             0x404040FF  /**< Dark gray */
#define UFT_COLOR_PROTECTED         0xFF00FFFF  /**< Magenta */
#define UFT_COLOR_UNKNOWN           0x808080FF  /**< Gray */

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief View mode
 */
typedef enum {
    UFT_VIEW_TRACK_MAP = 0,     /**< Track/sector overview */
    UFT_VIEW_TRACK_DETAIL,      /**< Single track detail */
    UFT_VIEW_SECTOR_HEX,        /**< Sector hex dump */
    UFT_VIEW_FLUX_GRAPH,        /**< Flux transition graph */
    UFT_VIEW_PROTECTION,        /**< Protection report */
    UFT_VIEW_METADATA           /**< File metadata */
} uft_web_view_mode_t;

/**
 * @brief Sector status
 */
typedef enum {
    UFT_SECTOR_UNKNOWN = 0,
    UFT_SECTOR_GOOD,            /**< CRC OK */
    UFT_SECTOR_BAD,             /**< CRC error */
    UFT_SECTOR_WEAK,            /**< Weak/fuzzy bits */
    UFT_SECTOR_EMPTY,           /**< No data */
    UFT_SECTOR_PROTECTED        /**< Copy protection */
} uft_web_sector_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    UFT_WEB_OK = 0,
    UFT_WEB_ERR_NOMEM,          /**< Out of memory */
    UFT_WEB_ERR_FILE,           /**< File error */
    UFT_WEB_ERR_FORMAT,         /**< Unknown format */
    UFT_WEB_ERR_CORRUPT,        /**< Corrupt data */
    UFT_WEB_ERR_PARAM           /**< Invalid parameter */
} uft_web_error_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Sector info for visualization
 */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  sector;
    uint8_t  status;            /**< uft_web_sector_status_t */
    uint16_t size;              /**< Sector size */
    uint16_t crc;               /**< CRC value */
    float    confidence;        /**< 0.0 - 1.0 */
    uint32_t color;             /**< Display color (RGBA) */
} uft_web_sector_info_t;

/**
 * @brief Track info for visualization
 */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  sector_count;
    uint8_t  good_sectors;
    uint8_t  bad_sectors;
    uint8_t  weak_sectors;
    bool     has_protection;
    float    avg_confidence;
    uint32_t data_size;
    uint32_t color;
} uft_web_track_info_t;

/**
 * @brief Disk info summary
 */
typedef struct {
    char     format_name[32];   /**< Format name (e.g., "ADF") */
    char     platform[32];      /**< Platform (e.g., "Amiga") */
    
    uint8_t  tracks;            /**< Total tracks */
    uint8_t  sides;             /**< Number of sides */
    uint8_t  sectors_per_track; /**< Sectors per track */
    uint16_t sector_size;       /**< Sector size */
    
    uint32_t total_sectors;     /**< Total sectors */
    uint32_t good_sectors;      /**< Good sectors */
    uint32_t bad_sectors;       /**< Bad sectors */
    uint32_t weak_sectors;      /**< Weak sectors */
    
    uint64_t file_size;         /**< File size */
    uint64_t data_size;         /**< Actual data size */
    
    bool     has_protection;    /**< Protection detected */
    char     protection_type[64]; /**< Protection name */
    
    char     hash_md5[33];      /**< MD5 hash */
    char     hash_sha1[41];     /**< SHA1 hash */
} uft_web_disk_info_t;

/**
 * @brief Viewer state
 */
typedef struct {
    /* Current file */
    uint8_t *data;
    size_t   data_size;
    char     filename[256];
    
    /* Parsed info */
    uft_web_disk_info_t disk_info;
    uft_web_track_info_t tracks[UFT_WEB_MAX_TRACKS];
    uint8_t  track_count;
    
    /* View state */
    uft_web_view_mode_t view_mode;
    uint8_t  selected_track;
    uint8_t  selected_sector;
    float    zoom;
    int32_t  scroll_x;
    int32_t  scroll_y;
    
    /* Canvas buffer */
    uint32_t *canvas;
    uint16_t  canvas_width;
    uint16_t  canvas_height;
} uft_web_viewer_t;

/*===========================================================================
 * WASM Exported Functions - Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize viewer
 * @return Viewer handle or 0 on error
 */
UFT_WASM_EXPORT
uintptr_t uft_web_init(void);

/**
 * @brief Destroy viewer
 */
UFT_WASM_EXPORT
void uft_web_destroy(uintptr_t handle);

/**
 * @brief Get version string
 */
UFT_WASM_EXPORT
const char *uft_web_version(void);

/*===========================================================================
 * WASM Exported Functions - File Operations
 *===========================================================================*/

/**
 * @brief Load disk image from buffer
 * 
 * @param handle Viewer handle
 * @param data File data
 * @param size Data size
 * @param filename Original filename (for format detection)
 * @return Error code
 */
UFT_WASM_EXPORT
int uft_web_load(uintptr_t handle, const uint8_t *data, size_t size,
                 const char *filename);

/**
 * @brief Unload current file
 */
UFT_WASM_EXPORT
void uft_web_unload(uintptr_t handle);

/**
 * @brief Check if file is loaded
 */
UFT_WASM_EXPORT
bool uft_web_is_loaded(uintptr_t handle);

/**
 * @brief Get disk info JSON string
 */
UFT_WASM_EXPORT
const char *uft_web_get_disk_info_json(uintptr_t handle);

/*===========================================================================
 * WASM Exported Functions - Rendering
 *===========================================================================*/

/**
 * @brief Set canvas size
 */
UFT_WASM_EXPORT
void uft_web_set_canvas_size(uintptr_t handle, uint16_t width, uint16_t height);

/**
 * @brief Get canvas buffer pointer
 * 
 * @return Pointer to RGBA pixel buffer
 */
UFT_WASM_EXPORT
uint8_t *uft_web_get_canvas(uintptr_t handle);

/**
 * @brief Render current view to canvas
 */
UFT_WASM_EXPORT
void uft_web_render(uintptr_t handle);

/**
 * @brief Set view mode
 */
UFT_WASM_EXPORT
void uft_web_set_view(uintptr_t handle, int mode);

/**
 * @brief Set zoom level
 */
UFT_WASM_EXPORT
void uft_web_set_zoom(uintptr_t handle, float zoom);

/**
 * @brief Set scroll position
 */
UFT_WASM_EXPORT
void uft_web_set_scroll(uintptr_t handle, int32_t x, int32_t y);

/*===========================================================================
 * WASM Exported Functions - Selection
 *===========================================================================*/

/**
 * @brief Select track
 */
UFT_WASM_EXPORT
void uft_web_select_track(uintptr_t handle, uint8_t track, uint8_t side);

/**
 * @brief Select sector
 */
UFT_WASM_EXPORT
void uft_web_select_sector(uintptr_t handle, uint8_t sector);

/**
 * @brief Get selected track info
 */
UFT_WASM_EXPORT
const char *uft_web_get_track_info_json(uintptr_t handle);

/**
 * @brief Get selected sector data as hex string
 */
UFT_WASM_EXPORT
const char *uft_web_get_sector_hex(uintptr_t handle);

/**
 * @brief Get selected sector data as base64
 */
UFT_WASM_EXPORT
const char *uft_web_get_sector_base64(uintptr_t handle);

/*===========================================================================
 * WASM Exported Functions - Analysis
 *===========================================================================*/

/**
 * @brief Get protection report JSON
 */
UFT_WASM_EXPORT
const char *uft_web_get_protection_report(uintptr_t handle);

/**
 * @brief Get file list JSON (if filesystem detected)
 */
UFT_WASM_EXPORT
const char *uft_web_get_file_list(uintptr_t handle);

/**
 * @brief Extract file from disk image
 * 
 * @param handle Viewer handle
 * @param path File path within disk
 * @param size Output: file size
 * @return File data (caller must free)
 */
UFT_WASM_EXPORT
uint8_t *uft_web_extract_file(uintptr_t handle, const char *path, size_t *size);

/*===========================================================================
 * WASM Exported Functions - Conversion
 *===========================================================================*/

/**
 * @brief Get list of supported output formats
 */
UFT_WASM_EXPORT
const char *uft_web_get_output_formats(uintptr_t handle);

/**
 * @brief Convert to different format
 * 
 * @param handle Viewer handle
 * @param format Target format name
 * @param size Output: converted data size
 * @return Converted data (caller must free)
 */
UFT_WASM_EXPORT
uint8_t *uft_web_convert(uintptr_t handle, const char *format, size_t *size);

/*===========================================================================
 * WASM Exported Functions - Input Handling
 *===========================================================================*/

/**
 * @brief Handle mouse click
 */
UFT_WASM_EXPORT
void uft_web_on_click(uintptr_t handle, int32_t x, int32_t y, int button);

/**
 * @brief Handle mouse move
 */
UFT_WASM_EXPORT
void uft_web_on_mousemove(uintptr_t handle, int32_t x, int32_t y);

/**
 * @brief Handle key press
 */
UFT_WASM_EXPORT
void uft_web_on_keypress(uintptr_t handle, int key);

/*===========================================================================
 * Utility Functions (Internal, not exported)
 *===========================================================================*/

/** Get status color */
uint32_t uft_web_status_color(uft_web_sector_status_t status);

/** Render track map */
void uft_web_render_track_map(uft_web_viewer_t *viewer);

/** Render track detail */
void uft_web_render_track_detail(uft_web_viewer_t *viewer);

/** Render sector hex */
void uft_web_render_hex_view(uft_web_viewer_t *viewer);

/** Render flux graph */
void uft_web_render_flux_graph(uft_web_viewer_t *viewer);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WEB_VIEWER_H */
