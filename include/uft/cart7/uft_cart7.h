/**
 * @file uft_cart7.h
 * @brief 7-in-1 Cartridge Reader HAL Provider
 * 
 * Multi-System Cartridge Reader supporting:
 * - NES / Famicom
 * - SNES / Super Famicom
 * - Nintendo 64
 * - Sega Mega Drive / Genesis
 * - Game Boy Advance
 * - Game Boy / Game Boy Color
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#ifndef UFT_CART7_H
#define UFT_CART7_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../cart7/cart7_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DEVICE HANDLE
 *============================================================================*/

typedef struct uft_cart7_device uft_cart7_device_t;

/*============================================================================
 * ERROR CODES
 *============================================================================*/

typedef enum {
    CART7_OK                = 0,
    CART7_ERROR             = -1,
    CART7_ERROR_NOT_FOUND   = -2,
    CART7_ERROR_ACCESS      = -3,
    CART7_ERROR_NO_CART     = -4,
    CART7_ERROR_WRONG_SLOT  = -5,
    CART7_ERROR_READ        = -6,
    CART7_ERROR_WRITE       = -7,
    CART7_ERROR_TIMEOUT     = -8,
    CART7_ERROR_UNSUPPORTED = -9,
    CART7_ERROR_CRC         = -10,
    CART7_ERROR_ABORTED     = -11,
} uft_cart7_error_t;

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

typedef struct {
    char     port[64];              /* COM port / device path */
    char     firmware_version[32];
    char     serial[32];
    uint32_t features;              /* Supported systems bitmask */
    uint8_t  hw_revision;
} uft_cart7_device_info_t;

/*============================================================================
 * CARTRIDGE INFO - SYSTEM SPECIFIC
 *============================================================================*/

/* NES/Famicom cartridge info */
typedef struct {
    uint32_t prg_size;              /* PRG-ROM size in bytes */
    uint32_t chr_size;              /* CHR-ROM size (0 = CHR-RAM) */
    uint16_t mapper;                /* Mapper number */
    uint8_t  submapper;
    uint8_t  mirroring;             /* 0=H, 1=V, 2=4-screen */
    bool     has_battery;
    bool     has_trainer;
    uint8_t  prg_ram_size;          /* In 8KB units */
    uint8_t  chr_ram_size;
    uint8_t  tv_system;             /* 0=NTSC, 1=PAL */
} uft_cart7_nes_info_t;

/* SNES/SFC cartridge info */
typedef struct {
    char     title[22];
    uint8_t  rom_type;              /* 1=LoROM, 2=HiROM, etc. */
    uint8_t  special_chip;          /* DSP, SA-1, etc. */
    uint32_t rom_size;
    uint32_t sram_size;
    uint8_t  country;
    uint8_t  version;
    bool     has_battery;
    uint16_t checksum;
    uint16_t checksum_comp;
} uft_cart7_snes_info_t;

/* N64 cartridge info */
typedef struct {
    char     title[21];
    char     game_code[5];
    uint32_t crc1;
    uint32_t crc2;
    uint8_t  version;
    uint8_t  cic_type;              /* CIC chip type */
    uint8_t  save_type;             /* 0=none, 1=EEPROM4K, 2=EEPROM16K, 3=SRAM, 4=Flash */
    uint8_t  region;                /* 'N', 'P', 'J' */
    uint32_t rom_size;
} uft_cart7_n64_info_t;

/* Mega Drive/Genesis cartridge info */
typedef struct {
    char     console[17];
    char     title_domestic[49];
    char     title_overseas[49];
    char     serial[15];
    char     region[4];
    uint16_t checksum;
    uint32_t rom_size;
    uint32_t sram_size;
    bool     has_sram;
    uint8_t  sram_type;             /* 0=none, 1=SRAM, 2=EEPROM */
} uft_cart7_md_info_t;

/* GBA cartridge info */
typedef struct {
    char     title[13];
    char     game_code[5];
    char     maker_code[3];
    uint8_t  version;
    uint32_t rom_size;
    uint8_t  save_type;             /* 0=none, 1-2=EEPROM, 3=SRAM, 4-5=Flash */
    uint8_t  gpio_type;             /* 0=none, 1=RTC, 2=Solar, 3=Gyro, 4=Rumble */
    bool     logo_valid;
    bool     checksum_valid;
} uft_cart7_gba_info_t;

