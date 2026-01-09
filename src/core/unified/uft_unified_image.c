#include "uft_error.h"
#include "uft_error_compat.h"
/**
 * @file uft_unified_image.c
 * @brief Unified Image Model Implementation
 * 
 * Dieser Code vereinheitlicht uft_disk_t und flux_disk_t in ein 
 * layer-basiertes Image-Modell.
 * 
 * @version 1.0.0
 * @date 2025
 */

#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include "uft/uft_format_plugin.h"
#include "uft/flux_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Internal Helpers
// ============================================================================

static inline size_t track_index(const uft_unified_image_t* img, int cyl, int head) {
    return (size_t)cyl * img->geometry.heads + head;
}

static void free_flux_revolution(uft_flux_revolution_t* rev) {
    if (rev) {
        free(rev->transitions);
        rev->transitions = NULL;
        rev->count = 0;
        rev->capacity = 0;
    }
}

static void free_flux_track_data(uft_flux_track_data_t* flux) {
    if (!flux) return;
    
    if (flux->revolutions) {
        for (size_t i = 0; i < flux->revolution_count; i++) {
            free_flux_revolution(&flux->revolutions[i]);
        }
        free(flux->revolutions);
    }
    free(flux);
}

static void free_bitstream_track(uft_bitstream_track_t* bs) {
    if (!bs) return;
    free(bs->bits);
    free(bs->sync_positions);
    free(bs);
}

static void free_sector_track(uft_track_t* track) {
    if (!track) return;
    
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    free(track->raw_data);
    free(track->flux);
    free(track);
}

static void free_unified_track(uft_unified_track_t* track) {
    if (!track) return;
    
    free_flux_track_data(track->flux);
    free_bitstream_track(track->bitstream);
    free_sector_track(track->sectors);
    free(track);
}

// ============================================================================
// Flux Track API
// ============================================================================

uft_flux_track_data_t* uft_flux_track_create(int cyl, int head) {
    uft_flux_track_data_t* track = calloc(1, sizeof(uft_flux_track_data_t));
    if (!track) return NULL;
    
    track->cylinder = cyl;
    track->head = head;
    track->revolution_capacity = 8;  // Initial capacity
    track->revolutions = calloc(track->revolution_capacity, sizeof(uft_flux_revolution_t));
    
    if (!track->revolutions) {
        free(track);
        return NULL;
    }
    
    return track;
}

void uft_flux_track_destroy(uft_flux_track_data_t* track) {
    free_flux_track_data(track);
}

uft_error_t uft_flux_track_add_revolution(uft_flux_track_data_t* track,
                                           const uint32_t* samples,
                                           size_t count,
                                           uint32_t sample_rate_hz) {
    if (!track || !samples || count == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    // Grow array if needed
    if (track->revolution_count >= track->revolution_capacity) {
        size_t new_cap = track->revolution_capacity * 2;
        uft_flux_revolution_t* new_revs = realloc(track->revolutions,
                                                   new_cap * sizeof(uft_flux_revolution_t));
        if (!new_revs) return UFT_ERROR_NO_MEMORY;
        
        track->revolutions = new_revs;
        track->revolution_capacity = new_cap;
        
        // Zero new entries
        memset(&track->revolutions[track->revolution_count], 0,
               (new_cap - track->revolution_count) * sizeof(uft_flux_revolution_t));
    }
    
    uft_flux_revolution_t* rev = &track->revolutions[track->revolution_count];
    
    // Allocate transitions
    rev->transitions = malloc(count * sizeof(uft_flux_transition_t));
    if (!rev->transitions) return UFT_ERROR_NO_MEMORY;
    
    rev->capacity = count;
    rev->count = count;
    
    // Convert samples to nanoseconds
    double ns_per_tick = 1000000000.0 / (double)sample_rate_hz;
    uint64_t total_ns = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t delta_ns = (uint32_t)(samples[i] * ns_per_tick);
        rev->transitions[i].delta_ns = delta_ns;
        rev->transitions[i].flags = 0;
        total_ns += delta_ns;
    }
    
    rev->total_time_ns = total_ns;
    
    // Calculate RPM: 60 / (total_time_in_seconds)
    if (total_ns > 0) {
        double seconds = (double)total_ns / 1000000000.0;
        rev->rpm = 60.0 / seconds;
    } else {
        rev->rpm = 0.0;
    }
    
    track->revolution_count++;
    track->total_transitions += count;
    
    // Update statistics
    if (track->revolution_count == 1) {
        track->avg_rpm = rev->rpm;
    } else {
        // Running average
        track->avg_rpm = (track->avg_rpm * (track->revolution_count - 1) + rev->rpm) 
                         / track->revolution_count;
    }
    
    track->source_sample_rate_hz = sample_rate_hz;
    
    return UFT_OK;
}

