/**
 * @file uft_fdi_plugin.c
 * @brief FDI (ZX Spectrum "Full Disk Image", Alex Makeev) Plugin-B
 *
 * This is the ZX Spectrum FDI format (ASCII "FDI" signature) — NOT the Anex86
 * PC-98 .fdi (a headerless raw dump handled by the fdi_pc98 plugin; the two are
 * disjoint by content, PC-98 FDI has no "FDI" magic). The prior implementation
 * had the right 14-byte header but a fabricated track/sector model (a flat
 * 4-byte-offset table + inline 5-byte sector descriptors with data). Rewritten
 * (MF-359) against SAMdisk's ReadFDI (src/samdisk/fdi.cpp) and the WoS format
 * reference (KNOWN_ISSUES FMT-12).
 *
 * Header (14 bytes):
 *   +0  "FDI"   +3 write-protect   +4 cylinders(LE16)   +6 heads(LE16)
 *   +8  desc offset(LE16)   +0x0A data offset(LE16)   +0x0C extra length(LE16)
 *
 * Track headers begin at 14 + extra-length and are walked sequentially (each is
 * variable length). FDI_TRACK = track-data offset (LE32, rel. to data offset)
 * + 2 reserved + sector count(1); then `sector count` FDI_SECTOR entries follow
 * inline. FDI_SECTOR = C(1) H(1) R(1) N(1) flags(1) sector-data offset(LE16,
 * rel. to the track's data). Sector data lives at data_off + track_off +
 * sector_off, size 128<<(N&3). Flags: bit7=deleted-DAM, bit6=no-data,
 * bit(N&3)=data-CRC-OK.
 */
#include "uft/uft_format_common.h"

#define FDI_SIG     "FDI"
#define FDI_HDR     14

typedef struct {
    uint8_t *data;
    size_t   size;
    char    *path;         /* kept so write_track can persist in place */
    uint16_t cyls;
    uint16_t heads;
    size_t   data_pos;     /* absolute file offset of the main data block */
    size_t   header_pos;   /* absolute file offset of the first track header */
} fdi_pd_t;

static char *fdi_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static bool fdi_plugin_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < FDI_HDR) return false;
    if (memcmp(d, FDI_SIG, 3) != 0) return false;
    uint16_t cyls = uft_read_le16(d + 4);
    uint16_t heads = uft_read_le16(d + 6);
    *c = (cyls > 0 && cyls <= 180 && heads > 0 && heads <= 2) ? 92 : 75;
    return true;
}

/* Walk the variable-length track headers to the `target`-th track. Returns the
 * absolute offset of its FDI_TRACK, or SIZE_MAX if out of range/truncated. */
static size_t fdi_track_cursor(const fdi_pd_t *p, int target) {
    size_t cur = p->header_pos;
    for (int t = 0; t < target; t++) {
        if (cur + 7 > p->size) return (size_t)-1;
        uint8_t nsec = p->data[cur + 6];
        cur += 7 + (size_t)nsec * 7;
    }
    if (cur + 7 > p->size) return (size_t)-1;
    return cur;
}

static uft_error_t fdi_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < FDI_HDR) { free(data); return UFT_ERROR_FILE_OPEN; }
    if (memcmp(data, FDI_SIG, 3) != 0) { free(data); return UFT_ERROR_FORMAT_INVALID; }

    fdi_pd_t *p = calloc(1, sizeof(fdi_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->cyls = uft_read_le16(data + 4);
    p->heads = uft_read_le16(data + 6);
    p->data_pos = uft_read_le16(data + 0x0A);
    p->header_pos = (size_t)FDI_HDR + uft_read_le16(data + 0x0C);

    if (p->cyls == 0 || p->cyls > 180 || p->heads == 0 || p->heads > 2 ||
        p->header_pos > file_size) {
        free(data); free(p); return UFT_ERROR_FORMAT_INVALID;
    }
    p->path = fdi_strdup(path);

    /* Determine geometry: walk all track headers for the max sectors/track and
     * a representative sector size. */
    uint16_t sec_size = 512;
    int max_spt = 0;
    size_t cur = p->header_pos;
    int ntracks = (int)p->cyls * p->heads;
    for (int t = 0; t < ntracks; t++) {
        if (cur + 7 > file_size) break;
        uint8_t nsec = p->data[cur + 6];
        if (nsec > max_spt) max_spt = nsec;
        if (t == 0 && nsec > 0 && cur + 7 + 4 <= file_size) {
            uint8_t n = p->data[cur + 7 + 3];   /* first sector's N */
            sec_size = (uint16_t)(128u << (n & 3));
        }
        cur += 7 + (size_t)nsec * 7;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyls;
    disk->geometry.heads = p->heads;
    disk->geometry.sectors = max_spt > 0 ? (uint32_t)max_spt : 9;
    disk->geometry.sector_size = sec_size;
    disk->geometry.total_sectors = (uint32_t)p->cyls * p->heads *
                                   (max_spt > 0 ? (uint32_t)max_spt : 9);
    return UFT_OK;
}

static void fdi_plugin_close(uft_disk_t *disk) {
    fdi_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p->path); free(p); disk->plugin_data = NULL; }
}

