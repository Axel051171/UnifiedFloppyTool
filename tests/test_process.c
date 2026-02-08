/**
 * @file test_process.c
 * @brief Tests for Cross-Platform Process Execution (W-P1-001)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_process.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)

/*============================================================================
 * TESTS: BASIC EXECUTION
 *============================================================================*/

TEST(exec_echo) {
    uft_process_result_t result;
    int rc = uft_process_exec("echo hello", NULL, &result);
    
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_NOT_NULL(result.stdout_data);
    ASSERT_TRUE(strstr(result.stdout_data, "hello") != NULL);
    
    uft_process_result_free(&result);
}

TEST(exec_exit_code) {
    uft_process_result_t result;
    
#ifdef _WIN32
    int rc = uft_process_exec("cmd /c exit 42", NULL, &result);
#else
    int rc = uft_process_exec("exit 42", NULL, &result);
#endif
    
    ASSERT_EQ(rc, 0);
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.exit_code, 42);
    
    uft_process_result_free(&result);
}

TEST(exec_capture_stdout) {
    uft_process_result_t result;
    uft_process_options_t opts = UFT_PROCESS_OPTIONS_DEFAULT;
    opts.capture_stdout = true;
    opts.capture_stderr = false;
    
    int rc = uft_process_exec("echo test_output", &opts, &result);
    
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(result.stdout_data);
    ASSERT_TRUE(result.stdout_size > 0);
    
    uft_process_result_free(&result);
}

TEST(exec_no_capture) {
    uft_process_result_t result;
    uft_process_options_t opts = UFT_PROCESS_OPTIONS_DEFAULT;
    opts.capture_stdout = false;
    opts.capture_stderr = false;
    
    int rc = uft_process_exec("echo silent", &opts, &result);
    
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.success);
    ASSERT_NULL(result.stdout_data);
    
    uft_process_result_free(&result);
}

TEST(exec_invalid_command) {
    uft_process_result_t result;
    int rc = uft_process_exec("this_command_does_not_exist_xyz123", NULL, &result);
    
    /* Should not crash, but command should fail */
    ASSERT_EQ(rc, 0);
    ASSERT_FALSE(result.success);
    ASSERT_NE(result.exit_code, 0);
    
    uft_process_result_free(&result);
}

TEST(exec_null_args) {
    uft_process_result_t result;
    
    ASSERT_EQ(uft_process_exec(NULL, NULL, &result), -1);
    ASSERT_EQ(uft_process_exec("echo", NULL, NULL), -1);
}

/*============================================================================
 * TESTS: SIMPLE EXECUTION
 *============================================================================*/

TEST(run_simple) {
    int rc = uft_process_run("echo test");
    ASSERT_EQ(rc, 0);
}

TEST(run_failure) {
#ifdef _WIN32
    int rc = uft_process_run("cmd /c exit 1");
#else
    int rc = uft_process_run("exit 1");
#endif
    ASSERT_EQ(rc, 1);
}

TEST(output_line) {
    char output[256];
    int rc = uft_process_output_line("echo single_line", output, sizeof(output));
    
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(strstr(output, "single_line") != NULL);
}

TEST(output_line_null) {
    char output[256];
    ASSERT_EQ(uft_process_output_line(NULL, output, sizeof(output)), -1);
    ASSERT_EQ(uft_process_output_line("echo", NULL, sizeof(output)), -1);
    ASSERT_EQ(uft_process_output_line("echo", output, 0), -1);
}

/*============================================================================
 * TESTS: TOOL DETECTION
 *============================================================================*/

TEST(tool_exists_echo) {
    /* 'echo' should exist on all platforms */
#ifdef _WIN32
    ASSERT_TRUE(uft_tool_exists("cmd"));
#else
    ASSERT_TRUE(uft_tool_exists("echo"));
#endif
}

TEST(tool_exists_nonexistent) {
    ASSERT_FALSE(uft_tool_exists("this_tool_does_not_exist_xyz789"));
}

TEST(tool_find) {
    char path[256];
#ifdef _WIN32
    int rc = uft_tool_find("cmd", path, sizeof(path));
#else
    int rc = uft_tool_find("ls", path, sizeof(path));
#endif
    
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(strlen(path) > 0);
}

