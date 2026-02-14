/**
 * @file cart7_3ds_protocol.h
 * @brief Nintendo 3DS Cartridge Protocol for Cart8 (8-in-1 Reader)
 * 
 * Extends Cart7 with Nintendo 3DS support.
 * 
 * 3DS Cartridge Specifications:
 *   - 17-pin edge connector
 *   - SPI-like protocol
 *   - 1.8V voltage (CRITICAL: different from other systems!)
 *   - AES-128-CTR encryption
 *   - Capacity: 128MB - 8GB
 * 
 * Formats: NCSD, NCCH, ExeFS, RomFS, SMDH
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#ifndef CART7_3DS_PROTOCOL_H
#define CART7_3DS_PROTOCOL_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cart7_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Slot ID from cart7_protocol.h enum (SLOT_3DS = 0x09) */
#define CART7_3DS_VOLTAGE           18      /* 1.8V (x10) */

/* Media Unit */
#define CTR_MEDIA_UNIT_SIZE         0x200   /* 512 bytes */

/* Header sizes */
#define CTR_NCSD_HEADER_SIZE        0x200
#define CTR_NCCH_HEADER_SIZE        0x200
#define CTR_EXEFS_HEADER_SIZE       0x200
#define CTR_SMDH_SIZE               0x36C0

/* Magic values */
#define CTR_NCSD_MAGIC              "NCSD"
#define CTR_NCCH_MAGIC              "NCCH"
#define CTR_SMDH_MAGIC              "SMDH"

/* 3DS Commands (0x70-0x7F) */
#define CART7_CMD_3DS_GET_HEADER    0x70
#define CART7_CMD_3DS_GET_NCCH      0x71
#define CART7_CMD_3DS_READ_ROM      0x72
#define CART7_CMD_3DS_READ_NCCH     0x73
#define CART7_CMD_3DS_READ_EXEFS    0x74
#define CART7_CMD_3DS_READ_ROMFS    0x75
#define CART7_CMD_3DS_GET_UNIQUE_ID 0x76
#define CART7_CMD_3DS_GET_CARD_ID   0x77
#define CART7_CMD_3DS_INIT_CARD     0x78
#define CART7_CMD_3DS_READ_SAVE     0x79
#define CART7_CMD_3DS_WRITE_SAVE    0x7A
#define CART7_CMD_3DS_DETECT_SAVE   0x7B
#define CART7_CMD_3DS_GET_SMDH      0x7C

/* Card sizes */
typedef enum {
    CTR_CARD_SIZE_128MB = 0x00,
    CTR_CARD_SIZE_256MB = 0x01,
    CTR_CARD_SIZE_512MB = 0x02,
    CTR_CARD_SIZE_1GB   = 0x03,
    CTR_CARD_SIZE_2GB   = 0x04,
    CTR_CARD_SIZE_4GB   = 0x05,
    CTR_CARD_SIZE_8GB   = 0x06,
} ctr_card_size_t;

/* Save types */
typedef enum {
    CTR_SAVE_NONE       = 0,
    CTR_SAVE_EEPROM_4K  = 1,
    CTR_SAVE_EEPROM_64K = 2,
    CTR_SAVE_EEPROM_512K= 3,
    CTR_SAVE_FLASH_512K = 5,
    CTR_SAVE_FLASH_1M   = 6,
    CTR_SAVE_FLASH_2M   = 7,
    CTR_SAVE_FLASH_4M   = 8,
    CTR_SAVE_FLASH_8M   = 9,
} ctr_save_type_t;

/* Partition indices */
#define CTR_PARTITION_GAME          0
#define CTR_PARTITION_MANUAL        1
#define CTR_PARTITION_DLP_CHILD     2
#define CTR_PARTITION_N3DS_UPDATE   6
#define CTR_PARTITION_O3DS_UPDATE   7

