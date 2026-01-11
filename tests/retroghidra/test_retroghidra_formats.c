/**
 * @file test_retroghidra_formats.c
 * @brief Tests for formats derived from RetroGhidra analysis
 * - BBC Micro UEF
 * - ZX Spectrum SNA
 * - Amstrad CPC SNA
 * - C64 CRT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/tape/uft_uef_format.h"
#include "uft/snapshot/uft_zx_sna.h"
#include "uft/snapshot/uft_cpc_sna.h"
#include "uft/cartridge/uft_c64_crt.h"

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
 * UEF Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_uef_header_size(void) {
    return sizeof(uft_uef_header_t) == 12;
}

static int test_uef_chunk_header_size(void) {
    return sizeof(uft_uef_chunk_header_t) == 6;
}

static int test_uef_verify_signature(void) {
    uint8_t valid[12] = { 'U', 'E', 'F', ' ', 'F', 'i', 'l', 'e', '!', 0x00, 0, 1 };
    uint8_t invalid[12] = { 'B', 'A', 'D', ' ', 'F', 'i', 'l', 'e', '!', 0x00, 0, 1 };
    
    return uft_uef_verify_signature(valid, 12) &&
           !uft_uef_verify_signature(invalid, 12);
}

static int test_uef_probe_valid(void) {
    uint8_t data[24];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "UEF File!", 9);
    data[9] = 0x00;
    data[10] = 6;   /* minor = 6 */
    data[11] = 0;   /* major = 0 (v0.6) */
    
    /* First chunk */
    data[12] = 0x00; data[13] = 0x01;  /* ID = 0x0100 (data block) */
    data[14] = 4; /* length = 4 */
    
    int score = uft_uef_probe(data, sizeof(data));
    return score >= 70;
}

static int test_uef_probe_invalid(void) {
    uint8_t data[16];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_uef_probe(data, sizeof(data));
    return score == 0;
}

static int test_uef_chunk_names(void) {
    return strstr(uft_uef_chunk_name(UFT_UEF_CHUNK_DATA_BLOCK), "Data") != NULL &&
           strstr(uft_uef_chunk_name(UFT_UEF_CHUNK_CPU_STATE), "CPU") != NULL &&
           strstr(uft_uef_chunk_name(UFT_UEF_CHUNK_RAM), "RAM") != NULL;
}

static int test_uef_machine_names(void) {
    return strstr(uft_uef_machine_name(UFT_UEF_MACHINE_BBC_B), "BBC") != NULL &&
           strstr(uft_uef_machine_name(UFT_UEF_MACHINE_ELECTRON), "Electron") != NULL;
}

static int test_uef_parse_header(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "UEF File!", 9);
    data[9] = 0x00;
    data[10] = 10;  /* minor */
    data[11] = 0;   /* major */
    
    /* Chunk 1: Target machine */
    data[12] = 0x05; data[13] = 0x00;  /* ID = 0x0005 */
    data[14] = 1;  /* length = 1 */
    data[18] = UFT_UEF_MACHINE_BBC_B;  /* BBC Model B */
    
    /* Chunk 2: Data block */
    data[19] = 0x00; data[20] = 0x01;  /* ID = 0x0100 */
    data[21] = 2;  /* length = 2 */
    
    uft_uef_file_info_t info;
    bool ok = uft_uef_parse_header(data, 27, &info);
    
    return ok && info.version_major == 0 && info.version_minor == 10 &&
           info.chunk_count == 2 && info.data_chunks >= 1 &&
           info.target_machine == UFT_UEF_MACHINE_BBC_B;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ZX Spectrum SNA Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_zx_sna_header_size(void) {
    return sizeof(uft_zx_sna_header_t) == 27;
}

static int test_zx_sna_128k_ext_size(void) {
    return sizeof(uft_zx_sna_128k_ext_t) == 4;
}

static int test_zx_sna_file_sizes(void) {
    return UFT_ZX_SNA_SIZE_48K == 49179 &&
           UFT_ZX_SNA_SIZE_128K_SHORT == 131103 &&
           UFT_ZX_SNA_SIZE_128K_LONG == 147487;
}

