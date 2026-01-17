/**
 * @file uft_gameboy.h
 * @brief Nintendo Game Boy / Game Boy Advance ROM Support
 * 
 * Support for Nintendo handheld ROM formats:
 * - Game Boy (.gb) - DMG
 * - Game Boy Color (.gbc) - CGB
 * - Super Game Boy (.sgb) - SGB enhanced
 * - Game Boy Advance (.gba) - AGB
 * 
 * Features:
 * - ROM header parsing (title, licensee, cartridge type)
 * - MBC type detection (MBC1-5, MMM01, HuC1/3, etc.)
 * - Header checksum verification
 * - Global checksum verification
 * - Save type detection (SRAM, Flash, EEPROM)
 * - GBA ROM detection and info extraction
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_GAMEBOY_H
#define UFT_GAMEBOY_H

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

/** GB Header location */
#define GB_HEADER_OFFSET        0x100
#define GB_HEADER_SIZE          0x50

/** Nintendo logo at 0x104-0x133 */
#define GB_LOGO_OFFSET          0x104
#define GB_LOGO_SIZE            48

/** GBA Header location */
#define GBA_HEADER_OFFSET       0x00
#define GBA_HEADER_SIZE         0xC0

/** Entry point */
#define GB_ENTRY_POINT          0x100
#define GBA_ENTRY_POINT         0x00

/** Cartridge types */
typedef enum {
    GB_MBC_ROM_ONLY         = 0x00,
    GB_MBC_MBC1             = 0x01,
    GB_MBC_MBC1_RAM         = 0x02,
    GB_MBC_MBC1_RAM_BATT    = 0x03,
    GB_MBC_MBC2             = 0x05,
    GB_MBC_MBC2_BATT        = 0x06,
    GB_MBC_ROM_RAM          = 0x08,
    GB_MBC_ROM_RAM_BATT     = 0x09,
    GB_MBC_MMM01            = 0x0B,
    GB_MBC_MMM01_RAM        = 0x0C,
    GB_MBC_MMM01_RAM_BATT   = 0x0D,
    GB_MBC_MBC3_TIMER_BATT  = 0x0F,
    GB_MBC_MBC3_TIMER_RAM_BATT = 0x10,
    GB_MBC_MBC3             = 0x11,
    GB_MBC_MBC3_RAM         = 0x12,
    GB_MBC_MBC3_RAM_BATT    = 0x13,
    GB_MBC_MBC5             = 0x19,
    GB_MBC_MBC5_RAM         = 0x1A,
    GB_MBC_MBC5_RAM_BATT    = 0x1B,
    GB_MBC_MBC5_RUMBLE      = 0x1C,
    GB_MBC_MBC5_RUMBLE_RAM  = 0x1D,
    GB_MBC_MBC5_RUMBLE_RAM_BATT = 0x1E,
    GB_MBC_MBC6             = 0x20,
    GB_MBC_MBC7_SENSOR_RUMBLE_RAM_BATT = 0x22,
    GB_MBC_POCKET_CAMERA    = 0xFC,
    GB_MBC_BANDAI_TAMA5     = 0xFD,
    GB_MBC_HUC3             = 0xFE,
    GB_MBC_HUC1_RAM_BATT    = 0xFF
} gb_mbc_type_t;

/** Console compatibility */
typedef enum {
    GB_COMPAT_DMG           = 0,    /**< Original Game Boy only */
    GB_COMPAT_DMG_CGB       = 1,    /**< DMG + Color compatible */
    GB_COMPAT_CGB_ONLY      = 2,    /**< Game Boy Color only */
    GB_COMPAT_SGB           = 3     /**< Super Game Boy enhanced */
} gb_compat_t;

/** ROM sizes */
typedef enum {
    GB_ROM_32KB     = 0x00,     /**< 2 banks */
    GB_ROM_64KB     = 0x01,     /**< 4 banks */
    GB_ROM_128KB    = 0x02,     /**< 8 banks */
    GB_ROM_256KB    = 0x03,     /**< 16 banks */
    GB_ROM_512KB    = 0x04,     /**< 32 banks */
    GB_ROM_1MB      = 0x05,     /**< 64 banks */
    GB_ROM_2MB      = 0x06,     /**< 128 banks */
    GB_ROM_4MB      = 0x07,     /**< 256 banks */
    GB_ROM_8MB      = 0x08      /**< 512 banks */
} gb_rom_size_t;

