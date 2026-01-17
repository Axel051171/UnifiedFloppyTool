/**
 * @file uft_spectrum.h
 * @brief ZX Spectrum Tape and Disk Support
 * 
 * Support for ZX Spectrum formats:
 * - TAP (.tap) - Raw tape blocks
 * - TZX (.tzx) - Extended tape format
 * - Z80 (.z80) - Snapshot format
 * - SNA (.sna) - Snapshot format
 * - DSK (.dsk) - +3 disk image
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_SPECTRUM_H
#define UFT_SPECTRUM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** TZX magic */
#define TZX_MAGIC           "ZXTape!\x1A"
#define TZX_MAGIC_SIZE      8

/** TAP block types */
#define TAP_BLOCK_HEADER    0x00
#define TAP_BLOCK_DATA      0xFF

/** Z80 snapshot versions */
typedef enum {
    Z80_VERSION_1       = 1,    /**< 48K only */
    Z80_VERSION_2       = 2,    /**< 128K support */
    Z80_VERSION_3       = 3     /**< Extended */
} z80_version_t;

/** Spectrum models */
typedef enum {
    SPEC_MODEL_48K      = 0,
    SPEC_MODEL_128K     = 1,
    SPEC_MODEL_PLUS2    = 2,
    SPEC_MODEL_PLUS2A   = 3,
    SPEC_MODEL_PLUS3    = 4
} spec_model_t;

/** File types */
typedef enum {
    SPEC_FORMAT_UNKNOWN = 0,
    SPEC_FORMAT_TAP     = 1,
    SPEC_FORMAT_TZX     = 2,
    SPEC_FORMAT_Z80     = 3,
    SPEC_FORMAT_SNA     = 4,
    SPEC_FORMAT_DSK     = 5
} spec_format_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief TAP file header block
 */
typedef struct {
    uint8_t     type;               /**< 0=Program, 1=Number array, 2=Char array, 3=Code */
    char        filename[10];       /**< Filename (space-padded) */
    uint16_t    length;             /**< Data length */
    uint16_t    param1;             /**< Autostart line (Program) or Start address (Code) */
    uint16_t    param2;             /**< Program length (Program) or 32768 (Code) */
} tap_header_t;

/**
 * @brief Z80 snapshot header (v1)
 */
typedef struct {
    uint8_t     a, f;               /**< AF register */
    uint16_t    bc, hl, pc, sp;     /**< Registers */
    uint8_t     i, r;               /**< Interrupt/refresh */
    uint8_t     flags;              /**< Flags byte */
    uint16_t    de, bc_, de_, hl_;  /**< More registers */
    uint8_t     a_, f_;             /**< Alternate AF */
    uint16_t    iy, ix;             /**< Index registers */
    uint8_t     iff1, iff2;         /**< Interrupt flip-flops */
    uint8_t     im;                 /**< Interrupt mode */
} z80_header_v1_t;

/**
 * @brief SNA snapshot header (48K)
 */
typedef struct {
    uint8_t     i;                  /**< I register */
    uint16_t    hl_, de_, bc_, af_; /**< Alternate registers */
    uint16_t    hl, de, bc, iy, ix; /**< Main registers */
    uint8_t     iff2;               /**< IFF2 */
    uint8_t     r;                  /**< R register */
    uint16_t    af, sp;             /**< AF, SP */
    uint8_t     im;                 /**< Interrupt mode */
    uint8_t     border;             /**< Border color */
} sna_header_t;

/**
 * @brief Spectrum file info
 */
typedef struct {
    spec_format_t format;           /**< File format */
    const char  *format_name;       /**< Format name */
    size_t      file_size;          /**< File size */
    spec_model_t model;             /**< Spectrum model */
    const char  *model_name;        /**< Model name */
    bool        is_compressed;      /**< Data compressed */
    int         block_count;        /**< Number of blocks (TAP/TZX) */
    z80_version_t z80_version;      /**< Z80 version (if Z80) */
} spec_info_t;

/**
 * @brief Spectrum file context
 */
typedef struct {
    uint8_t     *data;              /**< File data */
    size_t      size;               /**< Data size */
    spec_format_t format;           /**< Detected format */
    bool        owns_data;          /**< true if we allocated */
} spec_file_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect Spectrum file format
 * @param data File data
 * @param size Data size
 * @return File format
 */
spec_format_t spec_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Get format name
 */
const char *spec_format_name(spec_format_t format);

/**
 * @brief Get model name
 */
const char *spec_model_name(spec_model_t model);

/* ============================================================================
 * API Functions - File Operations
 * ============================================================================ */

int spec_open(const uint8_t *data, size_t size, spec_file_t *file);
int spec_load(const char *filename, spec_file_t *file);
void spec_close(spec_file_t *file);
int spec_get_info(const spec_file_t *file, spec_info_t *info);

/* ============================================================================
 * API Functions - TAP Operations
 * ============================================================================ */

int spec_tap_block_count(const spec_file_t *file);
int spec_tap_get_block(const spec_file_t *file, int index, 
                       const uint8_t **data, size_t *size, uint8_t *flag);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

void spec_print_info(const spec_file_t *file, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SPECTRUM_H */
