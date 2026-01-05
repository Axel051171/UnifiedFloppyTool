/**
 * @file uft_dmk_parser_v3.c
 * @brief GOD MODE DMK Parser v3 - TRS-80 Disk Format
 * 
 * DMK ist das raw-level TRS-80 Format:
 * - Variable Sektoren pro Track
 * - FM und MFM Support
 * - IDAM (ID Address Mark) Tabelle
 * - Raw Track Data
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DMK_HEADER_SIZE         16
#define DMK_MAX_TRACKS          96
#define DMK_MAX_SECTORS         64
#define DMK_IDAM_TABLE_SIZE     128
#define DMK_IDAM_ENTRIES        64

/* Header flags */
#define DMK_FLAG_SINGLE_SIDE    0x10
#define DMK_FLAG_SINGLE_DENSITY 0x40
#define DMK_FLAG_IGNORE_DENSITY 0x80

typedef enum {
    DMK_DIAG_OK = 0,
    DMK_DIAG_TRUNCATED,
    DMK_DIAG_BAD_TRACK_LEN,
    DMK_DIAG_NO_IDAM,
    DMK_DIAG_CRC_ERROR,
    DMK_DIAG_MISSING_SECTOR,
    DMK_DIAG_FM_DATA,
    DMK_DIAG_COUNT
} dmk_diag_code_t;

typedef struct { float overall; bool valid; uint8_t sectors; } dmk_score_t;
typedef struct { dmk_diag_code_t code; uint8_t track; char msg[128]; } dmk_diagnosis_t;
typedef struct { dmk_diagnosis_t* items; size_t count; size_t cap; float quality; } dmk_diagnosis_list_t;

typedef struct {
    uint16_t idam_offset;       /* Offset in track */
    bool is_double_density;
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    bool valid;
} dmk_idam_t;

typedef struct {
    uint8_t track_num;
    uint8_t side;
    dmk_idam_t idams[DMK_IDAM_ENTRIES];
    uint8_t idam_count;
    uint8_t* raw_data;
    uint16_t raw_size;
    dmk_score_t score;
} dmk_track_t;

typedef struct {
    uint8_t write_protect;
    uint8_t track_count;
    uint16_t track_length;
    uint8_t flags;
    
    bool single_sided;
    bool single_density;
    
    dmk_track_t tracks[DMK_MAX_TRACKS * 2];
    uint8_t actual_tracks;
    uint8_t sides;
    
    dmk_score_t score;
    dmk_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} dmk_disk_t;

static dmk_diagnosis_list_t* dmk_diagnosis_create(void) {
    dmk_diagnosis_list_t* l = calloc(1, sizeof(dmk_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(dmk_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void dmk_diagnosis_free(dmk_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t dmk_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool dmk_parse_track(const uint8_t* data, dmk_track_t* track, uint16_t track_len) {
    if (!data || !track || track_len < DMK_IDAM_TABLE_SIZE) return false;
    
    /* Parse IDAM table */
    track->idam_count = 0;
    for (int i = 0; i < DMK_IDAM_ENTRIES; i++) {
        uint16_t entry = dmk_read_le16(data + i * 2);
        if (entry == 0) continue;
        
        dmk_idam_t* idam = &track->idams[track->idam_count];
        idam->idam_offset = entry & 0x3FFF;
        idam->is_double_density = (entry & 0x8000) != 0;
        
        /* Parse ID field if offset is valid */
        if (idam->idam_offset + 6 < track_len) {
            const uint8_t* id = data + DMK_IDAM_TABLE_SIZE + idam->idam_offset;
            idam->cylinder = id[1];
            idam->head = id[2];
            idam->sector = id[3];
            idam->size_code = id[4];
            idam->valid = true;
        }
        
        track->idam_count++;
    }
    
    track->score.sectors = track->idam_count;
    track->score.overall = track->idam_count > 0 ? 1.0f : 0.0f;
    track->score.valid = track->idam_count > 0;
    
    return true;
}

static bool dmk_parse(const uint8_t* data, size_t size, dmk_disk_t* disk) {
    if (!data || !disk || size < DMK_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(dmk_disk_t));
    disk->diagnosis = dmk_diagnosis_create();
    disk->source_size = size;
    
    /* Parse header */
    disk->write_protect = data[0];
    disk->track_count = data[1];
    disk->track_length = dmk_read_le16(data + 2);
    disk->flags = data[4];
    
    disk->single_sided = (disk->flags & DMK_FLAG_SINGLE_SIDE) != 0;
    disk->single_density = (disk->flags & DMK_FLAG_SINGLE_DENSITY) != 0;
    disk->sides = disk->single_sided ? 1 : 2;
    
    /* Validate */
    if (disk->track_length < DMK_IDAM_TABLE_SIZE || disk->track_length > 0x4000) {
        return false;
    }
    
    /* Calculate expected size */
    size_t expected = DMK_HEADER_SIZE + disk->track_count * disk->sides * disk->track_length;
    if (size < expected) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    /* Parse tracks */
    size_t pos = DMK_HEADER_SIZE;
    disk->actual_tracks = 0;
    
    for (int t = 0; t < disk->track_count && pos + disk->track_length <= size; t++) {
        for (int s = 0; s < disk->sides && pos + disk->track_length <= size; s++) {
            dmk_track_t* track = &disk->tracks[disk->actual_tracks];
            track->track_num = t;
            track->side = s;
            track->raw_size = disk->track_length;
            
            dmk_parse_track(data + pos, track, disk->track_length);
            
            disk->actual_tracks++;
            pos += disk->track_length;
        }
    }
    
    disk->score.overall = disk->actual_tracks > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->actual_tracks > 0;
    disk->valid = true;
    return true;
}

static void dmk_disk_free(dmk_disk_t* disk) {
    if (!disk) return;
    for (int t = 0; t < DMK_MAX_TRACKS * 2; t++) {
        if (disk->tracks[t].raw_data) free(disk->tracks[t].raw_data);
    }
    if (disk->diagnosis) dmk_diagnosis_free(disk->diagnosis);
}

#ifdef DMK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DMK Parser v3 Tests ===\n");
    
    printf("Testing DMK header... ");
    uint8_t dmk[512];
    memset(dmk, 0, sizeof(dmk));
    dmk[0] = 0;     /* Write protect */
    dmk[1] = 40;    /* Track count */
    dmk[2] = 0x90;  /* Track length low (0x1990 = 6544) */
    dmk[3] = 0x19;  /* Track length high */
    dmk[4] = 0;     /* Flags */
    
    dmk_disk_t disk;
    bool ok = dmk_parse(dmk, sizeof(dmk), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.track_count == 40);
    assert(disk.track_length == 0x1990);
    assert(disk.sides == 2);
    dmk_disk_free(&disk);
    printf("✓\n");
    
    printf("Testing single-sided... ");
    dmk[4] = DMK_FLAG_SINGLE_SIDE;
    ok = dmk_parse(dmk, sizeof(dmk), &disk);
    assert(ok);
    assert(disk.single_sided);
    assert(disk.sides == 1);
    dmk_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
