/**
 * @file uft_amigados_extended.h
 * @brief Extended AmigaDOS Operations (inspired by amitools xdftool)
 * 
 * P2: Advanced AmigaDOS filesystem operations
 * 
 * Features from amitools:
 * - Volume-level operations (format, relabel)
 * - Pattern matching for file operations
 * - Recursive directory operations
 * - File packing/unpacking to host filesystem
 * - Bootblock operations
 * - Disk validation and repair
 * - LHA archive extraction (on-disk)
 */

#ifndef UFT_AMIGADOS_EXTENDED_H
#define UFT_AMIGADOS_EXTENDED_H

#include "uft/fs/uft_amigados.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Volume Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Volume information structure
 */
typedef struct {
    char name[32];              /**< Volume name */
    char fs_type[16];           /**< Filesystem type string */
    uint32_t dos_type;          /**< DOS type (DOS0-DOS7) */
    uint32_t total_blocks;      /**< Total blocks */
    uint32_t used_blocks;       /**< Used blocks */
    uint32_t free_blocks;       /**< Free blocks */
    uint64_t total_bytes;       /**< Total size in bytes */
    uint64_t used_bytes;        /**< Used size in bytes */
    uint64_t free_bytes;        /**< Free size in bytes */
    uint32_t num_files;         /**< Number of files */
    uint32_t num_dirs;          /**< Number of directories */
    time_t creation_date;       /**< Creation date */
    time_t modification_date;   /**< Last modification */
    bool is_bootable;           /**< Has valid bootblock */
    uint8_t boot_checksum;      /**< Bootblock checksum valid */
} uft_amiga_volume_info_t;

/**
 * @brief Get volume information
 */
int uft_amiga_get_volume_info(uft_amigados_ctx_t *ctx, 
                               uft_amiga_volume_info_t *info);

/**
 * @brief Relabel volume (change disk name)
 */
int uft_amiga_relabel(uft_amigados_ctx_t *ctx, const char *new_name);

/**
 * @brief Format disk with specified filesystem type
 * @param dos_type: 0=OFS, 1=FFS, 2=OFS-INTL, 3=FFS-INTL, etc.
 */
int uft_amiga_format(uft_amigados_ctx_t *ctx, const char *name, 
                      uint8_t dos_type, bool install_boot);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pattern Matching Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief File match callback
 */
typedef int (*uft_amiga_match_cb)(const uft_amiga_dir_entry_t *entry,
                                   const char *full_path,
                                   void *user_data);

/**
 * @brief Find files matching Amiga-style pattern
 * Supports: ? (single char), * (multiple chars), #? (same as *)
 */
int uft_amiga_find_pattern(uft_amigados_ctx_t *ctx,
                            const char *pattern,
                            bool recursive,
                            uft_amiga_match_cb callback,
                            void *user_data);

/**
 * @brief List all files recursively
 */
int uft_amiga_list_all(uft_amigados_ctx_t *ctx,
                        uft_amiga_match_cb callback,
                        void *user_data);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pack/Unpack Operations (Host Filesystem <-> Disk Image)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Pack options
 */
typedef struct {
    bool recursive;             /**< Include subdirectories */
    bool preserve_dates;        /**< Preserve file dates */
    bool preserve_comments;     /**< Preserve Amiga comments */
    bool preserve_protection;   /**< Preserve protection bits */
    const char *exclude_pattern;/**< Exclude pattern (NULL for none) */
} uft_amiga_pack_opts_t;

/**
 * @brief Unpack disk/directory to host filesystem
 */
int uft_amiga_unpack_to_host(uft_amigados_ctx_t *ctx,
                              const char *amiga_path,
                              const char *host_path,
                              const uft_amiga_pack_opts_t *opts);

/**
 * @brief Pack host directory into disk image
 */
int uft_amiga_pack_from_host(uft_amigados_ctx_t *ctx,
                              const char *host_path,
                              const char *amiga_path,
                              const uft_amiga_pack_opts_t *opts);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bootblock Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Bootblock info
 */
typedef struct {
    bool valid;                 /**< Has valid bootblock */
    uint32_t dos_type;          /**< DOS type from bootblock */
    uint32_t checksum;          /**< Bootblock checksum */
    bool checksum_valid;        /**< Checksum is correct */
    bool has_virus;             /**< Known virus signature detected */
    char virus_name[32];        /**< Virus name if detected */
    uint8_t raw[1024];          /**< Raw bootblock data */
} uft_amiga_bootblock_t;

