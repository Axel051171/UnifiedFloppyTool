/**
 * @file uft_amiga_bootblock.h
 * @brief Amiga Bootblock Analysis, Virus Detection and Recovery
 * 
 * Based on:
 * - XVS Library (xvs.library) API by Georg Hörmann
 * - XCopy bootblock detection
 * - DiskSalv recovery concepts
 * 
 * Features:
 * - Bootblock type identification
 * - Known virus signature detection
 * - Standard bootblock installation
 * - Checksum calculation and repair
 */

#ifndef UFT_AMIGA_BOOTBLOCK_H
#define UFT_AMIGA_BOOTBLOCK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * BOOTBLOCK CONSTANTS
 *============================================================================*/

#define UFT_AMIGA_BOOTBLOCK_SIZE    1024    /* 2 sectors */
#define UFT_AMIGA_BOOTBLOCK_WORDS   256     /* 32-bit words */

/*============================================================================
 * BOOTBLOCK TYPES (based on XVS Library)
 *============================================================================*/

typedef enum {
    UFT_BB_UNKNOWN      = 0,    /* Unknown bootblock */
    UFT_BB_NOT_DOS      = 1,    /* Not a DOS bootblock (no DOS magic) */
    UFT_BB_STANDARD_13  = 2,    /* Standard Kickstart 1.3 bootblock */
    UFT_BB_STANDARD_20  = 3,    /* Standard Kickstart 2.0+ bootblock */
    UFT_BB_VIRUS        = 4,    /* Known virus detected */
    UFT_BB_CUSTOM       = 5,    /* Custom/game bootblock (not virus) */
    UFT_BB_INSTALLER    = 6,    /* Disk installer bootblock */
    UFT_BB_FASTMEM      = 7,    /* FastMem loader bootblock */
    UFT_BB_NOCLICK      = 8,    /* NoClick bootblock */
    UFT_BB_CORRUPT      = 9     /* Corrupted/damaged bootblock */
} uft_amiga_bb_type_t;

/*============================================================================
 * VIRUS SIGNATURE
 *============================================================================*/

typedef struct uft_amiga_virus_sig {
    const char* name;           /* Virus name */
    uint32_t offset;            /* Offset in bootblock */
    uint32_t length;            /* Signature length */
    const uint8_t* signature;   /* Signature bytes */
    const char* description;    /* Description */
    bool is_dangerous;          /* Destructive virus? */
} uft_amiga_virus_sig_t;

/*============================================================================
 * BOOTBLOCK INFO (based on XVS xvsBootInfo)
 *============================================================================*/

typedef struct uft_amiga_bootblock_info {
    /* Raw data */
    uint8_t data[UFT_AMIGA_BOOTBLOCK_SIZE];
    
    /* Parsed info */
    uft_amiga_bb_type_t type;
    int dos_type;               /* 0=OFS, 1=FFS, etc. (-1 if not DOS) */
    
    /* Checksum */
    uint32_t checksum_stored;
    uint32_t checksum_computed;
    bool checksum_valid;
    
    /* Virus info (if type == UFT_BB_VIRUS) */
    const char* virus_name;
    const char* virus_description;
    bool virus_dangerous;
    
    /* Custom bootblock info */
    const char* custom_name;    /* If recognized custom bootblock */
    
    /* Statistics */
    int executable_offset;      /* Offset to executable code */
    int code_size;              /* Size of boot code */
    bool has_disk_io;           /* Uses trackdisk.device */
    bool has_dos_calls;         /* Uses DOS library */
} uft_amiga_bootblock_info_t;

/*============================================================================
 * KNOWN BOOTBLOCKS DATABASE
 *============================================================================*/

/**
 * @brief Standard Kickstart 1.3 bootblock
 * 
 * The original AmigaDOS bootblock from Workbench 1.3.
 * Can be used to restore infected disks.
 */
extern const uint8_t UFT_AMIGA_BOOTBLOCK_13[UFT_AMIGA_BOOTBLOCK_SIZE];

/**
 * @brief Standard Kickstart 2.0+ bootblock
 * 
 * The AmigaDOS bootblock from Workbench 2.0 and later.
 */
extern const uint8_t UFT_AMIGA_BOOTBLOCK_20[UFT_AMIGA_BOOTBLOCK_SIZE];

/**
 * @brief Get virus signature database
 * 
 * @param count Output: number of signatures
 * @return Array of virus signatures
 */
const uft_amiga_virus_sig_t* uft_amiga_get_virus_db(size_t* count);

/*============================================================================
 * BOOTBLOCK ANALYSIS
 *============================================================================*/

/**
 * @brief Analyze bootblock and detect type/virus
 * 
 * @param bootblock Raw 1024-byte bootblock data
 * @param info Output: analysis results
 * @return Type of bootblock detected
 */
uft_amiga_bb_type_t uft_amiga_analyze_bootblock(const uint8_t* bootblock,
                                                 uft_amiga_bootblock_info_t* info);

/**
 * @brief Check if bootblock contains a known virus
 * 
 * @param bootblock Raw bootblock data
 * @param virus_name Output: virus name if found (can be NULL)
 * @return true if virus detected
 */
bool uft_amiga_check_bootblock_virus(const uint8_t* bootblock,
                                      const char** virus_name);

