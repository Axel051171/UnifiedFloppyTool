/**
 * @file cart7_protocol.h
 * @brief 7-in-1 Cartridge Reader Protocol Specification
 * 
 * Multi-System Cartridge Reader supporting:
 * - NES / Famicom
 * - SNES / Super Famicom
 * - Nintendo 64
 * - Sega Mega Drive / Genesis
 * - Game Boy Advance
 * - Game Boy / Game Boy Color
 * 
 * Hardware: STM32/ESP32 based multi-slot reader
 * Interface: USB CDC Serial
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#ifndef CART7_PROTOCOL_H
#define CART7_PROTOCOL_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * USB CONNECTION PARAMETERS
 *============================================================================*/

#define CART7_USB_VID               0x1209      /* pid.codes VID */
#define CART7_USB_PID               0x7CA7      /* "7CArt" */
#define CART7_USB_BAUDRATE          921600
#define CART7_USB_TIMEOUT_MS        5000

/*============================================================================
 * FRAME FORMAT
 *============================================================================*/

/*
 * COMMAND FRAME (Host → Device):
 * ┌──────────┬──────────┬──────────┬────────────────┬──────────┐
 * │ SYNC (1) │ CMD (1)  │ LEN (2)  │ PAYLOAD (0-N)  │ CRC8 (1) │
 * │   0xC7   │ Command  │ LE16     │ Command Data   │ CRC      │
 * └──────────┴──────────┴──────────┴────────────────┴──────────┘
 * 
 * RESPONSE FRAME (Device → Host):
 * ┌──────────┬──────────┬──────────┬──────────┬────────────────┬──────────┐
 * │ SYNC (1) │ STATUS(1)│ CMD (1)  │ LEN (2)  │ DATA (0-N)     │ CRC8 (1) │
 * │   0x7C   │ Status   │ Echo Cmd │ LE16     │ Response Data  │ CRC      │
 * └──────────┴──────────┴──────────┴──────────┴────────────────┴──────────┘
 * 
 * STREAM FRAME (for large reads):
 * ┌──────────┬──────────┬──────────┬────────────────┬──────────┐
 * │ SYNC (1) │ SEQ (2)  │ LEN (2)  │ DATA (N)       │ CRC8 (1) │
 * │   0xCC   │ LE16     │ LE16     │ Chunk Data     │ CRC      │
 * └──────────┴──────────┴──────────┴────────────────┴──────────┘
 */

#define CART7_SYNC_COMMAND          0xC7
#define CART7_SYNC_RESPONSE         0x7C
#define CART7_SYNC_STREAM           0xCC
#define CART7_SYNC_EVENT            0xEE

#define CART7_MAX_PAYLOAD           65535
#define CART7_STREAM_CHUNK_SIZE     4096

/*============================================================================
 * SLOT / SYSTEM TYPES
 *============================================================================*/

typedef enum {
    SLOT_NONE           = 0x00,
    SLOT_NES            = 0x01,     /* NES (72-pin) */
    SLOT_SNES           = 0x02,     /* SNES (62-pin) */
    SLOT_N64            = 0x03,     /* Nintendo 64 */
    SLOT_MD             = 0x04,     /* Mega Drive / Genesis */
    SLOT_GBA            = 0x05,     /* Game Boy Advance */
    SLOT_GB             = 0x06,     /* Game Boy / Game Boy Color */
    SLOT_FC             = 0x07,     /* Famicom (60-pin) */
    SLOT_SFC            = 0x08,     /* Super Famicom */
    SLOT_3DS            = 0x09,     /* Nintendo 3DS (17-pin, 1.8V!) */
    SLOT_AUTO           = 0xFF,     /* Auto-detect */
} cart7_slot_t;

/*============================================================================
 * STATUS CODES
 *============================================================================*/

