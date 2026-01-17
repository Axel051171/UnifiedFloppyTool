/**
 * @file uft_xci.h
 * @brief Nintendo Switch XCI (Gamecard) format definitions
 * @version 1.0.0
 * 
 * Extracted and adapted from nxdumptool (GPL-3.0)
 * Original: Copyright (c) 2020-2024, DarkMatterCore <pabloacurielz@gmail.com>
 * 
 * XCI Structure:
 *   0x0000 - 0x0FFF: Key Area (4 KiB)
 *   0x1000 - 0x11FF: XCI Header (512 bytes)
 *   0x1200+:         Root HFS0 partition
 * 
 * For use in UFI (Universal Flux Interface) / UFT (Universal Flux Tool)
 */

#ifndef UFT_XCI_H
#define UFT_XCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define XCI_HEAD_MAGIC              0x48454144  /* "HEAD" */
#define XCI_PAGE_SIZE               0x200       /* 512 bytes */
#define XCI_PAGE_OFFSET(x)          ((uint64_t)(x) * XCI_PAGE_SIZE)

#define XCI_KEY_AREA_OFFSET         0x0000      /* KeyArea at start of XCI */
#define XCI_KEY_AREA_SIZE           0x1000      /* 4 KiB */
#define XCI_HEADER_OFFSET           0x1000      /* Header after KeyArea */
#define XCI_HEADER_SIZE             0x200       /* 512 bytes */
#define XCI_CERT_OFFSET             0x7000      /* Certificate location */

#define AES_128_KEY_SIZE            16
#define SHA256_HASH_SIZE            32

/* System Update Title ID */
#define XCI_UPDATE_TID              0x0100000000000816ULL

/*============================================================================
 * ROM Size Enumeration
 *============================================================================*/

typedef enum {
    XCI_ROM_SIZE_1GiB  = 0xFA,
    XCI_ROM_SIZE_2GiB  = 0xF8,
    XCI_ROM_SIZE_4GiB  = 0xF0,
    XCI_ROM_SIZE_8GiB  = 0xE0,
    XCI_ROM_SIZE_16GiB = 0xE1,
    XCI_ROM_SIZE_32GiB = 0xE2
} xci_rom_size_t;

/*============================================================================
 * Gamecard Flags
 *============================================================================*/

typedef enum {
    XCI_FLAG_NONE                  = 0,
    XCI_FLAG_AUTO_BOOT             = (1 << 0),  /* Autoboot capable */
    XCI_FLAG_HISTORY_ERASE         = (1 << 1),  /* No HOME menu icon */
    XCI_FLAG_REPAIR_TOOL           = (1 << 2),
    XCI_FLAG_DIFF_REGION_CUP_TERRA = (1 << 3),
    XCI_FLAG_DIFF_REGION_CUP_GLOB  = (1 << 4),
    XCI_FLAG_CARD_HEADER_SIGN_KEY  = (1 << 7),
} xci_flags_t;

/*============================================================================
 * Gamecard Version
 *============================================================================*/

typedef enum {
    XCI_VERSION_DEFAULT      = 0,
    XCI_VERSION_UNKNOWN1     = 1,
    XCI_VERSION_UNKNOWN2     = 2,
    XCI_VERSION_T2_SUPPORTED = 3,  /* T2 security scheme */
} xci_version_t;

/*============================================================================
 * Security Selection
 *============================================================================*/

typedef enum {
    XCI_SEL_SEC_T1 = 1,     /* T1 security */
    XCI_SEL_SEC_T2 = 2,     /* T2 security */
} xci_sel_sec_t;

/*============================================================================
 * Key Index (packed byte)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t kek_index        : 4;
    uint8_t titlekey_dec_idx : 4;
} xci_key_index_t;

/*============================================================================
 * Firmware Version (for CardInfo)
 *============================================================================*/

typedef enum {
    XCI_FW_VERSION_DEV        = 0,
    XCI_FW_VERSION_SINCE_1_0  = 1,  /* HOS 1.0.0+ */
    XCI_FW_VERSION_SINCE_4_0  = 2,  /* HOS 4.0.0+ */
    XCI_FW_VERSION_SINCE_9_0  = 3,  /* HOS 9.0.0+ */
    XCI_FW_VERSION_SINCE_11_0 = 4,  /* HOS 11.0.0+ */
    XCI_FW_VERSION_SINCE_12_0 = 5,  /* HOS 12.0.0+ */
} xci_fw_version_t;

