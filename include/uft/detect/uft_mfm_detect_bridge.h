/**
 * uft_mfm_detect_bridge.h — Bridge between MFM Detect Module and UFT
 * ====================================================================
 *
 * Connects the standalone MFM format detection engine to UFT's
 * format detection pipeline and image loading infrastructure.
 *
 * Integration points:
 *   1. Image file detection (raw .img/.dsk → filesystem identification)
 *   2. Flux-decoded sector data → format identification
 *   3. CP/M filesystem access for recognized CP/M disks
 *   4. Boot sector analysis for FAT/Amiga/Atari format details
 */

#ifndef UFT_MFM_DETECT_BRIDGE_H
#define UFT_MFM_DETECT_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles (avoid struct tag conflicts between mfm_detect.h and cpm_fs.h) */
typedef void *mfm_detect_handle_t;
typedef void *cpm_disk_handle_t;

/* ---- Error codes ---- */
typedef enum {
    UFT_MFMD_OK = 0,
    UFT_MFMD_ERR_NULL,
    UFT_MFMD_ERR_NOMEM,
    UFT_MFMD_ERR_NO_DATA,
    UFT_MFMD_ERR_TOO_SMALL,
    UFT_MFMD_ERR_DETECT_FAIL,
    UFT_MFMD_ERR_UNSUPPORTED,
    UFT_MFMD_ERR_IO,
} uft_mfmd_error_t;

/* ---- Detection result (simplified for UFT consumers) ---- */
typedef struct {
    /* Best match */
    const char *fs_name;           /* e.g. "FAT12 (MS-DOS)", "Amiga FFS", "CP/M 2.2" */
    const char *system_name;       /* e.g. "MS-DOS", "AmigaOS", "Kaypro II" */
    uint8_t     confidence;        /* 0-100 */

    /* Physical parameters */
    uint16_t    sector_size;
    uint8_t     sectors_per_track;
    uint8_t     heads;
    uint16_t    cylinders;
    uint32_t    disk_size;
    const char *geometry_name;     /* e.g. "3.5\" DS/DD 80T (720K)" */
    const char *encoding_name;     /* e.g. "MFM (Double/High Density)" */

    /* Candidate count */
    uint8_t     num_candidates;

    /* Flags */
    bool        is_fat;
    bool        is_amiga;
    bool        is_cpm;
    bool        is_encrypted;      /* Amiga NDOS / CP/M protected */
    bool        has_boot_sector;

    /* Opaque handle for detailed queries */
    mfm_detect_handle_t detail;
} uft_mfm_detect_info_t;

/* ---- Sector read callback (for live hardware / flux decode) ---- */
typedef int (*uft_mfmd_read_fn)(void *ctx, uint16_t cyl, uint8_t head,
                                 uint8_t sector, uint8_t *buf, uint16_t *len);

/* ================================================================== */
/*  API: Image file detection                                          */
/* ================================================================== */

/**
 * Detect filesystem format from a raw disk image in memory.
 *
 * Runs all 3 detection stages:
 *   Stage 1: Geometry from image size
 *   Stage 2: Boot sector analysis (FAT BPB, Amiga, Atari ST)
 *   Stage 3: Filesystem heuristics (CP/M directory patterns)
 *
 * @param data      Raw disk image data
 * @param size      Image size in bytes
 * @param info      Output: detection result (caller must call uft_mfmd_free)
 * @return UFT_MFMD_OK on success
 */
uft_mfmd_error_t uft_mfmd_detect_image(const uint8_t *data, size_t size,
                                         uft_mfm_detect_info_t *info);

/**
 * Detect format using sector-read callback (for hardware/flux).
 *
 * @param reader    Sector read function
 * @param ctx       Context for reader
 * @param sector_size       Known sector size
 * @param sectors_per_track Known sectors per track
 * @param heads             Number of heads
 * @param cylinders         Number of cylinders
 * @param info              Output result
 */
uft_mfmd_error_t uft_mfmd_detect_live(uft_mfmd_read_fn reader, void *ctx,
                                        uint16_t sector_size,
                                        uint8_t sectors_per_track,
                                        uint8_t heads, uint16_t cylinders,
                                        uft_mfm_detect_info_t *info);

/**
 * Detect format from boot sector data only (quick mode).
 * Only runs stage 2 — no CP/M heuristics.
 */
uft_mfmd_error_t uft_mfmd_detect_boot(const uint8_t *boot_sector,
                                        uint16_t size,
                                        uint16_t sector_size,
                                        uint8_t sectors_per_track,
                                        uint8_t heads, uint16_t cylinders,
                                        uft_mfm_detect_info_t *info);

/* ================================================================== */
/*  API: Result access                                                 */
/* ================================================================== */

/**
 * Get Nth candidate (0 = best match).
 * Returns false if index >= num_candidates.
 */
bool uft_mfmd_get_candidate(const uft_mfm_detect_info_t *info, uint8_t index,
                              const char **fs_name, const char **system_name,
                              uint8_t *confidence);

/**
 * Print formatted report to FILE*.
 */
void uft_mfmd_print_report(const uft_mfm_detect_info_t *info, void *file_handle);

/**
 * Free detection result (releases internal detail handle).
 */
void uft_mfmd_free(uft_mfm_detect_info_t *info);

/* ================================================================== */
/*  API: CP/M filesystem access (when CP/M detected)                   */
/* ================================================================== */

/**
 * Open CP/M filesystem on a raw image buffer.
 * Only valid when info->is_cpm == true.
 *
 * @param data      Raw disk image (must remain valid while disk is open)
 * @param size      Image size
 * @param info      Detection result (provides DPB parameters)
 * @param disk_out  Output: CP/M disk handle (caller must call uft_mfmd_cpm_close)
 */
uft_mfmd_error_t uft_mfmd_cpm_open(const uint8_t *data, size_t size,
                                     const uft_mfm_detect_info_t *info,
                                     cpm_disk_handle_t *disk_out);

/**
 * Close CP/M filesystem handle.
 */
void uft_mfmd_cpm_close(cpm_disk_handle_t disk);

/* ================================================================== */
/*  API: Utility                                                       */
/* ================================================================== */

/**
 * Get error string.
 */
const char *uft_mfmd_error_str(uft_mfmd_error_t err);

/**
 * Get version string of the MFM detect module.
 */
const char *uft_mfmd_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_DETECT_BRIDGE_H */
