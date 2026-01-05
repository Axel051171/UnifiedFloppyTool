/**
 * @file test_d64.c
 * @brief Tests für D64 Format
 */

#include "../uft_test.h"

#define D64_SECTOR_SIZE 256
#define D64_BAM_TRACK 18
#define D64_DIR_TRACK 18

static const uint8_t d64_spt[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17
};

static const uint16_t d64_track_offset[42] = {
    0,21,42,63,84,105,126,147,168,189,210,231,252,273,294,315,336,
    357,376,395,414,433,452,471,490,508,526,544,562,580,598,
    615,632,649,666,683,700,717,734,751,768
};

// Berechne Offset für Track/Sektor
static size_t d64_offset(int track, int sector) {
    if (track < 1 || track > 40) return 0;
    if (sector < 0 || sector >= d64_spt[track - 1]) return 0;
    return ((size_t)d64_track_offset[track - 1] + sector) * D64_SECTOR_SIZE;
}

int test_format_d64(void) {
    TEST_BEGIN("D64 Format Tests");
    
    // Track Offset Tests
    TEST_ASSERT_EQ(d64_track_offset[0], 0, "Track 1 offset = 0");
    TEST_ASSERT_EQ(d64_track_offset[17], 357, "Track 18 offset = 357");
    TEST_ASSERT_EQ(d64_track_offset[35], 683, "Track 36 offset = 683");
    
    // Sektoren pro Track
    TEST_ASSERT_EQ(d64_spt[0], 21, "Track 1 has 21 sectors");
    TEST_ASSERT_EQ(d64_spt[17], 19, "Track 18 has 19 sectors");
    TEST_ASSERT_EQ(d64_spt[24], 18, "Track 25 has 18 sectors");
    TEST_ASSERT_EQ(d64_spt[30], 17, "Track 31 has 17 sectors");
    
    // BAM Location
    size_t bam_offset = d64_offset(D64_BAM_TRACK, 0);
    TEST_ASSERT_EQ(bam_offset, 357 * 256, "BAM at correct offset");
    
    // Directory Location
    size_t dir_offset = d64_offset(D64_DIR_TRACK, 1);
    TEST_ASSERT_EQ(dir_offset, 358 * 256, "Directory at correct offset");
    
    // Zone-Grenzen
    TEST_ASSERT_EQ(d64_spt[16], 21, "Zone 0 ends at track 17");
    TEST_ASSERT_EQ(d64_spt[17], 19, "Zone 1 starts at track 18");
    TEST_ASSERT_EQ(d64_spt[23], 19, "Zone 1 ends at track 24");
    TEST_ASSERT_EQ(d64_spt[24], 18, "Zone 2 starts at track 25");
    TEST_ASSERT_EQ(d64_spt[29], 18, "Zone 2 ends at track 30");
    TEST_ASSERT_EQ(d64_spt[30], 17, "Zone 3 starts at track 31");
    
    // GCR Speed Zones (für Flux-Level)
    // Zone 0 (1-17): schnellste Zone
    // Zone 3 (31-40): langsamste Zone
    TEST_ASSERT(21 > 17, "Zone 0 has more sectors than Zone 3");
    
    return tests_failed;
}
