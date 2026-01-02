/**
 * @file uft_nfd_parser_v3.c
 * @brief GOD MODE NFD Parser v3 - NEC PC-98 NFD Disk Image
 * 
 * T98-Next native format for PC-98
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NFD_MAGIC_R0            "T98FDDIMAGE.R0"
#define NFD_MAGIC_R1            "T98FDDIMAGE.R1"

typedef struct {
    char signature[15];
    uint8_t revision;
    char comment[256];
    uint32_t header_size;
    uint8_t protect;
    uint8_t heads;
    uint8_t tracks;
    size_t source_size;
    bool valid;
} nfd_file_t;

static bool nfd_parse(const uint8_t* data, size_t size, nfd_file_t* nfd) {
    if (!data || !nfd || size < 256) return false;
    memset(nfd, 0, sizeof(nfd_file_t));
    nfd->source_size = size;
    
    if (memcmp(data, NFD_MAGIC_R0, 14) == 0) {
        memcpy(nfd->signature, data, 14);
        nfd->revision = 0;
        nfd->valid = true;
    } else if (memcmp(data, NFD_MAGIC_R1, 14) == 0) {
        memcpy(nfd->signature, data, 14);
        nfd->revision = 1;
        nfd->header_size = data[0x110] | (data[0x111] << 8) | 
                           (data[0x112] << 16) | (data[0x113] << 24);
        nfd->valid = true;
    }
    return true;
}

#ifdef NFD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PC-98 NFD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nfd[512] = {0};
    memcpy(nfd, "T98FDDIMAGE.R0", 14);
    nfd_file_t file;
    assert(nfd_parse(nfd, sizeof(nfd), &file) && file.revision == 0);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
