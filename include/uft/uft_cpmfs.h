/**
 * @file uft_cpmfs.h
 * @brief CP/M Filesystem Support
 * @version 3.1.4.002
 *
 * Read/write support for CP/M 2.2 and CP/M Plus (3.0) filesystems.
 * Supports various timestamp formats:
 * - CP/M Plus date stamps
 * - DateStamper (DS) format
 * - P2DOS timestamps
 * - ZSDOS timestamps
 *
 * Based on cpmtools/cpmfs.c (GPL) with clean-room reimplementation
 * for MIT license compatibility.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_CPMFS_H
#define UFT_CPMFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CP/M Constants
 *============================================================================*/

/** Maximum user number in standard CP/M */
#define UFT_CPM_MAX_USER        15

/** Maximum user number in extended systems (ZSDOS) */
#define UFT_CPM_MAX_USER_EXT    31

/** Directory entry size */
#define UFT_CPM_DIRENT_SIZE     32

/** FCB extent size */
#define UFT_CPM_EXTENT_SIZE     16384  /* 16KB per extent */

/*============================================================================
 * CP/M File Attributes
 *============================================================================*/

#define UFT_CPM_ATTR_F1     (1 << 0)   /**< User attribute 1 */
#define UFT_CPM_ATTR_F2     (1 << 1)   /**< User attribute 2 */
#define UFT_CPM_ATTR_F3     (1 << 2)   /**< User attribute 3 */
#define UFT_CPM_ATTR_F4     (1 << 3)   /**< User attribute 4 */
#define UFT_CPM_ATTR_RO     (1 << 8)   /**< Read-only */
#define UFT_CPM_ATTR_SYS    (1 << 9)   /**< System file */
#define UFT_CPM_ATTR_ARC    (1 << 10)  /**< Archive bit */
#define UFT_CPM_ATTR_PWDEL  (1 << 11)  /**< Password to delete (CP/M+) */
#define UFT_CPM_ATTR_PWWR   (1 << 12)  /**< Password to write (CP/M+) */
#define UFT_CPM_ATTR_PWRD   (1 << 13)  /**< Password to read (CP/M+) */

/*============================================================================
 * Filesystem Types
 *============================================================================*/

typedef enum {
    UFT_CPMFS_DR22      = 0,    /**< Digital Research CP/M 2.2 */
    UFT_CPMFS_P2DOS     = 1,    /**< P2DOS (CP/M+ dates, hi user) */
    UFT_CPMFS_DR3       = 2,    /**< CP/M Plus 3.0 */
    UFT_CPMFS_ISX       = 3,    /**< ISX (exact file size) */
    UFT_CPMFS_ZSDOS     = 4,    /**< ZSDOS (hi user, DateStamper) */
    UFT_CPMFS_DOSPLUS   = 5,    /**< DOS Plus */
} uft_cpmfs_type_t;

/** Feature flags */
#define UFT_CPMFS_HI_USER       (1 << 0)  /**< User 0-31 instead of 0-15 */
#define UFT_CPMFS_CPM3_DATES    (1 << 1)  /**< CP/M+ timestamps */
#define UFT_CPMFS_CPM3_OTHER    (1 << 2)  /**< Passwords, labels */
#define UFT_CPMFS_DS_DATES      (1 << 3)  /**< DateStamper timestamps */
#define UFT_CPMFS_EXACT_SIZE    (1 << 4)  /**< Exact file size stored */

/*============================================================================
 * Disk Parameter Block (DPB)
 *============================================================================*/

/**
 * @brief CP/M Disk Parameter Block
 *
 * Defines the disk geometry and allocation parameters.
 */
typedef struct {
    uint16_t    spt;        /**< Sectors per track (128-byte logical) */
    uint8_t     bsh;        /**< Block shift (log2(blksiz/128)) */
    uint8_t     blm;        /**< Block mask (2^bsh - 1) */
    uint8_t     exm;        /**< Extent mask */
    uint16_t    dsm;        /**< Max block number (disk size - 1) */
    uint16_t    drm;        /**< Max directory entry (dir size - 1) */
    uint8_t     al0;        /**< Directory allocation byte 0 */
    uint8_t     al1;        /**< Directory allocation byte 1 */
    uint16_t    cks;        /**< Checksum vector size */
    uint16_t    off;        /**< Track offset (boot tracks) */
} uft_cpm_dpb_t;

/**
 * @brief UFT extended disk definition
 */
typedef struct {
    const char *name;       /**< Format name */
    
    /* Physical geometry */
    uint16_t    seclen;     /**< Physical sector size */
    uint16_t    tracks;     /**< Total tracks */
    uint16_t    sectrk;     /**< Sectors per track */
    
    /* CP/M parameters */
    uint16_t    blksiz;     /**< Block size (1024, 2048, 4096, ...) */
    uint16_t    maxdir;     /**< Directory entries */
    uint16_t    skew;       /**< Sector skew */
    uint16_t    boottrk;    /**< Boot track count */
    int32_t     offset;     /**< Byte offset to start */
    
    /* Features */
    uint32_t    flags;      /**< Feature flags */
} uft_cpm_diskdef_t;

