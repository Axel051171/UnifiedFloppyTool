/**
 * @file uft_dmk_mixed.c
 * @brief DMK Mixed Density Implementation
 * 
 * KRITISCHER FIX: FM/MFM Mixed Density Erkennung
 * 
 * DMK IDAM Format:
 * - FM: 0xFE followed by C H R N
 * - MFM: 0xA1 0xA1 0xA1 0xFE followed by C H R N
 * 
 * Der Parser analysiert die IDAMs um FM vs MFM zu unterscheiden.
 */

#include "uft/formats/variants/parsers/uft_dmk_mixed.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Constants
// ============================================================================

#define DMK_HEADER_SIZE     16
#define DMK_IDAM_TABLE_SIZE 128     // 64 IDAMs * 2 bytes each
#define DMK_MAX_TRACK_SIZE  0x4000

// IDAM markers
#define DMK_FM_IDAM_MARK    0xFE
#define DMK_MFM_SYNC        0xA1

// ============================================================================
// Helpers
// ============================================================================

static inline uint16_t read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

// ============================================================================
// Encoding Detection
// ============================================================================

/**
 * @brief Detect encoding from track IDAMs
 * 
 * DMK stores IDAM offsets in first 128 bytes of track.
 * Bit 15 of offset indicates double-density (MFM).
 * We also analyze the actual data patterns.
 */
dmk_encoding_t dmk_detect_track_encoding(const uint8_t* track_data, size_t track_len) {
    if (!track_data || track_len < DMK_IDAM_TABLE_SIZE) {
        return DMK_ENC_UNKNOWN;
    }
    
    int fm_idams = 0;
    int mfm_idams = 0;
    
    // Parse IDAM table (first 128 bytes)
    for (int i = 0; i < 64; i++) {
        uint16_t idam_ptr = read_le16(track_data + i * 2);
        
        if (idam_ptr == 0) break;  // End of IDAMs
        
        // Bit 15 = 1 means double density (MFM)
        bool is_mfm = (idam_ptr & 0x8000) != 0;
        uint16_t offset = idam_ptr & 0x3FFF;  // Lower 14 bits
        
        if (offset < track_len) {
            if (is_mfm) {
                mfm_idams++;
            } else {
                fm_idams++;
            }
        }
    }
    
    // Analyze patterns if IDAM hints are ambiguous
    if (fm_idams == 0 && mfm_idams == 0) {
        // No IDAMs found - check for sync patterns
        const uint8_t* data = track_data + DMK_IDAM_TABLE_SIZE;
        size_t data_len = track_len - DMK_IDAM_TABLE_SIZE;
        
        int mfm_sync_count = 0;
        for (size_t i = 0; i + 3 < data_len; i++) {
            // MFM uses A1 A1 A1 sync pattern
            if (data[i] == 0xA1 && data[i+1] == 0xA1 && data[i+2] == 0xA1) {
                mfm_sync_count++;
                i += 2;  // Skip
            }
        }
        
        if (mfm_sync_count > 0) {
            return DMK_ENC_MFM;
        }
        
        // Check for FM pattern (FE without A1 A1 A1 prefix)
        for (size_t i = 0; i + 1 < data_len; i++) {
            if (data[i] == 0xFE && (i < 3 || data[i-1] != 0xA1)) {
                return DMK_ENC_FM;
            }
        }
        
        return DMK_ENC_UNKNOWN;
    }
    
    // Determine encoding from IDAM flags
    if (mfm_idams > 0 && fm_idams > 0) {
        // Mixed! This shouldn't happen within one track
        // Return based on majority
        return (mfm_idams > fm_idams) ? DMK_ENC_MFM : DMK_ENC_FM;
    }
    
    return (mfm_idams > 0) ? DMK_ENC_MFM : DMK_ENC_FM;
}

// ============================================================================
// Probing
// ============================================================================

