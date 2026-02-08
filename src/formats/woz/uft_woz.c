/**
 * @file uft_woz.c
 * @brief WOZ v2.1 Full Implementation
 * 
 * ROADMAP F1.3 - Priority P0
 * 
 * Based on existing variant parser, now with full API.
 */

#include "uft/formats/uft_woz.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Helpers
// ============================================================================

static inline uint16_t read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Chunk info
typedef struct {
    uint32_t id;
    uint32_t size;
    size_t offset;
} chunk_info_t;

static bool find_chunk(const uint8_t* data, size_t size, uint32_t id,
                       chunk_info_t* out) {
    size_t pos = 12;
    
    while (pos + 8 <= size) {
        uint32_t chunk_id = read_le32(data + pos);
        uint32_t chunk_size = read_le32(data + pos + 4);
        
        if (chunk_id == id) {
            out->id = id;
            out->size = chunk_size;
            out->offset = pos + 8;
            return true;
        }
        
        pos += 8 + chunk_size;
    }
    
    return false;
}

// ============================================================================
// Detection
// ============================================================================

int woz_detect_variant(const uint8_t* data, size_t size,
                       woz_detect_result_t* result) {
    if (!data || !result || size < 12) return -1;
    
    memset(result, 0, sizeof(*result));
    
    uint32_t magic = read_le32(data);
    uint32_t tail = read_le32(data + 4);
    
    if (tail != WOZ_TAIL) return -1;
    
    if (magic == WOZ1_MAGIC) {
        result->variant = WOZ_VAR_V1;
        result->woz_version = 10;
        result->confidence = 100;
    } else if (magic == WOZ2_MAGIC) {
        result->variant = WOZ_VAR_V2;
        result->woz_version = 20;
        result->confidence = 100;
        
        // Check for v2.1 (INFO version 3)
        chunk_info_t info_chunk;
        if (find_chunk(data, size, WOZ_CHUNK_INFO, &info_chunk)) {
            if (info_chunk.size >= 1) {
                uint8_t info_version = data[info_chunk.offset];
                if (info_version >= 3) {
                    result->variant = WOZ_VAR_V21 | WOZ_VAR_FLUX_TIMING;
                    result->woz_version = 21;
                    result->has_flux = true;
                }
            }
            
            // Disk type
            if (info_chunk.size >= 2) {
                result->disk_type = data[info_chunk.offset + 1];
                if (result->disk_type == WOZ_DISK_525) {
                    result->variant |= WOZ_VAR_525;
                } else if (result->disk_type == WOZ_DISK_35) {
                    result->variant |= WOZ_VAR_35;
                }
            }
        }
    } else {
        return -1;
    }
    
    snprintf(result->explanation, sizeof(result->explanation),
             "WOZ %d.%d %s%s",
             result->woz_version / 10, result->woz_version % 10,
             result->disk_type == WOZ_DISK_525 ? "5.25\"" : "3.5\"",
             result->has_flux ? " with flux timing" : "");
    
    return 0;
}

// ============================================================================
// Open/Close
// ============================================================================

