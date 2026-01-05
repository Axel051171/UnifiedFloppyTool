/**
 * @file test_imd.c
 * @brief Tests for IMD format support
 */

#include "uft_imd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test header parsing */
static void test_header_parsing(void)
{
    uft_imd_header_t header;
    
    /* Test valid header */
    int result = uft_imd_parse_header("IMD 1.18: 15/06/2024 12:30:45", &header);
    assert(result == 0);
    assert(header.version_major == 1);
    assert(header.version_minor == 18);
    assert(header.day == 15);
    assert(header.month == 6);
    assert(header.year == 2024);
    assert(header.hour == 12);
    assert(header.minute == 30);
    assert(header.second == 45);
    
    /* Test invalid header */
    result = uft_imd_parse_header("INVALID HEADER", &header);
    assert(result != 0);
    
    printf("  Header parsing: PASS\n");
}

/* Test mode functions */
static void test_mode_functions(void)
{
    /* Test rate conversion */
    assert(uft_imd_mode_to_rate(UFT_IMD_MODE_500K_FM) == 500);
    assert(uft_imd_mode_to_rate(UFT_IMD_MODE_300K_FM) == 300);
    assert(uft_imd_mode_to_rate(UFT_IMD_MODE_250K_FM) == 250);
    assert(uft_imd_mode_to_rate(UFT_IMD_MODE_500K_MFM) == 500);
    assert(uft_imd_mode_to_rate(UFT_IMD_MODE_300K_MFM) == 300);
    assert(uft_imd_mode_to_rate(UFT_IMD_MODE_250K_MFM) == 250);
    
    /* Test MFM detection */
    assert(uft_imd_mode_is_mfm(UFT_IMD_MODE_500K_FM) == false);
    assert(uft_imd_mode_is_mfm(UFT_IMD_MODE_500K_MFM) == true);
    
    printf("  Mode functions: PASS\n");
}

/* Test sector size conversion */
static void test_sector_size(void)
{
    assert(uft_imd_ssize_to_bytes(0) == 128);
    assert(uft_imd_ssize_to_bytes(1) == 256);
    assert(uft_imd_ssize_to_bytes(2) == 512);
    assert(uft_imd_ssize_to_bytes(3) == 1024);
    assert(uft_imd_ssize_to_bytes(4) == 2048);
    assert(uft_imd_ssize_to_bytes(5) == 4096);
    assert(uft_imd_ssize_to_bytes(6) == 8192);
    
    assert(uft_imd_bytes_to_ssize(128) == 0);
    assert(uft_imd_bytes_to_ssize(512) == 2);
    assert(uft_imd_bytes_to_ssize(1024) == 3);
    assert(uft_imd_bytes_to_ssize(999) == 0xFF);
    
    printf("  Sector size: PASS\n");
}

/* Test sector type functions */
static void test_sector_types(void)
{
    /* Test data present */
    assert(uft_imd_sec_has_data(UFT_IMD_SEC_UNAVAIL) == false);
    assert(uft_imd_sec_has_data(UFT_IMD_SEC_NORMAL) == true);
    assert(uft_imd_sec_has_data(UFT_IMD_SEC_COMPRESSED) == true);
    
    /* Test compressed */
    assert(uft_imd_sec_is_compressed(UFT_IMD_SEC_NORMAL) == false);
    assert(uft_imd_sec_is_compressed(UFT_IMD_SEC_COMPRESSED) == true);
    assert(uft_imd_sec_is_compressed(UFT_IMD_SEC_DEL_COMP) == true);
    
    /* Test deleted */
    assert(uft_imd_sec_is_deleted(UFT_IMD_SEC_NORMAL) == false);
    assert(uft_imd_sec_is_deleted(UFT_IMD_SEC_DELETED) == true);
    assert(uft_imd_sec_is_deleted(UFT_IMD_SEC_DEL_ERR_COMP) == true);
    
    /* Test error */
    assert(uft_imd_sec_has_error(UFT_IMD_SEC_NORMAL) == false);
    assert(uft_imd_sec_has_error(UFT_IMD_SEC_ERROR) == true);
    
    printf("  Sector types: PASS\n");
}

/* Test gap length lookup */
static void test_gap_lengths(void)
{
    uint8_t gap_rw, gap_fmt;
    
    /* Test 720K format (9 sectors, 512 bytes, MFM) */
    int result = uft_imd_get_gap_lengths(UFT_IMD_MODE_250K_MFM, 2, 9, 
                                          &gap_rw, &gap_fmt);
    assert(result == 0);
    assert(gap_rw > 0);
    assert(gap_fmt > 0);
    
    /* Test 1.44MB format (18 sectors, 512 bytes, MFM) */
    result = uft_imd_get_gap_lengths(UFT_IMD_MODE_500K_MFM, 2, 18,
                                      &gap_rw, &gap_fmt);
    assert(result == 0);
    
    printf("  Gap lengths: PASS\n");
}

int main(int argc, char* argv[])
{
    printf("Testing IMD format support...\n\n");
    
    test_header_parsing();
    test_mode_functions();
    test_sector_size();
    test_sector_types();
    test_gap_lengths();
    
    printf("\nAll IMD tests passed!\n");
    return 0;
}
