/**
 * @file test_freezer.c
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

#include "uft/formats/c64/uft_freezer.h"

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

/* Create test Action Replay snapshot */
static uint8_t *create_test_ar_snapshot(size_t *size)
{
    /* AR format: header + color RAM + main RAM */
    size_t header_size = 0x80 + FREEZER_COLORRAM_SIZE;
    size_t total = header_size + FREEZER_RAM_SIZE;
    uint8_t *data = calloc(1, total);
    
    /* CPU state */
    data[0] = 0x42;     /* A */
    data[1] = 0x10;     /* X */
    data[2] = 0x20;     /* Y */
    data[3] = 0xFF;     /* SP */
    data[4] = 0x24;     /* Status (bit 5 set, IRQ disabled) */
    data[5] = 0x00;     /* PC low */
    data[6] = 0x08;     /* PC high = $0800 */
    data[7] = 0x37;     /* Port $01 */
    data[8] = 0x2F;     /* Port direction */
    
    /* VIC-II registers at 0x10 */
    data[0x10 + 0x11] = 0x1B;   /* $D011 */
    data[0x10 + 0x16] = 0xC8;   /* $D016 */
    data[0x10 + 0x18] = 0x14;   /* $D018 - screen at $0400 */
    data[0x10 + 0x20] = 0x0E;   /* Border color */
    data[0x10 + 0x21] = 0x06;   /* Background color */
    
    /* CIA1 at 0x60 */
    data[0x60] = 0x7F;  /* PRA */
    data[0x61] = 0xFF;  /* PRB */
    
    /* CIA2 at 0x70 */
    data[0x70] = 0x03;  /* PRA - VIC bank 0 */
    
    /* Color RAM at 0x80 */
    for (int i = 0; i < FREEZER_COLORRAM_SIZE; i++) {
        data[0x80 + i] = (i % 16);
    }
    
    /* Main RAM after color RAM */
    uint8_t *ram = data + 0x80 + FREEZER_COLORRAM_SIZE;
    
    /* BASIC program at $0801 */
    ram[0x0801] = 0x0B;
    ram[0x0802] = 0x08;
    ram[0x0803] = 0x0A;
    ram[0x0804] = 0x00;
    ram[0x0805] = 0x9E;  /* SYS */
    
    /* Screen at $0400 */
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i] = 0x20;  /* Spaces */
    }
    ram[0x0400] = 0x08;  /* H */
    ram[0x0401] = 0x05;  /* E */
    ram[0x0402] = 0x0C;  /* L */
    ram[0x0403] = 0x0C;  /* L */
    ram[0x0404] = 0x0F;  /* O */
    
    *size = total;
    return data;
}

/* Create test Retro Replay FRZ snapshot */
static uint8_t *create_test_rr_snapshot(size_t *size)
{
    size_t header = 16 + 10 + 64 + 32 + 32;  /* Header + CPU + VIC + SID + CIAs */
    size_t total = header + FREEZER_COLORRAM_SIZE + FREEZER_RAM_SIZE;
    uint8_t *data = calloc(1, total);
    
    /* Magic */
    memcpy(data, "C64FRZ", 6);
    data[6] = 1;  /* Version */
    
    /* CPU at offset 16 */
    data[16] = 0x55;    /* A */
    data[17] = 0xAA;    /* X */
    data[18] = 0x33;    /* Y */
    data[19] = 0xFE;    /* SP */
    data[20] = 0x24;    /* Status */
    data[21] = 0x00;    /* PC low */
    data[22] = 0xC0;    /* PC high = $C000 */
    
    *size = total;
    return data;
}

/* Tests */
TEST(detect_ar)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_type_t type = freezer_detect(data, size);
    ASSERT_EQ(type, FREEZER_TYPE_AR);
    
    free(data);
}

TEST(detect_rr)
{
    size_t size;
    uint8_t *data = create_test_rr_snapshot(&size);
    
    freezer_type_t type = freezer_detect(data, size);
    ASSERT_EQ(type, FREEZER_TYPE_RR);
    
    free(data);
}

TEST(type_name)
{
    ASSERT_STR_EQ(freezer_type_name(FREEZER_TYPE_AR), "Action Replay");
    ASSERT_STR_EQ(freezer_type_name(FREEZER_TYPE_RR), "Retro Replay");
    ASSERT_STR_EQ(freezer_type_name(FREEZER_TYPE_FC3), "Final Cartridge III");
}

