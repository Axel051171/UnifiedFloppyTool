/**
 * @file uft_cart7_hal.h
 * @brief 7-in-1 Cartridge Reader HAL Provider
 * 
 * Hardware abstraction layer for the 7-in-1 multi-system cartridge reader.
 * Supports: NES, SNES, N64, Mega Drive, GBA, GB/GBC
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#ifndef UFT_CART7_HAL_H
#define UFT_CART7_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define CART7_USB_VID               0x1209
#define CART7_USB_PID               0x7CA7
#define CART7_BAUDRATE              921600

#define CART7_MAX_DEVICES           8
#define CART7_MAX_ROM_SIZE          (64 * 1024 * 1024)  /* 64 MB max */
#define CART7_CHUNK_SIZE            4096

/*============================================================================
 * SLOT / SYSTEM TYPES  
 * (Only defined if cart7_protocol.h is not already included)
 *============================================================================*/

#ifndef CART7_PROTOCOL_H  /* Avoid redefinition conflict with cart7_protocol.h */

/*============================================================================
 * PROTOCOL CONSTANTS (from cart7_protocol.h)
 *============================================================================*/

#define CART7_SYNC_COMMAND          0xC7
#define CART7_SYNC_RESPONSE         0x7C
#define CART7_MAX_PAYLOAD           65535

typedef enum {
    CART7_CMD_PING              = 0x00,
    CART7_CMD_GET_INFO          = 0x01,
    CART7_CMD_SELECT_SLOT       = 0x03,
    CART7_CMD_GET_CART_STATUS   = 0x04,
    CART7_CMD_ABORT             = 0x05,
    CART7_CMD_NES_GET_HEADER    = 0x10,
    CART7_CMD_NES_READ_PRG      = 0x11,
    CART7_CMD_NES_READ_CHR      = 0x12,
    CART7_CMD_NES_READ_SRAM     = 0x13,
    CART7_CMD_NES_WRITE_SRAM    = 0x14,
    CART7_CMD_NES_DETECT_MAPPER = 0x15,
    CART7_CMD_SNES_GET_HEADER   = 0x20,
    CART7_CMD_SNES_READ_ROM     = 0x21,
    CART7_CMD_SNES_READ_SRAM    = 0x22,
    CART7_CMD_SNES_WRITE_SRAM   = 0x23,
    CART7_CMD_SNES_DETECT_TYPE  = 0x24,
} cart7_cmd_t;

/*============================================================================
 * SLOT / SYSTEM TYPES
 *============================================================================*/

typedef enum {
    CART7_SLOT_NONE     = 0x00,
    CART7_SLOT_NES      = 0x01,
    CART7_SLOT_SNES     = 0x02,
    CART7_SLOT_N64      = 0x03,
    CART7_SLOT_MD       = 0x04,
    CART7_SLOT_GBA      = 0x05,
    CART7_SLOT_GB       = 0x06,
    CART7_SLOT_FC       = 0x07,
    CART7_SLOT_SFC      = 0x08,
    CART7_SLOT_AUTO     = 0xFF,
} cart7_slot_t;

/*============================================================================
 * STATUS CODES
 *============================================================================*/

typedef enum {
    CART7_OK                = 0,
    CART7_ERR_NO_DEVICE     = -1,
    CART7_ERR_NOT_OPEN      = -2,
    CART7_ERR_TIMEOUT       = -3,
    CART7_ERR_CRC           = -4,
    CART7_ERR_NO_CART       = -5,
    CART7_ERR_WRONG_SLOT    = -6,
    CART7_ERR_READ          = -7,
    CART7_ERR_WRITE         = -8,
    CART7_ERR_UNSUPPORTED   = -9,
    CART7_ERR_ABORTED       = -10,
    CART7_ERR_PARAM         = -11,
} cart7_error_t;

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

typedef struct {
    uint8_t  protocol_version;
    uint8_t  hw_revision;
    char     fw_version[16];
    char     serial[16];
    char     build_date[12];
    uint32_t features;
    uint8_t  slot_count;
    uint8_t  active_slot;
} cart7_device_info_t;

/*============================================================================
 * CARTRIDGE STATUS
 *============================================================================*/

typedef struct {
    bool     inserted;
    uint8_t  slot;
    uint8_t  detected_system;
    uint8_t  voltage;           /* 0=auto, 33=3.3V, 50=5V */
} cart7_cart_status_t;

