/**
 * @file uft_fat_badblock.h
 * @brief FAT Bad Block/Sector Management
 * @version 1.0.0
 * 
 * Bad block handling for FAT filesystems:
 * - Import bad block lists (dosfstools format)
 * - Export bad block lists
 * - Mark/unmark clusters as bad
 * - Surface scan integration
 * - Bad sector remapping
 * 
 * File format compatible with:
 * - dosfstools badblocks (-l option)
 * - e2fsck/badblocks utility output
 * - Custom sector lists
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

#ifndef UFT_FAT_BADBLOCK_H
#define UFT_FAT_BADBLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "uft/fs/uft_fat12.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum bad blocks in list */
#define UFT_BADBLOCK_MAX_ENTRIES    65536

/** Bad block list file magic (optional header) */
#define UFT_BADBLOCK_MAGIC          "BADBLK01"

/*===========================================================================
 * Bad Block Entry Types
 *===========================================================================*/

/**
 * @brief Bad block entry unit type
 */
typedef enum {
    UFT_BADBLOCK_SECTOR = 0,        /**< Entry is sector number */
    UFT_BADBLOCK_CLUSTER,           /**< Entry is cluster number */
    UFT_BADBLOCK_BYTE_OFFSET,       /**< Entry is byte offset */
    UFT_BADBLOCK_BLOCK_1K           /**< Entry is 1KB block (mkfs.fat) */
} uft_badblock_unit_t;

/**
 * @brief Bad block source
 */
typedef enum {
    UFT_BADBLOCK_SRC_MANUAL = 0,    /**< Manually added */
    UFT_BADBLOCK_SRC_FILE,          /**< Imported from file */
    UFT_BADBLOCK_SRC_SCAN,          /**< Found by surface scan */
    UFT_BADBLOCK_SRC_FAT            /**< Read from FAT table */
} uft_badblock_source_t;

/*===========================================================================
 * Bad Block Entry
 *===========================================================================*/

/**
 * @brief Single bad block entry
 */
typedef struct {
    uint64_t location;              /**< Location (interpretation depends on unit) */
    uft_badblock_unit_t unit;       /**< Unit type */
    uft_badblock_source_t source;   /**< How this was detected */
    uint32_t cluster;               /**< Corresponding cluster (if known) */
    bool marked_in_fat;             /**< Already marked in FAT */
} uft_badblock_entry_t;

/**
 * @brief Bad block list
 */
typedef struct {
    uft_badblock_entry_t *entries;  /**< Array of entries */
    size_t count;                   /**< Number of entries */
    size_t capacity;                /**< Allocated capacity */
    uft_badblock_unit_t default_unit; /**< Default unit for new entries */
} uft_badblock_list_t;

/*===========================================================================
 * Bad Block Statistics
 *===========================================================================*/

/**
 * @brief Bad block analysis results
 */
typedef struct {
    size_t total_bad;               /**< Total bad entries */
    size_t in_data_area;            /**< Bad blocks in data area */
    size_t in_reserved;             /**< Bad blocks in reserved area */
    size_t in_fat;                  /**< Bad blocks in FAT area */
    size_t in_root_dir;             /**< Bad blocks in root directory */
    size_t already_marked;          /**< Already marked in FAT */
    size_t needs_marking;           /**< Not yet marked in FAT */
    uint64_t bytes_affected;        /**< Total bytes in bad areas */
    uint32_t clusters_affected;     /**< Total clusters affected */
} uft_badblock_stats_t;

/*===========================================================================
 * API - List Management
 *===========================================================================*/

/**
 * @brief Create empty bad block list
 * @return New list or NULL
 */
uft_badblock_list_t *uft_badblock_list_create(void);

/**
 * @brief Destroy bad block list
 * @param list List to destroy
 */
void uft_badblock_list_destroy(uft_badblock_list_t *list);

/**
 * @brief Clear all entries from list
 * @param list List to clear
 */
void uft_badblock_list_clear(uft_badblock_list_t *list);

/**
 * @brief Add entry to list
 * @param list Target list
 * @param location Bad location
 * @param unit Location unit type
 * @return 0 on success
 */
int uft_badblock_list_add(uft_badblock_list_t *list, uint64_t location,
                          uft_badblock_unit_t unit);

/**
 * @brief Add entry with full details
 * @param list Target list
 * @param entry Entry to add (copied)
 * @return 0 on success
 */
int uft_badblock_list_add_entry(uft_badblock_list_t *list,
                                const uft_badblock_entry_t *entry);

/**
 * @brief Remove entry from list
 * @param list Target list
 * @param index Entry index
 * @return 0 on success
 */
int uft_badblock_list_remove(uft_badblock_list_t *list, size_t index);

/**
 * @brief Sort list by location
 * @param list List to sort
 */
void uft_badblock_list_sort(uft_badblock_list_t *list);

/**
 * @brief Remove duplicate entries
 * @param list List to deduplicate
 * @return Number of duplicates removed
 */
size_t uft_badblock_list_dedupe(uft_badblock_list_t *list);

/*===========================================================================
 * API - File Import/Export
 *===========================================================================*/

/**
 * @brief Import bad block list from file
 * @param list Target list
 * @param filename Path to bad block file
 * @param unit Unit type for entries in file
 * @return 0 on success, negative on error
 * 
 * File format: One number per line (decimal or hex with 0x prefix)
 * Lines starting with # are comments
 */
