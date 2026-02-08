/**
 * @file uft_pc_cdrom_protection.h
 * @brief PC CD-ROM Copy Protection Detection
 * 
 * Detects and analyzes PC CD-ROM copy protection schemes:
 * - SafeDisc (Macrovision)
 * - SecuROM (Sony DADC)
 * - LaserLock
 * - ProtectCD
 * - StarForce
 * 
 * Note: These are primarily CD/DVD protections, but UFT can
 * analyze the floppy-based components and signatures.
 */

#ifndef UFT_PC_CDROM_PROTECTION_H
#define UFT_PC_CDROM_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** SafeDisc signatures */
#define UFT_SAFEDISC_SIG_V1     "BoG_ *90.0&!!"
#define UFT_SAFEDISC_SIG_V2     "~SD~"
#define UFT_SAFEDISC_CLCD_SIG   "CLCD"
#define UFT_SAFEDISC_STXT_SIG   "stxt"

/** SecuROM signatures */
#define UFT_SECUROM_SIG_V4      "~@&@~"
#define UFT_SECUROM_CMS_SIG     "CMS16"
#define UFT_SECUROM_DAT_SIG     ".cms"

/** Weak sector characteristics */
#define UFT_WEAK_SECTOR_MIN     1       /**< Minimum weak sectors */
#define UFT_WEAK_SECTOR_MAX     1000    /**< Maximum weak sectors */

/*===========================================================================
 * Protection Types
 *===========================================================================*/

/**
 * @brief PC CD protection types
 */
typedef enum {
    UFT_PC_PROT_NONE = 0,
    UFT_PC_PROT_SAFEDISC,           /**< Macrovision SafeDisc */
    UFT_PC_PROT_SAFEDISC2,          /**< SafeDisc v2.x */
    UFT_PC_PROT_SAFEDISC3,          /**< SafeDisc v3.x */
    UFT_PC_PROT_SAFEDISC4,          /**< SafeDisc v4.x */
    UFT_PC_PROT_SECUROM,            /**< Sony SecuROM */
    UFT_PC_PROT_SECUROM_NEW,        /**< SecuROM New (v7+) */
    UFT_PC_PROT_LASERLOCK,          /**< LaserLock */
    UFT_PC_PROT_PROTECTCD,          /**< ProtectCD-VOB */
    UFT_PC_PROT_STARFORCE,          /**< StarForce */
    UFT_PC_PROT_MULTIPLE            /**< Multiple protections */
} uft_pc_prot_type_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief SafeDisc protection info
 */
typedef struct {
    /* Version detection */
    uint8_t  major_version;         /**< Major version (1-4) */
    uint8_t  minor_version;         /**< Minor version */
    uint16_t build_version;         /**< Build number */
    
    /* Signature locations */
    uint32_t sig_offset;            /**< Signature file offset */
    char     sig_file[64];          /**< File containing signature */
    
    /* Weak sectors */
    uint32_t weak_sector_start;     /**< First weak sector */
    uint32_t weak_sector_count;     /**< Number of weak sectors */
    
    /* Component files */
    bool     has_clcd;              /**< CLCD32.DLL present */
    bool     has_clokspl;           /**< CLOKSPL.EXE present */
    bool     has_drvmgt;            /**< drvmgt.dll present */
    bool     has_secdrv;            /**< secdrv.sys present */
    
    /* Digital signature */
    uint8_t  digital_sig[64];       /**< Digital signature (if found) */
    bool     sig_valid;             /**< Signature validation result */
    
    bool     detected;              /**< SafeDisc detected */
    double   confidence;            /**< Detection confidence */
} uft_safedisc_t;

/**
 * @brief SecuROM protection info
 */
typedef struct {
    /* Version detection */
    uint8_t  major_version;         /**< Major version */
    uint8_t  minor_version;         /**< Minor version */
    
    /* Signature locations */
    uint32_t sig_offset;            /**< Signature offset in executable */
    char     exe_name[64];          /**< Protected executable name */
    
    /* CMS data */
    uint32_t cms_offset;            /**< CMS data offset */
    uint32_t cms_size;              /**< CMS data size */
    uint8_t  cms_key[16];           /**< CMS encryption key */
    
    /* Trigger sector */
    uint32_t trigger_sector;        /**< Trigger sector LBA */
    uint8_t  trigger_data[16];      /**< Expected trigger data */
    
    /* Sub-channel data */
    bool     uses_subchannel;       /**< Uses sub-channel data */
    uint8_t  subchannel_key[16];    /**< Sub-channel key */
    
    bool     detected;              /**< SecuROM detected */
    double   confidence;            /**< Detection confidence */
} uft_securom_t;

/**
 * @brief Weak sector info (for both protections)
 */
