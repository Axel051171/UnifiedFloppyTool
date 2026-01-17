/**
 * @file test_d64_file.c
 * @brief Unit tests for D64 File Extraction/Insertion
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_d64_file.h"
#include "uft/formats/c64/uft_bam_editor.h"

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
 * @brief Create test D64 with a PRG file
 */
static uint8_t *create_test_d64(size_t *size)
{
    uint8_t *data;
    bam_create_d64(35, "TEST DISK", "TD", &data, size);
    
    /* Create a simple PRG file */
    uint8_t prg_data[20] = {
        0x01, 0x08,  /* Load address $0801 */
        0x0B, 0x08, 0x0A, 0x00,  /* BASIC line: 10 PRINT"HI" */
        0x99, 0x22, 0x48, 0x49, 0x22, 0x00, 0x00, 0x00
    };
    
    d64_insert_opts_t opts;
    d64_get_insert_defaults(&opts);
    opts.file_type = D64_FILE_PRG;
    
    d64_insert_file(data, *size, "TEST PRG", prg_data, sizeof(prg_data), &opts);
    
    return data;
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(calc_blocks)
{
    ASSERT_EQ(d64_calc_blocks(0), 0);
    ASSERT_EQ(d64_calc_blocks(1), 1);
    ASSERT_EQ(d64_calc_blocks(254), 1);
    ASSERT_EQ(d64_calc_blocks(255), 2);
    ASSERT_EQ(d64_calc_blocks(508), 2);
    ASSERT_EQ(d64_calc_blocks(509), 3);
}

TEST(file_extension)
{
    ASSERT_STR_EQ(d64_file_extension(D64_FILE_DEL), "del");
    ASSERT_STR_EQ(d64_file_extension(D64_FILE_SEQ), "seq");
    ASSERT_STR_EQ(d64_file_extension(D64_FILE_PRG), "prg");
    ASSERT_STR_EQ(d64_file_extension(D64_FILE_USR), "usr");
    ASSERT_STR_EQ(d64_file_extension(D64_FILE_REL), "rel");
}

TEST(parse_extension)
{
    ASSERT_EQ(d64_parse_extension("prg"), D64_FILE_PRG);
    ASSERT_EQ(d64_parse_extension("PRG"), D64_FILE_PRG);
    ASSERT_EQ(d64_parse_extension("seq"), D64_FILE_SEQ);
    ASSERT_EQ(d64_parse_extension("unknown"), D64_FILE_PRG);
}

TEST(make_filename)
{
    char c64_name[17];
    
    d64_make_filename("test.prg", c64_name);
    ASSERT_EQ(c64_name[0], 'T');
    ASSERT_EQ(c64_name[1], 'E');
    ASSERT_EQ(c64_name[2], 'S');
    ASSERT_EQ(c64_name[3], 'T');
    
    d64_make_filename("/path/to/file.prg", c64_name);
    ASSERT_EQ(c64_name[0], 'F');
    ASSERT_EQ(c64_name[1], 'I');
    ASSERT_EQ(c64_name[2], 'L');
    ASSERT_EQ(c64_name[3], 'E');
}

TEST(defaults)
{
    d64_extract_opts_t ext_opts;
    d64_get_extract_defaults(&ext_opts);
    ASSERT_TRUE(ext_opts.include_load_addr);
    ASSERT_TRUE(ext_opts.convert_petscii);
    
    d64_insert_opts_t ins_opts;
    d64_get_insert_defaults(&ins_opts);
    ASSERT_EQ(ins_opts.file_type, D64_FILE_PRG);
    ASSERT_FALSE(ins_opts.overwrite);
}

/* ============================================================================
 * Unit Tests - File Insertion
 * ============================================================================ */

TEST(insert_prg)
{
    uint8_t *d64_data;
    size_t d64_size;
    bam_create_d64(35, "INSERT TEST", "IT", &d64_data, &d64_size);
    
    /* Create test PRG data */
    uint8_t prg[10] = { 0x01, 0x08, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x00 };
    
    int ret = d64_insert_prg(d64_data, d64_size, "HELLO", prg, sizeof(prg), 0);
    ASSERT_EQ(ret, 0);
    
    /* Verify file exists */
    d64_file_t file;
    ret = d64_extract_file(d64_data, d64_size, "HELLO", &file);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(file.file_type, D64_FILE_PRG);
    ASSERT(file.data_size >= 10);
    
    d64_free_file(&file);
    free(d64_data);
}

TEST(insert_multiple)
{
    uint8_t *d64_data;
    size_t d64_size;
    bam_create_d64(35, "MULTI TEST", "MT", &d64_data, &d64_size);
    
    uint8_t data1[] = { 0x01, 0x08, 0x11, 0x22 };
    uint8_t data2[] = { 0x00, 0xC0, 0x33, 0x44, 0x55 };
    uint8_t data3[] = { 0x00, 0x40, 0x66, 0x77, 0x88, 0x99 };
    
    d64_insert_prg(d64_data, d64_size, "FILE1", data1, sizeof(data1), 0);
    d64_insert_prg(d64_data, d64_size, "FILE2", data2, sizeof(data2), 0);
    d64_insert_prg(d64_data, d64_size, "FILE3", data3, sizeof(data3), 0);
    
    /* Extract and verify */
    d64_file_t file;
    
    ASSERT_EQ(d64_extract_file(d64_data, d64_size, "FILE1", &file), 0);
    ASSERT_EQ(file.load_address, 0x0801);
    d64_free_file(&file);
    
    ASSERT_EQ(d64_extract_file(d64_data, d64_size, "FILE2", &file), 0);
    ASSERT_EQ(file.load_address, 0xC000);
    d64_free_file(&file);
    
    ASSERT_EQ(d64_extract_file(d64_data, d64_size, "FILE3", &file), 0);
    ASSERT_EQ(file.load_address, 0x4000);
    d64_free_file(&file);
    
    free(d64_data);
}

TEST(insert_no_overwrite)
{
    uint8_t *d64_data;
    size_t d64_size;
    bam_create_d64(35, "NO OVERWRITE", "NO", &d64_data, &d64_size);
    
    uint8_t data[] = { 0x01, 0x08, 0x00 };
    
    /* Insert first file */
    int ret = d64_insert_prg(d64_data, d64_size, "SAME NAME", data, sizeof(data), 0);
    ASSERT_EQ(ret, 0);
    
    /* Try to insert again without overwrite - should fail */
    d64_insert_opts_t opts;
    d64_get_insert_defaults(&opts);
    opts.overwrite = false;
    
    ret = d64_insert_file(d64_data, d64_size, "SAME NAME", data, sizeof(data), &opts);
    ASSERT_EQ(ret, -2);  /* File exists error */
    
    free(d64_data);
}

/* ============================================================================
 * Unit Tests - File Extraction
 * ============================================================================ */

TEST(extract_file)
{
    size_t d64_size;
    uint8_t *d64_data = create_test_d64(&d64_size);
    
    d64_file_t file;
    int ret = d64_extract_file(d64_data, d64_size, "TEST PRG", &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    ASSERT(file.data_size > 0);
    ASSERT_EQ(file.file_type, D64_FILE_PRG);
    ASSERT_TRUE(file.has_load_address);
    ASSERT_EQ(file.load_address, 0x0801);
    
    d64_free_file(&file);
    free(d64_data);
}

TEST(extract_not_found)
{
    size_t d64_size;
    uint8_t *d64_data = create_test_d64(&d64_size);
    
    d64_file_t file;
    int ret = d64_extract_file(d64_data, d64_size, "NONEXISTENT", &file);
    
    ASSERT_EQ(ret, -2);  /* Not found */
    
    free(d64_data);
}

TEST(extract_by_index)
{
    size_t d64_size;
    uint8_t *d64_data = create_test_d64(&d64_size);
    
    d64_file_t file;
    int ret = d64_extract_by_index(d64_data, d64_size, 0, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    
    d64_free_file(&file);
    free(d64_data);
}

TEST(extract_all)
{
    uint8_t *d64_data;
    size_t d64_size;
    bam_create_d64(35, "EXTRACT ALL", "EA", &d64_data, &d64_size);
    
    /* Insert 3 files */
    uint8_t data[] = { 0x01, 0x08, 0x00 };
    d64_insert_prg(d64_data, d64_size, "FILE1", data, sizeof(data), 0);
    d64_insert_prg(d64_data, d64_size, "FILE2", data, sizeof(data), 0);
    d64_insert_prg(d64_data, d64_size, "FILE3", data, sizeof(data), 0);
    
    d64_file_t files[10];
    int count = d64_extract_all(d64_data, d64_size, files, 10);
    
    ASSERT_EQ(count, 3);
    
    for (int i = 0; i < count; i++) {
        d64_free_file(&files[i]);
    }
    free(d64_data);
}

/* ============================================================================
 * Unit Tests - File Chain
 * ============================================================================ */

TEST(get_chain)
{
    size_t d64_size;
    uint8_t *d64_data = create_test_d64(&d64_size);
    
    /* Get first file's track/sector from directory */
    int dir_offset = (357 + 1) * 256;  /* Track 18, sector 1 */
    int first_track = d64_data[dir_offset + 3];
    int first_sector = d64_data[dir_offset + 4];
    
    d64_chain_t chain;
    int ret = d64_get_chain(d64_data, d64_size, first_track, first_sector, &chain);
    
    ASSERT_EQ(ret, 0);
    ASSERT(chain.num_entries > 0);
    ASSERT_EQ(chain.entries[0].track, first_track);
    ASSERT_EQ(chain.entries[0].sector, first_sector);
    
    d64_free_chain(&chain);
    free(d64_data);
}

TEST(validate_chain)
{
    size_t d64_size;
    uint8_t *d64_data = create_test_d64(&d64_size);
    
    int dir_offset = (357 + 1) * 256;
    int first_track = d64_data[dir_offset + 3];
    int first_sector = d64_data[dir_offset + 4];
    
    d64_chain_t chain;
    d64_get_chain(d64_data, d64_size, first_track, first_sector, &chain);
    
    int errors;
    bool valid = d64_validate_chain(d64_data, d64_size, &chain, &errors);
    
    ASSERT_TRUE(valid);
    ASSERT_EQ(errors, 0);
    
    d64_free_chain(&chain);
    free(d64_data);
}

/* ============================================================================
 * Unit Tests - Directory
 * ============================================================================ */

TEST(find_free_dir_entry)
{
    uint8_t *d64_data;
    size_t d64_size;
    bam_create_d64(35, "DIR TEST", "DT", &d64_data, &d64_size);
    
    int track, sector, offset;
    int ret = d64_find_free_dir_entry(d64_data, d64_size, &track, &sector, &offset);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(track, 18);
    ASSERT_EQ(sector, 1);
    ASSERT(offset >= 0);
    
    free(d64_data);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== D64 File Operations Tests ===\n\n");
    
    printf("Utilities:\n");
    RUN_TEST(calc_blocks);
    RUN_TEST(file_extension);
    RUN_TEST(parse_extension);
    RUN_TEST(make_filename);
    RUN_TEST(defaults);
    
    printf("\nFile Insertion:\n");
    RUN_TEST(insert_prg);
    RUN_TEST(insert_multiple);
    RUN_TEST(insert_no_overwrite);
    
    printf("\nFile Extraction:\n");
    RUN_TEST(extract_file);
    RUN_TEST(extract_not_found);
    RUN_TEST(extract_by_index);
    RUN_TEST(extract_all);
    
    printf("\nFile Chain:\n");
    RUN_TEST(get_chain);
    RUN_TEST(validate_chain);
    
    printf("\nDirectory:\n");
    RUN_TEST(find_free_dir_entry);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
