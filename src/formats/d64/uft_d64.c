/**
 * @file uft_d64.c
 * @brief D64 Extended Variant Support Implementation
 * 
 * ROADMAP F1.1 - Priority P0
 */

#include "uft/formats/uft_d64.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Sector Table
// ============================================================================

const int D64_SECTORS_PER_TRACK[43] = {
    0,  // Track 0 doesn't exist
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  // Tracks 1-10
    21, 21, 21, 21, 21, 21, 21,              // Tracks 11-17
    19, 19, 19, 19, 19, 19, 19,              // Tracks 18-24
    18, 18, 18, 18, 18, 18,                  // Tracks 25-30
    17, 17, 17, 17, 17,                      // Tracks 31-35
    17, 17, 17, 17, 17,                      // Tracks 36-40
    17, 17                                   // Tracks 41-42
};

// Cumulative sector count for offset calculation
static const int D64_TRACK_OFFSET[43] = {
    0,    // Track 0
    0,    21,   42,   63,   84,   105,  126,  147,  168,  189,   // 1-10
    210,  231,  252,  273,  294,  315,  336,                     // 11-17
    357,  376,  395,  414,  433,  452,  471,                     // 18-24
    490,  508,  526,  544,  562,  580,                           // 25-30
    598,  615,  632,  649,  666,                                 // 31-35
    683,  700,  717,  734,  751,                                 // 36-40
    768,  785                                                    // 41-42
};

// ============================================================================
// Internal Helpers
// ============================================================================

static int count_sectors(int max_track) {
    int count = 0;
    for (int t = 1; t <= max_track; t++) {
        count += D64_SECTORS_PER_TRACK[t];
    }
    return count;
}

static bool has_geos_signature(const uint8_t* bam) {
    // GEOS uses specific bytes in BAM
    // Check for "GEOS" or GEOS-specific BAM pattern
    return (bam[0xAD] == 'G' && bam[0xAE] == 'E' && 
            bam[0xAF] == 'O' && bam[0xB0] == 'S');
}

static bool has_speeddos_bam(const uint8_t* data, size_t size) {
    // SpeedDOS uses additional BAM at track 18, sector 0 offset 0xC0
    if (size < D64_SIZE_40) return false;
    
    // Check for SpeedDOS signature in extended BAM area
    int offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    if (offset + 0xC0 + 5 > (int)size) return false;
    
    // SpeedDOS has BAM extension for tracks 36-40
    const uint8_t* bam = data + offset;
    
    // Check if track 36-40 BAM entries are present and valid
    for (int t = 0; t < 5; t++) {
        int ext_offset = 0xC0 + t * 4;
        if (bam[ext_offset] > 21) return false;  // Invalid free count
    }
    
    return true;
}

static bool has_dolphindos_bam(const uint8_t* data, size_t size) {
    // DolphinDOS uses track 18, sector 0 differently
    if (size < D64_SIZE_40) return false;
    
    int offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    const uint8_t* bam = data + offset;
    
    // DolphinDOS signature check
    return (bam[0x03] == 0x41 && bam[0xA5] != 0x00);
}

// ============================================================================
// Detection
// ============================================================================