/*============================================================================
 * Access Control
 *============================================================================*/

typedef enum {
    XCI_ACC_CTRL_25MHZ = 0xA10011,  /* Standard speed */
    XCI_ACC_CTRL_50MHZ = 0xA10010,  /* High speed (8GB+) */
} xci_acc_ctrl_t;

/*============================================================================
 * Compatibility Type
 *============================================================================*/

typedef enum {
    XCI_COMPAT_NORMAL = 0,
    XCI_COMPAT_TERRA  = 1,  /* China region */
} xci_compat_type_t;

/*============================================================================
 * Version Structure (4 bytes)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t micro;
    uint8_t minor;
    uint8_t major;
    uint8_t revision;
} xci_version_info_t;

/*============================================================================
 * Card Info (encrypted with XCI header key) - 0x70 bytes
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint64_t fw_version;            /* FW version enum or UINT64_MAX for T2 */
    uint32_t acc_ctrl_1;            /* Access control */
    uint32_t wait_1_time_read;      /* Always 0x1388 */
    uint32_t wait_2_time_read;      /* Always 0 */
    uint32_t wait_1_time_write;     /* Always 0 */
    uint32_t wait_2_time_write;     /* Always 0 */
    xci_version_info_t fw_mode;     /* SDK version */
    xci_version_info_t upp_version; /* Bundled system update version */
    uint8_t compatibility_type;
    uint8_t reserved_1[0x3];
    uint64_t upp_hash;              /* Update partition checksum */
    uint64_t upp_id;                /* Must be system update TID */
    uint8_t reserved_2[0x38];
} xci_card_info_t;

/*============================================================================
 * XCI Header (at offset 0x1000, size 0x200)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    /* 0x000 */ uint8_t signature[0x100];       /* RSA-2048-PKCS#1 v1.5 with SHA-256 */
    /* 0x100 */ uint32_t magic;                  /* "HEAD" (0x48454144) */
    /* 0x104 */ uint32_t rom_area_start_page;    /* In XCI_PAGE_SIZE units */
    /* 0x108 */ uint32_t backup_area_start_page; /* Always 0xFFFFFFFF */
    /* 0x10C */ xci_key_index_t key_index;
    /* 0x10D */ uint8_t rom_size;                /* xci_rom_size_t */
    /* 0x10E */ uint8_t version;                 /* xci_version_t */
    /* 0x10F */ uint8_t flags;                   /* xci_flags_t */
    /* 0x110 */ uint8_t package_id[0x8];         /* Challenge-response auth */
    /* 0x118 */ uint32_t valid_data_end_page;    /* In XCI_PAGE_SIZE units */
    /* 0x11C */ uint8_t reserved_1[0x4];
    /* 0x120 */ uint8_t card_info_iv[AES_128_KEY_SIZE]; /* Reversed for AES-CBC */
    /* 0x130 */ uint64_t hfs_header_offset;      /* Root HFS0 offset */
    /* 0x138 */ uint64_t hfs_header_size;        /* Root HFS0 size */
    /* 0x140 */ uint8_t hfs_header_hash[SHA256_HASH_SIZE];
    /* 0x160 */ uint8_t initial_data_hash[SHA256_HASH_SIZE];
    /* 0x180 */ uint32_t sel_sec;                /* Security selection (1=T1, 2=T2) */
    /* 0x184 */ uint32_t sel_t1_key;             /* T1: 0x02, T2: 0x00 */
    /* 0x188 */ uint32_t sel_key;                /* Always 0x00 */
    /* 0x18C */ uint32_t lim_area_page;          /* In XCI_PAGE_SIZE units */
    /* 0x190 */ xci_card_info_t card_info;       /* Encrypted area */
} xci_header_t;

/*============================================================================
 * Key Area Structures (precedes header)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    union {
        uint8_t value[0x10];
        struct {
            uint8_t package_id[0x8];  /* Matches header package_id */
            uint8_t reserved[0x8];    /* Zeroes */
        };
    };
} xci_key_source_t;