uft_error_t uft_flux_track_normalize(uft_flux_track_data_t* track, uint32_t target_rate_hz) {
    if (!track || target_rate_hz == 0) return UFT_ERROR_INVALID_ARG;
    if (track->source_sample_rate_hz == target_rate_hz) return UFT_OK;
    
    double scale = (double)target_rate_hz / (double)track->source_sample_rate_hz;
    
    for (size_t r = 0; r < track->revolution_count; r++) {
        uft_flux_revolution_t* rev = &track->revolutions[r];
        uint64_t new_total = 0;
        
        for (size_t i = 0; i < rev->count; i++) {
            uint32_t new_delta = (uint32_t)(rev->transitions[i].delta_ns * scale);
            rev->transitions[i].delta_ns = new_delta;
            new_total += new_delta;
        }
        
        rev->total_time_ns = new_total;
    }
    
    track->source_sample_rate_hz = target_rate_hz;
    return UFT_OK;
}

// ============================================================================
// Unified Image Lifecycle
// ============================================================================

uft_unified_image_t* uft_image_create(void) {
    uft_unified_image_t* img = calloc(1, sizeof(uft_unified_image_t));
    if (!img) return NULL;
    
    img->detected_format = UFT_FORMAT_UNKNOWN;
    img->source_format = UFT_FORMAT_UNKNOWN;
    img->detection_confidence = 0;
    
    return img;
}

void uft_image_destroy(uft_unified_image_t* img) {
    if (!img) return;
    
    // Free tracks
    if (img->tracks) {
        for (size_t i = 0; i < img->track_count; i++) {
            free_unified_track(img->tracks[i]);
        }
        free(img->tracks);
    }
    
    free(img->path);
    free(img);
}

// ============================================================================
// Image Open/Save
// ============================================================================

uft_error_t uft_image_open(uft_unified_image_t* img, const char* path) {
    if (!img || !path) return UFT_ERROR_NULL_POINTER;
    
    // Detect format
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[4096];
    size_t header_size = fread(header, 1, sizeof(header), f);
    
    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = (size_t)ftell(f);
    fclose(f);
    
    if (header_size == 0) {
        return UFT_ERROR_FILE_READ;
    }
    
    // Auto-detect format
    uft_format_t format = UFT_FORMAT_UNKNOWN;
    int confidence = 0;
    
    // Try all registered format plugins
    extern const uft_format_plugin_t* uft_format_plugins[];
    extern size_t uft_format_plugin_count;
    
    for (size_t i = 0; i < uft_format_plugin_count; i++) {
        const uft_format_plugin_t* plugin = uft_format_plugins[i];
        if (plugin && plugin->probe) {
            int plugin_confidence = 0;
            if (plugin->probe(header, header_size, file_size, &plugin_confidence)) {
                if (plugin_confidence > confidence) {
                    confidence = plugin_confidence;
                    format = plugin->format;
                    img->provider = plugin;
                }
            }
        }
    }
    
    if (format == UFT_FORMAT_UNKNOWN || confidence < 50) {
        return UFT_ERROR_FORMAT_UNKNOWN;
    }
    
    img->detected_format = format;
    img->source_format = format;
    img->detection_confidence = confidence;
    
    // Duplicate path
    img->path = strdup(path);
    if (!img->path) return UFT_ERROR_NO_MEMORY;
    
    // Open via plugin
    const uft_format_plugin_t* plugin = img->provider;
    if (plugin && plugin->open) {
        // Create temporary disk for legacy plugin
        uft_disk_t temp_disk;
        memset(&temp_disk, 0, sizeof(temp_disk));
        
        uft_error_t err = plugin->open(&temp_disk, path, true);
        if (err != UFT_OK) {
            free(img->path);
            img->path = NULL;
            return err;
        }
        
        // Copy geometry
        img->geometry = temp_disk.geometry;
        
        // Allocate tracks
        size_t total_tracks = (size_t)img->geometry.cylinders * img->geometry.heads;
        img->tracks = calloc(total_tracks, sizeof(uft_unified_track_t*));
        if (!img->tracks) {
            if (plugin->close) plugin->close(&temp_disk);
            free(img->path);
            img->path = NULL;
            return UFT_ERROR_NO_MEMORY;
        }
        img->track_count = total_tracks;
        
        // Store provider data
        img->provider_data = temp_disk.plugin_data;
        
        // Determine available layers
        img->available_layers = UFT_LAYER_SECTOR;
        img->primary_layer = UFT_LAYER_SECTOR;
    }
    
    return UFT_OK;
}

