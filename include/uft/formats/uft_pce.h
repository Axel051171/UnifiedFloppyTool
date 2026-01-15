/**
 * @file uft_pce.h
 * @brief PCE Disk Image Formats (PSI and PRI)
 * 
 * PSI = PCE Sector Image (decoded sector data)
 * PRI = PCE Raw Image (raw bitstream/flux data)
 * 
 * Used by PCE (PC Emulator) and related tools.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_PCE_H
#define UFT_PCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PSI FORMAT (Sector Image)
 *===========================================================================*/

#define UFT_PSI_MAGIC       0x50534900  /* "PSI\0" */
#define UFT_PSI_VERSION     0

/**
 * @brief PSI chunk types
 */
typedef enum {
    UFT_PSI_CHUNK_END       = 0x0000,   /**< End of file */
    UFT_PSI_CHUNK_TEXT      = 0x0001,   /**< Text comment */
    UFT_PSI_CHUNK_DISK      = 0x0100,   /**< Disk info */
    UFT_PSI_CHUNK_TRACK     = 0x0200,   /**< Track header */
    UFT_PSI_CHUNK_SECTOR    = 0x0210,   /**< Sector data */
    UFT_PSI_CHUNK_DATA      = 0x0220,   /**< Raw data */
    UFT_PSI_CHUNK_WEAK      = 0x0230,   /**< Weak bits mask */
    UFT_PSI_CHUNK_OFFSET    = 0x0240,   /**< Sector offset */
    UFT_PSI_CHUNK_TIME      = 0x0250,   /**< Timing data */
    UFT_PSI_CHUNK_IBMFM     = 0x0300,   /**< IBM FM params */
    UFT_PSI_CHUNK_IBMMFM    = 0x0301,   /**< IBM MFM params */
    UFT_PSI_CHUNK_MACGCR    = 0x0302,   /**< Mac GCR params */
} uft_psi_chunk_type_t;

/**
 * @brief PSI sector flags
 */
typedef enum {
    UFT_PSI_FLAG_CRC_ID     = 0x0001,   /**< ID field CRC error */
    UFT_PSI_FLAG_CRC_DATA   = 0x0002,   /**< Data field CRC error */
    UFT_PSI_FLAG_DEL_DAM    = 0x0004,   /**< Deleted data mark */
    UFT_PSI_FLAG_NO_DAM     = 0x0008,   /**< Missing data mark */
    UFT_PSI_FLAG_ALTERNATE  = 0x0010,   /**< Alternate data */
    UFT_PSI_FLAG_COMPRESSED = 0x0020,   /**< Compressed data */
} uft_psi_sector_flags_t;

#pragma pack(push, 1)

/**
 * @brief PSI file header
 */
typedef struct {
    uint32_t magic;             /* 0x50534900 "PSI\0" */
    uint16_t version;           /* Version (0) */
    uint16_t flags;             /* File flags */
} uft_psi_header_t;

/**
 * @brief PSI chunk header
 */
typedef struct {
    uint16_t type;              /* Chunk type */
    uint32_t size;              /* Chunk data size */
} uft_psi_chunk_t;

/**
 * @brief PSI track header data
 */
typedef struct {
    uint16_t cylinder;          /* Cylinder number */
    uint16_t head;              /* Head number */
} uft_psi_track_data_t;

/**
 * @brief PSI sector header data
 */
typedef struct {
    uint16_t cylinder;          /* ID cylinder */
    uint16_t head;              /* ID head */
    uint16_t sector;            /* ID sector */
    uint16_t size;              /* Sector size (128 << n) */
    uint16_t flags;             /* Sector flags */
    uint32_t data_size;         /* Actual data size */
} uft_psi_sector_data_t;

#pragma pack(pop)

/*===========================================================================
 * PRI FORMAT (Raw/Bitstream Image)
 *===========================================================================*/

#define UFT_PRI_MAGIC       0x50524900  /* "PRI\0" */
#define UFT_PRI_VERSION     0

/**
 * @brief PRI chunk types
 */
typedef enum {
    UFT_PRI_CHUNK_END       = 0x0000,   /**< End of file */
    UFT_PRI_CHUNK_TEXT      = 0x0001,   /**< Text comment */
    UFT_PRI_CHUNK_TRACK     = 0x0100,   /**< Track header */
    UFT_PRI_CHUNK_DATA      = 0x0110,   /**< Bitstream data */
    UFT_PRI_CHUNK_WEAK      = 0x0120,   /**< Weak bits */
    UFT_PRI_CHUNK_CLOCK     = 0x0130,   /**< Clock rate */
} uft_pri_chunk_type_t;

#pragma pack(push, 1)

/**
 * @brief PRI file header
 */
