/**
 * @file uft_cqm_parser_v3.c
 * @brief GOD MODE CQM Parser v3 - CopyQM Compressed Disk Image
 * 
 * CQM ist das Format von CopyQM (Sydex):
 * - RLE Kompression
 * - Umfangreiche Metadaten
 * - Variable Sektorgrößen
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CQM_SIGNATURE           "CQ"
#define CQM_HEADER_SIZE         133

typedef enum {
    CQM_DIAG_OK = 0,
    CQM_DIAG_BAD_SIGNATURE,
    CQM_DIAG_BAD_HEADER,
    CQM_DIAG_RLE_ERROR,
    CQM_DIAG_TRUNCATED,
    CQM_DIAG_COUNT
} cqm_diag_code_t;

typedef struct { float overall; bool valid; bool compressed; } cqm_score_t;
typedef struct { cqm_diag_code_t code; char msg[128]; } cqm_diagnosis_t;
typedef struct { cqm_diagnosis_t* items; size_t count; size_t cap; float quality; } cqm_diagnosis_list_t;

typedef struct {
    uint16_t sector_size;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media_byte;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t drive_type;
    uint8_t tracks;
    uint8_t blind;
    uint8_t density;
    uint8_t used_tracks;
    uint8_t total_tracks;
    uint32_t crc;
    char volume_label[12];
    char timestamp[13];
    char comment[81];
    
    bool is_compressed;
    
    cqm_score_t score;
    cqm_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} cqm_disk_t;

static cqm_diagnosis_list_t* cqm_diagnosis_create(void) {
    cqm_diagnosis_list_t* l = calloc(1, sizeof(cqm_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(cqm_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void cqm_diagnosis_free(cqm_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t cqm_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t cqm_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static bool cqm_parse(const uint8_t* data, size_t size, cqm_disk_t* disk) {
    if (!data || !disk || size < CQM_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(cqm_disk_t));
    disk->diagnosis = cqm_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (data[0] != 'C' || data[1] != 'Q') {
        return false;
    }
    
    /* Parse header */
    disk->sector_size = cqm_read_le16(data + 3);
    disk->sectors_per_cluster = data[5];
    disk->reserved_sectors = cqm_read_le16(data + 6);
    disk->fats = data[8];
    disk->root_entries = cqm_read_le16(data + 9);
    disk->total_sectors = cqm_read_le16(data + 11);
    disk->media_byte = data[13];
    disk->sectors_per_fat = cqm_read_le16(data + 14);
    disk->sectors_per_track = cqm_read_le16(data + 16);
    disk->heads = cqm_read_le16(data + 18);
    disk->hidden_sectors = cqm_read_le32(data + 20);
    disk->total_sectors_32 = cqm_read_le32(data + 24);
    disk->drive_type = data[106];
    disk->tracks = data[107];
    disk->blind = data[109];
    disk->density = data[110];
    disk->used_tracks = data[111];
    disk->total_tracks = data[112];
    disk->crc = cqm_read_le32(data + 113);
    
    memcpy(disk->volume_label, data + 117, 11);
    disk->volume_label[11] = '\0';
    
    memcpy(disk->timestamp, data + 128, 12);
    disk->timestamp[12] = '\0';
    
    /* Comment starts at offset 133 */
    if (size > CQM_HEADER_SIZE) {
        size_t comment_len = size - CQM_HEADER_SIZE;
        if (comment_len > 80) comment_len = 80;
        memcpy(disk->comment, data + CQM_HEADER_SIZE, comment_len);
        disk->comment[comment_len] = '\0';
    }
    
    disk->is_compressed = (data[2] == 0x14);  /* Compression flag */
    
    disk->score.compressed = disk->is_compressed;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void cqm_disk_free(cqm_disk_t* disk) {
    if (disk && disk->diagnosis) cqm_diagnosis_free(disk->diagnosis);
}

#ifdef CQM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CQM Parser v3 Tests ===\n");
    
    printf("Testing CQM parsing... ");
    uint8_t cqm[256];
    memset(cqm, 0, sizeof(cqm));
    cqm[0] = 'C'; cqm[1] = 'Q';
    cqm[3] = 0x00; cqm[4] = 0x02;  /* 512 sector size */
    cqm[16] = 18; cqm[17] = 0;    /* 18 sectors/track */
    cqm[18] = 2; cqm[19] = 0;     /* 2 heads */
    cqm[107] = 80;                /* 80 tracks */
    
    cqm_disk_t disk;
    bool ok = cqm_parse(cqm, sizeof(cqm), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.sector_size == 512);
    assert(disk.tracks == 80);
    cqm_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