int d64_detect_variant(const uint8_t* data, size_t size,
                       d64_detect_result_t* result) {
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Size-based detection
    switch (size) {
        case D64_SIZE_35:
            result->variant = D64_VAR_35_TRACK;
            result->tracks = 35;
            result->confidence = 90;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Standard 35-track D64 (174848 bytes)");
            break;
            
        case D64_SIZE_35_ERR:
            result->variant = D64_VAR_35_TRACK | D64_VAR_ERROR_INFO;
            result->tracks = 35;
            result->has_errors = true;
            result->confidence = 95;
            snprintf(result->explanation, sizeof(result->explanation),
                     "35-track D64 with error info (175531 bytes)");
            break;
            
        case D64_SIZE_40:
            result->variant = D64_VAR_40_TRACK;
            result->tracks = 40;
            result->confidence = 90;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Extended 40-track D64 (196608 bytes)");
            break;
            
        case D64_SIZE_40_ERR:
            result->variant = D64_VAR_40_TRACK | D64_VAR_ERROR_INFO;
            result->tracks = 40;
            result->has_errors = true;
            result->confidence = 95;
            snprintf(result->explanation, sizeof(result->explanation),
                     "40-track D64 with error info (197376 bytes)");
            break;
            
        case D64_SIZE_42:
            result->variant = D64_VAR_42_TRACK;
            result->tracks = 42;
            result->confidence = 85;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Extended 42-track D64 (205312 bytes)");
            break;
            
        case D64_SIZE_42_ERR:
            result->variant = D64_VAR_42_TRACK | D64_VAR_ERROR_INFO;
            result->tracks = 42;
            result->has_errors = true;
            result->confidence = 90;
            snprintf(result->explanation, sizeof(result->explanation),
                     "42-track D64 with error info (206114 bytes)");
            break;
            
        default:
            // Unknown size - try to detect
            if (size >= D64_SIZE_35 && size <= D64_SIZE_42_ERR + 1000) {
                result->variant = D64_VAR_35_TRACK;
                result->tracks = 35;
                result->confidence = 50;
                snprintf(result->explanation, sizeof(result->explanation),
                         "Non-standard D64 size (%zu bytes)", size);
            } else {
                return -1;  // Not a D64
            }
    }
    
    // Content-based enhancements
    if (size >= D64_SIZE_35) {
        int bam_offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
        const uint8_t* bam = data + bam_offset;
        
        // Check for GEOS
        if (has_geos_signature(bam)) {
            result->variant |= D64_VAR_GEOS;
            result->is_geos = true;
            result->confidence += 5;
            strncat(result->explanation, " [GEOS]", 
                    sizeof(result->explanation) - strlen(result->explanation) - 1);
        }
        
        // Check for SpeedDOS
        if (result->tracks >= 40 && has_speeddos_bam(data, size)) {
            result->variant |= D64_VAR_SPEEDDOS;
            result->is_speeddos = true;
            strncat(result->explanation, " [SpeedDOS]",
                    sizeof(result->explanation) - strlen(result->explanation) - 1);
        }
        
        // Check for DolphinDOS
        if (result->tracks >= 40 && has_dolphindos_bam(data, size)) {
            result->variant |= D64_VAR_DOLPHINDOS;
            strncat(result->explanation, " [DolphinDOS]",
                    sizeof(result->explanation) - strlen(result->explanation) - 1);
        }
        
        // Validate BAM structure
        if (bam[0x00] == 18 && bam[0x01] == 1 && bam[0x02] == 0x41) {
            result->confidence += 5;  // Valid BAM header
        }
    }
    
    // Cap confidence
    if (result->confidence > 100) result->confidence = 100;
    
    return 0;
}

// ============================================================================
// Open/Create/Close
// ============================================================================

d64_image_t* d64_open_memory(const uint8_t* data, size_t size) {
    d64_detect_result_t detect;
    if (d64_detect_variant(data, size, &detect) != 0) {
        return NULL;
    }
    
    d64_image_t* img = calloc(1, sizeof(d64_image_t));
    if (!img) return NULL;
    
    // Allocate and copy data
    img->data = malloc(size);
    if (!img->data) {
        free(img);
        return NULL;
    }
    memcpy(img->data, data, size);
    img->data_size = size;
    
    // Set variant info
    img->variant = detect.variant;
    img->confidence = detect.confidence;
    img->num_tracks = detect.tracks;
    img->total_sectors = count_sectors(detect.tracks);
    
    // Handle error info
    if (detect.has_errors) {
        img->has_errors = true;
        size_t data_size = detect.tracks == 35 ? D64_SIZE_35 :
                          (detect.tracks == 40 ? D64_SIZE_40 : D64_SIZE_42);
        img->error_count = img->total_sectors;
        img->error_info = malloc(img->error_count);
        if (img->error_info && size > data_size) {
            memcpy(img->error_info, data + data_size, 
                   size - data_size < img->error_count ? size - data_size : img->error_count);
        }
    }
    
    // Read BAM
    d64_read_bam(img);
    
    // Detect special formats
    img->is_geos = detect.is_geos;
    img->is_speeddos = detect.is_speeddos;
    
    img->is_valid = true;
    return img;
}