typedef enum {
    /* Success (0x00-0x0F) */
    CART7_STATUS_OK             = 0x00,
    CART7_STATUS_OK_MORE        = 0x01,
    CART7_STATUS_OK_DONE        = 0x02,
    
    /* General Errors (0x10-0x1F) */
    CART7_STATUS_ERROR          = 0x10,
    CART7_STATUS_UNKNOWN_CMD    = 0x11,
    CART7_STATUS_INVALID_PARAM  = 0x12,
    CART7_STATUS_CRC_ERROR      = 0x13,
    CART7_STATUS_TIMEOUT        = 0x14,
    CART7_STATUS_BUSY           = 0x15,
    CART7_STATUS_ABORTED        = 0x16,
    
    /* Slot/Cart Errors (0x20-0x2F) */
    CART7_STATUS_NO_CART        = 0x20,
    CART7_STATUS_CART_REMOVED   = 0x21,
    CART7_STATUS_WRONG_SLOT     = 0x22,
    CART7_STATUS_UNSUPPORTED    = 0x23,
    CART7_STATUS_DETECT_FAILED  = 0x24,
    
    /* Read/Write Errors (0x30-0x3F) */
    CART7_STATUS_READ_ERROR     = 0x30,
    CART7_STATUS_WRITE_ERROR    = 0x31,
    CART7_STATUS_VERIFY_ERROR   = 0x32,
    CART7_STATUS_OUT_OF_RANGE   = 0x33,
    CART7_STATUS_PROTECTED      = 0x34,
    
} cart7_status_t;

/*============================================================================
 * COMMAND CODES
 *============================================================================*/

