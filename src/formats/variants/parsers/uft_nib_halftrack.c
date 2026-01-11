/**
 * @file uft_nib_halftrack.c
 * @brief NIB Half-Track Support Implementation
 * 
 * KRITISCHER FIX: Half-tracks für Copy-Protection jetzt unterstützt!
 * 
 * Apple II Kopierschutz nutzt oft half-tracks:
 * - Spirale: Daten auf half-tracks zwischen normalen Tracks
 * - Timing: Präzise Bit-Timings zwischen Spuren
 * - Fat Tracks: Überlappende Daten auf benachbarten Spuren
 */

#include "uft/formats/variants/parsers/uft_nib_halftrack.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Mode Detection
// ============================================================================

nib_mode_t nib_detect_mode(size_t file_size) {
    switch (file_size) {
        case NIB_35_FULL_SIZE:  return NIB_MODE_35_FULL;
        case NIB_40_FULL_SIZE:  return NIB_MODE_40_FULL;
        case NIB_35_HALF_SIZE:  return NIB_MODE_35_HALF;
        case NIB_40_HALF_SIZE:  return NIB_MODE_40_HALF;
        default:
            // Check if it's a valid multiple of track size
            if (file_size % NIB_TRACK_SIZE == 0) {
                int tracks = file_size / NIB_TRACK_SIZE;
                if (tracks <= 40) return NIB_MODE_35_FULL;
                if (tracks <= 80) return NIB_MODE_35_HALF;
            }
            return NIB_MODE_UNKNOWN;
    }
}

bool nib_probe(const uint8_t* data, size_t size) {
    return nib_detect_mode(size) != NIB_MODE_UNKNOWN;
}

// ============================================================================
// Track Analysis
// ============================================================================

/**
 * @brief Count sync bytes in track
 */
static int count_sync_bytes(const uint8_t* data, size_t size) {
    int count = 0;
    int consecutive = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (data[i] == NIB_SYNC_BYTE) {
            consecutive++;
        } else {
            if (consecutive >= 5) {  // Valid sync sequence
                count++;
            }
            consecutive = 0;
        }
    }
    
    return count;
}

/**
 * @brief Find Apple II sector headers in track
 */
static int find_sectors(const uint8_t* data, size_t size) {
    int sectors = 0;
    
    // Look for address field prologue: D5 AA 96
    for (size_t i = 0; i + 2 < size; i++) {
        if (data[i] == 0xD5 && data[i+1] == 0xAA && data[i+2] == 0x96) {
            sectors++;
        }
    }
    
    return sectors;
}

/**
 * @brief Check if track appears empty
 */
static bool is_track_empty(const uint8_t* data, size_t size) {
    // Count non-sync bytes
    int non_sync = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] != NIB_SYNC_BYTE && data[i] != 0x00) {
            non_sync++;
        }
    }
    
    // If less than 5% non-sync, consider empty
    return (non_sync < (int)(size * 0.05));
}

void nib_analyze_track(nib_track_t* track) {
    if (!track || !track->data || track->size == 0) return;
    
    track->sync_count = count_sync_bytes(track->data, track->size);
    track->sector_count = find_sectors(track->data, track->size);
    track->has_valid_sectors = (track->sector_count > 0);
    track->appears_empty = is_track_empty(track->data, track->size);
}

// ============================================================================
// Image Parsing
// ============================================================================

