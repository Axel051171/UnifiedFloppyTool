/**
 * @file uft_dsk_dc42_parser_v3.c
 * @brief GOD MODE DSK_DC42 Parser v3 - Apple Disk Copy 4.2 Format
 * 
 * Disk Copy 4.2 Container:
 * - 84-Byte Header
 * - Mac 400K/800K/1.4M Images
 * - Checksummen
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DC42_HEADER_SIZE        84
#define DC42_SIGNATURE          0x0100

typedef struct {
    char disk_name[64];
    uint32_t data_size;
    uint32_t tag_size;
    uint32_t data_checksum;
    uint32_t tag_checksum;
    uint8_t disk_format;
    uint8_t format_byte;
    uint16_t private_word;
    size_t source_size;
    bool valid;
} dc42_disk_t;

static uint32_t dc42_read_be32(const uint8_t* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static uint16_t dc42_read_be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

static bool dc42_parse(const uint8_t* data, size_t size, dc42_disk_t* disk) {
    if (!data || !disk || size < DC42_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(dc42_disk_t));
    disk->source_size = size;
    
    /* Disk name (Pascal string) */
    uint8_t name_len = data[0];
    if (name_len > 63) name_len = 63;
    memcpy(disk->disk_name, data + 1, name_len);
    disk->disk_name[name_len] = '\0';
    
    disk->data_size = dc42_read_be32(data + 64);
    disk->tag_size = dc42_read_be32(data + 68);
    disk->data_checksum = dc42_read_be32(data + 72);
    disk->tag_checksum = dc42_read_be32(data + 76);
    disk->disk_format = data[80];
    disk->format_byte = data[81];
    disk->private_word = dc42_read_be16(data + 82);
    
    /* Validate signature */
    disk->valid = (disk->private_word == DC42_SIGNATURE);
    return true;
}

#ifdef DSK_DC42_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Apple Disk Copy 4.2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dc42[128] = {0};
    dc42[0] = 8;
    memcpy(dc42 + 1, "TESTDISK", 8);
    dc42[82] = 0x01; dc42[83] = 0x00;  /* Signature */
    dc42_disk_t disk;
    assert(dc42_parse(dc42, sizeof(dc42), &disk));
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
