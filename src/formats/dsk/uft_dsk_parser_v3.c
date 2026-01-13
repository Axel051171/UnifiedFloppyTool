/**
 * @file uft_dsk_parser_v3.c
 * @brief GOD MODE DSK Parser v3 - Amstrad CPC Extended DSK
 * 
 * DSK (CPCEMU) ist das Amstrad CPC Format:
 * - Standard DSK und Extended DSK Support
 * - Variable Sektor-Größen
 * - Kopierschutz-Support
 * - FDC Status Bytes
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DSK_SIGNATURE           "MV - CPC"
#define EDSK_SIGNATURE          "EXTENDED"
#define DSK_SIGNATURE_LEN       8
#define DSK_HEADER_SIZE         256
#define DSK_TRACK_HEADER_SIZE   256
#define DSK_MAX_TRACKS          84
#define DSK_MAX_SECTORS         29

typedef enum {
    DSK_DIAG_OK = 0,
    DSK_DIAG_BAD_SIGNATURE,
    DSK_DIAG_TRUNCATED,
    DSK_DIAG_BAD_TRACK_INFO,
    DSK_DIAG_SECTOR_SIZE_ERROR,
    DSK_DIAG_FDC_ERROR,
    DSK_DIAG_WEAK_SECTOR,
    DSK_DIAG_DELETED_DATA,
    DSK_DIAG_COUNT
} dsk_diag_code_t;

typedef struct dsk_score { float overall; bool valid; bool has_fdc_errors; bool has_weak; } dsk_score_t;
typedef struct dsk_diagnosis { dsk_diag_code_t code; uint8_t track; uint8_t sector; char msg[128]; } dsk_diagnosis_t;
typedef struct dsk_diagnosis_list { dsk_diagnosis_t* items; size_t count; size_t cap; float quality; } dsk_diagnosis_list_t;

typedef struct dsk_sector {
    uint8_t track;
    uint8_t side;
    uint8_t sector_id;
    uint8_t sector_size;    /* N: size = 128 << N */
    uint8_t fdc_status1;
    uint8_t fdc_status2;
    uint16_t actual_size;   /* Extended DSK only */
    uint8_t* data;
    bool present;
    bool has_error;
    bool is_deleted;
    bool is_weak;
} dsk_sector_t;

typedef struct dsk_track {
    uint8_t track_num;
    uint8_t side;
    uint8_t sector_count;
    uint8_t sector_size;
    uint8_t gap3;
    uint8_t filler;
    dsk_sector_t sectors[DSK_MAX_SECTORS];
    dsk_score_t score;
} dsk_track_t;

typedef struct dsk_disk {
    char signature[16];
    char creator[16];
    uint8_t track_count;
    uint8_t side_count;
    uint16_t track_size;
    bool is_extended;
    
    /* Extended DSK track sizes */
    uint8_t track_sizes[DSK_MAX_TRACKS * 2];
    
    dsk_track_t tracks[DSK_MAX_TRACKS * 2];
    uint8_t actual_tracks;
    
    bool has_fdc_errors;
    bool has_weak_sectors;
    bool has_deleted;
    
    dsk_score_t score;
    dsk_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} dsk_disk_t;

