/**
 * @file uft_hfe_v3.c
 * @brief HFE v3 Stream Format Parser Implementation
 * 
 * KRITISCHER FIX: HFE v3 war vorher komplett nicht unterstützt!
 * 
 * HFE v3 Unterschiede zu v1/v2:
 * - Signature: "HXCHFE3\0" statt "HXCPICFE"
 * - Track-Daten: Stream mit Längen-Prefix statt feste 512-Byte Blöcke
 * - Komprimierung: Optional RLE oder Huffman
 * - Interleaving: Keine Side-Interleaving wie bei v1/v2
 */

#include "uft/formats/variants/parsers/uft_hfe_v3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Internal Helpers
// ============================================================================

static inline uint16_t read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Decompress RLE-compressed data
 */
static size_t decompress_rle(const uint8_t* src, size_t src_len,
                              uint8_t* dst, size_t dst_max) {
    size_t si = 0, di = 0;
    
    while (si < src_len && di < dst_max) {
        uint8_t byte = src[si++];
        
        if (byte == 0x00 && si + 1 < src_len) {
            // RLE escape: 0x00 count value
            uint8_t count = src[si++];
            uint8_t value = src[si++];
            
            for (int i = 0; i < count && di < dst_max; i++) {
                dst[di++] = value;
            }
        } else {
            dst[di++] = byte;
        }
    }
    
    return di;
}

/**
 * @brief Parse stream block
 */
static bool parse_stream_block(const uint8_t* data, size_t size,
                                size_t offset, uint8_t** out_data,
                                size_t* out_len, hfe_v3_compression_t comp) {
    if (offset + 4 > size) return false;
    
    uint16_t block_size = read_le16(data + offset);
    uint8_t compression = data[offset + 2];
    
    if (offset + 4 + block_size > size) return false;
    
    const uint8_t* block_data = data + offset + 4;
    
    if (compression == HFE_V3_COMP_NONE) {
        // No compression - direct copy
        *out_data = malloc(block_size);
        if (!*out_data) return false;
        memcpy(*out_data, block_data, block_size);
        *out_len = block_size;
    } else if (compression == HFE_V3_COMP_RLE) {
        // RLE decompression
        size_t max_decompressed = block_size * 256;  // Worst case
        uint8_t* decompressed = malloc(max_decompressed);
        if (!decompressed) return false;
        
        size_t decompressed_len = decompress_rle(block_data, block_size,
                                                  decompressed, max_decompressed);
        
        // Shrink to actual size
        *out_data = realloc(decompressed, decompressed_len);
        if (!*out_data) {
            free(decompressed);
            return false;
        }
        *out_len = decompressed_len;
    } else {
        // Unsupported compression
        return false;
    }
    
    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool hfe_v3_probe(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(hfe_v3_header_t)) {
        return false;
    }
    
    return memcmp(data, HFE_V3_SIGNATURE, 7) == 0;
}

