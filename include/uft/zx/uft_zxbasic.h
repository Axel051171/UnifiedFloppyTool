/**
 * @file uft_zxbasic.h
 * @brief ZX Spectrum BASIC Tokenizer/Detokenizer
 * @version 1.0.0
 * 
 * Handles ZX Spectrum BASIC programs:
 * - Token to keyword conversion (detokenize)
 * - Line number handling
 * - Variable type detection
 * - Numeric literals (floating point, integers)
 * 
 * Reference: ZX Spectrum ROM (addresses $0386-$0556 contain token table)
 * 
 * "Bei uns geht kein Bit verloren"
 */

#ifndef UFT_ZXBASIC_H
#define UFT_ZXBASIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Token Ranges
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define ZX_TOKEN_FIRST      0xA5    /* First keyword token (RND) */
#define ZX_TOKEN_LAST       0xFF    /* Last keyword token (COPY) */
#define ZX_TOKEN_COUNT      (ZX_TOKEN_LAST - ZX_TOKEN_FIRST + 1)

/* Special characters */
#define ZX_CHAR_NUMBER      0x0E    /* 5-byte number follows */
#define ZX_CHAR_NEWLINE     0x0D    /* End of line */
#define ZX_CHAR_INK         0x10    /* INK control */
#define ZX_CHAR_PAPER       0x11    /* PAPER control */
#define ZX_CHAR_FLASH       0x12    /* FLASH control */
#define ZX_CHAR_BRIGHT      0x13    /* BRIGHT control */
#define ZX_CHAR_INVERSE     0x14    /* INVERSE control */
#define ZX_CHAR_OVER        0x15    /* OVER control */
#define ZX_CHAR_AT          0x16    /* AT control (2 params) */
#define ZX_CHAR_TAB         0x17    /* TAB control (2 params) */

/* Block graphics (0x80-0x8F) */
#define ZX_BLOCK_FIRST      0x80
#define ZX_BLOCK_LAST       0x8F

/* UDG characters (0x90-0xA4) */
#define ZX_UDG_FIRST        0x90
#define ZX_UDG_LAST         0xA4

/* ═══════════════════════════════════════════════════════════════════════════════
 * BASIC Line Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parsed BASIC line
 */
typedef struct {
    uint16_t line_number;       /**< Line number (1-9999) */
    uint16_t length;            /**< Line length including NEWLINE */
    const uint8_t *data;        /**< Raw line data */
    char *text;                 /**< Detokenized text (caller must free) */
    bool has_rem;               /**< Line contains REM statement */
} uft_zx_line_t;

/**
 * @brief BASIC program information
 */
typedef struct {
    uint16_t autostart;         /**< Auto-start line (or 0x8000 if none) */
    uint16_t var_offset;        /**< Offset to variables area */
    size_t program_size;        /**< Size of program area */
    size_t line_count;          /**< Number of lines */
    uft_zx_line_t *lines;       /**< Array of lines (caller must free) */
} uft_zx_program_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Variable Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    ZX_VAR_NUMBER = 0,          /**< Numeric variable (A-Z) */
    ZX_VAR_NUMBER_ARRAY,        /**< Numeric array */
    ZX_VAR_FOR_LOOP,            /**< FOR loop control variable */
    ZX_VAR_STRING,              /**< String variable (A$-Z$) */
    ZX_VAR_STRING_ARRAY,        /**< String array */
    ZX_VAR_UNKNOWN
} uft_zx_var_type_t;

/**
 * @brief Variable entry
 */
typedef struct {
    char name[16];              /**< Variable name */
    uft_zx_var_type_t type;     /**< Variable type */
    size_t size;                /**< Size in bytes */
    union {
        double number;          /**< Numeric value */
        struct {
            char *data;
            size_t length;
        } string;               /**< String value */
    } value;
} uft_zx_var_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detokenization Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get keyword for token
 * @param token Token byte (0xA5-0xFF)
 * @return Keyword string or NULL if not a token
 */
