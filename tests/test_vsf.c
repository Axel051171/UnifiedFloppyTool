/**
 * @file test_vsf.c
 * @brief Unit tests for VICE Snapshot Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_vsf.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #condition); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/**
 * @brief Create minimal VSF snapshot
 */
static uint8_t *create_test_vsf(size_t *size)
{
    /* Header (37 bytes) + MAINCPU module */
    size_t mod_data_size = 32;
    size_t mod_total = 22 + mod_data_size;  /* Module header + data */
    size_t total = 37 + mod_total;
    uint8_t *data = calloc(1, total);
    
    /* Header */
    memcpy(data, "VICE Snapshot File\032", 19);
    data[19] = 1;  /* Version major */
    data[20] = 1;  /* Version minor */
    memcpy(data + 21, "C64", 3);  /* Machine */
    memset(data + 24, ' ', 13);   /* Pad machine name */
    
    /* MAINCPU module at offset 37 */
    uint8_t *mod = data + 37;
    memcpy(mod, "MAINCPU", 7);
    memset(mod + 7, ' ', 9);  /* Pad name */
    mod[16] = 1;  /* Module version major */
    mod[17] = 0;  /* Module version minor */
    
    /* Module length (little endian) */
    mod[18] = mod_data_size & 0xFF;
    mod[19] = (mod_data_size >> 8) & 0xFF;
    mod[20] = 0;
    mod[21] = 0;
    
    /* Module data - simple CPU state */
    uint8_t *cpu_data = mod + 22;
    /* Clock (4 bytes) */
    cpu_data[0] = 0x00; cpu_data[1] = 0x00;
    cpu_data[2] = 0x00; cpu_data[3] = 0x00;
    /* A, X, Y, SP */
    cpu_data[4] = 0x00;  /* A */
    cpu_data[5] = 0x00;  /* X */
    cpu_data[6] = 0x00;  /* Y */
    cpu_data[7] = 0xFF;  /* SP */
    /* PC (little endian) */
    cpu_data[8] = 0x00;
    cpu_data[9] = 0xE0;  /* PC = $E000 */
    /* Status */
    cpu_data[10] = 0x20;  /* Status */
    
    *size = total;
    return data;
}

/**
 * @brief Create VSF with multiple modules
 */
static uint8_t *create_multi_module_vsf(size_t *size)
{
    /* Header + 2 modules */
    size_t mod1_data = 32;
    size_t mod2_data = 16;
    size_t total = 37 + (22 + mod1_data) + (22 + mod2_data);
    uint8_t *data = calloc(1, total);
    
    /* Header */
    memcpy(data, "VICE Snapshot File\032", 19);
    data[19] = 1;
    data[20] = 1;
    memcpy(data + 21, "C64", 3);
    memset(data + 24, ' ', 13);
    
    /* Module 1: MAINCPU */
    uint8_t *mod1 = data + 37;
    memcpy(mod1, "MAINCPU", 7);
    memset(mod1 + 7, ' ', 9);
    mod1[16] = 1; mod1[17] = 0;
    mod1[18] = mod1_data; mod1[19] = 0; mod1[20] = 0; mod1[21] = 0;
    
    /* Module 2: CIA1 */
    uint8_t *mod2 = mod1 + 22 + mod1_data;
    memcpy(mod2, "CIA1", 4);
    memset(mod2 + 4, ' ', 12);
    mod2[16] = 1; mod2[17] = 0;
    mod2[18] = mod2_data; mod2[19] = 0; mod2[20] = 0; mod2[21] = 0;
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_valid)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    ASSERT_TRUE(vsf_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[100] = {0};
    ASSERT_FALSE(vsf_detect(data, 100));
    ASSERT_FALSE(vsf_detect(NULL, 100));
    ASSERT_FALSE(vsf_detect(data, 10));
}

TEST(validate_valid)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    ASSERT_TRUE(vsf_validate(data, size));
    
    free(data);
}

TEST(machine_type)
{
    ASSERT_EQ(vsf_get_machine_type("C64"), VSF_MACHINE_C64);
    ASSERT_EQ(vsf_get_machine_type("C128"), VSF_MACHINE_C128);
    ASSERT_EQ(vsf_get_machine_type("VIC20"), VSF_MACHINE_VIC20);
    ASSERT_EQ(vsf_get_machine_type("PLUS4"), VSF_MACHINE_PLUS4);
    ASSERT_EQ(vsf_get_machine_type("PET"), VSF_MACHINE_PET);
}

