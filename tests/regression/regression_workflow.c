/**
 * @file regression_workflow.c
 * @brief Bug → Test → Fix Workflow Implementation
 * 
 * WORKFLOW: BUG → TEST → FIX
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                    BUG REPRODUCTION WORKFLOW                           │
 *   ├─────────────────────────────────────────────────────────────────────────┤
 *   │                                                                         │
 *   │   STEP 1: BUG DISCOVERY                                                │
 *   │   ─────────────────────────                                             │
 *   │   • Fuzzer finds crash                                                 │
 *   │   • User reports issue                                                 │
 *   │   • CI catches regression                                              │
 *   │                                                                         │
 *   │           ↓                                                             │
 *   │                                                                         │
 *   │   STEP 2: CREATE REPRO FILE                                            │
 *   │   ─────────────────────────────                                         │
 *   │   • Minimize crash input (afl-tmin)                                   │
 *   │   • Save to tests/regression/issue_NNN.bin                            │
 *   │   • Document crash class and symptoms                                  │
 *   │                                                                         │
 *   │           ↓                                                             │
 *   │                                                                         │
 *   │   STEP 3: CREATE TEST                                                  │
 *   │   ────────────────────────                                              │
 *   │   • Write test_issue_NNN.c                                            │
 *   │   • Test MUST FAIL before fix                                         │
 *   │   • Test verifies crash/error behavior                                │
 *   │                                                                         │
 *   │           ↓                                                             │
 *   │                                                                         │
 *   │   STEP 4: FIX BUG                                                      │
 *   │   ───────────────────                                                   │
 *   │   • Implement fix in source                                           │
 *   │   • Test now passes                                                    │
 *   │   • No other tests regress                                             │
 *   │                                                                         │
 *   │           ↓                                                             │
 *   │                                                                         │
 *   │   STEP 5: COMMIT                                                       │
 *   │   ──────────────────                                                    │
 *   │   • Test stays in suite forever                                       │
 *   │   • CI runs test on every commit                                      │
 *   │   • Bug can never silently regress                                    │
 *   │                                                                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 */

#include "uft/uft_test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ============================================================================
// Regression Test Registry
// ============================================================================

static uft_regression_test_t g_regression_tests[] = {
    // Example entries (would be populated from actual bugs)
    {
        .issue_id = "ISSUE-001",
        .description = "D64 crash on truncated file",
        .input_path = "tests/regression/issue_001.bin",
        .crash_class = UFT_CRASH_OOB_READ,
        .fixed = true,
        .fix_commit = "abc123def",
    },
    {
        .issue_id = "ISSUE-002", 
        .description = "SCP integer overflow in track count",
        .input_path = "tests/regression/issue_002.bin",
        .crash_class = UFT_CRASH_INT_OVERFLOW,
        .fixed = true,
        .fix_commit = "def456abc",
    },
    {
        .issue_id = "ISSUE-003",
        .description = "G64 null pointer on empty track table",
        .input_path = "tests/regression/issue_003.bin",
        .crash_class = UFT_CRASH_NULL_DEREF,
        .fixed = true,
        .fix_commit = "789xyz012",
    },
    {
        .issue_id = "FUZZ-001",
        .description = "HFE heap overflow on track list",
        .input_path = "tests/regression/fuzz_001.bin",
        .crash_class = UFT_CRASH_OOB_WRITE,
        .fixed = true,
        .fix_commit = "aaa111bbb",
    },
    {
        .issue_id = "FUZZ-002",
        .description = "ADF division by zero on corrupt BPB",
        .input_path = "tests/regression/fuzz_002.bin",
        .crash_class = UFT_CRASH_DIV_BY_ZERO,
        .fixed = false,  // Not yet fixed!
        .fix_commit = NULL,
    },
    // Terminator
    { .issue_id = NULL }
};

// ============================================================================
// Create Regression Test from Crash
// ============================================================================

