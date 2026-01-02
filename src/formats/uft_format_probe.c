/**
 * @file uft_format_probe.c
 * @brief Format Probe Pipeline Implementation
 */

#include "uft/uft_format_probe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// D64 Format Variants
// ============================================================================

static int d64_validate_35(const uint8_t* data, size_t size);
static int d64_validate_40(const uint8_t* data, size_t size);
static int d64_validate_42(const uint8_t* data, size_t size);

static const uft_format_variant_t d64_variants[] = {
    {
        .name = "D64-35",
        .description = "Standard 35 track",
        .base_format = UFT_FORMAT_D64,
        .exact_sizes = { 174848, 175531, 0 },  // Without/with error map
        .cylinders = 35, .heads = 1,
        .sectors_min = 17, .sectors_max = 21,
        .sector_size = 256,
        .validate = d64_validate_35,
    },
    {
        .name = "D64-40",
        .description = "40 track extended",
        .base_format = UFT_FORMAT_D64,
        .exact_sizes = { 196608, 197376, 0 },
        .cylinders = 40, .heads = 1,
        .sectors_min = 17, .sectors_max = 21,
        .sector_size = 256,
        .validate = d64_validate_40,
    },
    {
        .name = "D64-42",
        .description = "42 track extended",
        .base_format = UFT_FORMAT_D64,
        .exact_sizes = { 205312, 206114, 0 },
        .cylinders = 42, .heads = 1,
        .sectors_min = 17, .sectors_max = 21,
        .sector_size = 256,
        .validate = d64_validate_42,
    },
};

// ============================================================================
// ADF Format Variants
// ============================================================================

static int adf_validate_dd(const uint8_t* data, size_t size);
static int adf_validate_hd(const uint8_t* data, size_t size);

static const uft_format_variant_t adf_variants[] = {
    {
        .name = "ADF-DD",
        .description = "Amiga DD 880KB",
        .base_format = UFT_FORMAT_ADF,
        .exact_sizes = { 901120, 0 },
        .cylinders = 80, .heads = 2,
        .sectors_min = 11, .sectors_max = 11,
        .sector_size = 512,
        .validate = adf_validate_dd,
    },
    {
        .name = "ADF-HD",
        .description = "Amiga HD 1.76MB",
        .base_format = UFT_FORMAT_ADF,
        .exact_sizes = { 1802240, 0 },
        .cylinders = 80, .heads = 2,
        .sectors_min = 22, .sectors_max = 22,
        .sector_size = 512,
        .validate = adf_validate_hd,
    },
};

// ============================================================================
// IMG Format Variants
// ============================================================================

static const uft_format_variant_t img_variants[] = {
    {
        .name = "IMG-160K",
        .description = "PC 160KB SS/DD",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 163840, 0 },
        .cylinders = 40, .heads = 1,
        .sectors_min = 8, .sectors_max = 8,
        .sector_size = 512,
    },
    {
        .name = "IMG-180K",
        .description = "PC 180KB SS/DD",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 184320, 0 },
        .cylinders = 40, .heads = 1,
        .sectors_min = 9, .sectors_max = 9,
        .sector_size = 512,
    },
    {
        .name = "IMG-320K",
        .description = "PC 320KB DS/DD",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 327680, 0 },
        .cylinders = 40, .heads = 2,
        .sectors_min = 8, .sectors_max = 8,
        .sector_size = 512,
    },
    {
        .name = "IMG-360K",
        .description = "PC 360KB DS/DD",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 368640, 0 },
        .cylinders = 40, .heads = 2,
        .sectors_min = 9, .sectors_max = 9,
        .sector_size = 512,
    },
    {
        .name = "IMG-720K",
        .description = "PC 720KB DS/DD 3.5\"",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 737280, 0 },
        .cylinders = 80, .heads = 2,
        .sectors_min = 9, .sectors_max = 9,
        .sector_size = 512,
    },
    {
        .name = "IMG-1200K",
        .description = "PC 1.2MB DS/HD 5.25\"",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 1228800, 0 },
        .cylinders = 80, .heads = 2,
        .sectors_min = 15, .sectors_max = 15,
        .sector_size = 512,
    },
    {
        .name = "IMG-1440K",
        .description = "PC 1.44MB DS/HD 3.5\"",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 1474560, 0 },
        .cylinders = 80, .heads = 2,
        .sectors_min = 18, .sectors_max = 18,
        .sector_size = 512,
    },
    {
        .name = "IMG-2880K",
        .description = "PC 2.88MB DS/ED 3.5\"",
        .base_format = UFT_FORMAT_IMG,
        .exact_sizes = { 2949120, 0 },
        .cylinders = 80, .heads = 2,
        .sectors_min = 36, .sectors_max = 36,
        .sector_size = 512,
    },
};

