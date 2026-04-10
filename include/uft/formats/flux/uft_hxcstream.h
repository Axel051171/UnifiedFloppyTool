/**
 * @file uft_hxcstream.h
 * @brief HxC Floppy Emulator native flux stream format
 *
 * HxCStream is the raw flux capture format used by the HxC Floppy
 * Emulator hardware/software. Unlike HFE (which stores bitstream data),
 * HxCStream stores raw flux timing intervals similar to SCP.
 *
 * Reference: HxC Floppy Emulator project by Jean-Francois DEL NERO
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#ifndef UFT_HXCSTREAM_H
#define UFT_HXCSTREAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * HXCSTREAM FORMAT DEFINITIONS
 *===========================================================================*/

#define UFT_HXCSTREAM_MAGIC         "HXCSTREAM"
#define UFT_HXCSTREAM_MAGIC_LEN     9

/* Default sample rate: 72 MHz (same as Greaseweazle) */
#define UFT_HXCSTREAM_DEFAULT_RATE  72000000

/* Maximum values */
#define UFT_HXCSTREAM_MAX_TRACKS    168     /* 84 cyl * 2 sides */
#define UFT_HXCSTREAM_MAX_REVS      10
#define UFT_HXCSTREAM_MAX_FLUX      2000000 /* Max flux transitions per track */

/* Track flags */
#define UFT_HXCSTREAM_TF_INDEX      0x01    /* Has index marks */
#define UFT_HXCSTREAM_TF_MULTIREV   0x02    /* Multi-revolution data */

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief HxCStream file header
 */
typedef struct {
    char     magic[9];              /* "HXCSTREAM" */
    uint8_t  version;               /* Format version */
    uint8_t  num_tracks;            /* Number of tracks */
    uint8_t  num_sides;             /* Number of sides (1 or 2) */
    uint32_t sample_rate;           /* Sample rate in Hz */
    uint8_t  flags;                 /* Global flags */
    uint8_t  reserved[14];          /* Reserved for future use */
} uft_hxcstream_file_header_t;

/**
 * @brief HxCStream track entry (in track offset table)
 */
typedef struct {
    uint32_t offset;                /* Offset to track data from file start */
    uint32_t length;                /* Length of track data in bytes */
    uint8_t  revolutions;           /* Number of revolutions captured */
    uint8_t  flags;                 /* Track flags */
    uint16_t reserved;
} uft_hxcstream_track_entry_t;

/**
 * @brief HxCStream track data header
 */
typedef struct {
    uint8_t  cylinder;              /* Physical cylinder */
    uint8_t  head;                  /* Physical head */
    uint32_t flux_count;            /* Number of flux intervals */
    uint32_t index_offset;          /* Offset to first index pulse */
    uint8_t  encoding_hint;         /* 0=unknown, 1=FM, 2=MFM, 3=GCR */
    uint8_t  reserved[3];
} uft_hxcstream_track_header_t;

#pragma pack(pop)

/*===========================================================================
 * RUNTIME STRUCTURES
 *===========================================================================*/

/**
 * @brief Parsed flux data for one track
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;

    uint16_t *flux_intervals;       /* Raw flux intervals (sample ticks) */
    size_t    flux_count;

    uint32_t *index_positions;      /* Flux index of each revolution start */
    size_t    index_count;

    uint8_t   encoding_hint;
} uft_hxcstream_track_t;

/**
 * @brief HxCStream image context
 */
typedef struct {
    uft_hxcstream_file_header_t header;

    uft_hxcstream_track_t *tracks;
    size_t                  track_count;

    uint32_t sample_rate;
    uint8_t  cylinders;
    uint8_t  heads;

    bool     is_open;
} uft_hxcstream_image_t;

/**
 * @brief HxCStream read result
 */
typedef struct {
    bool        success;
    int         error_code;
    const char *error_detail;

    uint8_t     cylinders;
    uint8_t     heads;
    uint32_t    sample_rate;
    size_t      track_count;
    size_t      total_flux_count;
} uft_hxcstream_read_result_t;

/*===========================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Initialize HxCStream image structure
 */
void uft_hxcstream_image_init(uft_hxcstream_image_t *image);

/**
 * @brief Free HxCStream image resources
 */
void uft_hxcstream_image_free(uft_hxcstream_image_t *image);

/**
 * @brief Probe if data is HxCStream format
 */
bool uft_hxcstream_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read HxCStream file
 * @param path    Path to .hxcstream file
 * @param image   Output image structure
 * @param result  Optional read result details
 * @return 0 on success, negative on error
 */
int uft_hxcstream_read(const char *path, uft_hxcstream_image_t *image,
                       uft_hxcstream_read_result_t *result);

/**
 * @brief Get flux data for a specific track
 * @param image   Parsed HxCStream image
 * @param cyl     Cylinder number
 * @param head    Head number
 * @return Track data pointer, or NULL if not found
 */
const uft_hxcstream_track_t *uft_hxcstream_get_track(
    const uft_hxcstream_image_t *image, uint8_t cyl, uint8_t head);

/**
 * @brief Convert flux intervals to nanoseconds
 * @param image   Image (for sample rate)
 * @param track   Track data
 * @param ns_out  Output array for nanosecond intervals
 * @param max_count  Maximum entries in ns_out
 * @return Number of intervals converted
 */
size_t uft_hxcstream_flux_to_ns(const uft_hxcstream_image_t *image,
                                const uft_hxcstream_track_t *track,
                                uint32_t *ns_out, size_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HXCSTREAM_H */
