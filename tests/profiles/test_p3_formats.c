/**
 * @file test_p3_formats.c
 * @brief Unit tests for P3 format profiles (FDI, DIM, ATR, TRD, MSX, 86F, KFX, MFI, DSK, ST)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/profiles/uft_fdi_format.h"
#include "uft/profiles/uft_dim_format.h"
#include "uft/profiles/uft_atr_format.h"
#include "uft/profiles/uft_trd_format.h"
#include "uft/profiles/uft_msx_format.h"
#include "uft/profiles/uft_86f_format.h"
#include "uft/profiles/uft_kfx_format.h"
#include "uft/profiles/uft_mfi_format.h"
#include "uft/profiles/uft_dsk_format.h"
#include "uft/profiles/uft_st_format.h"

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
 * FDI Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_fdi_signature(void) {
    uint8_t valid[14] = {'F', 'D', 'I', 0};
    uint8_t invalid[14] = {'X', 'X', 'X', 0};
    
    return uft_fdi_validate_signature(valid, 14) &&
           !uft_fdi_validate_signature(invalid, 14);
}

static int test_fdi_size_codes(void) {
    return uft_fdi_size_code_to_bytes(0) == 128 &&
           uft_fdi_size_code_to_bytes(1) == 256 &&
           uft_fdi_size_code_to_bytes(2) == 512;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DIM Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_dim_media_types(void) {
    const uft_dim_geometry_t* geom = uft_dim_get_geometry(UFT_DIM_MEDIA_2HD);
    return geom != NULL && geom->cylinders == 77;
}

static int test_dim_signature(void) {
    uint8_t valid[256] = {0};
    valid[UFT_DIM_SIGNATURE_POS] = 0x00;  /* Valid signature */
    
    return uft_dim_validate(valid, 256);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ATR Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_atr_signature(void) {
    uint8_t valid[16] = {UFT_ATR_MAGIC_LO, UFT_ATR_MAGIC_HI, 0};
    uint8_t invalid[16] = {0xFF, 0xFF, 0};
    
    return uft_atr_validate_signature(valid, 16) &&
           !uft_atr_validate_signature(invalid, 16);
}