// ============================================================================
// Probe Functions - D64
// ============================================================================

static int probe_d64_magic(const uint8_t* data, size_t size) {
    // D64 has no magic bytes
    return 0;
}

static int probe_d64_size(size_t size) {
    // Check all valid D64 sizes
    size_t valid[] = { 174848, 175531, 196608, 197376, 205312, 206114 };
    for (int i = 0; i < 6; i++) {
        if (size == valid[i]) return 20;  // Size match
    }
    return 0;
}

static int probe_d64_structure(const uint8_t* data, size_t size) {
    if (size < 174848) return 0;
    
    int score = 0;
    
    // Check BAM (Block Availability Map) at track 18, sector 0
    // Offset: sum of sectors in tracks 1-17
    size_t bam_offset = 0;
    int sectors_per_track[] = { 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21 };
    for (int i = 0; i < 17; i++) bam_offset += sectors_per_track[i] * 256;
    
    // BAM signature: first two bytes should be track/sector of directory
    if (data[bam_offset] == 18 && data[bam_offset + 1] == 1) {
        score += 15;  // Directory link valid
    }
    
    // Disk DOS version (offset +2)
    uint8_t dos_ver = data[bam_offset + 2];
    if (dos_ver == 0x41 || dos_ver == 0x42) {  // 'A' or 'B'
        score += 10;
    }
    
    // Check disk name (offset +144, 16 bytes, should be PETSCII)
    bool valid_name = true;
    for (int i = 0; i < 16; i++) {
        uint8_t c = data[bam_offset + 144 + i];
        if (c != 0xA0 && (c < 0x20 || c > 0x7F)) {
            valid_name = false;
            break;
        }
    }
    if (valid_name) score += 5;
    
    return score;
}

static int probe_d64_heuristic(const uint8_t* data, size_t size) {
    // Check for typical CBM data patterns
    // GCR encoding leaves traces even in sector data
    return 0;  // No additional heuristics
}

static int d64_validate_35(const uint8_t* data, size_t size) {
    return (size == 174848 || size == 175531) ? 100 : 0;
}

static int d64_validate_40(const uint8_t* data, size_t size) {
    return (size == 196608 || size == 197376) ? 100 : 0;
}

static int d64_validate_42(const uint8_t* data, size_t size) {
    return (size == 205312 || size == 206114) ? 100 : 0;
}

// ============================================================================
// Probe Functions - ADF
// ============================================================================

static int probe_adf_magic(const uint8_t* data, size_t size) {
    // ADF has no magic, but bootblock has signature
    if (size >= 4) {
        // DOS\0 for DOS disks
        if (memcmp(data, "DOS", 3) == 0) {
            uint8_t flags = data[3];
            if (flags <= 7) return 30;  // Valid DOS type
        }
        // KICK for Kickstart disks
        if (memcmp(data, "KICK", 4) == 0) return 25;
    }
    return 0;
}

static int probe_adf_size(size_t size) {
    if (size == 901120) return 20;   // DD
    if (size == 1802240) return 20;  // HD
    return 0;
}

