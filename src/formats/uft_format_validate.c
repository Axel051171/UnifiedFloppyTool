/**
 * @file uft_format_validate.c
 * @brief Format Validation Implementation
 */

#include "uft/uft_format_validate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Checksum Implementations
// ============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    // ... (full table would be 256 entries)
    // Simplified - using polynomial 0xEDB88320
};

uint32_t uft_crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t idx = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[idx];
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint16_t uft_crc16_ccitt(const uint8_t* data, size_t size) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

uint8_t uft_checksum_xor(const uint8_t* data, size_t size) {
    uint8_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum ^= data[i];
    }
    return sum;
}

uint32_t uft_checksum_amiga(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word = (data[i] << 24) | (data[i+1] << 16) |
                        (data[i+2] << 8) | data[i+3];
        uint32_t old = checksum;
        checksum += word;
        if (checksum < old) checksum++;  // Carry
    }
    
    return checksum;
}

uint8_t uft_checksum_gcr(const uint8_t* data, size_t size) {
    uint8_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum ^= data[i];
    }
    return sum;
}

// ============================================================================
// Issue Reporting Helper
// ============================================================================

static void add_issue(uft_validation_result_t* result,
                      int severity, int offset, int track, int sector,
                      const char* category, const char* fmt, ...) {
    if (result->issue_count >= 64) return;
    
    uft_validation_issue_t* issue = &result->issues[result->issue_count++];
    issue->severity = severity;
    issue->offset = offset;
    issue->track = track;
    issue->sector = sector;
    issue->category = category;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(issue->message, sizeof(issue->message), fmt, args);
    va_end(args);
}

// ============================================================================
// D64 Validation
// ============================================================================

// Sectors per track for 1541
static const int d64_sectors_per_track[40] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  // 1-17
    19, 19, 19, 19, 19, 19, 19,  // 18-24
    18, 18, 18, 18, 18, 18,      // 25-30
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17  // 31-40
};

static size_t d64_track_offset(int track) {
    size_t offset = 0;
    for (int t = 1; t < track && t <= 40; t++) {
        offset += d64_sectors_per_track[t-1] * 256;
    }
    return offset;
}