uft_error_t uft_image_save(uft_unified_image_t* img, const char* path, uft_format_t format) {
    if (!img || !path) return UFT_ERROR_NULL_POINTER;
    
    if (format == UFT_FORMAT_AUTO) {
        format = img->source_format;
    }
    
    // Find plugin for target format
    extern const uft_format_plugin_t* uft_format_plugins[];
    extern size_t uft_format_plugin_count;
    
    const uft_format_plugin_t* plugin = NULL;
    for (size_t i = 0; i < uft_format_plugin_count; i++) {
        if (uft_format_plugins[i] && uft_format_plugins[i]->format == format) {
            plugin = uft_format_plugins[i];
            break;
        }
    }
    
    if (!plugin) {
        return UFT_ERROR_FORMAT_NOT_SUPPORTED;
    }
    
    if (!plugin->create || !(plugin->capabilities & UFT_FORMAT_CAP_WRITE)) {
        return UFT_ERROR_READ_ONLY;
    }
    
    // Create output via plugin
    uft_disk_t temp_disk;
    memset(&temp_disk, 0, sizeof(temp_disk));
    
    uft_error_t err = plugin->create(&temp_disk, path, &img->geometry);
    if (err != UFT_OK) return err;
    
    // Write all tracks
    for (int cyl = 0; cyl < img->geometry.cylinders; cyl++) {
        for (int head = 0; head < img->geometry.heads; head++) {
            uft_track_t* track = NULL;
            err = uft_image_get_sector_track(img, cyl, head, &track);
            if (err == UFT_OK && track) {
                plugin->write_track(&temp_disk, cyl, head, track);
            }
        }
    }
    
    // Close
    if (plugin->close) {
        plugin->close(&temp_disk);
    }
    
    img->modified = false;
    return UFT_OK;
}

// ============================================================================
// Layer Management
// ============================================================================

bool uft_image_has_layer(const uft_unified_image_t* img, uft_layer_t layer) {
    if (!img) return false;
    return (img->available_layers & layer) != 0;
}

/**
 * @brief Convert flux to bitstream for a single track using PLL
 */
static uft_error_t convert_flux_to_bitstream_track(uft_unified_track_t* track) {
    if (!track || !track->flux || track->flux->revolution_count == 0) {
        return UFT_ERROR_NO_DATA;
    }
    
    /* Use first revolution for now */
    uft_flux_revolution_t* rev = &track->flux->revolutions[0];
    if (!rev->transitions || rev->count == 0) {
        return UFT_ERROR_NO_DATA;
    }
    
    /* Estimate output size */
    size_t est_bits = rev->count * 2;
    size_t est_bytes = (est_bits + 7) / 8;
    
    /* Allocate bitstream */
    if (track->bitstream.data) free(track->bitstream.data);
    track->bitstream.data = calloc(est_bytes, 1);
    if (!track->bitstream.data) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    /* Simple PLL-like decode: convert transitions to cells */
    uint32_t nominal_ns = 2000;  /* 2µs for DD MFM */
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < rev->count && bit_pos < est_bits; i++) {
        uint32_t delta = rev->transitions[i];
        int cells = (delta + nominal_ns / 2) / nominal_ns;
        
        if (cells >= 1 && cells <= 4) {
            /* Output '1' followed by (cells-1) zeros */
            track->bitstream.data[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
            bit_pos++;
            
            for (int c = 1; c < cells && bit_pos < est_bits; c++) {
                bit_pos++;  /* Zero bit */
            }
        }
    }
    
    track->bitstream.length = bit_pos;
    track->bitstream.encoding = UFT_ENCODING_MFM;
    track->available_layers |= UFT_LAYER_BITSTREAM;
    
    return UFT_OK;
}

/**
 * @brief Convert bitstream to sectors for a single track
 */
