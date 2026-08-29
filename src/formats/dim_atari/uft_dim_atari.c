/**
 * @file uft_dim_atari.c
 * @brief Atari ST DIM (FastCopy Pro) Plugin-B
 *
 * DIM is the FastCopy Pro disk image format for the Atari ST.
 * 32-byte header followed by raw sector data.
 *
 * Header layout:
 *   Offset  Size  Description
 *   0x00    2     ID header 0x4242 ("BB") — REQUIRED
 *   0x02    1     1 = geometry auto-detected, 0 = user-specified
 *   0x03    1     Sectors used only (0=all, 1=used only)
 *   0x04    2     Reserved
 *   0x06    1     Sides (0=single, 1=double)
 *   0x07    1     Reserved
 *   0x08    1     Sectors per track (9, 10, 11, 18)
 *   0x09    1     Reserved
 *   0x0A    1     Start track
 *   0x0B    1     Reserved
 *   0x0C    1     End track (typically 79 or 39)
 *   0x0D    1     Density (0=DD, 1=HD)
 *   0x0E-1F       Reserved (zero padded)
 *
 * Reference for the layout above, and for the magic in particular
 * (MF-687): Hatari `src/floppies/dim.c:34-45` (header table) and `:75-76`
 * (rejection). Cross-checked in two further independent implementations —
 * HxC libhxcfe `dim_loader/dim_loader.c:74` and `:110`, and Jacknife
 * `dllmain.c:590`. All three refuse a file whose first two bytes are not
 * 0x42 0x42.
 *
 * Until MF-687 this comment claimed offset 0x00 was "Flags (unused)" and
 * 0x01-0x02 "Reserved". No source supports that, and the probe checked no
 * magic at all: any file of a plausible length was accepted as DIM and its
 * first 32 bytes read as geometry.
 *
 * IMPORTANT: Must NOT match X68000 DIM which uses byte 0 as media type
 * (valid X68000 media types: 0x00, 0x01, 0x02, 0x03, 0x09, 0x11, 0x19)
 * and has a 256-byte header.
 *
 * Geometry:
 *   DD: 80 cyl, 1-2 sides, 9-11 spt, 512 bytes/sector
 *   HD: 80 cyl, 2 sides, 18 spt, 512 bytes/sector
 *
 * Reference: FastCopy Pro documentation, Atari ST preservation community
 */

#include "uft/uft_format_common.h"

#define DIM_ATARI_HDR_SIZE   32
#define DIM_ATARI_SEC_SIZE   512

/* X68000 media type codes that must NOT match */
static bool dim_atari_is_x68k_media(uint8_t byte0)
{
    return (byte0 == 0x00 || byte0 == 0x01 || byte0 == 0x02 ||
            byte0 == 0x03 || byte0 == 0x09 || byte0 == 0x11 ||
            byte0 == 0x19);
}

typedef struct {
    FILE    *file;
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  spt;
    uint8_t  density;  /* 0=DD, 1=HD */
} dim_atari_pd_t;

static bool dim_atari_get_geometry(const uint8_t *hdr, size_t file_size,
                                   uint8_t *cyl, uint8_t *heads,
                                   uint8_t *spt, uint8_t *density)
{
    uint8_t sides_byte  = hdr[0x06];  /* 0=single, 1=double */
    uint8_t spt_byte    = hdr[0x08];
    uint8_t start_track = hdr[0x0A];
    uint8_t end_track   = hdr[0x0C];
    uint8_t dens_byte   = hdr[0x0D];

    /* Validate sides */
    if (sides_byte > 1) return false;
    uint8_t h = sides_byte + 1;

    /* Validate sectors per track */
    if (spt_byte != 9 && spt_byte != 10 && spt_byte != 11 && spt_byte != 18)
        return false;

    /* Validate density */
    if (dens_byte > 1) return false;

    /* HD must be 18 spt, 2 sides */
    if (dens_byte == 1 && (spt_byte != 18 || h != 2))
        return false;

    /* Validate track range */
    if (end_track < start_track) return false;
    uint8_t c = end_track + 1;
    if (c == 0 || c > 85) return false;

    /* Validate file size: header + expected data */
    size_t expected = DIM_ATARI_HDR_SIZE +
                      (size_t)c * h * spt_byte * DIM_ATARI_SEC_SIZE;
    if (file_size != expected) return false;

    *cyl = c;
    *heads = h;
    *spt = spt_byte;
    *density = dens_byte;
    return true;
}