TEST(validate)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    ASSERT_TRUE(freezer_validate(data, size));
    ASSERT_FALSE(freezer_validate(NULL, 0));
    ASSERT_FALSE(freezer_validate(data, 100));
    
    free(data);
}

TEST(open_ar)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    int ret = freezer_open(data, size, &snapshot);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(snapshot.data);
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ(snapshot.type, FREEZER_TYPE_AR);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    freezer_info_t info;
    int ret = freezer_get_info(&snapshot, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, FREEZER_TYPE_AR);
    ASSERT_EQ(info.entry_point, 0x0800);
    ASSERT_TRUE(info.has_colorram);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(get_cpu)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    freezer_cpu_t cpu;
    int ret = freezer_get_cpu(&snapshot, &cpu);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cpu.a, 0x42);
    ASSERT_EQ(cpu.x, 0x10);
    ASSERT_EQ(cpu.y, 0x20);
    ASSERT_EQ(cpu.sp, 0xFF);
    ASSERT_EQ(cpu.pc, 0x0800);
    ASSERT_EQ(cpu.port, 0x37);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(get_vic)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    freezer_vic_t vic;
    int ret = freezer_get_vic(&snapshot, &vic);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(vic.regs[0x11], 0x1B);
    ASSERT_EQ(vic.regs[0x20], 0x0E);
    ASSERT_EQ(vic.regs[0x21], 0x06);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(get_ram)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    uint8_t buffer[16];
    int ret = freezer_get_ram(&snapshot, 0x0801, buffer, 5);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(buffer[0], 0x0B);
    ASSERT_EQ(buffer[1], 0x08);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(get_colorram)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    uint8_t colorram[FREEZER_COLORRAM_SIZE];
    int ret = freezer_get_colorram(&snapshot, colorram);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(colorram[0], 0);
    ASSERT_EQ(colorram[1], 1);
    ASSERT_EQ(colorram[15], 15);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(extract_prg)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    uint8_t prg[1024];
    size_t prg_size;
    int ret = freezer_extract_prg(&snapshot, 0x0801, 0x0810, prg, sizeof(prg), &prg_size);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(prg_size, 2 + 15);  /* Load addr + data */
    ASSERT_EQ(prg[0], 0x01);      /* Load addr low */
    ASSERT_EQ(prg[1], 0x08);      /* Load addr high */
    
    freezer_close(&snapshot);
    free(data);
}

TEST(extract_screen)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    uint8_t screen[1000];
    int ret = freezer_extract_screen(&snapshot, screen, NULL);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(screen[0], 0x08);  /* H */
    ASSERT_EQ(screen[1], 0x05);  /* E */
    
    freezer_close(&snapshot);
    free(data);
}

TEST(set_cpu)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    
    freezer_cpu_t cpu = {0};
    cpu.a = 0xFF;
    cpu.pc = 0xE000;
    
    int ret = freezer_set_cpu(&snapshot, &cpu);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(snapshot.state.cpu.a, 0xFF);
    ASSERT_EQ(snapshot.state.cpu.pc, 0xE000);
    
    freezer_close(&snapshot);
    free(data);
}

TEST(close_snapshot)
{
    size_t size;
    uint8_t *data = create_test_ar_snapshot(&size);
    
    freezer_snapshot_t snapshot;
    freezer_open(data, size, &snapshot);
    freezer_close(&snapshot);
    
    ASSERT_EQ(snapshot.data, NULL);
    ASSERT_FALSE(snapshot.valid);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== C64 Freezer Snapshot Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_ar);
    RUN_TEST(detect_rr);
    RUN_TEST(type_name);
    RUN_TEST(validate);
    
    printf("\nSnapshot Operations:\n");
    RUN_TEST(open_ar);
    RUN_TEST(get_info);
    RUN_TEST(close_snapshot);
    
    printf("\nState Access:\n");
    RUN_TEST(get_cpu);
    RUN_TEST(get_vic);
    RUN_TEST(get_ram);
    RUN_TEST(get_colorram);
    
    printf("\nState Modification:\n");
    RUN_TEST(set_cpu);
    
    printf("\nConversion:\n");
    RUN_TEST(extract_prg);
    RUN_TEST(extract_screen);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
