/**
 * @file fuzz_scp.c
 * @brief Fuzz target for SCP parser
 * 
 * Build: clang -g -O1 -fsanitize=fuzzer,address,undefined fuzz_scp.c -o fuzz_scp
 * Run:   ./fuzz_scp corpus/scp/
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// Minimal SCP header structure for fuzzing
#pragma pack(push, 1)
typedef struct {
    char magic[3];          // "SCP"
    uint8_t version;
    uint8_t disk_type;
    uint8_t num_revolutions;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t flags;
    uint8_t bit_cell_width;
    uint8_t heads;
    uint8_t resolution;
    uint32_t checksum;
} scp_header_t;
#pragma pack(pop)

// Security limits
#define SCP_MAX_TRACKS 168
#define SCP_MAX_REVOLUTIONS 32
#define SCP_MAX_TRACK_SIZE (2 * 1024 * 1024)

// Simulated parser entry point
int parse_scp_buffer(const uint8_t* data, size_t size) {
    // Minimum size check
    if (size < sizeof(scp_header_t)) {
        return -1;  // Too small
    }
    
    const scp_header_t* hdr = (const scp_header_t*)data;
    
    // Magic check
    if (memcmp(hdr->magic, "SCP", 3) != 0) {
        return -2;  // Bad magic
    }
    
    // Security checks
    if (hdr->num_revolutions > SCP_MAX_REVOLUTIONS) {
        return -3;  // Too many revolutions
    }
    
    if (hdr->end_track > SCP_MAX_TRACKS) {
        return -4;  // Track out of range
    }
    
    if (hdr->start_track > hdr->end_track) {
        return -5;  // Invalid track range
    }
    
    // Check track offsets would be in bounds
    size_t header_end = sizeof(scp_header_t) + 
                        (size_t)(hdr->end_track + 1) * 4;
    if (header_end > size) {
        return -6;  // Track offset table would overflow
    }
    
    // Parse track offsets (safely)
    const uint8_t* offset_table = data + sizeof(scp_header_t);
    for (int t = hdr->start_track; t <= hdr->end_track; t++) {
        size_t table_offset = (size_t)t * 4;
        if (table_offset + 4 > size - sizeof(scp_header_t)) {
            return -7;  // Offset table entry out of bounds
        }
        
        uint32_t track_offset;
        memcpy(&track_offset, offset_table + table_offset, 4);
        
        // Validate track offset
        if (track_offset != 0 && track_offset >= size) {
            return -8;  // Track offset points outside file
        }
    }
    
    return 0;  // Success
}

// libFuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Call parser - should never crash regardless of input
    parse_scp_buffer(data, size);
    return 0;
}
