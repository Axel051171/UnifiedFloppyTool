/**
 * @file uft_scp.h
 * @brief SCP (SuperCard Pro) flux format support
 * @version 3.9.0
 * 
 * SCP is a raw flux capture format created by Jim Drew for the
 * SuperCard Pro hardware. It stores flux transition timing data
 * with multiple revolutions per track.
 * 
 * Features:
 * - Multiple revolutions per track (typically 5)
 * - Index-to-index timing
 * - 25ns resolution (40 MHz sample rate)
 * - Optional checksum
 * - Wide format support (Amiga, C64, Apple II, PC, etc.)
 * 
 * Reference: SCP File Format Documentation v2.4
 */

#ifndef UFT_SCP_H
#define UFT_SCP_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SCP Magic Numbers */
#define SCP_MAGIC               "SCP"
#define SCP_MAGIC_LEN           3
#define SCP_HEADER_SIZE         16
#define SCP_TRACK_HEADER_SIZE   4

/* SCP Version */
#define SCP_VERSION_MAJOR       2
#define SCP_VERSION_MINOR       4

/* SCP Flags (byte offset 0x08) */
#define SCP_FLAG_INDEX          0x01    /* Flux data starts at index */
#define SCP_FLAG_TPI96          0x02    /* 96 TPI drive (5.25" HD) */
#define SCP_FLAG_RPM360         0x04    /* 360 RPM drive */
#define SCP_FLAG_NORMALIZED     0x08    /* Flux data normalized */
#define SCP_FLAG_READWRITE      0x10    /* Read/write image */
#define SCP_FLAG_FOOTER         0x20    /* Has footer with extension info */

/* SCP Disk Types */
#define SCP_DISK_C64            0x00
#define SCP_DISK_AMIGA          0x04
#define SCP_DISK_ATARI_FM       0x10
#define SCP_DISK_ATARI_MFM      0x14
#define SCP_DISK_APPLE_II       0x20
#define SCP_DISK_APPLE_II_PRO   0x24
#define SCP_DISK_APPLE_400K     0x30
#define SCP_DISK_APPLE_800K     0x34
#define SCP_DISK_APPLE_HD       0x38
#define SCP_DISK_IBM_PC_360K    0x40
#define SCP_DISK_IBM_PC_720K    0x44
#define SCP_DISK_IBM_PC_1200K   0x48
#define SCP_DISK_IBM_PC_1440K   0x4C
#define SCP_DISK_TRS80_SSSD     0x50
#define SCP_DISK_TRS80_SSDD     0x54
#define SCP_DISK_TRS80_DSSD     0x58
#define SCP_DISK_TRS80_DSDD     0x5C
#define SCP_DISK_TI994A         0x60
#define SCP_DISK_ROLAND         0x70
#define SCP_DISK_AMSTRAD_CPC    0x80
#define SCP_DISK_OTHER          0xE0
#define SCP_DISK_TAPEDRIVE      0xE4

/* SCP Sample Rate */
#define SCP_SAMPLE_RATE         40000000    /* 40 MHz = 25ns resolution */
#define SCP_TICK_NS             25          /* nanoseconds per tick */

/* Maximum values */
#define SCP_MAX_TRACKS          168         /* 84 cylinders * 2 sides */
#define SCP_MAX_REVOLUTIONS     16

/**
 * @brief SCP file header (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char     magic[3];          /* "SCP" */
    uint8_t  version;           /* Version (major << 4 | minor) */
    uint8_t  disk_type;         /* Disk type (SCP_DISK_*) */
    uint8_t  revolutions;       /* Number of revolutions */
    uint8_t  start_track;       /* First track */
    uint8_t  end_track;         /* Last track */
    uint8_t  flags;             /* SCP_FLAG_* */
    uint8_t  bit_cell_width;    /* 0 = variable, 16 = 16-bit */
    uint8_t  heads;             /* 0 = both sides, 1 = side 0, 2 = side 1 */
    uint8_t  resolution;        /* 25ns * (resolution + 1) */
    uint32_t checksum;          /* Optional checksum */
} scp_file_header_t;

