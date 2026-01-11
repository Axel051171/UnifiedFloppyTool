/**
 * @file test_retroghidra_formats_p2.c
 * @brief Tests for RetroGhidra Phase 2 formats:
 * - Commodore D80 Disk
 * - Apple II DOS 3.3 Disk
 * - Apple II ProDOS Disk
 * - Atari 8-bit XEX Executable
 * - Atari ST PRG/TOS Executable
 * - TRS-80 /CMD Executable
 * - CoCo CCC Cartridge
 * - Spectrum Next NEX Executable
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/disk/uft_d80_format.h"
#include "uft/disk/uft_apple_dos33.h"
#include "uft/disk/uft_apple_prodos.h"
#include "uft/executable/uft_atari_xex.h"
#include "uft/executable/uft_atari_st_prg.h"
#include "uft/executable/uft_trs80_cmd.h"
#include "uft/cartridge/uft_coco_ccc.h"
#include "uft/executable/uft_spectrum_nex.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  Testing: %s... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * D80 Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_d80_entry_size(void) {
    return sizeof(uft_d80_dir_entry_t) == 32;
}

static int test_d80_file_sizes(void) {
    return UFT_D80_FILE_SIZE == 533248 && UFT_D82_FILE_SIZE == 1066496;
}

static int test_d80_sectors_per_track(void) {
    return uft_d80_sectors_per_track(1) == 29 &&
           uft_d80_sectors_per_track(39) == 29 &&
           uft_d80_sectors_per_track(40) == 27 &&
           uft_d80_sectors_per_track(53) == 27 &&
           uft_d80_sectors_per_track(54) == 25 &&
           uft_d80_sectors_per_track(64) == 25 &&
           uft_d80_sectors_per_track(65) == 23 &&
           uft_d80_sectors_per_track(77) == 23;
}

static int test_d80_type_names(void) {
    return strcmp(uft_d80_type_name(UFT_D80_TYPE_PRG), "PRG") == 0 &&
           strcmp(uft_d80_type_name(UFT_D80_TYPE_SEQ), "SEQ") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Apple II DOS 3.3 Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_dos33_entry_size(void) {
    return sizeof(uft_dos33_dir_entry_t) == 35;
}

static int test_dos33_file_size(void) {
    return UFT_DOS33_FILE_SIZE == 143360;
}

static int test_dos33_geometry(void) {
    return UFT_DOS33_TRACKS == 35 &&
           UFT_DOS33_SECTORS == 16 &&
           UFT_DOS33_SECTOR_SIZE == 256;
}

static int test_dos33_type_names(void) {
    return strcmp(uft_dos33_type_name(UFT_DOS33_TYPE_BINARY), "B") == 0 &&
           strcmp(uft_dos33_type_name(UFT_DOS33_TYPE_APPLESOFT), "A") == 0 &&
           strcmp(uft_dos33_type_name(UFT_DOS33_TYPE_TEXT), "T") == 0;
}

static int test_dos33_sector_offset(void) {
    /* Track 0, Sector 0 = offset 0 */
    /* Track 17, Sector 0 = offset 17*16*256 = 69632 */
    return uft_dos33_sector_offset(0, 0) == 0 &&
           uft_dos33_sector_offset(17, 0) == 69632 &&
           uft_dos33_sector_offset(34, 15) == 143104;  /* Last sector */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Apple II ProDOS Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_prodos_entry_size(void) {
    return sizeof(uft_prodos_dir_entry_t) == 39 &&
           sizeof(uft_prodos_vol_header_t) == 39;
}

static int test_prodos_file_sizes(void) {
    return UFT_PRODOS_140K_SIZE == 143360 &&
           UFT_PRODOS_800K_SIZE == 819200;
}

static int test_prodos_storage_names(void) {
    return strstr(uft_prodos_storage_name(UFT_PRODOS_STORAGE_SEEDLING), "Seedling") != NULL &&
           strstr(uft_prodos_storage_name(UFT_PRODOS_STORAGE_SAPLING), "Sapling") != NULL &&
           strstr(uft_prodos_storage_name(UFT_PRODOS_STORAGE_TREE), "Tree") != NULL;
}

