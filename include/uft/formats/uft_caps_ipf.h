/**
 * @file uft_caps_ipf.h
 * @brief CAPS/IPF Disk Image Format Support
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#ifndef UFT_CAPS_IPF_H
#define UFT_CAPS_IPF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Error codes */
#define UFT_CAPS_OK             0
#define UFT_CAPS_ERROR          -1
#define UFT_CAPS_OUT_OF_RANGE   -2
#define UFT_CAPS_READ_ONLY      -3
#define UFT_CAPS_OPEN_ERROR     -4
#define UFT_CAPS_TYPE_ERROR     -5
#define UFT_CAPS_UNSUPPORTED    -6

/* Image types */
#define UFT_CAPS_TYPE_UNKNOWN   0
#define UFT_CAPS_TYPE_IPF       1
#define UFT_CAPS_TYPE_CTRAW     2
#define UFT_CAPS_TYPE_KFSTREAM  3

/* Encoding types */
#define UFT_CAPS_ENC_NA         0
#define UFT_CAPS_ENC_MFM        1
#define UFT_CAPS_ENC_GCR        2
#define UFT_CAPS_ENC_FM         3
#define UFT_CAPS_ENC_RAW        4

/* Platform IDs */
#define UFT_CAPS_PLATFORM_NA        0
#define UFT_CAPS_PLATFORM_AMIGA     1
#define UFT_CAPS_PLATFORM_ATARI_ST  2
#define UFT_CAPS_PLATFORM_PC        3
#define UFT_CAPS_PLATFORM_SPECTRUM  4
#define UFT_CAPS_PLATFORM_CPC       5
#define UFT_CAPS_PLATFORM_C64       6
#define UFT_CAPS_PLATFORM_MSX       7
#define UFT_CAPS_PLATFORM_ARCHIE    8
#define UFT_CAPS_PLATFORM_MAC       9
#define UFT_CAPS_PLATFORM_APPLE2    10
#define UFT_CAPS_PLATFORM_SAM       11

/* Track flags */
#define UFT_CAPS_TF_FLAKEY      (1 << 0)
#define UFT_CAPS_TF_TIMING      (1 << 1)
#define UFT_CAPS_TF_MULTI_REV   (1 << 2)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Types (Forward Declarations)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_caps_image uft_caps_image_t;

/**
 * @brief Track information structure
 */
typedef struct {
    uint32_t type;
    uint32_t cylinder;
    uint32_t head;
    uint32_t sector_count;
    uint32_t sector_size;
    uint8_t *track_buf;
    uint32_t track_len;
    uint8_t *decoded_data;
    uint32_t decoded_size;
    uint32_t *timing_data;
    uint32_t timing_len;
    int32_t overlap;
    uint32_t start_bit;
    uint32_t weak_bits;
    uint32_t cell_ns;
    uint32_t encoding;
    uint32_t flags;
} uft_caps_track_info_t;

/**
 * @brief Image information structure
 */
typedef struct {
    uint32_t type;
    uint32_t platform;
    uint32_t release;
    uint32_t revision;
    uint32_t min_cylinder;
    uint32_t max_cylinder;
    uint32_t min_head;
    uint32_t max_head;
    uint32_t creation_date;
    uint32_t media_type;
} uft_caps_image_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Function Declarations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize CAPS image structure
 */
void uft_caps_image_init(uft_caps_image_t *img);

/**
 * @brief Free CAPS image resources
 */
void uft_caps_image_free(uft_caps_image_t *img);

/**
 * @brief Check if file is IPF format
 */
bool uft_caps_is_ipf(const uint8_t *data, size_t size);

/**
 * @brief Load IPF image from file
 */
int uft_caps_load_image(uft_caps_image_t *img, const char *filename);

/**
 * @brief Get image information
 */
int uft_caps_get_image_info(const uft_caps_image_t *img, uft_caps_image_info_t *info);

/**
 * @brief Get track information
 */
int uft_caps_get_track_info(const uft_caps_image_t *img, 
                            uint32_t cylinder, uint32_t head,
                            uft_caps_track_info_t *info);

/**
 * @brief Get number of revolutions for a track
 */
int uft_caps_get_revolutions(const uft_caps_image_t *img,
                             uint32_t cylinder, uint32_t head);

/**
 * @brief Set active revolution for track reading
 */
int uft_caps_set_revolution(uft_caps_image_t *img, int revolution);

/**
 * @brief Create IPF file
 */
int uft_caps_create_ipf(const char *filename,
                        uint32_t platform,
                        uint32_t min_cyl, uint32_t max_cyl,
                        uint32_t min_head, uint32_t max_head);

/**
 * @brief Get platform name
 */
const char *uft_caps_platform_name(uint32_t platform);

/**
 * @brief Get encoding name
 */
const char *uft_caps_encoding_name(uint32_t encoding);

/**
 * @brief Print IPF image summary
 */
void uft_caps_print_info(const uft_caps_image_t *img, FILE *out);

/**
 * @brief Get library version
 */
void uft_caps_get_version(int *version, int *revision);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CAPS_IPF_H */