woz_image_t* woz_open_memory(const uint8_t* data, size_t size) {
    woz_detect_result_t detect;
    if (woz_detect_variant(data, size, &detect) != 0) {
        return NULL;
    }
    
    woz_image_t* img = calloc(1, sizeof(woz_image_t));
    if (!img) return NULL;
    
    img->data = malloc(size);
    if (!img->data) {
        free(img);
        return NULL;
    }
    memcpy(img->data, data, size);
    img->data_size = size;
    
    img->variant = detect.variant;
    img->confidence = detect.confidence;
    
    // Parse INFO chunk
    chunk_info_t info_chunk;
    if (find_chunk(data, size, WOZ_CHUNK_INFO, &info_chunk)) {
        const uint8_t* info = data + info_chunk.offset;
        
        img->version.info_version = info[0];
        img->info.disk_type = info[1];
        img->info.write_protected = info[2];
        img->info.synchronized = info[3];
        img->info.cleaned = info[4];
        memcpy(img->info.creator, info + 5, 32);
        img->info.creator[32] = '\0';
        
        if (img->version.info_version >= 2 && info_chunk.size >= 39) {
            img->info.disk_sides = info[37];
            img->info.boot_sector_format = info[38];
            img->info.optimal_bit_timing = info[39];
            img->info.compatible_hardware = read_le16(info + 40);
            img->info.required_ram = read_le16(info + 42);
            img->info.largest_track = read_le16(info + 44);
        }
        
        if (img->version.info_version >= 3) {
            img->version.has_flux_timing = true;
            img->has_flux_timing = true;
        }
    }
    
    // Set version
    uint32_t magic = read_le32(data);
    img->version.major = (magic == WOZ1_MAGIC) ? 1 : 2;
    img->version.minor = (img->version.info_version >= 3) ? 1 : 0;
    
    // Default timing
    img->default_bit_timing = img->info.optimal_bit_timing;
    if (img->default_bit_timing == 0) {
        img->default_bit_timing = (img->info.disk_type == WOZ_DISK_525) ?
                                   WOZ_TIMING_525 : WOZ_TIMING_35;
    }
    img->default_bit_cell_ns = img->default_bit_timing * 125.0;
    
    // Parse TMAP
    chunk_info_t tmap_chunk;
    if (find_chunk(data, size, WOZ_CHUNK_TMAP, &tmap_chunk)) {
        size_t copy = tmap_chunk.size < 160 ? tmap_chunk.size : 160;
        memcpy(img->tmap, data + tmap_chunk.offset, copy);
    } else {
        // Default sequential mapping
        for (int i = 0; i < 160; i++) {
            img->tmap[i] = (i % 4 == 0) ? (i / 4) : 0xFF;
        }
    }
    
    // Count tracks
    int max_track = -1;
    for (int i = 0; i < 160; i++) {
        if (img->tmap[i] != 0xFF && img->tmap[i] > max_track) {
            max_track = img->tmap[i];
        }
    }
    img->num_tracks = max_track + 1;
    
    if (img->num_tracks <= 0) {
        snprintf(img->error_msg, sizeof(img->error_msg), "No tracks found");
        free(img->data);
        free(img);
        return NULL;
    }
    
    // Allocate tracks
    img->tracks = calloc(img->num_tracks, sizeof(woz_track_t));
    if (!img->tracks) {
        free(img->data);
        free(img);
        return NULL;
    }
    
    // Parse TRKS chunk
    chunk_info_t trks_chunk;
    if (!find_chunk(data, size, WOZ_CHUNK_TRKS, &trks_chunk)) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Missing TRKS chunk");
        free(img->tracks);
        free(img->data);
        free(img);
        return NULL;
    }
    
    if (img->version.major == 1) {
        // WOZ 1: Fixed 6656-byte tracks
        for (int t = 0; t < img->num_tracks && t < 35; t++) {
            size_t track_offset = trks_chunk.offset + t * 6656;
            if (track_offset + 6646 > size) break;
            
            uint16_t bit_count = read_le16(data + track_offset + 6646);
            
            img->tracks[t].bits = malloc(6646);
            if (img->tracks[t].bits) {
                memcpy(img->tracks[t].bits, data + track_offset, 6646);
                img->tracks[t].bit_count = bit_count;
                img->tracks[t].byte_count = 6646;
                img->tracks[t].bit_timing = img->default_bit_timing;
                img->tracks[t].bit_cell_ns = img->default_bit_cell_ns;
                img->tracks[t].is_valid = true;
            }
        }
    } else {
        // WOZ 2/2.1: Variable tracks with TRK entries
        const uint8_t* trk_entries = data + trks_chunk.offset;
        
        for (int t = 0; t < img->num_tracks && t < 160; t++) {
            uint16_t start_block = read_le16(trk_entries + t * 8);
            uint16_t block_count = read_le16(trk_entries + t * 8 + 2);
            uint32_t bit_count = read_le32(trk_entries + t * 8 + 4);
            
            if (start_block == 0 || block_count == 0) continue;
            
            size_t track_offset = start_block * 512;
            size_t track_size = block_count * 512;
            
            if (track_offset + track_size > size) continue;
            
            img->tracks[t].bits = malloc(track_size);
            if (img->tracks[t].bits) {
                memcpy(img->tracks[t].bits, data + track_offset, track_size);
                img->tracks[t].bit_count = bit_count;
                img->tracks[t].byte_count = track_size;
                img->tracks[t].bit_timing = img->default_bit_timing;
                img->tracks[t].bit_cell_ns = img->default_bit_cell_ns;
                img->tracks[t].is_valid = true;
            }
        }
    }
    
    // Parse FLUX chunk (v2.1)
    if (img->has_flux_timing) {
        chunk_info_t flux_chunk;
        if (find_chunk(data, size, WOZ_CHUNK_FLUX, &flux_chunk)) {
            const uint8_t* flux_entries = data + flux_chunk.offset;
            
            for (int t = 0; t < img->num_tracks && t < 160; t++) {
                uint16_t start_block = read_le16(flux_entries + t * 8);
                uint16_t block_count = read_le16(flux_entries + t * 8 + 2);
                uint32_t flux_count = read_le32(flux_entries + t * 8 + 4);
                
                if (start_block == 0 || flux_count == 0) continue;
                
                size_t flux_offset = start_block * 512;
                
                if (flux_offset + flux_count * 2 > size) continue;
                
                img->tracks[t].flux_timing = malloc(flux_count * sizeof(uint32_t));
                if (img->tracks[t].flux_timing) {
                    const uint8_t* flux_data = data + flux_offset;
                    for (size_t f = 0; f < flux_count; f++) {
                        img->tracks[t].flux_timing[f] = read_le16(flux_data + f * 2);
                    }
                    img->tracks[t].flux_count = flux_count;
                    img->tracks[t].has_flux = true;
                }
            }
        }
        
        // Allocate per-track timing
        img->track_bit_timing = calloc(img->num_tracks, sizeof(uint8_t));
        if (img->track_bit_timing) {
            for (int t = 0; t < img->num_tracks; t++) {
                img->track_bit_timing[t] = img->default_bit_timing;
            }
        }
    }
    
    // Parse META chunk
    chunk_info_t meta_chunk;
    if (find_chunk(data, size, WOZ_CHUNK_META, &meta_chunk)) {
        img->has_meta = true;
        // Parse metadata (tab-separated key-value pairs)
        // Simplified: just check for title
        const char* meta_str = (const char*)(data + meta_chunk.offset);
        if (strstr(meta_str, "title")) {
            // Extract title (simplified)
            const char* title_start = strstr(meta_str, "title\t");
            if (title_start) {
                title_start += 6;
                const char* title_end = strchr(title_start, '\n');
                if (title_end) {
                    size_t len = title_end - title_start;
                    if (len > 255) len = 255;
                    img->meta.title = malloc(len + 1);
                    if (img->meta.title) {
                        memcpy(img->meta.title, title_start, len);
                        img->meta.title[len] = '\0';
                    }
                }
            }
        }
    }
    
    img->is_valid = true;
    return img;
}

