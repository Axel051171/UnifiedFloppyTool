/**
 * @file uft_woz_plugin.c
 * @brief WOZ (Apple II) Plugin-B wrapper
 *
 * Wraps the existing woz_load/woz_get_track_525/woz_free API
 * from uft_woz.c into the format plugin interface.
 */
#include "uft/uft_format_common.h"
#include "uft/formats/apple/uft_woz.h"

static bool woz_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 8) return false;
    uint32_t sig = uft_read_le32(data);
    if (sig == WOZ_SIGNATURE_V1 || sig == WOZ_SIGNATURE_V2) {
        *confidence = 98;
        return true;
    }
    return false;
}

static uft_error_t woz_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    woz_image_t *img = NULL;
    int rc = woz_load(path, &img);
    if (rc != 0 || !img) return UFT_ERROR_FORMAT_INVALID;

    disk->plugin_data = img;
    disk->geometry.cylinders = img->is_525 ? 35 : 80;
    disk->geometry.heads = img->is_525 ? 1 : (img->info.disk_sides ? img->info.disk_sides : 1);
    disk->geometry.sectors = img->is_525 ? 16 : 12;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders *
                                   disk->geometry.heads * disk->geometry.sectors;
    return UFT_OK;
}

static void woz_plugin_close(uft_disk_t *disk) {
    woz_image_t *img = (woz_image_t *)disk->plugin_data;
    if (img) { woz_free(img); disk->plugin_data = NULL; }
}

static uft_error_t woz_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    woz_image_t *img = (woz_image_t *)disk->plugin_data;
    if (!img) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    const uint8_t *bits = NULL;
    uint32_t bit_count = 0;
    int rc;

    if (img->is_525) {
        rc = woz_get_track_525(img, cyl * 4, &bits, &bit_count);  /* quarter-track */
    } else {
        rc = woz_get_track_35(img, cyl, head, &bits, &bit_count);
    }

    if (rc == 0 && bits && bit_count > 0) {
        /* Store raw bitstream as sector 0 */
        uint32_t byte_count = (bit_count + 7) / 8;
        uint16_t chunk = (byte_count > 65535) ? 65535 : (uint16_t)byte_count;
        uft_format_add_sector(track, 0, bits, chunk, (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_woz = {
    .name = "WOZ", .description = "Apple II WOZ (v1/v2/v2.1)",
    .extensions = "woz", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_WEAK_BITS,
    .probe = woz_plugin_probe, .open = woz_plugin_open,
    .close = woz_plugin_close, .read_track = woz_plugin_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(woz)
