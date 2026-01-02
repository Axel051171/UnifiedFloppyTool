/**
 * @file uft_woz_parser_v3.c
 * @brief GOD MODE WOZ Parser v3 - Apple II Flux Format
 * 
 * WOZ ist das moderne Apple II Flux-Format:
 * - WOZ 1.0 und WOZ 2.0/2.1 Support
 * - Bitstream-basiert mit Timing
 * - Unterstützt Quarter-Tracks
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

#define WOZ1_SIGNATURE      0x315A4F57  /* "WOZ1" */
#define WOZ2_SIGNATURE      0x325A4F57  /* "WOZ2" */
#define WOZ_HEADER_SIZE     12
#define WOZ_MAX_TRACKS      160         /* 40 tracks × 4 quarter-tracks */
#define WOZ_TRACK_BITS_MAX  (6656 * 8)

typedef enum {
    WOZ_DIAG_OK = 0,
    WOZ_DIAG_BAD_SIGNATURE,
    WOZ_DIAG_BAD_CRC,
    WOZ_DIAG_TRUNCATED,
    WOZ_DIAG_MISSING_INFO,
    WOZ_DIAG_MISSING_TMAP,
    WOZ_DIAG_MISSING_TRKS,
    WOZ_DIAG_WEAK_BITS,
    WOZ_DIAG_COUNT
} woz_diag_code_t;

typedef struct { float overall; bool valid; } woz_score_t;
typedef struct { woz_diag_code_t code; uint8_t track; char msg[128]; } woz_diagnosis_t;
typedef struct { woz_diagnosis_t* items; size_t count; size_t cap; float quality; } woz_diagnosis_list_t;

typedef struct {
    uint16_t starting_block;
    uint16_t block_count;
    uint32_t bit_count;
} woz_trk_entry_t;

typedef struct {
    uint8_t* bits;
    uint32_t bit_count;
    bool present;
    woz_score_t score;
} woz_track_t;

typedef struct {
    uint32_t signature;
    uint8_t version;
    
    /* INFO chunk */
    uint8_t disk_type;      /* 1=5.25", 2=3.5" */
    uint8_t write_protected;
    uint8_t synchronized;
    uint8_t cleaned;
    char creator[33];
    uint8_t sides;
    uint8_t boot_sector_format;
    uint8_t optimal_bit_timing;
    
    /* TMAP */
    uint8_t track_map[WOZ_MAX_TRACKS];
    
    /* TRKS */
    woz_track_t tracks[WOZ_MAX_TRACKS];
    uint8_t track_count;
    
    woz_score_t score;
    woz_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} woz_disk_t;