/*============================================================================
 * Directory Entry Structure
 *============================================================================*/

/** Raw directory entry (32 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t     status;     /**< User number or status (0xE5 = deleted) */
    char        name[8];    /**< Filename (high bits = attributes) */
    char        ext[3];     /**< Extension (high bits = attributes) */
    uint8_t     xl;         /**< Extent low byte */
    uint8_t     bc;         /**< Byte count (CP/M+) or reserved */
    uint8_t     xh;         /**< Extent high byte */
    uint8_t     rc;         /**< Record count in last extent */
    uint8_t     al[16];     /**< Allocation map (block pointers) */
} uft_cpm_dirent_t;

/** DateStamper timestamp entry */
typedef struct __attribute__((packed)) {
    uint8_t     year;       /**< Year (BCD, 00-99) */
    uint8_t     month;      /**< Month (BCD, 01-12) */
    uint8_t     day;        /**< Day (BCD, 01-31) */
    uint8_t     hour;       /**< Hour (BCD, 00-23) */
    uint8_t     minute;     /**< Minute (BCD, 00-59) */
} uft_ds_entry_t;

/** DateStamper record (one per directory entry) */
typedef struct __attribute__((packed)) {
    uft_ds_entry_t create;  /**< Creation time */
    uft_ds_entry_t access;  /**< Last access time */
    uft_ds_entry_t modify;  /**< Last modification time */
    uint8_t     checksum;   /**< Checksum */
} uft_ds_date_t;

/*============================================================================
 * Filesystem Structures
 *============================================================================*/

/** File inode */
typedef struct {
    uint32_t    ino;        /**< Inode number */
    uint32_t    mode;       /**< File mode */
    uint32_t    size;       /**< File size in bytes */
    uint32_t    attr;       /**< CP/M attributes */
    time_t      atime;      /**< Access time */
    time_t      mtime;      /**< Modification time */
    time_t      ctime;      /**< Creation time */
    void       *sb;         /**< Superblock pointer */
} uft_cpm_inode_t;

/** Open file handle */
typedef struct {
    uint32_t    mode;       /**< Open mode */
    uint32_t    pos;        /**< Current position */
    uft_cpm_inode_t *ino;   /**< Inode pointer */
} uft_cpm_file_t;

/** Directory entry (for readdir) */
typedef struct {
    uint32_t    ino;        /**< Inode number */
    uint32_t    off;        /**< Offset */
    char        name[14];   /**< Filename (UUfilename.ext\0) */
} uft_cpm_dirent_info_t;

/** Filesystem superblock */
typedef struct {
    /* Device access */
    void       *device;     /**< Device handle */
    int       (*read_sector)(void *dev, uint32_t lba, void *buf);
    int       (*write_sector)(void *dev, uint32_t lba, const void *buf);
    
    /* Geometry */
    uint16_t    seclen;     /**< Sector length */
    uint16_t    tracks;     /**< Track count */
    uint16_t    sectrk;     /**< Sectors per track */
    uint16_t    blksiz;     /**< Block size */
    uint16_t    maxdir;     /**< Max directory entries */
    uint16_t    skew;       /**< Sector skew */
    uint16_t    boottrk;    /**< Boot tracks */
    int32_t     offset;     /**< Byte offset */
    uint32_t    flags;      /**< Feature flags */
    
    /* Derived values */
    uint16_t    blocks;     /**< Total blocks */
    uint16_t    extents;    /**< Logical extents per physical */
    uint16_t    *skewtab;   /**< Skew table */
    
    /* Directory */
    uft_cpm_dirent_t *dir;  /**< Directory buffer */
    uint32_t    *alv;       /**< Allocation vector */
    uint32_t    alv_size;   /**< ALV size in words */
    
    /* DateStamper */
    uft_ds_date_t *ds;      /**< DateStamper records */
    bool        ds_dirty;   /**< DS needs write */
    
    /* State */
    bool        dir_dirty;  /**< Directory needs write */
    char       *label;      /**< Disk label */
    uft_cpm_inode_t *root;  /**< Root inode */
} uft_cpm_sb_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Mount CP/M filesystem
 * @param sb Superblock to initialize
 * @param diskdef Disk definition
 * @param device Device handle
 * @param read_fn Sector read function
 * @param write_fn Sector write function (NULL for read-only)
 * @return 0 on success, -1 on error
 */
int uft_cpmfs_mount(uft_cpm_sb_t *sb,
                    const uft_cpm_diskdef_t *diskdef,
                    void *device,
                    int (*read_fn)(void*, uint32_t, void*),
                    int (*write_fn)(void*, uint32_t, const void*));

/**
 * @brief Unmount filesystem
 * @param sb Superblock
 */
void uft_cpmfs_unmount(uft_cpm_sb_t *sb);

/**
 * @brief Sync filesystem to disk
 * @param sb Superblock
 * @return 0 on success
 */
int uft_cpmfs_sync(uft_cpm_sb_t *sb);

/**
 * @brief Look up file by name
 * @param sb Superblock
 * @param name Filename (UUfilename.ext format)
 * @param ino Output inode
 * @return 0 on success, -1 if not found
 */
