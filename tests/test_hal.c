/**
 * @file test_hal.c
 * @brief Unit tests for Hardware Abstraction Layer
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "uft/hal/uft_hal.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

TEST(controller_names) {
    /* All controller types should have names */
    for (int i = 0; i < HAL_CTRL_COUNT; i++) {
        const char *name = uft_hal_controller_name((uft_hal_controller_t)i);
        assert(name != NULL);
        assert(strlen(name) > 0);
        assert(strcmp(name, "Unknown") != 0);
    }
    
    /* Invalid type should return "Unknown" */
    const char *unknown = uft_hal_controller_name((uft_hal_controller_t)99);
    assert(strcmp(unknown, "Unknown") == 0);
}

TEST(controller_count) {
    int count = uft_hal_get_controller_count();
    assert(count == HAL_CTRL_COUNT);
    assert(count >= 8);  /* At least 8 controllers defined */
}

TEST(controller_by_index) {
    for (int i = 0; i < uft_hal_get_controller_count(); i++) {
        const char *name = uft_hal_get_controller_name_by_index(i);
        assert(name != NULL);
        assert(strlen(name) > 0);
    }
    
    /* Out of range should return NULL */
    assert(uft_hal_get_controller_name_by_index(-1) == NULL);
    assert(uft_hal_get_controller_name_by_index(100) == NULL);
}

TEST(controller_implemented) {
    /* Greaseweazle and FluxEngine should be implemented */
    assert(uft_hal_is_controller_implemented(HAL_CTRL_GREASEWEAZLE) == true);
    assert(uft_hal_is_controller_implemented(HAL_CTRL_FLUXENGINE) == true);
    
    /* Kryoflux and SCP should be partially implemented */
    assert(uft_hal_is_controller_implemented(HAL_CTRL_KRYOFLUX) == true);
    assert(uft_hal_is_controller_implemented(HAL_CTRL_SCP) == true);
    
    /* Others are stubs */
    assert(uft_hal_is_controller_implemented(HAL_CTRL_APPLESAUCE) == false);
    assert(uft_hal_is_controller_implemented(HAL_CTRL_XUM1541) == false);
    assert(uft_hal_is_controller_implemented(HAL_CTRL_ZOOMFLOPPY) == false);
    assert(uft_hal_is_controller_implemented(HAL_CTRL_FC5025) == false);
}

TEST(enumerate_empty) {
    /* Without hardware, enumerate should return 0 */
    uft_hal_controller_t controllers[10];
    int found = uft_hal_enumerate(controllers, 10);
    assert(found == 0);  /* No hardware connected in test env */
}

TEST(open_invalid_path) {
    /* Opening non-existent device should fail */
    uft_hal_t *hal = uft_hal_open(HAL_CTRL_GREASEWEAZLE, "/dev/nonexistent");
    assert(hal == NULL);
    
    hal = uft_hal_open(HAL_CTRL_SCP, "/dev/nonexistent");
    assert(hal == NULL);
}

TEST(null_handle_safety) {
    /* All functions should handle NULL gracefully */
    assert(uft_hal_get_caps(NULL, NULL) == -1);
    assert(uft_hal_read_flux(NULL, 0, 0, 1, NULL, NULL) == -1);
    assert(uft_hal_write_flux(NULL, 0, 0, NULL, 0) == -1);
    assert(uft_hal_seek(NULL, 0) == -1);
    assert(uft_hal_motor(NULL, true) == -1);
    
    const char *err = uft_hal_get_error(NULL);
    assert(err != NULL);
    assert(strcmp(err, "NULL handle") == 0);
    
    /* Close NULL should not crash */
    uft_hal_close(NULL);
}

int main(void) {
    printf("=== HAL Unit Tests ===\n");
    
    RUN_TEST(controller_names);
    RUN_TEST(controller_count);
    RUN_TEST(controller_by_index);
    RUN_TEST(controller_implemented);
    RUN_TEST(enumerate_empty);
    RUN_TEST(open_invalid_path);
    RUN_TEST(null_handle_safety);
    
    printf("\nAll tests passed!\n");
    return 0;
}
