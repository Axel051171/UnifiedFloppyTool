/**
 * @file test_frz.c
 * @brief Unit tests for C64 Freezer Snapshot Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_frz.h"

static int tests_run = 0;
static int tests_passed = 0;

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
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

/* Create test Action Replay-style snapshot */
static uint8_t *create_test_snapshot(size_t *size)
{
    /* Header (256) + RAM (65536) + ColorRAM (1024) */
    size_t total = 256 + FRZ_RAM_SIZE + FRZ_COLORRAM_SIZE;
    uint8_t *data = calloc(1, total);
    
    /* CPU state in header */
    data[0] = 0x42;     /* A */
    data[1] = 0x10;     /* X */
    data[2] = 0x20;     /* Y */
    data[3] = 0xFF;     /* SP */
    data[4] = 0x20;     /* Status */
    data[5] = 0x00;     /* PC low */
    data[6] = 0x08;     /* PC high ($0800) */
    data[7] = 0x37;     /* Port */
    data[8] = 0x2F;     /* Port dir */
    
    /* Some RAM content */
    uint8_t *ram = data + 256;
    ram[0x0801] = 0x0B;  /* BASIC start */
    ram[0x0802] = 0x08;
    
    *size = total;
    return data;
}

/* Tests */
TEST(detect_type_ar5)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_type_t type = frz_detect_type(data, size);
    ASSERT_EQ(type, FRZ_TYPE_AR5);
    
    free(data);
}

TEST(type_name)
{
    ASSERT_STR_EQ(frz_type_name(FRZ_TYPE_AR5), "Action Replay MK5");
    ASSERT_STR_EQ(frz_type_name(FRZ_TYPE_AR6), "Action Replay MK6");
    ASSERT_STR_EQ(frz_type_name(FRZ_TYPE_FC3), "Final Cartridge III");
    ASSERT_STR_EQ(frz_type_name(FRZ_TYPE_RR), "Retro Replay");
}

TEST(validate)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    ASSERT_TRUE(frz_validate(data, size));
    ASSERT_FALSE(frz_validate(NULL, 0));
    ASSERT_FALSE(frz_validate(data, 100));  /* Too small */
    
    free(data);
}

TEST(open_snapshot)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    int ret = frz_open(data, size, &snapshot);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(snapshot.data);
    ASSERT_TRUE(snapshot.state_valid);
    
    frz_close(&snapshot);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    frz_open(data, size, &snapshot);
    
    frz_info_t info;
    int ret = frz_get_info(&snapshot, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, FRZ_TYPE_AR5);
    ASSERT_EQ(info.file_size, size);
    
    frz_close(&snapshot);
    free(data);
}

TEST(get_cpu)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    frz_open(data, size, &snapshot);
    
    frz_cpu_t cpu;
    int ret = frz_get_cpu(&snapshot, &cpu);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cpu.a, 0x42);
    ASSERT_EQ(cpu.x, 0x10);
    ASSERT_EQ(cpu.y, 0x20);
    ASSERT_EQ(cpu.sp, 0xFF);
    ASSERT_EQ(cpu.pc, 0x0800);
    
    frz_close(&snapshot);
    free(data);
}

TEST(get_ram)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    frz_open(data, size, &snapshot);
    
    uint8_t ram[FRZ_RAM_SIZE];
    int ret = frz_get_ram(&snapshot, ram);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(ram[0x0801], 0x0B);
    ASSERT_EQ(ram[0x0802], 0x08);
    
    frz_close(&snapshot);
    free(data);
}

TEST(peek)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    frz_open(data, size, &snapshot);
    
    uint8_t val = frz_peek(&snapshot, 0x0801);
    ASSERT_EQ(val, 0x0B);
    
    frz_close(&snapshot);
    free(data);
}

TEST(extract_prg)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    frz_open(data, size, &snapshot);
    
    uint8_t prg[1024];
    size_t prg_size;
    int ret = frz_extract_prg(&snapshot, 0x0801, 0x0810, prg, sizeof(prg), &prg_size);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(prg_size, 2 + 15);  /* Load addr + data */
    ASSERT_EQ(prg[0], 0x01);      /* Load addr low */
    ASSERT_EQ(prg[1], 0x08);      /* Load addr high */
    
    frz_close(&snapshot);
    free(data);
}

TEST(close_snapshot)
{
    size_t size;
    uint8_t *data = create_test_snapshot(&size);
    
    frz_snapshot_t snapshot;
    frz_open(data, size, &snapshot);
    frz_close(&snapshot);
    
    ASSERT_EQ(snapshot.data, NULL);
    ASSERT_FALSE(snapshot.state_valid);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== C64 Freezer Snapshot Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_type_ar5);
    RUN_TEST(type_name);
    RUN_TEST(validate);
    
    printf("\nSnapshot Operations:\n");
    RUN_TEST(open_snapshot);
    RUN_TEST(get_info);
    RUN_TEST(close_snapshot);
    
    printf("\nState Access:\n");
    RUN_TEST(get_cpu);
    RUN_TEST(get_ram);
    RUN_TEST(peek);
    
    printf("\nConversion:\n");
    RUN_TEST(extract_prg);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
