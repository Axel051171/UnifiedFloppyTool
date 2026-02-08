/**
 * @file uft_woz_v21.c
 * @brief WOZ v2.1 Flux Timing Implementation
 * 
 * KRITISCHER FIX: WOZ v2.1 optimal bit timing war ignoriert!
 * 
 * WOZ 2.1 Features:
 * - INFO version 3 mit flux_block und largest_flux_track
 * - FLUX chunk mit präzisen Flux-Timings
 * - Per-Track bit timing für variable speed Zonen
 */

#include "uft/formats/variants/parsers/uft_woz_v21.h"
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

// ============================================================================
// Probing
// ============================================================================

bool woz_probe(const uint8_t* data, size_t size) {
    if (!data || size < 12) return false;
    
    uint32_t magic = read_le32(data);
    uint32_t tail = read_le32(data + 4);
    
    if (tail != WOZ_TAIL) return false;
    
    return (magic == WOZ1_MAGIC || magic == WOZ2_MAGIC);
}

int woz_get_version(const uint8_t* data, size_t size) {
    if (!woz_probe(data, size)) return 0;
    
    uint32_t magic = read_le32(data);
    
    if (magic == WOZ1_MAGIC) return 1;
    
    if (magic == WOZ2_MAGIC) {
        // Check INFO version for v2.1
        size_t pos = 12;
        while (pos + 8 <= size) {
            uint32_t chunk_id = read_le32(data + pos);
            uint32_t chunk_size = read_le32(data + pos + 4);
            
            if (chunk_id == WOZ_CHUNK_INFO && chunk_size >= 1) {
                uint8_t info_version = data[pos + 8];
                if (info_version >= 3) return 21;  // v2.1
                return 2;
            }
            
            pos += 8 + chunk_size;
        }
        return 2;
    }
    
    return 0;
}

// ============================================================================
// Chunk Parsing
// ============================================================================

typedef struct {
    uint32_t id;
    uint32_t size;
    size_t offset;
} chunk_info_t;

