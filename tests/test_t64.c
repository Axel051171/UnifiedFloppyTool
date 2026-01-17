/**
 * @file test_t64.c
 * @brief Unit tests for T64 Tape Format
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_t64.h"

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
 * @brief Create minimal T64 image
 */
static uint8_t *create_test_t64(size_t *size)
{
    /* Header + 1 dir entry + small file */
    size_t total = T64_HEADER_SIZE + T64_DIR_ENTRY_SIZE + 10;
    uint8_t *data = calloc(1, total);
    
    /* Magic */
    memcpy(data, "C64 tape image file", 19);
    
    /* Header */
    data[32] = 0x00; data[33] = 0x01;  /* Version 0x0100 */
    data[34] = 0x01; data[35] = 0x00;  /* Max entries = 1 */
    data[36] = 0x01; data[37] = 0x00;  /* Used entries = 1 */
    memset(data + 40, 0x20, 24);       /* Tape name */
    memcpy(data + 40, "TEST TAPE", 9);
    
    /* Directory entry */
    uint8_t *entry = data + T64_HEADER_SIZE;
    entry[0] = T64_TYPE_TAPE;          /* Entry type */
    entry[1] = T64_C1541_PRG;          /* C1541 type */
    entry[2] = 0x01; entry[3] = 0x08;  /* Start addr $0801 */
    entry[4] = 0x0B; entry[5] = 0x08;  /* End addr $080B */
    entry[8] = T64_HEADER_SIZE + T64_DIR_ENTRY_SIZE;  /* File offset */
    memset(entry + 16, 0x20, 16);
    memcpy(entry + 16, "TEST FILE", 9);
    
    /* File data (10 bytes) */
    uint8_t *file_data = data + T64_HEADER_SIZE + T64_DIR_ENTRY_SIZE;
    for (int i = 0; i < 10; i++) {
        file_data[i] = i + 0x10;
    }
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_valid)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    ASSERT_TRUE(t64_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[100] = {0};
    ASSERT_FALSE(t64_detect(data, 100));
    
    /* Too small */
    ASSERT_FALSE(t64_detect(data, 10));
    
    /* NULL */
    ASSERT_FALSE(t64_detect(NULL, 100));
}

TEST(validate_valid)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    ASSERT_TRUE(t64_validate(data, size));
    
    free(data);
}

/* ============================================================================
 * Unit Tests - Image Management
 * ============================================================================ */

TEST(open_t64)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    int ret = t64_open(data, size, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.size, size);
    ASSERT_EQ(image.header.max_entries, 1);
    ASSERT_EQ(image.header.used_entries, 1);
    ASSERT_EQ(image.num_entries, 1);
    
    t64_close(&image);
    free(data);
}

TEST(create_t64)
{
    t64_image_t image;
    int ret = t64_create("MY TAPE", 10, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.max_entries, 10);
    ASSERT_EQ(image.header.used_entries, 0);
    ASSERT_EQ(image.num_entries, 0);
    
    t64_close(&image);
}

TEST(close_t64)
{
    t64_image_t image;
    t64_create("TEST", 5, &image);
    
    t64_close(&image);
    
    ASSERT_EQ(image.data, NULL);
    ASSERT_EQ(image.entries, NULL);
    ASSERT_EQ(image.size, 0);
}

/* ============================================================================
 * Unit Tests - Directory
 * ============================================================================ */

TEST(get_file_count)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    ASSERT_EQ(t64_get_file_count(&image), 1);
    
    t64_close(&image);
    free(data);
}

TEST(get_file_info)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    t64_file_info_t info;
    int ret = t64_get_file_info(&image, 0, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.entry_type, T64_TYPE_TAPE);
    ASSERT_EQ(info.c1541_type, T64_C1541_PRG);
    ASSERT_EQ(info.start_addr, 0x0801);
    ASSERT_EQ(info.end_addr, 0x080B);
    ASSERT_EQ(info.data_size, 10);
    
    t64_close(&image);
    free(data);
}

TEST(find_file)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    t64_file_info_t info;
    int ret = t64_find_file(&image, "TEST FILE", &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.start_addr, 0x0801);
    
    /* Not found */
    ret = t64_find_file(&image, "NONEXISTENT", &info);
    ASSERT_EQ(ret, -1);
    
    t64_close(&image);
    free(data);
}

TEST(get_tape_name)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    char name[25];
    t64_get_tape_name(&image, name);
    
    ASSERT_EQ(name[0], 'T');
    ASSERT_EQ(name[1], 'E');
    ASSERT_EQ(name[2], 'S');
    ASSERT_EQ(name[3], 'T');
    
    t64_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - Extraction
 * ============================================================================ */

TEST(extract_file)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    t64_file_t file;
    int ret = t64_extract_file(&image, "TEST FILE", &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    ASSERT_EQ(file.data_size, 10);
    ASSERT_EQ(file.data[0], 0x10);
    ASSERT_EQ(file.data[9], 0x19);
    
    t64_free_file(&file);
    t64_close(&image);
    free(data);
}

