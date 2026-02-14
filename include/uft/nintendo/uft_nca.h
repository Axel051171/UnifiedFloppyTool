/**
 * @file uft_nca.h
 * @brief Nintendo Content Archive (NCA) format definitions
 * @version 1.0.0
 * 
 * Extracted and adapted from nxdumptool (GPL-3.0)
 * Original: Copyright (c) 2020-2024, DarkMatterCore <pabloacurielz@gmail.com>
 * 
 * NCA is the container format for game content:
 *   - Program NCA: Main executable
 *   - Meta NCA: Title metadata (CNMT)
 *   - Control NCA: Icons, names
 *   - Manual NCA: Digital manual
 *   - Data NCA: Game data
 * 
 * For use in UFI (Universal Flux Interface) / UFT (Universal Flux Tool)
 */

#ifndef UFT_NCA_H
#define UFT_NCA_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define NCA_NCA0_MAGIC      0x4E434130  /* "NCA0" - oldest format */
#define NCA_NCA2_MAGIC      0x4E434132  /* "NCA2" - intermediate */
#define NCA_NCA3_MAGIC      0x4E434133  /* "NCA3" - current format */

#define NCA_HEADER_SIZE     0x400       /* 1024 bytes */
#define NCA_FS_HEADER_SIZE  0x200       /* 512 bytes per FS section */
#define NCA_FS_HEADER_COUNT 4           /* Max 4 FS sections */
#define NCA_FULL_HEADER_SIZE (NCA_HEADER_SIZE + (NCA_FS_HEADER_SIZE * NCA_FS_HEADER_COUNT))

#define NCA_SECTOR_SIZE     0x200       /* 512 bytes */
#define NCA_SECTOR_OFFSET(x) ((uint64_t)(x) * NCA_SECTOR_SIZE)

#define AES_128_KEY_SIZE    16
#define SHA256_HASH_SIZE    32

#define NCA_KEY_AREA_KEY_COUNT  0x10
#define NCA_KEY_AREA_SIZE       (NCA_KEY_AREA_KEY_COUNT * AES_128_KEY_SIZE)

/* Content ID string length (hex) */
#define NCA_CONTENT_ID_LEN  32

/*============================================================================
 * Distribution Type
 *============================================================================*/

typedef enum {
    NCA_DIST_DOWNLOAD  = 0,     /* eShop download */
    NCA_DIST_GAMECARD  = 1,     /* Physical cartridge */
    NCA_DIST_COUNT     = 2
} nca_dist_type_t;

/*============================================================================
 * Content Type
 *============================================================================*/

typedef enum {
    NCA_CONTENT_PROGRAM     = 0,    /* Main game executable */
    NCA_CONTENT_META        = 1,    /* Title metadata (CNMT) */
    NCA_CONTENT_CONTROL     = 2,    /* Icons, descriptions */
    NCA_CONTENT_MANUAL      = 3,    /* Digital manual */
    NCA_CONTENT_DATA        = 4,    /* Additional data */
    NCA_CONTENT_PUBLIC_DATA = 5,    /* Public data */
    NCA_CONTENT_COUNT       = 6
} nca_content_type_t;

/*============================================================================
 * Key Area Encryption Key Index
 *============================================================================*/

typedef enum {
    NCA_KAEK_APPLICATION    = 0,
    NCA_KAEK_OCEAN          = 1,
    NCA_KAEK_SYSTEM         = 2,
    NCA_KAEK_COUNT          = 3
} nca_kaek_index_t;

/*============================================================================
 * Key Generation (crypto revision)
 *============================================================================*/

typedef enum {
    NCA_KEYGEN_100  = 0,        /* HOS 1.0.0 */
    NCA_KEYGEN_UNUSED = 1,
    NCA_KEYGEN_300  = 2,        /* HOS 3.0.0 */
    NCA_KEYGEN_301  = 3,        /* HOS 3.0.1+ */
    NCA_KEYGEN_400  = 4,        /* HOS 4.0.0+ */
    NCA_KEYGEN_500  = 5,        /* HOS 5.0.0+ */
    NCA_KEYGEN_600  = 6,        /* HOS 6.0.0+ */
    NCA_KEYGEN_620  = 7,        /* HOS 6.2.0+ */
    NCA_KEYGEN_700  = 8,        /* HOS 7.0.0+ */
    NCA_KEYGEN_810  = 9,        /* HOS 8.1.0+ */
    NCA_KEYGEN_900  = 10,       /* HOS 9.0.0+ */
    NCA_KEYGEN_910  = 11,       /* HOS 9.1.0+ */
    NCA_KEYGEN_1210 = 12,       /* HOS 12.1.0+ */
    NCA_KEYGEN_1300 = 13,       /* HOS 13.0.0+ */
    NCA_KEYGEN_1400 = 14,       /* HOS 14.0.0+ */
    NCA_KEYGEN_1500 = 15,       /* HOS 15.0.0+ */
    NCA_KEYGEN_1600 = 16,       /* HOS 16.0.0+ */
    NCA_KEYGEN_1700 = 17,       /* HOS 17.0.0+ */
    NCA_KEYGEN_1800 = 18,       /* HOS 18.0.0+ */
    NCA_KEYGEN_1900 = 19,       /* HOS 19.0.0+ */
    NCA_KEYGEN_2000 = 20,       /* HOS 20.0.0+ */
    NCA_KEYGEN_COUNT = 21
} nca_key_generation_t;

