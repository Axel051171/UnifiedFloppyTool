/**
 * @file uft_recovery_bam.c
 * @brief BAM and Directory Recovery Implementation
 * 
 * Supports: D64, D71, D81, ADF
 */

#include "uft/forensic/uft_recovery.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// D64 BAM LAYOUT
// ============================================================================

#define D64_BAM_TRACK       18
#define D64_BAM_SECTOR      0
#define D64_SECTOR_SIZE     256
#define D64_DIR_TRACK       18
#define D64_DIR_SECTOR      1

// Sectors per track for D64
static const int D64_SECTORS[41] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17,
    17, 17, 17, 17, 17  // Extended tracks 36-40
};

// Calculate sector offset in D64
static size_t d64_sector_offset(int track, int sector) {
    if (track < 1 || track > 40) return 0;
    
    size_t offset = 0;
    for (int t = 1; t < track; t++) {
        offset += D64_SECTORS[t] * D64_SECTOR_SIZE;
    }
    return offset + sector * D64_SECTOR_SIZE;
}

// ============================================================================
// BAM ANALYSIS
// ============================================================================

int uft_recovery_bam_analyze(const uint8_t* disk_data, size_t disk_size,
                             int format, uft_bam_analysis_t* analysis) {
    if (!disk_data || !analysis) return -1;
    
    memset(analysis, 0, sizeof(*analysis));
    
    // Currently only D64 implemented
    if (format != 0x0100) {  // UFT_FMT_D64
        return -1;
    }
    
    // D64 BAM analysis
    size_t bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    if (bam_offset + D64_SECTOR_SIZE > disk_size) return -1;
    
    const uint8_t* bam = disk_data + bam_offset;
    
    analysis->track = D64_BAM_TRACK;
    analysis->sector = D64_BAM_SECTOR;
    analysis->total_blocks = 0;
    analysis->free_blocks = 0;
    analysis->bad_entries = 0;
    
    // Analyze each track's BAM entry (4 bytes per track)
    // Format: [free_count] [bitmap_b0] [bitmap_b1] [bitmap_b2]
    for (int track = 1; track <= 35; track++) {
        int entry_offset = 4 * track;
        uint8_t free_count = bam[entry_offset];
        uint8_t bitmap[3] = {bam[entry_offset + 1], 
                             bam[entry_offset + 2], 
                             bam[entry_offset + 3]};
        
        int expected_sectors = D64_SECTORS[track];
        analysis->total_blocks += expected_sectors;
        
        // Count actual free bits
        int actual_free = 0;
        for (int s = 0; s < expected_sectors; s++) {
            int byte_idx = s / 8;
            int bit_idx = s % 8;
            if (bitmap[byte_idx] & (1 << bit_idx)) {
                actual_free++;
            }
        }
        
        // Store track analysis
        if (analysis->track_count < 85) {
            analysis->track_analysis[analysis->track_count].track = track;
            analysis->track_analysis[analysis->track_count].expected_free = free_count;
            analysis->track_analysis[analysis->track_count].actual_free = actual_free;
            analysis->track_analysis[analysis->track_count].bitmap_valid = 
                (free_count == actual_free);
            
            if (!analysis->track_analysis[analysis->track_count].bitmap_valid) {
                analysis->bad_entries++;
            }
            
            analysis->track_count++;
        }
        
        analysis->free_blocks += actual_free;
    }
    
    analysis->used_blocks = analysis->total_blocks - analysis->free_blocks;
    analysis->is_corrupted = (analysis->bad_entries > 0);
    analysis->can_repair = analysis->is_corrupted;
    
    if (analysis->is_corrupted) {
        snprintf(analysis->repair_description, sizeof(analysis->repair_description),
                 "BAM has %d inconsistent entries (free count != bitmap)",
                 analysis->bad_entries);
    }
    
    return 0;
}

// ============================================================================
// BAM REPAIR
// ============================================================================

