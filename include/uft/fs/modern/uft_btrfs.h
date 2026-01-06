/**
 * @file uft_btrfs.h
 * @brief Btrfs Filesystem Structures
 * 
 * Based on btrfscue by Christian Blichmann
 * License: BSD-2-Clause
 */

#ifndef UFT_BTRFS_H
#define UFT_BTRFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Magic: "_BHRfS_M" in little-endian */
#define UFT_BTRFS_MAGIC             0x4D5F53665248425FLL

/** Default block size (4 * 4096 = 16384) */
#define UFT_BTRFS_DEFAULT_BLOCK_SIZE    16384
#define UFT_BTRFS_PAGE_SIZE             4096

/** Superblock copy offsets */
#define UFT_BTRFS_SUPER_OFFSET1     0x10000         /**< 64 KiB */
#define UFT_BTRFS_SUPER_OFFSET2     0x4000000       /**< 64 MiB */
#define UFT_BTRFS_SUPER_OFFSET3     0x4000000000LL  /**< 256 GiB */
#define UFT_BTRFS_SUPER_OFFSET4     0x4000000000000LL /**< 1 PiB */

/** Fixed sizes */
#define UFT_BTRFS_CSUM_SIZE         32
#define UFT_BTRFS_UUID_SIZE         16
#define UFT_BTRFS_LABEL_SIZE        256
#define UFT_BTRFS_SYSTEM_CHUNK_ARRAY_SIZE 2048

/*===========================================================================
 * Object IDs
 *===========================================================================*/

#define UFT_BTRFS_ROOT_TREE_OBJECTID        1
#define UFT_BTRFS_EXTENT_TREE_OBJECTID      2
#define UFT_BTRFS_CHUNK_TREE_OBJECTID       3
#define UFT_BTRFS_DEV_TREE_OBJECTID         4
#define UFT_BTRFS_FS_TREE_OBJECTID          5
#define UFT_BTRFS_ROOT_TREE_DIR_OBJECTID    6
#define UFT_BTRFS_CSUM_TREE_OBJECTID        7
#define UFT_BTRFS_QUOTA_TREE_OBJECTID       8
#define UFT_BTRFS_UUID_TREE_OBJECTID        9
#define UFT_BTRFS_FREE_SPACE_TREE_OBJECTID  10

#define UFT_BTRFS_DEV_STATS_OBJECTID        0
#define UFT_BTRFS_BALANCE_OBJECTID          (UINT64_MAX - 3)
#define UFT_BTRFS_ORPHAN_OBJECTID           (UINT64_MAX - 4)
#define UFT_BTRFS_TREE_LOG_OBJECTID         (UINT64_MAX - 5)
#define UFT_BTRFS_TREE_LOG_FIXUP_OBJECTID   (UINT64_MAX - 6)
#define UFT_BTRFS_TREE_RELOC_OBJECTID       (UINT64_MAX - 7)
#define UFT_BTRFS_DATA_RELOC_TREE_OBJECTID  (UINT64_MAX - 8)
#define UFT_BTRFS_EXTENT_CSUM_OBJECTID      (UINT64_MAX - 9)
#define UFT_BTRFS_FREE_SPACE_OBJECTID       (UINT64_MAX - 10)
#define UFT_BTRFS_FREE_INO_OBJECTID         (UINT64_MAX - 11)

#define UFT_BTRFS_FIRST_FREE_OBJECTID       256
#define UFT_BTRFS_LAST_FREE_OBJECTID        (UINT64_MAX - 255)

#define UFT_BTRFS_DEV_ITEMS_OBJECTID        1
#define UFT_BTRFS_BTREE_INODE_OBJECTID      1
#define UFT_BTRFS_EMPTY_SUBVOL_DIR_OBJECTID 2

/*===========================================================================
 * Key Types
 *===========================================================================*/

