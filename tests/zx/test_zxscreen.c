/**
 * @file test_zxscreen.c
 * @brief Tests for ZX Spectrum Screen Converter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/zx/uft_zxscreen.h"

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test) do { \
    printf("  %-40s", #test "..."); \
    tests_run++; \
    test(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Data
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint8_t *create_test_screen(void) {
    uint8_t *data = calloc(1, ZXSCREEN_TOTAL_SIZE);
    if (!data) return NULL;
    
    /* Fill bitmap with a checkerboard pattern */
    for (int i = 0; i < ZXSCREEN_BITMAP_SIZE; i++) {
        data[i] = 0xAA;  /* 10101010 */
    }
    
    /* Fill attributes with different colors */
    for (int cell_y = 0; cell_y < 24; cell_y++) {
        for (int cell_x = 0; cell_x < 32; cell_x++) {
            int attr_idx = ZXSCREEN_BITMAP_SIZE + cell_y * 32 + cell_x;
            /* Ink = (x % 8), Paper = (y % 8), Bright = (x+y) % 2 */
            uint8_t ink = cell_x % 8;
            uint8_t paper = cell_y % 8;
            uint8_t bright = ((cell_x + cell_y) % 2) << 6;
            data[attr_idx] = (paper << 3) | ink | bright;
        }
    }
    
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_init(void) {
    uft_zxscreen_t screen;
    uft_zxscreen_init(&screen);
    
    assert(screen.valid == false);
    assert(screen.border_color == ZX_WHITE);
}

static void test_load_valid(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    bool result = uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    
    assert(result == true);
    assert(screen.valid == true);
    
    free(data);
}

static void test_load_invalid_size(void) {
    uint8_t data[100] = {0};
    
    uft_zxscreen_t screen;
    bool result = uft_zxscreen_load(&screen, data, sizeof(data));
    
    assert(result == false);
    assert(screen.valid == false);
}

static void test_get_attr(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    
    /* Cell (0,0): ink=0, paper=0, bright=0 */
    uft_zxattr_t attr00 = uft_zxscreen_get_attr(&screen, 0, 0);
    assert(attr00.ink == 0);
    assert(attr00.paper == 0);
    assert(attr00.bright == false);
    
    /* Cell (1,0): ink=1, paper=0, bright=1 */
    uft_zxattr_t attr10 = uft_zxscreen_get_attr(&screen, 1, 0);
    assert(attr10.ink == 1);
    assert(attr10.paper == 0);
    assert(attr10.bright == true);
    
    /* Cell (0,1): ink=0, paper=1, bright=1 */
    uft_zxattr_t attr01 = uft_zxscreen_get_attr(&screen, 0, 1);
    assert(attr01.ink == 0);
    assert(attr01.paper == 1);
    assert(attr01.bright == true);
    
    free(data);
}

static void test_get_pixel(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    
    /* We filled with 0xAA = 10101010, so alternating pixels */
    /* Note: ZX screen layout is complex, but pixel 0 should be 1 (MSB first) */
    int p0 = uft_zxscreen_get_pixel(&screen, 0, 0);
    int p1 = uft_zxscreen_get_pixel(&screen, 1, 0);
    
    /* 0xAA = 10101010, so pixel 0 = 1, pixel 1 = 0 */
    assert(p0 == 1);
    assert(p1 == 0);
    
    free(data);
}

static void test_palette_normal(void) {
    const uft_rgb_t *palette = uft_zxscreen_get_palette(false);
    
    /* Black */
    assert(palette[0].r == 0x00);
    assert(palette[0].g == 0x00);
    assert(palette[0].b == 0x00);
    
    /* White (not bright) */
    assert(palette[7].r == 0xD7);
    assert(palette[7].g == 0xD7);
    assert(palette[7].b == 0xD7);
}

static void test_palette_bright(void) {
    const uft_rgb_t *palette = uft_zxscreen_get_palette(true);
    
    /* Black (same as normal) */
    assert(palette[0].r == 0x00);
    assert(palette[0].g == 0x00);
    assert(palette[0].b == 0x00);
    
    /* Bright White */
    assert(palette[7].r == 0xFF);
    assert(palette[7].g == 0xFF);
    assert(palette[7].b == 0xFF);
}