/**
 * @brief Read bootblock
 */
int uft_amiga_read_bootblock(uft_amigados_ctx_t *ctx, 
                              uft_amiga_bootblock_t *boot);

/**
 * @brief Write bootblock (install standard boot code)
 */
int uft_amiga_install_bootblock(uft_amigados_ctx_t *ctx, uint32_t dos_type);

/**
 * @brief Clear bootblock (make non-bootable)
 */
int uft_amiga_clear_bootblock(uft_amigados_ctx_t *ctx);

/**
 * @brief Check for known boot viruses
 */
int uft_amiga_check_boot_virus(const uft_amiga_bootblock_t *boot);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation & Repair
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validation result
 */
typedef struct {
    bool valid;                 /**< Disk is valid */
    int error_count;            /**< Number of errors */
    int warning_count;          /**< Number of warnings */
    
    /* Specific checks */
    bool rootblock_valid;       /**< Root block valid */
    bool bitmap_valid;          /**< Bitmap consistent */
    bool directory_valid;       /**< Directory structure valid */
    bool files_valid;           /**< All file chains valid */
    
    /* Statistics */
    int orphan_blocks;          /**< Blocks not in bitmap or files */
    int crosslinked_blocks;     /**< Blocks used by multiple files */
    int bad_checksums;          /**< Blocks with bad checksums */
    
    /* Details buffer */
    char details[4096];         /**< Detailed error messages */
} uft_amiga_validate_result_t;

/**
 * @brief Validate disk structure
 */
int uft_amiga_validate(uft_amigados_ctx_t *ctx, 
                        uft_amiga_validate_result_t *result);

/**
 * @brief Repair bitmap (recalculate from directory)
 */
int uft_amiga_repair_bitmap(uft_amigados_ctx_t *ctx);

/**
 * @brief Salvage files from damaged disk
 */
int uft_amiga_salvage(uft_amigados_ctx_t *ctx,
                       const char *output_dir,
                       int *files_recovered);

/* ═══════════════════════════════════════════════════════════════════════════════
 * LHA Archive Support (On-Disk)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief LHA archive info
 */
typedef struct {
    char filename[108];         /**< Archive filename on disk */
    int num_files;              /**< Number of files in archive */
    uint64_t compressed_size;   /**< Total compressed size */
    uint64_t uncompressed_size; /**< Total uncompressed size */
} uft_amiga_lha_info_t;

/**
 * @brief List contents of LHA archive on disk
 */
int uft_amiga_lha_list(uft_amigados_ctx_t *ctx,
                        const char *archive_path,
                        uft_amiga_match_cb callback,
                        void *user_data);

/**
 * @brief Extract LHA archive to directory on same disk
 */
int uft_amiga_lha_extract(uft_amigados_ctx_t *ctx,
                           const char *archive_path,
                           const char *dest_path);

/**
 * @brief Extract LHA archive to host filesystem
 */
int uft_amiga_lha_extract_to_host(uft_amigados_ctx_t *ctx,
                                   const char *archive_path,
                                   const char *host_path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Batch Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Batch operation type
 */
typedef enum {
    UFT_AMIGA_BATCH_LIST,       /**< List files */
    UFT_AMIGA_BATCH_EXTRACT,    /**< Extract files */
    UFT_AMIGA_BATCH_DELETE,     /**< Delete files */
    UFT_AMIGA_BATCH_VALIDATE    /**< Validate disk */
} uft_amiga_batch_op_t;

/**
 * @brief Batch progress callback
 */
typedef void (*uft_amiga_batch_progress_cb)(const char *current_file,
                                             int current,
                                             int total,
                                             void *user_data);

/**
 * @brief Process multiple disk images
 */
int uft_amiga_batch_process(const char **paths,
                             int num_paths,
                             uft_amiga_batch_op_t operation,
                             const char *output_dir,
                             uft_amiga_batch_progress_cb progress,
                             void *user_data);

/**
 * @brief Scan directory for ADF/HDF files
 */
int uft_amiga_scan_directory(const char *dir_path,
                              bool recursive,
                              char ***found_paths,
                              int *num_found);

/**
 * @brief Free scan results
 */
void uft_amiga_free_scan_results(char **paths, int num_paths);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGADOS_EXTENDED_H */