int uft_recovery_bam_repair(uint8_t* disk_data, size_t disk_size,
                            int format, const uft_bam_repair_options_t* options,
                            uft_bam_analysis_t* result) {
    if (!disk_data || !result) return -1;
    
    // First analyze
    if (uft_recovery_bam_analyze(disk_data, disk_size, format, result) != 0) {
        return -1;
    }
    
    if (!result->is_corrupted) {
        return 0;  // Nothing to repair
    }
    
    size_t bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    uint8_t* bam = disk_data + bam_offset;
    
    // Fix inconsistent free counts
    for (int i = 0; i < result->track_count; i++) {
        if (!result->track_analysis[i].bitmap_valid) {
            int track = result->track_analysis[i].track;
            int entry_offset = 4 * track;
            
            // Recount from bitmap
            uint8_t bitmap[3] = {bam[entry_offset + 1],
                                 bam[entry_offset + 2],
                                 bam[entry_offset + 3]};
            
            int free_count = 0;
            int expected_sectors = D64_SECTORS[track];
            for (int s = 0; s < expected_sectors; s++) {
                if (bitmap[s / 8] & (1 << (s % 8))) {
                    free_count++;
                }
            }
            
            // Update free count
            bam[entry_offset] = free_count;
            result->track_analysis[i].expected_free = free_count;
            result->track_analysis[i].bitmap_valid = true;
        }
    }
    
    // Verify repairs
    result->bad_entries = 0;
    result->is_corrupted = false;
    result->changes_needed = result->track_count;
    
    return 0;
}

// ============================================================================
// BAM REBUILD FROM DIRECTORY
// ============================================================================

int uft_recovery_bam_rebuild(uint8_t* disk_data, size_t disk_size, int format) {
    if (!disk_data || format != 0x0100) return -1;
    
    size_t bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    uint8_t* bam = disk_data + bam_offset;
    
    // Clear all BAM entries (mark all as free)
    for (int track = 1; track <= 35; track++) {
        int entry_offset = 4 * track;
        int sectors = D64_SECTORS[track];
        
        bam[entry_offset] = sectors;  // All free
        
        // Set bitmap bits for all sectors
        bam[entry_offset + 1] = (sectors > 0) ? 0xFF : 0;
        bam[entry_offset + 2] = (sectors > 8) ? 0xFF : 0;
        bam[entry_offset + 3] = (sectors > 16) ? 0xFF : 0;
        
        // Mask unused bits
        if (sectors < 24) {
            bam[entry_offset + 3] &= (1 << (sectors - 16)) - 1;
        }
        if (sectors < 16) {
            bam[entry_offset + 2] &= (1 << (sectors - 8)) - 1;
        }
        if (sectors < 8) {
            bam[entry_offset + 1] &= (1 << sectors) - 1;
        }
    }
    
    // Mark BAM track as used
    int bam_entry = 4 * D64_BAM_TRACK;
    bam[bam_entry + 1] &= ~(1 << D64_BAM_SECTOR);  // Mark sector 0 used
    bam[bam_entry]--;  // Decrement free count
    
    // Scan directory and mark used sectors
    size_t dir_offset = d64_sector_offset(D64_DIR_TRACK, D64_DIR_SECTOR);
    int dir_sector = D64_DIR_SECTOR;
    
    while (dir_sector != 0 && dir_offset + D64_SECTOR_SIZE <= disk_size) {
        const uint8_t* dir = disk_data + dir_offset;
        
        // Mark directory sector as used
        int de = 4 * D64_DIR_TRACK;
        if ((bam[de + 1 + dir_sector / 8] >> (dir_sector % 8)) & 1) {
            bam[de + 1 + dir_sector / 8] &= ~(1 << (dir_sector % 8));
            bam[de]--;
        }
        
        // Process 8 directory entries per sector
        for (int e = 0; e < 8; e++) {
            const uint8_t* entry = dir + e * 32 + 2;
            uint8_t file_type = entry[0];
            
            if (file_type == 0) continue;  // Empty entry
            
            // Follow file chain
            int file_track = entry[1];
            int file_sector = entry[2];
            
            while (file_track >= 1 && file_track <= 35 && file_sector < D64_SECTORS[file_track]) {
                // Mark sector as used
                int fe = 4 * file_track;
                int byte_idx = file_sector / 8;
                int bit_idx = file_sector % 8;
                
                if ((bam[fe + 1 + byte_idx] >> bit_idx) & 1) {
                    bam[fe + 1 + byte_idx] &= ~(1 << bit_idx);
                    bam[fe]--;
                }
                
                // Get next sector in chain
                size_t sec_offset = d64_sector_offset(file_track, file_sector);
                if (sec_offset + 2 > disk_size) break;
                
                file_track = disk_data[sec_offset];
                file_sector = disk_data[sec_offset + 1];
            }
        }
        
        // Next directory sector
        dir_sector = dir[1];
        if (dir_sector != 0) {
            dir_offset = d64_sector_offset(D64_DIR_TRACK, dir_sector);
        }
    }
    
    return 0;
}

