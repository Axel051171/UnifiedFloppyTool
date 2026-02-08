/**
 * @file uft_crt_parser_v3.c
 * @brief GOD MODE CRT Parser v3 - Commodore 64 Cartridge
 * 
 * CRT ist das Cartridge-Format für C64/C128:
 * - 64-Byte Header
 * - CHIP Pakete mit ROM-Daten
 * - Viele Cartridge-Typen (Normal, Action Replay, EasyFlash, etc.)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CRT_SIGNATURE           "C64 CARTRIDGE   "
#define CRT_SIGNATURE_LEN       16
#define CRT_HEADER_SIZE         64
#define CRT_CHIP_HEADER_SIZE    16

/* Cartridge types */
#define CRT_TYPE_NORMAL         0
#define CRT_TYPE_ACTION_REPLAY  1
#define CRT_TYPE_KCS_POWER      2
#define CRT_TYPE_FINAL_III      3
#define CRT_TYPE_SIMONS_BASIC   4
#define CRT_TYPE_OCEAN          5
#define CRT_TYPE_EXPERT         6
#define CRT_TYPE_FUNPLAY        7
#define CRT_TYPE_SUPER_GAMES    8
#define CRT_TYPE_ATOMIC_POWER   9
#define CRT_TYPE_EPYX_FASTLOAD  10
#define CRT_TYPE_WESTERMANN     11
#define CRT_TYPE_REX            12
#define CRT_TYPE_FINAL_I        13
#define CRT_TYPE_MAGIC_FORMEL   14
#define CRT_TYPE_GS             15
#define CRT_TYPE_WARPSPEED      16
#define CRT_TYPE_DINAMIC        17
#define CRT_TYPE_ZAXXON         18
#define CRT_TYPE_MAGIC_DESK     19
#define CRT_TYPE_SUPER_SNAP_V5  20
#define CRT_TYPE_COMAL80        21
#define CRT_TYPE_ROSS           22
#define CRT_TYPE_DELA_EP64      23
#define CRT_TYPE_DELA_EP7x8     24
#define CRT_TYPE_DELA_EP256     25
#define CRT_TYPE_REX_EP256      26
#define CRT_TYPE_MIKRO_ASM      27
#define CRT_TYPE_FINAL_PLUS     28
#define CRT_TYPE_ACTION_REPLAY4 29
#define CRT_TYPE_STARDOS        30
#define CRT_TYPE_EASYFLASH      32
#define CRT_TYPE_EASYFLASH_XBANK 33
#define CRT_TYPE_CAPTURE        34
#define CRT_TYPE_ACTION_REPLAY3 35
#define CRT_TYPE_RETRO_REPLAY   36
#define CRT_TYPE_MMC64          37
#define CRT_TYPE_MMC_REPLAY     38
#define CRT_TYPE_IDE64          39
#define CRT_TYPE_SUPER_SNAP_V4  40
#define CRT_TYPE_IEEE488        41
#define CRT_TYPE_GAME_KILLER    43
#define CRT_TYPE_P64            44
#define CRT_TYPE_EXOS           45
#define CRT_TYPE_FREEZE_FRAME   46
#define CRT_TYPE_FREEZE_MACHINE 47
#define CRT_TYPE_SNAPSHOT64     48
#define CRT_TYPE_SUPER_EXPLODE  49
#define CRT_TYPE_MAGIC_VOICE    50
#define CRT_TYPE_ACTION_REPLAY2 51
#define CRT_TYPE_MACH5          52
#define CRT_TYPE_DIASHOW_MAKER  53
#define CRT_TYPE_PAGEFOX        54
#define CRT_TYPE_KINGSOFT       55
#define CRT_TYPE_SILVERROCK     56
#define CRT_TYPE_FORMEL64       57
#define CRT_TYPE_RGCD           58
#define CRT_TYPE_RRNETMK3       59
#define CRT_TYPE_EASYCALC       60
#define CRT_TYPE_GMOD2          61

#define CRT_MAX_CHIPS           256

typedef enum {
    CRT_DIAG_OK = 0,
    CRT_DIAG_BAD_SIGNATURE,
    CRT_DIAG_BAD_HEADER,
    CRT_DIAG_BAD_CHIP,
    CRT_DIAG_TRUNCATED,
    CRT_DIAG_COUNT
} crt_diag_code_t;

typedef struct { float overall; bool valid; uint16_t type; uint8_t chips; } crt_score_t;
typedef struct { crt_diag_code_t code; char msg[128]; } crt_diagnosis_t;
typedef struct { crt_diagnosis_t* items; size_t count; size_t cap; float quality; } crt_diagnosis_list_t;

typedef struct {
    uint16_t type;          /* ROM/RAM/Flash */
    uint16_t bank;
    uint16_t load_address;
    uint16_t size;
    uint32_t data_offset;
} crt_chip_t;

typedef struct {
    char signature[17];
    uint32_t header_length;
    uint16_t version;
    uint16_t hardware_type;
    uint8_t exrom;
    uint8_t game;
    char name[33];
    
    crt_chip_t chips[CRT_MAX_CHIPS];
    uint16_t chip_count;
    uint32_t total_rom_size;
    
    crt_score_t score;
    crt_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} crt_file_t;

