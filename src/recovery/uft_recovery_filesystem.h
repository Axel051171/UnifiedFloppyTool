/**
 * @file uft_recovery_filesystem.h
 * @brief GOD MODE Filesystem-Based Recovery
 * 
 * Filesystem-gestützte Recovery (optional):
 * - Directory-Plausibilitätsprüfung
 * - Block-Chain-Rekonstruktion
 * - Fragment-Salvage
 * - Teildateien mit Lückenmarkierung
 * - Read-Only-Analyse (niemals auto-fix)
 * 
 * WICHTIG: Read-Only! Keine automatischen Änderungen!
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_FILESYSTEM_H
#define UFT_RECOVERY_FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Filesystem Types
 * ============================================================================ */

typedef enum {
    UFT_FS_UNKNOWN = 0,
    
    /* Commodore */
    UFT_FS_CBM_DOS,             /**< CBM DOS (D64/D71/D81) */
    UFT_FS_CMD_DOS,             /**< CMD DOS (Native/Emulation) */
    
    /* Amiga */
    UFT_FS_AMIGA_OFS,           /**< Amiga OFS */
    UFT_FS_AMIGA_FFS,           /**< Amiga FFS */
    UFT_FS_AMIGA_PFS,           /**< Amiga PFS */
    
    /* Apple */
    UFT_FS_APPLE_DOS33,         /**< Apple DOS 3.3 */
    UFT_FS_APPLE_PRODOS,        /**< Apple ProDOS */
    UFT_FS_APPLE_MFS,           /**< Macintosh MFS */
    UFT_FS_APPLE_HFS,           /**< Macintosh HFS */
    
    /* PC */
    UFT_FS_FAT12,               /**< FAT12 */
    UFT_FS_FAT16,               /**< FAT16 */
    
    /* CP/M */
    UFT_FS_CPM,                 /**< CP/M */
    UFT_FS_CPM22,               /**< CP/M 2.2 */
    UFT_FS_CPM3,                /**< CP/M Plus */
    
    /* Other */
    UFT_FS_TRSDOS,              /**< TRSDOS */
    UFT_FS_BBC_DFS,             /**< BBC DFS */
    UFT_FS_ATARI_DOS,           /**< Atari DOS */
    UFT_FS_MSX_DOS,             /**< MSX-DOS */
} uft_filesystem_type_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Directory entry
 */
typedef struct {
    char     filename[256];     /**< Filename */
    uint32_t size;              /**< File size */
    uint8_t  type;              /**< File type */
    uint32_t start_block;       /**< Start block/sector */
    uint32_t block_count;       /**< Number of blocks */
    bool     is_deleted;        /**< Is deleted */
    bool     is_valid;          /**< Entry is valid */
    uint8_t  confidence;        /**< Confidence in entry */
} uft_dir_entry_t;

/**
 * @brief Block chain link
 */
typedef struct {
    uint32_t block_num;         /**< Block number */
    uint32_t next_block;        /**< Next block (or 0/EOF) */
    bool     is_valid;          /**< Link is valid */
    bool     is_end;            /**< Is end of chain */
    bool     is_damaged;        /**< Block is damaged */
    uint8_t  confidence;        /**< Confidence */
} uft_block_link_t;

/**
 * @brief Block chain
 */
typedef struct {
    uft_block_link_t *links;    /**< Chain links */
    size_t   link_count;        /**< Number of links */
    uint32_t start_block;       /**< Start block */
    uint32_t total_blocks;      /**< Total blocks */
    uint32_t valid_blocks;      /**< Valid blocks */
    uint32_t damaged_blocks;    /**< Damaged blocks */
    bool     is_complete;       /**< Chain is complete */
    bool     has_loops;         /**< Chain has loops */
    bool     has_cross_links;   /**< Cross-linked with other file */
} uft_block_chain_t;

/**
 * @brief File fragment
 */
typedef struct {
    uint32_t start_block;       /**< Fragment start */
    uint32_t block_count;       /**< Fragment blocks */
    uint32_t byte_offset;       /**< Offset in file */
    uint32_t byte_count;        /**< Bytes in fragment */
    uint8_t *data;              /**< Fragment data */
    bool     is_valid;          /**< Data is valid */
    uint8_t  confidence;        /**< Confidence */
} uft_fragment_t;

/**
 * @brief Recovered file
 */
typedef struct {
    char     filename[256];     /**< Filename */
    uft_dir_entry_t *dir_entry; /**< Directory entry */
    
    /* Fragments */
    uft_fragment_t *fragments;  /**< File fragments */
    size_t   fragment_count;    /**< Number of fragments */
    
    /* Gap info */
    uint32_t *gap_starts;       /**< Start of gaps */
    uint32_t *gap_lengths;      /**< Length of gaps */
    size_t   gap_count;         /**< Number of gaps */
    
    /* Status */
    uint32_t total_size;        /**< Expected total size */
    uint32_t recovered_size;    /**< Recovered size */
    double   recovery_percent;  /**< Recovery percentage */
    bool     is_complete;       /**< File is complete */
    bool     has_gaps;          /**< File has gaps */
} uft_recovered_file_t;