typedef struct {
    uint32_t lba;                   /**< Logical block address */
    uint32_t position;              /**< Position in track */
    uint8_t  expected_edc[4];       /**< Expected EDC */
    uint8_t  actual_edc[4];         /**< Actual EDC */
    bool     edc_mismatch;          /**< EDC mismatch flag */
    double   signal_variance;       /**< Signal variance at sector */
} uft_weak_sector_t;

/**
 * @brief Combined PC CD protection result
 */
typedef struct {
    uft_pc_prot_type_t primary_type; /**< Primary protection type */
    uint32_t type_flags;              /**< All detected types */
    
    /* Protection details */
    uft_safedisc_t safedisc;          /**< SafeDisc info */
    uft_securom_t securom;            /**< SecuROM info */
    
    /* Weak sectors */
    uft_weak_sector_t *weak_sectors;  /**< Weak sector array */
    uint32_t weak_sector_count;       /**< Weak sector count */
    
    /* File signatures */
    char detected_files[8][64];       /**< Protection files found */
    uint8_t file_count;               /**< Number of files found */
    
    /* Summary */
    double overall_confidence;        /**< Overall confidence */
    char description[256];            /**< Human-readable description */
    char version_string[32];          /**< Version string */
} uft_pc_prot_result_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

typedef struct {
    bool detect_safedisc;           /**< Detect SafeDisc */
    bool detect_securom;            /**< Detect SecuROM */
    bool detect_others;             /**< Detect other protections */
    bool scan_executables;          /**< Scan EXE/DLL files */
    bool analyze_weak_sectors;      /**< Analyze weak sectors */
    
    uint32_t max_weak_sectors;      /**< Maximum weak sectors to analyze */
} uft_pc_detect_config_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Initialize detection config
 */
void uft_pc_config_init(uft_pc_detect_config_t *config);

/**
 * @brief Allocate protection result
 */
uft_pc_prot_result_t *uft_pc_result_alloc(void);

/**
 * @brief Free protection result
 */
void uft_pc_result_free(uft_pc_prot_result_t *result);

/**
 * @brief Detect SafeDisc from executable
 * 
 * @param exe_data Executable file data
 * @param exe_len Executable length
 * @param filename Original filename
 * @param result Output SafeDisc result
 * @return 0 on success
 */
int uft_pc_detect_safedisc(const uint8_t *exe_data, size_t exe_len,
                           const char *filename, uft_safedisc_t *result);

/**
 * @brief Detect SecuROM from executable
 * 
 * @param exe_data Executable file data
 * @param exe_len Executable length
 * @param filename Original filename
 * @param result Output SecuROM result
 * @return 0 on success
 */
int uft_pc_detect_securom(const uint8_t *exe_data, size_t exe_len,
                          const char *filename, uft_securom_t *result);

/**
 * @brief Analyze weak sectors
 * 
 * @param sector_data Array of sector data (multiple reads)
 * @param read_count Number of reads per sector
 * @param sector_count Number of sectors
 * @param lba_start Starting LBA
 * @param results Output weak sector array
 * @param max_results Maximum results
 * @return Number of weak sectors found
 */
size_t uft_pc_analyze_weak_sectors(const uint8_t **sector_data,
                                   uint8_t read_count, uint32_t sector_count,
                                   uint32_t lba_start,
                                   uft_weak_sector_t *results,
                                   size_t max_results);

/**
 * @brief Detect SafeDisc version from signature
 * 
 * @param signature Signature bytes
 * @param sig_len Signature length
 * @param major Output major version
 * @param minor Output minor version
 * @return 0 on success
 */
int uft_pc_safedisc_version(const uint8_t *signature, size_t sig_len,
                            uint8_t *major, uint8_t *minor);

/**
 * @brief Detect SecuROM version
 */
int uft_pc_securom_version(const uint8_t *signature, size_t sig_len,
                           uint8_t *major, uint8_t *minor);

/**
 * @brief Scan directory for protection files
 * 
 * @param files Array of filenames
 * @param file_count Number of files
 * @param result Output protection result
 * @return Number of protection files found
 */
size_t uft_pc_scan_files(const char **files, size_t file_count,
                         uft_pc_prot_result_t *result);

/**
 * @brief Full protection detection
 */
int uft_pc_detect_all(const uint8_t **exe_data, const size_t *exe_lens,
                      const char **filenames, size_t file_count,
                      const uint8_t **sector_data, uint8_t read_count,
                      uint32_t sector_count, uint32_t lba_start,
                      const uft_pc_detect_config_t *config,
                      uft_pc_prot_result_t *result);

/**
 * @brief Get protection type name
 */
const char *uft_pc_prot_name(uft_pc_prot_type_t type);

/**
 * @brief Export result to JSON
 */
int uft_pc_result_to_json(const uft_pc_prot_result_t *result,
                          char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PC_CDROM_PROTECTION_H */