/* GB/GBC cartridge info */
typedef struct {
    char     title[17];
    uint8_t  cgb_flag;
    uint8_t  sgb_flag;
    uint8_t  cart_type;             /* MBC type code */
    uint8_t  mbc_type;              /* Detected MBC */
    uint32_t rom_size;
    uint32_t ram_size;
    bool     has_battery;
    bool     has_rtc;
    bool     has_rumble;
    bool     is_gbc;
    bool     logo_valid;
    uint8_t  header_checksum;
    uint16_t global_checksum;
} uft_cart7_gb_info_t;

/* Generic cart info union */
typedef struct {
    cart7_slot_t slot;
    bool         inserted;
    union {
        uft_cart7_nes_info_t  nes;
        uft_cart7_snes_info_t snes;
        uft_cart7_n64_info_t  n64;
        uft_cart7_md_info_t   md;
        uft_cart7_gba_info_t  gba;
        uft_cart7_gb_info_t   gb;
    };
} uft_cart7_cart_info_t;

/*============================================================================
 * PROGRESS CALLBACK
 *============================================================================*/

typedef bool (*uft_cart7_progress_cb)(uint64_t bytes_done, 
                                       uint64_t bytes_total, 
                                       void *user_data);

/*============================================================================
 * DEVICE ENUMERATION
 *============================================================================*/

/**
 * Find all connected Cart7 devices
 * 
 * @param ports     Array to fill with COM ports
 * @param max_ports Maximum number of ports
 * @return          Number found, or negative error
 */
int uft_cart7_enumerate(char **ports, int max_ports);

/*============================================================================
 * CONNECTION
 *============================================================================*/

/**
 * Open Cart7 device
 */
uft_cart7_error_t uft_cart7_open(const char *port, uft_cart7_device_t **device);

/**
 * Close device
 */
void uft_cart7_close(uft_cart7_device_t *device);

/**
 * Check if connected
 */
bool uft_cart7_is_connected(uft_cart7_device_t *device);

/**
 * Get device info
 */
uft_cart7_error_t uft_cart7_get_info(uft_cart7_device_t *device, 
                                     uft_cart7_device_info_t *info);

/*============================================================================
 * SLOT SELECTION
 *============================================================================*/

/**
 * Select cartridge slot/system
 */
uft_cart7_error_t uft_cart7_select_slot(uft_cart7_device_t *device, 
                                        cart7_slot_t slot);

/**
 * Get current slot
 */
cart7_slot_t uft_cart7_get_current_slot(uft_cart7_device_t *device);

/**
 * Check if cartridge is inserted in current slot
 */
bool uft_cart7_cart_inserted(uft_cart7_device_t *device);

/*============================================================================
 * CARTRIDGE INFO
 *============================================================================*/

/**
 * Get cartridge info for current slot
 */
uft_cart7_error_t uft_cart7_get_cart_info(uft_cart7_device_t *device,
                                          uft_cart7_cart_info_t *info);

/*============================================================================
 * ROM READING
 *============================================================================*/

/**
 * Read ROM data
 * 
 * @param device    Device handle
 * @param offset    Offset into ROM
 * @param buffer    Output buffer
 * @param length    Bytes to read
 * @return          Bytes read, or negative error
 */
int64_t uft_cart7_read_rom(uft_cart7_device_t *device,
                           uint64_t offset,
                           void *buffer,
                           size_t length);

/**
 * Dump entire ROM to file
 */
uft_cart7_error_t uft_cart7_dump_rom(uft_cart7_device_t *device,
                                     const char *filename,
                                     uft_cart7_progress_cb progress,
                                     void *user_data);

/*============================================================================
 * SAVE DATA (SRAM/EEPROM/FLASH)
 *============================================================================*/

/**
 * Read save data
 */
int64_t uft_cart7_read_save(uft_cart7_device_t *device,
                            uint64_t offset,
                            void *buffer,
                            size_t length);

/**
 * Write save data
 */
int64_t uft_cart7_write_save(uft_cart7_device_t *device,
                             uint64_t offset,
                             const void *buffer,
                             size_t length);

/**
 * Dump save to file
 */
uft_cart7_error_t uft_cart7_dump_save(uft_cart7_device_t *device,
                                      const char *filename);