bool dmk_probe(const uint8_t* data, size_t size) {
    if (!data || size < DMK_HEADER_SIZE) return false;
    
    // DMK has no magic number - validate by structure
    uint8_t num_tracks = data[1];
    uint16_t track_length = read_le16(data + 2);
    uint8_t flags = data[4];
    
    // Sanity checks
    if (num_tracks == 0 || num_tracks > 96) return false;
    if (track_length < 128 || track_length > DMK_MAX_TRACK_SIZE) return false;
    
    // Calculate expected size
    int num_sides = (flags & 0x10) ? 1 : 2;
    size_t expected = DMK_HEADER_SIZE + (size_t)num_tracks * num_sides * track_length;
    
    // Allow some tolerance
    if (size < expected - track_length || size > expected + track_length) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Parsing
// ============================================================================

dmk_mixed_image_t* dmk_mixed_open(const uint8_t* data, size_t size) {
    if (!dmk_probe(data, size)) return NULL;
    
    dmk_mixed_image_t* img = calloc(1, sizeof(dmk_mixed_image_t));
    if (!img) return NULL;
    
    // Parse header
    img->write_protect = data[0];
    img->num_tracks = data[1];
    img->track_length = read_le16(data + 2);
    img->flags = data[4];
    
    img->num_sides = (img->flags & 0x10) ? 1 : 2;
    
    // Allocate track info
    int total_tracks = img->num_tracks * img->num_sides;
    img->track_info = calloc(total_tracks, sizeof(dmk_track_info_t));
    if (!img->track_info) {
        snprintf(img->error_msg, sizeof(img->error_msg), "Memory allocation failed");
        img->is_valid = false;
        return img;
    }
    
    // Analyze each track
    size_t offset = DMK_HEADER_SIZE;
    
    for (int t = 0; t < img->num_tracks; t++) {
        for (int s = 0; s < img->num_sides; s++) {
            int idx = t * img->num_sides + s;
            dmk_track_info_t* ti = &img->track_info[idx];
            
            ti->track_num = t;
            ti->side = s;
            
            if (offset + img->track_length > size) {
                ti->encoding = DMK_ENC_UNKNOWN;
                continue;
            }
            
            const uint8_t* track_data = data + offset;
            ti->data_length = img->track_length;
            
            // Detect encoding
            ti->encoding = dmk_detect_track_encoding(track_data, img->track_length);
            
            // Count IDAMs
            for (int i = 0; i < 64; i++) {
                uint16_t idam_ptr = read_le16(track_data + i * 2);
                if (idam_ptr == 0) break;
                ti->idam_count++;
            }
            
            ti->sector_count = ti->idam_count;
            ti->is_empty = (ti->idam_count == 0);
            
            // Update statistics
            if (ti->encoding == DMK_ENC_FM) {
                img->fm_tracks++;
            } else if (ti->encoding == DMK_ENC_MFM) {
                img->mfm_tracks++;
            }
            
            offset += img->track_length;
        }
    }
    
    // Determine overall encoding
    if (img->fm_tracks > 0 && img->mfm_tracks > 0) {
        img->overall_encoding = DMK_ENC_MIXED;
        img->is_mixed_density = true;
    } else if (img->mfm_tracks > 0) {
        img->overall_encoding = DMK_ENC_MFM;
    } else if (img->fm_tracks > 0) {
        img->overall_encoding = DMK_ENC_FM;
    } else {
        img->overall_encoding = DMK_ENC_UNKNOWN;
    }
    
    img->is_valid = true;
    return img;
}

bool dmk_is_mixed_density(const dmk_mixed_image_t* img) {
    return img && img->is_mixed_density;
}

dmk_encoding_t dmk_get_track_encoding(const dmk_mixed_image_t* img, int track, int side) {
    if (!img || !img->track_info) return DMK_ENC_UNKNOWN;
    if (track < 0 || track >= img->num_tracks) return DMK_ENC_UNKNOWN;
    if (side < 0 || side >= img->num_sides) return DMK_ENC_UNKNOWN;
    
    int idx = track * img->num_sides + side;
    return img->track_info[idx].encoding;
}

void dmk_mixed_close(dmk_mixed_image_t* img) {
    if (!img) return;
    free(img->track_info);
    free(img);
}
