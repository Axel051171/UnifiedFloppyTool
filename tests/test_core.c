/**
 * @file test_core.c
 * @brief UnifiedFloppyTool - Core API Unit Tests
 * 
 * Einfaches Test-Framework ohne externe Abhängigkeiten.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft.h"

// ============================================================================
// Test Framework
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    fflush(stdout); \
    tests_run++; \
    test_##name(); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    Expected %ld, got %ld\n    at %s:%d\n", \
               (long)(b), (long)(a), __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n    Expected '%s', got '%s'\n    at %s:%d\n", \
               (b), (a), __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define PASS() do { \
    printf("OK\n"); \
    tests_passed++; \
} while(0)

// ============================================================================
// Tests: Error Handling
// ============================================================================

TEST(error_string_known) {
    const char* msg = uft_error_string(UFT_OK);
    ASSERT(msg != NULL);
    ASSERT_STR_EQ(msg, "Success");
    
    msg = uft_error_string(UFT_ERROR_FILE_NOT_FOUND);
    ASSERT(msg != NULL);
    ASSERT_STR_EQ(msg, "File not found");
    
    msg = uft_error_string(UFT_ERROR_CRC_ERROR);
    ASSERT(msg != NULL);
    ASSERT_STR_EQ(msg, "CRC error");
    
    PASS();
}

TEST(error_string_unknown) {
    const char* msg = uft_error_string(-9999);
    ASSERT(msg != NULL);
    // Sollte "Unknown error" oder ähnlich enthalten
    ASSERT(strstr(msg, "nknown") != NULL || strstr(msg, "-9999") != NULL);
    PASS();
}

TEST(error_name) {
    const char* name = uft_error_name(UFT_OK);
    ASSERT(name != NULL);
    ASSERT_STR_EQ(name, "UFT_OK");
    
    name = uft_error_name(UFT_ERROR_NO_MEMORY);
    ASSERT(name != NULL);
    ASSERT_STR_EQ(name, "UFT_ERROR_NO_MEMORY");
    
    PASS();
}

TEST(error_macros) {
    ASSERT(UFT_SUCCEEDED(UFT_OK));
    ASSERT(UFT_SUCCEEDED(0));
    ASSERT(!UFT_SUCCEEDED(-1));
    
    ASSERT(UFT_FAILED(-1));
    ASSERT(UFT_FAILED(UFT_ERROR_CRC_ERROR));
    ASSERT(!UFT_FAILED(UFT_OK));
    
    PASS();
}

// ============================================================================
// Tests: Types
// ============================================================================

TEST(geometry_preset_amiga_dd) {
    uft_geometry_t geo = uft_geometry_for_preset(UFT_GEO_AMIGA_DD);
    
    ASSERT_EQ(geo.cylinders, 80);
    ASSERT_EQ(geo.heads, 2);
    ASSERT_EQ(geo.sectors, 11);
    ASSERT_EQ(geo.sector_size, 512);
    ASSERT_EQ(geo.total_sectors, 80 * 2 * 11);
    
    PASS();
}

TEST(geometry_preset_pc_1440) {
    uft_geometry_t geo = uft_geometry_for_preset(UFT_GEO_PC_1440K);
    
    ASSERT_EQ(geo.cylinders, 80);
    ASSERT_EQ(geo.heads, 2);
    ASSERT_EQ(geo.sectors, 18);
    ASSERT_EQ(geo.sector_size, 512);
    ASSERT_EQ(geo.total_sectors, 2880);
    
    PASS();
}

TEST(geometry_preset_unknown) {
    uft_geometry_t geo = uft_geometry_for_preset(UFT_GEO_UNKNOWN);
    
    ASSERT_EQ(geo.cylinders, 0);
    ASSERT_EQ(geo.heads, 0);
    ASSERT_EQ(geo.sectors, 0);
    
    PASS();
}

TEST(format_info_adf) {
    const uft_format_info_t* info = uft_format_get_info(UFT_FORMAT_ADF);
    
    ASSERT(info != NULL);
    ASSERT_STR_EQ(info->name, "ADF");
    ASSERT(strstr(info->extensions, "adf") != NULL);
    ASSERT(info->can_write == true);
    ASSERT(info->has_flux == false);
    
    PASS();
}

TEST(format_info_scp) {
    const uft_format_info_t* info = uft_format_get_info(UFT_FORMAT_SCP);
    
    ASSERT(info != NULL);
    ASSERT_STR_EQ(info->name, "SCP");
    ASSERT(info->has_flux == true);
    ASSERT(info->preserves_timing == true);
    
    PASS();
}

TEST(format_from_extension) {
    ASSERT_EQ(uft_format_from_extension("adf"), UFT_FORMAT_ADF);
    ASSERT_EQ(uft_format_from_extension(".adf"), UFT_FORMAT_ADF);
    ASSERT_EQ(uft_format_from_extension("ADF"), UFT_FORMAT_ADF);
    
    ASSERT_EQ(uft_format_from_extension("scp"), UFT_FORMAT_SCP);
    ASSERT_EQ(uft_format_from_extension("img"), UFT_FORMAT_IMG);
    ASSERT_EQ(uft_format_from_extension("ima"), UFT_FORMAT_IMG);
    
    ASSERT_EQ(uft_format_from_extension("xyz"), UFT_FORMAT_UNKNOWN);
    ASSERT_EQ(uft_format_from_extension(""), UFT_FORMAT_UNKNOWN);
    ASSERT_EQ(uft_format_from_extension(NULL), UFT_FORMAT_UNKNOWN);
    
    PASS();
}

// ============================================================================
// Tests: CHS/LBA Conversion
// ============================================================================

TEST(chs_to_lba) {
    uft_geometry_t geo = {
        .cylinders = 80,
        .heads = 2,
        .sectors = 18,
        .sector_size = 512
    };
    
    // Erster Sektor
    ASSERT_EQ(uft_chs_to_lba(&geo, 0, 0, 0), 0);
    
    // Zweiter Sektor
    ASSERT_EQ(uft_chs_to_lba(&geo, 0, 0, 1), 1);
    
    // Erste Sektor, zweiter Kopf
    ASSERT_EQ(uft_chs_to_lba(&geo, 0, 1, 0), 18);
    
    // Erster Sektor, zweiter Zylinder
    ASSERT_EQ(uft_chs_to_lba(&geo, 1, 0, 0), 36);
    
    PASS();
}

TEST(lba_to_chs) {
    uft_geometry_t geo = {
        .cylinders = 80,
        .heads = 2,
        .sectors = 18,
        .sector_size = 512
    };
    
    int cyl, head, sector;
    
    uft_lba_to_chs(&geo, 0, &cyl, &head, &sector);
    ASSERT_EQ(cyl, 0);
    ASSERT_EQ(head, 0);
    ASSERT_EQ(sector, 0);
    
    uft_lba_to_chs(&geo, 18, &cyl, &head, &sector);
    ASSERT_EQ(cyl, 0);
    ASSERT_EQ(head, 1);
    ASSERT_EQ(sector, 0);
    
    uft_lba_to_chs(&geo, 36, &cyl, &head, &sector);
    ASSERT_EQ(cyl, 1);
    ASSERT_EQ(head, 0);
    ASSERT_EQ(sector, 0);
    
    PASS();
}

// ============================================================================
// Tests: Init/Shutdown
// ============================================================================

TEST(init_shutdown) {
    // Sollte mehrfach aufrufbar sein
    ASSERT_EQ(uft_init(), UFT_OK);
    ASSERT_EQ(uft_init(), UFT_OK);  // Doppelt OK
    
    uft_shutdown();
    
    // Nach Shutdown wieder init
    ASSERT_EQ(uft_init(), UFT_OK);
    
    PASS();
}

TEST(version_string) {
    const char* ver = uft_version();
    ASSERT(ver != NULL);
    ASSERT(strlen(ver) > 0);
    ASSERT(strstr(ver, ".") != NULL);  // Sollte Versionsnummer haben
    
    PASS();
}

// ============================================================================
// Tests: Disk Operations (ohne echte Datei)
// ============================================================================

TEST(disk_open_nonexistent) {
    uft_disk_t* disk = uft_disk_open("/nonexistent/path/file.adf", true);
    ASSERT(disk == NULL);
    
    PASS();
}

TEST(disk_open_null_path) {
    uft_disk_t* disk = uft_disk_open(NULL, true);
    ASSERT(disk == NULL);
    
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("UFT Core Unit Tests\n");
    printf("===================\n\n");
    
    // Init für Tests
    uft_init();
    
    printf("Error Handling:\n");
    RUN_TEST(error_string_known);
    RUN_TEST(error_string_unknown);
    RUN_TEST(error_name);
    RUN_TEST(error_macros);
    
    printf("\nTypes:\n");
    RUN_TEST(geometry_preset_amiga_dd);
    RUN_TEST(geometry_preset_pc_1440);
    RUN_TEST(geometry_preset_unknown);
    RUN_TEST(format_info_adf);
    RUN_TEST(format_info_scp);
    RUN_TEST(format_from_extension);
    
    printf("\nCHS/LBA:\n");
    RUN_TEST(chs_to_lba);
    RUN_TEST(lba_to_chs);
    
    printf("\nInit/Shutdown:\n");
    RUN_TEST(init_shutdown);
    RUN_TEST(version_string);
    
    printf("\nDisk Operations:\n");
    RUN_TEST(disk_open_nonexistent);
    RUN_TEST(disk_open_null_path);
    
    // Cleanup
    uft_shutdown();
    
    printf("\n===================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