static bool find_chunk(const uint8_t* data, size_t size, uint32_t chunk_id,
                       chunk_info_t* out) {
    size_t pos = 12;  // Skip header
    
    while (pos + 8 <= size) {
        uint32_t id = read_le32(data + pos);
        uint32_t chunk_size = read_le32(data + pos + 4);
        
        if (id == chunk_id) {
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
// Main Parser
// ============================================================================

woz_image_t* woz_open(const uint8_t* data, size_t size) {
    if (!woz_probe(data, size)) return NULL;
    
    woz_image_t* img = calloc(1, sizeof(woz_image_t));
    if (!img) return NULL;
    
    img->magic = read_le32(data);
    img->woz_version = (img->magic == WOZ1_MAGIC) ? 1 : 2;
    
    // Parse INFO chunk
    chunk_info_t info_chunk;
    if (!find_chunk(data, size, WOZ_CHUNK_INFO, &info_chunk)) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Missing INFO chunk");
        img->is_valid = false;
        return img;
    }
    
    // Copy INFO data
    size_t info_copy_size = info_chunk.size < sizeof(img->info) ? 
                            info_chunk.size : sizeof(img->info);
    memcpy(&img->info, data + info_chunk.offset, info_copy_size);
    
    // Check for v2.1
    if (img->info.info_version >= 3) {
        img->woz_version = 21;  // Signal v2.1
        img->has_flux_timing = true;
    }
    
    // Get default bit timing
    if (img->woz_version >= 2) {
        img->default_bit_timing = img->info.optimal_bit_timing;
        if (img->default_bit_timing == 0) {
            // Use default based on disk type
            img->default_bit_timing = (img->info.disk_type == WOZ_DISK_525) ?
                                       WOZ_DEFAULT_BIT_TIMING_525 :
                                       WOZ_DEFAULT_BIT_TIMING_35;
        }
    } else {
        img->default_bit_timing = (img->info.disk_type == WOZ_DISK_525) ?
                                   WOZ_DEFAULT_BIT_TIMING_525 :
                                   WOZ_DEFAULT_BIT_TIMING_35;
    }
    
    img->default_bit_cell_ns = img->default_bit_timing * 125.0;
    
    // Parse TMAP chunk
    chunk_info_t tmap_chunk;
    if (find_chunk(data, size, WOZ_CHUNK_TMAP, &tmap_chunk)) {
        size_t copy_size = tmap_chunk.size < 160 ? tmap_chunk.size : 160;
        memcpy(img->tmap, data + tmap_chunk.offset, copy_size);
    } else {
        // No TMAP - use sequential mapping
        for (int i = 0; i < 160; i++) {
            img->tmap[i] = (i % 4 == 0) ? (i / 4) : 0xFF;
        }
    }
    
    // Count actual tracks
    int max_track = -1;
    for (int i = 0; i < 160; i++) {
        if (img->tmap[i] != 0xFF && img->tmap[i] > max_track) {
            max_track = img->tmap[i];
        }
    }
    img->num_tracks = max_track + 1;
    
    if (img->num_tracks <= 0) {
        snprintf(img->error_msg, sizeof(img->error_msg), "No tracks found");
        img->is_valid = false;
        return img;
    }
    
    // Allocate tracks
    img->tracks = calloc(img->num_tracks, sizeof(woz_track_t));
    if (!img->tracks) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Memory allocation failed");
        img->is_valid = false;
        return img;
    }
    
    // Parse TRKS chunk
    chunk_info_t trks_chunk;
    if (!find_chunk(data, size, WOZ_CHUNK_TRKS, &trks_chunk)) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Missing TRKS chunk");
        img->is_valid = false;
        return img;
    }
    
    if (img->woz_version == 1) {
        // WOZ 1: Fixed 6656-byte tracks
        for (int t = 0; t < img->num_tracks && t < 35; t++) {
            size_t track_offset = trks_chunk.offset + t * 6656;
            if (track_offset + 6646 > size) break;
            
            // Bytes 0-6645: Track data
            // Bytes 6646-6647: Bit count (LE)
            uint16_t bit_count = read_le16(data + track_offset + 6646);
            
            img->tracks[t].bits = malloc(6646);
            if (img->tracks[t].bits) {
                memcpy(img->tracks[t].bits, data + track_offset, 6646);
                img->tracks[t].bit_count = bit_count;
                img->tracks[t].bit_timing = img->default_bit_timing;
                img->tracks[t].bit_cell_ns = img->default_bit_cell_ns;
                img->tracks[t].is_valid = true;
            }
        }
    } else {
        // WOZ 2/2.1: Variable-size tracks with TRK entries
        const uint8_t* trk_entries = data + trks_chunk.offset;
        size_t entry_count = trks_chunk.size / sizeof(woz2_trk_entry_t);
        
        if (entry_count > 160) entry_count = 160;
        
        for (size_t t = 0; t < entry_count && (int)t < img->num_tracks; t++) {
            woz2_trk_entry_t entry;
            memcpy(&entry, trk_entries + t * sizeof(woz2_trk_entry_t),
                   sizeof(woz2_trk_entry_t));
            
            uint16_t start_block = read_le16((uint8_t*)&entry.starting_block);
            uint16_t block_count = read_le16((uint8_t*)&entry.block_count);
            uint32_t bit_count = read_le32((uint8_t*)&entry.bit_count);
            
            if (start_block == 0 || block_count == 0) continue;
            
            size_t track_offset = start_block * 512;
            size_t track_size = block_count * 512;
            
            if (track_offset + track_size > size) continue;
            
            img->tracks[t].bits = malloc(track_size);
            if (img->tracks[t].bits) {
                memcpy(img->tracks[t].bits, data + track_offset, track_size);
                img->tracks[t].bit_count = bit_count;
                img->tracks[t].bit_timing = img->default_bit_timing;
                img->tracks[t].bit_cell_ns = img->default_bit_cell_ns;
                img->tracks[t].is_valid = true;
            }
        }
    }
    
    // Parse FLUX chunk (v2.1 only)
    if (img->woz_version == 21) {
        chunk_info_t flux_chunk;
        if (find_chunk(data, size, WOZ_CHUNK_FLUX, &flux_chunk)) {
            const uint8_t* flux_entries = data + flux_chunk.offset;
            size_t entry_count = flux_chunk.size / sizeof(woz_flux_entry_t);
            
            for (size_t t = 0; t < entry_count && (int)t < img->num_tracks; t++) {
                woz_flux_entry_t entry;
                memcpy(&entry, flux_entries + t * sizeof(woz_flux_entry_t),
                       sizeof(woz_flux_entry_t));
                
                uint16_t start_block = read_le16((uint8_t*)&entry.starting_block);
                uint16_t block_count = read_le16((uint8_t*)&entry.block_count);
                uint32_t flux_count = read_le32((uint8_t*)&entry.flux_count);
                
                if (start_block == 0 || block_count == 0 || flux_count == 0) continue;
                
                size_t flux_offset = start_block * 512;
                
                if (flux_offset + flux_count * 2 > size) continue;
                
                // Flux data is 16-bit tick values
                img->tracks[t].flux_timing = malloc(flux_count * sizeof(uint32_t));
                if (img->tracks[t].flux_timing) {
                    const uint8_t* flux_data = data + flux_offset;
                    for (size_t f = 0; f < flux_count; f++) {
                        img->tracks[t].flux_timing[f] = read_le16(flux_data + f * 2);
                    }
                    img->tracks[t].flux_count = flux_count;
                    img->tracks[t].has_flux_data = true;
                }
            }
        }
        
        // Allocate per-track timing array
        img->track_bit_timing = calloc(img->num_tracks, sizeof(uint8_t));
        if (img->track_bit_timing) {
            for (int t = 0; t < img->num_tracks; t++) {
                img->track_bit_timing[t] = img->default_bit_timing;
            }
        }
    }
    
    img->is_valid = true;
    return img;
}

