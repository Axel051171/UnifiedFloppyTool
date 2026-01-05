/**
 * @file test_param_bridge.c
 * @brief Unit tests for uft_param_bridge.h
 */

#include "uft/params/uft_param_bridge.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

static int passed = 0, failed = 0, total = 0;

TEST(bridge_create_destroy) {
    uft_param_bridge_t* bridge = uft_param_bridge_create();
    if (!bridge) return -1;
    uft_param_bridge_destroy(bridge);
    return 0;
}

TEST(bridge_set_get_int) {
    uft_param_bridge_t* bridge = uft_param_bridge_create();
    if (!bridge) return -1;
    
    int ret = uft_param_set_int(bridge, "pll.revolutions", 5);
    if (ret != 0) { uft_param_bridge_destroy(bridge); return -1; }
    
    int64_t val = uft_param_get_int(bridge, "pll.revolutions");
    uft_param_bridge_destroy(bridge);
    
    return (val == 5) ? 0 : -1;
}

TEST(bridge_set_get_double) {
    uft_param_bridge_t* bridge = uft_param_bridge_create();
    if (!bridge) return -1;
    
    int ret = uft_param_set_double(bridge, "pll.bandwidth", 0.05);
    if (ret != 0) { uft_param_bridge_destroy(bridge); return -1; }
    
    double val = uft_param_get_double(bridge, "pll.bandwidth");
    uft_param_bridge_destroy(bridge);
    
    return (val > 0.04 && val < 0.06) ? 0 : -1;
}

TEST(bridge_set_get_bool) {
    uft_param_bridge_t* bridge = uft_param_bridge_create();
    if (!bridge) return -1;
    
    int ret = uft_param_set_bool(bridge, "verify.enabled", true);
    if (ret != 0) { uft_param_bridge_destroy(bridge); return -1; }
    
    bool val = uft_param_get_bool(bridge, "verify.enabled");
    uft_param_bridge_destroy(bridge);
    
    return val ? 0 : -1;
}

TEST(bridge_transaction_rollback) {
    uft_param_bridge_t* bridge = uft_param_bridge_create();
    if (!bridge) return -1;
    
    uft_param_set_int(bridge, "test.value", 10);
    
    uft_param_begin_transaction(bridge);
    uft_param_set_int(bridge, "test.value", 99);
    uft_param_rollback_transaction(bridge);
    
    int64_t val = uft_param_get_int(bridge, "test.value");
    uft_param_bridge_destroy(bridge);
    
    return (val == 10) ? 0 : -1;
}

TEST(bridge_export_json) {
    uft_param_bridge_t* bridge = uft_param_bridge_create();
    if (!bridge) return -1;
    
    uft_param_set_int(bridge, "test.int", 42);
    uft_param_set_string(bridge, "test.str", "hello");
    
    char buf[4096];
    int ret = uft_param_export_json(bridge, buf, sizeof(buf));
    uft_param_bridge_destroy(bridge);
    
    if (ret != 0) return -1;
    if (strlen(buf) == 0) return -1;
    if (!strstr(buf, "42")) return -1;
    
    return 0;
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         UFT PARAM BRIDGE API TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    RUN(bridge_create_destroy);
    RUN(bridge_set_get_int);
    RUN(bridge_set_get_double);
    RUN(bridge_set_get_bool);
    RUN(bridge_transaction_rollback);
    RUN(bridge_export_json);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", passed, total, failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return (failed == 0) ? 0 : 1;
}