static uft_error_t convert_bitstream_to_sectors_track(uft_unified_track_t* track) {
    if (!track || !track->bitstream.data || track->bitstream.length == 0) {
        return UFT_ERROR_NO_DATA;
    }
    
    /* Look for MFM sync patterns and extract sectors */
    /* Simplified: look for 0xA1A1A1 sync pattern */
    
    uint32_t sync_pattern = 0;
    size_t sector_count = 0;
    
    /* Count potential sectors first */
    for (size_t i = 0; i < track->bitstream.length && i < track->bitstream.length - 48; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = i % 8;
        
        /* Shift in bit */
        uint8_t bit = (track->bitstream.data[byte_idx] >> (7 - bit_idx)) & 1;
        sync_pattern = (sync_pattern << 1) | bit;
        
        /* Check for MFM A1 sync (0x4489 x3 = 48 bits) */
        if ((sync_pattern & 0xFFFF) == 0x4489) {
            sector_count++;
            i += 15;  /* Skip past sync */
        }
    }
    
    if (sector_count == 0) {
        return UFT_ERROR_NO_SECTORS;
    }
    
    /* Allocate sector array (simplified - actual implementation needs full decode) */
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    track->sectors = calloc(sector_count, sizeof(uft_sector_data_t));
    if (!track->sectors) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    track->sector_count = sector_count;
    track->available_layers |= UFT_LAYER_SECTOR;
    
    return UFT_OK;
}

uft_error_t uft_image_ensure_layer(uft_unified_image_t* img, uft_layer_t target) {
    if (!img) return UFT_ERROR_NULL_POINTER;
    
    if (uft_image_has_layer(img, target)) {
        return UFT_OK;  /* Already available */
    }
    
    uft_error_t result = UFT_OK;
    
    /* Layer conversion logic */
    switch (target) {
        case UFT_LAYER_SECTOR:
            if (uft_image_has_layer(img, UFT_LAYER_BITSTREAM)) {
                /* Bitstream -> Sectors */
                for (size_t i = 0; i < img->track_count && result == UFT_OK; i++) {
                    if (img->tracks[i]) {
                        result = convert_bitstream_to_sectors_track(img->tracks[i]);
                    }
                }
                if (result == UFT_OK) {
                    img->available_layers |= UFT_LAYER_SECTOR;
                }
                return result;
            } else if (uft_image_has_layer(img, UFT_LAYER_FLUX)) {
                /* Flux -> Bitstream -> Sectors */
                result = uft_image_ensure_layer(img, UFT_LAYER_BITSTREAM);
                if (result != UFT_OK) return result;
                return uft_image_ensure_layer(img, UFT_LAYER_SECTOR);
            }
            break;
            
        case UFT_LAYER_BITSTREAM:
            if (uft_image_has_layer(img, UFT_LAYER_FLUX)) {
                /* Flux -> Bitstream (PLL decode) */
                for (size_t i = 0; i < img->track_count && result == UFT_OK; i++) {
                    if (img->tracks[i]) {
                        result = convert_flux_to_bitstream_track(img->tracks[i]);
                    }
                }
                if (result == UFT_OK) {
                    img->available_layers |= UFT_LAYER_BITSTREAM;
                }
                return result;
            }
            break;
            
        case UFT_LAYER_FLUX:
            /* Synthesis from sectors/bitstream - complex, not yet implemented */
            return UFT_ERROR_NOT_IMPLEMENTED;
            
        default:
            return UFT_ERROR_INVALID_ARG;
    }
    
    return UFT_ERROR_NO_DATA;
}

void uft_image_drop_layer(uft_unified_image_t* img, uft_layer_t layer) {
    if (!img) return;
    
    for (size_t i = 0; i < img->track_count; i++) {
        uft_unified_track_t* track = img->tracks[i];
        if (!track) continue;
        
        switch (layer) {
            case UFT_LAYER_FLUX:
                free_flux_track_data(track->flux);
                track->flux = NULL;
                track->available_layers &= ~UFT_LAYER_FLUX;
                break;
                
            case UFT_LAYER_BITSTREAM:
                free_bitstream_track(track->bitstream);
                track->bitstream = NULL;
                track->available_layers &= ~UFT_LAYER_BITSTREAM;
                break;
                
            case UFT_LAYER_SECTOR:
                free_sector_track(track->sectors);
                track->sectors = NULL;
                track->available_layers &= ~UFT_LAYER_SECTOR;
                break;
                
            default:
                break;
        }
    }
    
    img->available_layers &= ~layer;
}

// ============================================================================
// Track Access
// ============================================================================

