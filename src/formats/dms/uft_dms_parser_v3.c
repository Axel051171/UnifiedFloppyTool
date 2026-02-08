/**
 * @file uft_dms_parser_v3.c
 * @brief GOD MODE DMS Parser v3 - Amiga Disk Masher System
 * 
 * DMS ist das komprimierte Amiga-Format:
 * - Multiple Kompressionsarten (NONE, SIMPLE, QUICK, MEDIUM, DEEP, HEAVY)
 * - Track-basierte Kompression
 * - Optional verschlüsselt
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DMS_SIGNATURE           "DMS!"
#define DMS_HEADER_SIZE         56
#define DMS_TRACK_HEADER_SIZE   20

/* Compression modes */
#define DMS_COMP_NONE           0
#define DMS_COMP_SIMPLE         1
#define DMS_COMP_QUICK          2
#define DMS_COMP_MEDIUM         3
#define DMS_COMP_DEEP           4
#define DMS_COMP_HEAVY1         5
#define DMS_COMP_HEAVY2         6

typedef enum {
    DMS_DIAG_OK = 0,
    DMS_DIAG_BAD_SIGNATURE,
    DMS_DIAG_BAD_CRC,
    DMS_DIAG_ENCRYPTED,
    DMS_DIAG_TRUNCATED,
    DMS_DIAG_COUNT
} dms_diag_code_t;

typedef struct { float overall; bool valid; bool encrypted; uint8_t compression; } dms_score_t;
typedef struct { dms_diag_code_t code; char msg[128]; } dms_diagnosis_t;
typedef struct { dms_diagnosis_t* items; size_t count; size_t cap; float quality; } dms_diagnosis_list_t;

typedef struct {
    char signature[5];
    uint32_t info_bits;
    uint32_t date;
    uint16_t first_track;
    uint16_t last_track;
    uint32_t packed_size;
    uint32_t unpacked_size;
    char creator_version[5];
    uint16_t disk_type;
    uint16_t compression_mode;
    uint16_t info_crc;
    
    bool encrypted;
    bool banner;
    bool fileid;
    
    uint16_t track_count;
    
    dms_score_t score;
    dms_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} dms_disk_t;