const char *uft_zx_token_to_keyword(uint8_t token);

/**
 * @brief Check if byte is a token
 */
bool uft_zx_is_token(uint8_t byte);

/**
 * @brief Detokenize a single BASIC line
 * @param data Line data (after line number and length)
 * @param len Data length
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Length of detokenized string, or -1 on error
 */
int uft_zx_detokenize_line(const uint8_t *data, size_t len,
                           char *output, size_t output_size);

/**
 * @brief Parse 5-byte ZX Spectrum floating point number
 * @param data 5-byte number (exponent + mantissa)
 * @return Floating point value
 */
double uft_zx_parse_number(const uint8_t *data);

/**
 * @brief Format ZX Spectrum number for display
 * @param data 5-byte number
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Formatted string length
 */
int uft_zx_format_number(const uint8_t *data, char *output, size_t output_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Program Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse BASIC program from TAP/TZX block
 * @param data Program data
 * @param len Data length
 * @param program Output program structure
 * @return 0 on success, negative on error
 */
int uft_zx_parse_program(const uint8_t *data, size_t len,
                         uft_zx_program_t *program);

/**
 * @brief Free program structure
 */
void uft_zx_program_free(uft_zx_program_t *program);

/**
 * @brief List program to text
 * @param program Parsed program
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Bytes written, or -1 on error
 */
int uft_zx_list_program(const uft_zx_program_t *program,
                        char *output, size_t output_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Variable Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse variables area
 * @param data Variables data
 * @param len Data length
 * @param vars Output array (caller allocated)
 * @param max_vars Maximum variables to parse
 * @return Number of variables found
 */
int uft_zx_parse_variables(const uint8_t *data, size_t len,
                           uft_zx_var_t *vars, size_t max_vars);

/**
 * @brief Get variable type name
 */
const char *uft_zx_var_type_name(uft_zx_var_type_t type);

/* ═══════════════════════════════════════════════════════════════════════════════
 * TAP Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TAP header types
 */
typedef enum {
    ZX_TAP_PROGRAM = 0,         /**< BASIC program */
    ZX_TAP_NUMBER_ARRAY = 1,    /**< Number array */
    ZX_TAP_STRING_ARRAY = 2,    /**< Character array */
    ZX_TAP_CODE = 3             /**< Bytes (CODE) */
} uft_zx_tap_type_t;

/**
 * @brief TAP header structure
 */
typedef struct {
    uft_zx_tap_type_t type;     /**< Block type */
    char filename[11];          /**< 10-char filename + NUL */
    uint16_t length;            /**< Data length */
    uint16_t param1;            /**< For BASIC: autostart line */
    uint16_t param2;            /**< For BASIC: program length */
} uft_zx_tap_header_t;

/**
 * @brief Parse TAP header block
 * @param data 17-byte header (after flag byte)
 * @param header Output header structure
 * @return 0 on success
 */
int uft_zx_parse_tap_header(const uint8_t *data, uft_zx_tap_header_t *header);

/**
 * @brief Get TAP type name
 */
const char *uft_zx_tap_type_name(uft_zx_tap_type_t type);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Character Set Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert ZX Spectrum character to UTF-8
 * @param zx_char ZX character code
 * @param output UTF-8 output buffer (min 4 bytes)
 * @return Bytes written to output
 */
int uft_zx_char_to_utf8(uint8_t zx_char, char *output);

/**
 * @brief Get UDG character name (A-U)
 * @param zx_char Character code (0x90-0xA4)
 * @return UDG name or NULL
 */
const char *uft_zx_udg_name(uint8_t zx_char);

/**
 * @brief Get block graphics description
 * @param zx_char Character code (0x80-0x8F)
 * @return Description or NULL
 */
const char *uft_zx_block_name(uint8_t zx_char);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ZXBASIC_H */
