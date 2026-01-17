/**
 * @file uft_nds.h
 * @brief Nintendo DS / DSi ROM Support
 * 
 * Support for Nintendo DS ROM formats:
 * - NDS (.nds) - Nintendo DS cartridge dump
 * - DSi enhanced games
 * - DSiWare
 * 
 * Features:
 * - ROM header parsing
 * - Icon/title extraction
 * - ARM7/ARM9 binary info
 * - Secure area detection
 * - DSi enhanced detection
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_NDS_H
#define UFT_NDS_H

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

/** Header size */
#define NDS_HEADER_SIZE         0x200   /**< 512 bytes */
#define NDS_LOGO_SIZE           156
#define NDS_LOGO_OFFSET         0xC0

/** Unit codes */
typedef enum {
    NDS_UNIT_NDS        = 0x00,     /**< Nintendo DS */
    NDS_UNIT_NDS_DSI    = 0x02,     /**< NDS + DSi enhanced */
    NDS_UNIT_DSI        = 0x03      /**< DSi only */
} nds_unit_t;

/** Region codes */
typedef enum {
    NDS_REGION_NORMAL   = 0x00,
    NDS_REGION_KOREA    = 0x40,
    NDS_REGION_CHINA    = 0x80
} nds_region_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief NDS ROM header (512 bytes)
 */
typedef struct {
    char        title[12];          /**< Game title */
    char        game_code[4];       /**< Game code */
    char        maker_code[2];      /**< Maker code */
    uint8_t     unit_code;          /**< Unit code */
    uint8_t     device_type;        /**< Encryption seed */
    uint8_t     device_capacity;    /**< Device capacity (128KB << n) */
    uint8_t     reserved1[7];       /**< Reserved */
    uint8_t     dsi_flags;          /**< DSi flags */
    uint8_t     nds_region;         /**< NDS region */
    uint8_t     version;            /**< ROM version */
    uint8_t     autostart;          /**< Autostart */
    
    /* ARM9 */
    uint32_t    arm9_offset;        /**< ARM9 ROM offset */
    uint32_t    arm9_entry;         /**< ARM9 entry address */
    uint32_t    arm9_load;          /**< ARM9 load address */
    uint32_t    arm9_size;          /**< ARM9 size */
    
    /* ARM7 */
    uint32_t    arm7_offset;        /**< ARM7 ROM offset */
    uint32_t    arm7_entry;         /**< ARM7 entry address */
    uint32_t    arm7_load;          /**< ARM7 load address */
    uint32_t    arm7_size;          /**< ARM7 size */
    
    /* File system */
    uint32_t    fnt_offset;         /**< File Name Table offset */
    uint32_t    fnt_size;           /**< File Name Table size */
    uint32_t    fat_offset;         /**< File Allocation Table offset */
    uint32_t    fat_size;           /**< File Allocation Table size */
    
    /* Overlays */
    uint32_t    arm9_overlay_offset;
    uint32_t    arm9_overlay_size;
    uint32_t    arm7_overlay_offset;
    uint32_t    arm7_overlay_size;
    
    /* Control */
    uint32_t    normal_cmd;         /**< Port 40001A4h Normal command */
    uint32_t    key1_cmd;           /**< Port 40001A4h KEY1 command */
    
    uint32_t    icon_offset;        /**< Icon/Title offset */
    uint16_t    secure_crc;         /**< Secure area CRC */
    uint16_t    secure_timeout;     /**< Secure area timeout */
    
    uint32_t    arm9_autoload;      /**< ARM9 auto load */
    uint32_t    arm7_autoload;      /**< ARM7 auto load */
    
    uint8_t     secure_disable[8];  /**< Secure area disable */
    
    uint32_t    total_size;         /**< Total used ROM size */
    uint32_t    header_size;        /**< ROM header size */
    
    uint8_t     reserved2[0x38];    /**< Reserved */
    
    uint8_t     logo[156];          /**< Nintendo logo */
    uint16_t    logo_crc;           /**< Logo CRC */
    uint16_t    header_crc;         /**< Header CRC */
} nds_header_t;

/**
 * @brief NDS ROM info
 */
typedef struct {
    char        title[13];          /**< Title (null-terminated) */
    char        game_code[5];       /**< Game code (null-terminated) */
    char        maker_code[3];      /**< Maker code */
    nds_unit_t  unit;               /**< Unit type */
    const char  *unit_name;         /**< Unit name */
    size_t      file_size;          /**< File size */
    uint32_t    total_size;         /**< Used ROM size */
    uint32_t    capacity;           /**< Cartridge capacity */
    uint8_t     version;            /**< ROM version */
    bool        is_dsi_enhanced;    /**< DSi enhanced */
    bool        is_dsi_exclusive;   /**< DSi only */
    uint32_t    arm9_size;          /**< ARM9 binary size */
    uint32_t    arm7_size;          /**< ARM7 binary size */
    bool        has_icon;           /**< Has icon/title */
    uint16_t    header_crc;         /**< Header CRC */
    bool        header_valid;       /**< Header CRC valid */
} nds_info_t;

/**
 * @brief NDS ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    nds_header_t header;            /**< Parsed header */
    bool        owns_data;          /**< true if we allocated */
} nds_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Validate NDS ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid
 */
bool nds_validate(const uint8_t *data, size_t size);

/**
 * @brief Get unit name
 * @param unit Unit code
 * @return Unit name
 */
const char *nds_unit_name(nds_unit_t unit);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open NDS ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int nds_open(const uint8_t *data, size_t size, nds_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int nds_load(const char *filename, nds_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void nds_close(nds_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int nds_get_info(const nds_rom_t *rom, nds_info_t *info);

/* ============================================================================
 * API Functions - CRC
 * ============================================================================ */

/**
 * @brief Calculate header CRC
 * @param data ROM data
 * @param size Data size
 * @return CRC value
 */
uint16_t nds_calculate_header_crc(const uint8_t *data, size_t size);

/**
 * @brief Verify header CRC
 * @param rom ROM context
 * @return true if valid
 */
bool nds_verify_header_crc(const nds_rom_t *rom);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void nds_print_info(const nds_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NDS_H */