/*============================================================================
 * FS Type
 *============================================================================*/

typedef enum {
    NCA_FS_TYPE_ROMFS       = 0,    /* Read-only filesystem */
    NCA_FS_TYPE_PARTITIONFS = 1,    /* PFS0 partition */
    NCA_FS_TYPE_COUNT       = 2
} nca_fs_type_t;

/*============================================================================
 * Hash Type
 *============================================================================*/

typedef enum {
    NCA_HASH_AUTO                = 0,
    NCA_HASH_NONE                = 1,
    NCA_HASH_HIERARCHICAL_SHA256 = 2,   /* For PFS0 */
    NCA_HASH_HIERARCHICAL_IVFC   = 3,   /* For RomFS */
    NCA_HASH_AUTO_SHA3           = 4,
    NCA_HASH_HIERARCHICAL_SHA3   = 5,
    NCA_HASH_HIERARCHICAL_IVFC3  = 6,
    NCA_HASH_COUNT               = 7
} nca_hash_type_t;

/*============================================================================
 * Encryption Type
 *============================================================================*/

typedef enum {
    NCA_ENC_AUTO                    = 0,
    NCA_ENC_NONE                    = 1,
    NCA_ENC_AES_XTS                 = 2,
    NCA_ENC_AES_CTR                 = 3,
    NCA_ENC_AES_CTR_EX              = 4,
    NCA_ENC_AES_CTR_SKIP_LAYER_HASH = 5,
    NCA_ENC_AES_CTR_EX_SKIP_LAYER   = 6,
    NCA_ENC_COUNT                   = 7
} nca_encryption_type_t;

/*============================================================================
 * FS Section Info
 *============================================================================*/

typedef struct {
    uint32_t start_sector;      /* In NCA_SECTOR_SIZE units */
    uint32_t end_sector;        /* In NCA_SECTOR_SIZE units */
    uint32_t hash_sector;
    uint8_t reserved[0x4];
} nca_fs_info_t;

_Static_assert(sizeof(nca_fs_info_t) == 0x10, "nca_fs_info_t size mismatch");

/*============================================================================
 * FS Header Hash
 *============================================================================*/

typedef struct {
    uint8_t hash[SHA256_HASH_SIZE];
} nca_fs_header_hash_t;

_Static_assert(sizeof(nca_fs_header_hash_t) == 0x20, "nca_fs_header_hash_t size mismatch");

/*============================================================================
 * Encrypted Key Area
 *============================================================================*/

typedef struct {
    union {
        uint8_t keys[NCA_KEY_AREA_KEY_COUNT][AES_128_KEY_SIZE];
        struct {
            uint8_t aes_xts_1[AES_128_KEY_SIZE];     /* AES-128-XTS key 0 */
            uint8_t aes_xts_2[AES_128_KEY_SIZE];     /* AES-128-XTS key 1 */
            uint8_t aes_ctr[AES_128_KEY_SIZE];       /* AES-128-CTR key */
            uint8_t aes_ctr_ex[AES_128_KEY_SIZE];    /* Unused */
            uint8_t aes_ctr_hw[AES_128_KEY_SIZE];    /* Unused */
            uint8_t reserved[0xB0];
        };
    };
} nca_key_area_t;

_Static_assert(sizeof(nca_key_area_t) == NCA_KEY_AREA_SIZE, "nca_key_area_t size mismatch");

/*============================================================================
 * SDK Version
 *============================================================================*/

typedef struct {
    uint8_t micro;
    uint8_t minor;
    uint8_t major;
    uint8_t revision;
} nca_sdk_version_t;

