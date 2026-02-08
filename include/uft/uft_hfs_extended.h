/**
 * @file uft_hfs_extended.h
 * @brief UnifiedFloppyTool - Extended HFS Support
 * @version 3.1.4.008
 *
 * Extended HFS (Hierarchical File System) support with:
 * - B*-tree operations (catalog, extents)
 * - Resource fork parsing
 * - Volume bitmap management
 * - Time conversion (Mac epoch)
 * - Filename comparison (MacOS ordering)
 *
 * Sources analyzed:
 * - hfsutils-master/libhfs/* (Robert Leslie, 1996-1998)
 * - hfsutils-master/librsrc/* (resource fork library)
 */

#ifndef UFT_HFS_EXTENDED_H
#define UFT_HFS_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * HFS Constants
 *============================================================================*/

#define UFT_HFS_SIGWORD       0x4244    /**< 'BD' - HFS signature */
#define UFT_HFS_PLUS_SIGWORD  0x482B    /**< 'H+' - HFS+ signature */
#define UFT_HFS_BLOCKSZ       512       /**< Block size */
#define UFT_HFS_BLOCKSZ_BITS  9         /**< log2(512) */
#define UFT_HFS_MAX_VLEN      27        /**< Max volume name length */
#define UFT_HFS_MAX_FLEN      31        /**< Max filename length */
#define UFT_HFS_MAX_NRECS     35        /**< Max records per B*-tree node */
#define UFT_HFS_MAP1SZ        256       /**< Initial B*-tree map size */
#define UFT_HFS_MAPXSZ        492       /**< Extension map size */

/* Catalog Node IDs */
#define UFT_HFS_CNID_ROOTPAR  1         /**< Parent of root directory */
#define UFT_HFS_CNID_ROOTDIR  2         /**< Root directory */
#define UFT_HFS_CNID_EXTENTS  3         /**< Extents file */
#define UFT_HFS_CNID_CATALOG  4         /**< Catalog file */
#define UFT_HFS_CNID_BADALLOC 5         /**< Bad block file */

/* B*-tree node types */
#define UFT_HFS_ND_INDXNODE   0x00      /**< Index node */
#define UFT_HFS_ND_HDRNODE    0x01      /**< Header node */
#define UFT_HFS_ND_MAPNODE    0x02      /**< Map node */
#define UFT_HFS_ND_LEAFNODE   0xFF      /**< Leaf node */

/* Catalog record types */
#define UFT_HFS_CDREC_DIR     1         /**< Directory record */
#define UFT_HFS_CDREC_FILE    2         /**< File record */
#define UFT_HFS_CDREC_DIRTHR  3         /**< Directory thread */
#define UFT_HFS_CDREC_FILTHR  4         /**< File thread */

/* File attributes */
#define UFT_HFS_FNDR_ISINVISIBLE  0x4000
#define UFT_HFS_FNDR_NAMELOCKED   0x1000
#define UFT_HFS_FNDR_HASBUNDLE    0x2000
#define UFT_HFS_FNDR_ISALIAS      0x8000

/* Volume attributes */
#define UFT_HFS_ATRB_HLOCKED     0x0080  /**< Hardware locked */
#define UFT_HFS_ATRB_UNMOUNTED   0x0100  /**< Cleanly unmounted */
#define UFT_HFS_ATRB_BBSPARED    0x0200  /**< Bad blocks spared */
#define UFT_HFS_ATRB_SLOCKED     0x8000  /**< Software locked */

/* Mac epoch: Jan 1, 1904 00:00:00 UTC */
#define UFT_HFS_TIMEDIFF  2082844800UL

/*============================================================================
 * Big-Endian Data Marshalling
 *============================================================================*/

static inline uint16_t uft_hfs_get_u16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t uft_hfs_get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static inline int16_t uft_hfs_get_s16(const uint8_t *p)
{
    return (int16_t)uft_hfs_get_u16(p);
}

static inline int32_t uft_hfs_get_s32(const uint8_t *p)
{
    return (int32_t)uft_hfs_get_u32(p);
}

static inline void uft_hfs_put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

static inline void uft_hfs_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

/*============================================================================
 * HFS Filename Comparison (MacOS character ordering)
 *============================================================================*/

/**
 * @brief MacOS HFS character ordering table
 */
extern const uint8_t uft_hfs_char_order[256];

/**
 * @brief Compare strings using MacOS HFS ordering
 */
int uft_hfs_relstring(const char *s1, const char *s2);

/*============================================================================
 * Time Conversion
 *============================================================================*/

/**
 * @brief Convert Mac time to Unix time
 */