static void test_get_color(void) {
    uft_rgb_t red_normal = uft_zxscreen_get_color(ZX_RED, false);
    assert(red_normal.r == 0xD7);
    assert(red_normal.g == 0x00);
    assert(red_normal.b == 0x00);
    
    uft_rgb_t red_bright = uft_zxscreen_get_color(ZX_RED, true);
    assert(red_bright.r == 0xFF);
    assert(red_bright.g == 0x00);
    assert(red_bright.b == 0x00);
}

static void test_to_rgb(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    
    uint8_t *rgb = uft_zxscreen_to_rgb(&screen);
    assert(rgb != NULL);
    
    /* Check buffer size is correct */
    /* Just verify first pixel is valid */
    /* Pixel (0,0) in cell (0,0): ink=0 (black), pixel value=1, so color=ink=black */
    assert(rgb[0] == 0x00);  /* R */
    assert(rgb[1] == 0x00);  /* G */
    assert(rgb[2] == 0x00);  /* B */
    
    free(rgb);
    free(data);
}

static void test_to_rgba_with_border(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    screen.border_color = ZX_BLUE;
    
    int width, height;
    uint8_t *rgba = uft_zxscreen_to_rgba_with_border(&screen, 32, &width, &height);
    assert(rgba != NULL);
    assert(width == 256 + 64);
    assert(height == 192 + 64);
    
    /* Check border pixel (0,0) is blue */
    assert(rgba[0] == 0x00);  /* R */
    assert(rgba[1] == 0x00);  /* G */
    assert(rgba[2] == 0xD7);  /* B (normal blue) */
    assert(rgba[3] == 0xFF);  /* A */
    
    free(rgba);
    free(data);
}

static void test_export_bmp(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    
    const char *filename = "/tmp/test_zxscreen.bmp";
    bool result = uft_zxscreen_export_bmp(&screen, filename);
    assert(result == true);
    
    /* Verify file exists and has correct size */
    FILE *f = fopen(filename, "rb");
    assert(f != NULL);
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    /* BMP size: 54 (header) + 256*192*3 (padded to 4 bytes per row) */
    /* Row size = ((256*3 + 3) / 4) * 4 = 768 */
    /* Expected = 54 + 768 * 192 = 147510 */
    assert(size == 54 + 768 * 192);
    
    /* Cleanup */
    remove(filename);
    free(data);
}

static void test_boundary_conditions(void) {
    uint8_t *data = create_test_screen();
    assert(data != NULL);
    
    uft_zxscreen_t screen;
    uft_zxscreen_load(&screen, data, ZXSCREEN_TOTAL_SIZE);
    
    /* Test boundary pixels */
    int p_last_x = uft_zxscreen_get_pixel(&screen, 255, 0);
    int p_last_y = uft_zxscreen_get_pixel(&screen, 0, 191);
    int p_corner = uft_zxscreen_get_pixel(&screen, 255, 191);
    
    /* Should not crash, values should be valid (0 or 1) */
    assert(p_last_x == 0 || p_last_x == 1);
    assert(p_last_y == 0 || p_last_y == 1);
    assert(p_corner == 0 || p_corner == 1);
    
    /* Test out of bounds (should return 0) */
    int p_oob = uft_zxscreen_get_pixel(&screen, 256, 0);
    assert(p_oob == 0);
    
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" ZX Screen Converter Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("Basic Tests:\n");
    RUN_TEST(test_init);
    RUN_TEST(test_load_valid);
    RUN_TEST(test_load_invalid_size);
    
    printf("\nAttribute Tests:\n");
    RUN_TEST(test_get_attr);
    RUN_TEST(test_get_pixel);
    
    printf("\nPalette Tests:\n");
    RUN_TEST(test_palette_normal);
    RUN_TEST(test_palette_bright);
    RUN_TEST(test_get_color);
    
    printf("\nConversion Tests:\n");
    RUN_TEST(test_to_rgb);
    RUN_TEST(test_to_rgba_with_border);
    RUN_TEST(test_export_bmp);
    
    printf("\nBoundary Tests:\n");
    RUN_TEST(test_boundary_conditions);
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf(" ✓ All ZX Screen tests passed! (%d/%d)\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}
