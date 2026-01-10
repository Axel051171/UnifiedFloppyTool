/**
 * @file uft_prg_parser_v3.c
 * @brief GOD MODE PRG Parser v3 - Commodore 64 Program File
 * 
 * PRG ist das einfachste C64-Format:
 * - 2-Byte Load Address (Little Endian)
 * - Rohe Programmdaten
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PRG_MIN_SIZE            3       /* 2 byte address + 1 byte data */
#define PRG_MAX_SIZE            65538   /* 2 + 64K */

/* Common load addresses */
#define PRG_BASIC_START         0x0801  /* BASIC program start */
#define PRG_BASIC_END           0x9FFF  /* BASIC RAM end */
#define PRG_SCREEN_RAM          0x0400  /* Screen memory */
#define PRG_KERNAL_START        0xE000  /* KERNAL ROM */

typedef enum {
    PRG_DIAG_OK = 0,
    PRG_DIAG_TOO_SMALL,
    PRG_DIAG_TOO_LARGE,
    PRG_DIAG_BAD_ADDRESS,
    PRG_DIAG_OVERLAP_ROM,
    PRG_DIAG_COUNT
} prg_diag_code_t;

typedef enum {
    PRG_TYPE_BASIC = 0,
    PRG_TYPE_ML,            /* Machine Language */
    PRG_TYPE_DATA,
    PRG_TYPE_SCREEN,
    PRG_TYPE_CHARSET,
    PRG_TYPE_SPRITE,
    PRG_TYPE_UNKNOWN
} prg_type_t;

typedef struct { float overall; bool valid; prg_type_t type; } prg_score_t;
typedef struct { prg_diag_code_t code; char msg[128]; } prg_diagnosis_t;
typedef struct { prg_diagnosis_t* items; size_t count; size_t cap; float quality; } prg_diagnosis_list_t;

typedef struct {
    uint16_t load_address;
    uint16_t end_address;
    uint16_t data_size;
    prg_type_t type;
    
    /* BASIC detection */
    bool is_basic;
    uint16_t basic_start_line;
    
    prg_score_t score;
    prg_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} prg_file_t;

static prg_diagnosis_list_t* prg_diagnosis_create(void) {
    prg_diagnosis_list_t* l = calloc(1, sizeof(prg_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(prg_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void prg_diagnosis_free(prg_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t prg_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static const char* prg_type_name(prg_type_t t) {
    switch (t) {
        case PRG_TYPE_BASIC: return "BASIC Program";
        case PRG_TYPE_ML: return "Machine Language";
        case PRG_TYPE_DATA: return "Data File";
        case PRG_TYPE_SCREEN: return "Screen Data";
        case PRG_TYPE_CHARSET: return "Character Set";
        case PRG_TYPE_SPRITE: return "Sprite Data";
        default: return "Unknown";
    }
}

static prg_type_t prg_detect_type(uint16_t addr, const uint8_t* data, size_t size) {
    /* BASIC programs start at $0801 with a link pointer */
    if (addr == PRG_BASIC_START && size >= 4) {
        uint16_t link = prg_read_le16(data);
        if (link > addr && link < 0xA000) {
            return PRG_TYPE_BASIC;
        }
    }
    
    /* Screen RAM */
    if (addr >= 0x0400 && addr < 0x0800) {
        return PRG_TYPE_SCREEN;
    }
    
    /* Character ROM area */
    if (addr >= 0xD000 && addr < 0xD800) {
        return PRG_TYPE_CHARSET;
    }
    
    /* Common ML start addresses */
    if (addr == 0xC000 || addr == 0x0800 || addr == 0x1000 ||
        addr == 0x2000 || addr == 0x4000 || addr == 0x8000) {
        return PRG_TYPE_ML;
    }
    
    return PRG_TYPE_UNKNOWN;
}

static bool prg_parse(const uint8_t* data, size_t size, prg_file_t* prg) {
    if (!data || !prg || size < PRG_MIN_SIZE) return false;
    memset(prg, 0, sizeof(prg_file_t));
    prg->diagnosis = prg_diagnosis_create();
    prg->source_size = size;
    
    /* Parse load address */
    prg->load_address = prg_read_le16(data);
    prg->data_size = size - 2;
    prg->end_address = prg->load_address + prg->data_size - 1;
    
    /* Validate */
    if (prg->end_address < prg->load_address) {
        /* Overflow */
        prg->diagnosis->quality *= 0.5f;
    }
    
    /* Detect type */
    prg->type = prg_detect_type(prg->load_address, data + 2, prg->data_size);
    prg->is_basic = (prg->type == PRG_TYPE_BASIC);
    
    /* Parse BASIC start line if applicable */
    if (prg->is_basic && prg->data_size >= 4) {
        prg->basic_start_line = prg_read_le16(data + 4);
    }
    
    prg->score.type = prg->type;
    prg->score.overall = 1.0f;
    prg->score.valid = true;
    prg->valid = true;
    
    return true;
}

static void prg_file_free(prg_file_t* prg) {
    if (prg && prg->diagnosis) prg_diagnosis_free(prg->diagnosis);
}

#ifdef PRG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PRG Parser v3 Tests ===\n");
    
    printf("Testing type names... ");
    assert(strcmp(prg_type_name(PRG_TYPE_BASIC), "BASIC Program") == 0);
    assert(strcmp(prg_type_name(PRG_TYPE_ML), "Machine Language") == 0);
    printf("✓\n");
    
    printf("Testing BASIC detection... ");
    uint8_t basic[16] = {
        0x01, 0x08,     /* Load address $0801 */
        0x0B, 0x08,     /* Link to next line $080B */
        0x0A, 0x00,     /* Line number 10 */
        0x99, 0x20,     /* PRINT token + space */
        0x22, 0x48, 0x49, 0x22,  /* "HI" */
        0x00,           /* End of line */
        0x00, 0x00      /* End of program */
    };
    
    prg_file_t prg;
    bool ok = prg_parse(basic, sizeof(basic), &prg);
    assert(ok);
    assert(prg.valid);
    assert(prg.load_address == 0x0801);
    assert(prg.is_basic);
    assert(prg.type == PRG_TYPE_BASIC);
    prg_file_free(&prg);
    printf("✓\n");
    
    printf("Testing ML detection... ");
    uint8_t ml[8] = {
        0x00, 0xC0,     /* Load address $C000 */
        0x78,           /* SEI */
        0xA9, 0x00,     /* LDA #$00 */
        0x8D, 0x20, 0xD0 /* STA $D020 */
    };
    
    ok = prg_parse(ml, sizeof(ml), &prg);
    assert(ok);
    assert(prg.valid);
    assert(prg.load_address == 0xC000);
    assert(prg.type == PRG_TYPE_ML);
    prg_file_free(&prg);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
