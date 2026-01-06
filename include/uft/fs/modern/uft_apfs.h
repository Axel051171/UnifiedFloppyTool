/**
 * @file uft_apfs.h
 * @brief Apple File System (APFS) Structures
 * 
 * Based on Apple APFS Reference (2020-06-22)
 * and drat by various contributors
 */

#ifndef UFT_APFS_H
#define UFT_APFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_APFS_MAGIC              0x4253584E  /**< "NXSB" */
#define UFT_APFS_BLOCK_SIZE_MIN     4096
#define UFT_APFS_BLOCK_SIZE_MAX     65536
#define UFT_APFS_MAX_CKSUM_SIZE     8

#define UFT_APFS_LABEL_SIZE         256
#define UFT_APFS_UUID_SIZE          16
#define UFT_APFS_CSUM_SIZE          32
#define UFT_APFS_CHUNK_ARRAY_SIZE   2048

/** Superblock offsets */
#define UFT_APFS_SUPER_OFFSET       0x00000     /**< Block 0 */

/*===========================================================================
 * Object Types
 *===========================================================================*/

/** Object type masks */
#define UFT_APFS_OBJ_TYPE_MASK      0x0000FFFF
#define UFT_APFS_OBJ_STORAGETYPE_MASK 0xC0000000

/** Object types */
#define UFT_APFS_OBJ_NX_SUPERBLOCK      0x01
#define UFT_APFS_OBJ_BTREE              0x02
#define UFT_APFS_OBJ_BTREE_NODE         0x03
#define UFT_APFS_OBJ_SPACEMAN           0x05
#define UFT_APFS_OBJ_SPACEMAN_CAB       0x06
#define UFT_APFS_OBJ_SPACEMAN_CIB       0x07
#define UFT_APFS_OBJ_SPACEMAN_BITMAP    0x08
#define UFT_APFS_OBJ_OMAP               0x0B
#define UFT_APFS_OBJ_CHECKPOINT_MAP     0x0C
#define UFT_APFS_OBJ_FS                 0x0D
#define UFT_APFS_OBJ_FSTREE             0x0E
#define UFT_APFS_OBJ_BLOCKREFTREE       0x0F
#define UFT_APFS_OBJ_SNAPMETATREE       0x10
#define UFT_APFS_OBJ_NX_REAPER          0x11
#define UFT_APFS_OBJ_NX_REAP_LIST       0x12
#define UFT_APFS_OBJ_OMAP_SNAPSHOT      0x13
#define UFT_APFS_OBJ_EFI_JUMPSTART      0x14
#define UFT_APFS_OBJ_FUSION_MIDDLE_TREE 0x15
#define UFT_APFS_OBJ_NX_FUSION_WBC      0x16
#define UFT_APFS_OBJ_NX_FUSION_WBC_LIST 0x17
#define UFT_APFS_OBJ_ER_STATE           0x18
#define UFT_APFS_OBJ_GBITMAP            0x19
#define UFT_APFS_OBJ_GBITMAP_TREE       0x1A
#define UFT_APFS_OBJ_GBITMAP_BLOCK      0x1B

/** Storage types */
#define UFT_APFS_OBJ_VIRTUAL        0x00000000
#define UFT_APFS_OBJ_EPHEMERAL      0x80000000
#define UFT_APFS_OBJ_PHYSICAL       0x40000000

/** Object ID constants */
#define UFT_APFS_OID_INVALID        0
#define UFT_APFS_OID_RESERVED       1

/*===========================================================================
 * Key Types (j_obj_types)
 *===========================================================================*/

#define UFT_APFS_J_KEY_ANY          0
#define UFT_APFS_J_KEY_SNAP_METADATA 1
#define UFT_APFS_J_KEY_EXTENT       2
#define UFT_APFS_J_KEY_INODE        3
#define UFT_APFS_J_KEY_XATTR        4
#define UFT_APFS_J_KEY_SIBLING_LINK 5
#define UFT_APFS_J_KEY_DSTREAM_ID   6
#define UFT_APFS_J_KEY_CRYPTO_STATE 7
#define UFT_APFS_J_KEY_FILE_EXTENT  8
#define UFT_APFS_J_KEY_DIR_REC      9
#define UFT_APFS_J_KEY_DIR_STATS    10
#define UFT_APFS_J_KEY_SNAP_NAME    11
#define UFT_APFS_J_KEY_SIBLING_MAP  12

/*===========================================================================
 * Inode Flags
 *===========================================================================*/

