/**
 * @file golden_test_framework.c
 * @brief Golden Test Framework Implementation
 */

#include "golden_test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper: CRC32
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void) {
    if (crc32_table_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c >> 1) ^ ((c & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
    init_crc32_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper: File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint8_t* read_file(const char *path, size_t *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    if (size_out) *size_out = read;
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Execution
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool golden_test_run(const golden_test_case_t *tc, char *error_msg, size_t msg_size) {
    if (!tc || !tc->image_path) {
        if (error_msg) snprintf(error_msg, msg_size, "Invalid test case");
        return false;
    }
    
    /* Read golden image */
    size_t size;
    uint8_t *data = read_file(tc->image_path, &size);
    if (!data) {
        if (error_msg) snprintf(error_msg, msg_size, "Cannot read: %s", tc->image_path);
        return false;
    }
    
    bool passed = true;
    
    switch (tc->mode) {
        case GOLDEN_TEST_CHECKSUM: {
            uint32_t crc = calc_crc32(data, size);
            if (crc != tc->expected_crc32) {
                if (error_msg) {
                    snprintf(error_msg, msg_size, 
                             "CRC mismatch: got 0x%08X, expected 0x%08X",
                             crc, tc->expected_crc32);
                }
                passed = false;
            }
            break;
        }
        
        case GOLDEN_TEST_SECTOR_CRC: {
            /* Would parse sectors and compare CRCs */
            /* For now, just verify file exists */
            if (tc->expected_path) {
                FILE *exp = fopen(tc->expected_path, "r");
                if (!exp) {
                    if (error_msg) {
                        snprintf(error_msg, msg_size, "Expected file missing: %s", 
                                 tc->expected_path);
                    }
                    passed = false;
                } else {
                    fclose(exp);
                }
            }
            break;
        }
        
        case GOLDEN_TEST_FULL_COMPARE: {
            if (tc->expected_path) {
                size_t exp_size;
                uint8_t *exp_data = read_file(tc->expected_path, &exp_size);
                if (!exp_data) {
                    if (error_msg) {
                        snprintf(error_msg, msg_size, "Cannot read expected: %s", 
                                 tc->expected_path);
                    }
                    passed = false;
                } else {
                    if (size != exp_size || memcmp(data, exp_data, size) != 0) {
                        if (error_msg) {
                            snprintf(error_msg, msg_size, "Content mismatch");
                        }
                        passed = false;
                    }
                    free(exp_data);
                }
            }
            break;
        }
        
        case GOLDEN_TEST_SECTOR_COUNT: {
            /* Would parse and count sectors */
            /* Simplified: check file size is reasonable for format */
            if (tc->format) {
                bool size_ok = false;
                if (strcmp(tc->format, "adf") == 0) {
                    size_ok = (size == 901120 || size == 1802240);
                } else if (strcmp(tc->format, "d64") == 0) {
                    size_ok = (size == 174848 || size == 175531 || size == 196608);
                } else if (strcmp(tc->format, "st") == 0) {
                    size_ok = (size == 737280 || size == 819200);
                } else {
                    size_ok = true;  /* Unknown format, skip check */
                }
                
                if (!size_ok) {
                    if (error_msg) {
                        snprintf(error_msg, msg_size, 
                                 "Unexpected size %zu for format %s", size, tc->format);
                    }
                    passed = false;
                }
            }
            break;
        }
        
        default:
            if (error_msg) snprintf(error_msg, msg_size, "Unknown test mode");
            passed = false;
    }
    
    free(data);
    return passed;
}

void golden_test_run_all(const golden_test_case_t *tests, int count,
                         golden_test_results_t *results) {
    if (!tests || !results) return;
    
    memset(results, 0, sizeof(*results));
    results->total = count;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    results->failures = calloc(count, sizeof(char*));
    
    for (int i = 0; i < count; i++) {
        const golden_test_case_t *tc = &tests[i];
        
        if (tc->skip) {
            results->skipped++;
            continue;
        }
        
        char error[512] = {0};
        bool passed = golden_test_run(tc, error, sizeof(error));
        
        if (passed) {
            results->passed++;
            printf("  [PASS] %s\n", tc->name);
        } else {
            results->failed++;
            printf("  [FAIL] %s: %s\n", tc->name, error);
            
            results->failures[results->failure_count] = strdup(error);
            results->failure_count++;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    results->elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                          (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

void golden_test_print_results(const golden_test_results_t *results) {
    if (!results) return;
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("Golden Test Results:\n");
    printf("  Total:   %d\n", results->total);
    printf("  Passed:  %d\n", results->passed);
    printf("  Failed:  %d\n", results->failed);
    printf("  Skipped: %d\n", results->skipped);
    printf("  Time:    %.2f ms\n", results->elapsed_ms);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    if (results->failed > 0) {
        printf("\nFailures:\n");
        for (int i = 0; i < results->failure_count; i++) {
            printf("  %d. %s\n", i + 1, results->failures[i]);
        }
    }
}

void golden_test_free_results(golden_test_results_t *results) {
    if (!results) return;
    
    for (int i = 0; i < results->failure_count; i++) {
        free(results->failures[i]);
    }
    free(results->failures);
    memset(results, 0, sizeof(*results));
}

int golden_test_export_junit(const golden_test_results_t *results, 
                             const char *xml_path) {
    if (!results || !xml_path) return -1;
    
    FILE *f = fopen(xml_path, "w");
    if (!f) return -1;
    
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuite name=\"UFT Golden Tests\" tests=\"%d\" failures=\"%d\" "
               "errors=\"0\" skipped=\"%d\" time=\"%.3f\">\n",
            results->total, results->failed, results->skipped, 
            results->elapsed_ms / 1000.0);
    
    /* Would need test case names to generate proper entries */
    fprintf(f, "  <testcase name=\"golden_tests\" time=\"%.3f\">\n", 
            results->elapsed_ms / 1000.0);
    
    if (results->failed > 0) {
        fprintf(f, "    <failure message=\"%d tests failed\">\n", results->failed);
        for (int i = 0; i < results->failure_count; i++) {
            fprintf(f, "      %s\n", results->failures[i]);
        }
        fprintf(f, "    </failure>\n");
    }
    
    fprintf(f, "  </testcase>\n");
    fprintf(f, "</testsuite>\n");
    
    fclose(f);
    return 0;
}