typedef enum {
    /*------------------------------------------------------------------------
     * GENERAL COMMANDS (0x00-0x0F)
     *----------------------------------------------------------------------*/
    CART7_CMD_PING              = 0x00,     /* Connection test */
    CART7_CMD_GET_INFO          = 0x01,     /* Get device info */
    CART7_CMD_GET_STATUS        = 0x02,     /* Get device status */
    CART7_CMD_SELECT_SLOT       = 0x03,     /* Select slot/system */
    CART7_CMD_GET_CART_STATUS   = 0x04,     /* Get cartridge status */
    CART7_CMD_ABORT             = 0x05,     /* Abort operation */
    CART7_CMD_RESET             = 0x06,     /* Reset device */
    CART7_CMD_SET_LED           = 0x07,     /* Set LED */
    CART7_CMD_GET_VOLTAGE       = 0x08,     /* Get slot voltage */
    CART7_CMD_SET_VOLTAGE       = 0x09,     /* Set slot voltage (3.3V/5V) */
    
    /*------------------------------------------------------------------------
     * NES / FAMICOM COMMANDS (0x10-0x1F)
     *----------------------------------------------------------------------*/
    CART7_CMD_NES_GET_HEADER    = 0x10,     /* Get iNES header info */
    CART7_CMD_NES_READ_PRG      = 0x11,     /* Read PRG-ROM */
    CART7_CMD_NES_READ_CHR      = 0x12,     /* Read CHR-ROM */
    CART7_CMD_NES_READ_SRAM     = 0x13,     /* Read battery SRAM */
    CART7_CMD_NES_WRITE_SRAM    = 0x14,     /* Write battery SRAM */
    CART7_CMD_NES_DETECT_MAPPER = 0x15,     /* Auto-detect mapper */
    CART7_CMD_NES_SET_MAPPER    = 0x16,     /* Set mapper manually */
    CART7_CMD_NES_GET_MIRRORING = 0x17,     /* Get mirroring mode */
    
    /*------------------------------------------------------------------------
     * SNES / SUPER FAMICOM COMMANDS (0x20-0x2F)
     *----------------------------------------------------------------------*/
    CART7_CMD_SNES_GET_HEADER   = 0x20,     /* Get ROM header */
    CART7_CMD_SNES_READ_ROM     = 0x21,     /* Read ROM */
    CART7_CMD_SNES_READ_SRAM    = 0x22,     /* Read SRAM */
    CART7_CMD_SNES_WRITE_SRAM   = 0x23,     /* Write SRAM */
    CART7_CMD_SNES_DETECT_TYPE  = 0x24,     /* Detect LoROM/HiROM/ExHiROM */
    CART7_CMD_SNES_SET_TYPE     = 0x25,     /* Set mapping type manually */
    CART7_CMD_SNES_GET_SPECIAL  = 0x26,     /* Get special chip info */
    CART7_CMD_SNES_READ_SPC     = 0x27,     /* Read SPC data (SA-1, etc.) */
    
    /*------------------------------------------------------------------------
     * N64 COMMANDS (0x30-0x3F)
     *----------------------------------------------------------------------*/
    CART7_CMD_N64_GET_HEADER    = 0x30,     /* Get ROM header */
    CART7_CMD_N64_READ_ROM      = 0x31,     /* Read ROM */
    CART7_CMD_N64_READ_SAVE     = 0x32,     /* Read save */
    CART7_CMD_N64_WRITE_SAVE    = 0x33,     /* Write save */
    CART7_CMD_N64_DETECT_SAVE   = 0x34,     /* Detect save type */
    CART7_CMD_N64_GET_CIC       = 0x35,     /* Get CIC type */
    CART7_CMD_N64_SET_CIC       = 0x36,     /* Set CIC type manually */
    CART7_CMD_N64_CALC_CRC      = 0x37,     /* Calculate ROM CRC */
    
    /*------------------------------------------------------------------------
     * MEGA DRIVE / GENESIS COMMANDS (0x40-0x4F)
     *----------------------------------------------------------------------*/
    CART7_CMD_MD_GET_HEADER     = 0x40,     /* Get ROM header */
    CART7_CMD_MD_READ_ROM       = 0x41,     /* Read ROM */
    CART7_CMD_MD_READ_SRAM      = 0x42,     /* Read SRAM */
    CART7_CMD_MD_WRITE_SRAM     = 0x43,     /* Write SRAM */
    CART7_CMD_MD_VERIFY_CHECKSUM = 0x44,    /* Verify checksum */
    CART7_CMD_MD_GET_REGION     = 0x45,     /* Get region code */
    CART7_CMD_MD_DETECT_MAPPER  = 0x46,     /* Detect mapper (SSF2, etc.) */
    CART7_CMD_MD_UNLOCK_SRAM    = 0x47,     /* Unlock SRAM access */
    
    /*------------------------------------------------------------------------
     * GBA COMMANDS (0x50-0x5F)
     *----------------------------------------------------------------------*/
    CART7_CMD_GBA_GET_HEADER    = 0x50,     /* Get ROM header */
    CART7_CMD_GBA_READ_ROM      = 0x51,     /* Read ROM */
    CART7_CMD_GBA_READ_SAVE     = 0x52,     /* Read save */
    CART7_CMD_GBA_WRITE_SAVE    = 0x53,     /* Write save */
    CART7_CMD_GBA_DETECT_SAVE   = 0x54,     /* Detect save type */
    CART7_CMD_GBA_READ_GPIO     = 0x55,     /* Read GPIO (RTC, etc.) */
    CART7_CMD_GBA_WRITE_GPIO    = 0x56,     /* Write GPIO */
    CART7_CMD_GBA_DETECT_GPIO   = 0x57,     /* Detect GPIO devices */
    
    /*------------------------------------------------------------------------
     * GB / GBC COMMANDS (0x60-0x6F)
     *----------------------------------------------------------------------*/
    CART7_CMD_GB_GET_HEADER     = 0x60,     /* Get ROM header */
    CART7_CMD_GB_READ_ROM       = 0x61,     /* Read ROM */
    CART7_CMD_GB_READ_SRAM      = 0x62,     /* Read SRAM */
    CART7_CMD_GB_WRITE_SRAM     = 0x63,     /* Write SRAM */
    CART7_CMD_GB_DETECT_MBC     = 0x64,     /* Detect MBC type */
    CART7_CMD_GB_SET_MBC        = 0x65,     /* Set MBC type manually */
    CART7_CMD_GB_READ_RTC       = 0x66,     /* Read RTC (MBC3) */
    CART7_CMD_GB_WRITE_RTC      = 0x67,     /* Write RTC (MBC3) */
    CART7_CMD_GB_GET_LOGO       = 0x68,     /* Get Nintendo logo */
    
    /*------------------------------------------------------------------------
     * FIRMWARE COMMANDS (0xF0-0xFF)
     *----------------------------------------------------------------------*/
    CART7_CMD_FW_VERSION        = 0xF0,
    CART7_CMD_FW_UPDATE_START   = 0xF1,
    CART7_CMD_FW_UPDATE_DATA    = 0xF2,
    CART7_CMD_FW_UPDATE_FINISH  = 0xF3,
    CART7_CMD_BOOTLOADER        = 0xFF,
    
} cart7_cmd_t;

