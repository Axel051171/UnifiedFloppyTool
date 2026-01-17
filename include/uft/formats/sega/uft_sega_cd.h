/**
 * @file uft_sega_cd.h
 * @brief Sega Saturn / Dreamcast CD-ROM Support
 * 
 * Support for Sega CD-based consoles:
 * - Sega Saturn (.iso, .bin/.cue)
 * - Sega Dreamcast (.gdi, .cdi)
 * 
 * Features:
 * - IP.BIN header parsing
 * - GDI track parsing
 * - Region detection
 * - Disc info extraction
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_SEGA_CD_H
#define UFT_SEGA_CD_H

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

/** IP.BIN location */
#define SATURN_IP_OFFSET        0
#define DC_IP_OFFSET            0

/** Sector sizes */
#define SECTOR_RAW              2352
#define SECTOR_MODE1            2048
#define SECTOR_MODE2            2336

/** Platform types */
typedef enum {
    SEGA_CD_UNKNOWN     = 0,
    SEGA_CD_SATURN      = 1,
    SEGA_CD_DREAMCAST   = 2
} sega_cd_platform_t;

/** Disc format */
typedef enum {
    SEGA_CD_FORMAT_UNKNOWN  = 0,
    SEGA_CD_FORMAT_ISO      = 1,    /**< ISO-9660 */
    SEGA_CD_FORMAT_BIN_CUE  = 2,    /**< BIN/CUE */
    SEGA_CD_FORMAT_GDI      = 3,    /**< Dreamcast GDI */
    SEGA_CD_FORMAT_CDI      = 4     /**< DiscJuggler */
} sega_cd_format_t;

/** Region codes */
typedef enum {
    SEGA_CD_REGION_UNKNOWN  = 0,
    SEGA_CD_REGION_JAPAN    = 'J',
    SEGA_CD_REGION_USA      = 'U',
    SEGA_CD_REGION_EUROPE   = 'E'
} sega_cd_region_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Saturn IP.BIN header
 */
typedef struct {
    char        hardware_id[16];    /**< "SEGA SEGASATURN " */
    char        maker_id[16];       /**< Maker ID */
    char        product_number[10]; /**< Product number */
    char        version[6];         /**< Version */
    char        release_date[8];    /**< YYYYMMDD */
    char        device_info[8];     /**< Device info */
    char        area_symbols[10];   /**< Area symbols (JUE etc) */
    char        reserved1[6];       /**< Reserved */
    char        peripheral[16];     /**< Peripheral info */
    char        title[112];         /**< Game title */
    char        reserved2[16];      /**< Reserved */
    uint32_t    ip_size;            /**< IP size */
    uint32_t    reserved3;          /**< Reserved */
    uint32_t    master_stack;       /**< Master SH2 stack */
    uint32_t    slave_stack;        /**< Slave SH2 stack */
    uint32_t    first_read_addr;    /**< First read address */
    uint32_t    first_read_size;    /**< First read size */
} saturn_ip_t;

/**
 * @brief Dreamcast IP.BIN header
 */
typedef struct {
    char        hardware_id[16];    /**< "SEGA SEGAKATANA " */
    char        maker_id[16];       /**< Maker ID */
    char        device_info[16];    /**< "GD-ROM" etc */
    char        area_symbols[8];    /**< Region symbols */
    char        peripherals[8];     /**< Controller support */
    char        product_number[10]; /**< Product number */
    char        version[6];         /**< Version */
    char        release_date[16];   /**< Release date */
    char        boot_filename[16];  /**< Boot file */
    char        software_maker[16]; /**< Software maker */
    char        title[128];         /**< Game title */
} dreamcast_ip_t;

/**
 * @brief GDI track entry
 */
typedef struct {
    int         track_num;          /**< Track number */
    int         lba;                /**< Starting LBA */
    int         type;               /**< 0=audio, 4=data */
    int         sector_size;        /**< Sector size */
    char        filename[256];      /**< Track filename */
    int         offset;             /**< File offset */
} gdi_track_t;

/**
 * @brief Sega CD info
 */
typedef struct {
    sega_cd_platform_t platform;    /**< Platform */
    const char  *platform_name;     /**< Platform name */
    sega_cd_format_t format;        /**< Disc format */
    const char  *format_name;       /**< Format name */
    char        title[129];         /**< Game title */
    char        product_number[11]; /**< Product number */
    char        version[7];         /**< Version */
    char        maker_id[17];       /**< Maker ID */
    char        release_date[17];   /**< Release date */
    sega_cd_region_t region;        /**< Primary region */
    bool        region_japan;       /**< Supports Japan */
    bool        region_usa;         /**< Supports USA */
    bool        region_europe;      /**< Supports Europe */
    int         track_count;        /**< Number of tracks (GDI) */
} sega_cd_info_t;

/**
 * @brief Sega CD context
 */
typedef struct {
    uint8_t     *data;              /**< Image data */
    size_t      size;               /**< Data size */
    sega_cd_platform_t platform;    /**< Detected platform */
    sega_cd_format_t format;        /**< Image format */
    saturn_ip_t saturn_ip;          /**< Saturn IP.BIN */
    dreamcast_ip_t dc_ip;           /**< Dreamcast IP.BIN */
    gdi_track_t *tracks;            /**< GDI tracks */
    int         track_count;        /**< Track count */
    bool        owns_data;          /**< true if we allocated */
} sega_cd_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

sega_cd_platform_t sega_cd_detect_platform(const uint8_t *data, size_t size);
sega_cd_format_t sega_cd_detect_format(const uint8_t *data, size_t size);
const char *sega_cd_platform_name(sega_cd_platform_t platform);
const char *sega_cd_format_name(sega_cd_format_t format);

int sega_cd_open(const uint8_t *data, size_t size, sega_cd_t *cd);
int sega_cd_load(const char *filename, sega_cd_t *cd);
void sega_cd_close(sega_cd_t *cd);
int sega_cd_get_info(const sega_cd_t *cd, sega_cd_info_t *info);

int sega_gdi_parse(const char *gdi_content, gdi_track_t **tracks, int *count);
void sega_cd_print_info(const sega_cd_t *cd, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SEGA_CD_H */