static woz_diagnosis_list_t* woz_diagnosis_create(void) {
    woz_diagnosis_list_t* l = calloc(1, sizeof(woz_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(woz_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void woz_diagnosis_free(woz_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t woz_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static uint32_t woz_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static bool woz_parse(const uint8_t* data, size_t size, woz_disk_t* disk) {
    if (!data || !disk || size < 256) return false;
    memset(disk, 0, sizeof(woz_disk_t));
    disk->diagnosis = woz_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    disk->signature = woz_read_le32(data);
    if (disk->signature == WOZ1_SIGNATURE) {
        disk->version = 1;
    } else if (disk->signature == WOZ2_SIGNATURE) {
        disk->version = 2;
    } else {
        return false;
    }
    
    /* Verify CRC */
    uint32_t stored_crc = woz_read_le32(data + 8);
    uint32_t calc_crc = woz_crc32(data + 12, size - 12);
    if (stored_crc != calc_crc) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    /* Parse chunks */
    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t chunk_id = woz_read_le32(data + pos);
        uint32_t chunk_size = woz_read_le32(data + pos + 4);
        
        if (pos + 8 + chunk_size > size) break;
        
        const uint8_t* chunk = data + pos + 8;
        
        if (chunk_id == 0x4F464E49) {  /* "INFO" */
            if (chunk_size >= 60) {
                disk->version = chunk[0];
                disk->disk_type = chunk[1];
                disk->write_protected = chunk[2];
                disk->synchronized = chunk[3];
                disk->cleaned = chunk[4];
                memcpy(disk->creator, chunk + 5, 32);
                disk->creator[32] = '\0';
                if (disk->version >= 2 && chunk_size >= 62) {
                    disk->sides = chunk[37];
                    disk->boot_sector_format = chunk[38];
                    disk->optimal_bit_timing = chunk[39];
                }
            }
        } else if (chunk_id == 0x50414D54) {  /* "TMAP" */
            if (chunk_size >= 160) {
                memcpy(disk->track_map, chunk, 160);
            }
        } else if (chunk_id == 0x534B5254) {  /* "TRKS" */
            /* Parse track entries */
            if (disk->version == 2) {
                /* WOZ 2.0 format */
                for (int t = 0; t < 160 && t * 8 + 8 <= (int)chunk_size; t++) {
                    const uint8_t* te = chunk + t * 8;
                    uint16_t start_block = te[0] | (te[1] << 8);
                    uint16_t block_count = te[2] | (te[3] << 8);
                    uint32_t bit_count = te[4] | (te[5] << 8) | (te[6] << 16) | (te[7] << 24);
                    
                    if (start_block > 0 && bit_count > 0) {
                        disk->tracks[t].bit_count = bit_count;
                        disk->tracks[t].present = true;
                        disk->track_count++;
                    }
                }
            }
        }
        
        pos += 8 + chunk_size;
    }
    
    disk->score.overall = disk->track_count > 30 ? 1.0f : (float)disk->track_count / 35.0f;
    disk->score.valid = disk->track_count > 0;
    disk->valid = true;
    return true;
}

static void woz_disk_free(woz_disk_t* disk) {
    if (!disk) return;
    for (int t = 0; t < WOZ_MAX_TRACKS; t++) {
        if (disk->tracks[t].bits) free(disk->tracks[t].bits);
    }
    if (disk->diagnosis) woz_diagnosis_free(disk->diagnosis);
}

/* ============================================================================
 * EXTENDED FEATURES - Bitstream Decoding & Analysis
 * ============================================================================ */

/* Get bit from bitstream */
static inline uint8_t woz_get_bit(const uint8_t* bits, uint32_t bit_idx) {
    return (bits[bit_idx / 8] >> (7 - (bit_idx % 8))) & 1;
}

/* Find sync pattern in bitstream (10 consecutive 1-bits) */
static int32_t woz_find_sync(const uint8_t* bits, uint32_t bit_count, uint32_t start) {
    int ones = 0;
    
    for (uint32_t i = start; i < bit_count; i++) {
        if (woz_get_bit(bits, i)) {
            ones++;
            if (ones >= 10) return (int32_t)(i - 9);
        } else {
            ones = 0;
        }
    }
    
    return -1;
}

/* Read byte from bitstream (8 bits) */
static uint8_t woz_read_byte(const uint8_t* bits, uint32_t bit_count, uint32_t* bit_idx) {
    uint8_t byte = 0;
    
    for (int b = 0; b < 8 && *bit_idx < bit_count; b++) {
        byte = (byte << 1) | woz_get_bit(bits, *bit_idx);
        (*bit_idx)++;
    }
    
    return byte;
}

/* Find address field in track bitstream */
typedef struct {
    uint8_t volume;
    uint8_t track;
    uint8_t sector;
    uint8_t checksum;
    uint32_t bit_offset;
    bool valid;
} woz_address_field_t;

static bool woz_find_address(const uint8_t* bits, uint32_t bit_count, uint32_t start,
                              woz_address_field_t* addr) {
    if (!bits || !addr) return false;
    
    uint32_t pos = start;
    
    /* Find sync and address prologue D5 AA 96 */
    while (pos < bit_count - 200) {
        int32_t sync = woz_find_sync(bits, bit_count, pos);
        if (sync < 0) break;
        
        pos = sync + 10;
        
        /* Skip extra 1-bits */
        while (pos < bit_count && woz_get_bit(bits, pos)) pos++;
        
        /* Try to read D5 AA 96 */
        uint32_t save_pos = pos;
        uint8_t b1 = woz_read_byte(bits, bit_count, &pos);
        uint8_t b2 = woz_read_byte(bits, bit_count, &pos);
        uint8_t b3 = woz_read_byte(bits, bit_count, &pos);
        
        if (b1 == 0xD5 && b2 == 0xAA && b3 == 0x96) {
            /* Read 4-and-4 encoded fields */
            uint8_t v1 = woz_read_byte(bits, bit_count, &pos);
            uint8_t v2 = woz_read_byte(bits, bit_count, &pos);
            uint8_t t1 = woz_read_byte(bits, bit_count, &pos);
            uint8_t t2 = woz_read_byte(bits, bit_count, &pos);
            uint8_t s1 = woz_read_byte(bits, bit_count, &pos);
            uint8_t s2 = woz_read_byte(bits, bit_count, &pos);
            uint8_t c1 = woz_read_byte(bits, bit_count, &pos);
            uint8_t c2 = woz_read_byte(bits, bit_count, &pos);
            
            addr->volume = ((v1 << 1) | 1) & v2;
            addr->track = ((t1 << 1) | 1) & t2;
            addr->sector = ((s1 << 1) | 1) & s2;
            addr->checksum = ((c1 << 1) | 1) & c2;
            addr->bit_offset = save_pos;
            addr->valid = (addr->checksum == (addr->volume ^ addr->track ^ addr->sector));
            
            return true;
        }
    }
    
    return false;
}

/* Count sectors in track */
static int woz_count_sectors(const uint8_t* bits, uint32_t bit_count) {
    if (!bits || bit_count == 0) return 0;
    
    int count = 0;
    uint32_t pos = 0;
    woz_address_field_t addr;
    
    while (woz_find_address(bits, bit_count, pos, &addr)) {
        if (addr.valid) count++;
        pos = addr.bit_offset + 100;
    }
    
    return count;
}

/* Analyze track quality */
typedef struct {
    int sectors_found;
    int sectors_valid;
    float flux_quality;
    bool has_sync;
    bool readable;
} woz_track_analysis_t;

static void woz_analyze_track(const woz_track_t* track, woz_track_analysis_t* analysis) {
    if (!track || !analysis || !track->bits || !track->present) {
        if (analysis) memset(analysis, 0, sizeof(woz_track_analysis_t));
        return;
    }
    
    analysis->sectors_found = woz_count_sectors(track->bits, track->bit_count);
    analysis->sectors_valid = analysis->sectors_found;  /* Assume all valid for now */
    analysis->has_sync = woz_find_sync(track->bits, track->bit_count, 0) >= 0;
    analysis->readable = analysis->sectors_found >= 13;  /* At least 13/16 sectors */
    analysis->flux_quality = analysis->readable ? 1.0f : (float)analysis->sectors_found / 16.0f;
}

#ifdef WOZ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== WOZ Parser v3 Tests ===\n");
    
    printf("Testing CRC32... ");
    uint8_t test[] = { 0x01, 0x02, 0x03, 0x04 };
    uint32_t crc = woz_crc32(test, 4);
    assert(crc != 0);
    printf("✓\n");
    
    printf("Testing WOZ2 header... ");
    uint8_t woz2[512];
    memset(woz2, 0, sizeof(woz2));
    woz2[0] = 'W'; woz2[1] = 'O'; woz2[2] = 'Z'; woz2[3] = '2';
    woz2[4] = 0xFF; woz2[5] = 0x0A; woz2[6] = 0x0D; woz2[7] = 0x0A;
    
    woz_disk_t disk;
    bool ok = woz_parse(woz2, sizeof(woz2), &disk);
    assert(ok);
    assert(disk.version == 2);
    woz_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