static int test_zx_sna_probe_48k(void) {
    /* Create minimal 48K SNA */
    uint8_t* data = (uint8_t*)calloc(UFT_ZX_SNA_SIZE_48K, 1);
    if (!data) return 0;
    
    /* Set valid header fields */
    data[UFT_ZX_SNA_OFF_IFF2] = 0x04;      /* IFF2 enabled */
    data[UFT_ZX_SNA_OFF_SP] = 0x00;        /* SP low */
    data[UFT_ZX_SNA_OFF_SP + 1] = 0x60;    /* SP = 0x6000 */
    data[UFT_ZX_SNA_OFF_INT_MODE] = 1;     /* IM 1 */
    data[UFT_ZX_SNA_OFF_BORDER] = 7;       /* White border */
    
    int score = uft_zx_sna_probe(data, UFT_ZX_SNA_SIZE_48K);
    free(data);
    
    return score >= 70;
}

static int test_zx_sna_probe_invalid(void) {
    uint8_t data[1000];
    memset(data, 0xFF, sizeof(data));
    
    /* Invalid IFF2 */
    data[UFT_ZX_SNA_OFF_IFF2] = 0xFF;
    
    int score = uft_zx_sna_probe(data, sizeof(data));
    return score == 0;
}

static int test_zx_sna_parse_header(void) {
    uint8_t* data = (uint8_t*)calloc(UFT_ZX_SNA_SIZE_48K, 1);
    if (!data) return 0;
    
    /* Set header */
    data[UFT_ZX_SNA_OFF_IFF2] = 0x04;
    data[UFT_ZX_SNA_OFF_SP] = 0x00;
    data[UFT_ZX_SNA_OFF_SP + 1] = 0x60;    /* SP = 0x6000 */
    data[UFT_ZX_SNA_OFF_INT_MODE] = 1;
    data[UFT_ZX_SNA_OFF_BORDER] = 2;       /* Red */
    
    /* PC on stack (at SP in RAM dump) */
    size_t sp_offset = UFT_ZX_SNA_HEADER_SIZE + (0x6000 - UFT_ZX_SNA_RAM_START);
    data[sp_offset] = 0x34;     /* PC low */
    data[sp_offset + 1] = 0x12; /* PC = 0x1234 */
    
    uft_zx_sna_file_info_t info;
    bool ok = uft_zx_sna_parse_header(data, UFT_ZX_SNA_SIZE_48K, &info);
    free(data);
    
    return ok && info.is_48k && info.sp == 0x6000 && info.pc == 0x1234 &&
           info.int_mode == 1 && info.border == 2;
}