d64_image_t* d64_open(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 10 * 1024 * 1024) {
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
    
    d64_image_t* img = d64_open_memory(data, size);
    free(data);
    
    return img;
}

d64_image_t* d64_open_variant(const char* path, d64_variant_t variant) {
    d64_image_t* img = d64_open(path);
    if (img) {
        img->variant = variant;  // Override detected variant
    }
    return img;
}

d64_image_t* d64_create(int tracks, bool with_errors) {
    if (tracks != 35 && tracks != 40 && tracks != 42) {
        return NULL;
    }
    
    d64_image_t* img = calloc(1, sizeof(d64_image_t));
    if (!img) return NULL;
    
    // Calculate size
    img->num_tracks = tracks;
    img->total_sectors = count_sectors(tracks);
    img->data_size = img->total_sectors * D64_SECTOR_SIZE;
    
    img->data = calloc(1, img->data_size);
    if (!img->data) {
        free(img);
        return NULL;
    }
    
    // Set variant
    img->variant = (tracks == 35) ? D64_VAR_35_TRACK :
                   (tracks == 40) ? D64_VAR_40_TRACK : D64_VAR_42_TRACK;
    
    // Error info
    if (with_errors) {
        img->has_errors = true;
        img->error_count = img->total_sectors;
        img->error_info = calloc(1, img->error_count);
        if (!img->error_info) {
            free(img->data);
            free(img);
            return NULL;
        }
        // Initialize all to OK
        memset(img->error_info, D64_ERR_OK, img->error_count);
        img->variant |= D64_VAR_ERROR_INFO;
    }
    
    // Initialize BAM
    uint8_t* bam = img->data + D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    bam[0] = 18;  // Directory track
    bam[1] = 1;   // Directory sector
    bam[2] = 0x41; // DOS type 'A'
    bam[3] = 0x00;
    
    // Mark all sectors as free
    for (int t = 1; t <= tracks; t++) {
        int sectors = D64_SECTORS_PER_TRACK[t];
        int offset = 4 + (t - 1) * 4;
        
        if (offset + 4 > 256) break;
        
        bam[offset] = sectors;
        bam[offset + 1] = 0xFF;
        bam[offset + 2] = 0xFF;
        bam[offset + 3] = (sectors > 16) ? (1 << (sectors - 16)) - 1 : 0xFF;
    }
    
    // Set disk name
    memset(bam + 0x90, 0xA0, 16);
    memcpy(bam + 0x90, "EMPTY DISK", 10);
    bam[0xA2] = 0xA0;
    bam[0xA3] = 0xA0;
    bam[0xA4] = '0';
    bam[0xA5] = '0';
    
    // Initialize directory
    uint8_t* dir = img->data + (D64_TRACK_OFFSET[18] + 1) * D64_SECTOR_SIZE;
    dir[0] = 0;  // No next track
    dir[1] = 0xFF;
    
    // Allocate BAM and first directory sector
    d64_allocate_sector(img, 18, 0);
    d64_allocate_sector(img, 18, 1);
    
    img->is_valid = true;
    img->confidence = 100;
    
    d64_read_bam(img);
    
    return img;
}

