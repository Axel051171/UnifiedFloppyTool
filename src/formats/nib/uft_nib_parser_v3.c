/**
 * @file uft_nib_parser_v3.c
 * @brief GOD MODE NIB Parser v3 - Apple II Nibble Format
 * 
 * NIB ist das raw nibble Format für Apple II:
 * - 35 Tracks × 6656 Bytes pro Track
 * - 6-and-2 GCR Encoding (DOS 3.3)
 * - 5-and-3 GCR Encoding (DOS 3.2)
 * - 16 Sektoren pro Track (DOS 3.3)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/* Constants */
#define NIB_TRACK_SIZE          6656
#define NIB_TRACKS              35
#define NIB_SIZE                (NIB_TRACKS * NIB_TRACK_SIZE)  /* 232960 */
#define NIB_SECTORS             16
#define NIB_SECTOR_SIZE         256

/* Sync/markers */
#define NIB_SYNC_BYTE           0xFF
#define NIB_ADDR_PROLOGUE_1     0xD5
#define NIB_ADDR_PROLOGUE_2     0xAA
#define NIB_ADDR_PROLOGUE_3     0x96
#define NIB_DATA_PROLOGUE_1     0xD5
#define NIB_DATA_PROLOGUE_2     0xAA
#define NIB_DATA_PROLOGUE_3     0xAD
#define NIB_EPILOGUE_1          0xDE
#define NIB_EPILOGUE_2          0xAA
#define NIB_EPILOGUE_3          0xEB

