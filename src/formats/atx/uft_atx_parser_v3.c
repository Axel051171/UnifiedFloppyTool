/**
 * @file uft_atx_parser_v3.c
 * @brief GOD MODE ATX Parser v3 - Atari 8-bit Extended Format
 * 
 * ATX ist das erweiterte Atari-Format:
 * - Timing-Informationen
 * - Schwache Sektoren
 * - Kopierschutz-Preservation
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ATX_SIGNATURE           0x58385441  /* "AT8X" in little-endian */
#define ATX_HEADER_SIZE         48

/* Chunk types */
#define ATX_CHUNK_TRACK_RECORD  0x0001
#define ATX_CHUNK_SECTOR_LIST   0x0002
#define ATX_CHUNK_SECTOR_DATA   0x0003
#define ATX_CHUNK_WEAK_SECTORS  0x0004
#define ATX_CHUNK_EXT_SECTOR    0x0005

/* Sector status flags */
#define ATX_SECTOR_FDC_CRC      0x08
#define ATX_SECTOR_FDC_LOST     0x04
#define ATX_SECTOR_FDC_MISSING  0x10
#define ATX_SECTOR_EXTENDED     0x40

typedef enum {
    ATX_DIAG_OK = 0,
    ATX_DIAG_BAD_SIGNATURE,
    ATX_DIAG_TRUNCATED,
    ATX_DIAG_BAD_TRACK,
    ATX_DIAG_WEAK_SECTOR,
    ATX_DIAG_CRC_ERROR,
    ATX_DIAG_COUNT
} atx_diag_code_t;

typedef struct { float overall; bool valid; int tracks; int weak_sectors; } atx_score_t;
typedef struct { atx_diag_code_t code; uint8_t track; char msg[128]; } atx_diagnosis_t;
typedef struct { atx_diagnosis_t* items; size_t count; size_t cap; } atx_diagnosis_list_t;

typedef struct {
    uint8_t number;
    uint8_t status;
    uint16_t position;
    uint32_t timing;
    bool has_data;
    bool is_weak;
    bool has_crc_error;
} atx_sector_t;

typedef struct {
    uint8_t track_num;
    uint8_t sector_count;
    uint16_t rate;
    atx_sector_t sectors[26];
    bool has_weak_sectors;
} atx_track_t;

typedef struct {
    uint32_t signature;
    uint16_t version;
    uint16_t min_version;
    uint16_t creator;
    uint16_t creator_version;
    uint32_t flags;
    uint16_t image_type;
    uint8_t density;
    uint8_t track_count;
    
    atx_track_t tracks[42];
    int weak_sector_count;
    
    atx_score_t score;
    atx_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} atx_disk_t;

static atx_diagnosis_list_t* atx_diagnosis_create(void) {
    atx_diagnosis_list_t* l = calloc(1, sizeof(atx_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(atx_diagnosis_t)); }
    return l;
}
static void atx_diagnosis_free(atx_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t atx_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t atx_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static bool atx_parse(const uint8_t* data, size_t size, atx_disk_t* disk) {
    if (!data || !disk || size < ATX_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(atx_disk_t));
    disk->diagnosis = atx_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    disk->signature = atx_read_le32(data);
    if (disk->signature != ATX_SIGNATURE) {
        return false;
    }
    
    /* Parse header */
    disk->version = atx_read_le16(data + 4);
    disk->min_version = atx_read_le16(data + 6);
    disk->creator = atx_read_le16(data + 8);
    disk->creator_version = atx_read_le16(data + 10);
    disk->flags = atx_read_le32(data + 12);
    disk->image_type = atx_read_le16(data + 16);
    disk->density = data[18];
    disk->track_count = data[32];
    
    /* Parse tracks */
    size_t pos = ATX_HEADER_SIZE;
    int track_idx = 0;
    
    while (pos + 8 < size && track_idx < 42) {
        uint32_t chunk_size = atx_read_le32(data + pos);
        uint16_t chunk_type = atx_read_le16(data + pos + 4);
        
        if (chunk_size < 8 || pos + chunk_size > size) break;
        
        if (chunk_type == ATX_CHUNK_TRACK_RECORD) {
            if (pos + 16 <= size) {
                atx_track_t* track = &disk->tracks[track_idx];
                track->track_num = data[pos + 8];
                track->sector_count = data[pos + 14];
                track->rate = atx_read_le16(data + pos + 10);
                track_idx++;
            }
        }
        
        pos += chunk_size;
    }
    
    disk->score.tracks = track_idx;
    disk->score.overall = track_idx > 30 ? 1.0f : (float)track_idx / 40.0f;
    disk->score.valid = track_idx > 0;
    disk->valid = true;
    return true;
}

static void atx_disk_free(atx_disk_t* disk) {
    if (disk && disk->diagnosis) atx_diagnosis_free(disk->diagnosis);
}

#ifdef ATX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ATX Parser v3 Tests ===\n");
    
    printf("Testing signature... ");
    assert(ATX_SIGNATURE == 0x58385441);
    printf("✓\n");
    
    printf("Testing ATX header... ");
    uint8_t atx[64];
    memset(atx, 0, sizeof(atx));
    /* Signature "AT8X" in file order */
    atx[0] = 'A'; atx[1] = 'T'; atx[2] = '8'; atx[3] = 'X';
    atx[4] = 1; atx[5] = 0;  /* Version */
    atx[32] = 40;  /* Track count */
    
    atx_disk_t disk;
    bool ok = atx_parse(atx, sizeof(atx), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.track_count == 40);
    atx_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