static crt_diagnosis_list_t* crt_diagnosis_create(void) {
    crt_diagnosis_list_t* l = calloc(1, sizeof(crt_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(crt_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void crt_diagnosis_free(crt_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t crt_read_be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
static uint32_t crt_read_be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]; }

static const char* crt_type_name(uint16_t t) {
    switch (t) {
        case CRT_TYPE_NORMAL: return "Normal Cartridge";
        case CRT_TYPE_ACTION_REPLAY: return "Action Replay";
        case CRT_TYPE_FINAL_III: return "Final Cartridge III";
        case CRT_TYPE_OCEAN: return "Ocean";
        case CRT_TYPE_EASYFLASH: return "EasyFlash";
        case CRT_TYPE_RETRO_REPLAY: return "Retro Replay";
        case CRT_TYPE_SUPER_SNAP_V5: return "Super Snapshot V5";
        default: return "Unknown";
    }
}

static bool crt_parse(const uint8_t* data, size_t size, crt_file_t* crt) {
    if (!data || !crt || size < CRT_HEADER_SIZE) return false;
    memset(crt, 0, sizeof(crt_file_t));
    crt->diagnosis = crt_diagnosis_create();
    crt->source_size = size;
    
    /* Check signature */
    if (memcmp(data, CRT_SIGNATURE, CRT_SIGNATURE_LEN) != 0) {
        return false;
    }
    memcpy(crt->signature, data, 16);
    crt->signature[16] = '\0';
    
    /* Parse header (big-endian!) */
    crt->header_length = crt_read_be32(data + 16);
    crt->version = crt_read_be16(data + 20);
    crt->hardware_type = crt_read_be16(data + 22);
    crt->exrom = data[24];
    crt->game = data[25];
    
    /* Cartridge name at offset 32 */
    memcpy(crt->name, data + 32, 32);
    crt->name[32] = '\0';
    
    /* Parse CHIP packets */
    size_t pos = crt->header_length;
    crt->chip_count = 0;
    crt->total_rom_size = 0;
    
    while (pos + CRT_CHIP_HEADER_SIZE <= size && crt->chip_count < CRT_MAX_CHIPS) {
        /* Check CHIP signature */
        if (memcmp(data + pos, "CHIP", 4) != 0) break;
        
        crt_chip_t* chip = &crt->chips[crt->chip_count];
        
        uint32_t packet_length = crt_read_be32(data + pos + 4);
        chip->type = crt_read_be16(data + pos + 8);
        chip->bank = crt_read_be16(data + pos + 10);
        chip->load_address = crt_read_be16(data + pos + 12);
        chip->size = crt_read_be16(data + pos + 14);
        chip->data_offset = pos + CRT_CHIP_HEADER_SIZE;
        
        crt->total_rom_size += chip->size;
        crt->chip_count++;
        
        pos += packet_length;
    }
    
    crt->score.type = crt->hardware_type;
    crt->score.chips = crt->chip_count;
    crt->score.overall = crt->chip_count > 0 ? 1.0f : 0.0f;
    crt->score.valid = crt->chip_count > 0;
    crt->valid = true;
    
    return true;
}

static void crt_file_free(crt_file_t* crt) {
    if (crt && crt->diagnosis) crt_diagnosis_free(crt->diagnosis);
}

#ifdef CRT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CRT Parser v3 Tests ===\n");
    
    printf("Testing type names... ");
    assert(strcmp(crt_type_name(CRT_TYPE_NORMAL), "Normal Cartridge") == 0);
    assert(strcmp(crt_type_name(CRT_TYPE_EASYFLASH), "EasyFlash") == 0);
    printf("✓\n");
    
    printf("Testing CRT parsing... ");
    uint8_t crt_data[128];
    memset(crt_data, 0, sizeof(crt_data));
    memcpy(crt_data, "C64 CARTRIDGE   ", 16);
    /* Header length (BE) */
    crt_data[16] = 0; crt_data[17] = 0; crt_data[18] = 0; crt_data[19] = 64;
    /* Version 1.0 */
    crt_data[20] = 0; crt_data[21] = 1;
    /* Type: Normal */
    crt_data[22] = 0; crt_data[23] = 0;
    /* EXROM/GAME */
    crt_data[24] = 0; crt_data[25] = 0;
    /* Name */
    memcpy(crt_data + 32, "TEST CART", 9);
    
    /* CHIP packet */
    memcpy(crt_data + 64, "CHIP", 4);
    /* Packet length (BE) */
    crt_data[68] = 0; crt_data[69] = 0; crt_data[70] = 0; crt_data[71] = 32;
    /* Type, Bank */
    crt_data[72] = 0; crt_data[73] = 0;
    crt_data[74] = 0; crt_data[75] = 0;
    /* Load address (BE) $8000 */
    crt_data[76] = 0x80; crt_data[77] = 0x00;
    /* Size (BE) 16 bytes */
    crt_data[78] = 0; crt_data[79] = 16;
    
    crt_file_t crt;
    bool ok = crt_parse(crt_data, sizeof(crt_data), &crt);
    assert(ok);
    assert(crt.valid);
    assert(crt.hardware_type == CRT_TYPE_NORMAL);
    assert(crt.chip_count == 1);
    crt_file_free(&crt);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