TEST(machine_name)
{
    ASSERT_STR_EQ(vsf_machine_name(VSF_MACHINE_C64), "Commodore 64");
    ASSERT_STR_EQ(vsf_machine_name(VSF_MACHINE_C128), "Commodore 128");
    ASSERT_STR_EQ(vsf_machine_name(VSF_MACHINE_VIC20), "VIC-20");
}

/* ============================================================================
 * Unit Tests - Snapshot Operations
 * ============================================================================ */

TEST(open_vsf)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    vsf_snapshot_t snapshot;
    int ret = vsf_open(data, size, &snapshot);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(snapshot.data);
    ASSERT_EQ(snapshot.machine, VSF_MACHINE_C64);
    ASSERT(snapshot.num_modules >= 1);
    
    vsf_close(&snapshot);
    free(data);
}

TEST(close_vsf)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    vsf_close(&snapshot);
    
    ASSERT_EQ(snapshot.data, NULL);
    ASSERT_EQ(snapshot.modules, NULL);
    
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    
    vsf_info_t info;
    int ret = vsf_get_info(&snapshot, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.machine, VSF_MACHINE_C64);
    ASSERT_EQ(info.version_major, 1);
    ASSERT_EQ(info.version_minor, 1);
    ASSERT(info.num_modules >= 1);
    
    vsf_close(&snapshot);
    free(data);
}

/* ============================================================================
 * Unit Tests - Module Operations
 * ============================================================================ */

TEST(get_module_count)
{
    size_t size;
    uint8_t *data = create_multi_module_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    
    int count = vsf_get_module_count(&snapshot);
    ASSERT_EQ(count, 2);
    
    vsf_close(&snapshot);
    free(data);
}

TEST(get_module)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    
    vsf_module_t module;
    int ret = vsf_get_module(&snapshot, 0, &module);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(module.name, "MAINCPU");
    ASSERT(module.length > 0);
    
    vsf_close(&snapshot);
    free(data);
}

TEST(find_module)
{
    size_t size;
    uint8_t *data = create_multi_module_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    
    vsf_module_t module;
    
    /* Find MAINCPU */
    int ret = vsf_find_module(&snapshot, "MAINCPU", &module);
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(module.name, "MAINCPU");
    
    /* Find CIA1 */
    ret = vsf_find_module(&snapshot, "CIA1", &module);
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(module.name, "CIA1");
    
    /* Module not found */
    ret = vsf_find_module(&snapshot, "NONEXISTENT", &module);
    ASSERT_NE(ret, 0);
    
    vsf_close(&snapshot);
    free(data);
}

TEST(get_module_data)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    
    const uint8_t *mod_data;
    size_t mod_size;
    int ret = vsf_get_module_data(&snapshot, "MAINCPU", &mod_data, &mod_size);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(mod_data);
    ASSERT(mod_size > 0);
    
    vsf_close(&snapshot);
    free(data);
}

/* ============================================================================
 * Unit Tests - State Extraction
 * ============================================================================ */

TEST(get_cpu_state)
{
    size_t size;
    uint8_t *data = create_test_vsf(&size);
    
    vsf_snapshot_t snapshot;
    vsf_open(data, size, &snapshot);
    
    vsf_cpu_state_t state;
    int ret = vsf_get_cpu_state(&snapshot, &state);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(state.sp, 0xFF);
    ASSERT_EQ(state.pc, 0xE000);
    
    vsf_close(&snapshot);
    free(data);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== VICE Snapshot Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_valid);
    RUN_TEST(detect_invalid);
    RUN_TEST(validate_valid);
    RUN_TEST(machine_type);
    RUN_TEST(machine_name);
    
    printf("\nSnapshot Operations:\n");
    RUN_TEST(open_vsf);
    RUN_TEST(close_vsf);
    RUN_TEST(get_info);
    
    printf("\nModule Operations:\n");
    RUN_TEST(get_module_count);
    RUN_TEST(get_module);
    RUN_TEST(find_module);
    RUN_TEST(get_module_data);
    
    printf("\nState Extraction:\n");
    RUN_TEST(get_cpu_state);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
