/**
 * @file uft_crt.h
 * @brief CRT Cartridge Image Format Support
 * 
 * Complete CRT format handling for C64 cartridges:
 * - Parse CRT header and CHIP packets
 * - Extract ROM data
 * - Create CRT from ROM files
 * - Support for 50+ cartridge types
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_CRT_H
#define UFT_CRT_H

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

#define CRT_MAGIC               "C64 CARTRIDGE   "
#define CRT_MAGIC_LEN           16
#define CRT_HEADER_SIZE         64
#define CRT_CHIP_MAGIC          "CHIP"
#define CRT_CHIP_HEADER_SIZE    16
#define CRT_MAX_CHIPS           128

/** ROM types */
#define CRT_ROM_TYPE_ROM        0
#define CRT_ROM_TYPE_RAM        1
#define CRT_ROM_TYPE_FLASH      2
#define CRT_ROM_TYPE_EEPROM     3

/** Cartridge types */
typedef enum {
    CRT_TYPE_NORMAL             = 0,
    CRT_TYPE_ACTION_REPLAY      = 1,
    CRT_TYPE_KCS_POWER          = 2,
    CRT_TYPE_FINAL_III          = 3,
    CRT_TYPE_SIMONS_BASIC       = 4,
    CRT_TYPE_OCEAN_1            = 5,
    CRT_TYPE_EXPERT             = 6,
    CRT_TYPE_FUN_PLAY           = 7,
    CRT_TYPE_SUPER_GAMES        = 8,
    CRT_TYPE_ATOMIC_POWER       = 9,
    CRT_TYPE_EPYX_FASTLOAD      = 10,
    CRT_TYPE_WESTERMANN         = 11,
    CRT_TYPE_REX                = 12,
    CRT_TYPE_FINAL_I            = 13,
    CRT_TYPE_MAGIC_FORMEL       = 14,
    CRT_TYPE_GS                 = 15,
    CRT_TYPE_WARP_SPEED         = 16,
    CRT_TYPE_DINAMIC            = 17,
    CRT_TYPE_ZAXXON             = 18,
    CRT_TYPE_MAGIC_DESK         = 19,
    CRT_TYPE_SUPER_SNAPSHOT_V5  = 20,
    CRT_TYPE_COMAL_80           = 21,
    CRT_TYPE_EASYFLASH          = 32,
    CRT_TYPE_RETRO_REPLAY       = 36,
    CRT_TYPE_MMC64              = 37,
    CRT_TYPE_IDE64              = 39,
    CRT_TYPE_GMOD2              = 60,
    CRT_TYPE_MAX                = 255
} crt_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

typedef struct {
    char        magic[16];
    uint32_t    header_length;
    uint16_t    version;
    uint16_t    hw_type;
    uint8_t     exrom;
    uint8_t     game;
    uint8_t     subtype;
    uint8_t     reserved[5];
    char        name[32];
} crt_header_t;

typedef struct {
    char        magic[4];
    uint32_t    packet_length;
    uint16_t    chip_type;
    uint16_t    bank;
    uint16_t    load_address;
    uint16_t    rom_size;
} crt_chip_header_t;

typedef struct {
    crt_chip_header_t header;
    uint8_t     *data;
    size_t      data_size;
    size_t      file_offset;
} crt_chip_t;

typedef struct {
    char        name[33];
    crt_type_t  type;
    uint16_t    version;
    uint8_t     exrom;
    uint8_t     game;
    int         num_chips;
    size_t      total_rom_size;
    int         num_banks;
} crt_info_t;

typedef struct {
    uint8_t     *data;
    size_t      size;
    crt_header_t header;
    crt_chip_t  *chips;
    int         num_chips;
    bool        owns_data;
} crt_image_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

int crt_open(const uint8_t *data, size_t size, crt_image_t *image);
int crt_load(const char *filename, crt_image_t *image);
int crt_save(const crt_image_t *image, const char *filename);
void crt_close(crt_image_t *image);
bool crt_validate(const uint8_t *data, size_t size);
bool crt_detect(const uint8_t *data, size_t size);

int crt_get_info(const crt_image_t *image, crt_info_t *info);
const char *crt_type_name(crt_type_t type);
void crt_get_name(const crt_image_t *image, char *name);

int crt_get_chip_count(const crt_image_t *image);
int crt_get_chip(const crt_image_t *image, int index, crt_chip_t *chip);
int crt_extract_rom(const crt_image_t *image, uint8_t *buffer,
                    size_t buffer_size, size_t *extracted);

int crt_create(const char *name, crt_type_t type, uint8_t exrom, uint8_t game,
               crt_image_t *image);
int crt_add_chip(crt_image_t *image, uint16_t bank, uint16_t load_address,
                 const uint8_t *data, size_t size, uint16_t chip_type);
int crt_create_8k(const char *name, const uint8_t *rom_data, crt_image_t *image);
int crt_create_16k(const char *name, const uint8_t *rom_data, crt_image_t *image);

void crt_print_info(const crt_image_t *image, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRT_H */