bool woz_get_track(const woz_image_t* img, int quarter_track,
                   woz_track_t* track_out) {
    if (!img || !img->is_valid || !track_out) return false;
    if (quarter_track < 0 || quarter_track >= 160) return false;
    
    uint8_t track_idx = img->tmap[quarter_track];
    if (track_idx == 0xFF || track_idx >= img->num_tracks) return false;
    
    *track_out = img->tracks[track_idx];
    return track_out->is_valid;
}

bool woz_to_flux_timed(const woz_image_t* img, int quarter_track,
                       uint32_t** flux_ns_out, size_t* count_out) {
    if (!img || !img->is_valid || !flux_ns_out || !count_out) return false;
    
    woz_track_t track;
    if (!woz_get_track(img, quarter_track, &track)) return false;
    
    // If we have direct flux data (v2.1), convert ticks to nanoseconds
    if (track.has_flux_data && track.flux_timing) {
        uint32_t* flux_ns = malloc(track.flux_count * sizeof(uint32_t));
        if (!flux_ns) return false;
        
        // Flux tick = 125ns in WOZ 2.1
        for (size_t i = 0; i < track.flux_count; i++) {
            flux_ns[i] = track.flux_timing[i] * 125;
        }
        
        *flux_ns_out = flux_ns;
        *count_out = track.flux_count;
        return true;
    }
    
    // Otherwise, convert bits to flux using bit timing
    if (!track.bits || track.bit_count == 0) return false;
    
    // Allocate worst case (one flux per bit)
    uint32_t* flux_ns = malloc(track.bit_count * sizeof(uint32_t));
    if (!flux_ns) return false;
    
    size_t flux_count = 0;
    uint32_t accumulated_ns = 0;
    double bit_cell_ns = track.bit_cell_ns;
    
    for (size_t i = 0; i < track.bit_count; i++) {
        size_t byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        
        uint8_t bit = (track.bits[byte_idx] >> bit_idx) & 1;
        
        accumulated_ns += (uint32_t)bit_cell_ns;
        
        if (bit) {
            flux_ns[flux_count++] = accumulated_ns;
            accumulated_ns = 0;
        }
    }
    
    // Shrink to actual size
    *flux_ns_out = realloc(flux_ns, flux_count * sizeof(uint32_t));
    if (!*flux_ns_out) *flux_ns_out = flux_ns;
    *count_out = flux_count;
    
    return true;
}

uint8_t woz_get_track_timing(const woz_image_t* img, int quarter_track) {
    if (!img || !img->is_valid) return 0;
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

bool woz_has_flux_timing(const woz_image_t* img) {
    return img && img->has_flux_timing;
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
    free(img);
}