static bool dim_atari_probe(const uint8_t *data, size_t size, size_t file_size,
                             int *confidence)
{
    if (size < DIM_ATARI_HDR_SIZE) return false;

    /* ID header 0x4242 ("BB") — the format's magic (MF-687).
     *
     * Three independent implementations refuse a file without it: Hatari
     * `dim.c:75-76`, HxC `dim_loader.c:74`/`:110`, Jacknife
     * `dllmain.c:590`. Hatari additionally documents it as the format's
     * first field (`dim.c:36`).
     *
     * Deliberately NOT copied from Hatari: it also rejects `[0x03] != 0`
     * and `[0x0A] != 0`. Those are Hatari's own limits — it cannot handle
     * "used sectors only" images or a non-zero start track — not
     * properties of the format. The header table above documents both
     * fields as legal, and refusing them here would turn a reader
     * limitation into a false verdict about the file.
     *
     * Only the magic is agreed by all three sources, so only the magic is
     * enforced. */
    if (data[0x00] != 0x42 || data[0x01] != 0x42) return false;

    /* Reject if file looks like X68000 DIM (256-byte header, known media type)
     *
     * MF-687: with the magic check above this branch is very nearly dead —
     * an X68000 image would have to carry 0x4242 by coincidence. It is
     * left in place because removing it is a separate change with its own
     * evidence: a variants cycle reports that the media-type table here is
     * contradicted by MAME and HxC. Noted as Fundus, not acted on. */
    if (dim_atari_is_x68k_media(data[0]) && file_size > 256) {
        /* Check if X68000 geometry matches: 256-byte header + data */
        /* X68000 DIM has 256-byte header; Atari DIM has 32-byte header.
         * If byte 0 is a valid X68000 media type AND the file size
         * matches X68000 geometry (with 256-byte header), reject. */
        uint8_t x68k_media = data[0];
        size_t x68k_data_size = file_size - 256;
        bool x68k_match = false;
        switch (x68k_media) {
            case 0x00: case 0x01: case 0x02: case 0x03:
                /* 77*2*8*1024 = 1,261,568 */
                if (x68k_data_size == 1261568) x68k_match = true;
                break;
            case 0x09:
                /* 80*2*18*512 = 1,474,560 */
                if (x68k_data_size == 1474560) x68k_match = true;
                break;
            case 0x11:
                /* 80*2*8*512 = 655,360 */
                if (x68k_data_size == 655360) x68k_match = true;
                break;
            case 0x19:
                /* 80*2*9*512 = 737,280 */
                if (x68k_data_size == 737280) x68k_match = true;
                break;
        }
        if (x68k_match) return false;
    }

    uint8_t cyl, heads, spt, density;
    if (!dim_atari_get_geometry(data, file_size, &cyl, &heads, &spt, &density))
        return false;

    /* Size match with valid geometry => good confidence */
    *confidence = 80;

    /* Standard geometries get higher confidence */
    if ((cyl == 80 && spt == 9 && density == 0) ||
        (cyl == 80 && spt == 10 && density == 0) ||
        (cyl == 80 && spt == 18 && density == 1)) {
        *confidence = 90;
    }

    return true;
}

static uft_error_t dim_atari_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[DIM_ATARI_HDR_SIZE];
    if (fread(hdr, 1, DIM_ATARI_HDR_SIZE, f) != DIM_ATARI_HDR_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    /* ID header 0x4242 — before anything else (MF-688).
     *
     * The probe checks this since MF-687, but `open()` did not, and that
     * is the path that matters for damage: it opens "r+b" when the caller
     * wants write access. Anyone selecting the plugin directly — explicit
     * format choice, a fuzzer, a caller that bypasses the registry — got a
     * writable handle on ANY file of a plausible length, and
     * `write_track()` then wrote sectors into it.
     *
     * Measured before the fix (tests/test_dim_atari_magic.c): a foreign
     * file with a valid-looking 32-byte header but no magic was opened
     * with rc=0 and came back modified. That is silent alteration of
     * someone else's data — Principle 1, not a matter of taste.
     *
     * Same three references as the probe: Hatari dim.c:75-76, HxC
     * dim_loader.c:74/:110, Jacknife dllmain.c:590. */
    if (hdr[0x00] != 0x42 || hdr[0x01] != 0x42) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads, spt, density;
    if (!dim_atari_get_geometry(hdr, (size_t)fs, &cyl, &heads, &spt, &density)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    dim_atari_pd_t *p = calloc(1, sizeof(dim_atari_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->cylinders = cyl;
    p->heads = heads;
    p->spt = spt;
    p->density = density;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = DIM_ATARI_SEC_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;
    return UFT_OK;
}

static void dim_atari_close(uft_disk_t *disk)
{
    dim_atari_pd_t *p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dim_atari_read_track(uft_disk_t *disk, int cyl, int head,
                                         uft_track_t *track)
{
    dim_atari_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->cylinders || head >= p->heads) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset = DIM_ATARI_HDR_SIZE +
                  (long)((uint32_t)cyl * p->heads + (uint32_t)head) *
                  p->spt * DIM_ATARI_SEC_SIZE;

    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[DIM_ATARI_SEC_SIZE];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, DIM_ATARI_SEC_SIZE, p->file) != DIM_ATARI_SEC_SIZE) {
            memset(buf, 0xE5, DIM_ATARI_SEC_SIZE);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, DIM_ATARI_SEC_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t dim_atari_write_track(uft_disk_t *disk, int cyl, int head,
                                          const uft_track_t *track)
{
    dim_atari_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl >= p->cylinders || head >= p->heads) return UFT_ERROR_INVALID_STATE;

    long offset = DIM_ATARI_HDR_SIZE +
                  (long)((uint32_t)cyl * p->heads + (uint32_t)head) *
                  p->spt * DIM_ATARI_SEC_SIZE;

    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, offset + (long)s * DIM_ATARI_SEC_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[DIM_ATARI_SEC_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, DIM_ATARI_SEC_SIZE);
            data = pad;
        }
        if (fwrite(data, 1, DIM_ATARI_SEC_SIZE, p->file) != DIM_ATARI_SEC_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_dim_atari_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dim_atari = {
    .name = "DIM_ATARI",
    .description = "Atari ST DIM (FastCopy Pro)",
    .extensions = "dim",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = dim_atari_probe,
    .open = dim_atari_open,
    .close = dim_atari_close,
    .read_track = dim_atari_read_track,
    .write_track = dim_atari_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dim_atari_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dim_atari_features) / sizeof(uft_format_plugin_dim_atari_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(dim_atari)