/*============================================================================
 * ASYNC EVENTS
 *============================================================================*/

typedef enum {
    CART7_EVT_CART_INSERTED     = 0xE0,
    CART7_EVT_CART_REMOVED      = 0xE1,
    CART7_EVT_PROGRESS          = 0xE2,
    CART7_EVT_ERROR             = 0xE3,
    CART7_EVT_SLOT_CHANGED      = 0xE4,
} cart7_event_t;

/*============================================================================
 * FRAME STRUCTURES
 *============================================================================*/

typedef struct {
    uint8_t  sync;          /* 0xC7 */
    uint8_t  cmd;
    uint16_t length;
    /* payload[] */
    /* crc8 */
} cart7_cmd_header_t;

typedef struct {
    uint8_t  sync;          /* 0x7C */
    uint8_t  status;
    uint8_t  cmd;
    uint16_t length;
    /* data[] */
    /* crc8 */
} cart7_response_header_t;

/*============================================================================
 * DATA STRUCTURES - GENERAL
 *============================================================================*/

/**
 * @brief Device info response
 * Command: CART7_CMD_GET_INFO
 */
typedef struct {
    uint8_t  protocol_version;
    uint8_t  hw_revision;
    char     fw_version[16];
    char     serial[16];
    char     build_date[12];
    uint8_t  slot_count;            /* Number of slots */
    uint8_t  supported_systems;     /* Bitmask of supported systems */
    uint32_t features;
} cart7_device_info_t;

/* Supported systems bitmask */
#define CART7_SYS_NES               (1 << 0)
#define CART7_SYS_SNES              (1 << 1)
#define CART7_SYS_N64               (1 << 2)
#define CART7_SYS_MD                (1 << 3)
#define CART7_SYS_GBA               (1 << 4)
#define CART7_SYS_GB                (1 << 5)

/**
 * @brief Device status response
 * Command: CART7_CMD_GET_STATUS
 */
typedef struct {
    uint8_t  current_slot;
    uint8_t  cart_inserted;
    uint8_t  operation_active;
    uint8_t  reserved;
    uint16_t progress;              /* 0-1000 */
    uint32_t uptime_sec;
} cart7_device_status_t;

/**
 * @brief Select slot command
 * Command: CART7_CMD_SELECT_SLOT
 */
typedef struct {
    uint8_t  slot;                  /* cart7_slot_t */
    uint8_t  voltage;               /* 0=auto, 33=3.3V, 50=5V */
    uint8_t  flags;
    uint8_t  reserved;
} cart7_select_slot_cmd_t;

/**
 * @brief Cartridge status response
 * Command: CART7_CMD_GET_CART_STATUS
 */
typedef struct {
    uint8_t  inserted;
    uint8_t  slot_type;
    uint8_t  detected_system;       /* Auto-detected system */
    uint8_t  voltage_mv_high;       /* Voltage in mV (high byte) */
    uint8_t  voltage_mv_low;        /* Voltage in mV (low byte) */
    uint8_t  reserved[3];
} cart7_cart_status_t;

/*============================================================================
 * DATA STRUCTURES - NES / FAMICOM
 *============================================================================*/

/**
 * @brief NES header info response
 * Command: CART7_CMD_NES_GET_HEADER
 */