woz_image_t* woz_open(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    woz_image_t* img = woz_open_memory(data, size);
    free(data);
    
    return img;
}

void woz_close(woz_image_t* img) {
    if (!img) return;
    
    if (img->tracks) {
        for (int t = 0; t < img->num_tracks; t++) {
            free(img->tracks[t].bits);
            free(img->tracks[t].flux_timing);
        }
        free(img->tracks);
    }
    
    free(img->track_bit_timing);
    free(img->optimal_bit_timing_map);
    free(img->meta.title);
    free(img->meta.publisher);
    free(img->data);
    free(img);
}

// ============================================================================
// Version API
// ============================================================================

int woz_get_version(const woz_image_t* img, woz_version_t* version) {
    if (!img || !version) return -1;
    *version = img->version;
    return 0;
}

bool woz_is_v21(const woz_image_t* img) {
    return img && img->version.info_version >= 3;
}

bool woz_has_flux_timing(const woz_image_t* img) {
    return img && img->has_flux_timing;
}

// ============================================================================
// Track API
// ============================================================================

bool woz_get_track(const woz_image_t* img, int quarter_track,
                   woz_track_t* track_out) {
    if (!img || !track_out || quarter_track < 0 || quarter_track >= 160) {
        return false;
    }
    
    uint8_t track_idx = img->tmap[quarter_track];
    if (track_idx == 0xFF || track_idx >= img->num_tracks) {
        return false;
    }
    
    *track_out = img->tracks[track_idx];
    return track_out->is_valid;
}

