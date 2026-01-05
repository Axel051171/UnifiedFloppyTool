/**
 * @file test_td0.c
 * @brief Tests for TD0 format support
 */

#include "uft_td0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test signature detection */
static void test_detection(void)
{
    uint8_t normal_sig[] = { 0x54, 0x44 };  /* "TD" */
    uint8_t adv_sig[] = { 0x74, 0x64 };     /* "td" */
    uint8_t invalid[] = { 0x00, 0x00 };
    
    assert(uft_td0_detect(normal_sig, 2) == true);
    assert(uft_td0_detect(adv_sig, 2) == true);
    assert(uft_td0_detect(invalid, 2) == false);
    assert(uft_td0_detect(NULL, 0) == false);
    
    printf("  Detection: PASS\n");
}

/* Test compression check */
static void test_compression_check(void)
{
    uft_td0_header_t header;
    
    header.signature = UFT_TD0_SIG_NORMAL;
    assert(uft_td0_is_compressed(&header) == false);
    
    header.signature = UFT_TD0_SIG_ADVANCED;
    assert(uft_td0_is_compressed(&header) == true);
    
    printf("  Compression check: PASS\n");
}

/* Test drive type names */
static void test_drive_names(void)
{
    const char* name;
    
    name = uft_td0_drive_name(UFT_TD0_DRIVE_525_96);
    assert(name != NULL);
    assert(strstr(name, "5.25") != NULL);
    
    name = uft_td0_drive_name(UFT_TD0_DRIVE_35_HD);
    assert(name != NULL);
    assert(strstr(name, "3.5") != NULL);
    
    name = uft_td0_drive_name(UFT_TD0_DRIVE_8INCH);
    assert(name != NULL);
    assert(strstr(name, "8") != NULL);
    
    printf("  Drive names: PASS\n");
}

/* Test LZSS initialization */
static void test_lzss_init(void)
{
    uft_td0_lzss_state_t state;
    uint8_t dummy_data[] = { 0x00, 0x00, 0x00, 0x00 };
    
    uft_td0_lzss_init(&state, dummy_data, sizeof(dummy_data));
    
    assert(state.input == dummy_data);
    assert(state.input_size == sizeof(dummy_data));
    assert(state.input_pos == 0);
    assert(state.eof == false);
    assert(state.r == UFT_TD0_LZSS_SBSIZE - UFT_TD0_LZSS_LASIZE);
    
    /* Ring buffer should be initialized with spaces */
    assert(state.ring_buff[0] == ' ');
    
    printf("  LZSS init: PASS\n");
}

/* Test sector decoding */
static void test_sector_decode(void)
{
    uint8_t src[4];
    uint8_t dst[10];
    int result;
    
    /* Test raw encoding (method 0) */
    memset(src, 0xAA, sizeof(src));
    memset(dst, 0, sizeof(dst));
    result = uft_td0_decode_sector(src, 4, dst, 4, UFT_TD0_ENC_RAW);
    assert(result == 4);
    assert(dst[0] == 0xAA);
    
    printf("  Sector decode: PASS\n");
}

/* Test image initialization */
static void test_image_init(void)
{
    uft_td0_image_t img;
    
    int result = uft_td0_init(&img);
    assert(result == 0);
    assert(img.num_tracks == 0);
    assert(img.tracks == NULL);
    assert(img.comment == NULL);
    
    uft_td0_free(&img);
    
    printf("  Image init: PASS\n");
}

int main(int argc, char* argv[])
{
    printf("Testing TD0 format support...\n\n");
    
    test_detection();
    test_compression_check();
    test_drive_names();
    test_lzss_init();
    test_sector_decode();
    test_image_init();
    
    printf("\nAll TD0 tests passed!\n");
    return 0;
}