typedef struct {
    uint32_t prg_size;              /* PRG-ROM size in bytes */
    uint32_t chr_size;              /* CHR-ROM size in bytes (0 = CHR-RAM) */
    uint16_t mapper;                /* Mapper number */
    uint8_t  submapper;             /* Submapper number */
    uint8_t  mirroring;             /* 0=H, 1=V, 2=4-screen */
    uint8_t  has_battery;           /* Battery-backed SRAM */
    uint8_t  has_trainer;           /* 512-byte trainer */
    uint8_t  prg_ram_size;          /* PRG-RAM size (8KB units) */
    uint8_t  chr_ram_size;          /* CHR-RAM size (8KB units) */
    uint8_t  tv_system;             /* 0=NTSC, 1=PAL, 2=Dual */
    uint8_t  vs_system;             /* VS System flag */
    uint8_t  nes2_format;           /* NES 2.0 format detected */
    uint8_t  reserved;
} cart7_nes_header_t;

/* NES Mirroring modes */
#define NES_MIRROR_HORIZONTAL       0
#define NES_MIRROR_VERTICAL         1
#define NES_MIRROR_FOUR_SCREEN      2
#define NES_MIRROR_SINGLE_A         3
#define NES_MIRROR_SINGLE_B         4

/**
 * @brief NES read PRG/CHR command
 * Command: CART7_CMD_NES_READ_PRG, CART7_CMD_NES_READ_CHR
 */
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t chunk_size;            /* 0 = default */
    uint8_t  flags;
    uint8_t  reserved;
} cart7_nes_read_cmd_t;

/**
 * @brief NES mapper detect response
 * Command: CART7_CMD_NES_DETECT_MAPPER
 */
typedef struct {
    uint16_t mapper;
    uint8_t  submapper;
    uint8_t  confidence;            /* 0-100% */
    char     name[32];              /* Mapper name (e.g., "MMC3") */
} cart7_nes_mapper_info_t;

/*============================================================================
 * DATA STRUCTURES - SNES / SUPER FAMICOM
 *============================================================================*/

/**
 * @brief SNES ROM types
 */
typedef enum {
    SNES_TYPE_UNKNOWN   = 0,
    SNES_TYPE_LOROM     = 1,
    SNES_TYPE_HIROM     = 2,
    SNES_TYPE_EXLOROM   = 3,
    SNES_TYPE_EXHIROM   = 4,
    SNES_TYPE_SA1       = 5,
    SNES_TYPE_SDD1      = 6,
    SNES_TYPE_SPC7110   = 7,
} snes_rom_type_t;

/**
 * @brief SNES special chips
 */
typedef enum {
    SNES_CHIP_NONE      = 0,
    SNES_CHIP_DSP1      = 1,
    SNES_CHIP_DSP2      = 2,
    SNES_CHIP_DSP3      = 3,
    SNES_CHIP_DSP4      = 4,
    SNES_CHIP_GSU       = 5,        /* SuperFX */
    SNES_CHIP_OBC1      = 6,
    SNES_CHIP_SA1       = 7,
    SNES_CHIP_SDD1      = 8,
    SNES_CHIP_SPC7110   = 9,
    SNES_CHIP_ST010     = 10,
    SNES_CHIP_ST011     = 11,
    SNES_CHIP_ST018     = 12,
    SNES_CHIP_CX4       = 13,
} snes_chip_t;

/**
 * @brief SNES header info response
 * Command: CART7_CMD_SNES_GET_HEADER
 */
typedef struct {
    char     title[22];             /* Internal title */
    uint8_t  rom_type;              /* snes_rom_type_t */
    uint8_t  special_chip;          /* snes_chip_t */
    uint32_t rom_size;              /* ROM size in bytes */
    uint32_t sram_size;             /* SRAM size in bytes */
    uint8_t  country;               /* Country code */
    uint8_t  license;               /* License code */
    uint8_t  version;               /* ROM version */
    uint8_t  has_battery;
    uint16_t checksum;              /* Header checksum */
    uint16_t checksum_comp;         /* Checksum complement */
    uint8_t  fast_rom;              /* FastROM flag */
    uint8_t  reserved[3];
} cart7_snes_header_t;

/**
 * @brief SNES read ROM command
 * Command: CART7_CMD_SNES_READ_ROM
 */
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t chunk_size;
    uint8_t  rom_type;              /* Override auto-detect */
    uint8_t  reserved;
} cart7_snes_read_cmd_t;

/*============================================================================
 * DATA STRUCTURES - N64
 *============================================================================*/

/**
 * @brief N64 save types
 */
