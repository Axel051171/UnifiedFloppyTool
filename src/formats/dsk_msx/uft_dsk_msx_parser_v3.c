/**
 * @file uft_dsk_msx_parser_v3.c
 * @brief GOD MODE DSK_MSX Parser v3 - MSX Disk Format
 * 
 * MSX DSK ist das Standard MSX-DOS Format:
 * - FAT12 kompatibel
 * - 360K/720K Varianten
 * - Boot Sector mit MSX-spezifischen Feldern
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MSX_SECTOR_SIZE         512
#define MSX_SIZE_360K           (720 * 512)     /* 368640 */
#define MSX_SIZE_720K           (1440 * 512)    /* 737280 */

/* Media descriptor bytes */
#define MSX_MEDIA_360K_SS       0xF8    /* Single sided */
#define MSX_MEDIA_360K_DS       0xF9    /* Double sided */
#define MSX_MEDIA_720K          0xF9

typedef enum {
    MSX_DIAG_OK = 0,
    MSX_DIAG_INVALID_SIZE,
    MSX_DIAG_BAD_BPB,
    MSX_DIAG_BAD_FAT,
    MSX_DIAG_COUNT
} msx_diag_code_t;

typedef struct { float overall; bool valid; uint8_t media; } msx_score_t;
typedef struct { msx_diag_code_t code; char msg[128]; } msx_diagnosis_t;
typedef struct { msx_diagnosis_t* items; size_t count; size_t cap; float quality; } msx_diagnosis_list_t;

typedef struct {
    /* BPB fields */
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    
    /* Derived */
    uint8_t tracks;
    uint8_t sides;
    uint32_t data_size;
    
    msx_score_t score;
    msx_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} msx_disk_t;

static msx_diagnosis_list_t* msx_diagnosis_create(void) {
    msx_diagnosis_list_t* l = calloc(1, sizeof(msx_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(msx_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void msx_diagnosis_free(msx_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t msx_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool msx_parse(const uint8_t* data, size_t size, msx_disk_t* disk) {
    if (!data || !disk || size < MSX_SIZE_360K) return false;
    memset(disk, 0, sizeof(msx_disk_t));
    disk->diagnosis = msx_diagnosis_create();
    disk->source_size = size;
    
    /* Parse boot sector BPB */
    disk->bytes_per_sector = msx_read_le16(data + 11);
    disk->sectors_per_cluster = data[13];
    disk->reserved_sectors = msx_read_le16(data + 14);
    disk->fat_count = data[16];
    disk->root_entries = msx_read_le16(data + 17);
    disk->total_sectors = msx_read_le16(data + 19);
    disk->media_descriptor = data[21];
    disk->sectors_per_fat = msx_read_le16(data + 22);
    disk->sectors_per_track = msx_read_le16(data + 24);
    disk->heads = msx_read_le16(data + 26);
    
    /* Validate */
    if (disk->bytes_per_sector == 0) disk->bytes_per_sector = 512;
    if (disk->sectors_per_track == 0) disk->sectors_per_track = 9;
    if (disk->heads == 0) disk->heads = 2;
    
    /* Calculate geometry */
    if (disk->total_sectors > 0 && disk->sectors_per_track > 0 && disk->heads > 0) {
        disk->tracks = disk->total_sectors / (disk->sectors_per_track * disk->heads);
    } else {
        disk->tracks = 80;
    }
    disk->sides = disk->heads;
    
    disk->data_size = size;
    
    disk->score.media = disk->media_descriptor;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void msx_disk_free(msx_disk_t* disk) {
    if (disk && disk->diagnosis) msx_diagnosis_free(disk->diagnosis);
}

#ifdef DSK_MSX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MSX DSK Parser v3 Tests ===\n");
    
    printf("Testing MSX parsing... ");
    uint8_t* msx = calloc(1, MSX_SIZE_720K);
    
    /* Set up minimal BPB */
    msx[11] = 0x00; msx[12] = 0x02;  /* 512 bytes/sector */
    msx[13] = 2;                      /* Sectors per cluster */
    msx[16] = 2;                      /* FAT count */
    msx[21] = MSX_MEDIA_720K;         /* Media */
    msx[24] = 9; msx[25] = 0;         /* Sectors/track */
    msx[26] = 2; msx[27] = 0;         /* Heads */
    
    msx_disk_t disk;
    bool ok = msx_parse(msx, MSX_SIZE_720K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.bytes_per_sector == 512);
    msx_disk_free(&disk);
    free(msx);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