/** RAM sizes */
typedef enum {
    GB_RAM_NONE     = 0x00,
    GB_RAM_2KB      = 0x01,     /**< Unofficial */
    GB_RAM_8KB      = 0x02,     /**< 1 bank */
    GB_RAM_32KB     = 0x03,     /**< 4 banks */
    GB_RAM_128KB    = 0x04,     /**< 16 banks */
    GB_RAM_64KB     = 0x05      /**< 8 banks */
} gb_ram_size_t;

/** GBA save types */
typedef enum {
    GBA_SAVE_NONE       = 0,
    GBA_SAVE_EEPROM_512 = 1,    /**< 512 bytes EEPROM */
    GBA_SAVE_EEPROM_8K  = 2,    /**< 8KB EEPROM */
    GBA_SAVE_SRAM_32K   = 3,    /**< 32KB SRAM */
    GBA_SAVE_FLASH_64K  = 4,    /**< 64KB Flash */
    GBA_SAVE_FLASH_128K = 5     /**< 128KB Flash */
} gba_save_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Game Boy ROM header (at 0x100-0x14F)
 */
typedef struct {
    uint8_t     entry[4];           /**< Entry point (usually NOP; JP xxxx) */
    uint8_t     logo[48];           /**< Nintendo logo */
    char        title[16];          /**< Title (11 chars for CGB) */
    char        manufacturer[4];    /**< Manufacturer code (CGB) */
    uint8_t     cgb_flag;           /**< CGB compatibility flag */
    char        new_licensee[2];    /**< New licensee code */
    uint8_t     sgb_flag;           /**< SGB support flag */
    uint8_t     cartridge_type;     /**< MBC type */
    uint8_t     rom_size;           /**< ROM size code */
    uint8_t     ram_size;           /**< RAM size code */
    uint8_t     destination;        /**< 0=Japan, 1=Overseas */
    uint8_t     old_licensee;       /**< Old licensee code */
    uint8_t     version;            /**< ROM version */
    uint8_t     header_checksum;    /**< Header checksum */
    uint16_t    global_checksum;    /**< Global checksum (big-endian) */
} gb_header_t;

/**
 * @brief GBA ROM header (at 0x00-0xBF)
 */
typedef struct {
    uint32_t    entry_point;        /**< ARM branch instruction */
    uint8_t     logo[156];          /**< Nintendo logo */
    char        title[12];          /**< Game title */
    char        game_code[4];       /**< Game code (AGB-XXXX) */
    char        maker_code[2];      /**< Maker code */
    uint8_t     fixed_96;           /**< Fixed value 0x96 */
    uint8_t     unit_code;          /**< Unit code */
    uint8_t     device_type;        /**< Device type */
    uint8_t     reserved1[7];       /**< Reserved */
    uint8_t     version;            /**< ROM version */
    uint8_t     complement;         /**< Header checksum complement */
    uint8_t     reserved2[2];       /**< Reserved */
} gba_header_t;

/**
 * @brief Game Boy ROM info
 */
typedef struct {
    char        title[17];          /**< Title (null-terminated) */
    gb_compat_t compatibility;      /**< Console compatibility */
    gb_mbc_type_t mbc_type;         /**< MBC type */
    const char  *mbc_name;          /**< MBC type name */
    size_t      rom_size;           /**< ROM size in bytes */
    size_t      ram_size;           /**< RAM size in bytes */
    int         rom_banks;          /**< Number of ROM banks */
    int         ram_banks;          /**< Number of RAM banks */
    bool        has_battery;        /**< Has battery backup */
    bool        has_timer;          /**< Has RTC */
    bool        has_rumble;         /**< Has rumble */
    uint8_t     header_checksum;    /**< Header checksum */
    uint8_t     calculated_checksum; /**< Calculated header checksum */
    bool        header_valid;       /**< Header checksum valid */
    uint16_t    global_checksum;    /**< Global checksum */
    bool        is_japanese;        /**< Japanese region */
    char        licensee[8];        /**< Licensee name/code */
} gb_info_t;

/**
 * @brief GBA ROM info
 */
typedef struct {
    char        title[13];          /**< Title (null-terminated) */
    char        game_code[5];       /**< Game code (null-terminated) */
    char        maker_code[3];      /**< Maker code (null-terminated) */
    size_t      rom_size;           /**< ROM size in bytes */
    uint8_t     version;            /**< ROM version */
    gba_save_type_t save_type;      /**< Detected save type */
    const char  *save_name;         /**< Save type name */
    uint8_t     complement;         /**< Header complement */
    uint8_t     calculated;         /**< Calculated complement */
    bool        header_valid;       /**< Header valid */
} gba_info_t;

