/**
 * @file uft_ipf_ctraw.c
 * @brief IPF CTRaw Format Parser Implementation
 * 
 * KRITISCHER FIX: CTRaw war nur erkannt aber nicht dekodiert!
 * 
 * CTRaw Format:
 * - CAPS header record
 * - INFO record (standard IPF)
 * - CTEI record (CTRaw Extended Info) - sample rate, hardware
 * - DUMP records - raw flux data
 */

#include "uft/formats/variants/parsers/uft_ipf_ctraw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Helpers
// ============================================================================

static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static inline uint16_t read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

// ============================================================================
// Record Parsing
// ============================================================================

typedef struct {
    uint32_t type;
    uint32_t length;
    size_t data_offset;
    size_t data_length;
} parsed_record_t;

static bool parse_record(const uint8_t* data, size_t size, size_t offset,
                         parsed_record_t* record) {
    if (offset + 12 > size) return false;
    
    record->type = read_be32(data + offset);
    record->length = read_be32(data + offset + 4);
    // CRC is at offset + 8, we skip it
    
    record->data_offset = offset + 12;
    record->data_length = (record->length > 12) ? record->length - 12 : 0;
    
    return (record->data_offset + record->data_length <= size);
}

// ============================================================================
// CTRaw Detection
// ============================================================================

bool ipf_is_ctraw(const uint8_t* data, size_t size) {
    if (!data || size < 32) return false;
    
    // Must start with CAPS
    if (read_be32(data) != IPF_RECORD_CAPS) return false;
    
    // Scan for CTRaw-specific records
    size_t offset = 0;
    while (offset + 12 <= size) {
        parsed_record_t rec;
        if (!parse_record(data, size, offset, &rec)) break;
        
        // Look for CTEI or DUMP records
        if (rec.type == IPF_RECORD_CTEI || rec.type == IPF_RECORD_DUMP) {
            return true;
        }
        
        offset += rec.length;
        if (rec.length == 0) break;  // Prevent infinite loop
    }
    
    return false;
}

// ============================================================================
// Parsing
// ============================================================================

ipf_ctraw_image_t* ipf_ctraw_open(const uint8_t* data, size_t size) {
    if (!data || size < 32) return NULL;
    
    // Verify CAPS header
    if (read_be32(data) != IPF_RECORD_CAPS) return NULL;
    
    ipf_ctraw_image_t* img = calloc(1, sizeof(ipf_ctraw_image_t));
    if (!img) return NULL;
    
    img->sample_rate = 25000000;  // Default 25 MHz
    
    // Parse all records
    size_t offset = 0;
    bool found_info = false;
    
    // First pass: get geometry
    while (offset + 12 <= size) {
        parsed_record_t rec;
        if (!parse_record(data, size, offset, &rec)) break;
        
        if (rec.type == IPF_RECORD_INFO && rec.data_length >= sizeof(ipf_info_record_t)) {
            // Parse INFO record
            const uint8_t* info_data = data + rec.data_offset;
            
            img->info.type = read_be32(info_data);
            img->info.min_track = read_be32(info_data + 24);
            img->info.max_track = read_be32(info_data + 28);
            img->info.min_side = read_be32(info_data + 32);
            img->info.max_side = read_be32(info_data + 36);
            
            img->min_track = img->info.min_track;
            img->max_track = img->info.max_track;
            img->min_side = img->info.min_side;
            img->max_side = img->info.max_side;
            
            img->num_tracks = img->max_track - img->min_track + 1;
            img->num_sides = img->max_side - img->min_side + 1;
            
            found_info = true;
        }
        else if (rec.type == IPF_RECORD_CTEI && rec.data_length >= 8) {
            // CTRaw Extended Info - contains sample rate
            const uint8_t* ctei_data = data + rec.data_offset;
            img->sample_rate = read_be32(ctei_data);
            img->is_ctraw = true;
        }
        
        offset += rec.length;
        if (rec.length == 0) break;
    }
    
    if (!found_info) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Missing INFO record");
        img->is_valid = false;
        return img;
    }
    
    // Validate geometry
    if (img->num_tracks <= 0 || img->num_tracks > 168 ||
        img->num_sides <= 0 || img->num_sides > 2) {
        snprintf(img->error_msg, sizeof(img->error_msg), 
                 "Invalid geometry: %d tracks, %d sides",
                 img->num_tracks, img->num_sides);
        img->is_valid = false;
        return img;
    }
    
    // Allocate track array
    img->tracks = calloc(img->num_tracks, sizeof(ctraw_track_t*));
    if (!img->tracks) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Memory allocation failed");
        img->is_valid = false;
        return img;
    }
    
    for (int t = 0; t < img->num_tracks; t++) {
        img->tracks[t] = calloc(img->num_sides, sizeof(ctraw_track_t));
        if (!img->tracks[t]) {
            snprintf(img->error_msg, sizeof(img->error_msg), 
                     "Memory allocation failed for track %d", t);
            img->is_valid = false;
            return img;
        }
    }
    
    // Second pass: parse DUMP records
    offset = 0;
    while (offset + 12 <= size) {
        parsed_record_t rec;
        if (!parse_record(data, size, offset, &rec)) break;
        
        if (rec.type == IPF_RECORD_DUMP && rec.data_length >= 16) {
            const uint8_t* dump_data = data + rec.data_offset;
            
            // DUMP format:
            // Bytes 0-3: Track number (BE)
            // Bytes 4-7: Side (BE)
            // Bytes 8-11: Flux count (BE)
            // Bytes 12-15: Index position (BE)
            // Bytes 16+: Flux data (16-bit BE values)
            
            uint32_t track = read_be32(dump_data);
            uint32_t side = read_be32(dump_data + 4);
            uint32_t flux_count = read_be32(dump_data + 8);
            uint32_t index_pos = read_be32(dump_data + 12);
            
            int track_idx = track - img->min_track;
            int side_idx = side - img->min_side;
            
            if (track_idx < 0 || track_idx >= img->num_tracks ||
                side_idx < 0 || side_idx >= img->num_sides) {
                offset += rec.length;
                continue;
            }
            
            // Check if we have enough data
            size_t flux_data_size = rec.data_length - 16;
            size_t expected_size = flux_count * 2;  // 16-bit values
            
            if (flux_data_size < expected_size) {
                flux_count = flux_data_size / 2;
            }
            
            if (flux_count == 0) {
                offset += rec.length;
                continue;
            }
            
            // Allocate and parse flux data
            ctraw_track_t* trk = &img->tracks[track_idx][side_idx];
            trk->flux_data = malloc(flux_count * sizeof(uint32_t));
            
            if (trk->flux_data) {
                const uint8_t* flux_bytes = dump_data + 16;
                
                for (size_t f = 0; f < flux_count; f++) {
                    // 16-bit BE flux values
                    trk->flux_data[f] = read_be16(flux_bytes + f * 2);
                }
                
                trk->flux_count = flux_count;
                trk->sample_rate = img->sample_rate;
                trk->index_pos = index_pos;
                trk->has_index = (index_pos != 0xFFFFFFFF);
            }
            
            img->is_ctraw = true;
        }
        
        offset += rec.length;
        if (rec.length == 0) break;
    }
    
    img->is_valid = true;
    return img;
}

