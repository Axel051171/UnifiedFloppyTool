/**
 * @file test_d71.c
 * @brief Tests für D71 Format (Commodore 1571)
 */

#include "../uft_test.h"

#define D71_SECTOR_SIZE     256
#define D71_TRACKS_PER_SIDE 35
#define D71_TOTAL_TRACKS    70
#define D71_SECTORS_SIDE0   683
#define D71_SECTORS_SIDE1   683
#define D71_TOTAL_SECTORS   1366

static const uint8_t d71_spt[35] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

int test_format_d71(void) {
    TEST_BEGIN("D71 Format Tests");
    
    // Größen-Tests
    TEST_ASSERT_EQ(D71_SECTORS_SIDE0 * D71_SECTOR_SIZE, 174848, "Side 0 size");
    TEST_ASSERT_EQ(D71_TOTAL_SECTORS * D71_SECTOR_SIZE, 349696, "D71 total size");
    TEST_ASSERT_EQ(D71_TOTAL_SECTORS + D71_TOTAL_SECTORS, 2732, "D71 with errors sectors");
    
    // Track/Sektor Tests
    int total = 0;
    for (int i = 0; i < D71_TRACKS_PER_SIDE; i++) {
        total += d71_spt[i];
    }
    TEST_ASSERT_EQ(total, D71_SECTORS_SIDE0, "Side 0 sector count");
    TEST_ASSERT_EQ(total * 2, D71_TOTAL_SECTORS, "Total sector count");
    
    // BAM Tests
    // D71 hat BAM auf Track 18 (Seite 0) und Track 53 (Seite 1)
    TEST_ASSERT_EQ(18 + D71_TRACKS_PER_SIDE, 53, "Side 1 BAM track");
    
    // Layout: Seite 0 = Tracks 1-35, Seite 1 = Tracks 36-70
    TEST_ASSERT_EQ(D71_TRACKS_PER_SIDE, 35, "35 tracks per side");
    TEST_ASSERT_EQ(D71_TOTAL_TRACKS, 70, "70 total tracks");
    
    return tests_failed;
}
