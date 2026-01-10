/**
 * @file uft_zxscreen.h
 * @brief ZX Spectrum Screen Converter (SCREEN$ → PNG/BMP)
 * @version 1.0.0
 * 
 * Converts ZX Spectrum screen memory dumps to modern image formats.
 * 
 * Screen format:
 * - 6912 bytes total (6144 bitmap + 768 attributes)
 * - 256×192 pixels
 * - 8×8 pixel attribute cells (32×24 cells)
 * - 2 colors per cell (paper + ink, 15 colors with bright)
 * 
 * Usage:
 *   uft_zxscreen_t screen;
 *   uft_zxscreen_load(&screen, data, 6912);
 *   uint8_t *rgb = uft_zxscreen_to_rgb(&screen);
 *   // rgb is 256×192×3 = 147456 bytes
 */

#ifndef UFT_ZXSCREEN_H
#define UFT_ZXSCREEN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define ZXSCREEN_WIDTH          256
#define ZXSCREEN_HEIGHT         192
#define ZXSCREEN_BITMAP_SIZE    6144
#define ZXSCREEN_ATTR_SIZE      768
#define ZXSCREEN_TOTAL_SIZE     6912

#define ZXSCREEN_CELLS_X        32
#define ZXSCREEN_CELLS_Y        24
#define ZXSCREEN_CELL_SIZE      8

/* ═══════════════════════════════════════════════════════════════════════════════
 * Color Palette
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ZX Spectrum color indices
 */
typedef enum {
    ZX_BLACK = 0,
    ZX_BLUE,
    ZX_RED,
    ZX_MAGENTA,
    ZX_GREEN,
    ZX_CYAN,
    ZX_YELLOW,
    ZX_WHITE
} uft_zx_color_t;

/**
 * @brief RGB color value
 */
typedef struct {
    uint8_t r, g, b;
} uft_rgb_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Screen Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ZX Spectrum screen data
 */
typedef struct {
    uint8_t bitmap[ZXSCREEN_BITMAP_SIZE];   /**< Pixel data */
    uint8_t attrs[ZXSCREEN_ATTR_SIZE];      /**< Attribute data */
    bool valid;                              /**< Data loaded successfully */
    
    /* Options */
    bool use_bright_black;                   /**< Use dark gray for bright black */
    uint8_t border_color;                    /**< Border color (0-7) */
    
} uft_zxscreen_t;

/**
 * @brief Attribute cell info
 */
typedef struct {
    uint8_t ink;        /**< Foreground color (0-7) */
    uint8_t paper;      /**< Background color (0-7) */
    bool bright;        /**< Bright bit */
    bool flash;         /**< Flash bit */
} uft_zxattr_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize screen structure
 */
void uft_zxscreen_init(uft_zxscreen_t *screen);

/**
 * @brief Load screen data
 * @param screen Screen structure
 * @param data Raw screen data (6912 bytes)
 * @param size Data size (must be >= 6912)
 * @return true if loaded successfully
 */
bool uft_zxscreen_load(uft_zxscreen_t *screen, const uint8_t *data, size_t size);

/**
 * @brief Get attribute cell info
 * @param screen Screen structure
 * @param cell_x Cell X coordinate (0-31)
 * @param cell_y Cell Y coordinate (0-23)
 * @return Attribute info
 */
uft_zxattr_t uft_zxscreen_get_attr(const uft_zxscreen_t *screen, int cell_x, int cell_y);

/**
 * @brief Get pixel value (0 or 1)
 * @param screen Screen structure
 * @param x Pixel X coordinate (0-255)
 * @param y Pixel Y coordinate (0-191)
 * @return 0 (paper) or 1 (ink)
 */
int uft_zxscreen_get_pixel(const uft_zxscreen_t *screen, int x, int y);

/**
 * @brief Get RGB color for a pixel
 * @param screen Screen structure
 * @param x Pixel X coordinate
 * @param y Pixel Y coordinate
 * @return RGB color
 */
uft_rgb_t uft_zxscreen_get_pixel_rgb(const uft_zxscreen_t *screen, int x, int y);

/**
 * @brief Convert screen to RGB buffer
 * @param screen Screen structure
 * @return Allocated RGB buffer (256×192×3 = 147456 bytes), caller must free
 */
uint8_t *uft_zxscreen_to_rgb(const uft_zxscreen_t *screen);

/**
 * @brief Convert screen to RGBA buffer (with border)
 * @param screen Screen structure
 * @param border_size Border size in pixels (typically 32)
 * @param width Output width
 * @param height Output height
 * @return Allocated RGBA buffer, caller must free
 */
uint8_t *uft_zxscreen_to_rgba_with_border(
    const uft_zxscreen_t *screen,
    int border_size,
    int *width,
    int *height
);

/**
 * @brief Export screen to BMP file
 * @param screen Screen structure
 * @param filename Output filename
 * @return true if successful
 */
bool uft_zxscreen_export_bmp(const uft_zxscreen_t *screen, const char *filename);

/**
 * @brief Get standard ZX Spectrum color palette
 * @param bright Use bright colors
 * @return Pointer to 8-color palette array
 */
const uft_rgb_t *uft_zxscreen_get_palette(bool bright);

/**
 * @brief Get color by index and brightness
 * @param color Color index (0-7)
 * @param bright Brightness flag
 * @return RGB color
 */
uft_rgb_t uft_zxscreen_get_color(int color, bool bright);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ZXSCREEN_H */