static uft_error_t fdi_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    fdi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->cyls || head >= p->heads) return UFT_ERROR_INVALID_ARG;

    uft_track_init(track, cyl, head);

    size_t cur = fdi_track_cursor(p, cyl * p->heads + head);
    if (cur == (size_t)-1) return UFT_OK;

    /* FDI_TRACK: 24-bit track-data offset (SAMdisk uses 3 bytes) + 2 reserved
     * + sector count. */
    uint32_t trk_off = (uint32_t)p->data[cur] |
                       ((uint32_t)p->data[cur + 1] << 8) |
                       ((uint32_t)p->data[cur + 2] << 16);
    uint8_t nsec = p->data[cur + 6];
    size_t track_pos = p->data_pos + trk_off;
    size_t sh = cur + 7;   /* first FDI_SECTOR */

    for (int s = 0; s < nsec && sh + 7 <= p->size; s++) {
        uint8_t sec_r  = p->data[sh + 2];
        uint8_t sec_n  = p->data[sh + 3];
        uint8_t flags  = p->data[sh + 4];
        uint16_t soff  = uft_read_le16(p->data + sh + 5);
        sh += 7;

        uint16_t ss = (uint16_t)(128u << (sec_n & 3));
        size_t sec_pos = track_pos + soff;
        bool deleted = (flags & 0x80) != 0;
        bool no_data = (flags & 0x40) != 0;
        bool crc_ok  = (flags & (1u << (sec_n & 3))) != 0;

        if (no_data || sec_pos + ss > p->size) {
            /* No/short data: forensic fill rather than dropping or over-reading. */
            uint8_t *fill = malloc(ss);
            if (!fill) return UFT_ERROR_NO_MEMORY;
            memset(fill, 0xE5, ss);
            uft_format_add_sector(track, sec_r ? sec_r - 1 : 0, fill, ss,
                                  (uint8_t)cyl, (uint8_t)head);
            free(fill);
            crc_ok = false;
        } else {
            uft_format_add_sector(track, sec_r ? sec_r - 1 : 0,
                                  p->data + sec_pos, ss,
                                  (uint8_t)cyl, (uint8_t)head);
        }
        if (track->sector_count > 0) {
            uft_sector_t *sec = &track->sectors[track->sector_count - 1];
            if (deleted) sec->deleted = true;
            if (!crc_ok) uft_sector_set_crc(sec, false);
        }
    }
    return UFT_OK;
}

static uft_error_t fdi_plugin_write_track(uft_disk_t *disk, int cyl, int head,
                                            const uft_track_t *track) {
    fdi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl >= p->cyls || head >= p->heads) return UFT_ERROR_INVALID_ARG;
    if (!p->path) return UFT_ERROR_INVALID_STATE;

    size_t cur = fdi_track_cursor(p, cyl * p->heads + head);
    if (cur == (size_t)-1) return UFT_ERROR_INVALID_ARG;

    uint32_t trk_off = (uint32_t)p->data[cur] |
                       ((uint32_t)p->data[cur + 1] << 8) |
                       ((uint32_t)p->data[cur + 2] << 16);
    uint8_t nsec = p->data[cur + 6];
    size_t track_pos = p->data_pos + trk_off;
    size_t sh = cur + 7;

    FILE *f = fopen(p->path, "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    uft_error_t rc = UFT_OK;

    for (int s = 0; s < nsec && sh + 7 <= p->size; s++) {
        uint8_t sec_r = p->data[sh + 2];
        uint8_t sec_n = p->data[sh + 3];
        uint8_t flags = p->data[sh + 4];
        uint16_t soff = uft_read_le16(p->data + sh + 5);
        sh += 7;

        if (flags & 0x40) continue;                 /* no-data sector */
        uint16_t ss = (uint16_t)(128u << (sec_n & 3));
        size_t sec_pos = track_pos + soff;
        if (sec_pos + ss > p->size) continue;

        /* read_track stores id.sector = R (it passes R-1 and add_sector adds 1),
         * so match the input sector by R directly. */
        for (size_t ts = 0; ts < track->sector_count; ts++) {
            if (track->sectors[ts].id.sector == sec_r) {
                const uint8_t *src = track->sectors[ts].data;
                if (src && track->sectors[ts].data_len >= ss) {
                    memcpy(p->data + sec_pos, src, ss);
                    if (fseek(f, (long)sec_pos, SEEK_SET) != 0 ||
                        fwrite(src, 1, ss, f) != ss) {
                        rc = UFT_ERROR_IO;
                    }
                }
                break;
            }
        }
    }

    fclose(f);
    return rc;
}

static const uft_plugin_feature_t uft_format_plugin_fdi_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_fdi = {
    .name = "FDI", .description = "ZX Spectrum Full Disk Image",
    .extensions = "fdi", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = fdi_plugin_probe, .open = fdi_plugin_open,
    .close = fdi_plugin_close, .read_track = fdi_plugin_read_track,
    .write_track = fdi_plugin_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_fdi_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_fdi_features) / sizeof(uft_format_plugin_fdi_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(fdi)