static inline time_t uft_hfs_mac_to_unix(uint32_t mac_time)
{
    return (time_t)(mac_time - UFT_HFS_TIMEDIFF);
}

/**
 * @brief Convert Unix time to Mac time
 */
static inline uint32_t uft_hfs_unix_to_mac(time_t unix_time)
{
    return (uint32_t)(unix_time + UFT_HFS_TIMEDIFF);
}

/*============================================================================
 * Extent Descriptor
 *============================================================================*/

typedef struct {
    uint16_t start_block;   /**< First allocation block */
    uint16_t num_blocks;    /**< Number of allocation blocks */
} uft_hfs_extent_t;

typedef uft_hfs_extent_t uft_hfs_extent_rec_t[3];

/*============================================================================
 * Master Directory Block (MDB)
 *============================================================================*/

typedef struct {
    uint16_t signature;         /**< 0x4244 'BD' */
    uint32_t create_date;       /**< Volume creation date */
    uint32_t modify_date;       /**< Last modification date */
    uint16_t attributes;        /**< Volume attributes */
    uint16_t num_root_files;    /**< Files in root directory */
    uint16_t bitmap_start;      /**< First block of volume bitmap */
    uint16_t alloc_ptr;         /**< Start of next allocation search */
    uint16_t num_alloc_blocks;  /**< Number of allocation blocks */
    uint32_t alloc_block_size;  /**< Size of allocation blocks */
    uint32_t clump_size;        /**< Default clump size */
    uint16_t alloc_start;       /**< First allocation block in volume */
    uint32_t next_cnid;         /**< Next unused catalog node ID */
    uint16_t free_blocks;       /**< Number of free allocation blocks */
    char     volume_name[28];   /**< Volume name (Pascal string) */
    uint32_t backup_date;       /**< Last backup date */
    uint16_t backup_seq;        /**< Backup sequence number */
    uint32_t write_count;       /**< Volume write count */
    uint32_t xt_clump_size;     /**< Extents file clump size */
    uint32_t ct_clump_size;     /**< Catalog file clump size */
    uint16_t num_root_dirs;     /**< Directories in root */
    uint32_t file_count;        /**< Total files in volume */
    uint32_t dir_count;         /**< Total directories in volume */
    uint32_t finder_info[8];    /**< Finder information */
    uint16_t embed_sig;         /**< Embedded volume signature */
    uft_hfs_extent_t embed_extent; /**< Embedded volume extent */
    uint32_t xt_file_size;      /**< Extents file size */
    uft_hfs_extent_rec_t xt_extent_rec; /**< Extents file first 3 extents */
    uint32_t ct_file_size;      /**< Catalog file size */
    uft_hfs_extent_rec_t ct_extent_rec; /**< Catalog file first 3 extents */
} uft_hfs_mdb_t;

/*============================================================================
 * B*-Tree Structures
 *============================================================================*/

/**
 * @brief B*-tree node descriptor
 */
typedef struct {
    uint32_t flink;         /**< Forward link */
    uint32_t blink;         /**< Backward link */
    int8_t   type;          /**< Node type */
    int8_t   height;        /**< Node height */
    uint16_t num_recs;      /**< Number of records */
    int16_t  reserved;
} uft_hfs_btree_node_t;

/**
 * @brief B*-tree header record
 */
typedef struct {
    uint16_t depth;         /**< Tree depth */
    uint32_t root;          /**< Root node number */
    uint32_t num_recs;      /**< Total records in tree */
    uint32_t first_leaf;    /**< First leaf node */
    uint32_t last_leaf;     /**< Last leaf node */
    uint16_t node_size;     /**< Node size (always 512) */
    uint16_t max_key_len;   /**< Maximum key length */
    uint32_t total_nodes;   /**< Total nodes in tree */
    uint32_t free_nodes;    /**< Free nodes */
} uft_hfs_btree_header_t;

/*============================================================================
 * Catalog Key and Records
 *============================================================================*/

/**
 * @brief Catalog key
 */
typedef struct {
    uint8_t  key_len;       /**< Key length */
    uint8_t  reserved;
    uint32_t parent_id;     /**< Parent directory CNID */
    uint8_t  name_len;      /**< Name length */
    char     name[31];      /**< Filename */
} uft_hfs_cat_key_t;

/**
 * @brief Finder info for files
 */
typedef struct {
    char     type[4];       /**< File type */
    char     creator[4];    /**< Creator code */
    uint16_t flags;         /**< Finder flags */
    int16_t  location_v;    /**< Vertical position */
    int16_t  location_h;    /**< Horizontal position */
    int16_t  folder_id;     /**< Folder ID */
} uft_hfs_fndr_file_t;

