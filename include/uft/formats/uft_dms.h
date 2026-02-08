#ifndef UFT_DMS_H
#define UFT_DMS_H

/*
 * uft_dms.h — DMS (Disk Masher System) decompression library for UFT
 * ===================================================================
 * Reentrant, memory-buffer based DMS → ADF decoder.
 * Based on xDMS 1.3 by Andre Rodrigues de la Rocha (Public Domain).
 * Refactored: all globals → context struct, FILE* → memory buffers.
 *
 * Supported compression modes: NOCOMP, SIMPLE (RLE), QUICK, MEDIUM,
 *                              DEEP, HEAVY1, HEAVY2
 * Supports: encrypted archives, banners, FILEID.DIZ
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Error codes ---- */
typedef enum {
    DMS_OK              = 0,
    DMS_FILE_END        = 1,
    DMS_ERR_NOMEMORY    = 2,
    DMS_ERR_NOT_DMS     = 5,
    DMS_ERR_SHORT_READ  = 6,
    DMS_ERR_HEADER_CRC  = 7,
    DMS_ERR_NOT_TRACK   = 8,
    DMS_ERR_BIG_TRACK   = 9,
    DMS_ERR_TRACK_HCRC  = 10,
    DMS_ERR_TRACK_DCRC  = 11,
    DMS_ERR_CHECKSUM    = 12,
    DMS_ERR_BAD_DECOMP  = 14,
    DMS_ERR_UNKN_MODE   = 15,
    DMS_ERR_NO_PASSWD   = 16,
    DMS_ERR_BAD_PASSWD  = 17,
    DMS_ERR_FMS         = 18,
    DMS_ERR_OUTPUT_FULL = 30
} dms_error_t;

/* ---- Compression modes ---- */
typedef enum {
    DMS_MODE_NOCOMP  = 0,
    DMS_MODE_SIMPLE  = 1,   /* RLE only */
    DMS_MODE_QUICK   = 2,
    DMS_MODE_MEDIUM  = 3,
    DMS_MODE_DEEP    = 4,
    DMS_MODE_HEAVY1  = 5,
    DMS_MODE_HEAVY2  = 6
} dms_comp_mode_t;

/* ---- Amiga disk types ---- */
typedef enum {
    DMS_DISK_OFS          = 0,
    DMS_DISK_OFS_ALT      = 1,
    DMS_DISK_FFS          = 2,
    DMS_DISK_OFS_INTL     = 3,
    DMS_DISK_FFS_INTL     = 4,
    DMS_DISK_OFS_DIRCACHE = 5,
    DMS_DISK_FFS_DIRCACHE = 6,
    DMS_DISK_FMS          = 7
} dms_disk_type_t;

/* ---- General info flags ---- */
#define DMS_INFO_NOZERO     0x0001
#define DMS_INFO_ENCRYPTED  0x0002
#define DMS_INFO_APPENDS    0x0004
#define DMS_INFO_BANNER     0x0008
#define DMS_INFO_HD         0x0010
#define DMS_INFO_MSDOS      0x0020
#define DMS_INFO_DEV_FIXED  0x0040
#define DMS_INFO_REGISTERED 0x0080
#define DMS_INFO_FILEID_DIZ 0x0100

/* ---- DMS file header info ---- */
typedef struct dms_info {
    uint16_t  creator_version;  /* DMS version that created the file */
    uint16_t  geninfo;          /* general info flags (DMS_INFO_*) */
    uint32_t  creation_date;    /* UNIX timestamp */
    uint16_t  track_lo;         /* lowest track */
    uint16_t  track_hi;         /* highest track */
    uint32_t  packed_size;      /* total packed data length */
    uint32_t  unpacked_size;    /* unpacked data length (usually 901120) */
    uint16_t  disk_type;        /* dms_disk_type_t */
    uint16_t  comp_mode;        /* primary compression mode used */

    /* Extracted during decompression */
    uint8_t  *banner;           /* NULL if none (allocated, caller frees) */
    size_t    banner_len;
    uint8_t  *fileid_diz;       /* NULL if none (allocated, caller frees) */
    size_t    fileid_diz_len;
} dms_info_t;

/* ---- Per-track info (for detailed inspection) ---- */
typedef struct dms_track_info {
    uint16_t  number;       /* track number (0-79 for DD, 0-159 for HD) */
    uint16_t  packed_len;   /* packed data length */
    uint16_t  unpacked_len; /* unpacked data length */
    uint8_t   comp_mode;    /* compression mode for this track */
    uint8_t   flags;        /* track flags */
    uint16_t  checksum;     /* data checksum after unpacking */
    uint16_t  header_crc;   /* track header CRC */
    uint16_t  data_crc;     /* packed data CRC */
    int       crc_ok;       /* 1 if CRC verified OK */
    int       checksum_ok;  /* 1 if checksum verified OK */
} dms_track_info_t;

/* ---- Opaque context ---- */
typedef struct dms_ctx dms_ctx_t;

/* ---- Track callback (optional, for progress/inspection) ---- */
typedef void (*dms_track_cb_t)(const dms_track_info_t *info, void *user);

/* ---- Public API ---- */

/*
 * Quick check: is this a DMS file?
 * Returns 1 if data starts with "DMS!" magic and header CRC is valid.
 */
int dms_is_dms(const uint8_t *data, size_t len);

/*
 * Parse DMS header info without decompressing.
 * Reads the 56-byte file header and fills info struct.
 * Returns DMS_OK on success.
 */
dms_error_t dms_read_info(const uint8_t *data, size_t len, dms_info_t *info);

/*
 * Decompress DMS → ADF.
 *
 * Parameters:
 *   dms_data / dms_len: input DMS file in memory
 *   adf_out:            output buffer for ADF data (caller allocates)
 *   adf_cap:            capacity of adf_out buffer
 *   adf_written:        actual bytes written (may be NULL)
 *   password:           decryption password (NULL if not encrypted)
 *   override_errors:    if non-zero, continue past CRC/checksum errors
 *   info:               if non-NULL, filled with file header info
 *   track_cb:           per-track callback (may be NULL)
 *   track_cb_user:      user pointer for callback
 *
 * Typical ADF sizes:
 *   DD (880 KB) = 901120 bytes (80 tracks × 2 sides × 11 sectors × 512)
 *   HD (1.76 MB) = 1802240 bytes
 *
 * Returns DMS_OK on success.
 */
dms_error_t dms_unpack(const uint8_t *dms_data, size_t dms_len,
                       uint8_t *adf_out, size_t adf_cap,
                       size_t *adf_written,
                       const char *password,
                       int override_errors,
                       dms_info_t *info,
                       dms_track_cb_t track_cb,
                       void *track_cb_user);

/*
 * Free dynamically allocated fields in dms_info_t
 * (banner, fileid_diz).
 */
void dms_info_free(dms_info_t *info);

/*
 * Get human-readable names for disk type and compression mode.
 */
const char *dms_disk_type_name(uint16_t disk_type);
const char *dms_comp_mode_name(uint16_t comp_mode);
const char *dms_error_string(dms_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DMS_H */
