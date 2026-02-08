/**
 * @file uft_3ds.h
 * @brief Nintendo 3DS Container Format Support
 * 
 * Support for Nintendo 3DS file formats:
 * - 3DS/CCI: CTR Cart Image (physical cartridge dump)
 * - CIA: CTR Importable Archive (installable content)
 * - CXI: CTR Executable Image (executable content)
 * - CFA: CTR File Archive (non-executable content)
 * - NCCH: Nintendo Content Container Header
 * - NCSD: Nintendo Content Storage Device
 * - ExeFS: Executable Filesystem
 * - RomFS: Read-Only Memory Filesystem
 * 
 * Note: Decryption requires console-specific keys.
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_3DS_H
#define UFT_3DS_H

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

/** Magic signatures */
#define NCSD_MAGIC              0x4453434E  /* "NCSD" */
#define NCCH_MAGIC              0x4843434E  /* "NCCH" */
#define EXEFS_MAGIC             0x00000000  /* ExeFS has no magic, identified by structure */
#define IVFC_MAGIC              0x43465649  /* "IVFC" for RomFS */

/** CIA header magic */
#define CIA_HEADER_SIZE         0x2020
#define CIA_ALIGN               64

/** NCSD partition types */
typedef enum {
    NCSD_PART_EXECUTABLE    = 0,    /**< Main executable (CXI) */
    NCSD_PART_MANUAL        = 1,    /**< Electronic manual (CFA) */
    NCSD_PART_DLP_CHILD     = 2,    /**< Download Play child */
    NCSD_PART_RESERVED3     = 3,
    NCSD_PART_RESERVED4     = 4,
    NCSD_PART_RESERVED5     = 5,
    NCSD_PART_N3DS_UPDATE   = 6,    /**< New 3DS update data */
    NCSD_PART_O3DS_UPDATE   = 7     /**< Old 3DS update data */
} ncsd_partition_type_t;

/** NCCH content types */
typedef enum {
    NCCH_TYPE_CXI           = 0,    /**< Executable */
    NCCH_TYPE_CFA           = 1     /**< Archive */
} ncch_type_t;

/** Content platform */
typedef enum {
    PLATFORM_CTR            = 1,    /**< 3DS */
    PLATFORM_SNAKE          = 2     /**< New 3DS */
} n3ds_platform_t;

/** Media types */
typedef enum {
    MEDIA_CARD1             = 1,    /**< Gamecard CARD1 */
    MEDIA_CARD2             = 2,    /**< Gamecard CARD2 */
    MEDIA_NAND              = 0     /**< Internal storage */
} n3ds_media_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief NCSD header (512 bytes)
 */
typedef struct {
    uint8_t     signature[0x100];   /**< RSA-2048 signature */
    uint32_t    magic;              /**< "NCSD" */
    uint32_t    size;               /**< Size in media units */
    uint64_t    media_id;           /**< Media ID */
    uint8_t     partition_fs_type[8]; /**< Partition FS types */
    uint8_t     partition_crypt_type[8]; /**< Partition crypto types */
    struct {
        uint32_t offset;            /**< Offset in media units */
        uint32_t size;              /**< Size in media units */
    } partitions[8];                /**< Partition table */
    uint8_t     exheader_hash[0x20]; /**< ExHeader SHA-256 */
    uint32_t    additional_header_size;
    uint32_t    sector_zero_offset;
    uint8_t     partition_flags[8];
    uint64_t    partition_ids[8];
    uint8_t     reserved[0x30];
} ncsd_header_t;

/**
 * @brief NCCH header (512 bytes)
 */
typedef struct {
    uint8_t     signature[0x100];   /**< RSA-2048 signature */
    uint32_t    magic;              /**< "NCCH" */
    uint32_t    content_size;       /**< Size in media units */
    uint64_t    partition_id;       /**< Partition ID */
    uint16_t    maker_code;         /**< Maker code */
    uint16_t    version;            /**< Version */
    uint32_t    hash_seed_check;
    uint64_t    program_id;         /**< Program/Title ID */
    uint8_t     reserved1[0x10];
    uint8_t     logo_hash[0x20];    /**< Logo region SHA-256 */
    char        product_code[0x10]; /**< Product code */
    uint8_t     exheader_hash[0x20]; /**< Extended header SHA-256 */
    uint32_t    exheader_size;      /**< Extended header size */
    uint32_t    reserved2;
    uint8_t     flags[8];           /**< NCCH flags */
    uint32_t    plain_offset;       /**< Plain region offset */
    uint32_t    plain_size;         /**< Plain region size */
    uint32_t    logo_offset;        /**< Logo region offset */
    uint32_t    logo_size;          /**< Logo region size */
    uint32_t    exefs_offset;       /**< ExeFS offset */
    uint32_t    exefs_size;         /**< ExeFS size */
    uint32_t    exefs_hash_size;    /**< ExeFS hash region size */
    uint32_t    reserved3;
    uint32_t    romfs_offset;       /**< RomFS offset */
    uint32_t    romfs_size;         /**< RomFS size */
    uint32_t    romfs_hash_size;    /**< RomFS hash region size */
    uint32_t    reserved4;
    uint8_t     exefs_hash[0x20];   /**< ExeFS superblock SHA-256 */
    uint8_t     romfs_hash[0x20];   /**< RomFS superblock SHA-256 */
} ncch_header_t;

/**
 * @brief ExeFS file header (16 bytes each)
 */
typedef struct {
    char        name[8];            /**< Filename */
    uint32_t    offset;             /**< Offset in ExeFS */
    uint32_t    size;               /**< File size */
} exefs_file_header_t;

/**
 * @brief ExeFS header (512 bytes)
 */