/**
 * @brief Finder info for directories
 */
typedef struct {
    int16_t  rect[4];       /**< Folder rectangle */
    uint16_t flags;         /**< Finder flags */
    int16_t  location_v;    /**< Vertical position */
    int16_t  location_h;    /**< Horizontal position */
    int16_t  reserved;
} uft_hfs_fndr_dir_t;

/**
 * @brief Catalog directory record
 */
typedef struct {
    int16_t  record_type;   /**< Always 1 */
    uint16_t flags;
    uint16_t valence;       /**< Items in directory */
    uint32_t dir_id;        /**< Directory CNID */
    uint32_t create_date;
    uint32_t modify_date;
    uint32_t backup_date;
    uft_hfs_fndr_dir_t finder_info;
} uft_hfs_cat_dir_t;

/**
 * @brief Catalog file record
 */
typedef struct {
    int16_t  record_type;   /**< Always 2 */
    uint8_t  flags;
    uint8_t  file_type;
    uft_hfs_fndr_file_t finder_info;
    uint32_t file_id;       /**< File CNID */
    uint16_t data_start;    /**< First data fork alloc block */
    uint32_t data_leof;     /**< Data fork logical EOF */
    uint32_t data_peof;     /**< Data fork physical EOF */
    uint16_t rsrc_start;    /**< First resource fork alloc block */
    uint32_t rsrc_leof;     /**< Resource fork logical EOF */
    uint32_t rsrc_peof;     /**< Resource fork physical EOF */
    uint32_t create_date;
    uint32_t modify_date;
    uint32_t backup_date;
    uft_hfs_extent_rec_t data_extents;
    uft_hfs_extent_rec_t rsrc_extents;
    uint32_t reserved;
} uft_hfs_cat_file_t;

/**
 * @brief Catalog thread record
 */
typedef struct {
    int16_t  record_type;   /**< 3=dir thread, 4=file thread */
    uint32_t reserved[2];
    uint32_t parent_id;     /**< Parent directory CNID */
    uint8_t  name_len;
    char     name[31];      /**< Filename */
} uft_hfs_cat_thread_t;

/*============================================================================
 * Resource Fork Structures
 *============================================================================*/

/**
 * @brief Resource fork header
 */
typedef struct {
    uint32_t data_offset;   /**< Offset to resource data */
    uint32_t map_offset;    /**< Offset to resource map */
    uint32_t data_length;   /**< Length of resource data */
    uint32_t map_length;    /**< Length of resource map */
} uft_rsrc_header_t;

/**
 * @brief Resource map header
 */
typedef struct {
    uint16_t attributes;    /**< Resource file attributes */
    uint16_t type_offset;   /**< Offset to type list */
    uint16_t name_offset;   /**< Offset to name list */
    int16_t  num_types;     /**< Number of types minus 1 */
} uft_rsrc_map_t;

/**
 * @brief Resource type entry
 */
typedef struct {
    char     type[4];       /**< Resource type (e.g., 'CODE', 'ICON') */
    int16_t  count;         /**< Number of resources minus 1 */
    uint16_t ref_offset;    /**< Offset to reference list */
} uft_rsrc_type_t;

/**
 * @brief Resource reference entry
 */
typedef struct {
    int16_t  id;            /**< Resource ID */
    int16_t  name_offset;   /**< Offset to name (-1 if none) */
    uint8_t  attributes;    /**< Resource attributes */
    uint32_t data_offset;   /**< Offset to data (24-bit) + attrs */
    uint32_t handle;        /**< Reserved */
} uft_rsrc_ref_t;

/*============================================================================
 * Volume Context
 *============================================================================*/

typedef struct uft_hfs_vol uft_hfs_vol_t;
typedef struct uft_hfs_file uft_hfs_file_t;
typedef struct uft_hfs_dir uft_hfs_dir_t;

/**
 * @brief Volume entry information
 */
typedef struct {
    char     name[28];      /**< Volume name */
    uint16_t flags;         /**< Volume flags */
    uint32_t total_bytes;   /**< Total bytes */
    uint32_t free_bytes;    /**< Free bytes */
    uint32_t alloc_size;    /**< Allocation block size */
    uint32_t clump_size;    /**< Clump size */
    uint32_t num_files;     /**< Number of files */
    uint32_t num_dirs;      /**< Number of directories */
    time_t   create_date;   /**< Creation date */
    time_t   modify_date;   /**< Modification date */
    time_t   backup_date;   /**< Backup date */
    uint32_t blessed;       /**< Blessed folder CNID */
} uft_hfs_vol_entry_t;

