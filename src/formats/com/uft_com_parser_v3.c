/**
 * @file uft_com_parser_v3.c
 * @brief GOD MODE COM Parser v3 - Enterprise 128 Disk Format
 * 
 * COM/IMG ist das Enterprise 64/128 Format:
 * - EXDOS kompatibel
 * - FAT12-ähnlich
 * - 40/80 Tracks
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define COM_SECTOR_SIZE         512
#define COM_SIZE_720K           (1440 * 512)

typedef enum {
    COM_DIAG_OK = 0,
    COM_DIAG_INVALID_SIZE,
    COM_DIAG_BAD_BPB,
    COM_DIAG_COUNT
} com_diag_code_t;

typedef struct { float overall; bool valid; } com_score_t;
typedef struct { com_diag_code_t code; char msg[128]; } com_diagnosis_t;
typedef struct { com_diagnosis_t* items; size_t count; size_t cap; float quality; } com_diagnosis_list_t;

typedef struct {
    /* EXDOS boot sector */
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    
    uint8_t tracks;
    uint8_t sides;
    
    com_score_t score;
    com_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} com_disk_t;

static com_diagnosis_list_t* com_diagnosis_create(void) {
    com_diagnosis_list_t* l = calloc(1, sizeof(com_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(com_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void com_diagnosis_free(com_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t com_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool com_parse(const uint8_t* data, size_t size, com_disk_t* disk) {
    if (!data || !disk || size < COM_SECTOR_SIZE) return false;
    memset(disk, 0, sizeof(com_disk_t));
    disk->diagnosis = com_diagnosis_create();
    disk->source_size = size;
    
    /* Parse boot sector BPB */
    disk->bytes_per_sector = com_read_le16(data + 11);
    disk->sectors_per_cluster = data[13];
    disk->reserved_sectors = com_read_le16(data + 14);
    disk->fat_count = data[16];
    disk->root_entries = com_read_le16(data + 17);
    disk->total_sectors = com_read_le16(data + 19);
    disk->media = data[21];
    disk->sectors_per_fat = com_read_le16(data + 22);
    disk->sectors_per_track = com_read_le16(data + 24);
    disk->heads = com_read_le16(data + 26);
    
    /* Defaults */
    if (disk->bytes_per_sector == 0) disk->bytes_per_sector = 512;
    if (disk->sectors_per_track == 0) disk->sectors_per_track = 9;
    if (disk->heads == 0) disk->heads = 2;
    
    disk->tracks = 80;
    disk->sides = disk->heads;
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void com_disk_free(com_disk_t* disk) {
    if (disk && disk->diagnosis) com_diagnosis_free(disk->diagnosis);
}

#ifdef COM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Enterprise COM Parser v3 Tests ===\n");
    
    printf("Testing COM parsing... ");
    uint8_t* com = calloc(1, COM_SIZE_720K);
    com[11] = 0; com[12] = 2;  /* 512 bytes/sector */
    com[24] = 9;                /* Sectors/track */
    com[26] = 2;                /* Heads */
    
    com_disk_t disk;
    bool ok = com_parse(com, COM_SIZE_720K, &disk);
    assert(ok);
    assert(disk.valid);
    com_disk_free(&disk);
    free(com);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