typedef struct {
    uint32_t magic;             /* 0x50524900 "PRI\0" */
    uint16_t version;           /* Version (0) */
    uint16_t flags;             /* File flags */
} uft_pri_header_t;

/**
 * @brief PRI track header data
 */
typedef struct {
    uint16_t cylinder;          /* Cylinder number */
    uint16_t head;              /* Head number */
    uint32_t bit_count;         /* Number of bits */
    uint32_t clock;             /* Clock rate (bits/sec) */
} uft_pri_track_data_t;

#pragma pack(pop)

/*===========================================================================
 * PSI CONTEXT
 *===========================================================================*/

typedef struct uft_psi_context uft_psi_t;

/**
 * @brief PSI sector info
 */
typedef struct {
    int cylinder;
    int head;
    int sector;
    int size_code;              /* 0=128, 1=256, 2=512, 3=1024 */
    int flags;
    uint8_t *data;
    size_t data_size;
    uint8_t *weak_mask;         /* Weak bit mask (optional) */
} uft_psi_sector_info_t;

/*===========================================================================
 * PSI LIFECYCLE
 *===========================================================================*/

/**
 * @brief Probe file for PSI format
 */
bool uft_psi_probe(const char *path);

/**
 * @brief Open PSI file
 */
uft_psi_t* uft_psi_open(const char *path);

/**
 * @brief Create new PSI file
 */
uft_psi_t* uft_psi_create(const char *path);

/**
 * @brief Close PSI file
 */
void uft_psi_close(uft_psi_t *psi);

/*===========================================================================
 * PSI OPERATIONS
 *===========================================================================*/

/**
 * @brief Get number of cylinders
 */
int uft_psi_get_cylinders(uft_psi_t *psi);

/**
 * @brief Get number of heads
 */
int uft_psi_get_heads(uft_psi_t *psi);

/**
 * @brief Get sectors per track
 */
int uft_psi_get_sectors(uft_psi_t *psi, int cyl, int head);

/**
 * @brief Read sector
 */
int uft_psi_read_sector(uft_psi_t *psi, int cyl, int head, int sector,
                        uint8_t *buffer, size_t size);

/**
 * @brief Write sector
 */
int uft_psi_write_sector(uft_psi_t *psi, int cyl, int head, int sector,
                         const uint8_t *buffer, size_t size);

/**
 * @brief Get sector info
 */
int uft_psi_get_sector_info(uft_psi_t *psi, int cyl, int head, int sector,
                            uft_psi_sector_info_t *info);

/**
 * @brief Add comment
 */
int uft_psi_add_comment(uft_psi_t *psi, const char *text);

/*===========================================================================
 * PRI CONTEXT
 *===========================================================================*/

typedef struct uft_pri_context uft_pri_t;

/*===========================================================================
 * PRI LIFECYCLE
 *===========================================================================*/

/**
 * @brief Probe file for PRI format
 */
bool uft_pri_probe(const char *path);

/**
 * @brief Open PRI file
 */
uft_pri_t* uft_pri_open(const char *path);

/**
 * @brief Create new PRI file
 */
uft_pri_t* uft_pri_create(const char *path);

/**
 * @brief Close PRI file
 */
void uft_pri_close(uft_pri_t *pri);

/*===========================================================================
 * PRI OPERATIONS
 *===========================================================================*/

/**
 * @brief Get number of cylinders
 */
int uft_pri_get_cylinders(uft_pri_t *pri);

/**
 * @brief Get number of heads
 */
int uft_pri_get_heads(uft_pri_t *pri);

/**
 * @brief Read track bitstream
 */
int uft_pri_read_track(uft_pri_t *pri, int cyl, int head,
                       uint8_t *bits, size_t max_bytes,
                       uint32_t *bit_count, uint32_t *clock);

/**
 * @brief Write track bitstream
 */
int uft_pri_write_track(uft_pri_t *pri, int cyl, int head,
                        const uint8_t *bits, uint32_t bit_count,
                        uint32_t clock);

/**
 * @brief Get weak bit mask for track
 */
int uft_pri_get_weak_bits(uft_pri_t *pri, int cyl, int head,
                          uint8_t *mask, size_t max_bytes);

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

/**
 * @brief Convert PSI to IMG
 */
int uft_psi_to_img(const char *psi_path, const char *img_path);

/**
 * @brief Convert IMG to PSI
 */
int uft_img_to_psi(const char *img_path, const char *psi_path);

/**
 * @brief Convert PRI to raw flux
 */
int uft_pri_to_flux(const char *pri_path, const char *flux_path);

/**
 * @brief Convert PSI to PRI (decode to bitstream)
 */
int uft_psi_to_pri(const char *psi_path, const char *pri_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PCE_H */
