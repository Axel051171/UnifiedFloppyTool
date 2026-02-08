/**
 * @file uft_xci_parser.h
 * @brief XCI File Parser (wraps hactool)
 * 
 * @version 1.0.0
 * @date 2025-01-20
 */

#ifndef UFT_XCI_PARSER_H
#define UFT_XCI_PARSER_H

#include "uft_switch_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct uft_xci_ctx uft_xci_ctx_t;

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Open XCI file for reading
 * @param path Path to XCI file
 * @param ctx Output context handle
 * @return 0 on success
 */
int uft_xci_open(const char *path, uft_xci_ctx_t **ctx);

/**
 * @brief Close XCI file
 * @param ctx Context handle
 */
void uft_xci_close(uft_xci_ctx_t *ctx);

/**
 * @brief Get XCI header information
 * @param ctx Context handle
 * @param header Output header structure
 * @return 0 on success
 */
int uft_xci_get_header(uft_xci_ctx_t *ctx, uft_xci_header_t *header);

/**
 * @brief Get XCI info (parsed header)
 * @param ctx Context handle
 * @param info Output info structure
 * @return 0 on success
 */
int uft_xci_get_info(uft_xci_ctx_t *ctx, uft_xci_info_t *info);

/* ============================================================================
 * Partition Operations
 * ============================================================================ */

/**
 * @brief Get number of partitions
 * @param ctx Context handle
 * @return Number of partitions
 */
int uft_xci_get_partition_count(uft_xci_ctx_t *ctx);

/**
 * @brief Get partition info
 * @param ctx Context handle
 * @param index Partition index
 * @param info Output partition info
 * @return 0 on success
 */
int uft_xci_get_partition_info(uft_xci_ctx_t *ctx, int index, uft_partition_info_t *info);

/**
 * @brief List files in partition
 * @param ctx Context handle
 * @param partition Partition type
 * @param files Output array of file names
 * @param max_files Maximum number of files
 * @return Number of files found
 */
int uft_xci_list_partition_files(uft_xci_ctx_t *ctx, 
                                  uft_xci_partition_t partition,
                                  char **files, int max_files);

/* ============================================================================
 * NCA Operations
 * ============================================================================ */

/**
 * @brief Get number of NCAs in XCI
 * @param ctx Context handle
 * @return Number of NCAs
 */
int uft_xci_get_nca_count(uft_xci_ctx_t *ctx);

/**
 * @brief Get NCA info
 * @param ctx Context handle
 * @param index NCA index
 * @param info Output NCA info
 * @return 0 on success
 */
int uft_xci_get_nca_info(uft_xci_ctx_t *ctx, int index, uft_nca_info_t *info);

/* ============================================================================
 * Extraction Operations
 * ============================================================================ */

/**
 * @brief Extract partition to directory
 * @param ctx Context handle
 * @param partition Partition type
 * @param output_dir Output directory
 * @return 0 on success
 */
int uft_xci_extract_partition(uft_xci_ctx_t *ctx,
                               uft_xci_partition_t partition,
                               const char *output_dir);

/**
 * @brief Extract single file from partition
 * @param ctx Context handle
 * @param partition Partition type
 * @param filename File name in partition
 * @param output_path Output file path
 * @return 0 on success
 */
int uft_xci_extract_file(uft_xci_ctx_t *ctx,
                          uft_xci_partition_t partition,
                          const char *filename,
                          const char *output_path);

/**
 * @brief Extract NCA to file
 * @param ctx Context handle
 * @param index NCA index
 * @param output_path Output file path
 * @return 0 on success
 */
int uft_xci_extract_nca(uft_xci_ctx_t *ctx, int index, const char *output_path);

/* ============================================================================
 * Key Management
 * ============================================================================ */

/**
 * @brief Load keys from file
 * @param path Path to keys file (prod.keys format)
 * @return 0 on success
 */
int uft_xci_load_keys(const char *path);

/**
 * @brief Set XCI header key
 * @param key 16-byte key
 */
void uft_xci_set_header_key(const uint8_t *key);

/**
 * @brief Check if keys are loaded
 * @return true if keys available
 */
bool uft_xci_has_keys(void);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * @brief Verify XCI file integrity
 * @param ctx Context handle
 * @return 0 if valid
 */
int uft_xci_verify(uft_xci_ctx_t *ctx);

/**
 * @brief Trim XCI file (remove unused space)
 * @param input_path Input XCI path
 * @param output_path Output XCI path
 * @return 0 on success
 */
int uft_xci_trim(const char *input_path, const char *output_path);

/**
 * @brief Get cart size string
 * @param cart_type Cart type byte
 * @return Human-readable size string
 */
const char *uft_xci_cart_size_string(uint8_t cart_type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XCI_PARSER_H */
