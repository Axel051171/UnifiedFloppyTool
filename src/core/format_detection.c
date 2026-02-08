/**
 * @file format_detection.c
 * @brief Format variant detection and mapping utilities
 */

#include "uft/format_detection.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    const uft_format_variant_t *variant;
    float score;
    char note[128];
} uft_detection_candidate_t;

static const uint8_t k_fat12_magic[] = {0x55, 0xAA};
static const uint8_t k_amiga_magic[] = {'D', 'O', 'S'};

static const uft_format_variant_t k_variants[] = {
    {
        .id = "pc_360k_fat12",
        .name = "PC 360K FAT12 (5.25\" DD, 40c/2h/9s)",
        .family = UFT_FAMILY_FAT12,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {40, 2, 9, 512, 250000, 300},
        .interleave = 1,
        .first_sector_id = 1,
        .total_sectors = 40 * 2 * 9,
        .boot_magic = k_fat12_magic,
        .boot_magic_len = sizeof(k_fat12_magic),
        .boot_magic_offset = 510,
        .amiga_dos_type = 0
    },
    {
        .id = "pc_720k_fat12",
        .name = "PC 720K FAT12 (3.5\" DD, 80c/2h/9s)",
        .family = UFT_FAMILY_FAT12,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {80, 2, 9, 512, 250000, 300},
        .interleave = 1,
        .first_sector_id = 1,
        .total_sectors = 80 * 2 * 9,
        .boot_magic = k_fat12_magic,
        .boot_magic_len = sizeof(k_fat12_magic),
        .boot_magic_offset = 510,
        .amiga_dos_type = 0
    },
    {
        .id = "pc_12m_fat12",
        .name = "PC 1.2M FAT12 (5.25\" HD, 80c/2h/15s)",
        .family = UFT_FAMILY_FAT12,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {80, 2, 15, 512, 500000, 360},
        .interleave = 1,
        .first_sector_id = 1,
        .total_sectors = 80 * 2 * 15,
        .boot_magic = k_fat12_magic,
        .boot_magic_len = sizeof(k_fat12_magic),
        .boot_magic_offset = 510,
        .amiga_dos_type = 0
    },
    {
        .id = "pc_144m_fat12",
        .name = "PC 1.44M FAT12 (3.5\" HD, 80c/2h/18s)",
        .family = UFT_FAMILY_FAT12,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {80, 2, 18, 512, 500000, 300},
        .interleave = 1,
        .first_sector_id = 1,
        .total_sectors = 80 * 2 * 18,
        .boot_magic = k_fat12_magic,
        .boot_magic_len = sizeof(k_fat12_magic),
        .boot_magic_offset = 510,
        .amiga_dos_type = 0
    },
    {
        .id = "amiga_ofs_dd",
        .name = "Amiga OFS DD (880K, 80c/2h/11s)",
        .family = UFT_FAMILY_AMIGA,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {80, 2, 11, 512, 250000, 300},
        .interleave = 0,
        .first_sector_id = 0,
        .total_sectors = 80 * 2 * 11,
        .boot_magic = k_amiga_magic,
        .boot_magic_len = sizeof(k_amiga_magic),
        .boot_magic_offset = 0,
        .amiga_dos_type = 0
    },
    {
        .id = "amiga_ffs_dd",
        .name = "Amiga FFS DD (880K, 80c/2h/11s)",
        .family = UFT_FAMILY_AMIGA,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {80, 2, 11, 512, 250000, 300},
        .interleave = 0,
        .first_sector_id = 0,
        .total_sectors = 80 * 2 * 11,
        .boot_magic = k_amiga_magic,
        .boot_magic_len = sizeof(k_amiga_magic),
        .boot_magic_offset = 0,
        .amiga_dos_type = 1
    },
    {
        .id = "amiga_ffs_intl_dd",
        .name = "Amiga FFS Intl DD (880K, 80c/2h/11s)",
        .family = UFT_FAMILY_AMIGA,
        .encoding = UFT_ENCODING_MFM,
        .geometry = {80, 2, 11, 512, 250000, 300},
        .interleave = 0,
        .first_sector_id = 0,
        .total_sectors = 80 * 2 * 11,
        .boot_magic = k_amiga_magic,
        .boot_magic_len = sizeof(k_amiga_magic),
        .boot_magic_offset = 0,
        .amiga_dos_type = 2
    },
    {
        .id = "apple2_140k",
        .name = "Apple II 140K (35c/1h/16s)",
        .family = UFT_FAMILY_APPLE_II,
        .encoding = UFT_ENCODING_GCR,
        .geometry = {35, 1, 16, 256, 250000, 300},
        .interleave = 6,
        .first_sector_id = 0,
        .total_sectors = 35 * 1 * 16,
        .boot_magic = NULL,
        .boot_magic_len = 0,
        .boot_magic_offset = 0,
        .amiga_dos_type = 0
    },
    {
        .id = "c64_1541",
        .name = "Commodore 1541 (35c/1h, zoned)",
        .family = UFT_FAMILY_C64,
        .encoding = UFT_ENCODING_GCR,
        .geometry = {35, 1, 21, 256, 250000, 300},
        .interleave = 10,
        .first_sector_id = 0,
        .total_sectors = 0,
        .boot_magic = NULL,
        .boot_magic_len = 0,
        .boot_magic_offset = 0,
        .amiga_dos_type = 0
    }
};

