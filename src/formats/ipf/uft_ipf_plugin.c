/**
 * @file uft_ipf_plugin.c
 * @brief IPF (Interchangeable Preservation Format) Plugin-B
 *
 * IPF (CAPS/SPS) stores flux-level data with copy protection info.
 * This plugin wraps the existing uft_caps_is_ipf() + ipf parser.
 */
#include "uft/uft_format_common.h"

extern bool uft_caps_is_ipf(const uint8_t *data, size_t size);

static bool ipf_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 12) return false;
    /* IPF magic: "CAPS" at offset 0 */
    if (data[0] == 'C' && data[1] == 'A' && data[2] == 'P' && data[3] == 'S') {
        *confidence = 95;
        return true;
    }
    if (uft_caps_is_ipf(data, size)) {
        *confidence = 90;
        return true;
    }
    return false;
}

typedef struct { uint8_t *data; size_t size; } ipf_data_t;

static uft_error_t ipf_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t sz = 0;
    uint8_t *data = uft_read_file(path, &sz);
    if (!data) return UFT_ERR_FILE_OPEN;

    ipf_data_t *p = calloc(1, sizeof(ipf_data_t));
    if (!p) { free(data); return UFT_ERR_MEMORY; }
    p->data = data; p->size = sz;

    disk->plugin_data = p;
    disk->geometry.cylinders = 84;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 11;  /* Amiga typical */
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = 84 * 2 * 11;
    return UFT_OK;
}

static void ipf_plugin_close(uft_disk_t *disk) {
    ipf_data_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t ipf_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    ipf_data_t *p = (ipf_data_t *)disk->plugin_data;
    if (!p) return UFT_ERR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    /* IPF: identification + metadata only. Track decode requires CAPS
     * record parsing which is not yet integrated. probe() + open() work,
     * read_track() returns empty. Use raw data via plugin_data for analysis. */
    return UFT_ERR_NOT_IMPLEMENTED;
}

/* NOTE: write_track omitted by design — IPF is a proprietary preservation
 * format that requires libcapsimage (SPS, closed-source) to encode. Sector
 * write would also bypass the copy-protection signatures that IPF is
 * purpose-built to preserve. */

/* Prinzip 7 Feature-Matrix — see docs/DESIGN_PRINCIPLES.md §7 */
static const uft_plugin_feature_t ipf_features[] = {
    { "Standard MFM Tracks",           UFT_FEATURE_SUPPORTED,   NULL },
    { "Timing Tracks",                 UFT_FEATURE_SUPPORTED,   NULL },
    { "Weak Bits",                     UFT_FEATURE_PARTIAL,
      "Detection yes, exact position within +/- 3 bits" },
    { "Data Cells",                    UFT_FEATURE_SUPPORTED,   NULL },
    { "Multi-Revolution Samples",      UFT_FEATURE_UNSUPPORTED, NULL },
    { "SPS Protection Markers",        UFT_FEATURE_UNSUPPORTED, NULL },
    { "Write / encode",                UFT_FEATURE_UNSUPPORTED,
      "requires closed-source libcapsimage" },
};

const uft_format_plugin_t uft_format_plugin_ipf = {
    .name = "IPF", .description = "CAPS/SPS Preservation Format",
    .extensions = "ipf;ct;ctr", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe = ipf_plugin_probe, .open = ipf_plugin_open,
    .close = ipf_plugin_close, .read_track = ipf_plugin_read_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* SPS never published full IPF spec */
    .features = ipf_features,
    .feature_count = sizeof(ipf_features) / sizeof(ipf_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(ipf)