TEST(tool_find_nonexistent) {
    char path[256];
    int rc = uft_tool_find("nonexistent_tool_abc123", path, sizeof(path));
    
    ASSERT_NE(rc, 0);
}

/*============================================================================
 * TESTS: TOOL REGISTRY
 *============================================================================*/

TEST(tool_detect_all) {
    uft_tool_info_t tools[UFT_TOOL_COUNT];
    int count = uft_tool_detect_all(tools, UFT_TOOL_COUNT);
    
    ASSERT_EQ(count, UFT_TOOL_COUNT);
    
    /* Check that all tools have names */
    for (int i = 0; i < count; i++) {
        ASSERT_NOT_NULL(tools[i].name);
        ASSERT_TRUE(strlen(tools[i].name) > 0);
    }
}

TEST(tool_get_info) {
    for (int i = 0; i < UFT_TOOL_COUNT; i++) {
        const uft_tool_info_t *info = uft_tool_get_info((uft_tool_id_t)i);
        ASSERT_NOT_NULL(info);
        ASSERT_NOT_NULL(info->name);
    }
}

TEST(tool_get_info_invalid) {
    const uft_tool_info_t *info = uft_tool_get_info((uft_tool_id_t)999);
    ASSERT_NULL(info);
}

/*============================================================================
 * TESTS: RESULT FREE
 *============================================================================*/

TEST(result_free_null) {
    /* Should not crash */
    uft_process_result_free(NULL);
}

TEST(result_free_empty) {
    uft_process_result_t result = {0};
    uft_process_result_free(&result);
    
    /* Should be zeroed */
    ASSERT_NULL(result.stdout_data);
    ASSERT_NULL(result.stderr_data);
}

TEST(result_free_with_data) {
    uft_process_result_t result;
    uft_process_exec("echo data", NULL, &result);
    
    ASSERT_NOT_NULL(result.stdout_data);
    
    uft_process_result_free(&result);
    
    ASSERT_NULL(result.stdout_data);
    ASSERT_NULL(result.stderr_data);
}

/*============================================================================
 * TESTS: EXEC_ARGS
 *============================================================================*/

TEST(exec_args_simple) {
    uft_process_result_t result;
    const char *args[] = {"hello", "world", NULL};
    
    int rc = uft_process_exec_args("echo", args, NULL, &result);
    
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.success);
    ASSERT_NOT_NULL(result.stdout_data);
    ASSERT_TRUE(strstr(result.stdout_data, "hello") != NULL);
    
    uft_process_result_free(&result);
}

TEST(exec_args_null) {
    uft_process_result_t result;
    ASSERT_EQ(uft_process_exec_args(NULL, NULL, NULL, &result), -1);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Process Execution Tests (W-P1-001)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Basic Execution */
    printf("[SUITE] Basic Execution\n");
    RUN_TEST(exec_echo);
    RUN_TEST(exec_exit_code);
    RUN_TEST(exec_capture_stdout);
    RUN_TEST(exec_no_capture);
    RUN_TEST(exec_invalid_command);
    RUN_TEST(exec_null_args);
    
    /* Simple Execution */
    printf("\n[SUITE] Simple Execution\n");
    RUN_TEST(run_simple);
    RUN_TEST(run_failure);
    RUN_TEST(output_line);
    RUN_TEST(output_line_null);
    
    /* Tool Detection */
    printf("\n[SUITE] Tool Detection\n");
    RUN_TEST(tool_exists_echo);
    RUN_TEST(tool_exists_nonexistent);
    RUN_TEST(tool_find);
    RUN_TEST(tool_find_nonexistent);
    
    /* Tool Registry */
    printf("\n[SUITE] Tool Registry\n");
    RUN_TEST(tool_detect_all);
    RUN_TEST(tool_get_info);
    RUN_TEST(tool_get_info_invalid);
    
    /* Result Free */
    printf("\n[SUITE] Result Free\n");
    RUN_TEST(result_free_null);
    RUN_TEST(result_free_empty);
    RUN_TEST(result_free_with_data);
    
    /* Exec Args */
    printf("\n[SUITE] Exec Args\n");
    RUN_TEST(exec_args_simple);
    RUN_TEST(exec_args_null);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
