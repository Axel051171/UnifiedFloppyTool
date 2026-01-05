/**
 * @file test_dmk.c
 * @brief Tests for DMK format support
 */

#include "uft_dmk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test CRC-16 calculation */
static void test_crc16(void)
{
    uint8_t data1[] = { 0xA1, 0xA1, 0xA1 };
    uint16_t crc;
    
    crc = uft_dmk_crc16(data1, 3, 0xFFFF);
    /* CRC of A1 A1 A1 should be 0xCDB4 */
    assert(crc == UFT_DMK_CRC_A1A1A1);
    
    printf("  CRC-16: PASS\n");
}

/* Test format detection */
static void test_detection(void)
{
    uint8_t valid_dmk[16] = {0};
    
    /* Set up valid header */
    valid_dmk[0] = 0x00;  /* write protect */
    valid_dmk[1] = 40;    /* tracks */
    valid_dmk[2] = 0x00;  /* track length low */
    valid_dmk[3] = 0x19;  /* track length high (0x1900 = 6400) */
    valid_dmk[4] = 0x00;  /* flags */
    
    assert(uft_dmk_detect(valid_dmk, 16) == true);
    
    /* Invalid - zero tracks */
    valid_dmk[1] = 0;
    assert(uft_dmk_detect(valid_dmk, 16) == false);
    
    /* Invalid - too many tracks */
    valid_dmk[1] = 255;
    assert(uft_dmk_detect(valid_dmk, 16) == false);
    
    printf("  Detection: PASS\n");
}

/* Test image initialization */
static void test_image_init(void)
{
    uft_dmk_image_t img;
    
    int result = uft_dmk_init(&img);
    assert(result == 0);
    assert(img.num_tracks == 0);
    assert(img.tracks == NULL);
    
    uft_dmk_free(&img);
    
    printf("  Image init: PASS\n");
}

/* Test header flags */
static void test_flags(void)
{
    /* Test single-sided flag */
    assert((UFT_DMK_FLAG_SS & 0x10) == 0x10);
    
    /* Test single-density flag */
    assert((UFT_DMK_FLAG_SD & 0x40) == 0x40);
    
    /* Test ignore density flag */
    assert((UFT_DMK_FLAG_IGNDEN & 0x80) == 0x80);
    
    printf("  Flags: PASS\n");
}

/* Test IDAM pointer handling */
static void test_idam_pointers(void)
{
    /* Test SD flag extraction */
    uint16_t ptr = 0x8100;  /* SD flag set, offset 0x100 */
    
    bool sd = (ptr & UFT_DMK_IDAM_SD_FLAG) != 0;
    uint16_t offset = ptr & UFT_DMK_IDAM_MASK;
    
    assert(sd == true);
    assert(offset == 0x100);
    
    /* Test without SD flag */
    ptr = 0x0200;
    sd = (ptr & UFT_DMK_IDAM_SD_FLAG) != 0;
    offset = ptr & UFT_DMK_IDAM_MASK;
    
    assert(sd == false);
    assert(offset == 0x200);
    
    printf("  IDAM pointers: PASS\n");
}

/* Test address marks */
static void test_address_marks(void)
{
    assert(UFT_DMK_MFM_IDAM == 0xFE);
    assert(UFT_DMK_MFM_DAM == 0xFB);
    assert(UFT_DMK_MFM_DDAM == 0xF8);
    assert(UFT_DMK_MFM_SYNC == 0xA1);
    
    printf("  Address marks: PASS\n");
}

int main(int argc, char* argv[])
{
    printf("Testing DMK format support...\n\n");
    
    test_crc16();
    test_detection();
    test_image_init();
    test_flags();
    test_idam_pointers();
    test_address_marks();
    
    printf("\nAll DMK tests passed!\n");
    return 0;
}
