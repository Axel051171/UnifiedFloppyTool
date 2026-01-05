/**
 * @file uft_tar_parser_v3.c
 * @brief GOD MODE TAR Parser v3 - Tape Archive
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TAR_USTAR_MAGIC         "ustar"
#define TAR_BLOCK_SIZE          512

typedef struct {
    char first_filename[101];
    uint32_t first_file_size;
    bool is_ustar;
    bool is_gnu;
    uint32_t file_count;
    size_t source_size;
    bool valid;
} tar_file_t;

static uint32_t tar_octal_to_int(const uint8_t* p, size_t len) {
    uint32_t val = 0;
    for (size_t i = 0; i < len && p[i] >= '0' && p[i] <= '7'; i++) {
        val = (val << 3) | (p[i] - '0');
    }
    return val;
}

static bool tar_parse(const uint8_t* data, size_t size, tar_file_t* tar) {
    if (!data || !tar || size < TAR_BLOCK_SIZE) return false;
    memset(tar, 0, sizeof(tar_file_t));
    tar->source_size = size;
    
    /* Check for ustar magic at offset 257 */
    if (memcmp(data + 257, TAR_USTAR_MAGIC, 5) == 0) {
        tar->is_ustar = true;
    }
    
    /* First entry */
    if (data[0] != 0) {
        memcpy(tar->first_filename, data, 100);
        tar->first_filename[100] = '\0';
        tar->first_file_size = tar_octal_to_int(data + 124, 11);
        tar->file_count = 1;
        tar->valid = true;
    }
    
    return true;
}

#ifdef TAR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TAR Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t tar[512] = {0};
    memcpy(tar, "test.txt", 8);
    memcpy(tar + 257, "ustar", 5);
    tar_file_t file;
    assert(tar_parse(tar, sizeof(tar), &file) && file.is_ustar);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
