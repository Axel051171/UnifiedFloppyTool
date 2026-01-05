/**
 * @file uft_imd_parser_v3.c
 * @brief GOD MODE IMD Parser v3 - ImageDisk Format
 * 
 * IMD ist das Format von Dave Dunfield's ImageDisk:
 * - ASCII Header mit Kommentaren
 * - Sektor-Daten mit Mode-Bytes
 * - Unterstützt FM und MFM
 * - Kompression für identische Sektoren
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include "uft/uft_safe_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define IMD_SIGNATURE           "IMD "
#define IMD_MAX_TRACKS          160
#define IMD_MAX_SECTORS         64

/* Mode byte values */
#define IMD_MODE_500_FM         0x00
#define IMD_MODE_300_FM         0x01
#define IMD_MODE_250_FM         0x02
#define IMD_MODE_500_MFM        0x03
#define IMD_MODE_300_MFM        0x04
#define IMD_MODE_250_MFM        0x05

/* Sector data types */
#define IMD_DATA_UNAVAILABLE    0x00
#define IMD_DATA_NORMAL         0x01
#define IMD_DATA_COMPRESSED     0x02
#define IMD_DATA_DELETED        0x03
#define IMD_DATA_DEL_COMPRESSED 0x04
#define IMD_DATA_ERROR          0x05
#define IMD_DATA_ERR_COMPRESSED 0x06
#define IMD_DATA_DEL_ERROR      0x07
#define IMD_DATA_DEL_ERR_COMP   0x08

typedef enum {
    IMD_DIAG_OK = 0,
    IMD_DIAG_BAD_SIGNATURE,
    IMD_DIAG_NO_HEADER_END,
    IMD_DIAG_TRUNCATED,
    IMD_DIAG_BAD_MODE,
    IMD_DIAG_SECTOR_ERROR,
    IMD_DIAG_DELETED_DATA,
    IMD_DIAG_COUNT
} imd_diag_code_t;

typedef struct { float overall; bool valid; uint8_t tracks; uint8_t errors; } imd_score_t;
typedef struct { imd_diag_code_t code; uint8_t track; uint8_t sector; char msg[128]; } imd_diagnosis_t;
typedef struct { imd_diagnosis_t* items; size_t count; size_t cap; float quality; } imd_diagnosis_list_t;

typedef struct {
    uint8_t cylinder;
    uint8_t head;
    uint8_t id;
    uint8_t size_code;
    uint8_t data_type;
    uint16_t data_size;
    bool has_error;
    bool deleted;
} imd_sector_t;

typedef struct {
    uint8_t mode;
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector_count;
    uint8_t sector_size_code;
    uint16_t sector_size;
    imd_sector_t sectors[IMD_MAX_SECTORS];
    uint8_t valid_sectors;
    imd_score_t score;
} imd_track_t;

typedef struct {
    char comment[4096];
    size_t comment_len;
    
    imd_track_t tracks[IMD_MAX_TRACKS];
    uint8_t track_count;
    uint8_t cylinder_count;
    uint8_t head_count;
    
    bool has_fm;
    bool has_mfm;
    bool has_errors;
    bool has_deleted;
    
    imd_score_t score;
    imd_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} imd_disk_t;

