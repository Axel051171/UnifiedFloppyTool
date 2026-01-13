/**
 * @file uft_hdf_parser.h
 * @brief HDF (Hard Disk File) Parser with RDB Support
 * 
 * P1: Supports Amiga hard disk images with Rigid Disk Block
 * Can parse partitioned HDFs with multiple volumes
 */

#ifndef UFT_HDF_PARSER_H
#define UFT_HDF_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_RDB_MAGIC           0x5244534B  /* "RDSK" */
#define UFT_PART_MAGIC          0x50415254  /* "PART" */
#define UFT_FSHD_MAGIC          0x46534844  /* "FSHD" */
#define UFT_LSEG_MAGIC          0x4C534547  /* "LSEG" */
#define UFT_BOOT_MAGIC          0x424F4F54  /* "BOOT" */

#define UFT_HDF_BLOCK_SIZE      512
#define UFT_HDF_MAX_PARTITIONS  16
#define UFT_HDF_MAX_NAME_LEN    32

/* Filesystem Types */
#define UFT_DOS_OFS     0x444F5300  /* DOS\0 - OFS */
#define UFT_DOS_FFS     0x444F5301  /* DOS\1 - FFS */
#define UFT_DOS_OFS_I   0x444F5302  /* DOS\2 - OFS Intl */
#define UFT_DOS_FFS_I   0x444F5303  /* DOS\3 - FFS Intl */
#define UFT_DOS_OFS_DC  0x444F5304  /* DOS\4 - OFS DirCache */
#define UFT_DOS_FFS_DC  0x444F5305  /* DOS\5 - FFS DirCache */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Drive Geometry
 */
typedef struct {
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint32_t block_size;
    uint32_t total_blocks;
    uint64_t total_bytes;
} uft_hdf_geometry_t;

/**
 * @brief Partition Information
 */
typedef struct {
    char name[UFT_HDF_MAX_NAME_LEN];
    uint32_t dos_type;              /* DOS\0, DOS\1, etc. */
    uint32_t start_cylinder;
    uint32_t end_cylinder;
    uint32_t start_block;
    uint32_t end_block;
    uint32_t num_blocks;
    uint64_t size_bytes;
    uint32_t block_size;
    uint32_t root_block;
    bool bootable;
    uint8_t boot_priority;
    uint32_t reserved_begin;        /* Reserved blocks at start */
    uint32_t reserved_end;          /* Reserved blocks at end */
    
    /* Calculated */
    const char *fs_type_name;
} uft_hdf_partition_t;

/**
 * @brief Rigid Disk Block Information
 */
typedef struct {
    bool valid;
    uint32_t host_id;
    uint32_t block_bytes;
    uint32_t flags;
    
    /* Bad block list */
    uint32_t bad_block_list;
    
    /* Partition list */
    uint32_t partition_list;
    
    /* Filesystem header list */
    uint32_t fs_header_list;
    
    /* Drive init code */
    uint32_t drive_init;
    
    /* Boot block list */
    uint32_t boot_block_list;
    
    /* Physical drive parameters */
    uint32_t cylinders;
    uint32_t sectors;
    uint32_t heads;
    uint32_t interleave;
    uint32_t park_cylinder;
    uint32_t write_precomp;
    uint32_t reduced_write;
    uint32_t step_rate;
    
    /* Logical drive parameters */
    uint32_t rdb_blocks_lo;
    uint32_t rdb_blocks_hi;
    uint32_t lo_cylinder;
    uint32_t hi_cylinder;
    uint32_t cyl_blocks;
    uint32_t auto_park_seconds;
    uint32_t high_rdsk_block;
    
    /* Drive identification */
    char disk_vendor[8];
    char disk_product[16];
    char disk_revision[4];
    char controller_vendor[8];
    char controller_product[16];
    char controller_revision[4];
} uft_rdb_info_t;

/**
 * @brief Complete HDF Information
 */
typedef struct {
    char filename[256];
    uft_hdf_geometry_t geometry;
    bool has_rdb;
    uft_rdb_info_t rdb;
    int num_partitions;
    uft_hdf_partition_t partitions[UFT_HDF_MAX_PARTITIONS];
} uft_hdf_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if file is an HDF
 */
bool uft_hdf_is_valid(const char *path);

/**
 * @brief Check if file has RDB (Rigid Disk Block)
 */
bool uft_hdf_has_rdb(const char *path);

/**
 * @brief Parse HDF file and extract all information
 */
int uft_hdf_parse(const char *path, uft_hdf_info_t *info);

/**
 * @brief Parse RDB only
 */
int uft_hdf_parse_rdb(const uint8_t *data, size_t len, uft_rdb_info_t *rdb);

/**
 * @brief Parse partition entry
 */
int uft_hdf_parse_partition(const uint8_t *data, size_t len, 
                            uft_hdf_partition_t *part);

/**
 * @brief Get filesystem type name
 */
const char* uft_hdf_fs_type_name(uint32_t dos_type);

/**
 * @brief Read a block from HDF
 */
int uft_hdf_read_block(const char *path, uint32_t block, uint8_t *buffer);

/**
 * @brief Read blocks from a partition
 */
int uft_hdf_read_partition_block(const char *path, 
                                 const uft_hdf_partition_t *part,
                                 uint32_t rel_block, uint8_t *buffer);

/**
 * @brief Print HDF info to string
 */
int uft_hdf_info_to_string(const uft_hdf_info_t *info, char *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HDF_PARSER_H */
