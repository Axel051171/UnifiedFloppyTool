/**
 * @file test_adf.c
 * @brief Tests für ADF Format (Amiga)
 */

#include "../uft_test.h"

#define ADF_DD_CYLINDERS    80
#define ADF_DD_HEADS        2
#define ADF_DD_SECTORS      11
#define ADF_DD_SECTOR_SIZE  512
#define ADF_DD_SIZE         901120

#define ADF_HD_CYLINDERS    80
#define ADF_HD_HEADS        2
#define ADF_HD_SECTORS      22
#define ADF_HD_SECTOR_SIZE  512
#define ADF_HD_SIZE         1802240

int test_format_adf(void) {
    TEST_BEGIN("ADF Format Tests");
    
    // DD Geometrie
    int dd_sectors = ADF_DD_CYLINDERS * ADF_DD_HEADS * ADF_DD_SECTORS;
    TEST_ASSERT_EQ(dd_sectors, 1760, "DD = 80×2×11 = 1760 sectors");
    TEST_ASSERT_EQ(dd_sectors * ADF_DD_SECTOR_SIZE, ADF_DD_SIZE, "DD size = 901120");
    
    // HD Geometrie
    int hd_sectors = ADF_HD_CYLINDERS * ADF_HD_HEADS * ADF_HD_SECTORS;
    TEST_ASSERT_EQ(hd_sectors, 3520, "HD = 80×2×22 = 3520 sectors");
    TEST_ASSERT_EQ(hd_sectors * ADF_HD_SECTOR_SIZE, ADF_HD_SIZE, "HD size = 1802240");
    
    // Boot Block
    TEST_ASSERT_EQ(2 * ADF_DD_SECTOR_SIZE, 1024, "Boot block = 1024 bytes");
    
    // Amiga Bootblock Magic
    uint8_t dos_magic[] = { 'D', 'O', 'S', 0x00 };
    TEST_ASSERT(memcmp(dos_magic, "DOS", 3) == 0, "DOS magic at offset 0");
    
    // OFS vs FFS
    // DOS\0 = OFS, DOS\1 = FFS, DOS\2 = OFS intl, DOS\3 = FFS intl
    TEST_ASSERT(dos_magic[3] == 0x00, "DOS type byte at offset 3");
    
    return tests_failed;
}
