/**
 * @file uft_rcpmfs.h
 * @brief RCPMFS (Remote CP/M File System) support
 * @version 3.9.0
 * 
 * RCPMFS is a network-accessible CP/M file system format used
 * by some CP/M emulators and servers. It provides a standardized
 * way to access CP/M disk images over a network or as a container.
 * 
 * Features:
 * - Multiple disk definitions in one container
 * - User area support (0-15)
 * - File attributes (R/O, SYS, ARC)
 * - Optional compression
 * 
 * Reference: libdsk drvrcpm.c
 */

#ifndef UFT_RCPMFS_H
#define UFT_RCPMFS_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_cpm_diskdef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RCPMFS Magic Numbers */
#define RCPMFS_MAGIC            "RCPM"
#define RCPMFS_MAGIC_LEN        4
#define RCPMFS_VERSION          1
#define RCPMFS_HEADER_SIZE      64

/* Maximum values */
#define RCPMFS_MAX_DISKS        16
#define RCPMFS_MAX_NAME         32
#define RCPMFS_MAX_COMMENT      256

/* File flags */
#define RCPMFS_FILE_RO          0x01    /* Read-only */
#define RCPMFS_FILE_SYS         0x02    /* System file */
#define RCPMFS_FILE_ARC         0x04    /* Archived */

/**
 * @brief RCPMFS container header (64 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char     magic[4];          /* "RCPM" */
    uint8_t  version;           /* Container version */
    uint8_t  flags;             /* Container flags */
    uint16_t num_disks;         /* Number of disk images */
    uint32_t total_size;        /* Total container size */
    char     comment[48];       /* Optional comment */
} rcpmfs_header_t;

/**
 * @brief RCPMFS disk entry (48 bytes per disk)
 */
typedef struct {
    char     name[16];          /* Disk name */
    char     diskdef[16];       /* CP/M disk definition name */
    uint32_t offset;            /* Offset to disk data */
    uint32_t size;              /* Disk data size */
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    uint8_t  reserved[6];
} rcpmfs_disk_entry_t;
#pragma pack(pop)

/**
 * @brief RCPMFS read options
 */
typedef struct {
    int      disk_index;        /* Which disk to read (0-based, -1 = all) */
    char     disk_name[32];     /* Or select by name */
} rcpmfs_read_options_t;

/**
 * @brief RCPMFS write options
 */
typedef struct {
    char     comment[RCPMFS_MAX_COMMENT];
    bool     compress;          /* Enable compression */
} rcpmfs_write_options_t;

/**
 * @brief RCPMFS disk info
 */
typedef struct {
    char     name[16];
    char     diskdef[16];
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    size_t   data_size;
} rcpmfs_disk_info_t;

/**
 * @brief RCPMFS read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint16_t num_disks;
    rcpmfs_disk_info_t disks[RCPMFS_MAX_DISKS];
    char comment[RCPMFS_MAX_COMMENT];
    
    size_t container_size;
    
} rcpmfs_read_result_t;

/* ============================================================================
 * RCPMFS Functions
 * ============================================================================ */

/**
 * @brief Initialize read options
 */
void uft_rcpmfs_read_options_init(rcpmfs_read_options_t *opts);

/**
 * @brief Initialize write options
 */
void uft_rcpmfs_write_options_init(rcpmfs_write_options_t *opts);

/**
 * @brief Read RCPMFS container
 */
uft_error_t uft_rcpmfs_read(const char *path,
                            uft_disk_image_t **out_disk,
                            const rcpmfs_read_options_t *opts,
                            rcpmfs_read_result_t *result);

/**
 * @brief Read RCPMFS from memory
 */
uft_error_t uft_rcpmfs_read_mem(const uint8_t *data, size_t size,
                                uft_disk_image_t **out_disk,
                                const rcpmfs_read_options_t *opts,
                                rcpmfs_read_result_t *result);

/**
 * @brief Write disk image to RCPMFS container
 */
uft_error_t uft_rcpmfs_write(const uft_disk_image_t *disk,
                             const char *path,
                             const char *disk_name,
                             const char *diskdef_name,
                             const rcpmfs_write_options_t *opts);

/**
 * @brief Add disk to existing RCPMFS container
 */
uft_error_t uft_rcpmfs_add_disk(const char *container_path,
                                const uft_disk_image_t *disk,
                                const char *disk_name,
                                const char *diskdef_name);

/**
 * @brief List disks in RCPMFS container
 */
uft_error_t uft_rcpmfs_list(const char *path,
                            rcpmfs_disk_info_t *disks,
                            size_t max_disks,
                            size_t *num_disks);

/**
 * @brief Probe if data is RCPMFS format
 */
bool uft_rcpmfs_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Validate RCPMFS header
 */
bool uft_rcpmfs_validate_header(const rcpmfs_header_t *header);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RCPMFS_H */