/* NCSD Header */
typedef struct {
    uint8_t  signature[0x100];
    uint8_t  magic[4];
    uint32_t size;
    uint8_t  media_id[8];
    uint8_t  partition_fs_type[8];
    uint8_t  partition_crypt_type[8];
    struct {
        uint32_t offset;
        uint32_t size;
    } partitions[8];
    uint8_t  exheader_hash[0x20];
    uint32_t additional_header_size;
    uint32_t sector_zero_offset;
    uint8_t  partition_flags[8];
    uint8_t  partition_id_table[8][8];
    uint8_t  reserved[0x30];
} ctr_ncsd_header_t;

/* NCCH Header */
typedef struct {
    uint8_t  signature[0x100];
    uint8_t  magic[4];
    uint32_t content_size;
    uint8_t  partition_id[8];
    uint8_t  maker_code[2];
    uint16_t version;
    uint32_t seed_hash_check;
    uint8_t  program_id[8];
    uint8_t  reserved1[0x10];
    uint8_t  logo_region_hash[0x20];
    uint8_t  product_code[0x10];
    uint8_t  exheader_hash[0x20];
    uint32_t exheader_size;
    uint32_t reserved2;
    uint8_t  flags[8];
    uint32_t plain_region_offset;
    uint32_t plain_region_size;
    uint32_t logo_region_offset;
    uint32_t logo_region_size;
    uint32_t exefs_offset;
    uint32_t exefs_size;
    uint32_t exefs_hash_size;
    uint32_t reserved3;
    uint32_t romfs_offset;
    uint32_t romfs_size;
    uint32_t romfs_hash_size;
    uint32_t reserved4;
    uint8_t  exefs_hash[0x20];
    uint8_t  romfs_hash[0x20];
} ctr_ncch_header_t;

/* ExeFS Header */
typedef struct {
    struct {
        char     name[8];
        uint32_t offset;
        uint32_t size;
    } files[10];
    uint8_t reserved[0x20];
    uint8_t file_hashes[10][0x20];
} ctr_exefs_header_t;

/* SMDH Title */
typedef struct {
    uint16_t short_desc[0x40];
    uint16_t long_desc[0x80];
    uint16_t publisher[0x40];
} ctr_smdh_title_t;

/* SMDH Header */
typedef struct {
    uint8_t  magic[4];
    uint16_t version;
    uint16_t reserved1;
    ctr_smdh_title_t titles[16];
    uint8_t  ratings[0x10];
    uint32_t region_lockout;
    uint8_t  matchmaker_id[0xC];
    uint32_t flags;
    uint16_t eula_version;
    uint16_t reserved2;
    uint32_t optimal_animation_frame;
    uint32_t cec_id;
    uint64_t reserved3;
    uint8_t  small_icon[0x480];
    uint8_t  large_icon[0x1200];
} ctr_smdh_t;

/* 3DS Cart Info */
typedef struct {
    char     product_code[17];
    uint8_t  maker_code[2];
    uint32_t card_size;
    uint8_t  crypto_type;
    char     title_short[65];
    char     title_long[129];
    char     publisher[65];
    uint8_t  partition_count;
    struct {
        uint32_t offset;
        uint32_t size;
        uint8_t  type;
        bool     encrypted;
    } partitions[8];
    ctr_save_type_t save_type;
    uint32_t save_size;
    bool     is_new3ds_exclusive;
    bool     has_manual;
    bool     has_dlp_child;
} cart7_3ds_info_t;

/* Utility functions */
static inline uint32_t ctr_media_to_bytes(uint32_t m) { return m * CTR_MEDIA_UNIT_SIZE; }
static inline bool ctr_ncsd_is_valid(const ctr_ncsd_header_t *h) { return h && memcmp(h->magic, CTR_NCSD_MAGIC, 4) == 0; }
static inline bool ctr_ncch_is_valid(const ctr_ncch_header_t *h) { return h && memcmp(h->magic, CTR_NCCH_MAGIC, 4) == 0; }

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* CART7_3DS_PROTOCOL_H */
