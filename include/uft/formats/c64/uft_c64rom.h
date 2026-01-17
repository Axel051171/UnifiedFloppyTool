/**
 * @file uft_c64rom.h
 * @brief C64 ROM Image Support
 * 
 * Support for Commodore 64 ROM images:
 * - BASIC ROM (8KB @ $A000-$BFFF)
 * - KERNAL ROM (8KB @ $E000-$FFFF)
 * - Character ROM (4KB @ $D000-$DFFF, active when I/O off)
 * - Combined ROM files (16KB BASIC+KERNAL, 20KB with CHAR)
 * 
 * Features:
 * - ROM identification and validation
 * - Version detection (original, JiffyDOS, etc.)
 * - Checksum verification
 * - ROM patching support
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_C64ROM_H
#define UFT_C64ROM_H

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

/** ROM sizes */
#define C64ROM_BASIC_SIZE       8192    /**< 8KB BASIC ROM */
#define C64ROM_KERNAL_SIZE      8192    /**< 8KB KERNAL ROM */
#define C64ROM_CHAR_SIZE        4096    /**< 4KB Character ROM */
#define C64ROM_COMBINED_SIZE    16384   /**< 16KB BASIC + KERNAL */
#define C64ROM_FULL_SIZE        20480   /**< 20KB with Character ROM */

/** ROM load addresses */
#define C64ROM_BASIC_ADDR       0xA000
#define C64ROM_KERNAL_ADDR      0xE000
#define C64ROM_CHAR_ADDR        0xD000

/** Known ROM checksums (CRC32) */
#define C64ROM_BASIC_V2_CRC     0x79015323  /**< Original BASIC V2 */
#define C64ROM_KERNAL_901227_03 0xDBE3E7C7  /**< 901227-03 */
#define C64ROM_KERNAL_JIFFY_CRC 0x00000000  /**< JiffyDOS (varies) */
#define C64ROM_CHAR_901225_01   0x3E135179  /**< 901225-01 */

/** ROM types */
typedef enum {
    C64ROM_TYPE_UNKNOWN     = 0,
    C64ROM_TYPE_BASIC       = 1,    /**< BASIC ROM only */
    C64ROM_TYPE_KERNAL      = 2,    /**< KERNAL ROM only */
    C64ROM_TYPE_CHAR        = 3,    /**< Character ROM only */
    C64ROM_TYPE_COMBINED    = 4,    /**< BASIC + KERNAL */
    C64ROM_TYPE_FULL        = 5     /**< BASIC + KERNAL + CHAR */
} c64rom_type_t;

/** ROM versions */
typedef enum {
    C64ROM_VER_UNKNOWN      = 0,
    C64ROM_VER_ORIGINAL     = 1,    /**< Original Commodore */
    C64ROM_VER_JIFFYDOS     = 2,    /**< JiffyDOS */
    C64ROM_VER_DOLPHINDOS   = 3,    /**< Dolphin DOS */
    C64ROM_VER_SPEEDDOS     = 4,    /**< SpeedDOS */
    C64ROM_VER_PRODOS       = 5,    /**< Professional DOS */
    C64ROM_VER_EXOS         = 6,    /**< EXOS */
    C64ROM_VER_CUSTOM       = 255   /**< Custom/modified */
} c64rom_version_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief ROM info
 */
typedef struct {
    c64rom_type_t type;             /**< ROM type */
    c64rom_version_t version;       /**< ROM version */
    const char *version_name;       /**< Version name string */
    size_t size;                    /**< Total size */
    uint32_t crc32;                 /**< CRC32 checksum */
    bool has_basic;                 /**< Contains BASIC */
    bool has_kernal;                /**< Contains KERNAL */
    bool has_char;                  /**< Contains Character ROM */
} c64rom_info_t;

/**
 * @brief ROM image context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    c64rom_type_t type;             /**< Detected type */
    c64rom_version_t version;       /**< Detected version */
    uint8_t     *basic;             /**< Pointer to BASIC ROM */
    uint8_t     *kernal;            /**< Pointer to KERNAL ROM */
    uint8_t     *charrom;           /**< Pointer to Character ROM */
    bool        owns_data;          /**< true if we allocated */
} c64rom_image_t;

/**
 * @brief KERNAL vector table
 */
