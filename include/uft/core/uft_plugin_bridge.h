/**
 * @file uft_plugin_bridge.h
 * @brief Bridge between uft_unified_types.h and the plugin API (uft_disk_t)
 *
 * Files that use both uft_disk_image_t (unified types) and uft_disk_t
 * (plugin callbacks) include this instead of mixing incompatible headers.
 *
 * Provides a local uft_disk_t definition compatible with the plugin API
 * while keeping uft_unified_types.h's definitions of uft_track_t,
 * uft_sector_t, etc.
 */

#ifndef UFT_PLUGIN_BRIDGE_H
#define UFT_PLUGIN_BRIDGE_H

#include "uft/core/uft_unified_types.h"
#include "uft/core/uft_error_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Plugin API geometry (matches uft_types.h uft_geometry_t layout)
 * ============================================================================ */

#ifndef UFT_GEOMETRY_T_DEFINED
#define UFT_GEOMETRY_T_DEFINED
typedef struct {
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;
    uint32_t total_sectors;
    bool     double_step;
} uft_geometry_t;
#endif

/* ============================================================================
 * Plugin API encoding type
 * ============================================================================ */

#ifndef UFT_ENCODING_TYPE_DEFINED
#define UFT_ENCODING_TYPE_DEFINED
#ifndef uft_encoding_t
typedef uint8_t uft_encoding_t;
#endif
#endif

/* ============================================================================
 * Plugin API disk handle (matches uft_disk.h struct uft_disk)
 * ============================================================================ */

#ifndef UFT_DISK_STRUCT_DEFINED
#define UFT_DISK_STRUCT_DEFINED

typedef struct uft_writer_backend uft_writer_backend_t;
typedef void (*uft_progress_fn)(void*, int, int);
typedef void (*uft_log_fn)(void*, int, const char*);

/* NOTE (Phase 1): struct uft_disk definition removed — now lives only
 * in uft_format_plugin.h (canonical, all legacy fields merged). */
struct uft_disk;  /* forward decl only */
typedef struct uft_disk uft_disk_t;

#endif /* UFT_DISK_STRUCT_DEFINED */

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLUGIN_BRIDGE_H */
