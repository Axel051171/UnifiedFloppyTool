/**
 * @file uft_apd_format.h
 * @brief Acorn Protected Disk (APD) Format Handler
 * 
 * APD is a flux-level disk image format for Acorn Archimedes
 * copy-protected disks. Created by Tom Walker for Arculator.
 * 
 * Features:
 * - Raw FM/MFM bitstream storage
 * - Triple density support (SD/DD/QD per track)
 * - GZip compression
 * - 160 tracks (80 cylinders × 2 heads)
 * - Used by Arculator, ADFFS, RPCEmu
 * 
 * Supported platforms:
 * - Acorn Archimedes (A300/A400/A3000/A5000)
 * - Acorn RISC PC
 * - BBC Micro (with ADFS)
 * 
 * Reference: ADFFS !Structure documentation by Jon Abbott
 * Reference: kryotools by Daniel Sheridan (drdpj)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_APD_FORMAT_H
#define UFT_APD_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * APD FORMAT CONSTANTS
 *===========================================================================*/

/** APD magic identifier */
#define UFT_APD_MAGIC           "APDX0001"
#define UFT_APD_MAGIC_LEN       8

/** Number of tracks in APD */
#define UFT_APD_NUM_TRACKS      166
#define UFT_APD_USED_TRACKS     160  /**< Tracks 0-159 used, 160-165 blank */

/** Header size (track table) */
#define UFT_APD_HEADER_SIZE     (UFT_APD_MAGIC_LEN + (UFT_APD_NUM_TRACKS * 12))

/** Track densities */
#define UFT_APD_DENSITY_SD      0    /**< Single Density (FM) */
#define UFT_APD_DENSITY_DD      1    /**< Double Density (MFM) */
#define UFT_APD_DENSITY_QD      2    /**< Quad Density (MFM) */
#define UFT_APD_NUM_DENSITIES   3

/** Acorn ADFS sector sizes */
#define UFT_ADFS_SECTOR_SIZE_S  256  /**< Single density */
#define UFT_ADFS_SECTOR_SIZE_D  1024 /**< Double density (E format) */
#define UFT_ADFS_SECTOR_SIZE_L  256  /**< L format */

/** Acorn ADFS formats */
#define UFT_ADFS_FORMAT_S       0    /**< S: 40T SS SD 100K */
#define UFT_ADFS_FORMAT_M       1    /**< M: 80T SS SD 200K */
#define UFT_ADFS_FORMAT_L       2    /**< L: 80T DS SD 640K */
#define UFT_ADFS_FORMAT_D       3    /**< D: 80T DS DD 800K */
#define UFT_ADFS_FORMAT_E       4    /**< E: 80T DS DD 800K (new map) */
#define UFT_ADFS_FORMAT_F       5    /**< F: 80T DS QD 1600K */
#define UFT_ADFS_FORMAT_G       6    /**< G: HD 1.6M */

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief APD track entry in header
 */
typedef struct {
    uint32_t sd_bits;           /**< Single density track length in bits */
    uint32_t dd_bits;           /**< Double density track length in bits */
    uint32_t qd_bits;           /**< Quad density track length in bits */
} uft_apd_track_entry_t;

/**
 * @brief APD track data
 */
typedef struct {
    uint8_t *sd_data;           /**< Single density raw FM data */
    size_t sd_size;             /**< SD data size in bytes */
    uint32_t sd_bits;           /**< SD data size in bits */
    
    uint8_t *dd_data;           /**< Double density raw MFM data */
    size_t dd_size;             /**< DD data size in bytes */
    uint32_t dd_bits;           /**< DD data size in bits */
    
    uint8_t *qd_data;           /**< Quad density raw MFM data */
    size_t qd_size;             /**< QD data size in bytes */
    uint32_t qd_bits;           /**< QD data size in bits */
    
    int track_num;              /**< Track number (0-159) */
    int cylinder;               /**< Cylinder (0-79) */
    int head;                   /**< Head (0-1) */
} uft_apd_track_t;

/**
 * @brief APD disk info
 */
