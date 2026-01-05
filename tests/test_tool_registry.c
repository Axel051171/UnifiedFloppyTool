/**
 * @file test_tool_registry.c
 * @brief Unit Tests für Tool Registry
 */

#include "uft/uft_tool_adapter.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) printf("TEST: %s... ", #name)
#define PASS() printf("PASS\n")
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_registry_init(void) {
    TEST(registry_init);
    
    uft_error_t err = uft_tool_registry_init();
    if (err != UFT_OK) FAIL("init failed");
    
    PASS();
    return 0;
}

static int test_list_tools(void) {
    TEST(list_tools);
    
    const uft_tool_adapter_t* tools[16];
    size_t count = uft_tool_list(tools, 16);
    
    if (count == 0) FAIL("no tools registered");
    
    printf("(%zu tools) ", count);
    
    for (size_t i = 0; i < count; i++) {
        printf("%s ", tools[i]->name);
    }
    
    PASS();
    return 0;
}

static int test_find_tool(void) {
    TEST(find_tool);
    
    const uft_tool_adapter_t* gw = uft_tool_find("gw");
    if (!gw) FAIL("greaseweazle not found");
    if (strcmp(gw->name, "gw") != 0) FAIL("wrong tool returned");
    
    PASS();
    return 0;
}

static int test_find_for_format(void) {
    TEST(find_for_format);
    
    const uft_tool_adapter_t* tool = uft_tool_find_for_format(UFT_FORMAT_SCP);
    // May be NULL if no tools available - that's OK
    printf("(found: %s) ", tool ? tool->name : "none");
    
    PASS();
    return 0;
}

static int test_find_for_operation(void) {
    TEST(find_for_operation);
    
    const uft_tool_adapter_t* tool = uft_tool_find_for_operation(
        UFT_TOOL_CAP_READ | UFT_TOOL_CAP_FLUX);
    printf("(found: %s) ", tool ? tool->name : "none");
    
    PASS();
    return 0;
}

static int test_list_available(void) {
    TEST(list_available);
    
    const uft_tool_adapter_t* tools[16];
    size_t count = uft_tool_list_available(tools, 16);
    
    printf("(%zu available) ", count);
    
    PASS();
    return 0;
}

int main(void) {
    printf("=== Tool Registry Tests ===\n\n");
    
    int failures = 0;
    failures += test_registry_init();
    failures += test_list_tools();
    failures += test_find_tool();
    failures += test_find_for_format();
    failures += test_find_for_operation();
    failures += test_list_available();
    
    uft_tool_registry_shutdown();
    
    printf("\n%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}
