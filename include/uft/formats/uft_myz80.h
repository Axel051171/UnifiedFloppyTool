/**
 * @file uft_myz80.h
 * @brief MYZ80 Hard Drive Image format support
 * @version 3.9.0
 * 
 * MYZ80 is a CP/M emulator by Simeon Cran. The hard drive image
 * format is essentially raw disk data with a 256-byte header
 * containing geometry and identification information.
 * 
 * Features:
 * - 256-byte header with geometry
 * - Raw sector data follows header
 * - Used for CP/M 2.2 and CP/M 3 emulation
 * 
 * Reference: libdsk drvmyz80.c
 */

#ifndef UFT_MYZ80_H
#define UFT_MYZ80_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MYZ80 Constants */
#define MYZ80_HEADER_SIZE       256
#define MYZ80_MAGIC             "MYZ80 "
#define MYZ80_MAGIC_LEN         6

/* Default geometry (CP/M standard) */
#define MYZ80_DEFAULT_CYLINDERS 77
#define MYZ80_DEFAULT_HEADS     2
#define MYZ80_DEFAULT_SECTORS   26
#define MYZ80_DEFAULT_SECSIZE   128

/**
 * @brief MYZ80 header structure (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char     magic[6];          /* "MYZ80 " */
    uint8_t  version;           /* Format version */
    uint8_t  flags;             /* Flags */
    uint16_t cylinders;         /* Number of cylinders */
    uint8_t  heads;             /* Number of heads */
    uint8_t  sectors;           /* Sectors per track */
    uint16_t sector_size;       /* Bytes per sector */
    uint8_t  first_sector;      /* First sector number (usually 1) */
    uint8_t  reserved1;
    char     label[32];         /* Volume label */
    char     comment[64];       /* Comment */
    uint8_t  reserved[142];     /* Padding to 256 bytes */
} myz80_header_t;
#pragma pack(pop)

/**
 * @brief MYZ80 read options
 */
typedef struct {
    bool     ignore_header;     /* Treat as raw if header invalid */
} myz80_read_options_t;

/**
 * @brief MYZ80 write options
 */
typedef struct {
    char     label[32];         /* Volume label */
    char     comment[64];       /* Comment */
} myz80_write_options_t;

/**
 * @brief MYZ80 read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    
    char     label[32];
    char     comment[64];
    
    size_t   image_size;
    bool     has_valid_header;
    
} myz80_read_result_t;

/* ============================================================================
 * MYZ80 Functions
 * ============================================================================ */

/**
 * @brief Initialize read options
 */
void uft_myz80_read_options_init(myz80_read_options_t *opts);

/**
 * @brief Initialize write options
 */
void uft_myz80_write_options_init(myz80_write_options_t *opts);

/**
 * @brief Read MYZ80 file
 */
uft_error_t uft_myz80_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const myz80_read_options_t *opts,
                           myz80_read_result_t *result);

/**
 * @brief Read MYZ80 from memory
 */
uft_error_t uft_myz80_read_mem(const uint8_t *data, size_t size,
                               uft_disk_image_t **out_disk,
                               const myz80_read_options_t *opts,
                               myz80_read_result_t *result);

/**
 * @brief Write MYZ80 file
 */
uft_error_t uft_myz80_write(const uft_disk_image_t *disk,
                            const char *path,
                            const myz80_write_options_t *opts);

/**
 * @brief Probe if data is MYZ80 format
 */
bool uft_myz80_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Validate MYZ80 header
 */
bool uft_myz80_validate_header(const myz80_header_t *header);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MYZ80_H */