uft_unified_track_t* uft_image_get_track(uft_unified_image_t* img, int cyl, int head) {
    if (!img || !img->tracks) return NULL;
    if (cyl < 0 || cyl >= img->geometry.cylinders) return NULL;
    if (head < 0 || head >= img->geometry.heads) return NULL;
    
    size_t idx = track_index(img, cyl, head);
    
    // Lazy create track
    if (!img->tracks[idx]) {
        img->tracks[idx] = calloc(1, sizeof(uft_unified_track_t));
        if (img->tracks[idx]) {
            img->tracks[idx]->cylinder = cyl;
            img->tracks[idx]->head = head;
        }
    }
    
    return img->tracks[idx];
}

uft_error_t uft_image_get_flux_track(uft_unified_image_t* img, int cyl, int head,
                                      uft_flux_track_data_t** flux) {
    if (!img || !flux) return UFT_ERROR_NULL_POINTER;
    
    uft_unified_track_t* track = uft_image_get_track(img, cyl, head);
    if (!track) return UFT_ERROR_INVALID_ARG;
    
    if (!track->flux) {
        // Try to load/generate flux data
        if (track->source_layer == UFT_LAYER_FLUX) {
            // Load from source
            return UFT_ERROR_NOT_IMPLEMENTED;
        } else {
            return UFT_ERROR_NO_DATA;
        }
    }
    
    *flux = track->flux;
    return UFT_OK;
}

uft_error_t uft_image_get_sector_track(uft_unified_image_t* img, int cyl, int head,
                                        uft_track_t** sectors) {
    if (!img || !sectors) return UFT_ERROR_NULL_POINTER;
    
    uft_unified_track_t* utrack = uft_image_get_track(img, cyl, head);
    if (!utrack) return UFT_ERROR_INVALID_ARG;
    
    // If sectors not loaded, load via plugin
    if (!utrack->sectors && img->provider) {
        const uft_format_plugin_t* plugin = img->provider;
        
        if (plugin->read_track) {
            utrack->sectors = calloc(1, sizeof(uft_track_t));
            if (!utrack->sectors) return UFT_ERROR_NO_MEMORY;
            
            // Create temporary disk struct for legacy plugin
            uft_disk_t temp_disk;
            memset(&temp_disk, 0, sizeof(temp_disk));
            temp_disk.geometry = img->geometry;
            temp_disk.plugin_data = img->provider_data;
            
            uft_error_t err = plugin->read_track(&temp_disk, cyl, head, utrack->sectors);
            if (err != UFT_OK) {
                free(utrack->sectors);
                utrack->sectors = NULL;
                return err;
            }
            
            utrack->available_layers |= UFT_LAYER_SECTOR;
            utrack->source_layer = UFT_LAYER_SECTOR;
        }
    }
    
    *sectors = utrack->sectors;
    return (*sectors) ? UFT_OK : UFT_ERROR_NO_DATA;
}

// ============================================================================
// Conversion
// ============================================================================

uft_error_t uft_image_convert(const uft_unified_image_t* src,
                               uft_format_t target_format,
                               uft_unified_image_t* dst) {
    if (!src || !dst) return UFT_ERROR_NULL_POINTER;
    
    // Initialize destination
    dst->geometry = src->geometry;
    dst->detected_format = target_format;
    dst->source_format = target_format;
    
    // Allocate tracks
    size_t total = (size_t)dst->geometry.cylinders * dst->geometry.heads;
    dst->tracks = calloc(total, sizeof(uft_unified_track_t*));
    if (!dst->tracks) return UFT_ERROR_NO_MEMORY;
    dst->track_count = total;
    
    // Copy/convert tracks
    for (int cyl = 0; cyl < src->geometry.cylinders; cyl++) {
        for (int head = 0; head < src->geometry.heads; head++) {
            size_t idx = (size_t)cyl * src->geometry.heads + head;
            
            const uft_unified_track_t* src_track = src->tracks[idx];
            if (!src_track) continue;
            
            uft_unified_track_t* dst_track = calloc(1, sizeof(uft_unified_track_t));
            if (!dst_track) {
                // Cleanup and return
                return UFT_ERROR_NO_MEMORY;
            }
            
            dst_track->cylinder = cyl;
            dst_track->head = head;
            
            // Copy sector data if available
            if (src_track->sectors) {
                dst_track->sectors = calloc(1, sizeof(uft_track_t));
                if (dst_track->sectors) {
                    *dst_track->sectors = *src_track->sectors;
                    
                    // Deep copy sector data
                    if (src_track->sectors->sectors && src_track->sectors->sector_count > 0) {
                        dst_track->sectors->sectors = calloc(src_track->sectors->sector_count,
                                                             sizeof(uft_sector_t));
                        if (dst_track->sectors->sectors) {
                            for (size_t s = 0; s < src_track->sectors->sector_count; s++) {
                                dst_track->sectors->sectors[s] = src_track->sectors->sectors[s];
                                if (src_track->sectors->sectors[s].data) {
                                    size_t dsize = src_track->sectors->sectors[s].data_size;
                                    dst_track->sectors->sectors[s].data = malloc(dsize);
                                    if (dst_track->sectors->sectors[s].data) {
                                        memcpy(dst_track->sectors->sectors[s].data,
                                               src_track->sectors->sectors[s].data, dsize);
                                    }
                                }
                            }
                        }
                    }
                    dst_track->available_layers |= UFT_LAYER_SECTOR;
                }
            }
            
            dst->tracks[idx] = dst_track;
        }
    }
    
    dst->available_layers = UFT_LAYER_SECTOR;
    return UFT_OK;
}