/**
 * @brief Directory consistency check result
 */
typedef struct {
    bool     valid_signature;   /**< Valid FS signature */
    bool     valid_boot;        /**< Valid boot sector */
    bool     valid_fat;         /**< Valid FAT/BAM */
    bool     valid_root;        /**< Valid root directory */
    
    /* Errors found */
    uint32_t cross_links;       /**< Cross-linked files */
    uint32_t lost_chains;       /**< Lost allocation chains */
    uint32_t invalid_entries;   /**< Invalid directory entries */
    uint32_t bad_blocks_marked; /**< Marked bad blocks */
    uint32_t orphan_blocks;     /**< Orphan blocks */
    
    /* Summary */
    bool     is_consistent;     /**< Overall consistent */
    uint8_t  confidence;        /**< Confidence */
    char    *report;            /**< Detailed report */
} uft_fs_consistency_t;

/**
 * @brief Filesystem recovery context
 */
typedef struct {
    /* Filesystem info */
    uft_filesystem_type_t fs_type;
    char     fs_name[64];
    
    /* Block device */
    const uint8_t *disk_data;
    size_t   disk_size;
    uint16_t block_size;
    uint32_t block_count;
    
    /* Directory */
    uft_dir_entry_t *entries;
    size_t   entry_count;
    
    /* Block allocation */
    uint8_t *block_map;         /**< Block allocation map */
    uft_block_chain_t *chains;  /**< All block chains */
    size_t   chain_count;
    
    /* Consistency */
    uft_fs_consistency_t consistency;
    
    /* Recovered files */
    uft_recovered_file_t *files;
    size_t   file_count;
    
    /* Options */
    bool     read_only;         /**< ALWAYS TRUE! */
    bool     recover_deleted;   /**< Try to recover deleted */
    bool     salvage_fragments; /**< Salvage fragments */
    bool     mark_gaps;         /**< Mark gaps in output */
} uft_fs_recovery_ctx_t;

/* ============================================================================
 * Directory Analysis (Read-Only!)
 * ============================================================================ */

/**
 * @brief Detect filesystem type
 */
uft_filesystem_type_t uft_fs_detect(const uint8_t *disk_data, size_t size);

/**
 * @brief Validate directory structure
 * 
 * Read-Only! Nur Analyse, keine Änderungen.
 */
void uft_fs_validate_directory(const uint8_t *disk_data, size_t size,
                               uft_filesystem_type_t fs_type,
                               uft_fs_consistency_t *result);

/**
 * @brief Read directory entries
 */
size_t uft_fs_read_directory(const uint8_t *disk_data, size_t size,
                             uft_filesystem_type_t fs_type,
                             uft_dir_entry_t **entries);

/**
 * @brief Check entry plausibility
 */
bool uft_fs_check_entry_plausibility(const uft_dir_entry_t *entry,
                                     uint32_t total_blocks);

/**
 * @brief Find deleted entries
 */
size_t uft_fs_find_deleted(const uint8_t *disk_data, size_t size,
                           uft_filesystem_type_t fs_type,
                           uft_dir_entry_t **deleted);

/* ============================================================================
 * Block Chain Reconstruction
 * ============================================================================ */

/**
 * @brief Read block allocation table
 */
bool uft_fs_read_allocation(const uint8_t *disk_data, size_t size,
                            uft_filesystem_type_t fs_type,
                            uint8_t **block_map, size_t *block_count);

/**
 * @brief Trace block chain
 */
bool uft_fs_trace_chain(const uint8_t *disk_data, size_t size,
                        uft_filesystem_type_t fs_type,
                        uint32_t start_block,
                        uft_block_chain_t *chain);

/**
 * @brief Detect chain problems
 */
void uft_fs_analyze_chain(uft_block_chain_t *chain);

/**
 * @brief Find lost chains
 */
size_t uft_fs_find_lost_chains(const uint8_t *disk_data, size_t size,
                               uft_filesystem_type_t fs_type,
                               const uint8_t *block_map,
                               uft_block_chain_t **chains);

/**
 * @brief Reconstruct broken chain
 * 
 * Versucht Kette zu rekonstruieren. Erstellt Hypothese, ändert nichts!
 */
bool uft_fs_reconstruct_chain(const uint8_t *disk_data, size_t size,
                              uft_filesystem_type_t fs_type,
                              const uft_block_chain_t *broken,
                              uft_block_chain_t *reconstructed,
                              double *confidence);

/* ============================================================================
 * Fragment Salvage
 * ============================================================================ */

/**
 * @brief Salvage file fragments
 */
size_t uft_fs_salvage_fragments(const uint8_t *disk_data, size_t size,
                                uft_filesystem_type_t fs_type,
                                const uft_dir_entry_t *entry,
                                uft_fragment_t **fragments);

/**
 * @brief Order fragments
 */
void uft_fs_order_fragments(uft_fragment_t *fragments, size_t count,
                            const uft_dir_entry_t *entry);

/**
 * @brief Merge fragments with gap marking
 */
