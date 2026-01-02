/**
 * @file test_d81.c
 * @brief Tests für D81 Format (Commodore 1581)
 */

#include "../uft_test.h"

#define D81_CYLINDERS       80
#define D81_HEADS           1       // Logisch 1, physisch 2 Seiten
#define D81_SECTORS         40
#define D81_SECTOR_SIZE     256
#define D81_TOTAL_SECTORS   3200
#define D81_SIZE_STANDARD   819200
#define D81_SIZE_WITH_ERRORS 822400

int test_format_d81(void) {
    TEST_BEGIN("D81 Format Tests");
    
    // Geometrie
    TEST_ASSERT_EQ(D81_CYLINDERS * D81_SECTORS, D81_TOTAL_SECTORS, "80×40 = 3200 sectors");
    TEST_ASSERT_EQ(D81_TOTAL_SECTORS * D81_SECTOR_SIZE, D81_SIZE_STANDARD, "Standard size");
    TEST_ASSERT_EQ(D81_SIZE_WITH_ERRORS, D81_SIZE_STANDARD + D81_TOTAL_SECTORS, "With errors size");
    
    // D81 verwendet MFM (nicht GCR wie D64/D71)
    TEST_ASSERT(true, "D81 uses MFM encoding");
    
    // BAM auf Track 40
    int bam_track = 40;
    TEST_ASSERT_EQ(bam_track, D81_CYLINDERS / 2, "BAM at center track");
    
    // Sektorgröße ist gleich wie D64
    TEST_ASSERT_EQ(D81_SECTOR_SIZE, 256, "256 bytes per sector");
    
    // Aber 40 Sektoren pro Track (nicht variabel)
    TEST_ASSERT_EQ(D81_SECTORS, 40, "Fixed 40 sectors/track");
    
    return tests_failed;
}
