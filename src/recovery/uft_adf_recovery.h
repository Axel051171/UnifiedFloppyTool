/**
 * @file uft_adf_recovery.h
 * @brief ADF Deleted File Recovery
 * 
 * P3: Tools for recovering deleted files from Amiga disks
 * - Scan for deleted entries
 * - Block chain analysis
 * - Data recovery attempts
 */

#ifndef UFT_ADF_RECOVERY_H
#define UFT_ADF_RECOVERY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery Status
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_RECOVERY_OK = 0,
    UFT_RECOVERY_PARTIAL,       /* Some blocks recovered */
    UFT_RECOVERY_OVERWRITTEN,   /* Blocks have been reused */
    UFT_RECOVERY_CHAIN_BROKEN,  /* Block chain is broken */
    UFT_RECOVERY_FAILED,        /* Recovery not possible */
} uft_recovery_status_t;

typedef enum {
    UFT_ENTRY_RECOVERABLE,      /* Full recovery possible */
    UFT_ENTRY_PARTIAL,          /* Partial recovery only */
    UFT_ENTRY_LOST,             /* Data overwritten */
} uft_entry_recoverability_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char name[256];
    bool is_directory;
    uint32_t header_block;
    uint32_t parent_block;
    uint32_t size;
    int day, month, year;
    int hour, minute, second;
    
    /* Recovery analysis */
    uft_entry_recoverability_t recoverability;
    int blocks_recoverable;
    int blocks_total;
    int blocks_overwritten;
    double recovery_confidence;
} uft_deleted_entry_t;

typedef struct {
    uint32_t block_num;
    bool is_available;          /* Block is free in bitmap */
    bool has_valid_data;        /* Block appears to have data */
    uint32_t next_block;        /* Link to next block in chain */
    uint8_t data[512];
} uft_block_info_t;

typedef struct {
    int total_entries;
    int recoverable_entries;
    int partial_entries;
    int lost_entries;
    uint32_t total_bytes_recoverable;
} uft_recovery_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Scanning API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Scan ADF for deleted entries
 * @param path Path to ADF file
 * @param entries Output array
 * @param max_entries Maximum entries to find
 * @return Number of deleted entries found
 */
int uft_adf_scan_deleted(const char *path, 
                         uft_deleted_entry_t *entries, 
                         int max_entries);

/**
 * @brief Analyze recoverability of a deleted entry
 * @param path Path to ADF file
 * @param entry Entry to analyze (will be updated)
 * @return Recovery status
 */
uft_recovery_status_t uft_adf_analyze_entry(const char *path,
                                            uft_deleted_entry_t *entry);

/**
 * @brief Get recovery statistics
 * @param path Path to ADF file
 * @param stats Output statistics
 * @return 0 on success
 */
int uft_adf_get_recovery_stats(const char *path, 
                               uft_recovery_stats_t *stats);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Recover a deleted file to local filesystem
 * @param path Path to ADF file
 * @param entry Deleted entry to recover
 * @param output_path Local output path
 * @return Recovery status
 */
uft_recovery_status_t uft_adf_recover_file(const char *path,
                                           const uft_deleted_entry_t *entry,
                                           const char *output_path);

/**
 * @brief Undelete file within ADF
 * @param path Path to ADF file (will be modified)
 * @param entry Entry to undelete
 * @return Recovery status
 */
uft_recovery_status_t uft_adf_undelete_file(const char *path,
                                            const uft_deleted_entry_t *entry);

/**
 * @brief Recover all recoverable files
 * @param path Path to ADF file
 * @param output_dir Output directory
 * @param callback Progress callback (can be NULL)
 * @param user_data User data for callback
 * @return Number of files recovered
 */
typedef void (*uft_recovery_progress_fn)(const char *filename, 
                                         int current, int total,
                                         void *user_data);

int uft_adf_recover_all(const char *path,
                        const char *output_dir,
                        uft_recovery_progress_fn callback,
                        void *user_data);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Block Analysis API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get block chain for deleted entry
 * @param path Path to ADF file
 * @param entry Deleted entry
 * @param blocks Output block array
 * @param max_blocks Maximum blocks
 * @return Number of blocks in chain
 */
int uft_adf_get_block_chain(const char *path,
                            const uft_deleted_entry_t *entry,
                            uft_block_info_t *blocks,
                            int max_blocks);

/**
 * @brief Check if block is available (not in use)
 * @param path Path to ADF file
 * @param block_num Block number
 * @return true if block is free
 */
bool uft_adf_is_block_free(const char *path, uint32_t block_num);

/**
 * @brief Read raw block data
 * @param path Path to ADF file
 * @param block_num Block number
 * @param buffer Output buffer (512 bytes)
 * @return 0 on success
 */
int uft_adf_read_raw_block(const char *path, 
                           uint32_t block_num, 
                           uint8_t *buffer);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get recoverability name
 */
const char* uft_recoverability_name(uft_entry_recoverability_t r);

/**
 * @brief Get recovery status name
 */
const char* uft_recovery_status_name(uft_recovery_status_t s);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_RECOVERY_H */