TEST(extract_by_index)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    t64_file_t file;
    int ret = t64_extract_by_index(&image, 0, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    
    /* Invalid index */
    t64_free_file(&file);
    ret = t64_extract_by_index(&image, 99, &file);
    ASSERT_NE(ret, 0);
    
    t64_close(&image);
    free(data);
}

TEST(extract_all)
{
    size_t size;
    uint8_t *data = create_test_t64(&size);
    
    t64_image_t image;
    t64_open(data, size, &image);
    
    t64_file_t files[10];
    int count = t64_extract_all(&image, files, 10);
    
    ASSERT_EQ(count, 1);
    ASSERT_NOT_NULL(files[0].data);
    
    for (int i = 0; i < count; i++) {
        t64_free_file(&files[i]);
    }
    
    t64_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - Creation & Modification
 * ============================================================================ */

TEST(add_file)
{
    t64_image_t image;
    t64_create("ADD TEST", 10, &image);
    
    uint8_t file_data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    int ret = t64_add_file(&image, "MY FILE", file_data, sizeof(file_data),
                           0x0801, T64_C1541_PRG);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.num_entries, 1);
    ASSERT_EQ(image.header.used_entries, 1);
    
    /* Extract and verify */
    t64_file_t file;
    ret = t64_extract_file(&image, "MY FILE", &file);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(file.data_size, 4);
    ASSERT_EQ(file.data[0], 0xAA);
    
    t64_free_file(&file);
    t64_close(&image);
}

TEST(add_multiple_files)
{
    t64_image_t image;
    t64_create("MULTI", 10, &image);
    
    uint8_t data1[] = { 0x11, 0x22 };
    uint8_t data2[] = { 0x33, 0x44, 0x55 };
    uint8_t data3[] = { 0x66 };
    
    t64_add_file(&image, "FILE1", data1, sizeof(data1), 0x0801, T64_C1541_PRG);
    t64_add_file(&image, "FILE2", data2, sizeof(data2), 0xC000, T64_C1541_PRG);
    t64_add_file(&image, "FILE3", data3, sizeof(data3), 0x4000, T64_C1541_PRG);
    
    ASSERT_EQ(image.num_entries, 3);
    
    t64_file_info_t info;
    t64_get_file_info(&image, 0, &info);
    ASSERT_EQ(info.start_addr, 0x0801);
    
    t64_get_file_info(&image, 1, &info);
    ASSERT_EQ(info.start_addr, 0xC000);
    
    t64_get_file_info(&image, 2, &info);
    ASSERT_EQ(info.start_addr, 0x4000);
    
    t64_close(&image);
}

TEST(remove_file)
{
    t64_image_t image;
    t64_create("REMOVE", 10, &image);
    
    uint8_t data[] = { 0x00 };
    t64_add_file(&image, "TO DELETE", data, 1, 0x0801, T64_C1541_PRG);
    
    ASSERT_EQ(image.num_entries, 1);
    
    int ret = t64_remove_file(&image, "TO DELETE");
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.num_entries, 0);
    
    /* Try to find removed file */
    ret = t64_find_file(&image, "TO DELETE", NULL);
    ASSERT_EQ(ret, -1);
    
    t64_close(&image);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(type_name)
{
    ASSERT_STR_EQ(t64_type_name(T64_C1541_DEL), "DEL");
    ASSERT_STR_EQ(t64_type_name(T64_C1541_SEQ), "SEQ");
    ASSERT_STR_EQ(t64_type_name(T64_C1541_PRG), "PRG");
    ASSERT_STR_EQ(t64_type_name(T64_C1541_USR), "USR");
    ASSERT_STR_EQ(t64_type_name(T64_C1541_REL), "REL");
    ASSERT_STR_EQ(t64_type_name(99), "???");
}

TEST(petscii_conversion)
{
    char ascii[17];
    char petscii[17];
    
    /* ASCII to PETSCII */
    t64_ascii_to_petscii("hello", petscii, 16);
    ASSERT_EQ(petscii[0], 'H');  /* Uppercase */
    ASSERT_EQ(petscii[1], 'E');
    ASSERT_EQ(petscii[5], 0x20);  /* Padded with space */
    
    /* PETSCII to ASCII */
    t64_petscii_to_ascii("HELLO", ascii, 5);
    ASSERT_EQ(ascii[0], 'H');
    ASSERT_EQ(ascii[4], 'O');
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== T64 Tape Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_valid);
    RUN_TEST(detect_invalid);
    RUN_TEST(validate_valid);
    
    printf("\nImage Management:\n");
    RUN_TEST(open_t64);
    RUN_TEST(create_t64);
    RUN_TEST(close_t64);
    
    printf("\nDirectory:\n");
    RUN_TEST(get_file_count);
    RUN_TEST(get_file_info);
    RUN_TEST(find_file);
    RUN_TEST(get_tape_name);
    
    printf("\nExtraction:\n");
    RUN_TEST(extract_file);
    RUN_TEST(extract_by_index);
    RUN_TEST(extract_all);
    
    printf("\nCreation & Modification:\n");
    RUN_TEST(add_file);
    RUN_TEST(add_multiple_files);
    RUN_TEST(remove_file);
    
    printf("\nUtilities:\n");
    RUN_TEST(type_name);
    RUN_TEST(petscii_conversion);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