#define UFT_BTRFS_INODE_ITEM_KEY            1
#define UFT_BTRFS_INODE_REF_KEY             12
#define UFT_BTRFS_INODE_EXTREF_KEY          13
#define UFT_BTRFS_XATTR_ITEM_KEY            24
#define UFT_BTRFS_ORPHAN_ITEM_KEY           48
#define UFT_BTRFS_DIR_LOG_ITEM_KEY          60
#define UFT_BTRFS_DIR_LOG_INDEX_KEY         72
#define UFT_BTRFS_DIR_ITEM_KEY              84
#define UFT_BTRFS_DIR_INDEX_KEY             96
#define UFT_BTRFS_EXTENT_DATA_KEY           108
#define UFT_BTRFS_CSUM_ITEM_KEY             120
#define UFT_BTRFS_EXTENT_CSUM_KEY           128
#define UFT_BTRFS_ROOT_ITEM_KEY             132
#define UFT_BTRFS_ROOT_BACKREF_KEY          144
#define UFT_BTRFS_ROOT_REF_KEY              156
#define UFT_BTRFS_EXTENT_ITEM_KEY           168
#define UFT_BTRFS_METADATA_ITEM_KEY         169
#define UFT_BTRFS_TREE_BLOCK_REF_KEY        176
#define UFT_BTRFS_EXTENT_DATA_REF_KEY       178
#define UFT_BTRFS_SHARED_BLOCK_REF_KEY      182
#define UFT_BTRFS_SHARED_DATA_REF_KEY       184
#define UFT_BTRFS_BLOCK_GROUP_ITEM_KEY      192
#define UFT_BTRFS_FREE_SPACE_INFO_KEY       198
#define UFT_BTRFS_FREE_SPACE_EXTENT_KEY     199
#define UFT_BTRFS_FREE_SPACE_BITMAP_KEY     200
#define UFT_BTRFS_DEV_EXTENT_KEY            204
#define UFT_BTRFS_DEV_ITEM_KEY              216
#define UFT_BTRFS_CHUNK_ITEM_KEY            228
#define UFT_BTRFS_QGROUP_STATUS_KEY         240
#define UFT_BTRFS_QGROUP_INFO_KEY           242
#define UFT_BTRFS_QGROUP_LIMIT_KEY          244
#define UFT_BTRFS_QGROUP_RELATION_KEY       246
#define UFT_BTRFS_STRING_ITEM_KEY           253

/*===========================================================================
 * Checksum Types
 *===========================================================================*/

#define UFT_BTRFS_CSUM_TYPE_CRC32   0
#define UFT_BTRFS_CSUM_TYPE_XXHASH  1
#define UFT_BTRFS_CSUM_TYPE_SHA256  2
#define UFT_BTRFS_CSUM_TYPE_BLAKE2  3

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Btrfs disk key (17 bytes)
 */
typedef struct __attribute__((packed)) {
    uint64_t objectid;              /**< Object ID */
    uint8_t type;                   /**< Key type */
    uint64_t offset;                /**< Type-specific offset */
} uft_btrfs_disk_key_t;

/**
 * @brief Btrfs time structure
 */
typedef struct __attribute__((packed)) {
    uint64_t sec;                   /**< Seconds */
    uint32_t nsec;                  /**< Nanoseconds */
} uft_btrfs_timespec_t;

/**
 * @brief Device item (stored in chunk tree)
 */
typedef struct __attribute__((packed)) {
    uint64_t devid;                 /**< Device ID */
    uint64_t total_bytes;           /**< Total size */
    uint64_t bytes_used;            /**< Bytes used */
    uint32_t io_align;              /**< I/O alignment */
    uint32_t io_width;              /**< I/O width */
    uint32_t sector_size;           /**< Sector size */
    uint64_t type;                  /**< Device type */
    uint64_t generation;            /**< Generation */
    uint64_t start_offset;          /**< Start offset */
    uint32_t dev_group;             /**< Device group */
    uint8_t seek_speed;             /**< Seek speed (0-255) */
    uint8_t bandwidth;              /**< Bandwidth (0-255) */
    uint8_t uuid[UFT_BTRFS_UUID_SIZE];  /**< Device UUID */
    uint8_t fsid[UFT_BTRFS_UUID_SIZE];  /**< FS UUID */
} uft_btrfs_dev_item_t;

/**
 * @brief Superblock structure
 */
typedef struct __attribute__((packed)) {
    uint8_t csum[UFT_BTRFS_CSUM_SIZE];  /**< Checksum of everything past this */
    uint8_t fsid[UFT_BTRFS_UUID_SIZE];  /**< FS UUID */
    uint64_t bytenr;                    /**< Physical address of this block */
    uint64_t flags;                     /**< Flags */
    uint64_t magic;                     /**< "_BHRfS_M" */
    uint64_t generation;                /**< Generation */
    uint64_t root;                      /**< Logical address of root tree */
    uint64_t chunk_root;                /**< Logical address of chunk tree */
    uint64_t log_root;                  /**< Logical address of log tree */
    uint64_t log_root_transid;          /**< Log tree transaction ID */
    uint64_t total_bytes;               /**< Total bytes */
    uint64_t bytes_used;                /**< Bytes used */
    uint64_t root_dir_objectid;         /**< Root directory object ID */
    uint64_t num_devices;               /**< Number of devices */
    uint32_t sectorsize;                /**< Sector size */
    uint32_t nodesize;                  /**< Node size */
    uint32_t leafsize;                  /**< Leaf size (= nodesize) */
    uint32_t stripesize;                /**< Stripe size */
    uint32_t sys_chunk_array_size;      /**< Size of sys_chunk_array */
    uint64_t chunk_root_generation;     /**< Chunk tree generation */
    uint64_t compat_flags;              /**< Compatible feature flags */
    uint64_t compat_ro_flags;           /**< Compatible read-only flags */
    uint64_t incompat_flags;            /**< Incompatible feature flags */
    uint16_t csum_type;                 /**< Checksum type */
    uint8_t root_level;                 /**< Root tree level */
    uint8_t chunk_root_level;           /**< Chunk tree level */
    uint8_t log_root_level;             /**< Log tree level */
    uft_btrfs_dev_item_t dev_item;      /**< Device item for this device */
    char label[UFT_BTRFS_LABEL_SIZE];   /**< Volume label */
    uint64_t cache_generation;          /**< Cache generation */
    uint64_t uuid_tree_generation;      /**< UUID tree generation */
    uint8_t metadata_uuid[UFT_BTRFS_UUID_SIZE]; /**< Metadata UUID */
    uint64_t reserved[28];              /**< Reserved for future use */
    uint8_t sys_chunk_array[UFT_BTRFS_SYSTEM_CHUNK_ARRAY_SIZE]; /**< System chunks */
    /* Super roots follow (backup copies) */
} uft_btrfs_super_block_t;