int uft_badblock_import_file(uft_badblock_list_t *list, const char *filename,
                             uft_badblock_unit_t unit);

/**
 * @brief Import from stdio stream
 * @param list Target list
 * @param fp File stream
 * @param unit Unit type
 * @return 0 on success
 */
int uft_badblock_import_stream(uft_badblock_list_t *list, FILE *fp,
                               uft_badblock_unit_t unit);

/**
 * @brief Import from string buffer
 * @param list Target list
 * @param data String data (newline separated)
 * @param size Data size
 * @param unit Unit type
 * @return 0 on success
 */
int uft_badblock_import_buffer(uft_badblock_list_t *list, const char *data,
                               size_t size, uft_badblock_unit_t unit);

/**
 * @brief Export bad block list to file
 * @param list Source list
 * @param filename Output path
 * @param unit Output unit type
 * @return 0 on success
 */
int uft_badblock_export_file(const uft_badblock_list_t *list, 
                             const char *filename, uft_badblock_unit_t unit);

/**
 * @brief Export to stdio stream
 * @param list Source list
 * @param fp File stream
 * @param unit Output unit type
 * @return 0 on success
 */
int uft_badblock_export_stream(const uft_badblock_list_t *list, FILE *fp,
                               uft_badblock_unit_t unit);

/*===========================================================================
 * API - FAT Integration
 *===========================================================================*/

/**
 * @brief Read bad clusters from FAT table
 * @param list Target list
 * @param ctx FAT context
 * @return Number of bad clusters found
 */
size_t uft_badblock_read_from_fat(uft_badblock_list_t *list,
                                  const uft_fat_ctx_t *ctx);

/**
 * @brief Mark bad blocks in FAT table
 * @param list Bad block list
 * @param ctx FAT context (modified)
 * @param stats Output statistics (optional)
 * @return Number of clusters marked
 * 
 * @note Converts all entries to cluster numbers and marks them
 */
size_t uft_badblock_mark_in_fat(const uft_badblock_list_t *list,
                                uft_fat_ctx_t *ctx,
                                uft_badblock_stats_t *stats);

/**
 * @brief Unmark bad clusters in FAT (mark as free)
 * @param list Bad block list
 * @param ctx FAT context (modified)
 * @return Number of clusters unmarked
 * 
 * @warning Use with caution - may cause data corruption
 */
size_t uft_badblock_unmark_in_fat(const uft_badblock_list_t *list,
                                  uft_fat_ctx_t *ctx);

/**
 * @brief Analyze bad blocks against FAT
 * @param list Bad block list
 * @param ctx FAT context
 * @param stats Output statistics
 * @return 0 on success
 */
int uft_badblock_analyze(const uft_badblock_list_t *list,
                         const uft_fat_ctx_t *ctx,
                         uft_badblock_stats_t *stats);

/*===========================================================================
 * API - Conversion
 *===========================================================================*/

/**
 * @brief Convert sector to cluster
 * @param ctx FAT context
 * @param sector Sector number
 * @return Cluster number or 0 if in system area
 */
uint32_t uft_badblock_sector_to_cluster(const uft_fat_ctx_t *ctx, 
                                        uint64_t sector);

/**
 * @brief Convert byte offset to cluster
 * @param ctx FAT context
 * @param offset Byte offset
 * @return Cluster number or 0 if in system area
 */
uint32_t uft_badblock_offset_to_cluster(const uft_fat_ctx_t *ctx,
                                        uint64_t offset);

/**
 * @brief Convert 1KB block to cluster (mkfs.fat format)
 * @param ctx FAT context
 * @param block Block number
 * @return Cluster number or 0 if in system area
 */
uint32_t uft_badblock_block_to_cluster(const uft_fat_ctx_t *ctx,
                                       uint64_t block);

/**
 * @brief Convert cluster to sector range
 * @param ctx FAT context
 * @param cluster Cluster number
 * @param first_sector Output: first sector
 * @param sector_count Output: number of sectors
 * @return 0 on success
 */
int uft_badblock_cluster_to_sectors(const uft_fat_ctx_t *ctx,
                                    uint32_t cluster,
                                    uint64_t *first_sector,
                                    uint32_t *sector_count);

/*===========================================================================
 * API - Utilities
 *===========================================================================*/

/**
 * @brief Check if location is in data area
 * @param ctx FAT context
 * @param location Location value
 * @param unit Location unit
 * @return true if in data area
 */
bool uft_badblock_in_data_area(const uft_fat_ctx_t *ctx, uint64_t location,
                               uft_badblock_unit_t unit);

/**
 * @brief Get string name for unit type
 * @param unit Unit type
 * @return String name
 */
const char *uft_badblock_unit_str(uft_badblock_unit_t unit);

/**
 * @brief Get string name for source type
 * @param source Source type
 * @return String name
 */
const char *uft_badblock_source_str(uft_badblock_source_t source);

/**
 * @brief Print bad block list summary
 * @param list Bad block list
 * @param fp Output stream
 */
void uft_badblock_print_summary(const uft_badblock_list_t *list, FILE *fp);

/**
 * @brief Print bad block statistics
 * @param stats Statistics
 * @param fp Output stream
 */
void uft_badblock_print_stats(const uft_badblock_stats_t *stats, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_BADBLOCK_H */
