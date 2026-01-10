/**
 * @file uft_zxscreen.c
 * @brief ZX Spectrum Screen Converter Implementation
 * @version 1.0.0
 * 
 * ZX Spectrum screen memory layout:
 * 
 * Bitmap (6144 bytes):
 *   Lines are interleaved in a complex pattern:
 *   - Screen is divided into 3 "thirds" of 64 lines each
 *   - Within each third, lines are interleaved by 8
 *   - Address = (line/64)*2048 + (line%8)*256 + ((line/8)%8)*32 + x/8
 * 
 * Attributes (768 bytes):
 *   - 32×24 cells, 1 byte per cell
 *   - Bits: F B P2 P1 P0 I2 I1 I0
 *   - F=Flash, B=Bright, P=Paper(0-7), I=Ink(0-7)
 */

#include "uft/zx/uft_zxscreen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Color Palettes
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Standard ZX Spectrum palette (normal brightness) */
static const uft_rgb_t zx_palette_normal[8] = {
    {0x00, 0x00, 0x00},  /* 0: Black */
    {0x00, 0x00, 0xD7},  /* 1: Blue */
    {0xD7, 0x00, 0x00},  /* 2: Red */
    {0xD7, 0x00, 0xD7},  /* 3: Magenta */
    {0x00, 0xD7, 0x00},  /* 4: Green */
    {0x00, 0xD7, 0xD7},  /* 5: Cyan */
    {0xD7, 0xD7, 0x00},  /* 6: Yellow */
    {0xD7, 0xD7, 0xD7}   /* 7: White */
};

/* Bright palette */
static const uft_rgb_t zx_palette_bright[8] = {
    {0x00, 0x00, 0x00},  /* 0: Black (same as normal) */
    {0x00, 0x00, 0xFF},  /* 1: Bright Blue */
    {0xFF, 0x00, 0x00},  /* 2: Bright Red */
    {0xFF, 0x00, 0xFF},  /* 3: Bright Magenta */
    {0x00, 0xFF, 0x00},  /* 4: Bright Green */
    {0x00, 0xFF, 0xFF},  /* 5: Bright Cyan */
    {0xFF, 0xFF, 0x00},  /* 6: Bright Yellow */
    {0xFF, 0xFF, 0xFF}   /* 7: Bright White */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate bitmap address for a pixel
 * 
 * ZX Spectrum screen memory layout is notoriously complex:
 * - Y coordinate bits: Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
 * - Address bits:      0  1  0  Y7 Y6 Y2 Y1 Y0 | Y5 Y4 Y3 X7 X6 X5 X4 X3
 */
static int zx_bitmap_address(int x, int y) {
    int line_addr = ((y & 0xC0) << 5) |  /* Y7 Y6 -> bits 11-12 */
                    ((y & 0x07) << 8) |   /* Y2 Y1 Y0 -> bits 8-10 */
                    ((y & 0x38) << 2);    /* Y5 Y4 Y3 -> bits 5-7 */
    int col = x >> 3;
    return line_addr + col;
}

/**
 * @brief Calculate attribute address for a cell
 */
static int zx_attr_address(int cell_x, int cell_y) {
    return cell_y * 32 + cell_x;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Public Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_zxscreen_init(uft_zxscreen_t *screen) {
    if (!screen) return;
    
    memset(screen, 0, sizeof(uft_zxscreen_t));
    screen->valid = false;
    screen->use_bright_black = false;
    screen->border_color = ZX_WHITE;
}

bool uft_zxscreen_load(uft_zxscreen_t *screen, const uint8_t *data, size_t size) {
    if (!screen || !data) return false;
    
    uft_zxscreen_init(screen);
    
    if (size < ZXSCREEN_TOTAL_SIZE) {
        return false;
    }
    
    memcpy(screen->bitmap, data, ZXSCREEN_BITMAP_SIZE);
    memcpy(screen->attrs, data + ZXSCREEN_BITMAP_SIZE, ZXSCREEN_ATTR_SIZE);
    screen->valid = true;
    
    return true;
}

uft_zxattr_t uft_zxscreen_get_attr(const uft_zxscreen_t *screen, int cell_x, int cell_y) {
    uft_zxattr_t attr = {0};
    
    if (!screen || !screen->valid) return attr;
    if (cell_x < 0 || cell_x >= ZXSCREEN_CELLS_X) return attr;
    if (cell_y < 0 || cell_y >= ZXSCREEN_CELLS_Y) return attr;
    
    uint8_t byte = screen->attrs[zx_attr_address(cell_x, cell_y)];
    
    attr.ink = byte & 0x07;
    attr.paper = (byte >> 3) & 0x07;
    attr.bright = (byte >> 6) & 0x01;
    attr.flash = (byte >> 7) & 0x01;
    
    return attr;
}

int uft_zxscreen_get_pixel(const uft_zxscreen_t *screen, int x, int y) {
    if (!screen || !screen->valid) return 0;
    if (x < 0 || x >= ZXSCREEN_WIDTH) return 0;
    if (y < 0 || y >= ZXSCREEN_HEIGHT) return 0;
    
    int addr = zx_bitmap_address(x, y);
    if (addr < 0 || addr >= ZXSCREEN_BITMAP_SIZE) return 0;
    
    uint8_t byte = screen->bitmap[addr];
    int bit = 7 - (x & 7);  /* MSB first */
    
    return (byte >> bit) & 1;
}

const uft_rgb_t *uft_zxscreen_get_palette(bool bright) {
    return bright ? zx_palette_bright : zx_palette_normal;
}

uft_rgb_t uft_zxscreen_get_color(int color, bool bright) {
    if (color < 0 || color > 7) color = 0;
    return bright ? zx_palette_bright[color] : zx_palette_normal[color];
}

uft_rgb_t uft_zxscreen_get_pixel_rgb(const uft_zxscreen_t *screen, int x, int y) {
    uft_rgb_t color = {0, 0, 0};
    
    if (!screen || !screen->valid) return color;
    
    int cell_x = x / ZXSCREEN_CELL_SIZE;
    int cell_y = y / ZXSCREEN_CELL_SIZE;
    
    uft_zxattr_t attr = uft_zxscreen_get_attr(screen, cell_x, cell_y);
    int pixel = uft_zxscreen_get_pixel(screen, x, y);
    
    int color_idx = pixel ? attr.ink : attr.paper;
    return uft_zxscreen_get_color(color_idx, attr.bright);
}

uint8_t *uft_zxscreen_to_rgb(const uft_zxscreen_t *screen) {
    if (!screen || !screen->valid) return NULL;
    
    size_t size = ZXSCREEN_WIDTH * ZXSCREEN_HEIGHT * 3;
    uint8_t *rgb = malloc(size);
    if (!rgb) return NULL;
    
    for (int y = 0; y < ZXSCREEN_HEIGHT; y++) {
        for (int x = 0; x < ZXSCREEN_WIDTH; x++) {
            uft_rgb_t color = uft_zxscreen_get_pixel_rgb(screen, x, y);
            int offset = (y * ZXSCREEN_WIDTH + x) * 3;
            rgb[offset + 0] = color.r;
            rgb[offset + 1] = color.g;
            rgb[offset + 2] = color.b;
        }
    }
    
    return rgb;
}

uint8_t *uft_zxscreen_to_rgba_with_border(
    const uft_zxscreen_t *screen,
    int border_size,
    int *width,
    int *height
) {
    if (!screen || !screen->valid) return NULL;
    
    int w = ZXSCREEN_WIDTH + border_size * 2;
    int h = ZXSCREEN_HEIGHT + border_size * 2;
    
    if (width) *width = w;
    if (height) *height = h;
    
    size_t size = w * h * 4;
    uint8_t *rgba = malloc(size);
    if (!rgba) return NULL;
    
    /* Get border color */
    uft_rgb_t border = uft_zxscreen_get_color(screen->border_color & 0x07, false);
    
    /* Fill with border color */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int offset = (y * w + x) * 4;
            rgba[offset + 0] = border.r;
            rgba[offset + 1] = border.g;
            rgba[offset + 2] = border.b;
            rgba[offset + 3] = 255;  /* Alpha */
        }
    }
    
    /* Draw screen content */
    for (int y = 0; y < ZXSCREEN_HEIGHT; y++) {
        for (int x = 0; x < ZXSCREEN_WIDTH; x++) {
            uft_rgb_t color = uft_zxscreen_get_pixel_rgb(screen, x, y);
            int px = x + border_size;
            int py = y + border_size;
            int offset = (py * w + px) * 4;
            rgba[offset + 0] = color.r;
            rgba[offset + 1] = color.g;
            rgba[offset + 2] = color.b;
            rgba[offset + 3] = 255;
        }
    }
    
    return rgba;
}