typedef struct {
    exefs_file_header_t files[10];  /**< File headers */
    uint8_t     reserved[0x20];
    uint8_t     file_hashes[10][0x20]; /**< SHA-256 hashes (reverse order) */
} exefs_header_t;

/**
 * @brief CIA header
 */
typedef struct {
    uint32_t    header_size;        /**< Header size */
    uint16_t    type;               /**< CIA type */
    uint16_t    version;            /**< CIA version */
    uint32_t    cert_size;          /**< Certificate chain size */
    uint32_t    ticket_size;        /**< Ticket size */
    uint32_t    tmd_size;           /**< Title metadata size */
    uint32_t    meta_size;          /**< Meta size */
    uint64_t    content_size;       /**< Content size */
    uint8_t     content_index[0x2000]; /**< Content index */
} cia_header_t;

/**
 * @brief 3DS image info
 */
typedef struct {
    bool        is_cci;             /**< true=CCI/3DS, false=CIA */
    uint64_t    title_id;           /**< Title ID */
    char        product_code[17];   /**< Product code */
    uint16_t    maker_code;         /**< Maker code */
    size_t      file_size;          /**< File size */
    int         num_partitions;     /**< Number of partitions */
    bool        has_exefs;          /**< Has ExeFS */
    bool        has_romfs;          /**< Has RomFS */
    bool        encrypted;          /**< Is encrypted */
} n3ds_info_t;

/**
 * @brief 3DS image context
 */
typedef struct {
    uint8_t     *data;              /**< File data */
    size_t      size;               /**< Data size */
    bool        is_cci;             /**< CCI or CIA */
    ncsd_header_t *ncsd;            /**< NCSD header (if CCI) */
    ncch_header_t *ncch;            /**< Main NCCH header */
    cia_header_t *cia;              /**< CIA header (if CIA) */
    bool        owns_data;          /**< true if we allocated */
} n3ds_ctx_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if data is 3DS/CCI format
 * @param data File data
 * @param size Data size
 * @return true if CCI
 */
bool n3ds_detect_cci(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is CIA format
 * @param data File data
 * @param size Data size
 * @return true if CIA
 */
bool n3ds_detect_cia(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is NCCH format
 * @param data File data
 * @param size Data size
 * @return true if NCCH
 */
bool n3ds_detect_ncch(const uint8_t *data, size_t size);

/**
 * @brief Check if content is encrypted
 * @param ncch NCCH header
 * @return true if encrypted
 */
bool n3ds_is_encrypted(const ncch_header_t *ncch);

/* ============================================================================
 * API Functions - Container Operations
 * ============================================================================ */

/**
 * @brief Open 3DS image
 * @param data File data
 * @param size Data size
 * @param ctx Output context
 * @return 0 on success
 */
int n3ds_open(const uint8_t *data, size_t size, n3ds_ctx_t *ctx);

/**
 * @brief Load 3DS image from file
 * @param filename File path
 * @param ctx Output context
 * @return 0 on success
 */
int n3ds_load(const char *filename, n3ds_ctx_t *ctx);

/**
 * @brief Close 3DS context
 * @param ctx Context to close
 */
void n3ds_close(n3ds_ctx_t *ctx);

/**
 * @brief Get image info
 * @param ctx 3DS context
 * @param info Output info
 * @return 0 on success
 */
int n3ds_get_info(const n3ds_ctx_t *ctx, n3ds_info_t *info);

/* ============================================================================
 * API Functions - Partition Access
 * ============================================================================ */

/**
 * @brief Get partition count
 * @param ctx 3DS context
 * @return Number of partitions
 */
int n3ds_get_partition_count(const n3ds_ctx_t *ctx);

/**
 * @brief Get partition NCCH header
 * @param ctx 3DS context
 * @param index Partition index
 * @param ncch Output NCCH header
 * @return 0 on success
 */
int n3ds_get_partition(const n3ds_ctx_t *ctx, int index, ncch_header_t *ncch);

/**
 * @brief Get partition offset and size
 * @param ctx 3DS context
 * @param index Partition index
 * @param offset Output offset
 * @param size Output size
 * @return 0 on success
 */
int n3ds_get_partition_bounds(const n3ds_ctx_t *ctx, int index,
                               size_t *offset, size_t *size);

/* ============================================================================
 * API Functions - ExeFS Access
 * ============================================================================ */

/**
 * @brief Get ExeFS file count
 * @param ctx 3DS context
 * @return Number of files
 */
int n3ds_exefs_file_count(const n3ds_ctx_t *ctx);

/**
 * @brief Get ExeFS file info
 * @param ctx 3DS context
 * @param index File index
 * @param name Output filename (8 bytes)
 * @param size Output file size
 * @return 0 on success
 */
int n3ds_exefs_get_file(const n3ds_ctx_t *ctx, int index,
                         char *name, size_t *size);

/**
 * @brief Extract ExeFS file
 * @param ctx 3DS context
 * @param name Filename
 * @param buffer Output buffer
 * @param max_size Maximum size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int n3ds_exefs_extract(const n3ds_ctx_t *ctx, const char *name,
                        uint8_t *buffer, size_t max_size, size_t *extracted);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Convert title ID to string
 * @param title_id Title ID
 * @param buffer Output buffer (17 bytes min)
 */
void n3ds_title_id_str(uint64_t title_id, char *buffer);

/**
 * @brief Print image info
 * @param ctx 3DS context
 * @param fp Output file
 */
void n3ds_print_info(const n3ds_ctx_t *ctx, FILE *fp);

/**
 * @brief Print ExeFS contents
 * @param ctx 3DS context
 * @param fp Output file
 */
void n3ds_print_exefs(const n3ds_ctx_t *ctx, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_3DS_H */