static int probe_adf_structure(const uint8_t* data, size_t size) {
    if (size < 512) return 0;
    
    int score = 0;
    
    // Check bootblock checksum
    uint32_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        uint32_t word = (data[i*4] << 24) | (data[i*4+1] << 16) |
                        (data[i*4+2] << 8) | data[i*4+3];
        uint32_t old = checksum;
        checksum += word;
        if (checksum < old) checksum++;  // Carry
    }
    if (checksum == 0) score += 20;
    
    // Root block at middle of disk (block 880 for DD)
    if (size >= 901120) {
        size_t root_offset = 880 * 512;
        // Type should be 2 (T_HEADER)
        uint32_t type = (data[root_offset] << 24) | (data[root_offset+1] << 16) |
                        (data[root_offset+2] << 8) | data[root_offset+3];
        if (type == 2) score += 10;
    }
    
    return score;
}

static int adf_validate_dd(const uint8_t* data, size_t size) {
    return (size == 901120) ? 100 : 0;
}

static int adf_validate_hd(const uint8_t* data, size_t size) {
    return (size == 1802240) ? 100 : 0;
}

// ============================================================================
// Probe Functions - SCP
// ============================================================================

static int probe_scp_magic(const uint8_t* data, size_t size) {
    if (size >= 3 && memcmp(data, "SCP", 3) == 0) {
        return 40;  // Perfect magic match
    }
    return 0;
}

static int probe_scp_structure(const uint8_t* data, size_t size) {
    if (size < 16) return 0;
    
    int score = 0;
    
    // Check header fields
    uint8_t version = data[3];
    if (version <= 5) score += 10;  // Known versions 0-5
    
    uint8_t disk_type = data[4];
    if (disk_type <= 3) score += 5;  // Valid disk types
    
    uint8_t num_revs = data[5];
    if (num_revs >= 1 && num_revs <= 20) score += 5;
    
    // Start track, end track
    uint8_t start = data[6];
    uint8_t end = data[7];
    if (end > start && end <= 170) score += 10;
    
    return score;
}

// ============================================================================
// Probe Functions - HFE
// ============================================================================

static int probe_hfe_magic(const uint8_t* data, size_t size) {
    if (size >= 8 && memcmp(data, "HXCPICFE", 8) == 0) {
        return 40;
    }
    if (size >= 8 && memcmp(data, "HXCHFE3", 7) == 0) {
        return 40;  // HFEv3
    }
    return 0;
}

static int probe_hfe_structure(const uint8_t* data, size_t size) {
    if (size < 512) return 0;
    
    int score = 0;
    
    // Version at offset 8
    uint8_t version = data[8];
    if (version <= 3) score += 10;
    
    // Number of tracks at offset 9
    uint8_t tracks = data[9];
    if (tracks > 0 && tracks <= 170) score += 10;
    
    // Number of sides at offset 10
    uint8_t sides = data[10];
    if (sides == 1 || sides == 2) score += 10;
    
    return score;
}

// ============================================================================
// Probe Functions - G64
// ============================================================================

static int probe_g64_magic(const uint8_t* data, size_t size) {
    if (size >= 8 && memcmp(data, "GCR-1541", 8) == 0) {
        return 40;
    }
    return 0;
}

static int probe_g64_structure(const uint8_t* data, size_t size) {
    if (size < 12) return 0;
    
    int score = 0;
    
    // Version at offset 8
    uint8_t version = data[8];
    if (version == 0) score += 10;  // Version 0
    
    // Number of tracks at offset 9
    uint8_t tracks = data[9];
    if (tracks >= 35 && tracks <= 84) score += 10;
    
    // Max track size at offset 10-11 (little endian)
    uint16_t max_track = data[10] | (data[11] << 8);
    if (max_track >= 7000 && max_track <= 8000) score += 10;
    
    return score;
}

// ============================================================================
// Probe Functions - IPF
// ============================================================================

static int probe_ipf_magic(const uint8_t* data, size_t size) {
    if (size >= 4 && memcmp(data, "CAPS", 4) == 0) {
        return 40;
    }
    return 0;
}

static int probe_ipf_structure(const uint8_t* data, size_t size) {
    if (size < 24) return 0;
    
    int score = 0;
    
    // Encoder type at offset 12
    uint32_t encoder = (data[12] << 24) | (data[13] << 16) |
                       (data[14] << 8) | data[15];
    if (encoder <= 2) score += 15;  // Valid encoder types
    
    // Platform at offset 48
    // Skip for now
    score += 15;
    
    return score;
}