static int test_prodos_type_names(void) {
    return strcmp(uft_prodos_type_name(UFT_PRODOS_TYPE_BINARY), "BIN") == 0 &&
           strcmp(uft_prodos_type_name(UFT_PRODOS_TYPE_SYSTEM), "SYS") == 0 &&
           strcmp(uft_prodos_type_name(UFT_PRODOS_TYPE_DIRECTORY), "DIR") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Atari XEX Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_xex_segment_header_size(void) {
    return sizeof(uft_xex_segment_header_t) == 4;
}

static int test_xex_addresses(void) {
    return UFT_XEX_RUNAD == 0x02E0 && UFT_XEX_INITAD == 0x02E2;
}

static int test_xex_probe_valid(void) {
    /* Simple XEX: header marker + one segment at $2000 */
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    data[0] = 0xFF; data[1] = 0xFF;  /* Header marker */
    data[2] = 0x00; data[3] = 0x20;  /* Start = $2000 */
    data[4] = 0x05; data[5] = 0x20;  /* End = $2005 */
    /* 6 bytes of data at $2000-$2005 */
    
    int score = uft_xex_probe(data, sizeof(data));
    return score >= 50;
}

static int test_xex_parse(void) {
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    data[0] = 0xFF; data[1] = 0xFF;
    data[2] = 0x00; data[3] = 0x20;  /* Start = $2000 */
    data[4] = 0x03; data[5] = 0x20;  /* End = $2003 */
    /* 4 bytes of data */
    
    uft_xex_file_info_t info;
    bool ok = uft_xex_parse(data, 10, &info);
    
    return ok && info.segment_count == 1 &&
           info.lowest_address == 0x2000 && info.highest_address == 0x2003;
}

static int test_xex_runad_initad(void) {
    return uft_xex_is_runad(0x02E0, 0x02E1) &&
           uft_xex_is_initad(0x02E2, 0x02E3) &&
           !uft_xex_is_runad(0x2000, 0x2010);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Atari ST PRG Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_st_prg_header_size(void) {
    return sizeof(uft_st_prg_header_t) == 28;
}

static int test_st_prg_symbol_size(void) {
    return sizeof(uft_st_prg_symbol_t) == 14;
}

static int test_st_prg_magic(void) {
    return UFT_ST_PRG_MAGIC_601A == 0x601A && UFT_ST_PRG_MAGIC_601B == 0x601B;
}

static int test_st_prg_probe_valid(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    /* Magic 0x601A */
    data[0] = 0x60; data[1] = 0x1A;
    /* TEXT size = 100 (BE) */
    data[5] = 100;
    /* DATA size = 20 (BE) */
    data[9] = 20;
    /* BSS size = 50 (BE) */
    data[13] = 50;
    
    int score = uft_st_prg_probe(data, sizeof(data));
    return score >= 50;
}

static int test_st_prg_parse(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    data[0] = 0x60; data[1] = 0x1A;  /* Magic */
    data[5] = 100;  /* TEXT size = 100 */
    data[9] = 20;   /* DATA size = 20 */
    data[13] = 50;  /* BSS size = 50 */
    
    uft_st_prg_info_t info;
    bool ok = uft_st_prg_parse(data, sizeof(data), &info);
    
    return ok && info.magic == 0x601A &&
           info.text_size == 100 && info.data_size == 20 &&
           info.bss_size == 50 && info.total_memory == 170;
}

static int test_st_prg_big_endian(void) {
    uint8_t data[] = { 0x12, 0x34, 0x56, 0x78 };
    return uft_st_prg_be16(data) == 0x1234 &&
           uft_st_prg_be32(data) == 0x12345678;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRS-80 CMD Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_trs80_record_header_size(void) {
    return sizeof(uft_trs80_record_header_t) == 2;
}

static int test_trs80_length_decode(void) {
    return uft_trs80_decode_length(0) == 256 &&
           uft_trs80_decode_length(1) == 257 &&
           uft_trs80_decode_length(2) == 258 &&
           uft_trs80_decode_length(3) == 3 &&
           uft_trs80_decode_length(100) == 100;
}

static int test_trs80_record_names(void) {
    return strstr(uft_trs80_record_name(UFT_TRS80_REC_OBJECT), "Object") != NULL &&
           strstr(uft_trs80_record_name(UFT_TRS80_REC_TRANSFER), "Transfer") != NULL &&
           strstr(uft_trs80_record_name(UFT_TRS80_REC_END), "End") != NULL;
}

static int test_trs80_probe_valid(void) {
    /* Simple CMD: header + one object record + transfer + end */
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    
    /* Header record */
    data[0] = UFT_TRS80_REC_HEADER;
    data[1] = 4;  /* Length = 4 */
    /* 4 bytes header data */
    
    /* Object code */
    data[6] = UFT_TRS80_REC_OBJECT;
    data[7] = 4;  /* Length = 4 (2 addr + 2 data) */
    data[8] = 0x00; data[9] = 0x50;  /* Addr = $5000 */
    
    /* Transfer */
    data[12] = UFT_TRS80_REC_TRANSFER;
    data[13] = 2;
    data[14] = 0x00; data[15] = 0x50;
    
    int score = uft_trs80_cmd_probe(data, sizeof(data));
    return score >= 40;
}

static int test_trs80_parse(void) {
    uint8_t data[16];
    memset(data, 0, sizeof(data));
    
    /* Single object record: type + len + addr(2) + data(2) */
    data[0] = UFT_TRS80_REC_OBJECT;
    data[1] = 4;  /* length = 4 (2 addr + 2 data) */
    data[2] = 0x00; data[3] = 0x50;  /* Addr = $5000 (LE) */
    data[4] = 0x12; data[5] = 0x34;  /* Code bytes */
    
    /* End record at offset 6 */
    data[6] = UFT_TRS80_REC_END;
    data[7] = 0;  /* Length doesn't matter for END */
    
    uft_trs80_cmd_info_t info;
    bool ok = uft_trs80_cmd_parse(data, sizeof(data), &info);
    
    return ok && info.object_records == 1 && info.has_end;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CoCo CCC Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_coco_standard_sizes(void) {
    return uft_coco_ccc_is_standard_size(UFT_COCO_CCC_SIZE_2K) &&
           uft_coco_ccc_is_standard_size(UFT_COCO_CCC_SIZE_4K) &&
           uft_coco_ccc_is_standard_size(UFT_COCO_CCC_SIZE_8K) &&
           uft_coco_ccc_is_standard_size(UFT_COCO_CCC_SIZE_16K) &&
           !uft_coco_ccc_is_standard_size(5000);
}

static int test_coco_size_names(void) {
    return strcmp(uft_coco_ccc_size_name(UFT_COCO_CCC_SIZE_8K), "8K") == 0 &&
           strstr(uft_coco_ccc_size_name(5000), "Non-standard") != NULL;
}

static int test_coco_big_endian(void) {
    uint8_t data[] = { 0xC0, 0x00 };  /* $C000 */
    return uft_coco_be16(data) == 0xC000;
}

static int test_coco_parse(void) {
    uint8_t data[UFT_COCO_CCC_SIZE_8K];
    memset(data, 0, sizeof(data));
    
    /* Entry point at $C100 (big endian) */
    data[0] = 0xC1;
    data[1] = 0x00;
    
    uft_coco_ccc_info_t info;
    bool ok = uft_coco_ccc_parse(data, sizeof(data), &info);
    
    return ok && info.rom_size == UFT_COCO_CCC_SIZE_8K &&
           info.load_address == 0xC000 &&
           info.entry_point == 0xC100 &&
           info.has_valid_entry && info.is_standard_size;
}

static int test_coco_region_names(void) {
    return strstr(uft_coco_region_name(0x0000), "RAM") != NULL &&
           strstr(uft_coco_region_name(0xC000), "Cartridge") != NULL &&
           strstr(uft_coco_region_name(0xFF00), "I/O") != NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Spectrum Next NEX Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_nex_header_size(void) {
    return sizeof(uft_nex_header_t) == 512;
}

static int test_nex_verify_signature(void) {
    uint8_t valid[512];
    memset(valid, 0, sizeof(valid));
    memcpy(valid, "Next", 4);
    
    uint8_t invalid[512];
    memset(invalid, 0, sizeof(invalid));
    memcpy(invalid, "Nope", 4);
    
    return uft_nex_verify_signature(valid, 512) &&
           !uft_nex_verify_signature(invalid, 512);
}

static int test_nex_probe_valid(void) {
    uint8_t data[512];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "Next", 4);
    memcpy(data + 4, "V1.2", 4);
    data[UFT_NEX_OFF_BORDER_COLOUR] = 1;  /* Blue */
    data[UFT_NEX_OFF_LOADING_SCREEN] = UFT_NEX_SCREEN_ULA;
    data[UFT_NEX_OFF_SP] = 0x00; data[UFT_NEX_OFF_SP + 1] = 0x60;  /* $6000 */
    
    int score = uft_nex_probe(data, sizeof(data));
    return score >= 70;
}

static int test_nex_screen_names(void) {
    return strstr(uft_nex_screen_name(UFT_NEX_SCREEN_NONE), "None") != NULL &&
           strstr(uft_nex_screen_name(UFT_NEX_SCREEN_ULA), "ULA") != NULL &&
           strstr(uft_nex_screen_name(UFT_NEX_SCREEN_LAYER2), "Layer 2") != NULL;
}

static int test_nex_screen_sizes(void) {
    return uft_nex_screen_size(UFT_NEX_SCREEN_NONE) == 0 &&
           uft_nex_screen_size(UFT_NEX_SCREEN_ULA) == 6912 &&
           uft_nex_screen_size(UFT_NEX_SCREEN_LAYER2) == 49152;
}

static int test_nex_parse(void) {
    uint8_t data[512];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "Next", 4);
    memcpy(data + 4, "V1.3", 4);
    data[UFT_NEX_OFF_NUM_BANKS] = 5;
    data[UFT_NEX_OFF_LOADING_SCREEN] = UFT_NEX_SCREEN_LAYER2;
    data[UFT_NEX_OFF_BORDER_COLOUR] = 2;  /* Red */
    data[UFT_NEX_OFF_SP] = 0x00; data[UFT_NEX_OFF_SP + 1] = 0xFF;  /* $FF00 */
    data[UFT_NEX_OFF_PC] = 0x00; data[UFT_NEX_OFF_PC + 1] = 0x80;  /* $8000 */
    
    uft_nex_info_t info;
    bool ok = uft_nex_parse(data, sizeof(data), &info);
    
    return ok && info.version_major == 1 && info.version_minor == 3 &&
           info.num_banks == 5 && info.border_colour == 2 &&
           info.sp == 0xFF00 && info.pc == 0x8000;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== RetroGhidra Format Tests (Phase 2) ===\n\n");
    
    printf("[Commodore D80 Disk]\n");
    TEST(d80_entry_size);
    TEST(d80_file_sizes);
    TEST(d80_sectors_per_track);
    TEST(d80_type_names);
    
    printf("\n[Apple II DOS 3.3 Disk]\n");
    TEST(dos33_entry_size);
    TEST(dos33_file_size);
    TEST(dos33_geometry);
    TEST(dos33_type_names);
    TEST(dos33_sector_offset);
    
    printf("\n[Apple II ProDOS Disk]\n");
    TEST(prodos_entry_size);
    TEST(prodos_file_sizes);
    TEST(prodos_storage_names);
    TEST(prodos_type_names);
    
    printf("\n[Atari 8-bit XEX]\n");
    TEST(xex_segment_header_size);
    TEST(xex_addresses);
    TEST(xex_probe_valid);
    TEST(xex_parse);
    TEST(xex_runad_initad);
    
    printf("\n[Atari ST PRG]\n");
    TEST(st_prg_header_size);
    TEST(st_prg_symbol_size);
    TEST(st_prg_magic);
    TEST(st_prg_probe_valid);
    TEST(st_prg_parse);
    TEST(st_prg_big_endian);
    
    printf("\n[TRS-80 /CMD]\n");
    TEST(trs80_record_header_size);
    TEST(trs80_length_decode);
    TEST(trs80_record_names);
    TEST(trs80_probe_valid);
    TEST(trs80_parse);
    
    printf("\n[CoCo CCC Cartridge]\n");
    TEST(coco_standard_sizes);
    TEST(coco_size_names);
    TEST(coco_big_endian);
    TEST(coco_parse);
    TEST(coco_region_names);
    
    printf("\n[Spectrum Next NEX]\n");
    TEST(nex_header_size);
    TEST(nex_verify_signature);
    TEST(nex_probe_valid);
    TEST(nex_screen_names);
    TEST(nex_screen_sizes);
    TEST(nex_parse);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