uft_error_t uft_regression_create(const char* issue_id,
                                   const uint8_t* crash_input,
                                   size_t crash_size,
                                   uft_crash_class_t crash_class,
                                   const char* description) {
    // Generate filename
    char bin_path[256];
    char test_path[256];
    snprintf(bin_path, sizeof(bin_path), "tests/regression/%s.bin", issue_id);
    snprintf(test_path, sizeof(test_path), "tests/regression/test_%s.c", issue_id);
    
    // Save crash input
    FILE* f = fopen(bin_path, "wb");
    if (!f) return UFT_ERROR_FILE_CREATE;
    fwrite(crash_input, 1, crash_size, f);
    fclose(f);
    
    // Generate test file
    f = fopen(test_path, "w");
    if (!f) return UFT_ERROR_FILE_CREATE;
    
    fprintf(f, "/**\n");
    fprintf(f, " * @file test_%s.c\n", issue_id);
    fprintf(f, " * @brief Regression test for %s\n", issue_id);
    fprintf(f, " * \n");
    fprintf(f, " * Description: %s\n", description);
    fprintf(f, " * Crash class: %d\n", crash_class);
    fprintf(f, " * Input file: %s\n", bin_path);
    fprintf(f, " */\n\n");
    
    fprintf(f, "#include \"uft/uft_test_framework.h\"\n");
    fprintf(f, "#include \"uft/uft_format_handlers.h\"\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n\n");
    
    fprintf(f, "int main(void) {\n");
    fprintf(f, "    // Load crash input\n");
    fprintf(f, "    FILE* f = fopen(\"%s\", \"rb\");\n", bin_path);
    fprintf(f, "    if (!f) return 1;\n");
    fprintf(f, "    \n");
    fprintf(f, "    fseek(f, 0, SEEK_END);\n");
    fprintf(f, "    size_t size = ftell(f);\n");
    fprintf(f, "    fseek(f, 0, SEEK_SET);\n");
    fprintf(f, "    \n");
    fprintf(f, "    uint8_t* data = malloc(size);\n");
    fprintf(f, "    fread(data, 1, size, f);\n");
    fprintf(f, "    fclose(f);\n");
    fprintf(f, "    \n");
    fprintf(f, "    // This input previously caused a crash.\n");
    fprintf(f, "    // After fix, it should return an error gracefully.\n");
    fprintf(f, "    uft_image_t* image = NULL;\n");
    fprintf(f, "    uft_error_t err = uft_format_probe_and_load(data, size, &image);\n");
    fprintf(f, "    \n");
    fprintf(f, "    // Should NOT crash, should return error\n");
    fprintf(f, "    if (image) uft_image_free(image);\n");
    fprintf(f, "    free(data);\n");
    fprintf(f, "    \n");
    fprintf(f, "    // Test passes if we reach here without crashing\n");
    fprintf(f, "    printf(\"Test %s: PASS\\n\");\n", issue_id);
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    
    fclose(f);
    
    printf("Created regression test:\n");
    printf("  Input: %s (%zu bytes)\n", bin_path, crash_size);
    printf("  Test:  %s\n", test_path);
    
    return UFT_OK;
}

// ============================================================================
// Run All Regression Tests
// ============================================================================

uft_test_result_t uft_regression_run_all(uft_test_stats_t* stats) {
    memset(stats, 0, sizeof(*stats));
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                        REGRESSION TEST SUITE\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    for (int i = 0; g_regression_tests[i].issue_id; i++) {
        const uft_regression_test_t* test = &g_regression_tests[i];
        stats->total++;
        
        printf("[%s] %s ... ", test->issue_id, test->description);
        
        // Check if input file exists
        struct stat st;
        if (stat(test->input_path, &st) != 0) {
            printf("SKIP (no input file)\n");
            stats->skipped++;
            continue;
        }
        
        // Load and run test
        FILE* f = fopen(test->input_path, "rb");
        if (!f) {
            printf("SKIP (cannot open)\n");
            stats->skipped++;
            continue;
        }
        
        uint8_t* data = malloc(st.st_size);
        fread(data, 1, st.st_size, f);
        fclose(f);
        
        // Run parser (should not crash)
        uft_image_t* image = NULL;
        uft_error_t err = uft_format_probe_and_load(data, st.st_size, &image);
        
        if (image) uft_image_free(image);
        free(data);
        
        // If we get here without crash, test passes
        if (test->fixed) {
            printf("PASS (fixed in %s)\n", test->fix_commit);
            stats->passed++;
        } else {
            printf("PASS (handled gracefully, not yet fixed)\n");
            stats->passed++;
        }
    }
    
    printf("\n");
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("Results: %d passed, %d failed, %d skipped (of %d total)\n",
           stats->passed, stats->failed, stats->skipped, stats->total);
    
    return (stats->failed == 0) ? UFT_TEST_PASS : UFT_TEST_FAIL;
}

// ============================================================================
// Minimize Crash Input (wrapper for afl-tmin)
// ============================================================================

uft_error_t uft_regression_minimize(const char* crash_input,
                                     const char* target,
                                     const char* output) {
    char cmd[1024];
    
    // Use afl-tmin to minimize
    snprintf(cmd, sizeof(cmd),
             "afl-tmin -i %s -o %s -- ./fuzz_%s @@",
             crash_input, output, target);
    
    printf("Minimizing crash input...\n");
    printf("  Command: %s\n", cmd);
    
    int ret = system(cmd);
    if (ret != 0) {
        printf("  Warning: afl-tmin failed or not available\n");
        // Fall back to copying original
        snprintf(cmd, sizeof(cmd), "cp %s %s", crash_input, output);
        system(cmd);
    }
    
    return UFT_OK;
}
