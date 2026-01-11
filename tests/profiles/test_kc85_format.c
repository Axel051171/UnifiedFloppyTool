/**
 * @file test_kc85_format.c
 * @brief Tests for KC85/Z1013 DDR Computer Disk Formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/profiles/uft_kc85_format.h"

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
 * Geometry Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_geometry_count(void) {
    int count = uft_kc_count_geometries();
    return count >= 14;  /* At least 14 geometries defined (added KC85_D004_MICRODOS) */
}

static int test_geometry_kc85_d004(void) {
    const uft_kc_geometry_t* g = uft_kc_find_geometry("KC85_D004_40T");
    if (!g) return 0;
    
    return g->system == UFT_KC_SYSTEM_KC85_4 &&
           g->tracks == 40 &&
           g->sides == 2 &&
           g->sectors_per_track == 5 &&
           g->sector_size == 512 &&
           g->total_size == 200 * 1024;
}

static int test_geometry_z1013(void) {
    const uft_kc_geometry_t* g = uft_kc_find_geometry("Z1013_SD");
    if (!g) return 0;
    
    return g->system == UFT_KC_SYSTEM_Z1013 &&
           g->tracks == 40 &&
           g->sides == 1 &&
           g->sectors_per_track == 16 &&
           g->sector_size == 256;
}

static int test_geometry_kc_compact(void) {
    const uft_kc_geometry_t* g = uft_kc_find_geometry("KC_COMPACT_SYS");
    if (!g) return 0;
    
    return g->system == UFT_KC_SYSTEM_KC_COMPACT &&
           g->disk_type == UFT_KC_DISK_EDSK &&
           g->sectors_per_track == 9 &&
           g->sector_size == 512;
}

static int test_geometry_pcm(void) {
    const uft_kc_geometry_t* g = uft_kc_find_geometry("PCM_SD");
    if (!g) return 0;
    
    return g->system == UFT_KC_SYSTEM_PC_M &&
           g->tracks == 77 &&
           g->sectors_per_track == 26 &&
           g->sector_size == 128;
}

static int test_find_by_size(void) {
    const uft_kc_geometry_t* g = uft_kc_find_by_size(UFT_KC_SYSTEM_KC85_5, 400 * 1024);
    return g != NULL && strcmp(g->name, "KC85_D004_80T") == 0;
}

static int test_get_geometries_for_system(void) {
    const uft_kc_geometry_t* geoms[10];
    int count = uft_kc_get_geometries(UFT_KC_SYSTEM_KC85_5, geoms, 10);
    return count >= 2;  /* KC85/5 has at least 2 geometries (80T DD and QD) */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * System/Type Name Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_system_names(void) {
    return strcmp(uft_kc_system_name(UFT_KC_SYSTEM_KC85_4), "KC85/4") == 0 &&
           strcmp(uft_kc_system_name(UFT_KC_SYSTEM_Z1013), "Z1013") == 0 &&
           strcmp(uft_kc_system_name(UFT_KC_SYSTEM_KC_COMPACT), "KC compact") == 0;
}

static int test_disk_type_names(void) {
    return strcmp(uft_kc_disk_type_name(UFT_KC_DISK_MICRODOS), "MicroDOS") == 0 &&
           strcmp(uft_kc_disk_type_name(UFT_KC_DISK_CPM), "CP/M") == 0 &&
           strcmp(uft_kc_disk_type_name(UFT_KC_DISK_EDSK), "EDSK") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe/Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_probe_microdos(void) {
    uint8_t data[512];
    memset(data, 0, sizeof(data));
    
    /* Create MicroDOS-like boot sector */
    data[0] = 0xC3;  /* JP instruction */
    memcpy(data + 3, "MICRODOS", 8);
    
    return uft_kc_is_microdos(data, sizeof(data));
}

static int test_probe_score(void) {
    uint8_t data[200 * 1024];
    memset(data, 0, sizeof(data));
    
    /* Add MicroDOS signature */
    data[0] = 0xC3;
    memcpy(data + 3, "MICRODOS", 8);
    
    int score = uft_kc85_probe(data, sizeof(data));
    return score >= 70;  /* Should get high score for MicroDOS + correct size */
}

static int test_detect_system_kc85(void) {
    uint8_t data[200 * 1024];
    memset(data, 0, sizeof(data));
    
    uft_kc_system_t system = uft_kc_detect_system(data, sizeof(data));
    return system == UFT_KC_SYSTEM_KC85_4;
}

static int test_detect_system_kc_compact(void) {
    uint8_t data[512];  /* Need > 256 for detection */
    memset(data, 0, sizeof(data));
    memcpy(data, "EXTENDED", 8);
    
    uft_kc_system_t system = uft_kc_detect_system(data, sizeof(data));
    return system == UFT_KC_SYSTEM_KC_COMPACT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Structure Size Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_struct_sizes(void) {
    /* MicroDOS boot sector should be exactly 30 bytes */
    /* CAOS dir entry is 28 bytes (packed) */
    return sizeof(uft_microdos_boot_t) == 30 &&
           sizeof(uft_caos_dir_entry_t) == 28;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== KC85/Z1013 Format Tests ===\n\n");
    
    printf("[Geometry]\n");
    TEST(geometry_count);
    TEST(geometry_kc85_d004);
    TEST(geometry_z1013);
    TEST(geometry_kc_compact);
    TEST(geometry_pcm);
    TEST(find_by_size);
    TEST(get_geometries_for_system);
    
    printf("\n[Names]\n");
    TEST(system_names);
    TEST(disk_type_names);
    
    printf("\n[Probe/Detection]\n");
    TEST(probe_microdos);
    TEST(probe_score);
    TEST(detect_system_kc85);
    TEST(detect_system_kc_compact);
    
    printf("\n[Structures]\n");
    TEST(struct_sizes);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    /* Print all geometries for info */
    printf("Available KC85/Z1013 Geometries:\n");
    uft_kc_list_geometries();
    
    return (tests_passed == tests_run) ? 0 : 1;
}