static int uft_min_int(int a, int b)
{
    return (a < b) ? a : b;
}

static bool uft_has_magic(const uint8_t *buffer, size_t buffer_len,
                          const uint8_t *magic, size_t magic_len, size_t offset)
{
    if (!buffer || !magic || magic_len == 0) {
        return false;
    }
    if (offset + magic_len > buffer_len) {
        return false;
    }
    return memcmp(buffer + offset, magic, magic_len) == 0;
}

static int uft_parse_amiga_dos_type(const uint8_t *boot_sector, size_t size)
{
    if (!boot_sector || size < 4) {
        return -1;
    }
    if (!uft_has_magic(boot_sector, size, k_amiga_magic, sizeof(k_amiga_magic), 0)) {
        return -1;
    }
    return boot_sector[3];
}

static float uft_score_geometry(const uft_format_variant_t *variant,
                                const uft_geometry_t *geometry)
{
    if (!variant || !geometry) {
        return 0.0f;
    }
    float score = 0.0f;
    if (geometry->tracks == variant->geometry.tracks) {
        score += 0.25f;
    }
    if (geometry->heads == variant->geometry.heads) {
        score += 0.25f;
    }
    if (geometry->sectors_per_track == variant->geometry.sectors_per_track) {
        score += 0.2f;
    }
    if (geometry->sector_size == variant->geometry.sector_size) {
        score += 0.2f;
    }
    if (geometry->bitrate == variant->geometry.bitrate) {
        score += 0.05f;
    }
    if (geometry->rpm == variant->geometry.rpm) {
        score += 0.05f;
    }
    return score;
}

const uft_format_variant_t *uft_get_format_variants(size_t *count)
{
    if (count) {
        *count = sizeof(k_variants) / sizeof(k_variants[0]);
    }
    return k_variants;
}

const uft_format_variant_t *uft_find_format_variant(const char *id)
{
    size_t count = 0;
    const uft_format_variant_t *variants = uft_get_format_variants(&count);
    if (!id) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(variants[i].id, id) == 0) {
            return &variants[i];
        }
    }
    return NULL;
}

static void uft_set_warning(uft_format_detection_t *result, const char *warning)
{
    if (!result || !warning) {
        return;
    }
    snprintf(result->warning, sizeof(result->warning), "%s", warning);
}

bool uft_detect_format_variant(const uint8_t *boot_sector,
                               size_t boot_size,
                               const uft_geometry_t *geometry,
                               uft_format_detection_t *result)
{
    if (!result) {
        return false;
    }

    memset(result, 0, sizeof(*result));
    result->variant = NULL;
    result->confidence = 0.0f;
    result->warning[0] = '\0';

    size_t count = 0;
    const uft_format_variant_t *variants = uft_get_format_variants(&count);
    uft_detection_candidate_t best = {0};
    uft_detection_candidate_t second = {0};

    int amiga_dos_type = uft_parse_amiga_dos_type(boot_sector, boot_size);

    for (size_t i = 0; i < count; i++) {
        const uft_format_variant_t *variant = &variants[i];
        float score = 0.0f;
        char note[128] = "";

        if (geometry) {
            score += uft_score_geometry(variant, geometry);
        }

        if (variant->boot_magic && boot_sector) {
            if (uft_has_magic(boot_sector, boot_size, variant->boot_magic,
                              variant->boot_magic_len, variant->boot_magic_offset)) {
                score += 0.35f;
                snprintf(note, sizeof(note), "boot signature matched");
            }
        }

        if (variant->family == UFT_FAMILY_AMIGA && amiga_dos_type >= 0) {
            if (variant->amiga_dos_type == amiga_dos_type) {
                score += 0.4f;
                snprintf(note, sizeof(note), "Amiga DOS type matched");
            } else if (variant->amiga_dos_type == 0) {
                score += 0.1f;
            }
        }

        if (score > best.score) {
            second = best;
            best.variant = variant;
            best.score = score;
            snprintf(best.note, sizeof(best.note), "%s", note);
        } else if (score > second.score) {
            second.variant = variant;
            second.score = score;
            snprintf(second.note, sizeof(second.note), "%s", note);
        }
    }

    result->variant = best.variant;
    result->confidence = best.score;

    if (!best.variant || best.score < 0.4f) {
        uft_set_warning(result, "Low confidence: unable to clearly identify format variant.");
        return false;
    }

    if (second.variant && (best.score - second.score) < 0.15f) {
        uft_set_warning(result, "Ambiguous match: multiple variants fit the image.");
    } else if (best.note[0] == '\0' && geometry) {
        uft_set_warning(result, "Matched by geometry only; boot signature missing.");
    }

    return true;
}