typedef struct __attribute__((packed)) {
    xci_key_source_t key_source;
    uint8_t encrypted_titlekey[0x10];  /* AES-128-CCM encrypted */
    uint8_t mac[0x10];                 /* Titlekey MAC */
    uint8_t nonce[0xC];                /* AES-128-CCM IV */
    uint8_t reserved[0x1C4];
} xci_initial_data_t;

typedef struct __attribute__((packed)) {
    uint8_t titlekey[0x10];            /* Decrypted titlekey (zeroes in retail) */
    uint8_t reserved[0xCF0];
} xci_titlekey_area_t;

typedef struct __attribute__((packed)) {
    uint8_t titlekey_enc_key[0x10];    /* AES-128-CTR key */
    uint8_t titlekey_enc_iv[0x10];     /* AES-128-CTR IV */
    uint8_t reserved[0xE0];
} xci_titlekey_area_enc_t;

typedef struct __attribute__((packed)) {
    xci_initial_data_t initial_data;           /* 0x000-0x1FF */
    xci_titlekey_area_t titlekey_area;         /* 0x200-0xEFF */
    xci_titlekey_area_enc_t titlekey_area_enc; /* 0xF00-0xFFF */
} xci_key_area_t;

/*============================================================================
 * XCI Context (runtime structure)
 *============================================================================*/

typedef struct {
    xci_header_t header;
    xci_key_area_t key_area;
    bool header_valid;
    bool is_trimmed;
    bool is_t2;
    uint64_t total_size;
    uint64_t trimmed_size;
    uint64_t rom_capacity;
    char title_id_str[20];
} xci_context_t;

/*============================================================================
 * Static Assertions
 *============================================================================*/

_Static_assert(sizeof(xci_key_index_t) == 1, "xci_key_index_t size mismatch");
_Static_assert(sizeof(xci_version_info_t) == 4, "xci_version_info_t size mismatch");
_Static_assert(sizeof(xci_card_info_t) == 0x70, "xci_card_info_t size mismatch");
_Static_assert(sizeof(xci_header_t) == 0x200, "xci_header_t size mismatch");
_Static_assert(sizeof(xci_key_source_t) == 0x10, "xci_key_source_t size mismatch");
_Static_assert(sizeof(xci_initial_data_t) == 0x200, "xci_initial_data_t size mismatch");
_Static_assert(sizeof(xci_titlekey_area_t) == 0xD00, "xci_titlekey_area_t size mismatch");
_Static_assert(sizeof(xci_titlekey_area_enc_t) == 0x100, "xci_titlekey_area_enc_t size mismatch");
_Static_assert(sizeof(xci_key_area_t) == 0x1000, "xci_key_area_t size mismatch");

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Convert ROM size enum to bytes
 */
uint64_t xci_rom_size_to_bytes(xci_rom_size_t size);

/**
 * Get ROM size as string
 */
const char* xci_rom_size_str(xci_rom_size_t size);

/**
 * Initialize XCI context from file
 * @param ctx Context to initialize
 * @param data Raw XCI data (at least header + key area)
 * @param data_size Size of data
 * @return true on success
 */
bool xci_init_context(xci_context_t *ctx, const uint8_t *data, size_t data_size);

/**
 * Initialize XCI context from file pointer
 * @param ctx Context to initialize
 * @param fp File pointer positioned at start
 * @return true on success
 */
bool xci_init_from_file(xci_context_t *ctx, void *fp);

/**
 * Validate XCI header
 */
bool xci_header_valid(const xci_header_t *hdr);

/**
 * Get total XCI size from header
 */
uint64_t xci_get_total_size(const xci_header_t *hdr);

/**
 * Get ROM capacity from header
 */
uint64_t xci_get_rom_capacity(const xci_header_t *hdr);

/**
 * Check if XCI uses T2 security scheme
 */
bool xci_is_t2(const xci_header_t *hdr);

/**
 * Get XCI flags as string
 */
const char* xci_flags_str(uint8_t flags, char *buf, size_t buf_size);

/**
 * Get firmware version string
 */
const char* xci_fw_version_str(xci_fw_version_t version);

/**
 * Format version info as string
 */
void xci_format_version(const xci_version_info_t *ver, char *buf, size_t buf_size);

/**
 * Print XCI info to stdout
 */
void xci_print_info(const xci_context_t *ctx);

/**
 * Dump XCI header to file
 */
bool xci_dump_header(const xci_context_t *ctx, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XCI_H */