typedef enum {
    N64_SAVE_NONE       = 0,
    N64_SAVE_EEPROM_4K  = 1,        /* 512 bytes */
    N64_SAVE_EEPROM_16K = 2,        /* 2 KB */
    N64_SAVE_SRAM_256K  = 3,        /* 32 KB */
    N64_SAVE_FLASH_1M   = 4,        /* 128 KB */
    N64_SAVE_CPAK       = 5,        /* Controller Pak */
} n64_save_type_t;

/**
 * @brief N64 CIC types
 */
typedef enum {
    N64_CIC_UNKNOWN     = 0,
    N64_CIC_6101        = 1,        /* Star Fox 64 */
    N64_CIC_6102        = 2,        /* Most common */
    N64_CIC_6103        = 3,
    N64_CIC_6105        = 4,
    N64_CIC_6106        = 5,
    N64_CIC_7101        = 6,        /* NTSC-J */
    N64_CIC_7102        = 7,
    N64_CIC_8303        = 8,        /* 64DD */
} n64_cic_type_t;

/**
 * @brief N64 header info response
 * Command: CART7_CMD_N64_GET_HEADER
 */
typedef struct {
    uint8_t  pi_settings[4];        /* PI BSD Domain settings */
    uint32_t clock_rate;
    uint32_t boot_address;
    uint32_t release;
    uint32_t crc1;
    uint32_t crc2;
    uint8_t  reserved1[8];
    char     title[20];             /* Internal title */
    uint8_t  reserved2[7];
    char     game_code[4];          /* Game code */
    uint8_t  version;
    uint8_t  cic_type;              /* Detected CIC */
    uint8_t  save_type;             /* Detected save type */
    uint8_t  region;                /* N=NTSC, P=PAL, J=Japan */
    uint32_t rom_size;              /* ROM size in bytes */
} cart7_n64_header_t;

/**
 * @brief N64 read ROM command
 * Command: CART7_CMD_N64_READ_ROM
 */
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t chunk_size;
    uint8_t  byte_swap;             /* 0=native, 1=swap, 2=auto */
    uint8_t  reserved;
} cart7_n64_read_cmd_t;

/**
 * @brief N64 save type detect response
 * Command: CART7_CMD_N64_DETECT_SAVE
 */
typedef struct {
    uint8_t  save_type;             /* n64_save_type_t */
    uint8_t  confidence;
    uint16_t size;                  /* Size in bytes */
    char     name[16];              /* "EEPROM 4K", "SRAM", etc. */
} cart7_n64_save_info_t;

/*============================================================================
 * DATA STRUCTURES - MEGA DRIVE / GENESIS
 *============================================================================*/

/**
 * @brief Mega Drive header info response
 * Command: CART7_CMD_MD_GET_HEADER
 */
typedef struct {
    char     console[16];           /* "SEGA MEGA DRIVE" or "SEGA GENESIS" */
    char     copyright[16];         /* Copyright info */
    char     title_domestic[48];    /* Japanese title */
    char     title_overseas[48];    /* International title */
    char     serial[14];            /* Serial number */
    uint16_t checksum;              /* ROM checksum */
    char     io_support[16];        /* I/O support */
    uint32_t rom_start;
    uint32_t rom_end;
    uint32_t ram_start;
    uint32_t ram_end;
    uint8_t  sram_info[12];
    char     region[3];             /* Region codes (J/U/E) */
    uint8_t  reserved;
    uint32_t rom_size;              /* Calculated ROM size */
    uint32_t sram_size;             /* SRAM size */
    uint8_t  has_sram;
    uint8_t  sram_type;             /* 0=none, 1=SRAM, 2=EEPROM */
    uint8_t  mapper_type;           /* 0=none, 1=SSF2, 2=Sega, etc. */
    uint8_t  extra_features;
} cart7_md_header_t;

/**
 * @brief Mega Drive read ROM command
 * Command: CART7_CMD_MD_READ_ROM
 */
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t chunk_size;
    uint8_t  word_swap;             /* 0=normal, 1=byte-swap */
    uint8_t  reserved;
} cart7_md_read_cmd_t;