/**
 * @brief SCP track header (4 bytes + revolution data)
 */
typedef struct {
    char     magic[3];          /* "TRK" */
    uint8_t  track_num;         /* Track number */
} scp_track_header_t;

/**
 * @brief SCP revolution entry (12 bytes per revolution)
 */
typedef struct {
    uint32_t index_time;        /* Index time (in SCP ticks) */
    uint32_t track_length;      /* Track data length (bytes) */
    uint32_t data_offset;       /* Offset to flux data from track header */
} scp_revolution_t;
#pragma pack(pop)

/**
 * @brief SCP track data structure
 */
typedef struct {
    uint8_t  track_num;
    uint8_t  revolutions;
    
    /* Per-revolution data */
    struct {
        uint32_t index_time;
        uint16_t *flux_data;    /* 16-bit flux transition times */
        size_t   flux_count;
    } rev[SCP_MAX_REVOLUTIONS];
    
} scp_track_data_t;

/**
 * @brief SCP image structure
 */
typedef struct {
    scp_file_header_t header;
    
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  track_count;
    
    scp_track_data_t *tracks;
    size_t   num_tracks;
    
    /* Derived info */
    uint8_t  cylinders;
    uint8_t  heads;
    
} scp_image_t;

/**
 * @brief SCP read options
 */
typedef struct {
    bool     decode_flux;       /* Decode flux to sectors */
    uint8_t  revolution;        /* Which revolution to use (0 = best) */
    uint8_t  disk_type;         /* Override disk type (0 = auto) */
} scp_read_options_t;

/**
 * @brief SCP write options
 */
typedef struct {
    uint8_t  disk_type;         /* SCP_DISK_* type */
    uint8_t  revolutions;       /* Revolutions to write (1-16) */
    uint8_t  flags;             /* SCP_FLAG_* */
    bool     add_checksum;      /* Include checksum */
} scp_write_options_t;

/**
 * @brief SCP read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  disk_type;
    uint8_t  revolutions;
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  flags;
    
    uint8_t  cylinders;
    uint8_t  heads;
    
    size_t   image_size;
    size_t   total_flux_count;
    
} scp_read_result_t;

/* ============================================================================
 * SCP Functions
 * ============================================================================ */

/**
 * @brief Initialize SCP image
 */
void uft_scp_image_init(scp_image_t *image);

/**
 * @brief Free SCP image resources
 */
void uft_scp_image_free(scp_image_t *image);

/**
 * @brief Initialize read options
 */
void uft_scp_read_options_init(scp_read_options_t *opts);

/**
 * @brief Initialize write options
 */
void uft_scp_write_options_init(scp_write_options_t *opts);

/**
 * @brief Read SCP file
 */
uft_error_t uft_scp_read(const char *path,
                         scp_image_t *image,
                         const scp_read_options_t *opts,
                         scp_read_result_t *result);

/**
 * @brief Read SCP from memory
 */
uft_error_t uft_scp_read_mem(const uint8_t *data, size_t size,
                             scp_image_t *image,
                             const scp_read_options_t *opts,
                             scp_read_result_t *result);

/**
 * @brief Write SCP file
 */
uft_error_t uft_scp_write(const scp_image_t *image,
                          const char *path,
                          const scp_write_options_t *opts);

/**
 * @brief Convert SCP to sector-based disk image
 */
uft_error_t uft_scp_to_disk(const scp_image_t *scp,
                            uft_disk_image_t **out_disk,
                            const scp_read_options_t *opts);

/**
 * @brief Probe if data is SCP format
 */
bool uft_scp_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Get disk type name
 */
const char* uft_scp_disk_type_name(uint8_t disk_type);

/**
 * @brief Get track from SCP image
 */
scp_track_data_t* uft_scp_get_track(scp_image_t *image, uint8_t track_num);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_H */