int d64_save(const d64_image_t* img, const char* path) {
    if (!img || !img->data || !path) return -1;
    
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    
    // Write data
    if (fwrite(img->data, 1, img->data_size, f) != img->data_size) {
        fclose(f);
        return -1;
    }
    
    // Write error info if present
    if (img->has_errors && img->error_info) {
        if (fwrite(img->error_info, 1, img->error_count, f) != img->error_count) {
            fclose(f);
            return -1;
        }
    }
    
    fclose(f);
    return 0;
}

void d64_close(d64_image_t* img) {
    if (!img) return;
    free(img->data);
    free(img->error_info);
    free(img);
}

// ============================================================================
// Sector API
// ============================================================================

int d64_get_sector_offset(int track, int sector) {
    if (track < 1 || track > 42) return -1;
    if (sector < 0 || sector >= D64_SECTORS_PER_TRACK[track]) return -1;
    
    return (D64_TRACK_OFFSET[track] + sector) * D64_SECTOR_SIZE;
}

int d64_read_sector(const d64_image_t* img, int track, int sector,
                    uint8_t* buffer) {
    if (!img || !img->data || !buffer) return -1;
    
    int offset = d64_get_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + D64_SECTOR_SIZE) > img->data_size) {
        return -1;
    }
    
    memcpy(buffer, img->data + offset, D64_SECTOR_SIZE);
    return 0;
}

int d64_write_sector(d64_image_t* img, int track, int sector,
                     const uint8_t* buffer) {
    if (!img || !img->data || !buffer) return -1;
    
    int offset = d64_get_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + D64_SECTOR_SIZE) > img->data_size) {
        return -1;
    }
    
    memcpy(img->data + offset, buffer, D64_SECTOR_SIZE);
    img->is_modified = true;
    return 0;
}

int d64_get_error(const d64_image_t* img, int track, int sector) {
    if (!img || !img->has_errors || !img->error_info) return 0;
    
    int lba = d64_ts_to_lba(track, sector);
    if (lba < 0 || (size_t)lba >= img->error_count) return 0;
    
    return img->error_info[lba];
}

int d64_set_error(d64_image_t* img, int track, int sector, int error) {
    if (!img || !img->has_errors || !img->error_info) return -1;
    
    int lba = d64_ts_to_lba(track, sector);
    if (lba < 0 || (size_t)lba >= img->error_count) return -1;
    
    img->error_info[lba] = error;
    img->is_modified = true;
    return 0;
}

// ============================================================================
// BAM API
// ============================================================================

int d64_read_bam(d64_image_t* img) {
    if (!img || !img->data) return -1;
    
    int offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    if ((size_t)(offset + 256) > img->data_size) return -1;
    
    const uint8_t* bam = img->data + offset;
    
    // Read disk name (offset 0x90, 16 bytes)
    for (int i = 0; i < 16; i++) {
        uint8_t c = bam[0x90 + i];
        img->disk_name[i] = (c == 0xA0) ? ' ' : c;
    }
    img->disk_name[16] = '\0';
    
    // Trim trailing spaces
    for (int i = 15; i >= 0 && img->disk_name[i] == ' '; i--) {
        img->disk_name[i] = '\0';
    }
    
    // Read disk ID
    img->disk_id[0] = bam[0xA2];
    img->disk_id[1] = bam[0xA3];
    img->disk_id[2] = '\0';
    
    // Count free blocks
    img->free_blocks = 0;
    for (int t = 1; t <= img->num_tracks; t++) {
        if (t == 18) continue;  // Skip directory track
        
        int bam_offset = 4 + (t - 1) * 4;
        if (bam_offset < 256) {
            img->free_blocks += bam[bam_offset];
        }
    }
    
    return 0;
}

bool d64_is_sector_free(const d64_image_t* img, int track, int sector) {
    if (!img || !img->data) return false;
    if (track < 1 || track > img->num_tracks) return false;
    if (sector < 0 || sector >= D64_SECTORS_PER_TRACK[track]) return false;
    
    int offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    const uint8_t* bam = img->data + offset;
    
    int bam_offset = 4 + (track - 1) * 4;
    int byte_offset = 1 + (sector / 8);
    int bit = sector % 8;
    
    return (bam[bam_offset + byte_offset] & (1 << bit)) != 0;
}