static int test_atr_type_detection(void) {
    return uft_atr_detect_type(UFT_ATR_SIZE_SSSD, 128) == UFT_ATR_TYPE_SSSD &&
           uft_atr_detect_type(UFT_ATR_SIZE_DSDD, 256) == UFT_ATR_TYPE_DSDD;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRD Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_trd_disk_types(void) {
    uint8_t tracks, sides;
    
    uft_trd_decode_disk_type(UFT_TRD_TYPE_80_2, &tracks, &sides);
    if (tracks != 80 || sides != 2) return 0;
    
    uft_trd_decode_disk_type(UFT_TRD_TYPE_40_1, &tracks, &sides);
    if (tracks != 40 || sides != 1) return 0;
    
    return 1;
}

static int test_trd_file_types(void) {
    return strcmp(uft_trd_file_type_name(UFT_TRD_FILE_BASIC), "BASIC") == 0 &&
           strcmp(uft_trd_file_type_name(UFT_TRD_FILE_CODE), "Code") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MSX Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_msx_sizes(void) {
    return UFT_MSX_SIZE_1DD == 368640 &&
           UFT_MSX_SIZE_2DD == 737280;
}

static int test_msx_types(void) {
    return strcmp(uft_msx_type_name(UFT_MSX_TYPE_2DD_DS), "2DD Double-Sided (720KB)") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 86F Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_86f_signature(void) {
    uint8_t valid[8] = {'8', '6', 'B', 'F', 0};
    uint8_t invalid[8] = {'X', 'X', 'X', 'X', 0};
    
    return uft_86f_validate_signature(valid, 8) &&
           !uft_86f_validate_signature(invalid, 8);
}

static int test_86f_bitrate(void) {
    return uft_86f_get_bitrate(UFT_86F_FLAG_BITRATE_250) == 250 &&
           uft_86f_get_bitrate(UFT_86F_FLAG_BITRATE_500) == 500;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * KFX Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_kfx_opcodes(void) {
    return uft_kfx_is_flux_opcode(0x00) &&     /* Flux2 */
           uft_kfx_is_flux_opcode(0x0C) &&     /* Flux3 */
           uft_kfx_is_flux_opcode(0xFF) &&     /* Flux1 */
           !uft_kfx_is_flux_opcode(0x0D);      /* OOB */
}

static int test_kfx_timing(void) {
    double ns = uft_kfx_ticks_to_ns(1000);
    /* ~208ns per tick, so 1000 ticks ≈ 208000 ns */
    return ns > 200000.0 && ns < 220000.0;
}

static int test_kfx_filename_parse(void) {
    uint8_t track, side;
    
    if (!uft_kfx_parse_filename("track00.0.raw", &track, &side)) return 0;
    if (track != 0 || side != 0) return 0;
    
    if (!uft_kfx_parse_filename("track35.1.raw", &track, &side)) return 0;
    if (track != 35 || side != 1) return 0;
    
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MFI Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_mfi_signature(void) {
    uint8_t v1[16] = {'M', 'A', 'M', 'E', 'F', 'L', 'O', 'P', 0};
    uint8_t v2[16] = {'M', 'F', 'I', '2', 0};
    
    return uft_mfi_is_v1(v1, 16) &&
           uft_mfi_is_v2(v2, 16);
}

static int test_mfi_mg_codes(void) {
    uint32_t cell = uft_mfi_make_cell(UFT_MFI_MG_A, 1000);
    
    return uft_mfi_get_mg_code(cell) == UFT_MFI_MG_A &&
           uft_mfi_get_time(cell) == 1000;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DSK Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_dsk_sizes(void) {
    return UFT_DSK_SIZE_APPLE_140K == 143360 &&
           UFT_DSK_SIZE_CPM_720K == 737280;
}

static int test_dsk_geometry(void) {
    const uft_dsk_geometry_t* geom = uft_dsk_find_geometry(143360);
    return geom != NULL && geom->platform == UFT_DSK_PLATFORM_APPLE_DOS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ST Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_st_sizes(void) {
    return UFT_ST_SIZE_360K == 368640 &&
           UFT_ST_SIZE_720K == 737280 &&
           UFT_ST_SIZE_1440K == 1474560;
}

static int test_st_geometry(void) {
    const uft_st_geometry_t* geom = uft_st_find_geometry(737280);
    return geom != NULL && geom->type == UFT_ST_TYPE_DSDD_9;
}

static int test_st_type_names(void) {
    return strstr(uft_st_type_name(UFT_ST_TYPE_DSDD_9), "DS/DD") != NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== P3 Format Profile Tests ===\n\n");
    
    printf("[FDI Format]\n");
    TEST(fdi_signature);
    TEST(fdi_size_codes);
    
    printf("\n[DIM Format]\n");
    TEST(dim_media_types);
    TEST(dim_signature);
    
    printf("\n[ATR Format]\n");
    TEST(atr_signature);
    TEST(atr_type_detection);
    
    printf("\n[TRD Format]\n");
    TEST(trd_disk_types);
    TEST(trd_file_types);
    
    printf("\n[MSX Format]\n");
    TEST(msx_sizes);
    TEST(msx_types);
    
    printf("\n[86F Format]\n");
    TEST(86f_signature);
    TEST(86f_bitrate);
    
    printf("\n[KFX Format]\n");
    TEST(kfx_opcodes);
    TEST(kfx_timing);
    TEST(kfx_filename_parse);
    
    printf("\n[MFI Format]\n");
    TEST(mfi_signature);
    TEST(mfi_mg_codes);
    
    printf("\n[DSK Format]\n");
    TEST(dsk_sizes);
    TEST(dsk_geometry);
    
    printf("\n[ST Format]\n");
    TEST(st_sizes);
    TEST(st_geometry);
    TEST(st_type_names);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