/**
 * @brief Tree header (at start of every node)
 */
typedef struct __attribute__((packed)) {
    uint8_t csum[UFT_BTRFS_CSUM_SIZE];  /**< Checksum */
    uint8_t fsid[UFT_BTRFS_UUID_SIZE];  /**< FS UUID */
    uint64_t bytenr;                    /**< Logical address */
    uint64_t flags;                     /**< Flags */
    uint8_t chunk_tree_uuid[UFT_BTRFS_UUID_SIZE]; /**< Chunk tree UUID */
    uint64_t generation;                /**< Generation */
    uint64_t owner;                     /**< Tree that owns this node */
    uint32_t nritems;                   /**< Number of items */
    uint8_t level;                      /**< Level (0 = leaf) */
} uft_btrfs_header_t;

/**
 * @brief Leaf item (index into leaf data)
 */
typedef struct __attribute__((packed)) {
    uft_btrfs_disk_key_t key;           /**< Item key */
    uint32_t offset;                    /**< Offset from end of header */
    uint32_t size;                      /**< Item size */
} uft_btrfs_item_t;

/**
 * @brief Key pointer (in internal nodes)
 */
typedef struct __attribute__((packed)) {
    uft_btrfs_disk_key_t key;           /**< First key in child */
    uint64_t blockptr;                  /**< Child block address */
    uint64_t generation;                /**< Generation */
} uft_btrfs_key_ptr_t;

/**
 * @brief Inode item
 */
typedef struct __attribute__((packed)) {
    uint64_t generation;                /**< Generation */
    uint64_t transid;                   /**< Transaction ID */
    uint64_t size;                      /**< File size */
    uint64_t nbytes;                    /**< Bytes used */
    uint64_t block_group;               /**< Block group hint */
    uint32_t nlink;                     /**< Link count */
    uint32_t uid;                       /**< User ID */
    uint32_t gid;                       /**< Group ID */
    uint32_t mode;                      /**< File mode */
    uint64_t rdev;                      /**< Device (for special files) */
    uint64_t flags;                     /**< Inode flags */
    uint64_t sequence;                  /**< Sequence number */
    uint64_t reserved[4];               /**< Reserved */
    uft_btrfs_timespec_t atime;         /**< Access time */
    uft_btrfs_timespec_t ctime;         /**< Change time */
    uft_btrfs_timespec_t mtime;         /**< Modification time */
    uft_btrfs_timespec_t otime;         /**< Creation time */
} uft_btrfs_inode_item_t;

/*===========================================================================
 * Checksum Functions
 *===========================================================================*/

/**
 * @brief CRC32C lookup table (for btrfs checksum)
 */
extern const uint32_t uft_btrfs_crc32c_table[256];

/**
 * @brief Compute btrfs CRC32C checksum
 */
uint32_t uft_btrfs_crc32c(uint32_t seed, const uint8_t *data, size_t len);

/**
 * @brief Verify superblock checksum
 */
bool uft_btrfs_verify_super_csum(const uft_btrfs_super_block_t *sb);

/**
 * @brief Verify tree node checksum
 */
bool uft_btrfs_verify_node_csum(const uft_btrfs_header_t *hdr, size_t node_size);

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Check if buffer contains btrfs superblock
 */
static inline bool uft_btrfs_is_superblock(const void *buf)
{
    const uft_btrfs_super_block_t *sb = (const uft_btrfs_super_block_t *)buf;
    return sb->magic == UFT_BTRFS_MAGIC;
}

/**
 * @brief Get key type name
 */
const char *uft_btrfs_key_type_name(uint8_t type);

/**
 * @brief Get checksum type name
 */
const char *uft_btrfs_csum_type_name(uint16_t type);

/**
 * @brief Search for superblock copies
 * @param read_fn Callback to read blocks
 * @param ctx Read callback context
 * @param device_size Total device size
 * @param found Array of found superblock offsets (max 4)
 * @return Number of valid superblocks found
 */
int uft_btrfs_find_superblocks(
    int (*read_fn)(void *ctx, uint64_t offset, void *buf, size_t len),
    void *ctx, uint64_t device_size, uint64_t found[4]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BTRFS_H */