bool uft_image_can_convert(const uft_unified_image_t* src,
                            uft_format_t target_format,
                            char* loss_info, size_t loss_info_size) {
    if (!src) return false;
    
    // Basic compatibility check
    bool can = true;
    const char* info = NULL;
    
    // Check if target format supports source data
    switch (target_format) {
        case UFT_FORMAT_IMG:
        case UFT_FORMAT_DSK:
            // Sector-only formats
            if (uft_image_has_layer(src, UFT_LAYER_FLUX)) {
                info = "Flux data will be lost";
            }
            break;
            
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_KRYOFLUX:
            // Flux formats
            if (!uft_image_has_layer(src, UFT_LAYER_FLUX)) {
                info = "Flux data not available, will synthesize";
            }
            break;
            
        default:
            break;
    }
    
    if (loss_info && loss_info_size > 0 && info) {
        snprintf(loss_info, loss_info_size, "%s", info);
    }
    
    return can;
}

// ============================================================================
// Migration from Legacy
// ============================================================================

uft_error_t uft_image_from_disk(uft_unified_image_t* img, const uft_disk_t* disk) {
    if (!img || !disk) return UFT_ERROR_NULL_POINTER;
    
    img->geometry = disk->geometry;
    img->source_format = disk->format;
    img->detected_format = disk->format;
    
    if (disk->path) {
        img->path = strdup(disk->path);
    }
    
    // Allocate tracks
    size_t total = (size_t)img->geometry.cylinders * img->geometry.heads;
    img->tracks = calloc(total, sizeof(uft_unified_track_t*));
    if (!img->tracks) return UFT_ERROR_NO_MEMORY;
    img->track_count = total;
    
    // Copy track cache if available
    if (disk->track_cache) {
        for (size_t i = 0; i < total; i++) {
            if (disk->track_cache[i]) {
                uft_unified_track_t* ut = calloc(1, sizeof(uft_unified_track_t));
                if (ut) {
                    ut->cylinder = i / img->geometry.heads;
                    ut->head = i % img->geometry.heads;
                    ut->sectors = disk->track_cache[i];  // Transfer ownership
                    ut->available_layers = UFT_LAYER_SECTOR;
                    ut->source_layer = UFT_LAYER_SECTOR;
                    img->tracks[i] = ut;
                }
            }
        }
    }
    
    img->available_layers = UFT_LAYER_SECTOR;
    img->primary_layer = UFT_LAYER_SECTOR;
    img->provider = disk->plugin;
    img->provider_data = disk->plugin_data;
    
    return UFT_OK;
}

uft_error_t uft_image_to_disk(const uft_unified_image_t* img, uft_disk_t* disk) {
    if (!img || !disk) return UFT_ERROR_NULL_POINTER;
    
    memset(disk, 0, sizeof(*disk));
    
    disk->geometry = img->geometry;
    disk->format = img->source_format;
    
    if (img->path) {
        disk->path = strdup(img->path);
    }
    
    // Allocate track cache
    size_t total = (size_t)disk->geometry.cylinders * disk->geometry.heads;
    disk->track_cache = calloc(total, sizeof(uft_track_t*));
    if (!disk->track_cache) return UFT_ERROR_NO_MEMORY;
    
    // Note: We don't copy tracks here - they would need to be re-read
    // This is just a shell for the legacy API
    
    disk->plugin = img->provider;
    disk->plugin_data = img->provider_data;
    
    return UFT_OK;
}
