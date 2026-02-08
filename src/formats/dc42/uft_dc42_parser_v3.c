/**
 * @file uft_dc42_parser_v3.c
 * @brief GOD MODE DC42 Parser v3 - Apple DiskCopy 4.2
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DC42_MAGIC              0x0100

typedef struct {
    char disk_name[64];
    uint32_t data_size;
    uint32_t tag_size;
    uint32_t data_checksum;
    uint32_t tag_checksum;
    uint8_t disk_format;
    uint8_t format_byte;
    uint16_t magic;
    size_t source_size;
    bool valid;
} dc42_file_t;

static uint32_t dc42_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool dc42_parse(const uint8_t* data, size_t size, dc42_file_t* dc42) {
    if (!data || !dc42 || size < 84) return false;
    memset(dc42, 0, sizeof(dc42_file_t));
    dc42->source_size = size;
    
    /* Name is Pascal string at offset 0 */
    uint8_t name_len = data[0];
    if (name_len > 63) name_len = 63;
    memcpy(dc42->disk_name, data + 1, name_len);
    dc42->disk_name[name_len] = '\0';
    
    dc42->data_size = dc42_read_be32(data + 64);
    dc42->tag_size = dc42_read_be32(data + 68);
    dc42->data_checksum = dc42_read_be32(data + 72);
    dc42->tag_checksum = dc42_read_be32(data + 76);
    dc42->disk_format = data[80];
    dc42->format_byte = data[81];
    dc42->magic = (data[82] << 8) | data[83];
    
    dc42->valid = (dc42->magic == DC42_MAGIC);
    return true;
}

#ifdef DC42_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DiskCopy 4.2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dc42[128] = {0};
    dc42[0] = 8;
    memcpy(dc42 + 1, "TestDisk", 8);
    dc42[82] = 0x01; dc42[83] = 0x00;  /* Magic */
    dc42_file_t file;
    assert(dc42_parse(dc42, sizeof(dc42), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