/**
 * @brief Identify custom/game bootblock
 * 
 * Many games use custom bootblocks that are not viruses.
 * This function tries to identify known custom bootblocks.
 * 
 * @param bootblock Raw bootblock data
 * @return Name of custom bootblock, or NULL if unknown
 */
const char* uft_amiga_identify_custom_bootblock(const uint8_t* bootblock);

/*============================================================================
 * BOOTBLOCK REPAIR
 *============================================================================*/

/**
 * @brief Install standard bootblock
 * 
 * Replaces the bootblock with a standard AmigaDOS bootblock.
 * Preserves the DOS type (OFS/FFS/etc).
 * 
 * @param bootblock Buffer to write (must be 1024 bytes)
 * @param dos_type DOS type (0-5)
 * @param kickstart_version 13 for 1.3, 20 for 2.0+
 * @return 0 on success
 */
int uft_amiga_install_bootblock(uint8_t* bootblock, int dos_type, int kickstart_version);

/**
 * @brief Fix bootblock checksum
 * 
 * Recalculates and writes the correct checksum.
 * 
 * @param bootblock Bootblock to fix (modified in place)
 */
void uft_amiga_fix_bootblock_checksum(uint8_t* bootblock);

/**
 * @brief Calculate bootblock checksum
 * 
 * @param bootblock Bootblock data
 * @return Calculated checksum value
 */
uint32_t uft_amiga_calc_bootblock_checksum(const uint8_t* bootblock);

/*============================================================================
 * SECTOR-LEVEL VIRUS DETECTION (based on XVS xvsSectorInfo)
 *============================================================================*/

typedef enum {
    UFT_SECTOR_UNKNOWN   = 0,   /* Unknown sector */
    UFT_SECTOR_NORMAL    = 1,   /* Normal filesystem sector */
    UFT_SECTOR_DESTROYED = 2,   /* Destroyed by virus */
    UFT_SECTOR_INFECTED  = 3    /* Contains virus payload */
} uft_amiga_sector_status_t;

typedef struct uft_amiga_sector_info {
    uint8_t data[512];
    uint32_t block_number;
    uft_amiga_sector_status_t status;
    const char* virus_name;     /* If infected/destroyed */
} uft_amiga_sector_info_t;

/**
 * @brief Check sector for virus signatures
 * 
 * Some viruses infect filesystem sectors (root block, directory blocks).
 * 
 * @param sector_data 512-byte sector data
 * @param block_number Block number on disk
 * @param info Output: sector analysis
 * @return Sector status
 */
uft_amiga_sector_status_t uft_amiga_check_sector(const uint8_t* sector_data,
                                                  uint32_t block_number,
                                                  uft_amiga_sector_info_t* info);

/*============================================================================
 * DISK-LEVEL SCANNING
 *============================================================================*/

typedef struct uft_amiga_scan_result {
    bool bootblock_infected;
    const char* bootblock_virus;
    
    int infected_sectors;
    int destroyed_sectors;
    
    bool root_block_ok;
    bool bitmap_ok;
    
    /* Recovery recommendations */
    bool can_recover_bootblock;
    bool can_recover_filesystem;
    const char* recovery_notes;
} uft_amiga_scan_result_t;

/**
 * @brief Scan entire ADF for viruses
 * 
 * @param adf_data Complete ADF image data
 * @param adf_size Size of ADF (880KB or 1.76MB)
 * @param result Output: scan results
 * @return Number of infections found
 */
int uft_amiga_scan_adf(const uint8_t* adf_data, size_t adf_size,
                       uft_amiga_scan_result_t* result);

/*============================================================================
 * BRAINFILE FORMAT (for virus databases)
 *============================================================================*/

/**
 * @brief Load virus signatures from brainfile
 * 
 * Brainfile is the format used by various Amiga virus scanners.
 * 
 * @param filename Path to brainfile
 * @param signatures Output: loaded signatures
 * @param max_signatures Maximum number to load
 * @return Number of signatures loaded
 */
int uft_amiga_load_brainfile(const char* filename,
                             uft_amiga_virus_sig_t* signatures,
                             int max_signatures);

/*============================================================================
 * RECOVERY OPERATIONS (DiskSalv-style)
 *============================================================================*/

typedef struct uft_amiga_recovery_options {
    bool repair_bootblock;      /* Install standard bootblock */
    bool repair_root_block;     /* Attempt root block recovery */
    bool repair_bitmap;         /* Rebuild bitmap from directory */
    bool repair_directories;    /* Fix directory chain errors */
    bool strict_mode;           /* Report only, don't modify */
    int kickstart_version;      /* 13 or 20 for bootblock */
} uft_amiga_recovery_options_t;

typedef struct uft_amiga_recovery_result {
    int errors_found;
    int errors_fixed;
    int files_recovered;
    int blocks_recovered;
    char log[4096];             /* Recovery log */
} uft_amiga_recovery_result_t;

/**
 * @brief Attempt to recover damaged ADF
 * 
 * @param adf_data ADF image (modified in place if not strict_mode)
 * @param adf_size Size of ADF
 * @param options Recovery options
 * @param result Output: recovery results
 * @return 0 on success
 */
int uft_amiga_recover_adf(uint8_t* adf_data, size_t adf_size,
                          const uft_amiga_recovery_options_t* options,
                          uft_amiga_recovery_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_BOOTBLOCK_H */