static dsk_diagnosis_list_t* dsk_diagnosis_create(void) {
    dsk_diagnosis_list_t* l = calloc(1, sizeof(dsk_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(dsk_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void dsk_diagnosis_free(dsk_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t dsk_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

bool dsk_parse(const uint8_t* data, size_t size, dsk_disk_t* disk) {
    if (!data || !disk || size < DSK_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(dsk_disk_t));
    disk->diagnosis = dsk_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, DSK_SIGNATURE, DSK_SIGNATURE_LEN) == 0) {
        disk->is_extended = false;
    } else if (memcmp(data, EDSK_SIGNATURE, DSK_SIGNATURE_LEN) == 0) {
        disk->is_extended = true;
    } else {
        return false;
    }
    
    memcpy(disk->signature, data, 15);
    disk->signature[15] = '\0';
    memcpy(disk->creator, data + 34, 14);
    disk->creator[14] = '\0';
    
    disk->track_count = data[48];
    disk->side_count = data[49];
    disk->track_size = dsk_read_le16(data + 50);
    
    /* Extended DSK: track size table */
    if (disk->is_extended) {
        memcpy(disk->track_sizes, data + 52, 
               (disk->track_count * disk->side_count < 204) ? 
               disk->track_count * disk->side_count : 204);
    }
    
    /* Parse tracks */
    size_t pos = DSK_HEADER_SIZE;
    uint8_t track_idx = 0;
    
    for (uint8_t t = 0; t < disk->track_count && pos < size; t++) {
        for (uint8_t s = 0; s < disk->side_count && pos < size; s++) {
            /* Check for track info block */
            if (pos + DSK_TRACK_HEADER_SIZE > size) break;
            if (memcmp(data + pos, "Track-Info\r\n", 12) != 0) {
                pos += disk->is_extended ? disk->track_sizes[track_idx] * 256 : disk->track_size;
                track_idx++;
                continue;
            }
            
            dsk_track_t* track = &disk->tracks[track_idx];
            track->track_num = data[pos + 16];
            track->side = data[pos + 17];
            track->sector_size = data[pos + 20];
            track->sector_count = data[pos + 21];
            track->gap3 = data[pos + 22];
            track->filler = data[pos + 23];
            
            /* Parse sector info */
            const uint8_t* sinfo = data + pos + 24;
            for (uint8_t sec = 0; sec < track->sector_count && sec < DSK_MAX_SECTORS; sec++) {
                dsk_sector_t* sector = &track->sectors[sec];
                sector->track = sinfo[sec * 8];
                sector->side = sinfo[sec * 8 + 1];
                sector->sector_id = sinfo[sec * 8 + 2];
                sector->sector_size = sinfo[sec * 8 + 3];
                sector->fdc_status1 = sinfo[sec * 8 + 4];
                sector->fdc_status2 = sinfo[sec * 8 + 5];
                
                if (disk->is_extended) {
                    sector->actual_size = sinfo[sec * 8 + 6] | (sinfo[sec * 8 + 7] << 8);
                } else {
                    sector->actual_size = 128 << sector->sector_size;
                }
                
                sector->present = true;
                sector->has_error = (sector->fdc_status1 != 0 || sector->fdc_status2 != 0);
                sector->is_deleted = (sector->fdc_status2 & 0x40) != 0;
                
                if (sector->has_error) disk->has_fdc_errors = true;
                if (sector->is_deleted) disk->has_deleted = true;
            }
            
            track->score.overall = 1.0f;
            track->score.valid = true;
            
            pos += disk->is_extended ? disk->track_sizes[track_idx] * 256 : disk->track_size;
            track_idx++;
            disk->actual_tracks++;
        }
    }
    
    disk->score.overall = disk->actual_tracks > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->actual_tracks > 0;
    disk->score.has_fdc_errors = disk->has_fdc_errors;
    disk->score.has_weak = disk->has_weak_sectors;
    disk->valid = true;
    return true;
}

void dsk_disk_free(dsk_disk_t* disk) {
    if (!disk) return;
    for (int t = 0; t < DSK_MAX_TRACKS * 2; t++) {
        for (int s = 0; s < DSK_MAX_SECTORS; s++) {
            if (disk->tracks[t].sectors[s].data) {
                free(disk->tracks[t].sectors[s].data);
            }
        }
    }
    if (disk->diagnosis) dsk_diagnosis_free(disk->diagnosis);
}

#ifdef DSK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DSK Parser v3 Tests ===\n");
    
    printf("Testing standard DSK... ");
    uint8_t dsk[512];
    memset(dsk, 0, sizeof(dsk));
    memcpy(dsk, "MV - CPCEMU Disk-File\r\nDisk-Info\r\n", 34);
    memcpy(dsk + 34, "UFT-V3", 6);
    dsk[48] = 40;  /* tracks */
    dsk[49] = 1;   /* sides */
    dsk[50] = 0x00; dsk[51] = 0x13;  /* track size 0x1300 */
    
    dsk_disk_t disk;
    bool ok = dsk_parse(dsk, sizeof(dsk), &disk);
    assert(ok);
    assert(disk.valid);
    assert(!disk.is_extended);
    assert(disk.track_count == 40);
    dsk_disk_free(&disk);
    printf("✓\n");
    
    printf("Testing extended DSK... ");
    memset(dsk, 0, sizeof(dsk));
    memcpy(dsk, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34);
    dsk[48] = 42;
    dsk[49] = 2;
    
    ok = dsk_parse(dsk, sizeof(dsk), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.is_extended);
    dsk_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
