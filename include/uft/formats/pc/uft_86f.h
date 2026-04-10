/**
 * @file uft_86f.h
 * @brief 86Box 86F Surface Format parser (pc/ variant)
 *
 * 86F is the native disk surface format used by the 86Box PC emulator.
 * It stores bitcell-level surface data with per-section variable timing,
 * supporting full copy protection preservation.
 *
 * This is the standalone parser (distinct from include/uft/formats/uft_86f.h
 * which defines the full API, and src/formats/flux/f86.c which is the
 * FloppyDevice adapter). This parser handles direct file I/O with MFM decode.
 *
 * Reference: 86Box source code (src/floppy/fdd_86f.c)
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#ifndef UFT_PC_86F_H
#define UFT_PC_86F_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 86F FORMAT DEFINITIONS
 *===========================================================================*/

#define UFT_PC86F_MAGIC         "86BF"
#define UFT_PC86F_MAGIC_LEN     4

/* Disk types */
#define UFT_PC86F_TYPE_MFM_DD   0   /* MFM Double Density */
#define UFT_PC86F_TYPE_MFM_HD   1   /* MFM High Density */
#define UFT_PC86F_TYPE_MFM_ED   2   /* MFM Extra Density */

/* Header flags */
#define UFT_PC86F_FL_WRITABLE       0x0001
#define UFT_PC86F_FL_HAS_SURFACE    0x0002
#define UFT_PC86F_FL_HOLE           0x0004  /* 360 RPM hole */
#define UFT_PC86F_FL_EXTRA_BITCELL  0x0008
#define UFT_PC86F_FL_REVERSE_ENDIAN 0x0010

/* Encoding types */
#define UFT_PC86F_ENC_FM        0
#define UFT_PC86F_ENC_MFM       1
#define UFT_PC86F_ENC_M2FM      2
#define UFT_PC86F_ENC_GCR       3

/* Data rates */
#define UFT_PC86F_RATE_500K     0
#define UFT_PC86F_RATE_300K     1
#define UFT_PC86F_RATE_250K     2
#define UFT_PC86F_RATE_1M       3
#define UFT_PC86F_RATE_2M       4

/* Limits */
#define UFT_PC86F_MAX_TRACKS    256
#define UFT_PC86F_MAX_BITCELLS  200000  /* Max bitcells per track */

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief 86F file header (16 bytes + track offset table)
 */
typedef struct {
    char     magic[4];              /* "86BF" */
    uint16_t version;               /* Format version */
    uint16_t flags;                 /* File flags */
    uint8_t  disk_type;             /* 0=DD, 1=HD, 2=ED */
    uint8_t  encoding;              /* Default encoding */
    uint8_t  rpm;                   /* 0=300, 1=360 */
    uint8_t  num_tracks;            /* Tracks per side */
    uint8_t  num_sides;             /* Number of sides */
    uint8_t  bitcell_mode;          /* Bitcell storage mode */
    uint16_t reserved;
} uft_pc86f_header_t;

/**
 * @brief 86F track header
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  encoding;
    uint8_t  data_rate;
    uint32_t bit_count;             /* Number of bitcells */
    uint32_t index_offset;          /* Index hole position in bits */
    uint16_t num_sectors;           /* Decoded sector count */
    uint16_t flags;
} uft_pc86f_track_header_t;

#pragma pack(pop)

/*===========================================================================
 * RUNTIME STRUCTURES
 *===========================================================================*/

/**
 * @brief Parsed track data
 */
typedef struct {
    uft_pc86f_track_header_t header;
    uint8_t *bitcell_data;          /* Raw bitcell data */
    size_t   bitcell_bytes;         /* Byte count of bitcell data */
    uint8_t *surface_data;          /* Surface/weak bit data (optional) */
    size_t   surface_bytes;
} uft_pc86f_track_t;

/**
 * @brief Decoded sector from bitcell data
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;             /* N: sector size = 128 << N */
    uint8_t *data;
    size_t   data_size;
    bool     crc_ok;
    bool     deleted;               /* Deleted Data Address Mark */
} uft_pc86f_decoded_sector_t;

/**
 * @brief 86F image context
 */
typedef struct {
    uft_pc86f_header_t header;
    uint32_t track_offsets[UFT_PC86F_MAX_TRACKS];

    uft_pc86f_track_t *tracks;
    size_t              track_count;

    uint8_t  cylinders;
    uint8_t  heads;

    /* Decoded sectors (populated on demand) */
    uft_pc86f_decoded_sector_t *decoded_sectors;
    size_t                      decoded_count;

    bool     is_open;
} uft_pc86f_image_t;

/**
 * @brief 86F read result
 */
typedef struct {
    bool        success;
    int         error_code;
    const char *error_detail;

    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     disk_type;
    uint8_t     encoding;
    uint16_t    version;
    size_t      track_count;
    bool        has_surface_data;
} uft_pc86f_read_result_t;

/*===========================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Initialize 86F image structure
 */
void uft_pc86f_image_init(uft_pc86f_image_t *image);

/**
 * @brief Free 86F image resources
 */
void uft_pc86f_image_free(uft_pc86f_image_t *image);

/**
 * @brief Probe if data is 86F format
 */
bool uft_pc86f_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read 86F file
 * @param path    Path to .86f file
 * @param image   Output image structure
 * @param result  Optional read result details
 * @return 0 on success, negative on error
 */
int uft_pc86f_read(const char *path, uft_pc86f_image_t *image,
                   uft_pc86f_read_result_t *result);

/**
 * @brief Get track bitcell data
 */
const uft_pc86f_track_t *uft_pc86f_get_track(const uft_pc86f_image_t *image,
                                              uint8_t cyl, uint8_t head);

/**
 * @brief Decode MFM bitcells to sectors for a track
 * @param image   Parsed 86F image
 * @param cyl     Cylinder
 * @param head    Head
 * @param sectors Output sector array
 * @param max_sectors  Maximum sectors to decode
 * @return Number of sectors decoded, or negative on error
 */
int uft_pc86f_decode_track(const uft_pc86f_image_t *image,
                           uint8_t cyl, uint8_t head,
                           uft_pc86f_decoded_sector_t *sectors,
                           int max_sectors);

/**
 * @brief Get disk type description
 */
const char *uft_pc86f_disk_type_name(uint8_t disk_type);

/**
 * @brief Convert 86F to raw IMG
 */
int uft_pc86f_to_img(const char *f86_path, const char *img_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PC_86F_H */
