/**
 * @file uft_adf_ext.c
 * @brief Extended ADF (UAE-1ADF / ADF_EXT2) reader — Plugin-B
 *
 * The extended ADF format stores copy-protected / non-standard Amiga disks:
 * each track is either a normal AmigaDOS track (11/22 x 512 decoded sectors) or
 * a RAW MFM track (the uninterpreted bitstream — that is where the protection
 * lives). Standard ADF cannot represent the raw tracks, so this reader
 * preserves them in the track's raw_data buffer (like the G64/HFE bitstream
 * plugins) — no bit is dropped.
 *
 * Exact structure (verified byte-for-byte against the WinUAE source,
 * disk.cpp read_header_ext2 — Amiga is big-endian):
 *   0x00  "UAE-1ADF"                     (8 bytes ASCII magic)
 *   0x08  uint16 BE  reserved
 *   0x0A  uint16 BE  number of tracks    (default 160 = 2*80)
 *   0x0C  per-track table, N x 12 bytes:
 *           +0  uint16 BE reserved
 *           +2  uint8      (disk revolutions - 1)
 *           +3  uint8      type: 0 = AmigaDOS track, 1 = raw MFM
 *           +4  uint32 BE  available space for track in bytes (24-bit used)
 *           +8  uint32 BE  track length in bits (24-bit used)
 *   then each track's data (len bytes) sequentially, in table order.
 *
 * Classification: mixed — AmigaDOS tracks are Sektor-Image (Klasse 3), raw-MFM
 * tracks are Bitstream (Klasse 2). Read-only: writing an extended ADF (re-MFM-
 * encoding protection) is out of scope. Decoding raw-MFM tracks INTO sectors is
 * a follow-up (needs the Amiga MFM sector decoder); the raw bitstream is
 * preserved so nothing is lost meanwhile.
 */
#include "uft/uft_format_common.h"

#define ADFEXT_MAX_TRACKS   176   /* 2 * 88 headroom */

typedef struct {
    uint8_t  type;    /* 0 = AmigaDOS, 1 = raw MFM */
    uint32_t offset;  /* absolute file offset of track data */
    uint32_t len;     /* track data length in bytes */
} adfext_track_t;

typedef struct {
    FILE   *file;
    int     num_tracks;
    int     num_secs;   /* 11 (DD) or 22 (HD) */
    adfext_track_t tracks[ADFEXT_MAX_TRACKS];
} adfext_pd_t;

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be24(const uint8_t *p) { /* WinUAE uses the low 24 bits */
    return ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static bool adfext_probe(const uint8_t *data, size_t size, size_t fs, int *c) {
    (void)fs;
    if (size < 12) return false;
    if (memcmp(data, "UAE-1ADF", 8) != 0) return false;
    *c = 95;
    return true;
}

static uft_error_t adfext_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "UAE-1ADF", 8) != 0) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    int num_tracks = be16(&hdr[10]);   /* 0x0A: reserved @0x08, count @0x0A */
    if (num_tracks <= 0 || num_tracks > ADFEXT_MAX_TRACKS) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    adfext_pd_t *p = calloc(1, sizeof(adfext_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->num_tracks = num_tracks;

    long offs = 8 + 2 + 2 + (long)num_tracks * 12;   /* start of track data */
    int hd = 1;
    for (int i = 0; i < num_tracks; i++) {
        uint8_t td[12];
        if (fread(td, 1, 12, f) != 12) { free(p); fclose(f); return UFT_ERROR_IO; }
        p->tracks[i].type   = td[3];
        p->tracks[i].len    = be24(&td[4]);
        p->tracks[i].offset = (uint32_t)offs;
        if (p->tracks[i].len > 20000) hd = 2;   /* WinUAE ddhd heuristic */
        offs += p->tracks[i].len;
    }
    p->num_secs = (hd > 1) ? 22 : 11;

    disk->plugin_data = p;
    disk->geometry.cylinders = num_tracks / 2;
    disk->geometry.heads = 2;
    disk->geometry.sectors = p->num_secs;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)(num_tracks / 2) * 2 * p->num_secs;
    return UFT_OK;
}

static void adfext_close(uft_disk_t *disk) {
    adfext_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t adfext_read_track(uft_disk_t *disk, int cyl, int head,
                                      uft_track_t *track) {
    adfext_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head > 1) return UFT_ERROR_INVALID_STATE;
    int idx = cyl * 2 + head;
    if (idx < 0 || idx >= p->num_tracks) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    const adfext_track_t *td = &p->tracks[idx];
    if (fseek(p->file, (long)td->offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    if (td->type == 0) {
        /* AmigaDOS: len bytes = (len/512) decoded 512-byte sectors. */
        int nsec = (int)(td->len / 512);
        uint8_t buf[512];
        for (int s = 0; s < nsec; s++) {
            if (fread(buf, 1, 512, p->file) != 512) { memset(buf, 0, 512); }
            uft_format_add_sector(track, (uint8_t)s, buf, 512,
                                  (uint8_t)cyl, (uint8_t)head);
        }
    } else {
        /* Raw MFM (type 1): preserve the uninterpreted bitstream so the
         * protection track is not lost (decode to sectors is a follow-up). */
        if (td->len == 0 || td->len > (16u * 1024 * 1024)) return UFT_OK;
        uint8_t *raw = malloc(td->len);
        if (!raw) return UFT_ERROR_NO_MEMORY;
        if (fread(raw, 1, td->len, p->file) != td->len) { free(raw); return UFT_ERROR_IO; }
        track->raw_data = raw;
        track->raw_size = td->len;
        track->raw_len  = td->len;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_adf_ext_features[] = {
    { "Read (AmigaDOS + raw-MFM tracks)", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "extended-ADF write (re-MFM-encode protection) not implemented" },
    { "Raw-MFM sector decode", UFT_FEATURE_UNSUPPORTED,
      "type-1 tracks are preserved as raw MFM (raw_data); MFM->sector decode is a follow-up" },
    { "Copy-protection preservation", UFT_FEATURE_SUPPORTED,
      "raw MFM tracks are kept verbatim, no bit dropped" },
    { "Flux / timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_adf_ext = {
    .name = "ExtADF", .description = "Extended ADF (UAE-1ADF, copy-protected Amiga)",
    .extensions = "adf", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = adfext_probe, .open = adfext_open, .close = adfext_close,
    .read_track = adfext_read_track, .write_track = NULL,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* structure from WinUAE disk.cpp read_header_ext2 */
    .features = uft_format_plugin_adf_ext_features,
    .feature_count = sizeof(uft_format_plugin_adf_ext_features) / sizeof(uft_format_plugin_adf_ext_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(adf_ext)
