/**
 * @file uft_p00_parser_v3.c
 * @brief GOD MODE P00 Parser v3 - PC64 Emulator Format
 * 
 * P00/S00/U00/R00 ist das PC64 Container-Format:
 * - 26-Byte Header mit C64-Filename
 * - PRG/SEQ/USR/REL Dateitypen
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define P00_SIGNATURE           "C64File"
#define P00_SIGNATURE_LEN       7
#define P00_HEADER_SIZE         26

typedef enum {
    P00_DIAG_OK = 0,
    P00_DIAG_BAD_SIGNATURE,
    P00_DIAG_TRUNCATED,
    P00_DIAG_COUNT
} p00_diag_code_t;

typedef enum {
    P00_TYPE_PRG = 'P',
    P00_TYPE_SEQ = 'S',
    P00_TYPE_USR = 'U',
    P00_TYPE_REL = 'R'
} p00_type_t;

typedef struct { float overall; bool valid; p00_type_t type; } p00_score_t;
typedef struct { p00_diag_code_t code; char msg[128]; } p00_diagnosis_t;
typedef struct { p00_diagnosis_t* items; size_t count; size_t cap; float quality; } p00_diagnosis_list_t;

typedef struct {
    char signature[8];
    char c64_filename[17];
    uint8_t record_size;        /* For REL files */
    p00_type_t type;
    
    uint16_t load_address;      /* For PRG files */
    uint32_t data_size;
    
    p00_score_t score;
    p00_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} p00_file_t;

static p00_diagnosis_list_t* p00_diagnosis_create(void) {
    p00_diagnosis_list_t* l = calloc(1, sizeof(p00_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(p00_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void p00_diagnosis_free(p00_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t p00_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static void p00_copy_filename(char* dest, const uint8_t* src) {
    for (int i = 0; i < 16; i++) {
        uint8_t c = src[i];
        if (c == 0xA0 || c == 0x00) c = ' ';
        else if (c < 0x20 || c > 0x7E) c = '.';
        dest[i] = c;
    }
    dest[16] = '\0';
    /* Trim trailing spaces */
    for (int i = 15; i >= 0 && dest[i] == ' '; i--) dest[i] = '\0';
}

static bool p00_parse(const uint8_t* data, size_t size, p00_file_t* p00) {
    if (!data || !p00 || size < P00_HEADER_SIZE) return false;
    memset(p00, 0, sizeof(p00_file_t));
    p00->diagnosis = p00_diagnosis_create();
    p00->source_size = size;
    
    /* Check signature */
    if (memcmp(data, P00_SIGNATURE, P00_SIGNATURE_LEN) != 0) {
        return false;
    }
    memcpy(p00->signature, data, 7);
    p00->signature[7] = '\0';
    
    /* C64 filename at offset 8 */
    p00_copy_filename(p00->c64_filename, data + 8);
    
    /* Record size at offset 25 (for REL files) */
    p00->record_size = data[25];
    
    /* Data starts at offset 26 */
    p00->data_size = size - P00_HEADER_SIZE;
    
    /* For PRG files, first 2 bytes of data are load address */
    if (p00->data_size >= 2) {
        p00->load_address = p00_read_le16(data + P00_HEADER_SIZE);
    }
    
    /* Type is determined by file extension (P00/S00/U00/R00) */
    /* We assume PRG since that's most common */
    p00->type = P00_TYPE_PRG;
    
    p00->score.type = p00->type;
    p00->score.overall = 1.0f;
    p00->score.valid = true;
    p00->valid = true;
    
    return true;
}

static void p00_file_free(p00_file_t* p00) {
    if (p00 && p00->diagnosis) p00_diagnosis_free(p00->diagnosis);
}

#ifdef P00_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== P00 Parser v3 Tests ===\n");
    
    printf("Testing P00 parsing... ");
    uint8_t p00[64];
    memset(p00, 0, sizeof(p00));
    memcpy(p00, "C64File", 7);
    p00[7] = 0x00;
    memcpy(p00 + 8, "TEST PROGRAM    ", 16);
    p00[25] = 0;  /* Record size */
    /* PRG data */
    p00[26] = 0x01; p00[27] = 0x08;  /* Load $0801 */
    
    p00_file_t file;
    bool ok = p00_parse(p00, sizeof(p00), &file);
    assert(ok);
    assert(file.valid);
    assert(file.load_address == 0x0801);
    assert(strcmp(file.c64_filename, "TEST PROGRAM") == 0);
    p00_file_free(&file);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
