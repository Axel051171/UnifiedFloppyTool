/**
 * @file uft_td0_parser_v3.c
 * @brief GOD MODE TD0 Parser v3 - Teledisk Format
 * 
 * TD0 ist das Teledisk-Format (Sydex):
 * - Optional komprimiert (LZHUF)
 * - Umfangreiche Disk-Metadaten
 * - CRC pro Sektor
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TD0_SIGNATURE           "TD"
#define TD0_SIGNATURE_ADV       "td"     /* Advanced compression */
#define TD0_HEADER_SIZE         12

typedef enum {
    TD0_DIAG_OK = 0,
    TD0_DIAG_BAD_SIGNATURE,
    TD0_DIAG_BAD_CRC,
    TD0_DIAG_COMPRESSED,
    TD0_DIAG_TRUNCATED,
    TD0_DIAG_COUNT
} td0_diag_code_t;

typedef struct { float overall; bool valid; bool compressed; } td0_score_t;
typedef struct { td0_diag_code_t code; char msg[128]; } td0_diagnosis_t;
typedef struct { td0_diagnosis_t* items; size_t count; size_t cap; float quality; } td0_diagnosis_list_t;

typedef struct {
    char signature[3];
    uint8_t sequence;
    uint8_t check_sig;
    uint8_t version;
    uint8_t data_rate;
    uint8_t drive_type;
    uint8_t stepping;
    uint8_t dos_alloc;
    uint8_t sides;
    uint16_t crc;
    
    bool is_advanced;
    
    /* Comment block (if present) */
    bool has_comment;
    uint16_t comment_crc;
    uint16_t comment_length;
    uint8_t year, month, day, hour, minute, second;
    char comment[256];
    
    td0_score_t score;
    td0_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} td0_disk_t;

static td0_diagnosis_list_t* td0_diagnosis_create(void) {
    td0_diagnosis_list_t* l = calloc(1, sizeof(td0_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(td0_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void td0_diagnosis_free(td0_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t td0_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static const char* td0_data_rate_name(uint8_t r) {
    switch (r & 0x03) {
        case 0: return "250 Kbps";
        case 1: return "300 Kbps";
        case 2: return "500 Kbps";
        default: return "Unknown";
    }
}

static bool td0_parse(const uint8_t* data, size_t size, td0_disk_t* disk) {
    if (!data || !disk || size < TD0_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(td0_disk_t));
    disk->diagnosis = td0_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, TD0_SIGNATURE, 2) == 0) {
        disk->is_advanced = false;
    } else if (memcmp(data, TD0_SIGNATURE_ADV, 2) == 0) {
        disk->is_advanced = true;
    } else {
        return false;
    }
    
    memcpy(disk->signature, data, 2);
    disk->signature[2] = '\0';
    
    disk->sequence = data[2];
    disk->check_sig = data[3];
    disk->version = data[4];
    disk->data_rate = data[5];
    disk->drive_type = data[6];
    disk->stepping = data[7];
    disk->dos_alloc = data[8];
    disk->sides = data[9];
    disk->crc = td0_read_le16(data + 10);
    
    /* Check for comment block (bit 7 of stepping) */
    if (disk->stepping & 0x80) {
        disk->has_comment = true;
        
        if (size >= TD0_HEADER_SIZE + 10) {
            const uint8_t* c = data + TD0_HEADER_SIZE;
            disk->comment_crc = td0_read_le16(c);
            disk->comment_length = td0_read_le16(c + 2);
            disk->year = c[4];
            disk->month = c[5];
            disk->day = c[6];
            disk->hour = c[7];
            disk->minute = c[8];
            disk->second = c[9];
            
            if (disk->comment_length > 0 && 
                TD0_HEADER_SIZE + 10 + disk->comment_length <= size) {
                size_t len = disk->comment_length;
                if (len > sizeof(disk->comment) - 1) len = sizeof(disk->comment) - 1;
                memcpy(disk->comment, c + 10, len);
                disk->comment[len] = '\0';
            }
        }
    }
    
    disk->score.compressed = disk->is_advanced;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void td0_disk_free(td0_disk_t* disk) {
    if (disk && disk->diagnosis) td0_diagnosis_free(disk->diagnosis);
}

#ifdef TD0_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TD0 Parser v3 Tests ===\n");
    
    printf("Testing data rate names... ");
    assert(strcmp(td0_data_rate_name(0), "250 Kbps") == 0);
    assert(strcmp(td0_data_rate_name(2), "500 Kbps") == 0);
    printf("✓\n");
    
    printf("Testing TD0 parsing... ");
    uint8_t td0[32];
    memset(td0, 0, sizeof(td0));
    td0[0] = 'T'; td0[1] = 'D';
    td0[4] = 21;  /* Version 2.1 */
    td0[5] = 2;   /* 500 Kbps */
    td0[9] = 2;   /* 2 sides */
    
    td0_disk_t disk;
    bool ok = td0_parse(td0, sizeof(td0), &disk);
    assert(ok);
    assert(disk.valid);
    assert(!disk.is_advanced);
    assert(disk.sides == 2);
    td0_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