/*============================================================================
 * NES CARTRIDGE INFO
 *============================================================================*/

typedef struct {
    uint32_t prg_size;          /* PRG-ROM size in bytes */
    uint32_t chr_size;          /* CHR-ROM size (0 = CHR-RAM) */
    uint16_t mapper;            /* Mapper number */
    uint8_t  submapper;
    uint8_t  mirroring;         /* 0=H, 1=V, 2=4-screen */
    bool     has_battery;
    bool     has_trainer;
    uint8_t  prg_ram_size;      /* In 8KB units */
    uint8_t  chr_ram_size;
    uint8_t  tv_system;         /* 0=NTSC, 1=PAL */
    bool     nes2_format;
} cart7_nes_info_t;

/*============================================================================
 * SNES CARTRIDGE INFO
 *============================================================================*/

typedef enum {
    SNES_TYPE_LOROM     = 1,
    SNES_TYPE_HIROM     = 2,
    SNES_TYPE_EXLOROM   = 3,
    SNES_TYPE_EXHIROM   = 4,
    SNES_TYPE_SA1       = 5,
    SNES_TYPE_SDD1      = 6,
    SNES_TYPE_SPC7110   = 7,
} snes_rom_type_t;

typedef struct {
    char     title[22];
    uint8_t  rom_type;          /* snes_rom_type_t */
    uint8_t  special_chip;      /* DSP, SA-1, etc. */
    uint32_t rom_size;
    uint32_t sram_size;
    uint8_t  country;
    uint8_t  license;
    uint8_t  version;
    bool     has_battery;
    uint16_t checksum;
    uint16_t checksum_comp;
    bool     fast_rom;
} cart7_snes_info_t;

/*============================================================================
 * N64 CARTRIDGE INFO
 *============================================================================*/

typedef enum {
    N64_SAVE_NONE       = 0,
    N64_SAVE_EEPROM_4K  = 1,
    N64_SAVE_EEPROM_16K = 2,
    N64_SAVE_SRAM       = 3,
    N64_SAVE_FLASH      = 4,
} n64_save_type_t;

typedef enum {
    N64_CIC_6101 = 1,
    N64_CIC_6102 = 2,
    N64_CIC_6103 = 3,
    N64_CIC_6105 = 4,
    N64_CIC_6106 = 5,
} n64_cic_type_t;

typedef struct {
    char     title[21];
    char     game_code[5];
    uint8_t  version;
    uint32_t crc1;
    uint32_t crc2;
    uint32_t rom_size;
    uint8_t  cic_type;          /* n64_cic_type_t */
    uint8_t  save_type;         /* n64_save_type_t */
    uint8_t  region;            /* 'N', 'P', 'J' */
} cart7_n64_info_t;

/*============================================================================
 * MEGA DRIVE CARTRIDGE INFO
 *============================================================================*/

typedef struct {
    char     console[17];
    char     copyright[17];
    char     title_domestic[49];
    char     title_overseas[49];
    char     serial[15];
    char     region[4];
    uint16_t checksum;
    uint32_t rom_size;
    uint32_t sram_size;
    bool     has_sram;
    uint8_t  sram_type;         /* 0=none, 1=SRAM, 2=EEPROM */
    uint8_t  mapper_type;
} cart7_md_info_t;

/*============================================================================
 * GBA CARTRIDGE INFO
 *============================================================================*/

typedef enum {
    GBA_SAVE_NONE       = 0,
    GBA_SAVE_EEPROM_512 = 1,
    GBA_SAVE_EEPROM_8K  = 2,
    GBA_SAVE_SRAM       = 3,
    GBA_SAVE_FLASH_64K  = 4,
    GBA_SAVE_FLASH_128K = 5,
} gba_save_type_t;

typedef enum {
    GBA_GPIO_NONE   = 0,
    GBA_GPIO_RTC    = 1,
    GBA_GPIO_SOLAR  = 2,
    GBA_GPIO_GYRO   = 3,
    GBA_GPIO_RUMBLE = 4,
} gba_gpio_type_t;

typedef struct {
    char     title[13];
    char     game_code[5];
    char     maker_code[3];
    uint8_t  version;
    uint32_t rom_size;
    uint8_t  save_type;         /* gba_save_type_t */
    uint8_t  gpio_type;         /* gba_gpio_type_t */
    bool     logo_valid;
    bool     checksum_valid;
} cart7_gba_info_t;