// ============================================================================
// Probe Functions - IMG (Raw)
// ============================================================================

static int probe_img_magic(const uint8_t* data, size_t size) {
    // IMG has no magic, but FAT boot sector has signature
    if (size >= 512) {
        // Boot signature 0x55 0xAA at offset 510
        if (data[510] == 0x55 && data[511] == 0xAA) {
            return 25;
        }
        // BPB validation
        if (data[0] == 0xEB || data[0] == 0xE9) {  // Jump instruction
            return 15;
        }
    }
    return 0;
}

static int probe_img_size(size_t size) {
    // Check standard sizes
    size_t valid[] = { 163840, 184320, 327680, 368640, 
                       737280, 1228800, 1474560, 2949120 };
    for (int i = 0; i < 8; i++) {
        if (size == valid[i]) return 20;
    }
    // Non-standard but valid
    if (size > 0 && size <= 2949120 && (size % 512) == 0) {
        return 10;
    }
    return 0;
}

static int probe_img_structure(const uint8_t* data, size_t size) {
    if (size < 512) return 0;
    
    int score = 0;
    
    // Validate BPB (BIOS Parameter Block)
    uint16_t bytes_per_sector = data[11] | (data[12] << 8);
    if (bytes_per_sector == 512) score += 5;
    
    uint8_t sectors_per_cluster = data[13];
    if (sectors_per_cluster >= 1 && sectors_per_cluster <= 128 &&
        (sectors_per_cluster & (sectors_per_cluster - 1)) == 0) {  // Power of 2
        score += 5;
    }
    
    uint8_t num_fats = data[16];
    if (num_fats == 1 || num_fats == 2) score += 5;
    
    uint8_t media_descriptor = data[21];
    // Valid media descriptors: F0, F8, F9, FA, FB, FC, FD, FE, FF
    if (media_descriptor >= 0xF0) score += 5;
    
    uint16_t sectors_per_fat = data[22] | (data[23] << 8);
    if (sectors_per_fat > 0 && sectors_per_fat <= 20) score += 5;
    
    return score;
}

// ============================================================================
// Probe Handler Registry
// ============================================================================

static const uft_probe_handler_t g_probe_handlers[] = {
    // FLUX formats (highest priority for magic)
    {
        .format = UFT_FORMAT_SCP,
        .name = "SCP",
        .format_class = UFT_CLASS_FLUX,
        .probe_magic = probe_scp_magic,
        .probe_structure = probe_scp_structure,
    },
    {
        .format = UFT_FORMAT_HFE,
        .name = "HFE",
        .format_class = UFT_CLASS_BITSTREAM,
        .probe_magic = probe_hfe_magic,
        .probe_structure = probe_hfe_structure,
    },
    {
        .format = UFT_FORMAT_IPF,
        .name = "IPF",
        .format_class = UFT_CLASS_CONTAINER,
        .probe_magic = probe_ipf_magic,
        .probe_structure = probe_ipf_structure,
    },
    {
        .format = UFT_FORMAT_G64,
        .name = "G64",
        .format_class = UFT_CLASS_BITSTREAM,
        .probe_magic = probe_g64_magic,
        .probe_structure = probe_g64_structure,
    },
    
    // SECTOR formats (size + structure based)
    {
        .format = UFT_FORMAT_D64,
        .name = "D64",
        .format_class = UFT_CLASS_SECTOR,
        .probe_magic = probe_d64_magic,
        .probe_size = probe_d64_size,
        .probe_structure = probe_d64_structure,
        .probe_heuristic = probe_d64_heuristic,
        .variants = d64_variants,
        .variant_count = sizeof(d64_variants) / sizeof(d64_variants[0]),
    },
    {
        .format = UFT_FORMAT_ADF,
        .name = "ADF",
        .format_class = UFT_CLASS_SECTOR,
        .probe_magic = probe_adf_magic,
        .probe_size = probe_adf_size,
        .probe_structure = probe_adf_structure,
        .variants = adf_variants,
        .variant_count = sizeof(adf_variants) / sizeof(adf_variants[0]),
    },
    {
        .format = UFT_FORMAT_IMG,
        .name = "IMG",
        .format_class = UFT_CLASS_SECTOR,
        .probe_magic = probe_img_magic,
        .probe_size = probe_img_size,
        .probe_structure = probe_img_structure,
        .variants = img_variants,
        .variant_count = sizeof(img_variants) / sizeof(img_variants[0]),
    },
    
    // Terminator
    { .format = UFT_FORMAT_UNKNOWN }
};

