/**
 * @file uft_atari.h
 * @brief Atari 2600/7800/5200 ROM Support
 * 
 * Support for Atari console ROM formats:
 * - Atari 2600 (.a26, .bin) - VCS
 * - Atari 7800 (.a78) - ProSystem
 * - Atari 5200 (.a52) - SuperSystem
 * - Atari Lynx (.lnx) - Handheld
 * 
 * Features:
 * - ROM size detection and validation
 * - A78 header parsing
 * - Lynx header parsing
 * - Bankswitching detection (2600)
 * - Controller type detection
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_ATARI_H
#define UFT_ATARI_H

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

/** A78 header */
#define A78_HEADER_SIZE         128
#define A78_MAGIC               "ATARI7800"
#define A78_MAGIC_OFFSET        1
#define A78_MAGIC_SIZE          9

/** Lynx header */
#define LYNX_HEADER_SIZE        64
#define LYNX_MAGIC              "LYNX"
#define LYNX_MAGIC_SIZE         4

/** Standard 2600 ROM sizes */
#define A26_SIZE_2K             2048
#define A26_SIZE_4K             4096
#define A26_SIZE_8K             8192
#define A26_SIZE_16K            16384
#define A26_SIZE_32K            32768

/** Console types */
typedef enum {
    ATARI_CONSOLE_UNKNOWN   = 0,
    ATARI_CONSOLE_2600      = 1,    /**< VCS */
    ATARI_CONSOLE_5200      = 2,    /**< SuperSystem */
    ATARI_CONSOLE_7800      = 3,    /**< ProSystem */
    ATARI_CONSOLE_LYNX      = 4     /**< Handheld */
} atari_console_t;

/** 2600 bankswitching types */
typedef enum {
    A26_BANK_NONE           = 0,    /**< 2K/4K (no banking) */
    A26_BANK_F8             = 1,    /**< 8K Atari */
    A26_BANK_F6             = 2,    /**< 16K Atari */
    A26_BANK_F4             = 3,    /**< 32K Atari */
    A26_BANK_FE             = 4,    /**< 8K Activision */
    A26_BANK_E0             = 5,    /**< 8K Parker Bros */
    A26_BANK_E7             = 6,    /**< 16K M-Network */
    A26_BANK_3F             = 7,    /**< Tigervision */
    A26_BANK_FA             = 8,    /**< 12K CBS RAM Plus */
    A26_BANK_CV             = 9,    /**< Commavid */
    A26_BANK_UA             = 10,   /**< UA Ltd */
    A26_BANK_SUPERCHIP      = 11,   /**< 128 bytes RAM */
    A26_BANK_AR             = 12,   /**< Supercharger */
    A26_BANK_DPC            = 13,   /**< Pitfall II */
    A26_BANK_UNKNOWN        = 255
} a26_bankswitch_t;

/** 7800 cartridge types */
typedef enum {
    A78_TYPE_NONE           = 0,
    A78_TYPE_POKEY          = 1,    /**< POKEY @ $4000 */
    A78_TYPE_SUPERGAME_RAM  = 2,    /**< SuperGame RAM @ $4000 */
    A78_TYPE_SUPERGAME_ROM  = 3,    /**< SuperGame banked ROM */
    A78_TYPE_ABSOLUTE       = 4,    /**< Absolute bankswitching */
    A78_TYPE_ACTIVISION     = 5     /**< Activision bankswitching */
} a78_cart_type_t;

/** Controller types (7800) */
typedef enum {
    A78_CTRL_NONE           = 0,
    A78_CTRL_JOYSTICK       = 1,
    A78_CTRL_LIGHTGUN       = 2,
    A78_CTRL_PADDLE         = 3,
    A78_CTRL_TRAKBALL       = 4,
    A78_CTRL_2600_JOYSTICK  = 5,
    A78_CTRL_2600_DRIVING   = 6,
    A78_CTRL_2600_KEYBOARD  = 7,
    A78_CTRL_ST_MOUSE       = 8,
    A78_CTRL_AMIGA_MOUSE    = 9
} a78_controller_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Atari 7800 header (128 bytes)
 */
