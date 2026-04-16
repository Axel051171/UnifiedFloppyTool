/**
 * @file uft_dim_atari.c
 * @brief Atari ST DIM (FastCopy Pro) Plugin-B
 *
 * DIM is the FastCopy Pro disk image format for the Atari ST.
 * 32-byte header followed by raw sector data.
 *
 * Header layout:
 *   Offset  Size  Description
 *   0x00    1     Flags (unused, often 0)
 *   0x01    2     Reserved
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

    /* Reject if file looks like X68000 DIM (256-byte header, known media type) */
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

const uft_format_plugin_t uft_format_plugin_dim_atari = {
    .name = "DIM_ATARI",
    .description = "Atari ST DIM (FastCopy Pro)",
    .extensions = "dim",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = dim_atari_probe,
    .open = dim_atari_open,
    .close = dim_atari_close,
    .read_track = dim_atari_read_track,
    .write_track = dim_atari_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(dim_atari)