typedef struct {
    int num_tracks;             /**< Number of tracks with data */
    int format;                 /**< Detected ADFS format */
    bool has_sd;                /**< Has single density tracks */
    bool has_dd;                /**< Has double density tracks */
    bool has_qd;                /**< Has quad density tracks */
    bool is_protected;          /**< Has copy protection */
    size_t total_size;          /**< Total uncompressed size */
    char volume_name[16];       /**< Volume name if detected */
} uft_apd_info_t;

/**
 * @brief Acorn sector info
 */
typedef struct {
    int cylinder;               /**< Cylinder number */
    int head;                   /**< Head number */
    int sector;                 /**< Sector number */
    int size;                   /**< Sector size (256/512/1024) */
    uint16_t crc;               /**< Sector CRC */
    bool crc_valid;             /**< CRC matches */
    bool deleted;               /**< Deleted data mark */
    uint32_t data_offset;       /**< Offset in track data */
} uft_acorn_sector_t;

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

typedef struct uft_apd_context uft_apd_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create APD context
 */
uft_apd_t* uft_apd_create(void);

/**
 * @brief Destroy APD context
 */
void uft_apd_destroy(uft_apd_t *apd);

/*===========================================================================
 * FILE OPERATIONS
 *===========================================================================*/

/**
 * @brief Open APD file for reading
 * 
 * @param apd Context
 * @param path Path to .apd file
 * @return 0 on success
 */
int uft_apd_open(uft_apd_t *apd, const char *path);

/**
 * @brief Create new APD file
 * 
 * @param apd Context
 * @param path Output path
 * @return 0 on success
 */
int uft_apd_create_file(uft_apd_t *apd, const char *path);

/**
 * @brief Save APD to file
 */
int uft_apd_save(uft_apd_t *apd, const char *path);

/**
 * @brief Close APD file
 */
void uft_apd_close(uft_apd_t *apd);

/**
 * @brief Get disk info
 */
int uft_apd_get_info(uft_apd_t *apd, uft_apd_info_t *info);

/*===========================================================================
 * DETECTION
 *===========================================================================*/

/**
 * @brief Check if file is APD format
 * 
 * @param data File data
 * @param size Data size
 * @return Detection score (0-100)
 */
int uft_apd_detect(const uint8_t *data, size_t size);

/**
 * @brief Check if file is APD by path
 */
bool uft_apd_detect_file(const char *path);

/**
 * @brief Detect ADFS format from APD
 */
int uft_apd_detect_format(uft_apd_t *apd);

/*===========================================================================
 * TRACK OPERATIONS
 *===========================================================================*/

/**
 * @brief Read track from APD
 * 
 * @param apd Context
 * @param track_num Track number (0-159)
 * @param track Output: track data
 * @return 0 on success
 */
int uft_apd_read_track(uft_apd_t *apd, int track_num, uft_apd_track_t *track);

/**
 * @brief Write track to APD
 * 
 * @param apd Context
 * @param track Track data to write
 * @return 0 on success
 */
int uft_apd_write_track(uft_apd_t *apd, const uft_apd_track_t *track);

/**
 * @brief Free track data
 */
void uft_apd_track_free(uft_apd_track_t *track);

/**
 * @brief Get track by cylinder/head
 */
int uft_apd_get_track(uft_apd_t *apd, int cylinder, int head, 
                       uft_apd_track_t *track);

/*===========================================================================
 * SECTOR OPERATIONS
 *===========================================================================*/

/**
 * @brief Decode sectors from track
 * 
 * @param track Track data
 * @param density Which density to decode (SD/DD/QD)
 * @param sectors Output: sector array
 * @param max_sectors Maximum sectors to return
 * @return Number of sectors found
 */
int uft_apd_decode_sectors(const uft_apd_track_t *track, int density,
                            uft_acorn_sector_t *sectors, int max_sectors);

/**
 * @brief Read sector data
 * 
 * @param track Track data
 * @param sector Sector info
 * @param data Output buffer
 * @param max_size Buffer size
 * @return Bytes read, or -1 on error
 */
int uft_apd_read_sector(const uft_apd_track_t *track,
                         const uft_acorn_sector_t *sector,
                         uint8_t *data, size_t max_size);

/**
 * @brief Read logical sector
 * 
 * @param apd Context
 * @param lba Logical block address
 * @param data Output buffer
 * @param sector_size Expected sector size
 * @return Bytes read
 */
int uft_apd_read_logical_sector(uft_apd_t *apd, uint32_t lba,
                                 uint8_t *data, int sector_size);

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

