/**
 * @file uft_d88_parser_v3.c
 * @brief GOD MODE D88 Parser v3 - PC-98/Sharp X1 Format
 * 
 * D88 ist das japanische Standard-Format für:
 * - NEC PC-98
 * - Sharp X1/X68000
 * - FM Towns
 * - MSX
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D88_HEADER_SIZE         0x2B0   /* 688 bytes */
#define D88_MAX_TRACKS          164     /* 82 tracks × 2 sides */
#define D88_NAME_SIZE           17

/* Media types */
#define D88_MEDIA_2D            0x00    /* 320K */
#define D88_MEDIA_2DD           0x10    /* 640K */
#define D88_MEDIA_2HD           0x20    /* 1.2M */
#define D88_MEDIA_1D            0x30    /* 160K */
#define D88_MEDIA_1DD           0x40    /* 320K */

/* Density flags */
#define D88_DENSITY_DOUBLE      0x00
#define D88_DENSITY_SINGLE      0x40
#define D88_DENSITY_HIGH        0x01

typedef enum {
    D88_DIAG_OK = 0,
    D88_DIAG_BAD_HEADER,
    D88_DIAG_TRUNCATED,
    D88_DIAG_BAD_TRACK,
    D88_DIAG_CRC_ERROR,
    D88_DIAG_COUNT
} d88_diag_code_t;

typedef struct { float overall; bool valid; uint8_t media_type; } d88_score_t;
typedef struct { d88_diag_code_t code; uint8_t track; char msg[128]; } d88_diagnosis_t;
typedef struct { d88_diagnosis_t* items; size_t count; size_t cap; float quality; } d88_diagnosis_list_t;

typedef struct {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    uint16_t sectors;
    uint8_t density;
    uint8_t deleted;
    uint8_t status;
    uint16_t data_size;
} d88_sector_t;

typedef struct {
    uint32_t offset;
    uint8_t sector_count;
    d88_sector_t sectors[64];
} d88_track_t;

typedef struct {
    char name[D88_NAME_SIZE];
    uint8_t reserved[9];
    uint8_t write_protect;
    uint8_t media_type;
    uint32_t disk_size;
    uint32_t track_offsets[D88_MAX_TRACKS];
    
    d88_track_t tracks[D88_MAX_TRACKS];
    uint8_t track_count;
    uint8_t max_cylinder;
    uint8_t max_head;
    
    d88_score_t score;
    d88_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} d88_disk_t;

static d88_diagnosis_list_t* d88_diagnosis_create(void) {
    d88_diagnosis_list_t* l = calloc(1, sizeof(d88_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(d88_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void d88_diagnosis_free(d88_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t d88_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t d88_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static const char* d88_media_name(uint8_t m) {
    switch (m) {
        case D88_MEDIA_2D: return "2D (320K)";
        case D88_MEDIA_2DD: return "2DD (640K)";
        case D88_MEDIA_2HD: return "2HD (1.2M)";
        case D88_MEDIA_1D: return "1D (160K)";
        case D88_MEDIA_1DD: return "1DD (320K)";
        default: return "Unknown";
    }
}

static bool d88_parse(const uint8_t* data, size_t size, d88_disk_t* disk) {
    if (!data || !disk || size < D88_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(d88_disk_t));
    disk->diagnosis = d88_diagnosis_create();
    disk->source_size = size;
    
    /* Parse header */
    memcpy(disk->name, data, D88_NAME_SIZE - 1);
    disk->name[D88_NAME_SIZE - 1] = '\0';
    
    disk->write_protect = data[0x1A];
    disk->media_type = data[0x1B];
    disk->disk_size = d88_read_le32(data + 0x1C);
    
    /* Read track offsets */
    for (int t = 0; t < D88_MAX_TRACKS; t++) {
        disk->track_offsets[t] = d88_read_le32(data + 0x20 + t * 4);
    }
    
    /* Parse tracks */
    disk->track_count = 0;
    disk->max_cylinder = 0;
    disk->max_head = 0;
    
    for (int t = 0; t < D88_MAX_TRACKS; t++) {
        if (disk->track_offsets[t] == 0) continue;
        if (disk->track_offsets[t] >= size) continue;
        
        d88_track_t* track = &disk->tracks[t];
        track->offset = disk->track_offsets[t];
        
        size_t pos = track->offset;
        track->sector_count = 0;
        
        /* Parse sectors */
        while (pos + 16 <= size && track->sector_count < 64) {
            d88_sector_t* sector = &track->sectors[track->sector_count];
            
            sector->cylinder = data[pos];
            sector->head = data[pos + 1];
            sector->sector = data[pos + 2];
            sector->size_code = data[pos + 3];
            sector->sectors = d88_read_le16(data + pos + 4);
            sector->density = data[pos + 6];
            sector->deleted = data[pos + 7];
            sector->status = data[pos + 8];
            sector->data_size = d88_read_le16(data + pos + 14);
            
            if (sector->sectors == 0) break;
            
            if (sector->cylinder > disk->max_cylinder) 
                disk->max_cylinder = sector->cylinder;
            if (sector->head > disk->max_head)
                disk->max_head = sector->head;
            
            pos += 16 + sector->data_size;
            track->sector_count++;
            
            if (track->sector_count >= sector->sectors) break;
        }
        
        disk->track_count++;
    }
    
    disk->score.media_type = disk->media_type;
    disk->score.overall = disk->track_count > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->track_count > 0;
    disk->valid = true;
    
    return true;
}

static void d88_disk_free(d88_disk_t* disk) {
    if (disk && disk->diagnosis) d88_diagnosis_free(disk->diagnosis);
}

#ifdef D88_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== D88 Parser v3 Tests ===\n");
    
    printf("Testing media names... ");
    assert(strcmp(d88_media_name(D88_MEDIA_2D), "2D (320K)") == 0);
    assert(strcmp(d88_media_name(D88_MEDIA_2HD), "2HD (1.2M)") == 0);
    printf("✓\n");
    
    printf("Testing D88 parsing... ");
    uint8_t* d88 = calloc(1, D88_HEADER_SIZE + 256);
    memcpy(d88, "TEST DISK", 9);
    d88[0x1B] = D88_MEDIA_2DD;
    /* Set disk size */
    uint32_t dsize = D88_HEADER_SIZE + 256;
    d88[0x1C] = dsize & 0xFF;
    d88[0x1D] = (dsize >> 8) & 0xFF;
    
    d88_disk_t disk;
    bool ok = d88_parse(d88, D88_HEADER_SIZE + 256, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.media_type == D88_MEDIA_2DD);
    d88_disk_free(&disk);
    free(d88);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