/**
 * @brief Game Boy/GBA ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    bool        is_gba;             /**< true=GBA, false=GB/GBC */
    gb_header_t gb_header;          /**< GB header */
    gba_header_t gba_header;        /**< GBA header */
    bool        owns_data;          /**< true if we allocated */
    bool        header_valid;       /**< Header parsed successfully */
} gb_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if data is Game Boy ROM
 * @param data ROM data
 * @param size Data size
 * @return true if GB/GBC ROM
 */
bool gb_detect(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is GBA ROM
 * @param data ROM data
 * @param size Data size
 * @return true if GBA ROM
 */
bool gba_detect(const uint8_t *data, size_t size);

/**
 * @brief Validate Nintendo logo
 * @param data ROM data
 * @param size Data size
 * @return true if logo matches
 */
bool gb_validate_logo(const uint8_t *data, size_t size);

/**
 * @brief Get MBC type name
 * @param type MBC type code
 * @return Type name string
 */
const char *gb_mbc_name(gb_mbc_type_t type);

/**
 * @brief Get compatibility mode name
 * @param compat Compatibility mode
 * @return Mode name string
 */
const char *gb_compat_name(gb_compat_t compat);

/**
 * @brief Get GBA save type name
 * @param type Save type
 * @return Type name string
 */
const char *gba_save_name(gba_save_type_t type);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open Game Boy/GBA ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int gb_open(const uint8_t *data, size_t size, gb_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int gb_load(const char *filename, gb_rom_t *rom);

/**
 * @brief Save ROM to file
 * @param rom ROM context
 * @param filename Output path
 * @return 0 on success
 */
int gb_save(const gb_rom_t *rom, const char *filename);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void gb_close(gb_rom_t *rom);

/**
 * @brief Get GB ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int gb_get_info(const gb_rom_t *rom, gb_info_t *info);

/**
 * @brief Get GBA ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int gba_get_info(const gb_rom_t *rom, gba_info_t *info);

/* ============================================================================
 * API Functions - Checksum
 * ============================================================================ */

/**
 * @brief Calculate GB header checksum
 * @param data ROM data
 * @param size ROM size
 * @return Header checksum
 */
uint8_t gb_calculate_header_checksum(const uint8_t *data, size_t size);

/**
 * @brief Calculate GB global checksum
 * @param data ROM data
 * @param size ROM size
 * @return Global checksum
 */
uint16_t gb_calculate_global_checksum(const uint8_t *data, size_t size);

/**
 * @brief Verify GB header checksum
 * @param rom ROM context
 * @return true if valid
 */
bool gb_verify_header_checksum(const gb_rom_t *rom);

/**
 * @brief Calculate GBA header complement
 * @param data ROM data
 * @param size ROM size
 * @return Complement value
 */
uint8_t gba_calculate_complement(const uint8_t *data, size_t size);

/**
 * @brief Fix GB header checksum
 * @param rom ROM context
 * @return 0 on success
 */
int gb_fix_header_checksum(gb_rom_t *rom);

/* ============================================================================
 * API Functions - Save Detection
 * ============================================================================ */

/**
 * @brief Detect GBA save type from ROM
 * @param data ROM data
 * @param size ROM size
 * @return Save type
 */
gba_save_type_t gba_detect_save_type(const uint8_t *data, size_t size);

/**
 * @brief Check if GB cartridge has battery
 * @param type MBC type
 * @return true if battery-backed
 */
bool gb_has_battery(gb_mbc_type_t type);

/**
 * @brief Check if GB cartridge has timer
 * @param type MBC type
 * @return true if has RTC
 */
bool gb_has_timer(gb_mbc_type_t type);

/* ============================================================================
 * API Functions - Size Conversion
 * ============================================================================ */

/**
 * @brief Get ROM size in bytes from code
 * @param code ROM size code
 * @return Size in bytes
 */
size_t gb_rom_size_bytes(uint8_t code);

/**
 * @brief Get RAM size in bytes from code
 * @param code RAM size code
 * @return Size in bytes
 */
size_t gb_ram_size_bytes(uint8_t code);

/**
 * @brief Get number of ROM banks from code
 * @param code ROM size code
 * @return Number of banks
 */
int gb_rom_banks(uint8_t code);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print GB ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void gb_print_info(const gb_rom_t *rom, FILE *fp);

/**
 * @brief Print GBA ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void gba_print_info(const gb_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GAMEBOY_H */