#define NUM_PROBE_HANDLERS \
    (sizeof(g_probe_handlers) / sizeof(g_probe_handlers[0]) - 1)

// ============================================================================
// Main Probe API
// ============================================================================

static int calculate_confidence(const uft_probe_handler_t* handler,
                                 const uint8_t* data, size_t size,
                                 uft_probe_result_t* result) {
    int confidence = 0;
    
    // Stage 1: Magic bytes (+40 max)
    if (handler->probe_magic) {
        int magic_score = handler->probe_magic(data, size);
        if (magic_score > 0) {
            result->magic_matched = true;
            confidence += magic_score;
        }
    }
    
    // Stage 2: Size check (+20 max)
    if (handler->probe_size) {
        int size_score = handler->probe_size(size);
        if (size_score > 0) {
            result->size_matched = true;
            confidence += size_score;
        }
    }
    
    // Stage 3: Structure validation (+30 max)
    if (handler->probe_structure) {
        int struct_score = handler->probe_structure(data, size);
        if (struct_score > 0) {
            result->structure_valid = true;
            confidence += struct_score;
        }
    }
    
    // Stage 4: Heuristics (+10 max)
    if (handler->probe_heuristic) {
        int heur_score = handler->probe_heuristic(data, size);
        confidence += heur_score;
    }
    
    return confidence > 100 ? 100 : confidence;
}

static const uft_format_variant_t* find_variant(const uft_probe_handler_t* handler,
                                                  const uint8_t* data, size_t size) {
    if (!handler->variants || handler->variant_count == 0) return NULL;
    
    for (size_t i = 0; i < handler->variant_count; i++) {
        const uft_format_variant_t* v = &handler->variants[i];
        
        // Check exact sizes
        for (int j = 0; v->exact_sizes[j] != 0; j++) {
            if (size == v->exact_sizes[j]) {
                // Validate if function provided
                if (v->validate) {
                    if (v->validate(data, size) > 0) {
                        return v;
                    }
                } else {
                    return v;
                }
            }
        }
        
        // Check size range
        if (v->min_size && v->max_size) {
            if (size >= v->min_size && size <= v->max_size) {
                if (!v->validate || v->validate(data, size) > 0) {
                    return v;
                }
            }
        }
    }
    
    return NULL;
}

uft_format_t uft_probe_format(const uint8_t* data, size_t size,
                               const char* filename,
                               uft_probe_result_t* result) {
    if (!data || size == 0 || !result) return UFT_FORMAT_UNKNOWN;
    
    memset(result, 0, sizeof(*result));
    result->format = UFT_FORMAT_UNKNOWN;
    
    int best_confidence = 0;
    const uft_probe_handler_t* best_handler = NULL;
    
    // Try all handlers
    for (size_t i = 0; i < NUM_PROBE_HANDLERS; i++) {
        const uft_probe_handler_t* h = &g_probe_handlers[i];
        
        uft_probe_result_t temp = {0};
        int conf = calculate_confidence(h, data, size, &temp);
        
        if (conf > best_confidence) {
            best_confidence = conf;
            best_handler = h;
            *result = temp;
        }
        
        // Track alternatives
        if (conf >= 40 && conf < best_confidence && result->alternative_count < 4) {
            result->alternatives[result->alternative_count] = h->format;
            result->alt_confidence[result->alternative_count] = conf;
            result->alternative_count++;
        }
    }
    
    if (best_handler && best_confidence >= 40) {
        result->format = best_handler->format;
        result->confidence = best_confidence;
        result->format_class = best_handler->format_class;
        result->variant = find_variant(best_handler, data, size);
        
        return best_handler->format;
    }
    
    // Low confidence - unknown
    snprintf(result->warnings, sizeof(result->warnings),
             "Could not determine format (best confidence: %d%%)", best_confidence);
    
    return UFT_FORMAT_UNKNOWN;
}

