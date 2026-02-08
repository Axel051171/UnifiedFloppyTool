/**
 * @file uft_spartados.h
 * @brief SpartaDOS Filesystem Support
 * 
 * EXT-008: Atari 8-bit SpartaDOS filesystem
 * 
 * SpartaDOS was a third-party DOS for Atari 8-bit computers
 * supporting subdirectories, timestamps, and larger disk sizes.
 */

#ifndef UFT_SPARTADOS_H
#define UFT_SPARTADOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_SPARTA_SECTOR_SIZE      256
#define UFT_SPARTA_MAX_FILENAME     8
#define UFT_SPARTA_MAX_EXT          3
#define UFT_SPARTA_DIR_ENTRY_SIZE   23
#define UFT_SPARTA_MAX_PATH         256

/* File status flags */
#define UFT_SPARTA_FLAG_INUSE       0x40    /**< Entry in use */
#define UFT_SPARTA_FLAG_DELETED     0x80    /**< File deleted */
#define UFT_SPARTA_FLAG_LOCKED      0x20    /**< File locked */
#define UFT_SPARTA_FLAG_OPENED      0x10    /**< File open */
#define UFT_SPARTA_FLAG_SUBDIR      0x08    /**< Subdirectory */

/*===========================================================================
 * Enumerations
 *===========================================================================*/

typedef enum {
    UFT_SPARTA_V1 = 1,              /**< SpartaDOS 1.x */
    UFT_SPARTA_V2,                  /**< SpartaDOS 2.x */
    UFT_SPARTA_V3,                  /**< SpartaDOS 3.x (SDX) */
    UFT_SPARTA_X                    /**< SpartaDOS X */
} uft_sparta_version_t;

typedef enum {
    UFT_SPARTA_DENSITY_SD = 0,      /**< Single density (128 bytes) */
    UFT_SPARTA_DENSITY_ED,          /**< Enhanced density (128 bytes, more sectors) */
    UFT_SPARTA_DENSITY_DD,          /**< Double density (256 bytes) */
    UFT_SPARTA_DENSITY_QD           /**< Quad density (512 bytes) */
} uft_sparta_density_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Boot sector (first 3 sectors)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  flags;                 /**< Boot flags */
    uint8_t  boot_sectors;          /**< Number of boot sectors */
    uint16_t boot_addr;             /**< Boot address */
    uint16_t init_addr;             /**< Init address */
    uint8_t  jmp_opcode;            /**< JMP instruction (0x4C) */
    uint16_t jmp_addr;              /**< JMP target address */
    
    /* Volume info */
    uint8_t  volume_seq;            /**< Volume sequence number */
    uint8_t  volume_random;         /**< Volume random ID */
    uint16_t total_sectors;         /**< Total sectors on disk */
    uint16_t free_sectors;          /**< Free sectors */
    uint8_t  bitmap_sectors;        /**< Number of bitmap sectors */
    uint16_t bitmap_start;          /**< First bitmap sector */
    uint16_t data_start;            /**< First data sector */
    uint16_t dir_start;             /**< First directory sector */
    uint8_t  volume_name[8];        /**< Volume name (ASCII) */
    uint8_t  tracks;                /**< Number of tracks */
    uint8_t  sector_size;           /**< Sector size code */
    uint8_t  version;               /**< DOS version */
} uft_sparta_boot_t;
UFT_PACK_END

/**
 * @brief Directory entry
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  status;                /**< Status flags */
    uint16_t sector_map_start;      /**< First sector map sector */
    uint16_t file_length_lo;        /**< File length (low word) */
    uint8_t  file_length_hi;        /**< File length (high byte) */
    uint8_t  filename[8];           /**< Filename (padded with spaces) */
    uint8_t  extension[3];          /**< Extension (padded with spaces) */
    uint8_t  date_day;              /**< Date: day */
    uint8_t  date_month;            /**< Date: month */
    uint8_t  date_year;             /**< Date: year */
    uint8_t  time_hour;             /**< Time: hour */
    uint8_t  time_minute;           /**< Time: minute */
    uint8_t  time_second;           /**< Time: second */
} uft_sparta_dirent_t;
UFT_PACK_END

/**
 * @brief Sector map entry
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t next_map;              /**< Next sector map (0 if last) */
    uint8_t  sequence;              /**< Sequence number */
    uint8_t  sector_count;          /**< Sectors in this map */
    uint16_t sectors[62];           /**< Sector numbers (253 bytes / 2 - 2 header) */
} uft_sparta_sector_map_t;
UFT_PACK_END

/**
 * @brief Filesystem context
 */
typedef struct {
    uft_sparta_boot_t boot;
    uft_sparta_version_t version;
    uft_sparta_density_t density;
    
    uint16_t sector_size;
    uint32_t total_size;
    uint32_t free_size;
    
    /* Disk image access */
    const uint8_t *image;
    size_t image_size;
} uft_sparta_ctx_t;

/**
 * @brief File info
 */
typedef struct {
    char filename[UFT_SPARTA_MAX_FILENAME + 1];
    char extension[UFT_SPARTA_MAX_EXT + 1];
    char full_path[UFT_SPARTA_MAX_PATH];
    
    uint32_t size;
    bool is_directory;
    bool is_locked;
    bool is_deleted;
    
    uint16_t first_sector;
    
    /* Timestamp */
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} uft_sparta_fileinfo_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Detect SpartaDOS filesystem
 */
bool uft_sparta_detect(const uint8_t *image, size_t size);

/**
 * @brief Initialize filesystem context
 */
int uft_sparta_init(uft_sparta_ctx_t *ctx, const uint8_t *image, size_t size);

/**
 * @brief Get filesystem info
 */
int uft_sparta_get_info(const uft_sparta_ctx_t *ctx, char *buffer, size_t size);

/**
 * @brief List directory
 */
int uft_sparta_list_dir(uft_sparta_ctx_t *ctx, const char *path,
                        uft_sparta_fileinfo_t *files, size_t max_files,
                        size_t *count);

/**
 * @brief Get file info
 */
int uft_sparta_stat(uft_sparta_ctx_t *ctx, const char *path,
                    uft_sparta_fileinfo_t *info);

/**
 * @brief Read file
 */
int uft_sparta_read_file(uft_sparta_ctx_t *ctx, const char *path,
                         uint8_t *buffer, size_t max_size, size_t *bytes_read);

/**
 * @brief Extract all files
 */
int uft_sparta_extract_all(uft_sparta_ctx_t *ctx, const char *output_dir);

/**
 * @brief Get version name
 */
const char *uft_sparta_version_name(uft_sparta_version_t ver);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SPARTADOS_H */
