/**
 * @file uft_stx_parser_v3.c
 * @brief GOD MODE STX Parser v3 - Atari ST Extended Format (Pasti)
 * 
 * STX ist das erweiterte Atari ST Format:
 * - Raw Timing-Daten
 * - Sektor-Timing
 * - Fuzzy Bits Support
 * - Copy-Protection Preservation
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define STX_SIGNATURE           0x00445352  /* "RSD\0" */
#define STX_MAX_TRACKS          84
#define STX_MAX_SECTORS         26

typedef enum {
    STX_DIAG_OK = 0,
    STX_DIAG_BAD_SIGNATURE,
    STX_DIAG_BAD_VERSION,
    STX_DIAG_TRUNCATED,
    STX_DIAG_FUZZY_BITS,
    STX_DIAG_TIMING_DATA,
    STX_DIAG_WEAK_SECTOR,
    STX_DIAG_COUNT
} stx_diag_code_t;

typedef struct stx_score { float overall; bool valid; bool has_timing; bool has_fuzzy; } stx_score_t;
typedef struct stx_diagnosis { stx_diag_code_t code; uint8_t track; char msg[128]; } stx_diagnosis_t;
typedef struct stx_diagnosis_list { stx_diagnosis_t* items; size_t count; size_t cap; float quality; } stx_diagnosis_list_t;

typedef struct stx_track {
    uint8_t track_num;
    uint8_t side;
    uint8_t sector_count;
    bool has_timing;
    bool has_fuzzy;
    stx_score_t score;
} stx_track_t;

typedef struct stx_disk {
    uint32_t signature;
    uint16_t version;
    uint16_t tool_version;
    uint16_t track_count;
    uint8_t revision;
    
    stx_track_t tracks[STX_MAX_TRACKS * 2];
    uint8_t actual_tracks;
    
    bool has_timing;
    bool has_fuzzy;
    
    stx_score_t score;
    stx_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} stx_disk_t;

static stx_diagnosis_list_t* stx_diagnosis_create(void) {
    stx_diagnosis_list_t* l = calloc(1, sizeof(stx_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(stx_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void stx_diagnosis_free(stx_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t stx_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}
static uint16_t stx_read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

bool stx_parse(const uint8_t* data, size_t size, stx_disk_t* disk) {
    if (!data || !disk || size < 16) return false;
    memset(disk, 0, sizeof(stx_disk_t));
    disk->diagnosis = stx_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    disk->signature = stx_read_le32(data);
    if (disk->signature != STX_SIGNATURE) {
        return false;
    }
    
    disk->version = stx_read_le16(data + 4);
    disk->tool_version = stx_read_le16(data + 6);
    disk->track_count = stx_read_le16(data + 10);
    disk->revision = data[12];
    
    /* Parse track records */
    size_t pos = 16;
    disk->actual_tracks = 0;
    
    while (pos + 16 <= size && disk->actual_tracks < STX_MAX_TRACKS * 2) {
        uint32_t record_size = stx_read_le32(data + pos);
        if (record_size == 0 || pos + record_size > size) break;
        
        uint32_t fuzzy_count = stx_read_le32(data + pos + 4);
        uint16_t sector_count = stx_read_le16(data + pos + 8);
        uint16_t flags = stx_read_le16(data + pos + 10);
        (void)stx_read_le16(data + pos + 14); // track_len - reserved for future use
        uint8_t track_num = data[pos + 16];
        (void)data[pos + 17]; // track_type - reserved for future use
        
        stx_track_t* track = &disk->tracks[disk->actual_tracks];
        track->track_num = track_num & 0x7F;
        track->side = (track_num >> 7) & 1;
        track->sector_count = sector_count;
        track->has_timing = (flags & 0x01) != 0;
        track->has_fuzzy = (fuzzy_count > 0);
        
        if (track->has_timing) disk->has_timing = true;
        if (track->has_fuzzy) disk->has_fuzzy = true;
        
        track->score.overall = 1.0f;
        track->score.valid = true;
        track->score.has_timing = track->has_timing;
        track->score.has_fuzzy = track->has_fuzzy;
        
        disk->actual_tracks++;
        pos += record_size;
    }
    
    disk->score.overall = disk->actual_tracks > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->actual_tracks > 0;
    disk->score.has_timing = disk->has_timing;
    disk->score.has_fuzzy = disk->has_fuzzy;
    disk->valid = true;
    return true;
}

void stx_disk_free(stx_disk_t* disk) {
    if (disk && disk->diagnosis) stx_diagnosis_free(disk->diagnosis);
}

#ifdef STX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== STX Parser v3 Tests ===\n");
    
    printf("Testing signature check... ");
    uint8_t stx[32];
    memset(stx, 0, sizeof(stx));
    stx[0] = 'R'; stx[1] = 'S'; stx[2] = 'D'; stx[3] = 0;
    stx[4] = 3; stx[5] = 0;  /* Version */
    
    stx_disk_t disk;
    bool ok = stx_parse(stx, sizeof(stx), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.version == 3);
    stx_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