int uft_probe_specific(const uint8_t* data, size_t size,
                        uft_format_t format,
                        uft_probe_result_t* result) {
    if (!data || size == 0 || !result) return 0;
    
    memset(result, 0, sizeof(*result));
    
    for (size_t i = 0; i < NUM_PROBE_HANDLERS; i++) {
        if (g_probe_handlers[i].format == format) {
            result->confidence = calculate_confidence(&g_probe_handlers[i], 
                                                       data, size, result);
            result->format = format;
            result->format_class = g_probe_handlers[i].format_class;
            result->variant = find_variant(&g_probe_handlers[i], data, size);
            return result->confidence;
        }
    }
    
    return 0;
}

int uft_probe_all(const uint8_t* data, size_t size,
                   uft_probe_result_t* results, int max_results) {
    if (!data || size == 0 || !results || max_results == 0) return 0;
    
    // Collect all results
    typedef struct {
        int confidence;
        size_t handler_idx;
    } score_entry_t;
    
    score_entry_t scores[32];
    int count = 0;
    
    for (size_t i = 0; i < NUM_PROBE_HANDLERS && count < 32; i++) {
        uft_probe_result_t temp = {0};
        int conf = calculate_confidence(&g_probe_handlers[i], data, size, &temp);
        
        if (conf > 0) {
            scores[count].confidence = conf;
            scores[count].handler_idx = i;
            count++;
        }
    }
    
    // Sort by confidence (bubble sort, small N)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (scores[j].confidence < scores[j+1].confidence) {
                score_entry_t tmp = scores[j];
                scores[j] = scores[j+1];
                scores[j+1] = tmp;
            }
        }
    }
    
    // Fill results
    int result_count = count < max_results ? count : max_results;
    for (int i = 0; i < result_count; i++) {
        const uft_probe_handler_t* h = &g_probe_handlers[scores[i].handler_idx];
        memset(&results[i], 0, sizeof(results[i]));
        results[i].format = h->format;
        results[i].confidence = scores[i].confidence;
        results[i].format_class = h->format_class;
        calculate_confidence(h, data, size, &results[i]);
        results[i].variant = find_variant(h, data, size);
    }
    
    return result_count;
}

uft_error_t uft_probe_handle_unknown(const uft_probe_result_t* result,
                                      uft_unknown_action_t action,
                                      uft_format_t* chosen) {
    if (!result || !chosen) return UFT_ERROR_NULL_POINTER;
    
    switch (action) {
        case UFT_UNKNOWN_REJECT:
            *chosen = UFT_FORMAT_UNKNOWN;
            return UFT_ERROR_INVALID_FORMAT;
            
        case UFT_UNKNOWN_BEST_GUESS:
            if (result->confidence >= 30) {
                *chosen = result->format;
                return UFT_OK;
            }
            if (result->alternative_count > 0) {
                *chosen = result->alternatives[0];
                return UFT_OK;
            }
            return UFT_ERROR_INVALID_FORMAT;
            
        case UFT_UNKNOWN_ASK_USER:
            *chosen = UFT_FORMAT_UNKNOWN;
            return (uft_error_t)100;  // Special code for "ask user"
            
        case UFT_UNKNOWN_RAW:
            *chosen = UFT_FORMAT_IMG;  // Treat as raw
            return UFT_OK;
    }
    
    return UFT_ERROR_INVALID_ARG;
}

const uft_probe_handler_t* uft_probe_get_handler(uft_format_t format) {
    for (size_t i = 0; i < NUM_PROBE_HANDLERS; i++) {
        if (g_probe_handlers[i].format == format) {
            return &g_probe_handlers[i];
        }
    }
    return NULL;
}
