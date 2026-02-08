/**
 * @file format_detection.h
 * @brief Floppy format variant detection and mapping utilities
 * @version 1.0.0
 */

#ifndef UFT_FORMAT_DETECTION_H
#define UFT_FORMAT_DETECTION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_ENCODING_MFM,
    UFT_ENCODING_FM,
    UFT_ENCODING_GCR,
    UFT_ENCODING_UNKNOWN
} uft_encoding_t;

typedef enum {
    UFT_FAMILY_FAT12,
    UFT_FAMILY_AMIGA,
    UFT_FAMILY_APPLE_II,
    UFT_FAMILY_C64,
    UFT_FAMILY_UNKNOWN
} uft_format_family_t;

typedef struct {
    int tracks;
    int heads;
    int sectors_per_track;
    int sector_size;
    int bitrate;
    int rpm;
    uft_encoding_t encoding;
} uft_geometry_t;

typedef struct {
    const char *id;
    const char *name;
    uft_format_family_t family;
    uft_encoding_t encoding;
    uft_geometry_t geometry;
    int interleave;
    int first_sector_id;
    int total_sectors;
    const uint8_t *boot_magic;
    size_t boot_magic_len;
    size_t boot_magic_offset;
    int amiga_dos_type;
} uft_format_variant_t;

typedef struct {
    const uft_format_variant_t *variant;
    float confidence;
    char warning[256];
} uft_format_detection_t;

const uft_format_variant_t *uft_get_format_variants(size_t *count);
const uft_format_variant_t *uft_find_format_variant(const char *id);

bool uft_detect_format_variant(const uint8_t *boot_sector,
                               size_t boot_size,
                               const uft_geometry_t *geometry,
                               uft_format_detection_t *result);

bool uft_format_sector_offset(const uft_format_variant_t *variant,
                              int track,
                              int head,
                              int sector_id,
                              size_t *offset,
                              char *warning,
                              size_t warning_len);

bool uft_image_read_sector(const uft_format_variant_t *variant,
                           const uint8_t *image,
                           size_t image_size,
                           int track,
                           int head,
                           int sector_id,
                           uint8_t *out_sector,
                           size_t out_size,
                           char *warning,
                           size_t warning_len);

bool uft_image_write_sector(const uft_format_variant_t *variant,
                            uint8_t *image,
                            size_t image_size,
                            int track,
                            int head,
                            int sector_id,
                            const uint8_t *sector_data,
                            size_t sector_size,
                            char *warning,
                            size_t warning_len);

bool uft_convert_variant(const uft_format_variant_t *source,
                         const uft_format_variant_t *target,
                         uint8_t *image,
                         size_t image_size,
                         char *warning,
                         size_t warning_len);

#ifdef __cplusplus
}
#endif

#endif