typedef struct {
    uint16_t    irq;                /**< IRQ vector */
    uint16_t    brk;                /**< BRK vector */
    uint16_t    nmi;                /**< NMI vector */
    uint16_t    reset;              /**< RESET vector */
    uint16_t    open;               /**< OPEN routine */
    uint16_t    close;              /**< CLOSE routine */
    uint16_t    chkin;              /**< CHKIN routine */
    uint16_t    chkout;             /**< CHKOUT routine */
    uint16_t    clrchn;             /**< CLRCHN routine */
    uint16_t    chrin;              /**< CHRIN routine */
    uint16_t    chrout;             /**< CHROUT routine */
    uint16_t    load;               /**< LOAD routine */
    uint16_t    save;               /**< SAVE routine */
} c64rom_vectors_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect ROM type from size
 * @param size ROM size
 * @return ROM type
 */
c64rom_type_t c64rom_detect_type(size_t size);

/**
 * @brief Detect ROM version
 * @param data ROM data
 * @param size ROM size
 * @return ROM version
 */
c64rom_version_t c64rom_detect_version(const uint8_t *data, size_t size);

/**
 * @brief Get type name
 * @param type ROM type
 * @return Type name string
 */
const char *c64rom_type_name(c64rom_type_t type);

/**
 * @brief Get version name
 * @param version ROM version
 * @return Version name string
 */
const char *c64rom_version_name(c64rom_version_t version);

/**
 * @brief Validate ROM data
 * @param data ROM data
 * @param size ROM size
 * @return true if valid
 */
bool c64rom_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open ROM image
 * @param data ROM data
 * @param size ROM size
 * @param rom Output ROM context
 * @return 0 on success
 */
int c64rom_open(const uint8_t *data, size_t size, c64rom_image_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int c64rom_load(const char *filename, c64rom_image_t *rom);

/**
 * @brief Save ROM to file
 * @param rom ROM context
 * @param filename Output path
 * @return 0 on success
 */
int c64rom_save(const c64rom_image_t *rom, const char *filename);

/**
 * @brief Close ROM image
 * @param rom ROM to close
 */
void c64rom_close(c64rom_image_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int c64rom_get_info(const c64rom_image_t *rom, c64rom_info_t *info);

/* ============================================================================
 * API Functions - ROM Access
 * ============================================================================ */

/**
 * @brief Get BASIC ROM pointer
 * @param rom ROM context
 * @return BASIC ROM data or NULL
 */
const uint8_t *c64rom_get_basic(const c64rom_image_t *rom);

/**
 * @brief Get KERNAL ROM pointer
 * @param rom ROM context
 * @return KERNAL ROM data or NULL
 */
const uint8_t *c64rom_get_kernal(const c64rom_image_t *rom);

/**
 * @brief Get Character ROM pointer
 * @param rom ROM context
 * @return Character ROM data or NULL
 */
const uint8_t *c64rom_get_charrom(const c64rom_image_t *rom);

/**
 * @brief Extract individual ROM
 * @param rom ROM context
 * @param type Which ROM to extract
 * @param buffer Output buffer
 * @param max_size Maximum buffer size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int c64rom_extract(const c64rom_image_t *rom, c64rom_type_t type,
                   uint8_t *buffer, size_t max_size, size_t *extracted);

/* ============================================================================
 * API Functions - KERNAL Analysis
 * ============================================================================ */

/**
 * @brief Get KERNAL vectors
 * @param rom ROM context
 * @param vectors Output vectors
 * @return 0 on success
 */
int c64rom_get_vectors(const c64rom_image_t *rom, c64rom_vectors_t *vectors);

/**
 * @brief Calculate ROM CRC32
 * @param data ROM data
 * @param size ROM size
 * @return CRC32 value
 */
uint32_t c64rom_crc32(const uint8_t *data, size_t size);

/**
 * @brief Check if KERNAL is JiffyDOS
 * @param rom ROM context
 * @return true if JiffyDOS
 */
bool c64rom_is_jiffydos(const c64rom_image_t *rom);

/* ============================================================================
 * API Functions - ROM Creation/Patching
 * ============================================================================ */

/**
 * @brief Create combined ROM from parts
 * @param basic BASIC ROM (8KB)
 * @param kernal KERNAL ROM (8KB)
 * @param charrom Character ROM (4KB, optional)
 * @param rom Output ROM context
 * @return 0 on success
 */
int c64rom_create(const uint8_t *basic, const uint8_t *kernal,
                  const uint8_t *charrom, c64rom_image_t *rom);

/**
 * @brief Patch ROM byte
 * @param rom ROM context
 * @param address Address within ROM
 * @param value New value
 * @return 0 on success
 */
int c64rom_patch(c64rom_image_t *rom, uint16_t address, uint8_t value);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void c64rom_print_info(const c64rom_image_t *rom, FILE *fp);

/**
 * @brief Print KERNAL vectors
 * @param vectors KERNAL vectors
 * @param fp Output file
 */
void c64rom_print_vectors(const c64rom_vectors_t *vectors, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64ROM_H */