int d64_allocate_sector(d64_image_t* img, int track, int sector) {
    if (!img || !img->data) return -1;
    if (!d64_is_sector_free(img, track, sector)) return -1;
    
    int offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    uint8_t* bam = img->data + offset;
    
    int bam_offset = 4 + (track - 1) * 4;
    int byte_offset = 1 + (sector / 8);
    int bit = sector % 8;
    
    bam[bam_offset + byte_offset] &= ~(1 << bit);
    bam[bam_offset]--;  // Decrease free count
    
    img->is_modified = true;
    return 0;
}

int d64_free_sector(d64_image_t* img, int track, int sector) {
    if (!img || !img->data) return -1;
    
    int offset = D64_TRACK_OFFSET[18] * D64_SECTOR_SIZE;
    uint8_t* bam = img->data + offset;
    
    int bam_offset = 4 + (track - 1) * 4;
    int byte_offset = 1 + (sector / 8);
    int bit = sector % 8;
    
    if ((bam[bam_offset + byte_offset] & (1 << bit)) == 0) {
        bam[bam_offset + byte_offset] |= (1 << bit);
        bam[bam_offset]++;  // Increase free count
        img->is_modified = true;
    }
    
    return 0;
}

int d64_get_free_blocks(const d64_image_t* img) {
    return img ? img->free_blocks : 0;
}

// ============================================================================
// Directory API
// ============================================================================

int d64_read_directory(const d64_image_t* img, d64_dir_entry_t* entries,
                       int max_entries) {
    if (!img || !entries || max_entries <= 0) return -1;
    
    int count = 0;
    int track = D64_DIR_TRACK;
    int sector = D64_DIR_SECTOR;
    
    uint8_t buffer[256];
    
    while (track != 0 && count < max_entries) {
        if (d64_read_sector(img, track, sector, buffer) != 0) break;
        
        // 8 entries per sector
        for (int i = 0; i < 8 && count < max_entries; i++) {
            d64_dir_entry_t* entry = &entries[count];
            const uint8_t* src = buffer + i * 32;
            
            memcpy(entry, src, 32);
            
            // Only count valid entries
            if (entry->file_type != 0) {
                count++;
            }
        }
        
        // Next sector
        track = buffer[0];
        sector = buffer[1];
        
        if (track == 0) break;
        if (count > 144) break;  // Safety limit
    }
    
    return count;
}