#define UFT_APFS_INODE_IS_APFS_PRIVATE       0x00000001
#define UFT_APFS_INODE_MAINTAIN_DIR_STATS    0x00000002
#define UFT_APFS_INODE_DIR_STATS_ORIGIN      0x00000004
#define UFT_APFS_INODE_PROT_CLASS_EXPLICIT   0x00000008
#define UFT_APFS_INODE_WAS_CLONED            0x00000010
#define UFT_APFS_INODE_FLAG_UNUSED           0x00000020
#define UFT_APFS_INODE_HAS_SECURITY_EA       0x00000040
#define UFT_APFS_INODE_BEING_TRUNCATED       0x00000080
#define UFT_APFS_INODE_HAS_FINDER_INFO       0x00000100
#define UFT_APFS_INODE_IS_SPARSE             0x00000200
#define UFT_APFS_INODE_WAS_EVER_CLONED       0x00000400
#define UFT_APFS_INODE_ACTIVE_FILE_TRIMMED   0x00000800
#define UFT_APFS_INODE_PINNED_TO_MAIN        0x00001000
#define UFT_APFS_INODE_PINNED_TO_TIER2       0x00002000
#define UFT_APFS_INODE_HAS_RSRC_FORK         0x00004000
#define UFT_APFS_INODE_NO_RSRC_FORK          0x00008000
#define UFT_APFS_INODE_ALLOCATION_SPILLEDOVER 0x00010000

/*===========================================================================
 * Structures
 *===========================================================================*/

typedef uint64_t uft_apfs_oid_t;    /**< Object identifier */
typedef uint64_t uft_apfs_xid_t;    /**< Transaction identifier */
typedef int64_t  uft_apfs_paddr_t;  /**< Physical block address */

/**
 * @brief Physical address range
 */
typedef struct __attribute__((packed)) {
    uft_apfs_paddr_t pr_start_paddr;
    uint64_t pr_block_count;
} uft_apfs_prange_t;

/**
 * @brief Object header (first 32 bytes of every object)
 */
typedef struct __attribute__((packed)) {
    uint8_t o_cksum[UFT_APFS_MAX_CKSUM_SIZE];  /**< Fletcher-64 checksum */
    uft_apfs_oid_t o_oid;                       /**< Object identifier */
    uft_apfs_xid_t o_xid;                       /**< Transaction ID */
    uint32_t o_type;                            /**< Object type + flags */
    uint32_t o_subtype;                         /**< Object subtype */
} uft_apfs_obj_phys_t;

/**
 * @brief Container superblock (nx_superblock_t)
 */
typedef struct __attribute__((packed)) {
    uft_apfs_obj_phys_t nx_o;                   /**< Object header */
    
    uint32_t nx_magic;                          /**< "NXSB" = 0x4253584E */
    uint32_t nx_block_size;                     /**< Block size (4096-65536) */
    uint64_t nx_block_count;                    /**< Total blocks in container */
    
    uint64_t nx_features;                       /**< Feature flags */
    uint64_t nx_readonly_compatible_features;
    uint64_t nx_incompatible_features;
    
    uint8_t nx_uuid[UFT_APFS_UUID_SIZE];        /**< Container UUID */
    
    uft_apfs_oid_t nx_next_oid;                 /**< Next available OID */
    uft_apfs_xid_t nx_next_xid;                 /**< Next transaction ID */
    
    uint32_t nx_xp_desc_blocks;                 /**< Checkpoint descriptor blocks */
    uint32_t nx_xp_data_blocks;                 /**< Checkpoint data blocks */
    uft_apfs_paddr_t nx_xp_desc_base;           /**< Checkpoint descriptor base */
    uft_apfs_paddr_t nx_xp_data_base;           /**< Checkpoint data base */
    uint32_t nx_xp_desc_next;
    uint32_t nx_xp_data_next;
    uint32_t nx_xp_desc_index;
    uint32_t nx_xp_desc_len;
    uint32_t nx_xp_data_index;
    uint32_t nx_xp_data_len;
    
    uft_apfs_oid_t nx_spaceman_oid;             /**< Space manager OID */
    uft_apfs_oid_t nx_omap_oid;                 /**< Object map OID */
    uft_apfs_oid_t nx_reaper_oid;               /**< Reaper OID */
    
    uint32_t nx_test_type;
    
    uint32_t nx_max_file_systems;               /**< Max volumes */
    uft_apfs_oid_t nx_fs_oid[100];              /**< Volume OIDs (first 100) */
    /* ... additional fields omitted for brevity ... */
} uft_apfs_nx_superblock_t;

/**
 * @brief Volume superblock (apfs_superblock_t)
 */
