/**
 * @file uft_nes.h
 * @brief Nintendo Entertainment System / Famicom ROM Support
 * 
 * Support for NES ROM formats:
 * - iNES (.nes) - Original format
 * - NES 2.0 (.nes) - Extended format
 * - UNIF (.unf) - Universal NES Image Format
 * - FDS (.fds) - Famicom Disk System
 * 
 * Features:
 * - Mapper detection (0-255+)
 * - PRG/CHR ROM parsing
 * - Battery/trainer detection
 * - TV system detection (NTSC/PAL/Dual)
 * - NES 2.0 extended attributes
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_NES_H
#define UFT_NES_H

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

/** iNES header */
#define NES_HEADER_SIZE         16
#define NES_MAGIC               "NES\x1A"
#define NES_MAGIC_SIZE          4

/** FDS header */
#define FDS_HEADER_SIZE         16
#define FDS_MAGIC               "FDS\x1A"
#define FDS_DISK_SIZE           65500

/** UNIF header */
#define UNIF_MAGIC              "UNIF"
#define UNIF_MAGIC_SIZE         4

/** PRG/CHR sizes */
#define NES_PRG_BANK_SIZE       16384   /**< 16KB PRG bank */
#define NES_CHR_BANK_SIZE       8192    /**< 8KB CHR bank */
#define NES_TRAINER_SIZE        512

/** ROM format types */
typedef enum {
    NES_FORMAT_UNKNOWN  = 0,
    NES_FORMAT_INES     = 1,    /**< iNES 1.0 */
    NES_FORMAT_NES20    = 2,    /**< NES 2.0 */
    NES_FORMAT_UNIF     = 3,    /**< UNIF */
    NES_FORMAT_FDS      = 4     /**< Famicom Disk System */
} nes_format_t;

/** Mirroring types */
typedef enum {
    NES_MIRROR_HORIZONTAL   = 0,
    NES_MIRROR_VERTICAL     = 1,
    NES_MIRROR_FOUR_SCREEN  = 2,
    NES_MIRROR_SINGLE_0     = 3,
    NES_MIRROR_SINGLE_1     = 4
} nes_mirror_t;

/** TV system */
typedef enum {
    NES_TV_NTSC         = 0,
    NES_TV_PAL          = 1,
    NES_TV_DUAL         = 2,
    NES_TV_DENDY        = 3
} nes_tv_t;

/** Console type (NES 2.0) */
typedef enum {
    NES_CONSOLE_NES         = 0,
    NES_CONSOLE_VS_SYSTEM   = 1,
    NES_CONSOLE_PLAYCHOICE  = 2,
    NES_CONSOLE_EXTENDED    = 3
} nes_console_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief iNES/NES 2.0 header (16 bytes)
 */
typedef struct {
    char        magic[4];           /**< "NES\x1A" */
    uint8_t     prg_rom_size;       /**< PRG ROM size (16KB units) */
    uint8_t     chr_rom_size;       /**< CHR ROM size (8KB units, 0=CHR RAM) */
    uint8_t     flags6;             /**< Mapper low, mirroring, battery, trainer */
    uint8_t     flags7;             /**< Mapper high, NES 2.0 signature, console */
    uint8_t     flags8;             /**< PRG RAM size (NES 2.0: mapper high) */
    uint8_t     flags9;             /**< TV system (NES 2.0: PRG/CHR ROM size high) */
    uint8_t     flags10;            /**< TV system, PRG RAM (unofficial) */
    uint8_t     padding[5];         /**< Padding (should be zero) */
} nes_header_t;

/**
 * @brief NES ROM info
 */
typedef struct {
    nes_format_t format;            /**< ROM format */
    const char  *format_name;       /**< Format name */
    size_t      file_size;          /**< File size */
    uint16_t    mapper;             /**< Mapper number */
    const char  *mapper_name;       /**< Mapper name (common ones) */
    uint32_t    prg_rom_size;       /**< PRG ROM size in bytes */
    uint32_t    chr_rom_size;       /**< CHR ROM size in bytes */
    uint32_t    prg_ram_size;       /**< PRG RAM size in bytes */
    uint32_t    chr_ram_size;       /**< CHR RAM size in bytes */
    int         prg_banks;          /**< Number of PRG banks */
    int         chr_banks;          /**< Number of CHR banks */
    nes_mirror_t mirroring;         /**< Mirroring type */
    bool        has_battery;        /**< Battery-backed RAM */
    bool        has_trainer;        /**< 512-byte trainer present */
    nes_tv_t    tv_system;          /**< TV system */
    nes_console_t console;          /**< Console type */
    bool        is_nes20;           /**< NES 2.0 format */
    uint8_t     submapper;          /**< Submapper (NES 2.0) */
} nes_info_t;

/**
 * @brief NES ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    nes_format_t format;            /**< Detected format */
    nes_header_t header;            /**< Parsed header */
    uint8_t     *prg_rom;           /**< PRG ROM pointer */
    uint8_t     *chr_rom;           /**< CHR ROM pointer */
    uint8_t     *trainer;           /**< Trainer pointer (if present) */
    bool        owns_data;          /**< true if we allocated */
} nes_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect NES ROM format
 * @param data ROM data
 * @param size Data size
 * @return ROM format
 */
nes_format_t nes_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Check if NES 2.0 format
 * @param header iNES header
 * @return true if NES 2.0
 */
bool nes_is_nes20(const nes_header_t *header);

/**
 * @brief Validate NES ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid
 */
bool nes_validate(const uint8_t *data, size_t size);

/**
 * @brief Get format name
 * @param format ROM format
 * @return Format name
 */
const char *nes_format_name(nes_format_t format);

/**
 * @brief Get mapper name
 * @param mapper Mapper number
 * @return Mapper name
 */
const char *nes_mapper_name(uint16_t mapper);

/**
 * @brief Get mirroring name
 * @param mirror Mirroring type
 * @return Mirroring name
 */
const char *nes_mirror_name(nes_mirror_t mirror);

/**
 * @brief Get TV system name
 * @param tv TV system
 * @return TV system name
 */
const char *nes_tv_name(nes_tv_t tv);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open NES ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int nes_open(const uint8_t *data, size_t size, nes_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int nes_load(const char *filename, nes_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void nes_close(nes_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int nes_get_info(const nes_rom_t *rom, nes_info_t *info);

/* ============================================================================
 * API Functions - Header Parsing
 * ============================================================================ */

/**
 * @brief Get mapper number from header
 * @param header iNES header
 * @return Mapper number
 */
uint16_t nes_get_mapper(const nes_header_t *header);

/**
 * @brief Get PRG ROM size in bytes
 * @param header iNES header
 * @return PRG ROM size
 */
uint32_t nes_get_prg_size(const nes_header_t *header);

/**
 * @brief Get CHR ROM size in bytes
 * @param header iNES header
 * @return CHR ROM size
 */
uint32_t nes_get_chr_size(const nes_header_t *header);

/**
 * @brief Get mirroring type
 * @param header iNES header
 * @return Mirroring type
 */
nes_mirror_t nes_get_mirroring(const nes_header_t *header);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void nes_print_info(const nes_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NES_H */