/*============================================================================
 * DATA STRUCTURES - GBA
 *============================================================================*/

/**
 * @brief GBA save types
 */
typedef enum {
    GBA_SAVE_NONE           = 0,
    GBA_SAVE_EEPROM_512     = 1,    /* 512 bytes */
    GBA_SAVE_EEPROM_8K      = 2,    /* 8 KB */
    GBA_SAVE_SRAM_32K       = 3,    /* 32 KB */
    GBA_SAVE_FLASH_64K      = 4,    /* 64 KB */
    GBA_SAVE_FLASH_128K     = 5,    /* 128 KB */
} gba_save_type_t;

/**
 * @brief GBA GPIO types
 */
typedef enum {
    GBA_GPIO_NONE           = 0,
    GBA_GPIO_RTC            = 1,    /* Real-Time Clock */
    GBA_GPIO_SOLAR          = 2,    /* Solar sensor (Boktai) */
    GBA_GPIO_GYRO           = 3,    /* Gyroscope (Wario Ware) */
    GBA_GPIO_RUMBLE         = 4,    /* Rumble (Drill Dozer) */
} gba_gpio_type_t;

/**
 * @brief GBA header info response
 * Command: CART7_CMD_GBA_GET_HEADER
 */
typedef struct {
    uint32_t entry_point;
    uint8_t  logo[156];             /* Nintendo logo (compressed in response) */
    char     title[12];
    char     game_code[4];
    char     maker_code[2];
    uint8_t  fixed;                 /* Should be 0x96 */
    uint8_t  unit_code;
    uint8_t  device_type;
    uint8_t  reserved1[7];
    uint8_t  version;
    uint8_t  checksum;
    uint8_t  reserved2[2];
    /* Extended info (detected) */
    uint32_t rom_size;
    uint8_t  save_type;             /* gba_save_type_t */
    uint8_t  gpio_type;             /* gba_gpio_type_t */
    uint8_t  logo_valid;            /* Nintendo logo valid */
    uint8_t  header_checksum_valid;
} cart7_gba_header_t;

/**
 * @brief GBA read ROM command
 * Command: CART7_CMD_GBA_READ_ROM
 */
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t chunk_size;
    uint8_t  flags;
    uint8_t  reserved;
} cart7_gba_read_cmd_t;

/**
 * @brief GBA RTC data
 * Command: CART7_CMD_GBA_READ_GPIO (for RTC)
 */
typedef struct {
    uint8_t  year;                  /* 0-99 */
    uint8_t  month;                 /* 1-12 */
    uint8_t  day;                   /* 1-31 */
    uint8_t  weekday;               /* 0-6 */
    uint8_t  hour;                  /* 0-23 */
    uint8_t  minute;                /* 0-59 */
    uint8_t  second;                /* 0-59 */
    uint8_t  status;
} cart7_gba_rtc_t;

/*============================================================================
 * DATA STRUCTURES - GB / GBC
 *============================================================================*/

/**
 * @brief Game Boy MBC types
 */
typedef enum {
    GB_MBC_NONE         = 0x00,
    GB_MBC_MBC1         = 0x01,
    GB_MBC_MBC1_RAM     = 0x02,
    GB_MBC_MBC1_RAM_BAT = 0x03,
    GB_MBC_MBC2         = 0x05,
    GB_MBC_MBC2_BAT     = 0x06,
    GB_MBC_ROM_RAM      = 0x08,
    GB_MBC_ROM_RAM_BAT  = 0x09,
    GB_MBC_MMM01        = 0x0B,
    GB_MBC_MBC3_RTC_BAT = 0x0F,
    GB_MBC_MBC3_RTC_RAM_BAT = 0x10,
    GB_MBC_MBC3         = 0x11,
    GB_MBC_MBC3_RAM     = 0x12,
    GB_MBC_MBC3_RAM_BAT = 0x13,
    GB_MBC_MBC5         = 0x19,
    GB_MBC_MBC5_RAM     = 0x1A,
    GB_MBC_MBC5_RAM_BAT = 0x1B,
    GB_MBC_MBC5_RUMBLE  = 0x1C,
    GB_MBC_MBC5_RUMBLE_RAM = 0x1D,
    GB_MBC_MBC5_RUMBLE_RAM_BAT = 0x1E,
    GB_MBC_MBC6         = 0x20,
    GB_MBC_MBC7         = 0x22,
    GB_MBC_CAMERA       = 0xFC,
    GB_MBC_TAMA5        = 0xFD,
    GB_MBC_HUC3         = 0xFE,
    GB_MBC_HUC1         = 0xFF,
} gb_mbc_type_t;