/**
 * Restore save from file
 */
uft_cart7_error_t uft_cart7_restore_save(uft_cart7_device_t *device,
                                         const char *filename);

/*============================================================================
 * NES SPECIFIC
 *============================================================================*/

/**
 * Read NES PRG-ROM
 */
int64_t uft_cart7_nes_read_prg(uft_cart7_device_t *device,
                               uint32_t offset,
                               void *buffer,
                               size_t length);

/**
 * Read NES CHR-ROM
 */
int64_t uft_cart7_nes_read_chr(uft_cart7_device_t *device,
                               uint32_t offset,
                               void *buffer,
                               size_t length);

/**
 * Detect NES mapper
 */
int uft_cart7_nes_detect_mapper(uft_cart7_device_t *device);

/**
 * Create iNES file from cartridge
 */
uft_cart7_error_t uft_cart7_nes_create_ines(uft_cart7_device_t *device,
                                            const char *filename,
                                            uft_cart7_progress_cb progress,
                                            void *user_data);

/*============================================================================
 * SNES SPECIFIC
 *============================================================================*/

/**
 * Detect SNES ROM type (LoROM/HiROM/etc.)
 */
int uft_cart7_snes_detect_type(uft_cart7_device_t *device);

/**
 * Create SFC file from cartridge
 */
uft_cart7_error_t uft_cart7_snes_create_sfc(uft_cart7_device_t *device,
                                            const char *filename,
                                            uft_cart7_progress_cb progress,
                                            void *user_data);

/*============================================================================
 * N64 SPECIFIC
 *============================================================================*/

/**
 * Detect N64 save type
 */
int uft_cart7_n64_detect_save(uft_cart7_device_t *device);

/**
 * Get N64 CIC type
 */
int uft_cart7_n64_get_cic(uft_cart7_device_t *device);

/**
 * Create Z64 file from cartridge (big-endian)
 */
uft_cart7_error_t uft_cart7_n64_create_z64(uft_cart7_device_t *device,
                                           const char *filename,
                                           uft_cart7_progress_cb progress,
                                           void *user_data);

/*============================================================================
 * MEGA DRIVE SPECIFIC
 *============================================================================*/

/**
 * Verify Mega Drive checksum
 */
bool uft_cart7_md_verify_checksum(uft_cart7_device_t *device);

/**
 * Create BIN file from cartridge
 */
uft_cart7_error_t uft_cart7_md_create_bin(uft_cart7_device_t *device,
                                          const char *filename,
                                          uft_cart7_progress_cb progress,
                                          void *user_data);

/*============================================================================
 * GBA SPECIFIC
 *============================================================================*/

/**
 * Detect GBA save type
 */
int uft_cart7_gba_detect_save(uft_cart7_device_t *device);

/**
 * Read GBA GPIO (RTC, Solar sensor, etc.)
 */
uft_cart7_error_t uft_cart7_gba_read_gpio(uft_cart7_device_t *device,
                                          void *buffer,
                                          size_t *size);

/**
 * Create GBA file from cartridge
 */
uft_cart7_error_t uft_cart7_gba_create_gba(uft_cart7_device_t *device,
                                           const char *filename,
                                           uft_cart7_progress_cb progress,
                                           void *user_data);

/*============================================================================
 * GB/GBC SPECIFIC
 *============================================================================*/

/**
 * Detect GB MBC type
 */
int uft_cart7_gb_detect_mbc(uft_cart7_device_t *device);

/**
 * Read GB RTC (MBC3)
 */
uft_cart7_error_t uft_cart7_gb_read_rtc(uft_cart7_device_t *device,
                                        cart7_gb_rtc_t *rtc);

/**
 * Write GB RTC (MBC3)
 */
uft_cart7_error_t uft_cart7_gb_write_rtc(uft_cart7_device_t *device,
                                         const cart7_gb_rtc_t *rtc);

/**
 * Create GB file from cartridge
 */
uft_cart7_error_t uft_cart7_gb_create_gb(uft_cart7_device_t *device,
                                         const char *filename,
                                         uft_cart7_progress_cb progress,
                                         void *user_data);

/*============================================================================
 * ABORT
 *============================================================================*/

/**
 * Abort current operation
 */
void uft_cart7_abort(uft_cart7_device_t *device);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CART7_H */
