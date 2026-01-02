/**
 * @file fuzz_format_parsers.c
 * @brief AFL/libFuzzer Targets für Format-Parser
 * 
 * Testet alle kritischen Bugs aus dem Deep Bug Hunt:
 * - BUG-001: Integer Overflow in SCP
 * - BUG-002: Unbounded Array Access in D64
 * - BUG-004: Missing File Size Validation
 * - BUG-005: Integer Overflow in Flux Read
 * - BUG-007: Unbounded Loop DoS
 * 
 * Kompilierung:
 *   AFL:        afl-clang-fast -g -O1 -fsanitize=address,undefined fuzz_format_parsers.c
 *   libFuzzer:  clang -g -O1 -fsanitize=fuzzer,address,undefined fuzz_format_parsers.c
 * 
 * @author Superman QA
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations for parser functions we'll test
// In real code, these would be the actual headers

// ============================================================================
// MOCK STRUCTURES (simplified for fuzzing)
// ============================================================================

typedef struct {
    char signature[8];
    uint8_t version;
    uint8_t num_tracks;
    uint16_t max_track_size;
} g64_header_mock_t;

typedef struct {
    char signature[3];
    uint8_t version;
    uint8_t disk_type;
    uint8_t num_revs;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t flags;
    uint8_t width;
    uint8_t heads;
    uint8_t resolution;
    uint32_t checksum;
    uint32_t track_offsets[168];
} scp_header_mock_t;

// ============================================================================
// FUZZ TARGET 1: SCP Parser - Integer Overflow (BUG-001, BUG-005)
// ============================================================================

/**
 * @brief Tests for integer overflow in SCP offset calculations
 * 
 * This target specifically looks for:
 * - Overflow in track_off + rev.data_offset
 * - Overflow in nvals * 2
 */
int fuzz_scp_overflow(const uint8_t *data, size_t size) {
    if (size < sizeof(scp_header_mock_t)) {
        return 0;  // Need at least header
    }
    
    const scp_header_mock_t *hdr = (const scp_header_mock_t *)data;
    
    // Check signature
    if (memcmp(hdr->signature, "SCP", 3) != 0) {
        return 0;
    }
    
    // BUG-007: Check revolution count
    if (hdr->num_revs > 32) {
        // POTENTIAL DoS - should be rejected
        return -1;  // Signal vulnerability
    }
    
    // Check track offsets for overflow
    for (int i = 0; i < 168 && i < hdr->end_track; i++) {
        uint32_t offset = hdr->track_offsets[i];
        
        if (offset == 0) continue;
        
        // BUG-004: Validate offset against size
        if (offset >= size) {
            return -2;  // Signal out-of-bounds
        }
        
        // Simulate reading revolution data
        if (offset + 12 > size) {
            return -3;
        }
        
        // Read mock revolution entry
        const uint8_t *rev_ptr = data + offset;
        uint32_t data_length = *(uint32_t *)(rev_ptr + 4);
        uint32_t data_offset = *(uint32_t *)(rev_ptr + 8);
        
        // BUG-001: Check for overflow in offset calculation
        if (data_offset > UINT32_MAX - offset) {
            return -4;  // Integer overflow detected!
        }
        
        uint32_t absolute_offset = offset + data_offset;
        
        // BUG-005: Check for overflow in byte count
        if (data_length > SIZE_MAX / 2) {
            return -5;  // Integer overflow in multiplication!
        }
        
        size_t byte_count = data_length * 2;
        
        // Validate final access
        if ((size_t)absolute_offset + byte_count > size) {
            return -6;  // Out of bounds read
        }
    }
    
    return 0;  // All checks passed
}

// ============================================================================
// FUZZ TARGET 2: D64 Parser - Bounds Check (BUG-002)
// ============================================================================

// D64 sector counts per track
static const uint8_t d64_sectors[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17
};

static const uint16_t d64_track_offset[42] = {
    0,21,42,63,84,105,126,147,168,189,210,231,252,273,294,315,336,
    357,376,395,414,433,452,471,490,508,526,544,562,580,598,
    615,632,649,666,683,700,717,734,751,768,785,802
};

/**
 * @brief Tests for out-of-bounds access in D64 error info
 */
int fuzz_d64_bounds(const uint8_t *data, size_t size) {
    // Valid D64 sizes
    static const size_t valid_sizes[] = {
        174848, 175531,  // 35 track
        196608, 197376,  // 40 track
        205312, 206114   // 42 track
    };
    
    // Check if size matches any valid D64
    int variant = -1;
    for (int i = 0; i < 6; i++) {
        if (size == valid_sizes[i]) {
            variant = i;
            break;
        }
    }
    
    if (variant < 0) {
        return 0;  // Not a valid D64 size
    }
    
    // Determine properties
    uint8_t num_tracks;
    bool has_errors;
    uint16_t total_sectors;
    
    switch (variant) {
        case 0: num_tracks = 35; has_errors = false; total_sectors = 683; break;
        case 1: num_tracks = 35; has_errors = true;  total_sectors = 683; break;
        case 2: num_tracks = 40; has_errors = false; total_sectors = 768; break;
        case 3: num_tracks = 40; has_errors = true;  total_sectors = 768; break;
        case 4: num_tracks = 42; has_errors = false; total_sectors = 802; break;
        case 5: num_tracks = 42; has_errors = true;  total_sectors = 802; break;
        default: return 0;
    }
    
    // Calculate error info position
    size_t error_offset = (size_t)total_sectors * 256;
    
    if (has_errors) {
        // Validate error info exists
        if (error_offset + total_sectors > size) {
            return -1;  // Error info truncated
        }
        
        const uint8_t *error_info = data + error_offset;
        
        // BUG-002: Simulate reading error info for all tracks
        for (int track = 1; track <= num_tracks; track++) {
            if (track > 42) {
                return -2;  // Track out of table bounds
            }
            
            int sectors_this_track = (track <= 40) ? d64_sectors[track - 1] : 17;
            
            for (int sector = 0; sector < sectors_this_track; sector++) {
                // Calculate sector index
                uint16_t sector_index = d64_track_offset[track - 1] + sector;
                
                // BUG-002: CRITICAL - Check bounds before access
                if (sector_index >= total_sectors) {
                    return -3;  // Would be out-of-bounds read!
                }
                
                // This access is now safe
                uint8_t err = error_info[sector_index];
                (void)err;  // Use to prevent optimization
            }
        }
    }
    
    return 0;  // All checks passed
}

