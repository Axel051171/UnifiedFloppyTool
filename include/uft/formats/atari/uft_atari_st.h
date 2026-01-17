/**
 * @file uft_atari_st.h
 * @brief Atari ST Disk Image Support
 * 
 * Support for Atari ST floppy disk formats:
 * - ST (.st) - Raw sector dump
 * - MSA (.msa) - Magic Shadow Archiver
 * - STX (.stx) - Pasti format (copy protection)
 * 
 * Features:
 * - Boot sector parsing
 * - Track/sector layout detection
 * - MSA compression/decompression
 * - FAT filesystem awareness
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_ATARI_ST_H
#define UFT_ATARI_ST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Sector size */
#define ST_SECTOR_SIZE          512

/** Standard disk sizes */
#define ST_SS_SD_SIZE           (360 * 1024)    /**< 360KB SS/SD */
#define ST_SS_DD_SIZE           (400 * 1024)    /**< 400KB SS/DD */
#define ST_DS_DD_SIZE           (720 * 1024)    /**< 720KB DS/DD */
#define ST_DS_HD_SIZE           (1440 * 1024)   /**< 1.44MB DS/HD */

/** MSA magic */
#define MSA_MAGIC               0x0E0F

/** Image types */
typedef enum {
    ST_FORMAT_UNKNOWN   = 0,
    ST_FORMAT_ST        = 1,    /**< Raw ST image */
    ST_FORMAT_MSA       = 2,    /**< MSA compressed */
    ST_FORMAT_STX       = 3     /**< Pasti format */
} st_format_t;

/** Disk types */
typedef enum {
    ST_DISK_SS_SD       = 0,    /**< Single-sided, single-density */
    ST_DISK_SS_DD       = 1,    /**< Single-sided, double-density */
    ST_DISK_DS_DD       = 2,    /**< Double-sided, double-density */
    ST_DISK_DS_HD       = 3     /**< Double-sided, high-density */
} st_disk_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Atari ST boot sector
 */
typedef struct {
    uint8_t     bra[2];             /**< Branch instruction */
    uint8_t     oem[6];             /**< OEM name */
    uint8_t     serial[3];          /**< Serial number */
    uint16_t    bps;                /**< Bytes per sector */
    uint8_t     spc;                /**< Sectors per cluster */
    uint16_t    res;                /**< Reserved sectors */
    uint8_t     nfats;              /**< Number of FATs */
    uint16_t    ndirs;              /**< Root directory entries */
    uint16_t    nsects;             /**< Total sectors */
    uint8_t     media;              /**< Media descriptor */
    uint16_t    spf;                /**< Sectors per FAT */
    uint16_t    spt;                /**< Sectors per track */
    uint16_t    nheads;             /**< Number of heads */
    uint16_t    nhid;               /**< Hidden sectors */
} st_bootsect_t;

/**
 * @brief MSA header
 */
typedef struct {
    uint16_t    magic;              /**< 0x0E0F */
    uint16_t    sectors_per_track;  /**< Sectors per track */
    uint16_t    sides;              /**< Sides (0=1, 1=2) */
    uint16_t    start_track;        /**< Starting track */
    uint16_t    end_track;          /**< Ending track */
} msa_header_t;

/**
 * @brief ST disk info
 */
typedef struct {
    st_format_t format;             /**< Image format */
    const char  *format_name;       /**< Format name */
    st_disk_type_t disk_type;       /**< Disk type */
    const char  *disk_name;         /**< Disk type name */
    size_t      file_size;          /**< File size */
    size_t      disk_size;          /**< Uncompressed disk size */
    uint16_t    tracks;             /**< Number of tracks */
    uint16_t    sectors_per_track;  /**< Sectors per track */
    uint16_t    sides;              /**< Number of sides */
    uint16_t    sector_size;        /**< Bytes per sector */
    bool        has_boot_sector;    /**< Has valid boot sector */
    bool        is_bootable;        /**< Is bootable */
} st_info_t;

/**
 * @brief ST disk context
 */
typedef struct {
    uint8_t     *data;              /**< Disk data (uncompressed) */
    size_t      size;               /**< Data size */
    st_format_t format;             /**< Original format */
    st_bootsect_t boot;             /**< Boot sector */
    bool        owns_data;          /**< true if we allocated */
} st_disk_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect disk image format
 * @param data Image data
 * @param size Data size
 * @return Image format
 */
st_format_t st_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Validate ST disk image
 * @param data Image data
 * @param size Data size
 * @return true if valid
 */
bool st_validate(const uint8_t *data, size_t size);

/**
 * @brief Get format name
 * @param format Image format
 * @return Format name
 */
const char *st_format_name(st_format_t format);

/**
 * @brief Get disk type name
 * @param type Disk type
 * @return Type name
 */
const char *st_disk_type_name(st_disk_type_t type);

/* ============================================================================
 * API Functions - Disk Operations
 * ============================================================================ */

/**
 * @brief Open ST disk image
 * @param data Image data
 * @param size Data size
 * @param disk Output disk context
 * @return 0 on success
 */
int st_open(const uint8_t *data, size_t size, st_disk_t *disk);

/**
 * @brief Load disk from file
 * @param filename File path
 * @param disk Output disk context
 * @return 0 on success
 */
int st_load(const char *filename, st_disk_t *disk);

/**
 * @brief Close disk
 * @param disk Disk to close
 */
void st_close(st_disk_t *disk);

/**
 * @brief Get disk info
 * @param disk Disk context
 * @param info Output info
 * @return 0 on success
 */
int st_get_info(const st_disk_t *disk, st_info_t *info);

/* ============================================================================
 * API Functions - MSA Compression
 * ============================================================================ */

/**
 * @brief Decompress MSA to raw ST
 * @param msa_data MSA data
 * @param msa_size MSA size
 * @param st_data Output ST data (allocated)
 * @param st_size Output ST size
 * @return 0 on success
 */
int st_msa_decompress(const uint8_t *msa_data, size_t msa_size,
                      uint8_t **st_data, size_t *st_size);

/**
 * @brief Compress ST to MSA format
 * @param st_data ST data
 * @param st_size ST size
 * @param msa_data Output MSA data (allocated)
 * @param msa_size Output MSA size
 * @return 0 on success
 */
int st_msa_compress(const uint8_t *st_data, size_t st_size,
                    uint8_t **msa_data, size_t *msa_size);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print disk info
 * @param disk Disk context
 * @param fp Output file
 */
void st_print_info(const st_disk_t *disk, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_ST_H */