bool uft_zxscreen_export_bmp(const uft_zxscreen_t *screen, const char *filename) {
    if (!screen || !screen->valid || !filename) return false;
    
    uint8_t *rgb = uft_zxscreen_to_rgb(screen);
    if (!rgb) return false;
    
    FILE *f = fopen(filename, "wb");
    if (!f) {
        free(rgb);
        return false;
    }
    
    /* BMP Header */
    int width = ZXSCREEN_WIDTH;
    int height = ZXSCREEN_HEIGHT;
    int row_size = ((width * 3 + 3) / 4) * 4;  /* Padded to 4 bytes */
    int pixel_data_size = row_size * height;
    int file_size = 54 + pixel_data_size;
    
    uint8_t header[54] = {0};
    
    /* BMP signature */
    header[0] = 'B';
    header[1] = 'M';
    
    /* File size */
    header[2] = file_size & 0xFF;
    header[3] = (file_size >> 8) & 0xFF;
    header[4] = (file_size >> 16) & 0xFF;
    header[5] = (file_size >> 24) & 0xFF;
    
    /* Pixel data offset */
    header[10] = 54;
    
    /* DIB header size */
    header[14] = 40;
    
    /* Width */
    header[18] = width & 0xFF;
    header[19] = (width >> 8) & 0xFF;
    
    /* Height (negative for top-down) */
    int neg_height = -height;
    header[22] = neg_height & 0xFF;
    header[23] = (neg_height >> 8) & 0xFF;
    header[24] = (neg_height >> 16) & 0xFF;
    header[25] = (neg_height >> 24) & 0xFF;
    
    /* Planes */
    header[26] = 1;
    
    /* Bits per pixel */
    header[28] = 24;
    
    /* Image size */
    header[34] = pixel_data_size & 0xFF;
    header[35] = (pixel_data_size >> 8) & 0xFF;
    header[36] = (pixel_data_size >> 16) & 0xFF;
    header[37] = (pixel_data_size >> 24) & 0xFF;
    
    /* Write header */
    fwrite(header, 1, 54, f);
    
    /* Write pixel data (BMP stores BGR, bottom-to-top, but we use negative height) */
    uint8_t padding[3] = {0, 0, 0};
    int pad_size = row_size - width * 3;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = (y * width + x) * 3;
            /* BGR order for BMP */
            fputc(rgb[offset + 2], f);  /* B */
            fputc(rgb[offset + 1], f);  /* G */
            fputc(rgb[offset + 0], f);  /* R */
        }
        if (pad_size > 0) {
            fwrite(padding, 1, pad_size, f);
        }
    }
    
    fclose(f);
    free(rgb);
    
    return true;
}