static dms_diagnosis_list_t* dms_diagnosis_create(void) {
    dms_diagnosis_list_t* l = calloc(1, sizeof(dms_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(dms_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void dms_diagnosis_free(dms_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t dms_read_be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
static uint32_t dms_read_be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]; }

static const char* dms_compression_name(uint16_t c) {
    switch (c) {
        case DMS_COMP_NONE: return "None";
        case DMS_COMP_SIMPLE: return "Simple";
        case DMS_COMP_QUICK: return "Quick";
        case DMS_COMP_MEDIUM: return "Medium";
        case DMS_COMP_DEEP: return "Deep";
        case DMS_COMP_HEAVY1: return "Heavy 1";
        case DMS_COMP_HEAVY2: return "Heavy 2";
        default: return "Unknown";
    }
}

static bool dms_parse(const uint8_t* data, size_t size, dms_disk_t* disk) {
    if (!data || !disk || size < DMS_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(dms_disk_t));
    disk->diagnosis = dms_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, DMS_SIGNATURE, 4) != 0) {
        return false;
    }
    memcpy(disk->signature, data, 4);
    disk->signature[4] = '\0';
    
    /* Parse header (big-endian!) */
    disk->info_bits = dms_read_be32(data + 4);
    disk->date = dms_read_be32(data + 8);
    disk->first_track = dms_read_be16(data + 12);
    disk->last_track = dms_read_be16(data + 14);
    disk->packed_size = dms_read_be32(data + 16);
    disk->unpacked_size = dms_read_be32(data + 20);
    
    /* Creator version string */
    memcpy(disk->creator_version, data + 24, 4);
    disk->creator_version[4] = '\0';
    
    disk->disk_type = dms_read_be16(data + 50);
    disk->compression_mode = dms_read_be16(data + 52);
    disk->info_crc = dms_read_be16(data + 54);
    
    /* Parse info bits */
    disk->encrypted = (disk->info_bits & 0x00000002) != 0;
    disk->banner = (disk->info_bits & 0x00000010) != 0;
    disk->fileid = (disk->info_bits & 0x00000020) != 0;
    
    disk->track_count = disk->last_track - disk->first_track + 1;
    
    disk->score.encrypted = disk->encrypted;
    disk->score.compression = disk->compression_mode;
    disk->score.overall = disk->encrypted ? 0.5f : 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void dms_disk_free(dms_disk_t* disk) {
    if (disk && disk->diagnosis) dms_diagnosis_free(disk->diagnosis);
}

/* ============================================================================
 * EXTENDED FEATURES - DMS Decompression Helpers
 * ============================================================================ */

/* Track header structure */
typedef struct {
    uint16_t signature;     /* "TR" = 0x5452 */
    uint16_t track_number;
    uint16_t packed_len;
    uint16_t unpacked_len;
    uint8_t flags;
    uint8_t compression_mode;
    uint16_t packed_crc;
    uint16_t unpacked_crc;
} dms_track_header_t;

/* Parse track header */
static bool dms_parse_track_header(const uint8_t* data, size_t size, size_t offset,
                                    dms_track_header_t* header) {
    if (!data || !header || offset + 20 > size) return false;
    
    const uint8_t* p = data + offset;
    
    header->signature = (p[0] << 8) | p[1];
    if (header->signature != 0x5452) return false;  /* "TR" */
    
    header->track_number = (p[2] << 8) | p[3];
    /* Skip unknown bytes */
    header->packed_len = (p[6] << 8) | p[7];
    header->unpacked_len = (p[8] << 8) | p[9];
    header->flags = p[10];
    header->compression_mode = p[11];
    header->packed_crc = (p[12] << 8) | p[13];
    header->unpacked_crc = (p[14] << 8) | p[15];
    
    return true;
}

/* Calculate CRC16 */
static uint16_t dms_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    return crc;
}

/* Count tracks in DMS file */
static int dms_count_tracks(const uint8_t* data, size_t size) {
    if (!data || size < DMS_HEADER_SIZE + 20) return 0;
    
    int count = 0;
    size_t pos = DMS_HEADER_SIZE;
    
    while (pos + 20 < size) {
        dms_track_header_t hdr;
        if (!dms_parse_track_header(data, size, pos, &hdr)) break;
        
        count++;
        pos += 20 + hdr.packed_len;
    }
    
    return count;
}

/* Get compression ratio */
static float dms_get_ratio(const dms_disk_t* disk) {
    if (!disk || disk->unpacked_size == 0) return 0.0f;
    return (float)disk->packed_size / (float)disk->unpacked_size;
}

/* Estimate decompression time (rough, based on compression mode) */
static const char* dms_decompression_estimate(uint16_t mode) {
    switch (mode) {
        case DMS_COMP_NONE: return "instant";
        case DMS_COMP_SIMPLE: return "< 1 sec";
        case DMS_COMP_QUICK: return "1-2 sec";
        case DMS_COMP_MEDIUM: return "2-5 sec";
        case DMS_COMP_DEEP: return "5-10 sec";
        case DMS_COMP_HEAVY1:
        case DMS_COMP_HEAVY2: return "10-30 sec";
        default: return "unknown";
    }
}

#ifdef DMS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DMS Parser v3 Tests ===\n");
    
    printf("Testing compression names... ");
    assert(strcmp(dms_compression_name(DMS_COMP_NONE), "None") == 0);
    assert(strcmp(dms_compression_name(DMS_COMP_DEEP), "Deep") == 0);
    printf("✓\n");
    
    printf("Testing DMS parsing... ");
    uint8_t dms[64];
    memset(dms, 0, sizeof(dms));
    memcpy(dms, "DMS!", 4);
    dms[12] = 0; dms[13] = 0;   /* First track */
    dms[14] = 0; dms[15] = 79;  /* Last track */
    dms[52] = 0; dms[53] = 3;   /* Medium compression */
    
    dms_disk_t disk;
    bool ok = dms_parse(dms, sizeof(dms), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.track_count == 80);
    assert(disk.compression_mode == DMS_COMP_MEDIUM);
    dms_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