bool ipf_ctraw_get_track(const ipf_ctraw_image_t* img, int track, int side,
                         uint32_t** flux_out, size_t* count_out) {
    if (!img || !img->is_valid || !flux_out || !count_out) return false;
    
    int track_idx = track - img->min_track;
    int side_idx = side - img->min_side;
    
    if (track_idx < 0 || track_idx >= img->num_tracks ||
        side_idx < 0 || side_idx >= img->num_sides) {
        return false;
    }
    
    ctraw_track_t* trk = &img->tracks[track_idx][side_idx];
    if (!trk->flux_data || trk->flux_count == 0) return false;
    
    // Return copy of flux data
    *flux_out = malloc(trk->flux_count * sizeof(uint32_t));
    if (!*flux_out) return false;
    
    memcpy(*flux_out, trk->flux_data, trk->flux_count * sizeof(uint32_t));
    *count_out = trk->flux_count;
    
    return true;
}

bool ipf_ctraw_flux_to_ns(const ipf_ctraw_image_t* img,
                          const uint32_t* flux_ticks, size_t count,
                          uint32_t** flux_ns_out) {
    if (!img || !flux_ticks || !flux_ns_out || count == 0) return false;
    
    *flux_ns_out = malloc(count * sizeof(uint32_t));
    if (!*flux_ns_out) return false;
    
    // Convert ticks to nanoseconds based on sample rate
    // ns = ticks * 1e9 / sample_rate
    double ns_per_tick = 1e9 / img->sample_rate;
    
    for (size_t i = 0; i < count; i++) {
        (*flux_ns_out)[i] = (uint32_t)(flux_ticks[i] * ns_per_tick);
    }
    
    return true;
}

uint32_t ipf_ctraw_get_sample_rate(const ipf_ctraw_image_t* img) {
    return img ? img->sample_rate : 0;
}

void ipf_ctraw_close(ipf_ctraw_image_t* img) {
    if (!img) return;
    
    if (img->tracks) {
        for (int t = 0; t < img->num_tracks; t++) {
            if (img->tracks[t]) {
                for (int s = 0; s < img->num_sides; s++) {
                    free(img->tracks[t][s].flux_data);
                }
                free(img->tracks[t]);
            }
        }
        free(img->tracks);
    }
    
    free(img);
}
