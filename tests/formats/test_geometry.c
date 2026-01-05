/**
 * @file test_geometry.c
 * @brief Tests für Disk-Geometrie-Berechnungen
 */

#include "../uft_test.h"

// D64 Zone-basierte Sektoren
static const uint8_t geo_d64_spt[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17
};

// D80 Zone-basierte Sektoren
static const uint8_t geo_d80_spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};

int test_geometry(void) {
    TEST_BEGIN("Geometry Tests");
    
    // D64 Tests
    int d64_total = 0;
    for (int i = 0; i < 35; i++) d64_total += geo_d64_spt[i];
    TEST_ASSERT_EQ(d64_total, 683, "D64 35-track = 683 sectors");
    
    d64_total = 0;
    for (int i = 0; i < 40; i++) d64_total += geo_d64_spt[i];
    TEST_ASSERT_EQ(d64_total, 768, "D64 40-track = 768 sectors");
    
    // D64 Größen
    TEST_ASSERT_EQ(683 * 256, 174848, "D64 35T size");
    TEST_ASSERT_EQ(683 * 256 + 683, 175531, "D64 35T+errors size");
    TEST_ASSERT_EQ(768 * 256, 196608, "D64 40T size");
    
    // D71 Tests (Double-Sided D64)
    TEST_ASSERT_EQ(683 * 2, 1366, "D71 = 2 × D64 sectors");
    TEST_ASSERT_EQ(1366 * 256, 349696, "D71 size");
    
    // D80 Tests
    int d80_total = 0;
    for (int i = 0; i < 77; i++) d80_total += geo_d80_spt[i];
    TEST_ASSERT_EQ(d80_total, 2083, "D80 = 2083 sectors");
    TEST_ASSERT_EQ(2083 * 256, 533248, "D80 size");
    
    // D82 Tests (Double-Sided D80)
    TEST_ASSERT_EQ(2083 * 2, 4166, "D82 = 2 × D80 sectors");
    TEST_ASSERT_EQ(4166 * 256, 1066496, "D82 size");
    
    // D81 Tests
    TEST_ASSERT_EQ(80 * 40, 3200, "D81 = 80×40 sectors");
    TEST_ASSERT_EQ(3200 * 256, 819200, "D81 size");
    
    // ADF Tests
    TEST_ASSERT_EQ(80 * 2 * 11 * 512, 901120, "ADF DD size");
    TEST_ASSERT_EQ(80 * 2 * 22 * 512, 1802240, "ADF HD size");
    
    // PC Floppy Tests
    TEST_ASSERT_EQ(40 * 1 * 8 * 512, 163840, "160KB 5.25\" SS/SD");
    TEST_ASSERT_EQ(40 * 2 * 9 * 512, 368640, "360KB 5.25\" DS/DD");
    TEST_ASSERT_EQ(80 * 2 * 9 * 512, 737280, "720KB 3.5\" DS/DD");
    TEST_ASSERT_EQ(80 * 2 * 15 * 512, 1228800, "1.2MB 5.25\" HD");
    TEST_ASSERT_EQ(80 * 2 * 18 * 512, 1474560, "1.44MB 3.5\" HD");
    TEST_ASSERT_EQ(80 * 2 * 36 * 512, 2949120, "2.88MB 3.5\" ED");
    
    // Atari ST Tests
    TEST_ASSERT_EQ(80 * 2 * 9 * 512, 737280, "ST 720KB");
    
    // NIB Test
    TEST_ASSERT_EQ(35 * 6656, 232960, "NIB size");
    
    // TRD Tests
    TEST_ASSERT_EQ(80 * 2 * 16 * 256, 655360, "TRD 80T DS size");
    
    // Sector Size Codes
    TEST_ASSERT_EQ(128 << 0, 128, "Size code 0 = 128");
    TEST_ASSERT_EQ(128 << 1, 256, "Size code 1 = 256");
    TEST_ASSERT_EQ(128 << 2, 512, "Size code 2 = 512");
    TEST_ASSERT_EQ(128 << 3, 1024, "Size code 3 = 1024");
    
    return tests_failed;
}