int uft_cpmfs_lookup(uft_cpm_sb_t *sb, const char *name, uft_cpm_inode_t *ino);

/**
 * @brief Open directory for reading
 * @param sb Superblock
 * @param dir Output directory handle
 * @return 0 on success
 */
int uft_cpmfs_opendir(uft_cpm_sb_t *sb, uft_cpm_file_t *dir);

/**
 * @brief Read next directory entry
 * @param dir Directory handle
 * @param ent Output entry info
 * @return 1 if entry read, 0 if end, -1 on error
 */
int uft_cpmfs_readdir(uft_cpm_file_t *dir, uft_cpm_dirent_info_t *ent);

/**
 * @brief Open file
 * @param ino Inode
 * @param file Output file handle
 * @param mode Open mode (O_RDONLY, O_WRONLY, O_RDWR)
 * @return 0 on success
 */
int uft_cpmfs_open(uft_cpm_inode_t *ino, uft_cpm_file_t *file, int mode);

/**
 * @brief Read from file
 * @param file File handle
 * @param buf Output buffer
 * @param count Bytes to read
 * @return Bytes read, or -1 on error
 */
int uft_cpmfs_read(uft_cpm_file_t *file, void *buf, size_t count);

/**
 * @brief Write to file
 * @param file File handle
 * @param buf Data to write
 * @param count Bytes to write
 * @return Bytes written, or -1 on error
 */
int uft_cpmfs_write(uft_cpm_file_t *file, const void *buf, size_t count);

/**
 * @brief Close file
 * @param file File handle
 * @return 0 on success
 */
int uft_cpmfs_close(uft_cpm_file_t *file);

/**
 * @brief Create new file
 * @param sb Superblock
 * @param name Filename
 * @param ino Output inode
 * @return 0 on success
 */
int uft_cpmfs_create(uft_cpm_sb_t *sb, const char *name, uft_cpm_inode_t *ino);

/**
 * @brief Delete file
 * @param sb Superblock
 * @param name Filename
 * @return 0 on success
 */
int uft_cpmfs_unlink(uft_cpm_sb_t *sb, const char *name);

/**
 * @brief Rename file
 * @param sb Superblock
 * @param oldname Current name
 * @param newname New name
 * @return 0 on success
 */
int uft_cpmfs_rename(uft_cpm_sb_t *sb, const char *oldname, const char *newname);

/**
 * @brief Get file attributes
 * @param ino Inode
 * @param attr Output attributes
 * @return 0 on success
 */
int uft_cpmfs_getattr(uft_cpm_inode_t *ino, uint32_t *attr);

/**
 * @brief Set file attributes
 * @param ino Inode
 * @param attr New attributes
 * @return 0 on success
 */
int uft_cpmfs_setattr(uft_cpm_inode_t *ino, uint32_t attr);

/**
 * @brief Get filesystem statistics
 * @param sb Superblock
 * @param total Output: total blocks
 * @param free Output: free blocks
 * @param files Output: file count
 */
void uft_cpmfs_statfs(uft_cpm_sb_t *sb, 
                      uint32_t *total, uint32_t *free, uint32_t *files);

/*============================================================================
 * Time Conversion Functions
 *============================================================================*/

/**
 * @brief Convert CP/M timestamp to Unix time
 * @param days Days since Jan 1, 1978
 * @param hour Hour (BCD)
 * @param min Minute (BCD)
 * @return Unix timestamp
 */
time_t uft_cpm_to_unix_time(uint16_t days, uint8_t hour, uint8_t min);

/**
 * @brief Convert Unix time to CP/M timestamp
 * @param t Unix timestamp
 * @param days Output: days since Jan 1, 1978
 * @param hour Output: hour (BCD)
 * @param min Output: minute (BCD)
 */
void uft_unix_to_cpm_time(time_t t, uint16_t *days, uint8_t *hour, uint8_t *min);

/**
 * @brief Convert DateStamper entry to Unix time
 * @param ds DateStamper entry
 * @return Unix timestamp
 */
time_t uft_ds_to_unix_time(const uft_ds_entry_t *ds);

/**
 * @brief Convert Unix time to DateStamper entry
 * @param t Unix timestamp
 * @param ds Output DateStamper entry
 */
void uft_unix_to_ds_time(time_t t, uft_ds_entry_t *ds);

/*============================================================================
 * Predefined Disk Definitions
 *============================================================================*/

/** IBM PC 1.44MB */
extern const uft_cpm_diskdef_t UFT_CPMFS_IBM144;

/** IBM PC 720KB */
extern const uft_cpm_diskdef_t UFT_CPMFS_IBM720;

/** Kaypro II */
extern const uft_cpm_diskdef_t UFT_CPMFS_KAYPRO2;

/** Osborne 1 */
extern const uft_cpm_diskdef_t UFT_CPMFS_OSBORNE1;

/** Amstrad PCW */
extern const uft_cpm_diskdef_t UFT_CPMFS_AMSTRAD;

/** Memotech MTX */
extern const uft_cpm_diskdef_t UFT_CPMFS_MEMOTECH;

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPMFS_H */