/**
 * @brief File/directory entry information
 */
typedef struct {
    char     name[32];      /**< Filename */
    uint32_t cnid;          /**< Catalog node ID */
    uint32_t parent_cnid;   /**< Parent directory CNID */
    bool     is_dir;        /**< True if directory */
    char     type[5];       /**< File type + null */
    char     creator[5];    /**< Creator code + null */
    uint16_t flags;         /**< Finder flags */
    uint32_t data_size;     /**< Data fork size */
    uint32_t rsrc_size;     /**< Resource fork size */
    time_t   create_date;
    time_t   modify_date;
    time_t   backup_date;
} uft_hfs_entry_t;

/*============================================================================
 * Volume API
 *============================================================================*/

/**
 * @brief Mount HFS volume from image
 */
uft_hfs_vol_t* uft_hfs_mount(const uint8_t *image, size_t image_size,
                              int partition, bool readonly);

/**
 * @brief Unmount HFS volume
 */
int uft_hfs_umount(uft_hfs_vol_t *vol);

/**
 * @brief Flush pending changes
 */
int uft_hfs_flush(uft_hfs_vol_t *vol);

/**
 * @brief Get volume statistics
 */
int uft_hfs_vstat(uft_hfs_vol_t *vol, uft_hfs_vol_entry_t *entry);

/**
 * @brief Change current directory
 */
int uft_hfs_chdir(uft_hfs_vol_t *vol, const char *path);

/**
 * @brief Get current directory path
 */
int uft_hfs_getcwd(uft_hfs_vol_t *vol, char *buf, size_t size);

/*============================================================================
 * Directory API
 *============================================================================*/

/**
 * @brief Open directory for reading
 */
uft_hfs_dir_t* uft_hfs_opendir(uft_hfs_vol_t *vol, const char *path);

/**
 * @brief Read next directory entry
 */
int uft_hfs_readdir(uft_hfs_dir_t *dir, uft_hfs_entry_t *entry);

/**
 * @brief Close directory
 */
int uft_hfs_closedir(uft_hfs_dir_t *dir);

/**
 * @brief Get file/directory info by path
 */
int uft_hfs_stat(uft_hfs_vol_t *vol, const char *path, uft_hfs_entry_t *entry);

/*============================================================================
 * File API
 *============================================================================*/

/**
 * @brief Open file for reading
 */
uft_hfs_file_t* uft_hfs_open(uft_hfs_vol_t *vol, const char *path);

/**
 * @brief Read from data fork
 */
size_t uft_hfs_read(uft_hfs_file_t *file, void *buf, size_t size);

/**
 * @brief Read from resource fork
 */
size_t uft_hfs_read_rsrc(uft_hfs_file_t *file, void *buf, size_t size);

/**
 * @brief Seek in data fork
 */
int uft_hfs_seek(uft_hfs_file_t *file, long offset, int whence);

/**
 * @brief Close file
 */
int uft_hfs_close(uft_hfs_file_t *file);

/*============================================================================
 * Resource Fork API
 *============================================================================*/

/**
 * @brief Parse resource fork
 */
int uft_rsrc_parse(const uint8_t *data, size_t size,
                   void (*callback)(const char *type, int16_t id,
                                   const char *name, const uint8_t *data,
                                   size_t size, void *user),
                   void *user);

/**
 * @brief Count resources of type
 */
int uft_rsrc_count(const uint8_t *data, size_t size, const char *type);

/**
 * @brief Get resource by type and ID
 */
const uint8_t* uft_rsrc_get(const uint8_t *data, size_t size,
                            const char *type, int16_t id,
                            size_t *out_size);

/**
 * @brief Get resource by type and name
 */
const uint8_t* uft_rsrc_get_named(const uint8_t *data, size_t size,
                                   const char *type, const char *name,
                                   size_t *out_size);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Parse MDB from raw block
 */
bool uft_hfs_parse_mdb(const uint8_t *block, uft_hfs_mdb_t *mdb);

/**
 * @brief Validate HFS signature
 */
bool uft_hfs_is_valid(const uint8_t *block);

/**
 * @brief Validate HFS+ signature
 */
bool uft_hfs_plus_is_valid(const uint8_t *block);

/**
 * @brief Find partition in Apple Partition Map
 */
int uft_hfs_find_partition(const uint8_t *image, size_t size,
                           int index, uint32_t *start, uint32_t *length);

/**
 * @brief Count HFS partitions in image
 */
int uft_hfs_count_partitions(const uint8_t *image, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFS_EXTENDED_H */
