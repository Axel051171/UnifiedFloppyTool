/**
 * @file uft_switch.h
 * @brief Nintendo Switch Container Format Support
 * 
 * Based on hactool by SciresM - support for:
 * - XCI: Game Card Image (physical cartridge dump)
 * - NSP/PFS0: Nintendo Submission Package (digital content)
 * - NCA: Nintendo Content Archive (encrypted content)
 * - ROMFS: Read-Only Memory Filesystem
 * 
 * Note: This module provides format parsing only.
 * Decryption requires console-specific keys.
 * 
 * @author UFT Project (based on hactool by SciresM)
 * @date 2026-01-17
 */

#ifndef UFT_SWITCH_H
#define UFT_SWITCH_H

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
#define XCI_MAGIC               0x44414548  /* "HEAD" */
#define PFS0_MAGIC              0x30534650  /* "PFS0" */
#define HFS0_MAGIC              0x30534648  /* "HFS0" */
#define NCA_MAGIC               0x3041434E  /* "NCA0" / "NCA2" / "NCA3" */
#define ROMFS_MAGIC             0x00000000  /* Level 3 header has no magic */

/** XCI cartridge sizes */
typedef enum {
    XCI_CART_1GB    = 0xFA,
    XCI_CART_2GB    = 0xF8,
    XCI_CART_4GB    = 0xF0,
    XCI_CART_8GB    = 0xE0,
    XCI_CART_16GB   = 0xE1,
    XCI_CART_32GB   = 0xE2
} xci_cart_size_t;

/** NCA content types */
typedef enum {
    NCA_TYPE_PROGRAM        = 0,
    NCA_TYPE_META           = 1,
    NCA_TYPE_CONTROL        = 2,
    NCA_TYPE_MANUAL         = 3,
    NCA_TYPE_DATA           = 4,
    NCA_TYPE_PUBLIC_DATA    = 5
} nca_content_type_t;

/** NCA crypto types */
typedef enum {
    NCA_CRYPTO_NONE         = 0,
    NCA_CRYPTO_XTS          = 1,
    NCA_CRYPTO_CTR          = 2,
    NCA_CRYPTO_BKTR         = 3
} nca_crypto_type_t;

/** NCA section types */
typedef enum {
    NCA_SECTION_ROMFS       = 0,
    NCA_SECTION_PFS0        = 1,
    NCA_SECTION_BKTR        = 2
} nca_section_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief XCI header (512 bytes at offset 0x100)
 */
typedef struct {
    uint8_t     signature[0x100];       /**< RSA-2048 signature */
    uint32_t    magic;                  /**< "HEAD" */
    uint32_t    secure_offset;          /**< Secure partition offset (media units) */
    uint32_t    _reserved1;
    uint8_t     _reserved2;
    uint8_t     cart_type;              /**< Cartridge type/size */
    uint8_t     _reserved3;
    uint8_t     _reserved4;
    uint64_t    _reserved5;
    uint64_t    cart_size;              /**< Cartridge size */
    uint8_t     iv[0x10];               /**< AES IV (reversed) */
    uint64_t    hfs0_offset;            /**< Root HFS0 partition offset */
    uint64_t    hfs0_header_size;       /**< Root HFS0 header size */
    uint8_t     hfs0_hash[0x20];        /**< SHA-256 of HFS0 header */
    uint8_t     initial_data_hash[0x20]; /**< SHA-256 of initial data */
    uint32_t    secure_mode;
    uint32_t    title_key_flag;
    uint32_t    key_flag;
    uint32_t    normal_area_end;
    uint8_t     encrypted_info[0x70];   /**< Encrypted gamecard info */
} xci_header_t;

/**
 * @brief PFS0/HFS0 header
 */
typedef struct {
    uint32_t    magic;                  /**< "PFS0" or "HFS0" */
    uint32_t    num_files;              /**< Number of files */
    uint32_t    string_table_size;      /**< String table size */
    uint32_t    reserved;
} pfs0_header_t;

/**
 * @brief PFS0/HFS0 file entry
 */
typedef struct {
    uint64_t    offset;                 /**< File data offset */
    uint64_t    size;                   /**< File size */
    uint32_t    string_offset;          /**< Filename offset in string table */
    uint32_t    reserved;               /**< PFS0: reserved, HFS0: hash offset */
} pfs0_file_entry_t;

/**
 * @brief NCA header (encrypted, 0x400 bytes)
 */
typedef struct {
    uint8_t     signature1[0x100];      /**< RSA-2048 signature 1 */
    uint8_t     signature2[0x100];      /**< RSA-2048 signature 2 */
    uint32_t    magic;                  /**< "NCA3", "NCA2", "NCA0" */
    uint8_t     distribution;           /**< Distribution type */
    uint8_t     content_type;           /**< Content type */
    uint8_t     crypto_type;            /**< Crypto generation (old) */
    uint8_t     key_index;              /**< Key area encryption key index */
    uint64_t    size;                   /**< NCA size */
    uint64_t    title_id;               /**< Title ID */
    uint32_t    context_id;
    uint32_t    sdk_version;            /**< SDK version */
    uint8_t     crypto_type2;           /**< Crypto generation (new) */
    uint8_t     _reserved1[0x0F];
    uint8_t     rights_id[0x10];        /**< Rights ID */
    /* Section entries follow... */
} nca_header_t;

/**
 * @brief NCA section entry (in header)
 */
typedef struct {
    uint32_t    start_offset;           /**< Media offset (0x200 units) */
    uint32_t    end_offset;             /**< Media offset end */
    uint32_t    _reserved1;
    uint32_t    _reserved2;
} nca_section_entry_t;