_Static_assert(sizeof(nca_sdk_version_t) == 4, "nca_sdk_version_t size mismatch");

/*============================================================================
 * Rights ID (for titlekey crypto)
 *============================================================================*/

typedef struct {
    uint8_t id[0x10];
} nca_rights_id_t;

_Static_assert(sizeof(nca_rights_id_t) == 0x10, "nca_rights_id_t size mismatch");

/*============================================================================
 * NCA Header (0x400 bytes, encrypted with header key)
 *============================================================================*/

typedef struct {
    /* 0x000 */ uint8_t main_signature[0x100];      /* RSA-2048-PSS with SHA-256 */
    /* 0x100 */ uint8_t acid_signature[0x100];      /* For Program NCAs */
    /* 0x200 */ uint32_t magic;                      /* "NCA0"/"NCA2"/"NCA3" */
    /* 0x204 */ uint8_t distribution_type;           /* nca_dist_type_t */
    /* 0x205 */ uint8_t content_type;                /* nca_content_type_t */
    /* 0x206 */ uint8_t key_generation_old;          /* Pre-3.0.1 keygen */
    /* 0x207 */ uint8_t kaek_index;                  /* nca_kaek_index_t */
    /* 0x208 */ uint64_t content_size;               /* Total NCA size */
    /* 0x210 */ uint64_t program_id;                 /* Title ID */
    /* 0x218 */ uint32_t content_index;
    /* 0x21C */ nca_sdk_version_t sdk_version;       /* SDK addon version */
    /* 0x220 */ uint8_t key_generation;              /* Post-3.0.1 keygen */
    /* 0x221 */ uint8_t sig_key_generation;
    /* 0x222 */ uint8_t reserved[0xE];
    /* 0x230 */ nca_rights_id_t rights_id;           /* Titlekey crypto */
    /* 0x240 */ nca_fs_info_t fs_info[NCA_FS_HEADER_COUNT];
    /* 0x280 */ nca_fs_header_hash_t fs_header_hash[NCA_FS_HEADER_COUNT];
    /* 0x300 */ nca_key_area_t key_area;
} nca_header_t;

_Static_assert(sizeof(nca_header_t) == 0x400, "nca_header_t size mismatch");

/*============================================================================
 * NCA Context (runtime structure)
 *============================================================================*/

typedef struct {
    nca_header_t header;
    bool header_valid;
    bool encrypted;
    int version;                    /* 0, 2, or 3 */
    uint8_t effective_keygen;
    bool uses_titlekey;
    char content_id[NCA_CONTENT_ID_LEN + 1];
    char title_id_str[20];
} nca_context_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Validate NCA header magic
 */
bool nca_header_valid(const nca_header_t *hdr);

/**
 * Get NCA version from magic
 */
int nca_get_version(const nca_header_t *hdr);

/**
 * Check if NCA uses titlekey crypto
 */
bool nca_uses_titlekey(const nca_header_t *hdr);

/**
 * Get effective key generation
 */
uint8_t nca_get_key_generation(const nca_header_t *hdr);

/**
 * Initialize NCA context from data
 * NOTE: Header is usually encrypted, this parses raw bytes
 * @param ctx Context to initialize
 * @param data Raw NCA data (at least header size)
 * @param data_size Size of data
 * @return true on success
 */
bool nca_init_context(nca_context_t *ctx, const uint8_t *data, size_t data_size);

/**
 * Get content type as string
 */
const char* nca_content_type_str(nca_content_type_t type);

/**
 * Get distribution type as string
 */
const char* nca_dist_type_str(nca_dist_type_t type);

/**
 * Get key generation as HOS version string
 */
const char* nca_keygen_str(nca_key_generation_t keygen);

/**
 * Get encryption type as string
 */
const char* nca_encryption_type_str(nca_encryption_type_t type);

/**
 * Format Title ID as string (buffer must be at least 17 bytes)
 */
void nca_format_title_id(uint64_t title_id, char *buf);

/**
 * Format SDK version as string
 */
void nca_format_sdk_version(const nca_sdk_version_t *ver, char *buf, size_t buf_size);

/**
 * Print NCA info to stdout
 */
void nca_print_info(const nca_context_t *ctx);

/**
 * Extract Content ID from NCA filename
 * @param filename NCA filename (e.g., "abc123...def.nca")
 * @param content_id Output buffer (at least 33 bytes)
 * @return true if valid
 */
bool nca_extract_content_id(const char *filename, char *content_id);

/**
 * Check if filename looks like an NCA
 */
bool nca_is_nca_filename(const char *filename);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* UFT_NCA_H */