static int test_zx_sna_border_names(void) {
    return strcmp(uft_zx_sna_border_name(0), "Black") == 0 &&
           strcmp(uft_zx_sna_border_name(1), "Blue") == 0 &&
           strcmp(uft_zx_sna_border_name(7), "White") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Amstrad CPC SNA Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_cpc_sna_header_size(void) {
    return sizeof(uft_cpc_sna_header_t) == 256;
}

static int test_cpc_sna_file_sizes(void) {
    return UFT_CPC_SNA_SIZE_64K == 65792 &&
           UFT_CPC_SNA_SIZE_128K == 131328;
}

static int test_cpc_sna_verify_signature(void) {
    uint8_t valid[256];
    memset(valid, 0, sizeof(valid));
    memcpy(valid, "MV - SNA", 8);
    
    uint8_t invalid[256];
    memset(invalid, 0, sizeof(invalid));
    memcpy(invalid, "BAD SIG!", 8);
    
    return uft_cpc_sna_verify_signature(valid, 256) &&
           !uft_cpc_sna_verify_signature(invalid, 256);
}

static int test_cpc_sna_probe_valid(void) {
    uint8_t* data = (uint8_t*)calloc(UFT_CPC_SNA_SIZE_64K, 1);
    if (!data) return 0;
    
    memcpy(data, "MV - SNA", 8);
    data[UFT_CPC_SNA_OFF_VERSION] = 1;
    data[UFT_CPC_SNA_OFF_CPC_TYPE] = UFT_CPC_TYPE_6128;
    data[UFT_CPC_SNA_OFF_INT_MODE] = 1;
    data[UFT_CPC_SNA_OFF_DUMP_SIZE] = 64;  /* 64K */
    
    int score = uft_cpc_sna_probe(data, UFT_CPC_SNA_SIZE_64K);
    free(data);
    
    return score >= 80;
}

static int test_cpc_sna_probe_invalid(void) {
    uint8_t data[256];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_cpc_sna_probe(data, sizeof(data));
    return score == 0;
}

static int test_cpc_sna_parse_header(void) {
    uint8_t* data = (uint8_t*)calloc(UFT_CPC_SNA_SIZE_64K, 1);
    if (!data) return 0;
    
    memcpy(data, "MV - SNA", 8);
    data[UFT_CPC_SNA_OFF_VERSION] = 2;
    data[UFT_CPC_SNA_OFF_CPC_TYPE] = UFT_CPC_TYPE_464;
    data[UFT_CPC_SNA_OFF_INT_MODE] = 1;
    data[UFT_CPC_SNA_OFF_SP] = 0x00;
    data[UFT_CPC_SNA_OFF_SP + 1] = 0xC0;   /* SP = 0xC000 */
    data[UFT_CPC_SNA_OFF_PC] = 0x00;
    data[UFT_CPC_SNA_OFF_PC + 1] = 0x40;   /* PC = 0x4000 */
    data[UFT_CPC_SNA_OFF_DUMP_SIZE] = 64;
    
    uft_cpc_sna_file_info_t info;
    bool ok = uft_cpc_sna_parse_header(data, UFT_CPC_SNA_SIZE_64K, &info);
    free(data);
    
    return ok && info.version == 2 && info.cpc_type == UFT_CPC_TYPE_464 &&
           info.sp == 0xC000 && info.pc == 0x4000 && info.is_64k;
}

static int test_cpc_sna_type_names(void) {
    return strstr(uft_cpc_sna_type_name(UFT_CPC_TYPE_464), "464") != NULL &&
           strstr(uft_cpc_sna_type_name(UFT_CPC_TYPE_6128), "6128") != NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 CRT Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_crt_header_size(void) {
    return sizeof(uft_crt_header_t) == 64;
}

static int test_crt_chip_header_size(void) {
    return sizeof(uft_crt_chip_header_t) == 16;
}

static int test_crt_verify_signature(void) {
    uint8_t valid[64];
    memset(valid, 0, sizeof(valid));
    memcpy(valid, "C64 CARTRIDGE   ", 16);
    
    uint8_t invalid[64];
    memset(invalid, 0, sizeof(invalid));
    memcpy(invalid, "NOT A CARTRIDGE!", 16);
    
    return uft_crt_verify_signature(valid, 64) &&
           !uft_crt_verify_signature(invalid, 64);
}

static int test_crt_probe_valid(void) {
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "C64 CARTRIDGE   ", 16);
    /* Header length (BE) = 64 */
    data[16] = 0; data[17] = 0; data[18] = 0; data[19] = 0x40;
    /* Version (BE) = 1.0 */
    data[20] = 0x01; data[21] = 0x00;
    /* Type (BE) = 0 (normal) */
    data[22] = 0; data[23] = 0;
    
    /* CHIP packet */
    memcpy(data + 64, "CHIP", 4);
    /* Packet length (BE) = 32 */
    data[68] = 0; data[69] = 0; data[70] = 0; data[71] = 32;
    
    int score = uft_crt_probe(data, sizeof(data));
    return score >= 80;
}

static int test_crt_probe_invalid(void) {
    uint8_t data[64];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_crt_probe(data, sizeof(data));
    return score == 0;
}

static int test_crt_parse_header(void) {
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "C64 CARTRIDGE   ", 16);
    /* Header length = 64 */
    data[16] = 0; data[17] = 0; data[18] = 0; data[19] = 0x40;
    /* Version = 1.0 */
    data[20] = 0x01; data[21] = 0x00;
    /* Type = 32 (EasyFlash) */
    data[22] = 0; data[23] = 32;
    /* EXROM = 1, GAME = 0 */
    data[24] = 1; data[25] = 0;
    /* Name */
    memcpy(data + 32, "Test Cartridge", 14);
    
    /* CHIP packet */
    memcpy(data + 64, "CHIP", 4);
    /* Packet length = 32 */
    data[68] = 0; data[69] = 0; data[70] = 0; data[71] = 32;
    /* Chip type = 0 (ROM) */
    data[72] = 0; data[73] = 0;
    /* Bank = 0 */
    data[74] = 0; data[75] = 0;
    /* Load address = $8000 */
    data[76] = 0x80; data[77] = 0x00;
    /* ROM size = 16 */
    data[78] = 0; data[79] = 16;
    
    uft_crt_file_info_t info;
    bool ok = uft_crt_parse_header(data, 96, &info);
    
    return ok && info.version == 0x0100 && info.type == 32 &&
           info.exrom == 1 && info.game == 0 &&
           strstr(info.name, "Test") != NULL &&
           info.chip_count == 1;
}