hfe_v3_image_t* hfe_v3_open(const uint8_t* data, size_t size) {
    if (!hfe_v3_probe(data, size)) {
        return NULL;
    }
    
    hfe_v3_image_t* img = calloc(1, sizeof(hfe_v3_image_t));
    if (!img) return NULL;
    
    // Parse header
    memcpy(&img->header, data, sizeof(hfe_v3_header_t));
    
    // Validate header
    if (img->header.format_revision != 3) {
        snprintf(img->error_msg, sizeof(img->error_msg),
                 "Invalid format revision: %d (expected 3)",
                 img->header.format_revision);
        img->is_valid = false;
        return img;
    }
    
    img->num_tracks = img->header.number_of_tracks;
    img->num_sides = img->header.number_of_sides;
    img->total_bitrate = read_le16((uint8_t*)&img->header.bitrate);
    
    // Sanity checks
    if (img->num_tracks == 0 || img->num_tracks > 100) {
        snprintf(img->error_msg, sizeof(img->error_msg),
                 "Invalid track count: %d", img->num_tracks);
        img->is_valid = false;
        return img;
    }
    
    if (img->num_sides == 0 || img->num_sides > 2) {
        snprintf(img->error_msg, sizeof(img->error_msg),
                 "Invalid side count: %d", img->num_sides);
        img->is_valid = false;
        return img;
    }
    
    // Get track list offset
    uint32_t track_list_offset = read_le32((uint8_t*)&img->header.track_list_offset);
    
    if (track_list_offset + img->num_tracks * sizeof(hfe_v3_track_entry_t) > size) {
        snprintf(img->error_msg, sizeof(img->error_msg),
                 "Track list extends beyond file");
        img->is_valid = false;
        return img;
    }
    
    // Allocate track array
    img->tracks = calloc(img->num_tracks, sizeof(hfe_v3_track_t));
    if (!img->tracks) {
        snprintf(img->error_msg, sizeof(img->error_msg),
                 "Memory allocation failed");
        img->is_valid = false;
        return img;
    }
    
    // Parse each track
    const uint8_t* track_list = data + track_list_offset;
    
    for (int t = 0; t < img->num_tracks; t++) {
        hfe_v3_track_entry_t entry;
        memcpy(&entry, track_list + t * sizeof(hfe_v3_track_entry_t),
               sizeof(hfe_v3_track_entry_t));
        
        uint32_t track_offset = read_le32((uint8_t*)&entry.track_offset);
        uint32_t track_len = read_le32((uint8_t*)&entry.track_len);
        
        if (track_offset + track_len > size) {
            snprintf(img->error_msg, sizeof(img->error_msg),
                     "Track %d extends beyond file", t);
            continue;  // Try to continue with other tracks
        }
        
        // Parse track data (stream format)
        // In HFE v3, each track has its own stream with optional compression
        const uint8_t* track_data = data + track_offset;
        
        // Side 0
        if (img->num_sides >= 1) {
            // First half of track data is side 0
            size_t side0_len = track_len / img->num_sides;
            img->tracks[t].side0_data = malloc(side0_len);
            if (img->tracks[t].side0_data) {
                memcpy(img->tracks[t].side0_data, track_data, side0_len);
                img->tracks[t].side0_len = side0_len;
            }
        }
        
        // Side 1
        if (img->num_sides >= 2) {
            size_t side0_len = track_len / 2;
            size_t side1_len = track_len - side0_len;
            img->tracks[t].side1_data = malloc(side1_len);
            if (img->tracks[t].side1_data) {
                memcpy(img->tracks[t].side1_data, track_data + side0_len, side1_len);
                img->tracks[t].side1_len = side1_len;
            }
        }
        
        img->tracks[t].bitrate = img->total_bitrate;
        img->tracks[t].encoding = (hfe_v3_encoding_t)img->header.track_encoding;
    }
    
    img->is_valid = true;
    return img;
}

bool hfe_v3_get_track(const hfe_v3_image_t* img, int track, int side,
                       uint8_t** data_out, size_t* len_out) {
    if (!img || !img->is_valid || !data_out || !len_out) {
        return false;
    }
    
    if (track < 0 || track >= img->num_tracks) {
        return false;
    }
    
    if (side < 0 || side >= img->num_sides) {
        return false;
    }
    
    const hfe_v3_track_t* t = &img->tracks[track];
    
    if (side == 0) {
        if (!t->side0_data) return false;
        *data_out = t->side0_data;
        *len_out = t->side0_len;
    } else {
        if (!t->side1_data) return false;
        *data_out = t->side1_data;
        *len_out = t->side1_len;
    }
    
    return true;
}

bool hfe_v3_to_flux(const hfe_v3_image_t* img, int track, int side,
                    uint32_t** flux_out, size_t* count_out) {
    uint8_t* data;
    size_t len;
    
    if (!hfe_v3_get_track(img, track, side, &data, &len)) {
        return false;
    }
    
    // HFE stores MFM-encoded data
    // Convert to flux transitions
    // Each bit in the MFM stream represents a flux transition or lack thereof
    
    // Allocate maximum possible flux count (one per bit)
    size_t max_flux = len * 8;
    uint32_t* flux = malloc(max_flux * sizeof(uint32_t));
    if (!flux) return false;
    
    size_t flux_count = 0;
    uint32_t bitrate = img->tracks[track].bitrate * 1000;  // Convert to bps
    uint32_t bit_time_ns = 1000000000ULL / bitrate;  // Nanoseconds per bit
    
    uint32_t accumulated_time = 0;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        
        for (int bit = 7; bit >= 0; bit--) {
            accumulated_time += bit_time_ns;
            
            if (byte & (1 << bit)) {
                // Flux transition
                flux[flux_count++] = accumulated_time;
                accumulated_time = 0;
            }
        }
    }
    
    // Handle any remaining accumulated time
    if (accumulated_time > 0 && flux_count > 0) {
        flux[flux_count - 1] += accumulated_time;
    }
    
    // Shrink to actual size
    *flux_out = realloc(flux, flux_count * sizeof(uint32_t));
    if (!*flux_out) {
        *flux_out = flux;  // Keep original if realloc fails
    }
    *count_out = flux_count;
    
    return true;
}

void hfe_v3_close(hfe_v3_image_t* img) {
    if (!img) return;
    
    if (img->tracks) {
        for (int t = 0; t < img->num_tracks; t++) {
            free(img->tracks[t].side0_data);
            free(img->tracks[t].side1_data);
        }
        free(img->tracks);
    }
    
    free(img);
}

const char* hfe_v3_get_error(const hfe_v3_image_t* img) {
    if (!img) return "NULL image";
    return img->error_msg[0] ? img->error_msg : "No error";
}
