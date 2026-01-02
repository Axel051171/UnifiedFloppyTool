/**
 * @file uft_t64_parser_v3.c
 * @brief GOD MODE T64 Parser v3 - Commodore 64 Tape Archive
 * 
 * T64 ist ein Container-Format für C64 Programme:
 * - Tape Archive (nicht echtes Tape-Format)
 * - Bis zu 30 Directory-Einträge
 * - 64-Byte Header + 32-Byte Einträge
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define T64_HEADER_SIZE         64
#define T64_ENTRY_SIZE          32
#define T64_MAX_ENTRIES         30
#define T64_MIN_SIZE            (T64_HEADER_SIZE + T64_ENTRY_SIZE)

#define T64_SIGNATURE_1         "C64 tape image file"
#define T64_SIGNATURE_2         "C64S tape image file"
#define T64_SIGNATURE_3         "C64S tape file"

typedef enum {
    T64_DIAG_OK = 0,
    T64_DIAG_BAD_SIGNATURE,
    T64_DIAG_NO_ENTRIES,
    T64_DIAG_TRUNCATED,
    T64_DIAG_BAD_ENTRY,
    T64_DIAG_COUNT
} t64_diag_code_t;

typedef struct { float overall; bool valid; uint8_t entries; } t64_score_t;
typedef struct { t64_diag_code_t code; uint8_t entry; char msg[128]; } t64_diagnosis_t;
typedef struct { t64_diagnosis_t* items; size_t count; size_t cap; float quality; } t64_diagnosis_list_t;

typedef struct {
    uint8_t entry_type;     /* 0=free, 1=normal, 2=snapshot, 3=tape block */
    uint8_t file_type;      /* C64 file type (PRG, SEQ, etc.) */
    uint16_t start_address;
    uint16_t end_address;
    uint32_t data_offset;
    char name[17];
    uint32_t size;
    bool valid;
} t64_entry_t;

typedef struct {
    char signature[33];
    uint16_t version;
    uint16_t max_entries;
    uint16_t used_entries;
    char tape_name[25];
    
    t64_entry_t entries[T64_MAX_ENTRIES];
    uint16_t valid_entries;
    
    t64_score_t score;
    t64_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} t64_disk_t;

static t64_diagnosis_list_t* t64_diagnosis_create(void) {
    t64_diagnosis_list_t* l = calloc(1, sizeof(t64_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(t64_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void t64_diagnosis_free(t64_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t t64_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t t64_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static void t64_copy_name(char* dest, const uint8_t* src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = src[i];
        if (c == 0xA0 || c == 0x00) c = ' ';
        else if (c < 0x20 || c > 0x7E) c = '.';
        dest[i] = c;
    }
    dest[len] = '\0';
    for (int i = len - 1; i >= 0 && dest[i] == ' '; i--) dest[i] = '\0';
}

static bool t64_parse(const uint8_t* data, size_t size, t64_disk_t* disk) {
    if (!data || !disk || size < T64_MIN_SIZE) return false;
    memset(disk, 0, sizeof(t64_disk_t));
    disk->diagnosis = t64_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    t64_copy_name(disk->signature, data, 32);
    if (strncmp(disk->signature, "C64", 3) != 0) {
        return false;
    }
    
    /* Parse header */
    disk->version = t64_read_le16(data + 0x20);
    disk->max_entries = t64_read_le16(data + 0x22);
    disk->used_entries = t64_read_le16(data + 0x24);
    t64_copy_name(disk->tape_name, data + 0x28, 24);
    
    if (disk->max_entries > T64_MAX_ENTRIES) {
        disk->max_entries = T64_MAX_ENTRIES;
    }
    
    /* Parse entries */
    disk->valid_entries = 0;
    for (uint16_t i = 0; i < disk->max_entries && i < disk->used_entries; i++) {
        size_t off = T64_HEADER_SIZE + i * T64_ENTRY_SIZE;
        if (off + T64_ENTRY_SIZE > size) break;
        
        const uint8_t* e = data + off;
        t64_entry_t* entry = &disk->entries[i];
        
        entry->entry_type = e[0];
        entry->file_type = e[1];
        entry->start_address = t64_read_le16(e + 2);
        entry->end_address = t64_read_le16(e + 4);
        entry->data_offset = t64_read_le32(e + 8);
        t64_copy_name(entry->name, e + 16, 16);
        
        if (entry->end_address > entry->start_address) {
            entry->size = entry->end_address - entry->start_address;
        }
        
        if (entry->entry_type == 1 && entry->data_offset > 0) {
            entry->valid = true;
            disk->valid_entries++;
        }
    }
    
    disk->score.entries = disk->valid_entries;
    disk->score.overall = disk->valid_entries > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->valid_entries > 0;
    disk->valid = true;
    
    return true;
}

static void t64_disk_free(t64_disk_t* disk) {
    if (disk && disk->diagnosis) t64_diagnosis_free(disk->diagnosis);
}

#ifdef T64_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== T64 Parser v3 Tests ===\n");
    
    printf("Testing T64 parsing... ");
    uint8_t t64[256];
    memset(t64, 0, sizeof(t64));
    memcpy(t64, "C64 tape image file", 19);
    t64[0x22] = 1;  /* max entries */
    t64[0x24] = 1;  /* used entries */
    
    /* Entry */
    t64[64] = 1;    /* entry type: normal */
    t64[65] = 0x82; /* file type: PRG */
    t64[66] = 0x01; t64[67] = 0x08;  /* start: $0801 */
    t64[68] = 0x00; t64[69] = 0x10;  /* end: $1000 */
    t64[72] = 0x60; /* data offset */
    memcpy(t64 + 80, "TEST            ", 16);
    
    t64_disk_t disk;
    bool ok = t64_parse(t64, sizeof(t64), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.valid_entries == 1);
    t64_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