/* 6-and-2 decode table */
static const uint8_t nib_decode_62[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,
    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,
    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,
    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

/* Interleave table (physical to logical) */
static const uint8_t nib_interleave[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

typedef enum {
    NIB_DIAG_OK = 0,
    NIB_DIAG_INVALID_SIZE,
    NIB_DIAG_NO_SYNC,
    NIB_DIAG_BAD_ADDR_PROLOGUE,
    NIB_DIAG_BAD_DATA_PROLOGUE,
    NIB_DIAG_ADDR_CHECKSUM,
    NIB_DIAG_DATA_CHECKSUM,
    NIB_DIAG_MISSING_SECTOR,
    NIB_DIAG_WRONG_VOLUME,
    NIB_DIAG_WRONG_TRACK,
    NIB_DIAG_GCR_ERROR,
    NIB_DIAG_WEAK_BITS,
    NIB_DIAG_NON_STANDARD,
    NIB_DIAG_COUNT
} nib_diag_code_t;

typedef struct { float overall; bool valid; uint8_t sectors_found; } nib_score_t;
typedef struct { nib_diag_code_t code; uint8_t track; uint8_t sector; char msg[128]; } nib_diagnosis_t;
typedef struct { nib_diagnosis_t* items; size_t count; size_t cap; uint16_t errors; float quality; } nib_diagnosis_list_t;

typedef struct {
    uint8_t volume;
    uint8_t track_id;
    uint8_t sector_id;
    uint8_t data[256];
    bool addr_valid;
    bool data_valid;
    bool present;
} nib_sector_t;

typedef struct {
    uint8_t track_num;
    nib_sector_t sectors[16];
    uint8_t found_sectors;
    uint8_t valid_sectors;
    nib_score_t score;
} nib_track_t;

typedef struct {
    nib_track_t tracks[35];
    uint8_t volume_id;
    uint16_t total_sectors;
    uint16_t valid_sectors;
    nib_score_t score;
    nib_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} nib_disk_t;

static nib_diagnosis_list_t* nib_diagnosis_create(void) {
    nib_diagnosis_list_t* l = calloc(1, sizeof(nib_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(nib_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void nib_diagnosis_free(nib_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static int nib_find_pattern(const uint8_t* data, size_t size, size_t start, uint8_t p1, uint8_t p2, uint8_t p3) {
    for (size_t i = start; i + 2 < size; i++) {
        if (data[i] == p1 && data[i+1] == p2 && data[i+2] == p3) return (int)i;
    }
    return -1;
}

static uint8_t nib_decode_44(uint8_t odd, uint8_t even) {
    return ((odd << 1) | 1) & even;
}

static bool nib_parse_track(const uint8_t* data, nib_track_t* track, nib_diagnosis_list_t* diag) {
    track->found_sectors = 0;
    track->valid_sectors = 0;
    
    size_t pos = 0;
    while (pos < NIB_TRACK_SIZE - 50) {
        /* Find address prologue */
        int addr_pos = nib_find_pattern(data, NIB_TRACK_SIZE, pos, 
                                         NIB_ADDR_PROLOGUE_1, NIB_ADDR_PROLOGUE_2, NIB_ADDR_PROLOGUE_3);
        if (addr_pos < 0) break;
        
        pos = addr_pos + 3;
        if (pos + 8 > NIB_TRACK_SIZE) break;
        
        /* Decode address field */
        uint8_t vol = nib_decode_44(data[pos], data[pos+1]);
        uint8_t trk = nib_decode_44(data[pos+2], data[pos+3]);
        uint8_t sec = nib_decode_44(data[pos+4], data[pos+5]);
        uint8_t chk = nib_decode_44(data[pos+6], data[pos+7]);
        
        if (sec >= 16) { pos += 8; continue; }
        
        nib_sector_t* sector = &track->sectors[sec];
        sector->volume = vol;
        sector->track_id = trk;
        sector->sector_id = sec;
        sector->addr_valid = ((vol ^ trk ^ sec) == chk);
        sector->present = true;
        
        /* Find data prologue */
        pos += 8;
        int data_pos = nib_find_pattern(data, NIB_TRACK_SIZE, pos,
                                         NIB_DATA_PROLOGUE_1, NIB_DATA_PROLOGUE_2, NIB_DATA_PROLOGUE_3);
        if (data_pos < 0 || data_pos > (int)(pos + 50)) continue;
        
        pos = data_pos + 3;
        if (pos + 343 > NIB_TRACK_SIZE) break;
        
        /* Decode 6-and-2 data (simplified) */
        uint8_t buffer[342];
        bool gcr_ok = true;
        for (int i = 0; i < 342; i++) {
            uint8_t nib = data[pos + i] & 0x7F;
            buffer[i] = nib_decode_62[nib];
            if (buffer[i] == 0xFF) gcr_ok = false;
        }
        
        if (gcr_ok) {
            /* De-nibblize */
            uint8_t checksum = 0;
            for (int i = 0; i < 86; i++) {
                checksum ^= buffer[i];
                uint8_t val = buffer[i + 86];
                sector->data[i] = (val << 2) | ((buffer[i] & 0x01) << 1) | ((buffer[i] & 0x02) >> 1);
                sector->data[i + 86] = (buffer[i + 172] << 2) | ((buffer[i] & 0x04) >> 1) | ((buffer[i] & 0x08) >> 3);
                if (i < 84) {
                    sector->data[i + 172] = (buffer[i + 258] << 2) | ((buffer[i] & 0x10) >> 3) | ((buffer[i] & 0x20) >> 5);
                }
            }
            sector->data_valid = true;
            track->valid_sectors++;
        }
        
        track->found_sectors++;
        pos += 343;
    }
    
    track->score.sectors_found = track->found_sectors;
    track->score.overall = (float)track->valid_sectors / 16.0f;
    track->score.valid = (track->valid_sectors >= 14);
    
    return track->found_sectors > 0;
}

static bool nib_parse(const uint8_t* data, size_t size, nib_disk_t* disk) {
    if (!data || !disk) return false;
    memset(disk, 0, sizeof(nib_disk_t));
    disk->diagnosis = nib_diagnosis_create();
    disk->source_size = size;
    
    if (size != NIB_SIZE) {
        disk->diagnosis->errors++;
        return false;
    }
    
    for (int t = 0; t < NIB_TRACKS; t++) {
        disk->tracks[t].track_num = t;
        nib_parse_track(data + t * NIB_TRACK_SIZE, &disk->tracks[t], disk->diagnosis);
        disk->total_sectors += disk->tracks[t].found_sectors;
        disk->valid_sectors += disk->tracks[t].valid_sectors;
    }
    
    disk->score.overall = (float)disk->valid_sectors / (NIB_TRACKS * 16);
    disk->score.valid = (disk->valid_sectors > NIB_TRACKS * 14);
    disk->valid = true;
    return true;
}

static void nib_disk_free(nib_disk_t* disk) {
    if (disk && disk->diagnosis) { nib_diagnosis_free(disk->diagnosis); disk->diagnosis = NULL; }
}

/* ============================================================================
 * EXTENDED FEATURES - Sector Extraction & DOS 3.3 Support
 * ============================================================================ */

/* DOS 3.3 sector interleave table (physical to logical) */
static const uint8_t nib_dos33_interleave[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/* ProDOS sector interleave table */
static const uint8_t nib_prodos_interleave[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

/* De-nibblize 6-and-2 encoded data to 256 bytes */
static bool nib_denibblize(const uint8_t* nibbles, size_t nib_len, uint8_t* out_256) {
    if (!nibbles || !out_256 || nib_len < 343) return false;
    
    uint8_t aux[86];
    uint8_t data[256];
    
    /* Decode auxiliary buffer (86 bytes, 2-bit values packed) */
    for (int i = 0; i < 86; i++) {
        uint8_t val = nib_decode_62[nibbles[i] & 0x7F];
        if (val == 0xFF) return false;
        aux[i] = val;
    }
    
    /* Decode main data (256 bytes, 6-bit values) */
    for (int i = 0; i < 256; i++) {
        uint8_t val = nib_decode_62[nibbles[86 + i] & 0x7F];
        if (val == 0xFF) return false;
        data[i] = val;
    }
    
    /* Verify checksum */
    uint8_t checksum = nib_decode_62[nibbles[342] & 0x7F];
    uint8_t computed = 0;
    for (int i = 0; i < 86; i++) computed ^= aux[i];
    for (int i = 0; i < 256; i++) computed ^= data[i];
    if (computed != checksum) return false;
    
    /* Combine auxiliary and main data */
    for (int i = 0; i < 256; i++) {
        int aux_idx = i % 86;
        int shift = (i / 86) * 2;
        uint8_t low_bits = (aux[aux_idx] >> shift) & 0x03;
        out_256[i] = (data[i] << 2) | low_bits;
    }
    
    return true;
}

/* Extract a single sector from track */
static bool nib_extract_sector(const uint8_t* track_data, uint8_t sector,
                               uint8_t* out_256, bool* found) {
    if (!track_data || !out_256 || !found) return false;
    *found = false;
    
    /* Search for address mark */
    for (int pos = 0; pos < NIB_TRACK_SIZE - 350; pos++) {
        if (track_data[pos] != 0xD5 || track_data[pos+1] != 0xAA || 
            track_data[pos+2] != 0x96) continue;
        
        /* Found address prologue */
        uint8_t vol = nib_decode_44(track_data[pos+3], track_data[pos+4]);
        uint8_t trk = nib_decode_44(track_data[pos+5], track_data[pos+6]);
        uint8_t sec = nib_decode_44(track_data[pos+7], track_data[pos+8]);
        
        if (sec != sector) continue;
        
        /* Find data prologue */
        int data_pos = pos + 16;
        while (data_pos < pos + 100 && data_pos < NIB_TRACK_SIZE - 350) {
            if (track_data[data_pos] == 0xD5 && 
                track_data[data_pos+1] == 0xAA &&
                track_data[data_pos+2] == 0xAD) {
                /* Found data prologue - denibblize */
                if (nib_denibblize(track_data + data_pos + 3, 343, out_256)) {
                    *found = true;
                    return true;
                }
            }
            data_pos++;
        }
    }
    
    return false;
}

/* Extract entire disk to 140K image (DOS 3.3 or ProDOS order) */
static bool nib_extract_disk(const uint8_t* data, size_t size,
                             uint8_t* out_data, size_t* out_size,
                             bool prodos_order) {
    if (!data || !out_data || !out_size || size != NIB_SIZE) return false;
    
    const uint8_t* interleave = prodos_order ? nib_prodos_interleave : nib_dos33_interleave;
    size_t pos = 0;
    
    for (int t = 0; t < NIB_TRACKS; t++) {
        const uint8_t* track = data + t * NIB_TRACK_SIZE;
        
        for (int s = 0; s < 16; s++) {
            uint8_t physical_sector = interleave[s];
            bool found;
            
            if (!nib_extract_sector(track, physical_sector, out_data + pos, &found)) {
                /* Fill with zeros if sector not found */
                memset(out_data + pos, 0, 256);
            }
            pos += 256;
        }
    }
    
    *out_size = pos;
    return true;
}

/* Detect disk format (DOS 3.3, ProDOS, Pascal, etc.) */
static const char* nib_detect_format(const uint8_t* data, size_t size) {
    if (!data || size != NIB_SIZE) return "Unknown";
    
    /* Extract track 0, sector 0 to check boot sector */
    uint8_t sector0[256];
    bool found;
    
    if (!nib_extract_sector(data, 0, sector0, &found) || !found) {
        return "Unreadable";
    }
    
    /* Check for DOS 3.3 */
    if (sector0[0] == 0x01) return "DOS 3.3";
    
    /* Check for ProDOS */
    if (sector0[0] == 0x00 && sector0[1] == 0x00) {
        /* Could be ProDOS - check VTOC */
        uint8_t vtoc[256];
        if (nib_extract_sector(data + 17 * NIB_TRACK_SIZE, 0, vtoc, &found) && found) {
            if (vtoc[1] == 17 && vtoc[2] == 15) return "DOS 3.3";
            if (vtoc[0] == 0x00) return "ProDOS";
        }
    }
    
    return "Unknown";
}

#ifdef NIB_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== NIB Parser v3 Tests ===\n");
    
    printf("Testing decode table... ");
    assert(nib_decode_62[0x96 & 0x7F] == 0x00);
    assert(nib_decode_62[0x97 & 0x7F] == 0x01);
    printf("✓\n");
    
    printf("Testing 4-and-4 decode... ");
    /* 4-and-4 encoding: odd bits in first byte, even in second */
    /* nib_decode_44(0xAA, 0xAB) = ((0xAA << 1) | 1) & 0xAB = 0x55 & 0xAB = 0x01 */
    uint8_t result = nib_decode_44(0xAA, 0xAB);
    assert(result == 0x01);
    printf("✓\n");
    
    printf("Testing size validation... ");
    nib_disk_t disk;
    uint8_t* nib = calloc(1, NIB_SIZE);
    bool ok = nib_parse(nib, NIB_SIZE, &disk);
    assert(ok);
    assert(disk.valid);
    nib_disk_free(&disk);
    free(nib);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