nib_image_t* nib_open(const uint8_t* data, size_t size) {
    nib_mode_t mode = nib_detect_mode(size);
    if (mode == NIB_MODE_UNKNOWN) return NULL;
    
    nib_image_t* img = calloc(1, sizeof(nib_image_t));
    if (!img) return NULL;
    
    img->file_size = size;
    img->mode = mode;
    
    // Determine geometry
    switch (mode) {
        case NIB_MODE_35_FULL:
            img->num_tracks = 35;
            img->physical_tracks = 35;
            img->has_half_tracks = false;
            break;
        case NIB_MODE_40_FULL:
            img->num_tracks = 40;
            img->physical_tracks = 40;
            img->has_half_tracks = false;
            break;
        case NIB_MODE_35_HALF:
            img->num_tracks = 70;
            img->physical_tracks = 35;
            img->has_half_tracks = true;
            break;
        case NIB_MODE_40_HALF:
            img->num_tracks = 80;
            img->physical_tracks = 40;
            img->has_half_tracks = true;
            break;
        default:
            img->num_tracks = size / NIB_TRACK_SIZE;
            img->physical_tracks = img->num_tracks;
            img->has_half_tracks = false;
    }
    
    // Allocate tracks
    img->tracks = calloc(img->num_tracks, sizeof(nib_track_t));
    if (!img->tracks) {
        snprintf(img->error_msg, sizeof(img->error_msg), 
                 "Memory allocation failed");
        img->is_valid = false;
        return img;
    }
    
    // Initialize quarter-track map to invalid
    for (int i = 0; i < NIB_MAX_QUARTER_TRACKS; i++) {
        img->quarter_track_map[i] = -1;
    }
    
    // Parse tracks
    for (int t = 0; t < img->num_tracks; t++) {
        size_t offset = t * NIB_TRACK_SIZE;
        if (offset + NIB_TRACK_SIZE > size) break;
        
        nib_track_t* track = &img->tracks[t];
        
        // Allocate and copy track data
        track->data = malloc(NIB_TRACK_SIZE);
        if (!track->data) continue;
        
        memcpy(track->data, data + offset, NIB_TRACK_SIZE);
        track->size = NIB_TRACK_SIZE;
        track->bit_count = NIB_TRACK_SIZE * 8;
        
        // Calculate physical position
        if (img->has_half_tracks) {
            // Half-track mode: track 0 = physical 0.0
            //                  track 1 = physical 0.5
            //                  track 2 = physical 1.0
            //                  etc.
            track->physical_track = t / 2;
            track->half_track_offset = t % 2;
            track->is_half_track = (t % 2) != 0;
        } else {
            // Full track mode
            track->physical_track = t;
            track->half_track_offset = 0;
            track->is_half_track = false;
        }
        
        // Analyze track content
        nib_analyze_track(track);
        
        img->total_sectors += track->sector_count;
        if (track->has_valid_sectors) {
            img->valid_sectors += track->sector_count;
        }
        
        // Update quarter-track map
        // Apple II uses quarter-tracks (0, 0.25, 0.5, 0.75, 1.0, ...)
        // We map full tracks to quarter positions 0, 4, 8, ...
        // Half tracks to quarter positions 2, 6, 10, ...
        int quarter_pos;
        if (img->has_half_tracks) {
            quarter_pos = t * 2;  // Half-tracks map to every other quarter
        } else {
            quarter_pos = t * 4;  // Full tracks map to every 4th quarter
        }
        
        if (quarter_pos < NIB_MAX_QUARTER_TRACKS) {
            img->quarter_track_map[quarter_pos] = t;
        }
    }
    
    // Detect copy protection
    if (img->has_half_tracks) {
        // Check if half-tracks have unique content
        int unique_halfs = 0;
        for (int t = 0; t < img->num_tracks; t++) {
            if (img->tracks[t].is_half_track && 
                !img->tracks[t].appears_empty &&
                img->tracks[t].sector_count > 0) {
                unique_halfs++;
            }
        }
        img->has_copy_protection = (unique_halfs > 0);
    }
    
    img->is_valid = true;
    return img;
}

bool nib_get_track(const nib_image_t* img, int track, int half,
                   nib_track_t* track_out) {
    if (!img || !img->is_valid || !track_out) return false;
    
    int idx;
    if (img->has_half_tracks) {
        idx = track * 2 + (half ? 1 : 0);
    } else {
        if (half) return false;  // No half tracks available
        idx = track;
    }
    
    if (idx < 0 || idx >= img->num_tracks) return false;
    
    *track_out = img->tracks[idx];
    return true;
}

bool nib_get_quarter_track(const nib_image_t* img, int quarter_track,
                           nib_track_t* track_out) {
    if (!img || !img->is_valid || !track_out) return false;
    if (quarter_track < 0 || quarter_track >= NIB_MAX_QUARTER_TRACKS) return false;
    
    int idx = img->quarter_track_map[quarter_track];
    if (idx < 0 || idx >= img->num_tracks) return false;
    
    *track_out = img->tracks[idx];
    return true;
}

bool nib_to_flux(const nib_track_t* track, double bit_cell_us,
                 uint32_t** flux_out, size_t* count_out) {
    if (!track || !track->data || !flux_out || !count_out) return false;
    
    // GCR: each 1-bit is a flux transition
    // Allocate worst case
    uint32_t* flux = malloc(track->bit_count * sizeof(uint32_t));
    if (!flux) return false;
    
    size_t flux_count = 0;
    double accumulated_us = 0;
    
    for (size_t byte_idx = 0; byte_idx < track->size; byte_idx++) {
        uint8_t byte = track->data[byte_idx];
        
        for (int bit = 7; bit >= 0; bit--) {
            accumulated_us += bit_cell_us;
            
            if (byte & (1 << bit)) {
                // Flux transition - convert to nanoseconds
                flux[flux_count++] = (uint32_t)(accumulated_us * 1000);
                accumulated_us = 0;
            }
        }
    }
    
    // Shrink to actual size
    *flux_out = realloc(flux, flux_count * sizeof(uint32_t));
    if (!*flux_out) *flux_out = flux;
    *count_out = flux_count;
    
    return true;
}

bool nib_has_protection(const nib_image_t* img) {
    return img && img->has_copy_protection;
}

const char* nib_get_protection_info(const nib_image_t* img) {
    if (!img || !img->has_copy_protection) {
        return "None detected";
    }
    
    static char buffer[256];
    
    // Count non-empty half-tracks
    int protected_tracks = 0;
    for (int t = 0; t < img->num_tracks; t++) {
        if (img->tracks[t].is_half_track && 
            !img->tracks[t].appears_empty) {
            protected_tracks++;
        }
    }
    
    snprintf(buffer, sizeof(buffer),
             "Half-track protection: %d half-tracks with data",
             protected_tracks);
    
    return buffer;
}

void nib_close(nib_image_t* img) {
    if (!img) return;
    
    if (img->tracks) {
        for (int t = 0; t < img->num_tracks; t++) {
            free(img->tracks[t].data);
        }
        free(img->tracks);
    }
    
    free(img);
}
