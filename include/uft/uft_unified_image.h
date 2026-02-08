/**
 * @file uft_unified_image.h
 * @brief Einheitliches Image-Modell für Sektor- und Flux-Daten
 * 
 * Dieses Modul vereinheitlicht die bisherigen getrennten Strukturen
 * uft_disk_t und flux_disk_t in einen einzigen, layer-basierten Container.
 * 
 * @version 1.0.0
 * @date 2025
 */

#ifndef UFT_UNIFIED_IMAGE_H
#define UFT_UNIFIED_IMAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// API Version
// ============================================================================

#define UFT_IMAGE_API_VERSION_MAJOR   1
#define UFT_IMAGE_API_VERSION_MINOR   0
#define UFT_IMAGE_API_VERSION         ((UFT_IMAGE_API_VERSION_MAJOR << 16) | UFT_IMAGE_API_VERSION_MINOR)

// ============================================================================
// Layer Definitions
// ============================================================================

typedef enum uft_layer {
    UFT_LAYER_NONE      = 0,
    UFT_LAYER_FLUX      = (1 << 0),  ///< Raw flux transitions
    UFT_LAYER_BITSTREAM = (1 << 1),  ///< Decoded bits (MFM/FM/GCR)
    UFT_LAYER_SECTOR    = (1 << 2),  ///< Decoded sectors with IDs
    UFT_LAYER_BLOCK     = (1 << 3),  ///< Logical blocks (LBA)
    UFT_LAYER_FILE      = (1 << 4),  ///< Filesystem level
} uft_layer_t;

// ============================================================================
// Flux Layer Structures
// ============================================================================

#define UFT_FLUX_FLAG_INDEX     (1 << 0)
#define UFT_FLUX_FLAG_WEAK      (1 << 1)
#define UFT_FLUX_FLAG_MISSING   (1 << 2)

typedef struct uft_flux_transition {
    uint32_t    delta_ns;   ///< Time since last transition (nanoseconds)
    uint8_t     flags;      ///< UFT_FLUX_FLAG_*
} uft_flux_transition_t;

typedef struct uft_flux_revolution {
    uft_flux_transition_t*  transitions;
    size_t                  count;
    size_t                  capacity;
    uint64_t                total_time_ns;
    uint32_t                index_position;
    double                  rpm;
} uft_flux_revolution_t;

typedef struct uft_flux_track_data {
    int                     cylinder;
    int                     head;
    uft_flux_revolution_t*  revolutions;
    size_t                  revolution_count;
    size_t                  revolution_capacity;
    double                  avg_rpm;
    double                  rpm_stddev;
    uint32_t                source_sample_rate_hz;
    char                    source_format[16];
} uft_flux_track_data_t;

// ============================================================================
// Bitstream Layer
// ============================================================================

typedef struct uft_bitstream_track {
    int             cylinder;
    int             head;
    uint8_t*        bits;
    size_t          bit_count;
    size_t          byte_capacity;
    uft_encoding_t  encoding;
    uint32_t        data_rate_bps;
    uint32_t*       sync_positions;
    size_t          sync_count;
} uft_bitstream_track_t;

// ============================================================================
// Unified Track Container
// ============================================================================

typedef struct uft_unified_track {
    int                     cylinder;
    int                     head;
    uint32_t                available_layers;
    uint32_t                dirty_layers;
    uft_layer_t             source_layer;
    uft_flux_track_data_t*  flux;
    uft_bitstream_track_t*  bitstream;
    uft_track_t*            sectors;
} uft_unified_track_t;

// ============================================================================
// Unified Image Container
// ============================================================================

typedef struct uft_unified_image {
    // Metadata
    char*               path;
    uft_format_t        source_format;
    uft_format_t        detected_format;
    int                 detection_confidence;
    
    // Geometry
    uft_geometry_t      geometry;
    
    // Tracks
    uft_unified_track_t** tracks;
    size_t              track_count;
    
    // Layer info
    uint32_t            available_layers;
    uft_layer_t         primary_layer;
    
    // Flux metadata
    struct {
        uint32_t        sample_rate_hz;
        double          avg_rpm;
        char            capture_tool[32];
        char            capture_date[32];
    } flux_meta;
    
    // Sector metadata
    struct {
        uft_encoding_t  encoding;
        uint32_t        total_sectors;
        uint32_t        bad_sectors;
        uint32_t        weak_sectors;
    } sector_meta;
    
    // State
    bool                read_only;
    bool                modified;
    
    // Provider
    const void*         provider;
    void*               provider_data;
    
    // Callbacks
    uft_log_fn          log_fn;
    void*               log_user;
} uft_unified_image_t;

// ============================================================================
// API: Lifecycle
// ============================================================================

uft_unified_image_t*    uft_image_create(void);
void                    uft_image_destroy(uft_unified_image_t* img);
uft_error_t             uft_image_open(uft_unified_image_t* img, const char* path);
uft_error_t             uft_image_save(uft_unified_image_t* img, const char* path, uft_format_t format);

// ============================================================================
// API: Layer Management
// ============================================================================

bool        uft_image_has_layer(const uft_unified_image_t* img, uft_layer_t layer);
uft_error_t uft_image_ensure_layer(uft_unified_image_t* img, uft_layer_t layer);
void        uft_image_drop_layer(uft_unified_image_t* img, uft_layer_t layer);

// ============================================================================
// API: Track Access
// ============================================================================

uft_unified_track_t* uft_image_get_track(uft_unified_image_t* img, int cyl, int head);
uft_error_t uft_image_get_flux_track(uft_unified_image_t* img, int cyl, int head, uft_flux_track_data_t** flux);
uft_error_t uft_image_get_sector_track(uft_unified_image_t* img, int cyl, int head, uft_track_t** sectors);

// ============================================================================
// API: Conversion
// ============================================================================

uft_error_t uft_image_convert(const uft_unified_image_t* src, uft_format_t target_format, uft_unified_image_t* dst);
bool uft_image_can_convert(const uft_unified_image_t* src, uft_format_t target_format, char* loss_info, size_t loss_info_size);

// ============================================================================
// API: Flux Track Helpers
// ============================================================================

uft_flux_track_data_t* uft_flux_track_create(int cyl, int head);
void                   uft_flux_track_destroy(uft_flux_track_data_t* track);
uft_error_t            uft_flux_track_add_revolution(uft_flux_track_data_t* track, const uint32_t* samples, size_t count, uint32_t sample_rate_hz);
uft_error_t            uft_flux_track_normalize(uft_flux_track_data_t* track, uint32_t target_rate_hz);

// ============================================================================
// API: Migration from Legacy
// ============================================================================

uft_error_t uft_image_from_disk(uft_unified_image_t* img, const struct uft_disk* disk);
uft_error_t uft_image_to_disk(const uft_unified_image_t* img, struct uft_disk* disk);

#ifdef __cplusplus
}
#endif

#endif // UFT_UNIFIED_IMAGE_H