/**
 * @brief XCI partition info
 */
typedef struct {
    char        name[64];               /**< Partition name */
    uint64_t    offset;                 /**< Offset in file */
    uint64_t    size;                   /**< Size in bytes */
    int         num_files;              /**< Number of files */
} xci_partition_info_t;

/**
 * @brief File entry info
 */
typedef struct {
    char        name[256];              /**< Filename */
    uint64_t    offset;                 /**< Offset in container */
    uint64_t    size;                   /**< File size */
} switch_file_entry_t;

/**
 * @brief XCI info
 */
typedef struct {
    uint64_t    cart_size;              /**< Cartridge size */
    xci_cart_size_t cart_type;          /**< Cartridge type */
    int         num_partitions;         /**< Number of partitions */
    xci_partition_info_t partitions[4]; /**< Partitions (normal, update, secure, logo) */
} xci_info_t;

/**
 * @brief NSP/PFS0 info
 */
typedef struct {
    int         num_files;              /**< Number of files */
    uint64_t    total_size;             /**< Total size */
} nsp_info_t;

/**
 * @brief Switch container context
 */
typedef struct {
    uint8_t     *data;                  /**< File data */
    size_t      size;                   /**< File size */
    bool        is_xci;                 /**< true if XCI, false if NSP */
    xci_header_t *xci_header;           /**< XCI header (if XCI) */
    pfs0_header_t *pfs0_header;         /**< PFS0 header (if NSP) */
    bool        owns_data;              /**< true if we allocated */
} switch_ctx_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if data is XCI format
 * @param data File data
 * @param size Data size
 * @return true if XCI
 */
bool xci_detect(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is NSP/PFS0 format
 * @param data File data
 * @param size Data size
 * @return true if NSP
 */
bool nsp_detect(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is NCA format
 * @param data File data
 * @param size Data size
 * @return true if NCA (may be encrypted)
 */
bool nca_detect(const uint8_t *data, size_t size);

/**
 * @brief Get cartridge size name
 * @param type Cartridge type
 * @return Size string (e.g., "8GB")
 */
const char *xci_cart_size_name(xci_cart_size_t type);

/**
 * @brief Get NCA content type name
 * @param type Content type
 * @return Type name
 */
const char *nca_content_type_name(nca_content_type_t type);

/* ============================================================================
 * API Functions - Container Operations
 * ============================================================================ */

/**
 * @brief Open Switch container (XCI or NSP)
 * @param data File data
 * @param size Data size
 * @param ctx Output context
 * @return 0 on success
 */
int switch_open(const uint8_t *data, size_t size, switch_ctx_t *ctx);

/**
 * @brief Load Switch container from file
 * @param filename File path
 * @param ctx Output context
 * @return 0 on success
 */
int switch_load(const char *filename, switch_ctx_t *ctx);

/**
 * @brief Close Switch container
 * @param ctx Context to close
 */
void switch_close(switch_ctx_t *ctx);

/* ============================================================================
 * API Functions - XCI Operations
 * ============================================================================ */

/**
 * @brief Get XCI info
 * @param ctx Switch context
 * @param info Output info
 * @return 0 on success
 */
int xci_get_info(const switch_ctx_t *ctx, xci_info_t *info);

/**
 * @brief Get XCI partition count
 * @param ctx Switch context
 * @return Number of partitions
 */
int xci_get_partition_count(const switch_ctx_t *ctx);

/**
 * @brief Get XCI partition info
 * @param ctx Switch context
 * @param index Partition index
 * @param info Output info
 * @return 0 on success
 */
int xci_get_partition(const switch_ctx_t *ctx, int index,
                      xci_partition_info_t *info);

/**
 * @brief List files in XCI partition
 * @param ctx Switch context
 * @param partition Partition name
 * @param files Output file array
 * @param max_files Maximum files
 * @return Number of files found
 */
int xci_list_files(const switch_ctx_t *ctx, const char *partition,
                   switch_file_entry_t *files, int max_files);

/* ============================================================================
 * API Functions - NSP Operations
 * ============================================================================ */

/**
 * @brief Get NSP info
 * @param ctx Switch context
 * @param info Output info
 * @return 0 on success
 */
int nsp_get_info(const switch_ctx_t *ctx, nsp_info_t *info);

/**
 * @brief Get NSP file count
 * @param ctx Switch context
 * @return Number of files
 */
int nsp_get_file_count(const switch_ctx_t *ctx);

/**
 * @brief Get NSP file entry
 * @param ctx Switch context
 * @param index File index
 * @param entry Output entry
 * @return 0 on success
 */
int nsp_get_file(const switch_ctx_t *ctx, int index,
                 switch_file_entry_t *entry);

/**
 * @brief Extract file from NSP
 * @param ctx Switch context
 * @param index File index
 * @param buffer Output buffer
 * @param max_size Maximum size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int nsp_extract_file(const switch_ctx_t *ctx, int index,
                     uint8_t *buffer, size_t max_size, size_t *extracted);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print XCI info
 * @param ctx Switch context
 * @param fp Output file
 */
void xci_print_info(const switch_ctx_t *ctx, FILE *fp);

/**
 * @brief Print NSP info
 * @param ctx Switch context
 * @param fp Output file
 */
void nsp_print_info(const switch_ctx_t *ctx, FILE *fp);

/**
 * @brief Convert title ID to string
 * @param title_id Title ID
 * @param buffer Output buffer (17 bytes min)
 */
void switch_title_id_str(uint64_t title_id, char *buffer);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SWITCH_H */
