/**
 * @file uft_ipf_plugin.c
 * @brief IPF (Interchangeable Preservation Format) Plugin-B
 *
 * IPF (CAPS/SPS) stores flux-level data with copy protection info.
 * This plugin wraps the existing uft_caps_is_ipf() + ipf parser.
 *
 * read_track() returns IMGE-record metadata (track_bits, density, flags)
 * without fabricating a bitstream — SPS data-element fusion required for
 * full bitstream decode lives in src/formats/ipf/uft_ipf_air.c with
 * file-local types and is not yet bridged to the plugin (TODO-A1
 * follow-up). Forensic principle: no invented data, partial honesty over
 * silent stub.
 */
#include "uft/uft_format_common.h"
#include "uft/profiles/uft_ipf_format.h"

extern bool uft_caps_is_ipf(const uint8_t *data, size_t size);

/* IPF IMGE.track_flags — fuzzy-bit indicator (matches IPF_TF_FUZZY in
 * uft_ipf_air.c). Local copy because the constant is TU-private there. */
#define IPF_PLUGIN_TF_FUZZY  0x01u

/* Platform IDs — only AMIGA is needed for encoding selection; matches
 * caps_platform_t::CAPS_PLATFORM_AMIGA. */
#define IPF_PLUGIN_PLATFORM_AMIGA  1u

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

typedef struct {
    uint8_t        *data;
    size_t          size;
    uft_ipf_info_t  info;     /* Parsed INFO record + IMGE count */
} ipf_data_t;

static uft_error_t ipf_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t sz = 0;
    uint8_t *data = uft_read_file(path, &sz);
    if (!data) return UFT_ERR_FILE_OPEN;

    ipf_data_t *p = calloc(1, sizeof(ipf_data_t));
    if (!p) { free(data); return UFT_ERR_MEMORY; }
    p->data = data; p->size = sz;

    /* Parse INFO + count IMGE records to derive real geometry. Falls back
     * to Amiga DD defaults only if the parser returns no IMGE records. */
    if (!uft_ipf_parse(data, sz, &p->info)) {
        free(data); free(p);
        return UFT_ERR_FORMAT_INVALID;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = (p->info.cylinders > 0) ? p->info.cylinders : 84;
    disk->geometry.heads     = (p->info.sides     > 0) ? p->info.sides     : 2;
    disk->geometry.sectors = 11;            /* Amiga DD typical; sector layer not derivable from IMGE alone */
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors =
        (uint32_t)disk->geometry.cylinders *
        (uint32_t)disk->geometry.heads *
        (uint32_t)disk->geometry.sectors;
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

    /* Walk the file linearly looking for an IMGE record matching (cyl, head).
     * IMGE record layout (80 bytes after 12-byte block header), all big-endian:
     *   +0  cylinder    +4  head        +8  density     +12 signal_type
     *   +16 track_bytes +20 start_byte  +24 start_bit   +28 data_bits
     *   +32 gap_bits    +36 track_bits  +40 block_count +44 encoder_process
     *   +48 track_flags +52 data_key    +56..79 reserved */
    size_t offset = 0;
    bool found = false;
    while (offset + UFT_IPF_BLOCK_HEADER_SIZE <= p->size) {
        uint32_t type   = uft_ipf_read_be32(p->data + offset);
        uint32_t length = uft_ipf_read_be32(p->data + offset + 4);
        if (length < UFT_IPF_BLOCK_HEADER_SIZE || offset + length > p->size) break;

        if (type == UFT_IPF_RECORD_IMGE &&
            length >= UFT_IPF_BLOCK_HEADER_SIZE + 56)
        {
            const uint8_t *rec = p->data + offset + UFT_IPF_BLOCK_HEADER_SIZE;
            uint32_t r_cyl  = uft_ipf_read_be32(rec + 0);
            uint32_t r_head = uft_ipf_read_be32(rec + 4);
            if ((int)r_cyl == cyl && (int)r_head == head) {
                uint32_t density     = uft_ipf_read_be32(rec + 8);
                uint32_t track_bits  = uft_ipf_read_be32(rec + 36);
                uint32_t track_flags = uft_ipf_read_be32(rec + 48);

                /* Encoding: Amiga images use AMIGA_MFM, others generic MFM.
                 * IPF uses 2µs MFM cells universally — DD bitcell. */
                track->encoding = (p->info.platforms == IPF_PLUGIN_PLATFORM_AMIGA)
                    ? UFT_ENC_AMIGA_MFM
                    : UFT_ENC_MFM;
                track->bitrate              = 500000;   /* 500 kbps cell rate (250 kbps data) */
                track->nominal_bit_rate_kbps = 250.0;
                track->nominal_rpm           = 300.0;
                track->avg_bit_cell_ns       = 2000.0;

                /* Track size: descriptor-derived bit count. raw_data stays
                 * NULL — payload extraction (SPS gap/data element fusion)
                 * deferred to a later integration pass; do NOT fabricate. */
                track->raw_bits = track_bits;

                /* Status flags from IPF descriptor */
                if (track_flags & IPF_PLUGIN_TF_FUZZY) {
                    track->status |= (uint32_t)UFT_TRACK_FUZZY |
                                     (uint32_t)UFT_TRACK_PROTECTED;
                    track->copy_protected = true;
                }
                /* density >= 3 = Copylock/Speedlock variants per IPF spec */
                if (density >= 3) {
                    track->status |= (uint32_t)UFT_TRACK_PROTECTED;
                    track->copy_protected = true;
                }

                found = true;
                break;
            }
        }
        offset += length;
    }

    if (!found) {
        /* No IMGE descriptor for this (cyl, head) — track absent from image. */
        return UFT_ERR_MISSING_SECTOR;
    }
    return UFT_OK;
}

/* NOTE: write_track omitted by design — IPF is a proprietary preservation
 * format that requires libcapsimage (SPS, closed-source) to encode. Sector
 * write would also bypass the copy-protection signatures that IPF is
 * purpose-built to preserve. */

/* Prinzip 7 Feature-Matrix — see docs/DESIGN_PRINCIPLES.md §7
 * Updated post-A1: reflect that read_track returns metadata only, payload
 * fusion is still deferred — partial honesty replaces previous overstated
 * SUPPORTED claims. */
static const uft_plugin_feature_t ipf_features[] = {
    { "Standard MFM Tracks",           UFT_FEATURE_PARTIAL,
      "IMGE metadata exposed (track_bits, density, flags); SPS data-element fusion deferred" },
    { "Timing Tracks",                 UFT_FEATURE_PARTIAL,
      "track_flags exposed; per-cell timing not extracted" },
    { "Weak Bits",                     UFT_FEATURE_PARTIAL,
      "Detection via FUZZY flag; exact bit positions not yet decoded" },
    { "Data Cells",                    UFT_FEATURE_UNSUPPORTED,
      "DATA-block element fusion not bridged to plugin" },
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
