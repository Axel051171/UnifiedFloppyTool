/**
 * @file uft_hfs.h
 * @brief Nintendo Switch HFS0 (Hash File System) format definitions
 * @version 1.0.0
 * 
 * Extracted and adapted from nxdumptool (GPL-3.0)
 * Original: Copyright (c) 2020-2024, DarkMatterCore <pabloacurielz@gmail.com>
 * 
 * HFS0 is used in XCI files for partition organization:
 *   - Root partition contains: update, logo (optional), normal, secure
 *   - Each sub-partition contains NCA files
 * 
 * For use in UFI (Universal Flux Interface) / UFT (Universal Flux Tool)
 */

#ifndef UFT_HFS_H
#define UFT_HFS_H

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

#define HFS0_MAGIC          0x48465330  /* "HFS0" */
#define HFS0_HEADER_SIZE    0x10        /* Base header size */
#define HFS0_ENTRY_SIZE     0x40        /* Entry size */

#define SHA256_HASH_SIZE    32

/*============================================================================
 * HFS0 Partition Types (for XCI)
 *============================================================================*/

typedef enum {
    HFS_PARTITION_NONE   = 0,
    HFS_PARTITION_ROOT   = 1,   /* Contains other partitions */
    HFS_PARTITION_UPDATE = 2,   /* System update data */
    HFS_PARTITION_LOGO   = 3,   /* Game logo (since HOS 4.0) */
    HFS_PARTITION_NORMAL = 4,   /* Normal content */
    HFS_PARTITION_SECURE = 5,   /* Encrypted game data */
    HFS_PARTITION_COUNT  = 6
} hfs_partition_type_t;

/*============================================================================
 * HFS0 Header (at start of each partition)
 *============================================================================*/

typedef struct {
    uint32_t magic;             /* "HFS0" (0x48465330) */
    uint32_t entry_count;       /* Number of files in partition */
    uint32_t name_table_size;   /* Size of name table after entries */
    uint8_t reserved[0x4];
} hfs_header_t;

_Static_assert(sizeof(hfs_header_t) == 0x10, "hfs_header_t size mismatch");

/*============================================================================
 * HFS0 Entry (one per file)
 *============================================================================*/

typedef struct {
    uint64_t offset;            /* File offset (relative to end of header+names) */
    uint64_t size;              /* File size */
    uint32_t name_offset;       /* Offset into name table */
    uint32_t hash_target_size;  /* Size of hashed region */
    uint64_t hash_target_offset;/* Offset of hashed region */
    uint8_t hash[SHA256_HASH_SIZE]; /* SHA-256 of hash target */
} hfs_entry_t;

_Static_assert(sizeof(hfs_entry_t) == 0x40, "hfs_entry_t size mismatch");

/*============================================================================
 * HFS0 Context (runtime structure)
 *============================================================================*/

typedef struct {
    hfs_partition_type_t type;
    char name[64];              /* Partition name */
    uint64_t offset;            /* Absolute offset in XCI */
    uint64_t size;              /* Total partition size */
    uint64_t header_size;       /* Header + entries + names */
    uint64_t data_offset;       /* Start of file data (absolute) */
    uint32_t entry_count;       /* Number of entries */
    uint32_t name_table_size;   /* Name table size */
    uint8_t *header_data;       /* Raw header data (dynamically allocated) */
    bool valid;
} hfs_context_t;

/*============================================================================
 * HFS Entry Info (for enumeration)
 *============================================================================*/

typedef struct {
    char name[256];
    uint64_t offset;            /* Absolute offset */
    uint64_t size;
    uint8_t hash[SHA256_HASH_SIZE];
    bool hash_valid;
} hfs_entry_info_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Validate HFS0 header magic
 */
bool hfs_header_valid(const hfs_header_t *hdr);

/**
 * Calculate total header size (header + entries + name table)
 */
uint64_t hfs_calc_header_size(const hfs_header_t *hdr);

/**
 * Get partition type as string
 */
const char* hfs_partition_type_str(hfs_partition_type_t type);

/**
 * Get partition type from name
 */
hfs_partition_type_t hfs_partition_type_from_name(const char *name);

/**
 * Initialize HFS context from raw data
 * @param ctx Context to initialize
 * @param data Raw partition data starting at HFS header
 * @param data_size Size of data available
 * @param abs_offset Absolute offset in XCI
 * @param type Partition type
 * @return true on success
 */
bool hfs_init_context(hfs_context_t *ctx, const uint8_t *data, size_t data_size,
                      uint64_t abs_offset, hfs_partition_type_t type);

/**
 * Free HFS context resources
 */
void hfs_free_context(hfs_context_t *ctx);

/**
 * Get entry count
 */
uint32_t hfs_get_entry_count(const hfs_context_t *ctx);

/**
 * Get entry info by index
 * @param ctx HFS context
 * @param idx Entry index
 * @param info Output entry info
 * @return true on success
 */
bool hfs_get_entry_info(const hfs_context_t *ctx, uint32_t idx, hfs_entry_info_t *info);

/**
 * Find entry by name
 * @param ctx HFS context
 * @param name Entry name to find
 * @param info Output entry info
 * @return true if found
 */
bool hfs_find_entry(const hfs_context_t *ctx, const char *name, hfs_entry_info_t *info);

/**
 * Calculate total data size of all entries
 */
bool hfs_get_total_data_size(const hfs_context_t *ctx, uint64_t *out_size);

/**
 * Verify entry hash
 * @param ctx HFS context
 * @param idx Entry index
 * @param data Entry data
 * @param data_size Size of data
 * @return true if hash matches
 */
bool hfs_verify_entry_hash(const hfs_context_t *ctx, uint32_t idx,
                           const uint8_t *data, size_t data_size);

/**
 * Print HFS partition info
 */
void hfs_print_info(const hfs_context_t *ctx);

/**
 * List all entries in partition
 */
void hfs_list_entries(const hfs_context_t *ctx);

/*============================================================================
 * Inline Helper Functions
 *============================================================================*/

/**
 * Get entry from raw header data by index
 */
static inline const hfs_entry_t* hfs_get_entry_raw(const uint8_t *header_data, uint32_t idx) {
    if (!header_data) return NULL;
    const hfs_header_t *hdr = (const hfs_header_t*)header_data;
    if (idx >= hdr->entry_count) return NULL;
    return (const hfs_entry_t*)(header_data + sizeof(hfs_header_t) + (idx * sizeof(hfs_entry_t)));
}

/**
 * Get name table from raw header data
 */
static inline const char* hfs_get_name_table_raw(const uint8_t *header_data) {
    if (!header_data) return NULL;
    const hfs_header_t *hdr = (const hfs_header_t*)header_data;
    return (const char*)(header_data + sizeof(hfs_header_t) + (hdr->entry_count * sizeof(hfs_entry_t)));
}

/**
 * Get entry name from raw header data
 */
static inline const char* hfs_get_entry_name_raw(const uint8_t *header_data, const hfs_entry_t *entry) {
    if (!header_data || !entry) return NULL;
    const hfs_header_t *hdr = (const hfs_header_t*)header_data;
    if (entry->name_offset >= hdr->name_table_size) return NULL;
    const char *name_table = hfs_get_name_table_raw(header_data);
    return name_table + entry->name_offset;
}

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* UFT_HFS_H */