uft_recovered_file_t* uft_fs_merge_fragments(const uft_fragment_t *fragments,
                                              size_t count,
                                              const uft_dir_entry_t *entry);

/**
 * @brief Find orphan blocks (might be fragments)
 */
size_t uft_fs_find_orphan_blocks(const uint8_t *disk_data, size_t size,
                                 uft_filesystem_type_t fs_type,
                                 const uint8_t *block_map,
                                 uint32_t **orphans);

/* ============================================================================
 * Partial Files
 * ============================================================================ */

/**
 * @brief Recover partial file
 */
uft_recovered_file_t* uft_fs_recover_partial(const uint8_t *disk_data,
                                              size_t size,
                                              uft_filesystem_type_t fs_type,
                                              const uft_dir_entry_t *entry);

/**
 * @brief Mark gaps in file
 */
void uft_fs_mark_gaps(uft_recovered_file_t *file, uint8_t gap_marker);

/**
 * @brief Get gap map
 */
uint8_t* uft_fs_get_gap_map(const uft_recovered_file_t *file,
                            size_t *map_size);

/**
 * @brief Export with gap markers
 */
bool uft_fs_export_with_gaps(const uft_recovered_file_t *file,
                             uint8_t **output, size_t *output_size,
                             uint8_t gap_marker);

/* ============================================================================
 * Read-Only Analysis
 * ============================================================================ */

/**
 * @brief Full filesystem analysis (READ-ONLY!)
 * 
 * Führt komplette Analyse durch ohne etwas zu ändern.
 */
uft_fs_recovery_ctx_t* uft_fs_analyze(const uint8_t *disk_data, size_t size);

/**
 * @brief Get recovery hints from filesystem
 * 
 * Verwendet Filesystem-Info um Sektor-Recovery zu unterstützen.
 */
size_t uft_fs_get_recovery_hints(const uft_fs_recovery_ctx_t *ctx,
                                 uint8_t track, uint8_t sector,
                                 char **hints);

/**
 * @brief Validate sector against filesystem knowledge
 */
bool uft_fs_validate_sector(const uft_fs_recovery_ctx_t *ctx,
                            uint8_t track, uint8_t sector,
                            const uint8_t *sector_data, size_t len);

/**
 * @brief Get expected sector content hints
 */
bool uft_fs_get_sector_hints(const uft_fs_recovery_ctx_t *ctx,
                             uint8_t track, uint8_t sector,
                             uint8_t *expected_start,
                             size_t *expected_len);

/* ============================================================================
 * Filesystem-Specific Recovery
 * ============================================================================ */

/**
 * @brief CBM DOS recovery (D64/D71/D81)
 */
uft_fs_recovery_ctx_t* uft_fs_recover_cbm(const uint8_t *disk_data, size_t size);

/**
 * @brief Amiga OFS/FFS recovery
 */
uft_fs_recovery_ctx_t* uft_fs_recover_amiga(const uint8_t *disk_data, size_t size);

/**
 * @brief FAT12 recovery
 */
uft_fs_recovery_ctx_t* uft_fs_recover_fat12(const uint8_t *disk_data, size_t size);

/**
 * @brief Apple DOS 3.3 recovery
 */
uft_fs_recovery_ctx_t* uft_fs_recover_apple(const uint8_t *disk_data, size_t size);

/**
 * @brief CP/M recovery
 */
uft_fs_recovery_ctx_t* uft_fs_recover_cpm(const uint8_t *disk_data, size_t size);

/* ============================================================================
 * Full Recovery
 * ============================================================================ */

/**
 * @brief Create filesystem recovery context
 */
uft_fs_recovery_ctx_t* uft_fs_recovery_create(const uint8_t *disk_data,
                                               size_t size);

/**
 * @brief Set filesystem type (if known)
 */
void uft_fs_recovery_set_type(uft_fs_recovery_ctx_t *ctx,
                              uft_filesystem_type_t fs_type);

/**
 * @brief Run recovery analysis
 */
void uft_fs_recovery_analyze_full(uft_fs_recovery_ctx_t *ctx);

/**
 * @brief Get recoverable files
 */
size_t uft_fs_recovery_get_files(const uft_fs_recovery_ctx_t *ctx,
                                 uft_recovered_file_t **files);

/**
 * @brief Export recovered file
 */
bool uft_fs_recovery_export_file(const uft_fs_recovery_ctx_t *ctx,
                                 size_t file_index,
                                 uint8_t **data, size_t *size);

/**
 * @brief Generate report
 */
char* uft_fs_recovery_report(const uft_fs_recovery_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_fs_recovery_free(uft_fs_recovery_ctx_t *ctx);

/* ============================================================================
 * Safety
 * ============================================================================ */

/**
 * @brief Ensure read-only mode
 * 
 * Diese Funktion stellt sicher, dass keine Änderungen möglich sind.
 */
static inline void uft_fs_ensure_readonly(uft_fs_recovery_ctx_t *ctx) {
    if (ctx) ctx->read_only = true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_FILESYSTEM_H */