static int test_crt_cbm80_check(void) {
    /* ROM with CBM80 signature */
    uint8_t rom_with_cbm80[16] = {
        0x00, 0x00, 0x00, 0x00,         /* Offset 0-3 */
        0xC3, 0xC2, 0xCD, '8', '0',     /* CBM80 at offset 4 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    uint8_t rom_without_cbm80[16] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    return uft_crt_has_cbm80(rom_with_cbm80, 16) &&
           !uft_crt_has_cbm80(rom_without_cbm80, 16);
}

static int test_crt_type_names(void) {
    return strstr(uft_crt_type_name(UFT_CRT_TYPE_NORMAL), "Normal") != NULL &&
           strstr(uft_crt_type_name(UFT_CRT_TYPE_EASYFLASH), "EasyFlash") != NULL &&
           strstr(uft_crt_type_name(UFT_CRT_TYPE_ACTION_REPLAY), "Action Replay") != NULL;
}

static int test_crt_big_endian(void) {
    uint8_t data[] = { 0x12, 0x34, 0x56, 0x78 };
    
    return uft_crt_be16(data) == 0x1234 &&
           uft_crt_be32(data) == 0x12345678;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== RetroGhidra Format Tests ===\n\n");
    
    printf("[BBC Micro UEF]\n");
    TEST(uef_header_size);
    TEST(uef_chunk_header_size);
    TEST(uef_verify_signature);
    TEST(uef_probe_valid);
    TEST(uef_probe_invalid);
    TEST(uef_chunk_names);
    TEST(uef_machine_names);
    TEST(uef_parse_header);
    
    printf("\n[ZX Spectrum SNA]\n");
    TEST(zx_sna_header_size);
    TEST(zx_sna_128k_ext_size);
    TEST(zx_sna_file_sizes);
    TEST(zx_sna_probe_48k);
    TEST(zx_sna_probe_invalid);
    TEST(zx_sna_parse_header);
    TEST(zx_sna_border_names);
    
    printf("\n[Amstrad CPC SNA]\n");
    TEST(cpc_sna_header_size);
    TEST(cpc_sna_file_sizes);
    TEST(cpc_sna_verify_signature);
    TEST(cpc_sna_probe_valid);
    TEST(cpc_sna_probe_invalid);
    TEST(cpc_sna_parse_header);
    TEST(cpc_sna_type_names);
    
    printf("\n[C64 CRT Cartridge]\n");
    TEST(crt_header_size);
    TEST(crt_chip_header_size);
    TEST(crt_verify_signature);
    TEST(crt_probe_valid);
    TEST(crt_probe_invalid);
    TEST(crt_parse_header);
    TEST(crt_cbm80_check);
    TEST(crt_type_names);
    TEST(crt_big_endian);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