typedef struct __attribute__((packed)) {
    uft_apfs_obj_phys_t apfs_o;                 /**< Object header */
    
    uint32_t apfs_magic;                        /**< "APSB" = 0x42535041 */
    uint32_t apfs_fs_index;                     /**< Volume index */
    
    uint64_t apfs_features;
    uint64_t apfs_readonly_compatible_features;
    uint64_t apfs_incompatible_features;
    
    uint64_t apfs_unmount_time;
    
    uint64_t apfs_fs_reserve_block_count;
    uint64_t apfs_fs_quota_block_count;
    uint64_t apfs_fs_alloc_count;
    
    /* Wrapped key for encryption */
    uint8_t apfs_meta_crypto[32];
    
    uint32_t apfs_root_tree_type;
    uint32_t apfs_extentref_tree_type;
    uint32_t apfs_snap_meta_tree_type;
    
    uft_apfs_oid_t apfs_omap_oid;               /**< Object map OID */
    uft_apfs_oid_t apfs_root_tree_oid;          /**< Root B-tree OID */
    uft_apfs_oid_t apfs_extentref_tree_oid;
    uft_apfs_oid_t apfs_snap_meta_tree_oid;
    
    uft_apfs_xid_t apfs_revert_to_xid;
    uft_apfs_oid_t apfs_revert_to_sblock_oid;
    
    uint64_t apfs_next_obj_id;
    
    uint64_t apfs_num_files;
    uint64_t apfs_num_directories;
    uint64_t apfs_num_symlinks;
    uint64_t apfs_num_other_fsobjects;
    uint64_t apfs_num_snapshots;
    
    uint64_t apfs_total_blocks_alloced;
    uint64_t apfs_total_blocks_freed;
    
    uint8_t apfs_vol_uuid[UFT_APFS_UUID_SIZE];  /**< Volume UUID */
    uint64_t apfs_last_mod_time;
    
    uint64_t apfs_fs_flags;
    
    char apfs_formatted_by[32];                 /**< Software that created volume */
    /* ... additional fields ... */
} uft_apfs_superblock_t;

/**
 * @brief B-tree key (j_key_t)
 */
typedef struct __attribute__((packed)) {
    uint64_t obj_id_and_type;                   /**< OID (60 bits) + type (4 bits) */
} uft_apfs_j_key_t;

/**
 * @brief Inode key (j_inode_key_t)
 */
typedef struct __attribute__((packed)) {
    uft_apfs_j_key_t hdr;
} uft_apfs_j_inode_key_t;

/**
 * @brief Inode value (j_inode_val_t) - partial
 */
typedef struct __attribute__((packed)) {
    uint64_t parent_id;
    uint64_t private_id;
    uint64_t create_time;
    uint64_t mod_time;
    uint64_t change_time;
    uint64_t access_time;
    uint64_t internal_flags;
    union {
        int32_t nchildren;
        int32_t nlink;
    };
    uint32_t default_protection_class;
    uint32_t write_generation_counter;
    uint32_t bsd_flags;
    uint32_t owner;
    uint32_t group;
    uint16_t mode;
    uint16_t pad1;
    uint64_t uncompressed_size;
    /* Extended fields follow */
} uft_apfs_j_inode_val_t;

/*===========================================================================
 * Fletcher-64 Checksum (APFS Variant)
 *===========================================================================*/

/**
 * @brief Compute APFS Fletcher-64 checksum
 * 
 * APFS uses a variant where the first 8 bytes (checksum location)
 * are treated as zero during computation.
 * 
 * @param block Block data
 * @param block_size Block size (must be multiple of 4)
 * @return Computed checksum
 */
static inline uint64_t uft_apfs_fletcher64(const uint32_t *block, size_t block_size)
{
    size_t num_words = block_size / 4;
    uint32_t modulus = 0xFFFFFFFF;
    
    uint64_t simple_sum = 0;
    uint64_t second_sum = 0;
    
    /* Skip first 2 words (checksum location) */
    for (size_t i = 2; i < num_words; i++) {
        simple_sum = (simple_sum + block[i]) % modulus;
        second_sum = (second_sum + simple_sum) % modulus;
    }
    
    /* APFS variant: c1 = -(simple_sum + second_sum) mod modulus */
    simple_sum = modulus - ((simple_sum + second_sum) % modulus);
    
    return ((uint64_t)second_sum << 32) | simple_sum;
}

/**
 * @brief Verify APFS block checksum
 */
static inline bool uft_apfs_verify_checksum(const void *block, size_t block_size)
{
    const uint64_t *stored = (const uint64_t *)block;
    uint64_t computed = uft_apfs_fletcher64((const uint32_t *)block, block_size);
    return computed == *stored;
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Check if block is APFS container superblock
 */
static inline bool uft_apfs_is_nx_superblock(const void *block)
{
    const uft_apfs_nx_superblock_t *sb = (const uft_apfs_nx_superblock_t *)block;
    return sb->nx_magic == UFT_APFS_MAGIC;
}

/**
 * @brief Extract object type from type field
 */
static inline uint32_t uft_apfs_obj_type(uint32_t type_field)
{
    return type_field & UFT_APFS_OBJ_TYPE_MASK;
}

/**
 * @brief Extract storage type from type field
 */
static inline uint32_t uft_apfs_storage_type(uint32_t type_field)
{
    return type_field & UFT_APFS_OBJ_STORAGETYPE_MASK;
}

/**
 * @brief Extract object ID from j_key
 */
static inline uint64_t uft_apfs_j_key_oid(const uft_apfs_j_key_t *key)
{
    return key->obj_id_and_type & 0x0FFFFFFFFFFFFFFFULL;
}

/**
 * @brief Extract type from j_key
 */
static inline uint8_t uft_apfs_j_key_type(const uft_apfs_j_key_t *key)
{
    return (key->obj_id_and_type >> 60) & 0x0F;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_APFS_H */