static imd_diagnosis_list_t* imd_diagnosis_create(void) {
    imd_diagnosis_list_t* l = calloc(1, sizeof(imd_diagnosis_list_t));
    if (l) { l->cap = 128; l->items = calloc(l->cap, sizeof(imd_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void imd_diagnosis_free(imd_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t imd_sector_size(uint8_t code) {
    switch (code) {
        case 0: return 128;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        case 4: return 2048;
        case 5: return 4096;
        case 6: return 8192;
        default: return 512;
    }
}

static const char* imd_mode_str(uint8_t mode) {
    switch (mode) {
        case IMD_MODE_500_FM: return "500 kbps FM";
        case IMD_MODE_300_FM: return "300 kbps FM";
        case IMD_MODE_250_FM: return "250 kbps FM";
        case IMD_MODE_500_MFM: return "500 kbps MFM";
        case IMD_MODE_300_MFM: return "300 kbps MFM";
        case IMD_MODE_250_MFM: return "250 kbps MFM";
        default: return "Unknown";
    }
}

static bool imd_parse(const uint8_t* data, size_t size, imd_disk_t* disk) {
    if (!data || !disk || size < 32) return false;
    memset(disk, 0, sizeof(imd_disk_t));
    disk->diagnosis = imd_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, IMD_SIGNATURE, 4) != 0) {
        return false;
    }
    
    /* Find end of comment (0x1A) */
    size_t pos = 0;
    while (pos < size && data[pos] != 0x1A) {
        if (pos < sizeof(disk->comment) - 1) {
            disk->comment[pos] = data[pos];
        }
        pos++;
    }
    disk->comment_len = pos < sizeof(disk->comment) ? pos : sizeof(disk->comment) - 1;
    disk->comment[disk->comment_len] = '\0';
    
    if (pos >= size) return false;
    pos++;  /* Skip 0x1A */
    
    /* Parse tracks */
    uint8_t max_cyl = 0, max_head = 0;
    
    while (pos < size && disk->track_count < IMD_MAX_TRACKS) {
        if (pos + 5 > size) break;
        
        imd_track_t* track = &disk->tracks[disk->track_count];
        track->mode = data[pos++];
        track->cylinder = data[pos++];
        track->head = data[pos++] & 0x0F;
        track->sector_count = data[pos++];
        track->sector_size_code = data[pos++];
        track->sector_size = imd_sector_size(track->sector_size_code);
        
        if (track->cylinder > max_cyl) max_cyl = track->cylinder;
        if (track->head > max_head) max_head = track->head;
        
        if (track->mode <= IMD_MODE_250_FM) disk->has_fm = true;
        else if (track->mode <= IMD_MODE_250_MFM) disk->has_mfm = true;
        
        /* Skip sector numbering map */
        if (track->sector_count > 0) {
            if (pos + track->sector_count > size) break;
            pos += track->sector_count;
        }
        
        /* Skip cylinder map if present */
        if (track->head & 0x80) {
            if (pos + track->sector_count > size) break;
            pos += track->sector_count;
        }
        
        /* Skip head map if present */
        if (track->head & 0x40) {
            if (pos + track->sector_count > size) break;
            pos += track->sector_count;
        }
        
        /* Parse sector data */
        for (int s = 0; s < track->sector_count && pos < size; s++) {
            imd_sector_t* sector = &track->sectors[s];
            sector->data_type = data[pos++];
            sector->size_code = track->sector_size_code;
            sector->data_size = track->sector_size;
            
            switch (sector->data_type) {
                case IMD_DATA_UNAVAILABLE:
                    break;
                case IMD_DATA_NORMAL:
                case IMD_DATA_DELETED:
                case IMD_DATA_ERROR:
                case IMD_DATA_DEL_ERROR:
                    pos += track->sector_size;
                    break;
                case IMD_DATA_COMPRESSED:
                case IMD_DATA_DEL_COMPRESSED:
                case IMD_DATA_ERR_COMPRESSED:
                case IMD_DATA_DEL_ERR_COMP:
                    pos += 1;
                    break;
            }
            
            if (sector->data_type >= IMD_DATA_ERROR) {
                sector->has_error = true;
                disk->has_errors = true;
            }
            if (sector->data_type == IMD_DATA_DELETED || 
                sector->data_type == IMD_DATA_DEL_COMPRESSED ||
                sector->data_type == IMD_DATA_DEL_ERROR ||
                sector->data_type == IMD_DATA_DEL_ERR_COMP) {
                sector->deleted = true;
                disk->has_deleted = true;
            }
            
            if (sector->data_type != IMD_DATA_UNAVAILABLE) {
                track->valid_sectors++;
            }
        }
        
        track->score.valid = track->valid_sectors > 0;
        track->score.overall = (float)track->valid_sectors / track->sector_count;
        disk->track_count++;
    }
    
    disk->cylinder_count = max_cyl + 1;
    disk->head_count = max_head + 1;
    disk->score.tracks = disk->track_count;
    disk->score.overall = disk->track_count > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->track_count > 0;
    disk->valid = true;
    return true;
}

static void imd_disk_free(imd_disk_t* disk) {
    if (disk && disk->diagnosis) imd_diagnosis_free(disk->diagnosis);
}

/* ============================================================================
 * EXTENDED FEATURES - Sector Extraction & Analysis
 * ============================================================================ */

/* Get sector data from IMD */
static bool imd_get_sector(const uint8_t* data, size_t size,
                           uint8_t cylinder, uint8_t head, uint8_t sector,
                           uint8_t* out_data, size_t* out_size) {
    if (!data || !out_data || !out_size || size < 32) return false;
    
    /* Skip comment */
    size_t pos = 0;
    while (pos < size && data[pos] != 0x1A) pos++;
    if (pos >= size) return false;
    pos++;
    
    /* Search tracks */
    while (pos + 5 < size) {
        uint8_t mode = data[pos++];
        uint8_t cyl = data[pos++];
        uint8_t hd = data[pos++] & 0x0F;
        uint8_t sec_count = data[pos++];
        uint8_t sec_size_code = data[pos++];
        
        uint16_t sec_size = imd_sector_size(sec_size_code);
        
        if (cyl != cylinder || hd != head) {
            /* Skip this track */
            /* Skip sector numbering map */
            pos += sec_count;
            
            /* Skip cylinder map if present */
            if (data[pos - sec_count - 3] & 0x80) pos += sec_count;
            /* Skip head map if present */
            if (data[pos - sec_count - 3] & 0x40) pos += sec_count;
            
            /* Skip sector data */
            for (int s = 0; s < sec_count && pos < size; s++) {
                uint8_t dtype = data[pos++];
                switch (dtype) {
                    case IMD_DATA_NORMAL:
                    case IMD_DATA_DELETED:
                    case IMD_DATA_ERROR:
                    case IMD_DATA_DEL_ERROR:
                        pos += sec_size;
                        break;
                    case IMD_DATA_COMPRESSED:
                    case IMD_DATA_DEL_COMPRESSED:
                    case IMD_DATA_ERR_COMPRESSED:
                    case IMD_DATA_DEL_ERR_COMP:
                        pos += 1;
                        break;
                }
            }
            continue;
        }
        
        /* Found the track - now find the sector */
        const uint8_t* sector_map = data + pos;
        pos += sec_count;
        
        /* Skip cylinder/head maps */
        if (mode & 0x80) pos += sec_count;
        if (mode & 0x40) pos += sec_count;
        
        /* Find sector in map */
        int sec_idx = -1;
        for (int s = 0; s < sec_count; s++) {
            if (sector_map[s] == sector) {
                sec_idx = s;
                break;
            }
        }
        
        if (sec_idx < 0) return false;
        
        /* Skip to the sector data */
        for (int s = 0; s < sec_idx && pos < size; s++) {
            uint8_t dtype = data[pos++];
            switch (dtype) {
                case IMD_DATA_NORMAL:
                case IMD_DATA_DELETED:
                case IMD_DATA_ERROR:
                case IMD_DATA_DEL_ERROR:
                    pos += sec_size;
                    break;
                case IMD_DATA_COMPRESSED:
                case IMD_DATA_DEL_COMPRESSED:
                case IMD_DATA_ERR_COMPRESSED:
                case IMD_DATA_DEL_ERR_COMP:
                    pos += 1;
                    break;
            }
        }
        
        if (pos >= size) return false;
        
        /* Read the sector */
        uint8_t dtype = data[pos++];
        if (dtype == IMD_DATA_UNAVAILABLE) return false;
        
        if (dtype == IMD_DATA_COMPRESSED || dtype == IMD_DATA_DEL_COMPRESSED ||
            dtype == IMD_DATA_ERR_COMPRESSED || dtype == IMD_DATA_DEL_ERR_COMP) {
            /* Compressed - fill with single byte */
            memset(out_data, data[pos], sec_size);
        } else {
            /* Normal data */
            if (pos + sec_size > size) return false;
            memcpy(out_data, data + pos, sec_size);
        }
        
        *out_size = sec_size;
        return true;
    }
    
    return false;
}

/* Calculate IMD disk statistics */
typedef struct {
    int total_sectors;
    int valid_sectors;
    int compressed_sectors;
    int deleted_sectors;
    int error_sectors;
    size_t total_data_size;
    size_t compressed_data_size;
    float compression_ratio;
} imd_stats_t;

static void imd_calculate_stats(const imd_disk_t* disk, imd_stats_t* stats) {
    if (!disk || !stats) return;
    memset(stats, 0, sizeof(imd_stats_t));
    
    for (int t = 0; t < disk->track_count; t++) {
        const imd_track_t* track = &disk->tracks[t];
        
        for (int s = 0; s < track->sector_count; s++) {
            const imd_sector_t* sec = &track->sectors[s];
            
            stats->total_sectors++;
            stats->total_data_size += sec->data_size;
            
            if (sec->data_type != IMD_DATA_UNAVAILABLE) {
                stats->valid_sectors++;
            }
            if (sec->has_error) {
                stats->error_sectors++;
            }
            if (sec->deleted) {
                stats->deleted_sectors++;
            }
            
            switch (sec->data_type) {
                case IMD_DATA_COMPRESSED:
                case IMD_DATA_DEL_COMPRESSED:
                case IMD_DATA_ERR_COMPRESSED:
                case IMD_DATA_DEL_ERR_COMP:
                    stats->compressed_sectors++;
                    stats->compressed_data_size += 1;
                    break;
                default:
                    stats->compressed_data_size += sec->data_size;
                    break;
            }
        }
    }
    
    if (stats->total_data_size > 0) {
        stats->compression_ratio = (float)stats->compressed_data_size / 
                                   (float)stats->total_data_size;
    }
}

#ifdef IMD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== IMD Parser v3 Tests ===\n");
    
    printf("Testing sector sizes... ");
    assert(imd_sector_size(0) == 128);
    assert(imd_sector_size(1) == 256);
    assert(imd_sector_size(2) == 512);
    assert(imd_sector_size(3) == 1024);
    printf("✓\n");
    
    printf("Testing mode strings... ");
    assert(strcmp(imd_mode_str(IMD_MODE_250_MFM), "250 kbps MFM") == 0);
    assert(strcmp(imd_mode_str(IMD_MODE_500_FM), "500 kbps FM") == 0);
    printf("✓\n");
    
    printf("Testing IMD header... ");
    /* IMD format: "IMD x.xx: comment\x1A" then track data */
    uint8_t imd[64];
    memset(imd, 0, sizeof(imd));
    memcpy(imd, "IMD 1.18: Test", 14);
    imd[14] = 0x1A;  /* End of comment */
    imd[15] = IMD_MODE_250_MFM;  /* Mode */
    imd[16] = 0;   /* Cylinder */
    imd[17] = 0;   /* Head */
    imd[18] = 1;   /* Sectors */
    imd[19] = 2;   /* Size code (512) */
    imd[20] = 1;   /* Sector numbering map: sector 1 */
    imd[21] = IMD_DATA_COMPRESSED;  /* Data type */
    imd[22] = 0xE5;  /* Fill byte */
    
    imd_disk_t disk;
    bool ok = imd_parse(imd, sizeof(imd), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.track_count >= 1);
    assert(disk.has_mfm);
    imd_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