/*============================================================================
 * GB/GBC CARTRIDGE INFO
 *============================================================================*/

typedef enum {
    GB_MBC_NONE     = 0x00,
    GB_MBC_MBC1     = 0x01,
    GB_MBC_MBC2     = 0x05,
    GB_MBC_MBC3     = 0x0F,
    GB_MBC_MBC5     = 0x19,
    GB_MBC_MBC6     = 0x20,
    GB_MBC_MBC7     = 0x22,
    GB_MBC_MMM01    = 0x0B,
    GB_MBC_HUC1     = 0xFF,
    GB_MBC_HUC3     = 0xFE,
} gb_mbc_type_t;

typedef struct {
    char     title[17];
    char     manufacturer[5];
    char     licensee[3];
    uint8_t  cgb_flag;
    uint8_t  sgb_flag;
    uint8_t  cart_type;
    uint32_t rom_size;
    uint32_t ram_size;
    uint8_t  mbc_type;          /* Detected MBC */
    bool     has_battery;
    bool     has_rtc;
    bool     has_rumble;
    bool     is_gbc;
    bool     logo_valid;
    bool     checksum_valid;
    uint8_t  version;
    uint8_t  header_checksum;
    uint16_t global_checksum;
} cart7_gb_info_t;

/*============================================================================
 * GB RTC DATA
 *============================================================================*/

typedef struct {
    uint8_t  seconds;
    uint8_t  minutes;
    uint8_t  hours;
    uint16_t days;
    bool     halted;
    bool     day_overflow;
} cart7_gb_rtc_t;

#endif /* !CART7_PROTOCOL_H */

/*============================================================================
 * PROGRESS CALLBACK
 *============================================================================*/

typedef void (*cart7_progress_cb)(uint64_t current, uint64_t total, 
                                   uint32_t speed_kbps, void *user_data);

/*============================================================================
 * DEVICE HANDLE
 *============================================================================*/

typedef struct cart7_device cart7_device_t;

/*============================================================================
 * DEVICE ENUMERATION
 *============================================================================*/

/**
 * @brief Enumerate available Cart7 devices
 * @param ports Array to store port names
 * @param max_ports Maximum number of ports
 * @return Number of devices found, or negative error
 */
int cart7_enumerate(char **ports, int max_ports);

/**
 * @brief Open a Cart7 device
 * @param port Port name (e.g., "COM3" or "/dev/ttyACM0")
 * @param device Pointer to store device handle
 * @return 0 on success, negative error code on failure
 */
int cart7_open(const char *port, cart7_device_t **device);

/**
 * @brief Close a Cart7 device
 * @param device Device handle
 */
void cart7_close(cart7_device_t *device);

/**
 * @brief Check if device is connected
 * @param device Device handle
 * @return true if connected
 */
bool cart7_is_connected(cart7_device_t *device);

/*============================================================================
 * DEVICE INFORMATION
 *============================================================================*/

/**
 * @brief Get device information
 * @param device Device handle
 * @param info Pointer to store info
 * @return 0 on success, negative error code on failure
 */
int cart7_get_info(cart7_device_t *device, cart7_device_info_t *info);

/**
 * @brief Get cartridge status
 * @param device Device handle
 * @param status Pointer to store status
 * @return 0 on success, negative error code on failure
 */
int cart7_get_cart_status(cart7_device_t *device, cart7_cart_status_t *status);

/*============================================================================
 * SLOT SELECTION
 *============================================================================*/

/**
 * @brief Select active slot/system
 * @param device Device handle
 * @param slot Slot to select (cart7_slot_t)
 * @param voltage Voltage (0=auto, 33=3.3V, 50=5V)
 * @return 0 on success, negative error code on failure
 */
int cart7_select_slot(cart7_device_t *device, cart7_slot_t slot, uint8_t voltage);

/*============================================================================
 * NES / FAMICOM
 *============================================================================*/

int cart7_nes_get_info(cart7_device_t *device, cart7_nes_info_t *info);
int cart7_nes_read_prg(cart7_device_t *device, uint8_t *buffer, uint32_t offset, 
                       uint32_t length, cart7_progress_cb cb, void *user);
int cart7_nes_read_chr(cart7_device_t *device, uint8_t *buffer, uint32_t offset,
                       uint32_t length, cart7_progress_cb cb, void *user);
