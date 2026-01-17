/**
 * @file uft_sms.h
 * @brief Sega Master System / Game Gear ROM Support
 * 
 * Support for Sega 8-bit console ROM formats:
 * - Sega Master System (.sms)
 * - Sega Game Gear (.gg)
 * - SG-1000 (.sg)
 * - SC-3000 (.sc)
 * 
 * Features:
 * - ROM header parsing (TMR SEGA signature)
 * - Region/checksum detection
 * - Mapper type detection (Sega, Codemasters, Korean)
 * - Save RAM detection
 * - Game Gear vs SMS detection
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_SMS_H
#define UFT_SMS_H

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

/** TMR SEGA header location */
#define SMS_HEADER_OFFSET_7FF0  0x7FF0
#define SMS_HEADER_OFFSET_3FF0  0x3FF0
#define SMS_HEADER_OFFSET_1FF0  0x1FF0
#define SMS_HEADER_SIZE         16

/** TMR SEGA signature */
#define SMS_SIGNATURE           "TMR SEGA"
#define SMS_SIGNATURE_SIZE      8

/** Console types */
typedef enum {
    SMS_CONSOLE_UNKNOWN     = 0,
    SMS_CONSOLE_SG1000      = 1,    /**< SG-1000 */
    SMS_CONSOLE_SC3000      = 2,    /**< SC-3000 */
    SMS_CONSOLE_SMS         = 3,    /**< Master System */
    SMS_CONSOLE_GG          = 4     /**< Game Gear */
} sms_console_t;

/** Region codes (from header) */
typedef enum {
    SMS_REGION_UNKNOWN      = 0,
    SMS_REGION_SMS_JAPAN    = 3,    /**< SMS Japan */
    SMS_REGION_SMS_EXPORT   = 4,    /**< SMS Export */
    SMS_REGION_GG_JAPAN     = 5,    /**< GG Japan */
    SMS_REGION_GG_EXPORT    = 6,    /**< GG Export */
    SMS_REGION_GG_INTL      = 7     /**< GG International */
} sms_region_t;

/** Mapper types */
typedef enum {
    SMS_MAPPER_NONE         = 0,    /**< No mapper (up to 48KB) */
    SMS_MAPPER_SEGA         = 1,    /**< Sega mapper */
    SMS_MAPPER_CODEMASTERS  = 2,    /**< Codemasters mapper */
    SMS_MAPPER_KOREAN       = 3,    /**< Korean mapper */
    SMS_MAPPER_MSX          = 4,    /**< MSX adapter */
    SMS_MAPPER_NEMESIS      = 5,    /**< Nemesis mapper */
    SMS_MAPPER_JANGGUN      = 6,    /**< Janggun mapper */
    SMS_MAPPER_4PAK         = 7     /**< 4-Pak All Action */
} sms_mapper_t;

/** ROM sizes */
typedef enum {
    SMS_SIZE_8K             = 0x00,
    SMS_SIZE_16K            = 0x01,
    SMS_SIZE_32K            = 0x02,
    SMS_SIZE_48K            = 0x03,  /**< Unusual */
    SMS_SIZE_64K            = 0x04,
    SMS_SIZE_128K           = 0x05,
    SMS_SIZE_256K           = 0x06,
    SMS_SIZE_512K           = 0x07,
    SMS_SIZE_1M             = 0x08
} sms_size_code_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief SMS/GG ROM header (at 0x7FF0, 0x3FF0, or 0x1FF0)
 */
typedef struct {
    char        signature[8];       /**< "TMR SEGA" */
    uint8_t     reserved[2];        /**< Reserved */
    uint16_t    checksum;           /**< Checksum */
    uint8_t     product_code[3];    /**< Product code (BCD) */
    uint8_t     version_region;     /**< Version (high nibble) + Region (low nibble) */
    uint8_t     size_code;          /**< ROM size code */
} sms_header_t;

/**
 * @brief ROM info
 */
typedef struct {
    sms_console_t console;          /**< Console type */
    sms_region_t region;            /**< Region */
    sms_mapper_t mapper;            /**< Mapper type */
    size_t      file_size;          /**< File size */
    size_t      rom_size;           /**< Declared ROM size */
    bool        has_header;         /**< Has TMR SEGA header */
    uint32_t    header_offset;      /**< Header offset */
    uint16_t    checksum;           /**< Header checksum */
    uint16_t    calc_checksum;      /**< Calculated checksum */
    bool        checksum_valid;     /**< Checksum matches */
    uint32_t    product_code;       /**< Product code */
    uint8_t     version;            /**< ROM version */
    bool        has_sram;           /**< Has battery-backed SRAM */
} sms_info_t;

/**
 * @brief ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    sms_console_t console;          /**< Console type */
    sms_header_t header;            /**< Parsed header */
    uint32_t    header_offset;      /**< Header location */
    bool        has_header;         /**< Header found */
    bool        owns_data;          /**< true if we allocated */
} sms_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect console type
 * @param data ROM data
 * @param size Data size
 * @return Console type
 */
sms_console_t sms_detect_console(const uint8_t *data, size_t size);

/**
 * @brief Find TMR SEGA header
 * @param data ROM data
 * @param size Data size
 * @param offset Output: header offset
 * @return true if found
 */
bool sms_find_header(const uint8_t *data, size_t size, uint32_t *offset);

/**
 * @brief Detect mapper type
 * @param data ROM data
 * @param size Data size
 * @return Mapper type
 */
sms_mapper_t sms_detect_mapper(const uint8_t *data, size_t size);

/**
 * @brief Get console name
 * @param console Console type
 * @return Console name
 */
const char *sms_console_name(sms_console_t console);

/**
 * @brief Get region name
 * @param region Region code
 * @return Region name
 */
const char *sms_region_name(sms_region_t region);

/**
 * @brief Get mapper name
 * @param mapper Mapper type
 * @return Mapper name
 */
const char *sms_mapper_name(sms_mapper_t mapper);

/**
 * @brief Validate ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid
 */
bool sms_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open SMS/GG ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int sms_open(const uint8_t *data, size_t size, sms_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int sms_load(const char *filename, sms_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void sms_close(sms_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int sms_get_info(const sms_rom_t *rom, sms_info_t *info);

/* ============================================================================
 * API Functions - Checksum
 * ============================================================================ */

/**
 * @brief Calculate ROM checksum
 * @param data ROM data
 * @param size Data size
 * @return Checksum
 */
uint16_t sms_calc_checksum(const uint8_t *data, size_t size);

/**
 * @brief Verify ROM checksum
 * @param rom ROM context
 * @return true if valid
 */
bool sms_verify_checksum(const sms_rom_t *rom);

/**
 * @brief Fix ROM checksum
 * @param rom ROM context
 * @return 0 on success
 */
int sms_fix_checksum(sms_rom_t *rom);

/* ============================================================================
 * API Functions - Size Conversion
 * ============================================================================ */

/**
 * @brief Get ROM size from code
 * @param code Size code
 * @return Size in bytes
 */
size_t sms_size_from_code(uint8_t code);

/**
 * @brief Get size code from ROM size
 * @param size ROM size
 * @return Size code
 */
uint8_t sms_code_from_size(size_t size);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void sms_print_info(const sms_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SMS_H */
