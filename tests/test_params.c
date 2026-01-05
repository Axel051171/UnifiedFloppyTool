/**
 * @file test_params.c
 * @brief Tests für Parameter-System
 */

#include "uft/uft_params.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("TEST: %s... ", #name)
#define PASS() printf("PASS\n")
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_default_params(void) {
    TEST(default_params);
    
    uft_params_t p = uft_params_default();
    
    if (p.struct_size != sizeof(uft_params_t)) FAIL("struct_size wrong");
    if (p.global.device_index != -1) FAIL("device_index should be -1");
    if (p.global.global_retries != 3) FAIL("retries should be 3");
    if (p.geometry.cylinder_start != 0) FAIL("cylinder_start should be 0");
    if (p.decoder.pll.tolerance != 0.25) FAIL("tolerance should be 0.25");
    
    PASS();
    return 0;
}

static int test_preset_params(void) {
    TEST(preset_params);
    
    uft_params_t p = uft_params_for_preset(UFT_GEO_PC_1440K);
    
    if (p.geometry.total_cylinders != 80) FAIL("cylinders should be 80");
    if (p.geometry.total_heads != 2) FAIL("heads should be 2");
    if (p.geometry.sectors_per_track != 18) FAIL("sectors should be 18");
    if (p.geometry.sector_size != 512) FAIL("sector_size should be 512");
    
    PASS();
    return 0;
}

static int test_format_params(void) {
    TEST(format_params);
    
    uft_params_t p = uft_params_for_format(UFT_FORMAT_D64);
    
    if (p.format.output_format != UFT_FORMAT_D64) FAIL("format wrong");
    if (p.geometry.sector_size != 256) FAIL("D64 needs 256-byte sectors");
    if (p.geometry.total_heads != 1) FAIL("D64 is single-sided");
    
    PASS();
    return 0;
}

static int test_validation_pass(void) {
    TEST(validation_pass);
    
    uft_params_t p = uft_params_for_preset(UFT_GEO_PC_1440K);
    uft_error_t err = uft_params_validate(&p);
    
    if (err != UFT_OK) FAIL("valid params should pass");
    if (!p.is_valid) FAIL("is_valid should be true");
    
    PASS();
    return 0;
}

static int test_validation_fail_cylinder(void) {
    TEST(validation_fail_cylinder);
    
    uft_params_t p = uft_params_default();
    p.geometry.cylinder_start = 50;
    p.geometry.cylinder_end = 10;  // Invalid: end < start
    
    uft_error_t err = uft_params_validate(&p);
    
    if (err == UFT_OK) FAIL("should fail validation");
    if (p.is_valid) FAIL("is_valid should be false");
    
    PASS();
    return 0;
}

static int test_validation_fail_pll(void) {
    TEST(validation_fail_pll);
    
    uft_params_t p = uft_params_default();
    p.decoder.pll.tolerance = 0.0;  // Invalid: must be > 0
    
    uft_error_t err = uft_params_validate(&p);
    
    if (err == UFT_OK) FAIL("should fail validation");
    
    PASS();
    return 0;
}

static int test_schema_lookup(void) {
    TEST(schema_lookup);
    
    const uft_param_schema_t* schema = 
        uft_params_get_schema_by_name("geometry.cylinder_start");
    
    if (!schema) FAIL("schema not found");
    if (strcmp(schema->name, "geometry.cylinder_start") != 0) FAIL("wrong schema");
    if (schema->type != UFT_PARAM_TYPE_INT) FAIL("wrong type");
    
    PASS();
    return 0;
}

static int test_schema_category(void) {
    TEST(schema_category);
    
    const uft_param_schema_t* schemas[32];
    int count = uft_params_get_by_category(UFT_PARAM_CAT_GEOMETRY, schemas, 32);
    
    if (count == 0) FAIL("no geometry params found");
    
    for (int i = 0; i < count; i++) {
        if (schemas[i]->category != UFT_PARAM_CAT_GEOMETRY) {
            FAIL("wrong category");
        }
    }
    
    printf("(%d params) ", count);
    PASS();
    return 0;
}

int main(void) {
    printf("=== Parameter System Tests ===\n\n");
    
    int failures = 0;
    failures += test_default_params();
    failures += test_preset_params();
    failures += test_format_params();
    failures += test_validation_pass();
    failures += test_validation_fail_cylinder();
    failures += test_validation_fail_pll();
    failures += test_schema_lookup();
    failures += test_schema_category();
    
    printf("\n%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}
