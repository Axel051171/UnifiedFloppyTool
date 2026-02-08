/**
 * @file uft_nsfe_parser_v3.c
 * @brief GOD MODE NSFE Parser v3 - Extended NES Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NSFE_MAGIC              "NSFE"

typedef struct {
    char signature[5];
    uint32_t info_chunk_size;
    uint8_t song_count;
    uint8_t starting_song;
    uint16_t load_address;
    uint16_t init_address;
    uint16_t play_address;
    char title[64];
    char artist[64];
    char copyright[64];
    size_t source_size;
    bool valid;
} nsfe_file_t;

static uint32_t nsfe_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool nsfe_parse(const uint8_t* data, size_t size, nsfe_file_t* nsfe) {
    if (!data || !nsfe || size < 8) return false;
    memset(nsfe, 0, sizeof(nsfe_file_t));
    nsfe->source_size = size;
    
    if (memcmp(data, NSFE_MAGIC, 4) == 0) {
        memcpy(nsfe->signature, data, 4);
        nsfe->signature[4] = '\0';
        
        /* Parse chunks */
        size_t offset = 4;
        while (offset + 8 < size) {
            uint32_t chunk_size = nsfe_read_le32(data + offset);
            const char* chunk_id = (const char*)(data + offset + 4);
            offset += 8;
            
            if (memcmp(chunk_id, "INFO", 4) == 0 && chunk_size >= 9) {
                nsfe->load_address = data[offset] | (data[offset+1] << 8);
                nsfe->init_address = data[offset+2] | (data[offset+3] << 8);
                nsfe->play_address = data[offset+4] | (data[offset+5] << 8);
                nsfe->song_count = data[offset+8];
            }
            offset += chunk_size;
            if (memcmp(chunk_id, "NEND", 4) == 0) break;
        }
        nsfe->valid = true;
    }
    return true;
}

#ifdef NSFE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== NSFe Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nsfe[32] = {'N', 'S', 'F', 'E', 9, 0, 0, 0, 'I', 'N', 'F', 'O'};
    nsfe[20] = 10;  /* 10 songs */
    nsfe_file_t file;
    assert(nsfe_parse(nsfe, sizeof(nsfe), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