typedef struct {
    uint8_t     version;            /**< Header version (1-3) */
    char        magic[9];           /**< "ATARI7800" */
    char        title[32];          /**< Game title */
    uint32_t    rom_size;           /**< ROM size (big-endian) */
    uint16_t    cart_type;          /**< Cartridge type flags */
    uint8_t     controller1;        /**< Controller 1 type */
    uint8_t     controller2;        /**< Controller 2 type */
    uint8_t     tv_type;            /**< 0=NTSC, 1=PAL */
    uint8_t     save_type;          /**< Save device type */
    uint8_t     reserved[64];       /**< Reserved */
    uint8_t     expansion_module;   /**< Expansion module */
} a78_header_t;

/**
 * @brief Atari Lynx header (64 bytes)
 */
typedef struct {
    char        magic[4];           /**< "LYNX" */
    uint16_t    page_size_bank0;    /**< Bank 0 page size */
    uint16_t    page_size_bank1;    /**< Bank 1 page size */
    uint16_t    version;            /**< Header version */
    char        title[32];          /**< Cartridge name */
    char        manufacturer[16];   /**< Manufacturer */
    uint8_t     rotation;           /**< Screen rotation */
    uint8_t     spare[5];           /**< Reserved */
} lynx_header_t;

/**
 * @brief Atari ROM info
 */
typedef struct {
    atari_console_t console;        /**< Console type */
    const char  *console_name;      /**< Console name */
    size_t      file_size;          /**< File size */
    size_t      rom_size;           /**< ROM size */
    bool        has_header;         /**< Has header */
    char        title[33];          /**< Title (if available) */
    
    /* 2600 specific */
    a26_bankswitch_t bankswitch;    /**< Bankswitching type */
    const char  *bankswitch_name;   /**< Bankswitching name */
    
    /* 7800 specific */
    a78_cart_type_t cart_type;      /**< Cartridge type */
    a78_controller_t controller1;   /**< Controller 1 */
    a78_controller_t controller2;   /**< Controller 2 */
    bool        is_pal;             /**< PAL version */
    bool        has_pokey;          /**< Has POKEY chip */
    
    /* Lynx specific */
    uint8_t     rotation;           /**< Screen rotation */
} atari_info_t;

/**
 * @brief Atari ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    atari_console_t console;        /**< Detected console */
    bool        has_header;         /**< Has header */
    size_t      header_size;        /**< Header size */
    a78_header_t a78_header;        /**< 7800 header */
    lynx_header_t lynx_header;      /**< Lynx header */
    bool        owns_data;          /**< true if we allocated */
} atari_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect Atari console from ROM
 * @param data ROM data
 * @param size Data size
 * @return Console type
 */
atari_console_t atari_detect_console(const uint8_t *data, size_t size);

/**
 * @brief Check if data is Atari 7800 ROM with header
 * @param data ROM data
 * @param size Data size
 * @return true if A78
 */
bool atari_is_a78(const uint8_t *data, size_t size);

/**
 * @brief Check if data is Atari Lynx ROM with header
 * @param data ROM data
 * @param size Data size
 * @return true if Lynx
 */
bool atari_is_lynx(const uint8_t *data, size_t size);

/**
 * @brief Detect 2600 bankswitching type
 * @param data ROM data
 * @param size ROM size
 * @return Bankswitching type
 */
a26_bankswitch_t a26_detect_bankswitch(const uint8_t *data, size_t size);

/**
 * @brief Get console name
 * @param console Console type
 * @return Console name
 */
const char *atari_console_name(atari_console_t console);

/**
 * @brief Get bankswitching name
 * @param type Bankswitching type
 * @return Type name
 */
const char *a26_bankswitch_name(a26_bankswitch_t type);

/**
 * @brief Get controller name
 * @param type Controller type
 * @return Controller name
 */
const char *a78_controller_name(a78_controller_t type);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open Atari ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int atari_open(const uint8_t *data, size_t size, atari_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int atari_load(const char *filename, atari_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void atari_close(atari_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int atari_get_info(const atari_rom_t *rom, atari_info_t *info);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get ROM data without header
 * @param rom ROM context
 * @return Pointer to ROM data
 */
const uint8_t *atari_get_rom_data(const atari_rom_t *rom);

/**
 * @brief Get ROM size without header
 * @param rom ROM context
 * @return ROM size
 */
size_t atari_get_rom_size(const atari_rom_t *rom);

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void atari_print_info(const atari_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_H */