/**
 * @brief Convert APD to Acorn ADF (sector image)
 * 
 * @param apd Source APD context
 * @param adf_path Output ADF path
 * @return 0 on success
 */
int uft_apd_to_adf(uft_apd_t *apd, const char *adf_path);

/**
 * @brief Convert Acorn ADF to APD
 * 
 * @param adf_path Source ADF path
 * @param apd_path Output APD path
 * @return 0 on success
 */
int uft_adf_to_apd(const char *adf_path, const char *apd_path);

/**
 * @brief Convert KryoFlux RAW to APD
 * 
 * @param kf_dir Directory with track00.0.raw files
 * @param apd_path Output APD path
 * @param scan_fm Also scan for FM sectors
 * @return 0 on success
 */
int uft_kryoflux_to_apd(const char *kf_dir, const char *apd_path, bool scan_fm);

/**
 * @brief Convert SCP to APD
 */
int uft_scp_to_apd(const char *scp_path, const char *apd_path);

/*===========================================================================
 * FM/MFM DECODING (ACORN SPECIFIC)
 *===========================================================================*/

/**
 * @brief Find FM sync in bitstream
 * 
 * Acorn uses different sync patterns than IBM/Amiga
 */
int uft_apd_find_fm_sync(const uint8_t *data, size_t bits, int start);

/**
 * @brief Find MFM sync in bitstream
 */
int uft_apd_find_mfm_sync(const uint8_t *data, size_t bits, int start);

/**
 * @brief Decode FM byte from bitstream
 */
int uft_apd_decode_fm_byte(const uint8_t *data, int bit_offset);

/**
 * @brief Decode MFM byte from bitstream
 */
int uft_apd_decode_mfm_byte(const uint8_t *data, int bit_offset);

/**
 * @brief Encode data as FM bitstream
 */
int uft_apd_encode_fm(const uint8_t *data, size_t len,
                       uint8_t *fm_out, size_t fm_max);

/**
 * @brief Encode data as MFM bitstream
 */
int uft_apd_encode_mfm(const uint8_t *data, size_t len,
                        uint8_t *mfm_out, size_t mfm_max);

/*===========================================================================
 * PROTECTION DETECTION
 *===========================================================================*/

/**
 * @brief Known Acorn protection types
 */
typedef enum {
    UFT_ACORN_PROT_NONE = 0,
    UFT_ACORN_PROT_WEAK_BITS,       /**< Weak/fuzzy bits */
    UFT_ACORN_PROT_LONG_TRACK,      /**< Track >160 */
    UFT_ACORN_PROT_MIXED_DENSITY,   /**< SD/DD sectors on same track */
    UFT_ACORN_PROT_INVALID_ID,      /**< Invalid sector IDs */
    UFT_ACORN_PROT_CRC_ERROR,       /**< Intentional CRC errors */
    UFT_ACORN_PROT_DUPLICATE,       /**< Duplicate sector IDs */
    UFT_ACORN_PROT_SECTOR_IN_SECTOR,/**< Nested sectors */
    UFT_ACORN_PROT_UNFORMATTED,     /**< Unformatted track check */
    UFT_ACORN_PROT_QD_TRACK         /**< Quad density track */
} uft_acorn_protection_t;

/**
 * @brief Detect protection type
 */
uft_acorn_protection_t uft_apd_detect_protection(uft_apd_t *apd);

/**
 * @brief Get protection name
 */
const char* uft_acorn_protection_name(uft_acorn_protection_t prot);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Get ADFS format name
 */
const char* uft_adfs_format_name(int format);

/**
 * @brief Calculate track number from cylinder/head
 */
static inline int uft_apd_track_num(int cylinder, int head) {
    return cylinder * 2 + head;
}

/**
 * @brief Get cylinder from track number
 */
static inline int uft_apd_cylinder(int track_num) {
    return track_num / 2;
}

/**
 * @brief Get head from track number
 */
static inline int uft_apd_head(int track_num) {
    return track_num % 2;
}

/**
 * @brief Print APD info
 */
void uft_apd_print_info(uft_apd_t *apd);

/**
 * @brief Print track info
 */
void uft_apd_print_track(const uft_apd_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APD_FORMAT_H */