int cart7_nes_read_sram(cart7_device_t *device, uint8_t *buffer, uint32_t *size);
int cart7_nes_write_sram(cart7_device_t *device, const uint8_t *buffer, uint32_t size);
int cart7_nes_detect_mapper(cart7_device_t *device, uint16_t *mapper);

/*============================================================================
 * SNES / SUPER FAMICOM
 *============================================================================*/

int cart7_snes_get_info(cart7_device_t *device, cart7_snes_info_t *info);
int cart7_snes_read_rom(cart7_device_t *device, uint8_t *buffer, uint32_t offset,
                        uint32_t length, cart7_progress_cb cb, void *user);
int cart7_snes_read_sram(cart7_device_t *device, uint8_t *buffer, uint32_t *size);
int cart7_snes_write_sram(cart7_device_t *device, const uint8_t *buffer, uint32_t size);
int cart7_snes_detect_type(cart7_device_t *device, snes_rom_type_t *type);

/*============================================================================
 * NINTENDO 64
 *============================================================================*/

int cart7_n64_get_info(cart7_device_t *device, cart7_n64_info_t *info);
int cart7_n64_read_rom(cart7_device_t *device, uint8_t *buffer, uint32_t offset,
                       uint32_t length, cart7_progress_cb cb, void *user);
int cart7_n64_read_save(cart7_device_t *device, uint8_t *buffer, uint32_t *size);
int cart7_n64_write_save(cart7_device_t *device, const uint8_t *buffer, uint32_t size);
int cart7_n64_detect_save(cart7_device_t *device, n64_save_type_t *type);
int cart7_n64_get_cic(cart7_device_t *device, n64_cic_type_t *cic);

/*============================================================================
 * MEGA DRIVE / GENESIS
 *============================================================================*/

int cart7_md_get_info(cart7_device_t *device, cart7_md_info_t *info);
int cart7_md_read_rom(cart7_device_t *device, uint8_t *buffer, uint32_t offset,
                      uint32_t length, cart7_progress_cb cb, void *user);
int cart7_md_read_sram(cart7_device_t *device, uint8_t *buffer, uint32_t *size);
int cart7_md_write_sram(cart7_device_t *device, const uint8_t *buffer, uint32_t size);
int cart7_md_verify_checksum(cart7_device_t *device, bool *valid);

/*============================================================================
 * GAME BOY ADVANCE
 *============================================================================*/

int cart7_gba_get_info(cart7_device_t *device, cart7_gba_info_t *info);
int cart7_gba_read_rom(cart7_device_t *device, uint8_t *buffer, uint32_t offset,
                       uint32_t length, cart7_progress_cb cb, void *user);
int cart7_gba_read_save(cart7_device_t *device, uint8_t *buffer, uint32_t *size);
int cart7_gba_write_save(cart7_device_t *device, const uint8_t *buffer, uint32_t size);
int cart7_gba_detect_save(cart7_device_t *device, gba_save_type_t *type);
int cart7_gba_read_gpio(cart7_device_t *device, uint8_t *data, uint32_t *size);

/*============================================================================
 * GAME BOY / GAME BOY COLOR
 *============================================================================*/

int cart7_gb_get_info(cart7_device_t *device, cart7_gb_info_t *info);
int cart7_gb_read_rom(cart7_device_t *device, uint8_t *buffer, uint32_t offset,
                      uint32_t length, cart7_progress_cb cb, void *user);
int cart7_gb_read_sram(cart7_device_t *device, uint8_t *buffer, uint32_t *size);
int cart7_gb_write_sram(cart7_device_t *device, const uint8_t *buffer, uint32_t size);
int cart7_gb_detect_mbc(cart7_device_t *device, gb_mbc_type_t *mbc);
int cart7_gb_read_rtc(cart7_device_t *device, cart7_gb_rtc_t *rtc);
int cart7_gb_write_rtc(cart7_device_t *device, const cart7_gb_rtc_t *rtc);

/*============================================================================
 * CONTROL
 *============================================================================*/

/**
 * @brief Abort current operation
 * @param device Device handle
 * @return 0 on success
 */
int cart7_abort(cart7_device_t *device);

/**
 * @brief Get error string
 * @param error Error code
 * @return Error string
 */
const char *cart7_strerror(int error);

/**
 * @brief Get slot name
 * @param slot Slot type
 * @return Slot name string
 */
const char *cart7_slot_name(cart7_slot_t slot);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CART7_HAL_H */
