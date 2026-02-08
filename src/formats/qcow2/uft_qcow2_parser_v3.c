/**
 * @file uft_qcow2_parser_v3.c
 * @brief GOD MODE QCOW2 Parser v3 - QEMU Copy-on-Write
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define QCOW2_MAGIC             0x514649FB

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t backing_file_offset;
    uint32_t backing_file_size;
    uint32_t cluster_bits;
    uint64_t size;
    uint32_t crypt_method;
    uint32_t l1_size;
    uint64_t l1_table_offset;
    size_t source_size;
    bool valid;
} qcow2_file_t;

static uint32_t qcow2_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static uint64_t qcow2_read_be64(const uint8_t* p) {
    return ((uint64_t)qcow2_read_be32(p) << 32) | qcow2_read_be32(p + 4);
}

static bool qcow2_parse(const uint8_t* data, size_t size, qcow2_file_t* qcow2) {
    if (!data || !qcow2 || size < 72) return false;
    memset(qcow2, 0, sizeof(qcow2_file_t));
    qcow2->source_size = size;
    
    qcow2->magic = qcow2_read_be32(data);
    if (qcow2->magic == QCOW2_MAGIC) {
        qcow2->version = qcow2_read_be32(data + 4);
        qcow2->backing_file_offset = qcow2_read_be64(data + 8);
        qcow2->backing_file_size = qcow2_read_be32(data + 16);
        qcow2->cluster_bits = qcow2_read_be32(data + 20);
        qcow2->size = qcow2_read_be64(data + 24);
        qcow2->valid = (qcow2->version >= 2);
    }
    return true;
}

#ifdef QCOW2_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== QCOW2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t qcow2[72] = {0x51, 0x46, 0x49, 0xFB, 0, 0, 0, 3};  /* v3 */
    qcow2_file_t file;
    assert(qcow2_parse(qcow2, sizeof(qcow2), &file) && file.version == 3);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