/**
 * @brief Game Boy header info response
 * Command: CART7_CMD_GB_GET_HEADER
 */
typedef struct {
    uint8_t  entry[4];              /* Entry point */
    uint8_t  logo[48];              /* Nintendo logo */
    char     title[16];             /* Title (11 for CGB, 15 for DMG) */
    char     manufacturer[4];
    uint8_t  cgb_flag;              /* CGB compatibility */
    char     licensee[2];
    uint8_t  sgb_flag;              /* SGB compatibility */
    uint8_t  cart_type;             /* MBC type */
    uint8_t  rom_size_code;
    uint8_t  ram_size_code;
    uint8_t  destination;           /* 0=Japan, 1=Other */
    uint8_t  old_licensee;
    uint8_t  version;
    uint8_t  header_checksum;
    uint16_t global_checksum;
    /* Extended info (detected) */
    uint32_t rom_size;              /* Actual ROM size */
    uint32_t ram_size;              /* Actual RAM size */
    uint8_t  mbc_type;              /* Detected MBC */
    uint8_t  has_battery;
    uint8_t  has_rtc;
    uint8_t  has_rumble;
    uint8_t  is_gbc;                /* GBC-only or enhanced */
    uint8_t  logo_valid;
    uint8_t  header_checksum_valid;
    uint8_t  reserved;
} cart7_gb_header_t;

/**
 * @brief Game Boy read ROM command
 * Command: CART7_CMD_GB_READ_ROM
 */
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint16_t chunk_size;
    uint8_t  mbc_override;          /* 0=auto, else MBC type */
    uint8_t  reserved;
} cart7_gb_read_cmd_t;

/**
 * @brief Game Boy RTC data (MBC3)
 * Command: CART7_CMD_GB_READ_RTC
 */
typedef struct {
    uint8_t  seconds;               /* 0-59 */
    uint8_t  minutes;               /* 0-59 */
    uint8_t  hours;                 /* 0-23 */
    uint8_t  days_low;              /* Low 8 bits of day counter */
    uint8_t  days_high;             /* Bit 0 = day counter high bit, Bit 6 = halt, Bit 7 = day overflow */
    uint8_t  latched_seconds;       /* Latched values */
    uint8_t  latched_minutes;
    uint8_t  latched_hours;
    uint8_t  latched_days_low;
    uint8_t  latched_days_high;
} cart7_gb_rtc_t;

/*============================================================================
 * PROGRESS EVENT
 *============================================================================*/

typedef struct {
    uint8_t  operation;
    uint8_t  slot;
    uint16_t progress;              /* 0-1000 */
    uint32_t bytes_done;
    uint32_t bytes_total;
    uint16_t speed_kbps;
    uint16_t eta_sec;
} cart7_progress_event_t;

/*============================================================================
 * CRC8 CALCULATION
 *============================================================================*/

static inline uint8_t cart7_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
        }
    }
    return crc;
}

/*============================================================================
 * HELPER MACROS
 *============================================================================*/

#define CART7_IS_OK(s)          ((s) <= CART7_STATUS_OK_DONE)
#define CART7_IS_ERROR(s)       ((s) >= CART7_STATUS_ERROR)

/* ROM size lookup tables */
static inline uint32_t gb_rom_size(uint8_t code) {
    return (code <= 8) ? (32768 << code) : 0;
}

static inline uint32_t gb_ram_size(uint8_t code) {
    static const uint32_t sizes[] = {0, 2048, 8192, 32768, 131072, 65536};
    return (code < 6) ? sizes[code] : 0;
}

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* CART7_PROTOCOL_H */
