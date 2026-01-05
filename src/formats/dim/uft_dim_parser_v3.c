/**
 * @file uft_dim_parser_v3.c
 * @brief GOD MODE DIM Parser v3 - Sharp X68000 Disk Image
 * 
 * DIM ist das Format für Sharp X68000:
 * - 256-Byte Header
 * - Variable Geometrie
 * - Unterstützt 2HD/2DD
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DIM_HEADER_SIZE         256
#define DIM_SECTOR_SIZE         1024    /* X68000 uses 1024-byte sectors */

/* Media types */
#define DIM_MEDIA_2HD           0x00
#define DIM_MEDIA_2HS           0x01
#define DIM_MEDIA_2HC           0x02
#define DIM_MEDIA_2HDE          0x03
#define DIM_MEDIA_2HQ           0x09
#define DIM_MEDIA_2DD_8         0x11
#define DIM_MEDIA_2DD_9         0x19

typedef enum {
    DIM_DIAG_OK = 0,
    DIM_DIAG_BAD_HEADER,
    DIM_DIAG_BAD_MEDIA,
    DIM_DIAG_TRUNCATED,
    DIM_DIAG_COUNT
} dim_diag_code_t;

typedef struct { float overall; bool valid; uint8_t media_type; } dim_score_t;
typedef struct { dim_diag_code_t code; char msg[128]; } dim_diagnosis_t;
typedef struct { dim_diagnosis_t* items; size_t count; size_t cap; float quality; } dim_diagnosis_list_t;

typedef struct {
    uint8_t media_type;
    uint8_t tracks;
    uint8_t heads;
    uint16_t sectors_per_track;
    uint16_t sector_size;
    uint8_t overtrack;
    
    uint32_t data_size;
    
    dim_score_t score;
    dim_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} dim_disk_t;

static dim_diagnosis_list_t* dim_diagnosis_create(void) {
    dim_diagnosis_list_t* l = calloc(1, sizeof(dim_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(dim_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void dim_diagnosis_free(dim_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static const char* dim_media_name(uint8_t m) {
    switch (m) {
        case DIM_MEDIA_2HD: return "2HD (1.2M)";
        case DIM_MEDIA_2HS: return "2HS";
        case DIM_MEDIA_2HC: return "2HC (1.2M)";
        case DIM_MEDIA_2HDE: return "2HDE";
        case DIM_MEDIA_2HQ: return "2HQ (1.44M)";
        case DIM_MEDIA_2DD_8: return "2DD 8-sector";
        case DIM_MEDIA_2DD_9: return "2DD 9-sector";
        default: return "Unknown";
    }
}

static void dim_get_geometry(uint8_t media, uint8_t* tracks, uint8_t* heads, 
                             uint16_t* spt, uint16_t* sector_size) {
    switch (media) {
        case DIM_MEDIA_2HD:
        case DIM_MEDIA_2HC:
            *tracks = 77; *heads = 2; *spt = 8; *sector_size = 1024;
            break;
        case DIM_MEDIA_2HQ:
            *tracks = 80; *heads = 2; *spt = 18; *sector_size = 512;
            break;
        case DIM_MEDIA_2DD_8:
            *tracks = 80; *heads = 2; *spt = 8; *sector_size = 512;
            break;
        case DIM_MEDIA_2DD_9:
            *tracks = 80; *heads = 2; *spt = 9; *sector_size = 512;
            break;
        default:
            *tracks = 77; *heads = 2; *spt = 8; *sector_size = 1024;
            break;
    }
}

static bool dim_parse(const uint8_t* data, size_t size, dim_disk_t* disk) {
    if (!data || !disk || size < DIM_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(dim_disk_t));
    disk->diagnosis = dim_diagnosis_create();
    disk->source_size = size;
    
    /* Parse header */
    disk->media_type = data[0];
    
    /* Get geometry from media type */
    dim_get_geometry(disk->media_type, &disk->tracks, &disk->heads,
                     &disk->sectors_per_track, &disk->sector_size);
    
    /* Overtrack info at offset 0xAB */
    disk->overtrack = data[0xAB];
    
    disk->data_size = size - DIM_HEADER_SIZE;
    
    /* Validate */
    uint32_t expected = disk->tracks * disk->heads * disk->sectors_per_track * disk->sector_size;
    if (disk->data_size < expected) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    disk->score.media_type = disk->media_type;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void dim_disk_free(dim_disk_t* disk) {
    if (disk && disk->diagnosis) dim_diagnosis_free(disk->diagnosis);
}

#ifdef DIM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DIM Parser v3 Tests ===\n");
    
    printf("Testing media names... ");
    assert(strcmp(dim_media_name(DIM_MEDIA_2HD), "2HD (1.2M)") == 0);
    assert(strcmp(dim_media_name(DIM_MEDIA_2HQ), "2HQ (1.44M)") == 0);
    printf("✓\n");
    
    printf("Testing DIM parsing... ");
    uint8_t* dim = calloc(1, DIM_HEADER_SIZE + 1024);
    dim[0] = DIM_MEDIA_2HD;
    
    dim_disk_t disk;
    bool ok = dim_parse(dim, DIM_HEADER_SIZE + 1024, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 77);
    assert(disk.sector_size == 1024);
    dim_disk_free(&disk);
    free(dim);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