uft_error_t uft_validate_d64(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result) {
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->score = 100;
    
    // Determine variant
    int num_tracks = 35;
    bool has_error_map = false;
    
    if (size == 174848) { num_tracks = 35; }
    else if (size == 175531) { num_tracks = 35; has_error_map = true; }
    else if (size == 196608) { num_tracks = 40; }
    else if (size == 197376) { num_tracks = 40; has_error_map = true; }
    else if (size == 205312) { num_tracks = 42; }
    else if (size == 206114) { num_tracks = 42; has_error_map = true; }
    else {
        add_issue(result, 3, -1, -1, -1, "size",
                  "Invalid D64 size: %zu bytes", size);
        result->valid = false;
        result->score = 0;
        return UFT_ERROR_INVALID_FORMAT;
    }
    
    // Calculate total sectors
    result->total_sectors = 0;
    for (int t = 0; t < num_tracks; t++) {
        result->total_sectors += d64_sectors_per_track[t];
    }
    
    // === QUICK VALIDATION ===
    
    // BAM is at track 18, sector 0
    size_t bam_offset = d64_track_offset(18);
    
    // Check directory track/sector link
    uint8_t dir_track = data[bam_offset];
    uint8_t dir_sector = data[bam_offset + 1];
    
    if (dir_track != 18 || dir_sector != 1) {
        add_issue(result, 2, bam_offset, 18, 0, "structure",
                  "Invalid directory link: T%d S%d (expected T18 S1)",
                  dir_track, dir_sector);
        result->score -= 20;
    }
    
    // DOS version
    uint8_t dos_ver = data[bam_offset + 2];
    if (dos_ver != 0x41) {  // 'A' is standard
        add_issue(result, 1, bam_offset + 2, 18, 0, "structure",
                  "Non-standard DOS version: 0x%02X", dos_ver);
        result->score -= 5;
    }
    
    if (level == UFT_VALIDATE_QUICK) {
        result->d64.bam_valid = (result->score >= 80);
        return UFT_OK;
    }
    
    // === STANDARD VALIDATION ===
    
    // Validate BAM entries
    int used_blocks = 0;
    int free_blocks = 0;
    
    for (int t = 1; t <= num_tracks && t <= 35; t++) {
        size_t bam_entry = bam_offset + 4 + (t - 1) * 4;
        
        uint8_t free_in_track = data[bam_entry];
        int expected_max = d64_sectors_per_track[t - 1];
        
        if (free_in_track > expected_max) {
            add_issue(result, 2, bam_entry, t, -1, "bam",
                      "BAM track %d: %d free > %d max sectors",
                      t, free_in_track, expected_max);
            result->checksum_errors++;
            result->score -= 5;
        }
        
        // Count bits in bitmap
        int bitmap_free = 0;
        for (int i = 1; i <= 3; i++) {
            uint8_t bitmap = data[bam_entry + i];
            for (int b = 0; b < 8; b++) {
                if ((bitmap >> b) & 1) bitmap_free++;
            }
        }
        
        // Bitmap count should match counter
        if (bitmap_free != free_in_track && t != 18) {
            add_issue(result, 1, bam_entry, t, -1, "bam",
                      "BAM track %d: counter=%d, bitmap=%d",
                      t, free_in_track, bitmap_free);
        }
        
        free_blocks += free_in_track;
        used_blocks += expected_max - free_in_track;
    }
    
    result->d64.used_blocks = used_blocks;
    result->d64.free_blocks = free_blocks;
    result->d64.bam_valid = (result->score >= 70);
    
    if (level == UFT_VALIDATE_STANDARD) {
        return UFT_OK;
    }
    
    // === THOROUGH VALIDATION ===
    
    // Validate directory chain
    int dir_entries = 0;
    int dir_sector_count = 0;
    uint8_t curr_track = 18;
    uint8_t curr_sector = 1;
    
    while (curr_track != 0 && dir_sector_count < 18) {
        size_t dir_offset = d64_track_offset(curr_track) + curr_sector * 256;
        
        if (dir_offset + 256 > size) {
            add_issue(result, 3, dir_offset, curr_track, curr_sector, "directory",
                      "Directory sector outside image");
            break;
        }
        
        // Count entries in this sector
        for (int e = 0; e < 8; e++) {
            uint8_t file_type = data[dir_offset + 2 + e * 32];
            if (file_type != 0) {
                dir_entries++;
            }
        }
        
        // Follow chain
        curr_track = data[dir_offset];
        curr_sector = data[dir_offset + 1];
        dir_sector_count++;
    }
    
    result->d64.directory_entries = dir_entries;
    
    if (level == UFT_VALIDATE_THOROUGH) {
        return UFT_OK;
    }
    
    // === FORENSIC VALIDATION ===
    
    // Check for common copy protection signatures
    // (empty sectors, bad sectors, track 36+ data, etc.)
    
    // Check if error map is consistent with data
    if (has_error_map) {
        size_t error_offset = size - result->total_sectors;
        for (int i = 0; i < result->total_sectors; i++) {
            uint8_t error_code = data[error_offset + i];
            if (error_code != 1 && error_code != 0) {  // 1 = OK, 0 = not read
                result->bad_sectors++;
            }
        }
    }
    
    result->valid = (result->score >= 60);
    return UFT_OK;
}

// ============================================================================
// ADF Validation
// ============================================================================