int d64_find_file(const d64_image_t* img, const char* name,
                  d64_dir_entry_t* entry) {
    d64_dir_entry_t entries[144];
    int count = d64_read_directory(img, entries, 144);
    
    for (int i = 0; i < count; i++) {
        // Compare PETSCII name
        bool match = true;
        for (int j = 0; j < 16 && name[j]; j++) {
            uint8_t c = entries[i].filename[j];
            if (c == 0xA0) c = ' ';
            if (c != (uint8_t)name[j]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            *entry = entries[i];
            return 0;
        }
    }
    
    return -1;
}

bool d64_is_geos_file(const d64_dir_entry_t* entry) {
    if (!entry) return false;
    return (entry->file_type & 0x80) && entry->geos_type != GEOS_TYPE_NON_GEOS;
}

const char* d64_get_file_type_str(uint8_t file_type) {
    switch (file_type & 0x0F) {
        case D64_FTYPE_DEL: return "DEL";
        case D64_FTYPE_SEQ: return "SEQ";
        case D64_FTYPE_PRG: return "PRG";
        case D64_FTYPE_USR: return "USR";
        case D64_FTYPE_REL: return "REL";
        case D64_FTYPE_CBM: return "CBM";
        default: return "???";
    }
}

// ============================================================================
// GEOS Detection
// ============================================================================

bool d64_is_geos_disk(const d64_image_t* img) {
    return img && img->is_geos;
}

const char* d64_get_geos_type_str(d64_geos_type_t type) {
    switch (type) {
        case GEOS_TYPE_NON_GEOS:     return "Non-GEOS";
        case GEOS_TYPE_BASIC:        return "BASIC";
        case GEOS_TYPE_ASSEMBLER:    return "Assembler";
        case GEOS_TYPE_DATA:         return "Data File";
        case GEOS_TYPE_SYSTEM:       return "System File";
        case GEOS_TYPE_DESK_ACC:     return "Desk Accessory";
        case GEOS_TYPE_APPLICATION:  return "Application";
        case GEOS_TYPE_APP_DATA:     return "App Data";
        case GEOS_TYPE_FONT:         return "Font";
        case GEOS_TYPE_PRINTER:      return "Printer Driver";
        case GEOS_TYPE_INPUT_DRIVER: return "Input Driver";
        case GEOS_TYPE_DISK_DRIVER:  return "Disk Driver";
        case GEOS_TYPE_BOOT:         return "Boot File";
        case GEOS_TYPE_TEMPORARY:    return "Temporary";
        case GEOS_TYPE_AUTO_EXEC:    return "Auto-Exec";
        default:                     return "Unknown";
    }
}

// ============================================================================
// DOS Detection
// ============================================================================

bool d64_is_speeddos(const d64_image_t* img) {
    return img && img->is_speeddos;
}

bool d64_is_dolphindos(const d64_image_t* img) {
    return img && img->is_dolphindos;
}

bool d64_is_prologic(const d64_image_t* img) {
    return img && img->is_prologic;
}

// ============================================================================
// Utility
// ============================================================================

int d64_sectors_in_track(int track) {
    if (track < 1 || track > 42) return -1;
    return D64_SECTORS_PER_TRACK[track];
}

int d64_ts_to_lba(int track, int sector) {
    if (track < 1 || track > 42) return -1;
    if (sector < 0 || sector >= D64_SECTORS_PER_TRACK[track]) return -1;
    
    return D64_TRACK_OFFSET[track] + sector;
}

int d64_lba_to_ts(int lba, int* track, int* sector) {
    if (lba < 0 || !track || !sector) return -1;
    
    for (int t = 1; t <= 42; t++) {
        if (lba < D64_TRACK_OFFSET[t] + D64_SECTORS_PER_TRACK[t]) {
            *track = t;
            *sector = lba - D64_TRACK_OFFSET[t];
            return 0;
        }
    }
    
    return -1;
}

bool d64_is_valid_ts(int track, int sector, int max_tracks) {
    if (track < 1 || track > max_tracks) return false;
    if (sector < 0 || sector >= D64_SECTORS_PER_TRACK[track]) return false;
    return true;
}

const char* d64_variant_name(d64_variant_t variant) {
    if (variant & D64_VAR_GEOS) return "GEOS";
    if (variant & D64_VAR_SPEEDDOS) return "SpeedDOS";
    if (variant & D64_VAR_DOLPHINDOS) return "DolphinDOS";
    if (variant & D64_VAR_42_TRACK) return "42-Track";
    if (variant & D64_VAR_40_TRACK) return "40-Track";
    if (variant & D64_VAR_35_TRACK) return "35-Track";
    return "Unknown";
}

size_t d64_variant_size(d64_variant_t variant) {
    bool has_errors = (variant & D64_VAR_ERROR_INFO) != 0;
    
    if (variant & D64_VAR_42_TRACK) {
        return has_errors ? D64_SIZE_42_ERR : D64_SIZE_42;
    }
    if (variant & D64_VAR_40_TRACK) {
        return has_errors ? D64_SIZE_40_ERR : D64_SIZE_40;
    }
    return has_errors ? D64_SIZE_35_ERR : D64_SIZE_35;
}
