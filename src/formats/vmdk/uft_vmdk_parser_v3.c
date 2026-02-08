/**
 * @file uft_vmdk_parser_v3.c
 * @brief GOD MODE VMDK Parser v3 - VMware Virtual Disk
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VMDK_SPARSE_MAGIC       0x564D444B  /* "VMDK" */
#define VMDK_DESCRIPTOR         "# Disk DescriptorFile"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint64_t capacity;
    uint64_t grain_size;
    uint64_t descriptor_offset;
    uint64_t descriptor_size;
    bool is_sparse;
    bool is_descriptor;
    size_t source_size;
    bool valid;
} vmdk_file_t;

static uint32_t vmdk_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool vmdk_parse(const uint8_t* data, size_t size, vmdk_file_t* vmdk) {
    if (!data || !vmdk || size < 512) return false;
    memset(vmdk, 0, sizeof(vmdk_file_t));
    vmdk->source_size = size;
    
    vmdk->magic = vmdk_read_le32(data);
    if (vmdk->magic == VMDK_SPARSE_MAGIC || 
        (data[0] == 'K' && data[1] == 'D' && data[2] == 'M' && data[3] == 'V')) {
        vmdk->is_sparse = true;
        vmdk->version = vmdk_read_le32(data + 4);
        vmdk->flags = vmdk_read_le32(data + 8);
        vmdk->valid = true;
    } else if (strstr((const char*)data, VMDK_DESCRIPTOR) != NULL) {
        vmdk->is_descriptor = true;
        vmdk->valid = true;
    }
    return true;
}

#ifdef VMDK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== VMDK Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t vmdk[512] = {'K', 'D', 'M', 'V', 1, 0, 0, 0};  /* VMDK LE */
    vmdk_file_t file;
    assert(vmdk_parse(vmdk, sizeof(vmdk), &file) && file.is_sparse);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