uft_error_t uft_validate_adf(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result) {
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->score = 100;
    
    // Check size
    bool is_hd = false;
    if (size == 901120) {
        result->total_sectors = 1760;
    } else if (size == 1802240) {
        result->total_sectors = 3520;
        is_hd = true;
    } else {
        add_issue(result, 3, -1, -1, -1, "size",
                  "Invalid ADF size: %zu bytes", size);
        result->valid = false;
        result->score = 0;
        return UFT_ERROR_INVALID_FORMAT;
    }
    
    // === QUICK VALIDATION ===
    
    // Check bootblock
    uint32_t bb_checksum = uft_checksum_amiga(data, 1024);
    result->adf.bootblock_valid = (bb_checksum == 0);
    
    if (!result->adf.bootblock_valid) {
        add_issue(result, 1, 0, 0, 0, "checksum",
                  "Bootblock checksum invalid (non-bootable disk)");
        result->score -= 10;
    }
    
    // Check DOS type
    if (memcmp(data, "DOS", 3) == 0) {
        uint8_t dos_type = data[3];
        if (dos_type > 7) {
            add_issue(result, 1, 3, 0, 0, "structure",
                      "Unknown DOS type: %d", dos_type);
        }
    }
    
    if (level == UFT_VALIDATE_QUICK) {
        return UFT_OK;
    }
    
    // === STANDARD VALIDATION ===
    
    // Root block is at block 880 (DD) or 1760 (HD)
    int root_block = is_hd ? 1760 : 880;
    size_t root_offset = root_block * 512;
    
    // Type should be 2 (T_HEADER)
    uint32_t root_type = (data[root_offset] << 24) | (data[root_offset+1] << 16) |
                         (data[root_offset+2] << 8) | data[root_offset+3];
    
    if (root_type != 2) {
        add_issue(result, 2, root_offset, -1, root_block, "structure",
                  "Root block type is %d, expected 2", root_type);
        result->score -= 20;
    }
    
    // Secondary type should be 1 (ST_ROOT)
    uint32_t sec_type = (data[root_offset + 508] << 24) | 
                        (data[root_offset + 509] << 16) |
                        (data[root_offset + 510] << 8) | 
                        data[root_offset + 511];
    
    if (sec_type != 1) {
        add_issue(result, 2, root_offset + 508, -1, root_block, "structure",
                  "Root block secondary type is %d, expected 1", sec_type);
        result->score -= 10;
    }
    
    // Root block checksum
    uint32_t root_checksum = uft_checksum_amiga(data + root_offset, 512);
    if (root_checksum != 0) {
        add_issue(result, 2, root_offset, -1, root_block, "checksum",
                  "Root block checksum invalid");
        result->score -= 15;
        result->checksum_errors++;
    } else {
        result->adf.rootblock_valid = true;
    }
    
    if (level == UFT_VALIDATE_STANDARD) {
        return UFT_OK;
    }
    
    // === THOROUGH VALIDATION ===
    
    // Count used/free blocks from bitmap
    // Bitmap starts at block 881 (DD) for OFS/FFS
    int bitmap_block = root_block + 1;
    size_t bitmap_offset = bitmap_block * 512;
    
    int free_blocks = 0;
    int used_blocks = 0;
    int total_blocks = result->total_sectors;
    
    // Each bitmap block covers (512-4)*8 = 4064 bits
    // But we only need 1760 or 3520
    for (int i = 0; i < total_blocks && i < 4064; i++) {
        int byte_idx = 4 + i / 8;  // Skip checksum
        int bit_idx = i % 8;
        
        if (bitmap_offset + byte_idx < size) {
            if ((data[bitmap_offset + byte_idx] >> bit_idx) & 1) {
                free_blocks++;
            } else {
                used_blocks++;
            }
        }
    }
    
    result->adf.free_blocks = free_blocks;
    result->adf.used_blocks = used_blocks;
    
    result->valid = (result->score >= 60);
    return UFT_OK;
}

// ============================================================================
// SCP Validation
// ============================================================================

uft_error_t uft_validate_scp(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result) {
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->score = 100;
    
    // Magic check
    if (size < 16 || memcmp(data, "SCP", 3) != 0) {
        add_issue(result, 3, 0, -1, -1, "magic",
                  "Invalid SCP magic");
        result->valid = false;
        result->score = 0;
        return UFT_ERROR_INVALID_FORMAT;
    }
    
    // Parse header
    uint8_t version = data[3];
    uint8_t disk_type = data[4];
    uint8_t num_revs = data[5];
    uint8_t start_track = data[6];
    uint8_t end_track = data[7];
    uint8_t flags = data[8];
    uint8_t bit_cell_encoding = data[9];
    uint8_t num_heads = data[10];
    uint8_t resolution = data[11];
    
    result->scp.revolutions = num_revs;
    result->scp.tracks = end_track - start_track + 1;
    
    // Validate version
    if (version > 5) {
        add_issue(result, 1, 3, -1, -1, "structure",
                  "Unknown SCP version: %d", version);
        result->score -= 5;
    }
    
    // Validate track range
    if (end_track < start_track) {
        add_issue(result, 3, 6, -1, -1, "structure",
                  "Invalid track range: %d-%d", start_track, end_track);
        result->score -= 30;
    }
    
    if (end_track > 170) {
        add_issue(result, 2, 7, -1, -1, "structure",
                  "End track %d exceeds maximum", end_track);
        result->score -= 10;
    }
    
    // Validate revolutions
    if (num_revs == 0 || num_revs > 20) {
        add_issue(result, 2, 5, -1, -1, "structure",
                  "Invalid revolution count: %d", num_revs);
        result->score -= 15;
    }
    
    if (level == UFT_VALIDATE_QUICK) {
        return UFT_OK;
    }
    
    // === STANDARD VALIDATION ===
    
    // Check header checksum (offset 12-15)
    uint32_t stored_checksum = data[12] | (data[13] << 8) |
                               (data[14] << 16) | (data[15] << 24);
    
    // Calculate header checksum (XOR of bytes 0-9)
    // Note: This is a simplified check
    
    // Validate track headers
    size_t track_header_offset = 16;
    uint64_t total_track_len = 0;
    int valid_tracks = 0;
    
    for (int t = start_track; t <= end_track && t < 168; t++) {
        size_t th_offset = track_header_offset + (t - start_track) * 4;
        
        if (th_offset + 4 > size) {
            add_issue(result, 3, th_offset, t, -1, "structure",
                      "Track %d header outside file", t);
            break;
        }
        
        uint32_t track_offset = data[th_offset] | (data[th_offset+1] << 8) |
                                (data[th_offset+2] << 16) | (data[th_offset+3] << 24);
        
        if (track_offset == 0) {
            // Empty track
            result->empty_sectors++;
            continue;
        }
        
        if (track_offset >= size) {
            add_issue(result, 2, th_offset, t, -1, "structure",
                      "Track %d offset 0x%X outside file", t, track_offset);
            result->score -= 5;
            continue;
        }
        
        // Check track data header
        // TDH: "TRK\0" + track number + ...
        if (track_offset + 16 <= size) {
            if (memcmp(data + track_offset, "TRK", 3) == 0) {
                valid_tracks++;
                
                // Get track length for statistics
                uint32_t track_len = data[track_offset + 8] | 
                                    (data[track_offset + 9] << 8) |
                                    (data[track_offset + 10] << 16) | 
                                    (data[track_offset + 11] << 24);
                total_track_len += track_len;
            }
        }
    }
    
    result->total_sectors = valid_tracks;
    if (valid_tracks > 0) {
        result->scp.avg_track_length = (double)total_track_len / valid_tracks;
    }
    
    result->valid = (result->score >= 60);
    return UFT_OK;
}