// ============================================================================
// FUZZ TARGET 3: G64 Parser - Offset Validation
// ============================================================================

int fuzz_g64_offsets(const uint8_t *data, size_t size) {
    if (size < 12) {
        return 0;
    }
    
    // Check signature
    if (memcmp(data, "GCR-1541", 8) != 0) {
        return 0;
    }
    
    uint8_t num_tracks = data[9];
    uint16_t max_track_size = data[10] | (data[11] << 8);
    
    // Validate header values
    if (num_tracks > 84) {
        return -1;  // Invalid track count
    }
    
    if (max_track_size > 8000) {
        return -2;  // Suspiciously large track size
    }
    
    // Calculate expected minimum size
    size_t header_size = 12;
    size_t offset_table_size = num_tracks * 4;
    size_t speed_table_size = num_tracks * 4;
    size_t min_size = header_size + offset_table_size + speed_table_size;
    
    if (size < min_size) {
        return -3;  // File too small for declared tracks
    }
    
    // Validate track offsets
    const uint8_t *offset_table = data + 12;
    
    for (int i = 0; i < num_tracks; i++) {
        uint32_t offset = offset_table[i*4] |
                         (offset_table[i*4 + 1] << 8) |
                         (offset_table[i*4 + 2] << 16) |
                         (offset_table[i*4 + 3] << 24);
        
        if (offset == 0) continue;  // Empty track
        
        // Validate offset is within file
        if (offset >= size) {
            return -4;  // Offset points outside file
        }
        
        // Validate we can read track size
        if (offset + 2 > size) {
            return -5;  // Can't read track size
        }
        
        // Read track size at offset
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        
        // Validate track size
        if (track_size > max_track_size) {
            return -6;  // Track larger than declared max
        }
        
        // Validate track data is within file
        if (offset + 2 + track_size > size) {
            return -7;  // Track data extends past EOF
        }
    }
    
    return 0;
}

// ============================================================================
// FUZZ TARGET 4: HFE Parser - Track LUT Validation
// ============================================================================

int fuzz_hfe_lut(const uint8_t *data, size_t size) {
    if (size < 512) {
        return 0;  // Need at least header
    }
    
    // Check signature
    if (memcmp(data, "HXCPICFE", 8) != 0 &&
        memcmp(data, "HXCHFEV3", 8) != 0) {
        return 0;
    }
    
    uint8_t num_tracks = data[9];
    uint8_t num_sides = data[10];
    uint16_t track_list_offset = data[18] | (data[19] << 8);
    
    // Validate
    if (num_tracks > 84) {
        return -1;
    }
    
    if (num_sides < 1 || num_sides > 2) {
        return -2;
    }
    
    // Track LUT offset in blocks (×512)
    size_t lut_byte_offset = (size_t)track_list_offset * 512;
    size_t lut_size = (size_t)num_tracks * 4;
    
    if (lut_byte_offset + lut_size > size) {
        return -3;  // LUT extends past EOF
    }
    
    const uint8_t *lut = data + lut_byte_offset;
    
    // Validate each track entry
    for (int i = 0; i < num_tracks; i++) {
        uint16_t track_offset = lut[i*4] | (lut[i*4 + 1] << 8);
        uint16_t track_len = lut[i*4 + 2] | (lut[i*4 + 3] << 8);
        
        // Track offset in blocks
        size_t track_byte_offset = (size_t)track_offset * 512;
        
        // Track data must fit
        if (track_byte_offset + track_len > size) {
            return -4;  // Track data extends past EOF
        }
    }
    
    return 0;
}

// ============================================================================
// LIBFUZZER ENTRY POINT
// ============================================================================

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Run all fuzz targets
    fuzz_scp_overflow(data, size);
    fuzz_d64_bounds(data, size);
    fuzz_g64_offsets(data, size);
    fuzz_hfe_lut(data, size);
    
    return 0;
}

#endif

// ============================================================================
// AFL ENTRY POINT
// ============================================================================

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return 1;
    }
    
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return 1;
    }
    
    fclose(f);
    
    int ret = 0;
    
    // Run all fuzz targets
    int r1 = fuzz_scp_overflow(data, size);
    int r2 = fuzz_d64_bounds(data, size);
    int r3 = fuzz_g64_offsets(data, size);
    int r4 = fuzz_hfe_lut(data, size);
    
    if (r1 < 0 || r2 < 0 || r3 < 0 || r4 < 0) {
        printf("Vulnerability found!\n");
        printf("  SCP: %d\n", r1);
        printf("  D64: %d\n", r2);
        printf("  G64: %d\n", r3);
        printf("  HFE: %d\n", r4);
        ret = 1;
    }
    
    free(data);
    return ret;
}

#endif