bool uft_format_sector_offset(const uft_format_variant_t *variant,
                              int track,
                              int head,
                              int sector_id,
                              size_t *offset,
                              char *warning,
                              size_t warning_len)
{
    if (!variant || !offset) {
        return false;
    }

    if (track < 0 || head < 0 || sector_id < variant->first_sector_id) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Invalid sector request.");
        }
        return false;
    }

    int sector_index = sector_id - variant->first_sector_id;
    if (sector_index >= variant->geometry.sectors_per_track) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Sector ID out of range for track.");
        }
        return false;
    }

    size_t track_index = (size_t)track * (size_t)variant->geometry.heads + (size_t)head;
    size_t sectors_per_cylinder = (size_t)variant->geometry.heads *
                                  (size_t)variant->geometry.sectors_per_track;
    size_t sector_offset = (track_index * sectors_per_cylinder + (size_t)sector_index) *
                           (size_t)variant->geometry.sector_size;

    *offset = sector_offset;
    return true;
}

bool uft_image_read_sector(const uft_format_variant_t *variant,
                           const uint8_t *image,
                           size_t image_size,
                           int track,
                           int head,
                           int sector_id,
                           uint8_t *out_sector,
                           size_t out_size,
                           char *warning,
                           size_t warning_len)
{
    size_t offset = 0;
    if (!variant || !image || !out_sector) {
        return false;
    }
    if (out_size < (size_t)variant->geometry.sector_size) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Output buffer too small for sector.");
        }
        return false;
    }
    if (!uft_format_sector_offset(variant, track, head, sector_id, &offset, warning, warning_len)) {
        return false;
    }
    if (offset + (size_t)variant->geometry.sector_size > image_size) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Sector read exceeds image size (dirty dump).");
        }
        return false;
    }
    memcpy(out_sector, image + offset, (size_t)variant->geometry.sector_size);
    return true;
}

bool uft_image_write_sector(const uft_format_variant_t *variant,
                            uint8_t *image,
                            size_t image_size,
                            int track,
                            int head,
                            int sector_id,
                            const uint8_t *sector_data,
                            size_t sector_size,
                            char *warning,
                            size_t warning_len)
{
    size_t offset = 0;
    if (!variant || !image || !sector_data) {
        return false;
    }
    if (sector_size != (size_t)variant->geometry.sector_size) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Sector size mismatch for format variant.");
        }
        return false;
    }
    if (!uft_format_sector_offset(variant, track, head, sector_id, &offset, warning, warning_len)) {
        return false;
    }
    if (offset + sector_size > image_size) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Sector write exceeds image size (dirty dump).");
        }
        return false;
    }
    memcpy(image + offset, sector_data, sector_size);
    return true;
}

bool uft_convert_variant(const uft_format_variant_t *source,
                         const uft_format_variant_t *target,
                         uint8_t *image,
                         size_t image_size,
                         char *warning,
                         size_t warning_len)
{
    if (!source || !target || !image) {
        return false;
    }
    if (source->family != target->family) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Conversion only supported within format family.");
        }
        return false;
    }
    if (source->geometry.sector_size != target->geometry.sector_size) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Sector size mismatch; conversion not supported.");
        }
        return false;
    }

    size_t source_sectors = (size_t)source->geometry.tracks *
                            (size_t)source->geometry.heads *
                            (size_t)source->geometry.sectors_per_track;
    size_t target_sectors = (size_t)target->geometry.tracks *
                            (size_t)target->geometry.heads *
                            (size_t)target->geometry.sectors_per_track;
    size_t min_sectors = (size_t)uft_min_int((int)source_sectors, (int)target_sectors);
    size_t needed_size = target_sectors * (size_t)target->geometry.sector_size;

    if (image_size < needed_size) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len, "Image buffer too small for target variant.");
        }
        return false;
    }

    size_t copy_bytes = min_sectors * (size_t)source->geometry.sector_size;

    if (target_sectors > source_sectors) {
        size_t fill_bytes = (target_sectors - source_sectors) *
                            (size_t)target->geometry.sector_size;
        memset(image + copy_bytes, 0x00, fill_bytes);
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len,
                     "Extended image with zero-filled sectors for target variant.");
        }
    } else if (target_sectors < source_sectors) {
        if (warning && warning_len > 0) {
            snprintf(warning, warning_len,
                     "Truncated image to fit target variant.");
        }
    }

    return true;
}