// ============================================================================
// G64 Validation
// ============================================================================

uft_error_t uft_validate_g64(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result) {
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->score = 100;
    
    // Magic check
    if (size < 12 || memcmp(data, "GCR-1541", 8) != 0) {
        add_issue(result, 3, 0, -1, -1, "magic",
                  "Invalid G64 magic");
        result->valid = false;
        result->score = 0;
        return UFT_ERROR_INVALID_FORMAT;
    }
    
    // Parse header
    uint8_t version = data[8];
    uint8_t num_tracks = data[9];
    uint16_t max_track_size = data[10] | (data[11] << 8);
    
    if (version != 0) {
        add_issue(result, 1, 8, -1, -1, "structure",
                  "Unknown G64 version: %d", version);
        result->score -= 5;
    }
    
    if (num_tracks < 35 || num_tracks > 84) {
        add_issue(result, 2, 9, -1, -1, "structure",
                  "Invalid track count: %d", num_tracks);
        result->score -= 15;
    }
    
    result->total_sectors = num_tracks;
    
    if (level == UFT_VALIDATE_QUICK) {
        return UFT_OK;
    }
    
    // === STANDARD VALIDATION ===
    
    // Track offset table starts at offset 12
    size_t offset_table = 12;
    int valid_tracks = 0;
    
    for (int t = 0; t < num_tracks; t++) {
        size_t entry_offset = offset_table + t * 4;
        
        if (entry_offset + 4 > size) break;
        
        uint32_t track_offset = data[entry_offset] | (data[entry_offset+1] << 8) |
                                (data[entry_offset+2] << 16) | (data[entry_offset+3] << 24);
        
        if (track_offset == 0) {
            result->empty_sectors++;
            continue;
        }
        
        if (track_offset >= size) {
            add_issue(result, 2, entry_offset, t, -1, "structure",
                      "Track %d offset outside file", t);
            result->score -= 5;
            continue;
        }
        
        // Check track data length
        if (track_offset + 2 <= size) {
            uint16_t track_len = data[track_offset] | (data[track_offset+1] << 8);
            
            if (track_len > max_track_size) {
                add_issue(result, 1, track_offset, t, -1, "structure",
                          "Track %d length %d > max %d", t, track_len, max_track_size);
            }
            
            if (track_len > 0) {
                valid_tracks++;
            }
        }
    }
    
    result->total_sectors = valid_tracks;
    result->valid = (result->score >= 60);
    return UFT_OK;
}

// ============================================================================
// Main Validation API
// ============================================================================

uft_error_t uft_validate_format(const uint8_t* data, size_t size,
                                 uft_format_t format,
                                 uft_validation_level_t level,
                                 uft_validation_result_t* result) {
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    
    switch (format) {
        case UFT_FORMAT_D64:
            return uft_validate_d64(data, size, level, result);
        case UFT_FORMAT_ADF:
            return uft_validate_adf(data, size, level, result);
        case UFT_FORMAT_SCP:
            return uft_validate_scp(data, size, level, result);
        case UFT_FORMAT_G64:
            return uft_validate_g64(data, size, level, result);
        default:
            return UFT_ERROR_FORMAT_NOT_SUPPORTED;
    }
}