bool woz_get_track_physical(const woz_image_t* img, int track,
                            woz_track_t* track_out) {
    return woz_get_track(img, track * 4, track_out);
}

uint8_t woz_get_bit_timing(const woz_image_t* img, int quarter_track) {
    if (!img) return 0;
    if (quarter_track < 0 || quarter_track >= 160) return 0;
    
    uint8_t track_idx = img->tmap[quarter_track];
    if (track_idx == 0xFF || track_idx >= img->num_tracks) {
        return img->default_bit_timing;
    }
    
    if (img->track_bit_timing) {
        return img->track_bit_timing[track_idx];
    }
    
    return img->tracks[track_idx].bit_timing;
}

double woz_get_bit_cell_ns(const woz_image_t* img, int quarter_track) {
    return woz_get_bit_timing(img, quarter_track) * 125.0;
}

// ============================================================================
// Flux API
// ============================================================================

int woz_to_flux(const woz_image_t* img, int quarter_track,
                uint32_t** flux_ns, size_t* count) {
    return woz_to_flux_timed(img, quarter_track, flux_ns, count);
}

int woz_to_flux_timed(const woz_image_t* img, int quarter_track,
                      uint32_t** flux_ns, size_t* count) {
    if (!img || !flux_ns || !count) return -1;
    
    woz_track_t track;
    if (!woz_get_track(img, quarter_track, &track)) return -1;
    
    // If we have direct flux data, use it
    if (track.has_flux && track.flux_timing) {
        uint32_t* flux = malloc(track.flux_count * sizeof(uint32_t));
        if (!flux) return -1;
        
        for (size_t i = 0; i < track.flux_count; i++) {
            flux[i] = track.flux_timing[i] * 125;  // Ticks to ns
        }
        
        *flux_ns = flux;
        *count = track.flux_count;
        return 0;
    }
    
    // Otherwise convert bits to flux
    if (!track.bits || track.bit_count == 0) return -1;
    
    uint32_t* flux = malloc(track.bit_count * sizeof(uint32_t));
    if (!flux) return -1;
    
    size_t flux_count = 0;
    uint32_t accumulated = 0;
    double bit_cell = track.bit_cell_ns;
    
    for (size_t i = 0; i < track.bit_count; i++) {
        size_t byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        
        uint8_t bit = (track.bits[byte_idx] >> bit_idx) & 1;
        
        accumulated += (uint32_t)bit_cell;
        
        if (bit) {
            flux[flux_count++] = accumulated;
            accumulated = 0;
        }
    }
    
    *flux_ns = realloc(flux, flux_count * sizeof(uint32_t));
    if (!*flux_ns) *flux_ns = flux;
    *count = flux_count;
    
    return 0;
}

// ============================================================================
// Metadata API
// ============================================================================

const char* woz_get_title(const woz_image_t* img) {
    return img && img->meta.title ? img->meta.title : "";
}

const char* woz_get_publisher(const woz_image_t* img) {
    return img && img->meta.publisher ? img->meta.publisher : "";
}

// ============================================================================
// Utility
// ============================================================================

const char* woz_disk_type_str(uint8_t type) {
    switch (type) {
        case WOZ_DISK_525: return "5.25\"";
        case WOZ_DISK_35:  return "3.5\"";
        default:           return "Unknown";
    }
}

const char* woz_variant_name(woz_variant_t variant) {
    if (variant & WOZ_VAR_V21) return "WOZ 2.1";
    if (variant & WOZ_VAR_V2) return "WOZ 2.0";
    if (variant & WOZ_VAR_V1) return "WOZ 1.0";
    return "Unknown";
}