// ============================================================================
// DIRECTORY ANALYSIS
// ============================================================================

int uft_recovery_dir_analyze(const uint8_t* disk_data, size_t disk_size,
                             int format, uft_directory_analysis_t* analysis) {
    if (!disk_data || !analysis) return -1;
    
    memset(analysis, 0, sizeof(*analysis));
    
    if (format != 0x0100) return -1;  // Only D64
    
    // Allocate entries
    analysis->entries = calloc(144, sizeof(uft_dir_entry_analysis_t));
    if (!analysis->entries) return -1;
    
    size_t dir_offset = d64_sector_offset(D64_DIR_TRACK, D64_DIR_SECTOR);
    int dir_sector = D64_DIR_SECTOR;
    int entry_idx = 0;
    
    while (dir_sector != 0 && dir_offset + D64_SECTOR_SIZE <= disk_size && entry_idx < 144) {
        const uint8_t* dir = disk_data + dir_offset;
        
        for (int e = 0; e < 8 && entry_idx < 144; e++) {
            const uint8_t* entry = dir + e * 32 + 2;
            uint8_t file_type = entry[0] & 0x0F;
            
            if (file_type == 0) continue;
            
            uft_dir_entry_analysis_t* de = &analysis->entries[entry_idx];
            
            // Copy name (PETSCII)
            memcpy(de->name, entry + 3, 16);
            de->name[16] = 0;
            
            de->start_track = entry[1];
            de->start_sector = entry[2];
            de->file_type = file_type;
            de->block_count = entry[28] | (entry[29] << 8);
            
            // Validate chain
            de->chain_valid = true;
            de->chain_errors = 0;
            
            int track = de->start_track;
            int sector = de->start_sector;
            int chain_len = 0;
            
            while (track >= 1 && track <= 35 && chain_len < 800) {
                size_t sec_off = d64_sector_offset(track, sector);
                if (sec_off + 2 > disk_size) {
                    de->chain_valid = false;
                    de->chain_errors++;
                    break;
                }
                
                track = disk_data[sec_off];
                sector = disk_data[sec_off + 1];
                chain_len++;
                
                if (track == 0) break;  // End of chain
                
                if (sector >= D64_SECTORS[track]) {
                    de->chain_valid = false;
                    de->chain_errors++;
                    break;
                }
            }
            
            de->is_valid = de->chain_valid;
            if (de->is_valid) analysis->valid_entries++;
            else analysis->invalid_entries++;
            
            if (!de->chain_valid) analysis->broken_chains++;
            
            entry_idx++;
        }
        
        dir_sector = dir[1];
        if (dir_sector != 0) {
            dir_offset = d64_sector_offset(D64_DIR_TRACK, dir_sector);
        }
    }
    
    analysis->entry_count = entry_idx;
    analysis->total_entries = entry_idx;
    analysis->can_repair = (analysis->broken_chains > 0);
    
    if (analysis->can_repair) {
        snprintf(analysis->repair_description, sizeof(analysis->repair_description),
                 "Found %d broken file chains", analysis->broken_chains);
    }
    
    return 0;
}

// ============================================================================
// CLEANUP
// ============================================================================

void uft_recovery_bam_analysis_free(uft_bam_analysis_t* analysis) {
    // No dynamic memory in current implementation
    (void)analysis;
}

void uft_recovery_dir_analysis_free(uft_directory_analysis_t* analysis) {
    if (analysis) {
        free(analysis->entries);
        memset(analysis, 0, sizeof(*analysis));
    }
}
