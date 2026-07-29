/**
 * @file uft_nfd_plugin.c
 * @brief NFD r0 (T98-Next PC-98) Plugin-B
 *
 * NFD is the disk image format for T98-Next (PC-98 emulator). This reader is
 * for the r0 variant, rewritten (MF-358) against the authoritative byte spec
 * (pc98.org/project/doc/nfdr0.html) cross-checked with a working reference
 * decoder (tomari/d88split nfd2mhlt.pl). The prior implementation was
 * fabricated: it modelled 164 per-TRACK entries with a per-entry data offset at
 * +12; the real format is a 163×26 per-SECTOR table with NO offset field and
 * data stored sequentially in table order — every field offset and the whole
 * data-location model were wrong (KNOWN_ISSUES FMT-11).
 *
 * r0 header (fixed 0x120 bytes):
 *   0x000  char   szFileID[15]  "T98FDDIMAGE.R0"
 *   0x00F  u8     reserve
 *   0x010  char   szComment[256]
 *   0x110  u32le  dwHeadSize     header size = data-section start offset
 *   0x114  u8     flProtect
 *   0x115  u8     byHead         number of heads
 *   0x116  u8     reserve[10]
 *   0x120  NFD_SECT_ID si[163][26]  sector-ID table, 16 bytes/entry
 *
 * r0 sector entry (16 bytes; only the first 11 are defined):
 *   +0 C  (0xFF = ignore this slot)   +1 H   +2 R   +3 N (128<<N bytes)
 *   +4 flMFM  +5 flDDAM  +6 byStatus  +7 ST0  +8 ST1  +9 ST2  +10 byPDA
 *
 * Sector data begins at dwHeadSize and is stored contiguously in the order the
 * valid (C!=0xFF) entries appear in the table — there is no per-sector offset.
 *
 * The r1 variant has a different layout (a 164-entry track-pointer table at
 * offset 0, then per-track headers); it is not handled here and returns an
 * explicit NOT_IMPLEMENTED rather than being mis-read as r0.
 */
#include "uft/uft_format_common.h"

#define NFD_SIG_BASE    "T98FDDIMAGE"
#define NFD_HEADER_SIZE 0x120   /* fixed r0 header incl. sector-ID table start */
#define NFD_OFF_HEADSZ  0x110   /* u32le dwHeadSize (data-section start) */
#define NFD_OFF_NHEAD   0x115   /* u8 number of heads */
#define NFD_TABLE_OFF   0x120   /* sector-ID table */
#define NFD_R0_TRACKS   163
#define NFD_R0_SECTORS  26
#define NFD_R0_ENTRY    16
#define NFD_R0_MAX_SECS (NFD_R0_TRACKS * NFD_R0_SECTORS)  /* 4238 */

/* One parsed, valid r0 sector, with its computed sequential data offset. */
typedef struct {
    uint8_t  c, h, r, n, mfm, ddam, status, st0, st1, st2, pda;
    size_t   data_off;   /* absolute file offset of this sector's data */
    uint16_t size;       /* 128 << N */
} nfd_sec_t;

typedef struct {
    uint8_t   *data;
    size_t     size;
    char      *path;      /* kept so write_track can persist in place */
    nfd_sec_t *secs;      /* parsed valid sectors, in table order */
    int        sec_count;
} nfd_pd_t;

/* Portable strdup (avoid relying on POSIX strdup availability). */
static char *nfd_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static uint16_t nfd_sec_size(uint8_t n)
{
    /* 128 << N, bounded so a corrupt N cannot request an absurd size. */
    if (n > 7) n = 7;                 /* 128<<7 = 16384 upper bound */
    return (uint16_t)(128u << n);
}

static bool nfd_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)file_size;
    if (size < 14) return false;
    /* "T98FDDIMAGE" + '.' + 'R' + '0'/'1' */
    if (memcmp(data, NFD_SIG_BASE, 11) != 0) return false;
    if (data[11] != '.' || data[12] != 'R') return false;
    if (data[13] != '0' && data[13] != '1') return false;
    *confidence = 95;
    return true;
}

static uft_error_t nfd_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < NFD_HEADER_SIZE) {
        free(data);
        return UFT_ERROR_FILE_OPEN;
    }
    if (memcmp(data, NFD_SIG_BASE, 11) != 0 || data[11] != '.' || data[12] != 'R') {
        free(data);
        return UFT_ERROR_FORMAT_INVALID;
    }
    if (data[13] != '0') {
        /* r1 (or unknown revision) has a different layout — refuse rather than
         * silently mis-read it as r0. */
        free(data);
        return UFT_ERROR_NOT_SUPPORTED;
    }

    uint32_t head_size = uft_read_le32(data + NFD_OFF_HEADSZ);
    if (head_size < NFD_HEADER_SIZE || head_size > file_size) {
        free(data);
        return UFT_ERROR_FORMAT_INVALID;
    }
    uint8_t nhead = data[NFD_OFF_NHEAD];

    nfd_sec_t *secs = calloc(NFD_R0_MAX_SECS, sizeof(nfd_sec_t));
    if (!secs) { free(data); return UFT_ERROR_NO_MEMORY; }

    /* Walk the 163×26 table; valid entries (C!=0xFF) get a sequential data
     * offset accumulated from dwHeadSize in table order. */
    int    count   = 0;
    size_t run_off = 0;
    uint8_t max_c = 0, max_h = 0;
    for (int slot = 0; slot < NFD_R0_MAX_SECS; slot++) {
        size_t eoff = NFD_TABLE_OFF + (size_t)slot * NFD_R0_ENTRY;
        if (eoff + NFD_R0_ENTRY > file_size) break;
        uint8_t c = data[eoff + 0];
        if (c == 0xFF) continue;               /* ignored slot */

        nfd_sec_t *s = &secs[count++];
        s->c      = c;
        s->h      = data[eoff + 1];
        s->r      = data[eoff + 2];
        s->n      = data[eoff + 3];
        s->mfm    = data[eoff + 4];
        s->ddam   = data[eoff + 5];
        s->status = data[eoff + 6];
        s->st0    = data[eoff + 7];
        s->st1    = data[eoff + 8];
        s->st2    = data[eoff + 9];
        s->pda    = data[eoff + 10];
        s->size   = nfd_sec_size(s->n);
        s->data_off = (size_t)head_size + run_off;
        run_off += s->size;
        if (s->c > max_c) max_c = s->c;
        if (s->h > max_h) max_h = s->h;
    }

    /* sectors-per-track = max count of entries sharing one (C,H). */
    int max_spt = 0;
    for (int i = 0; i < count; i++) {
        int n = 0;
        for (int j = 0; j < count; j++)
            if (secs[j].c == secs[i].c && secs[j].h == secs[i].h) n++;
        if (n > max_spt) max_spt = n;
    }

    nfd_pd_t *p = calloc(1, sizeof(nfd_pd_t));
    if (!p) { free(secs); free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->path = nfd_strdup(path);
    p->secs = secs;
    p->sec_count = count;

    disk->plugin_data = p;
    disk->geometry.cylinders = (uint32_t)max_c + 1;
    disk->geometry.heads = nhead > 0 ? nhead : (uint32_t)max_h + 1;
    disk->geometry.sectors = max_spt > 0 ? (uint32_t)max_spt : 8;
    disk->geometry.sector_size = count > 0 ? secs[0].size : 512;
    disk->geometry.total_sectors = (uint32_t)count;
    return UFT_OK;
}

static void nfd_close(uft_disk_t *disk)
{
    nfd_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->secs);
        free(p->data);
        free(p->path);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t nfd_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    nfd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    for (int i = 0; i < p->sec_count; i++) {
        nfd_sec_t *s = &p->secs[i];
        if ((int)s->c != cyl || (int)s->h != head) continue;

        /* add_sector stores id.sector = arg+1 (1-based), so pass R-1 to
         * preserve the real record number R (PC-98 records are 1-based). */
        uint8_t sec_arg = (uint8_t)(s->r ? s->r - 1 : 0);
        if (s->data_off + s->size > p->size) {
            /* Data truncated: represent the sector as a forensic fill rather
             * than dropping it or reading out of bounds. */
            uint8_t *fill = malloc(s->size);
            if (!fill) return UFT_ERROR_NO_MEMORY;
            memset(fill, 0xE5, s->size);
            uft_format_add_sector(track, sec_arg, fill, s->size,
                                  (uint8_t)cyl, (uint8_t)head);
            free(fill);
            if (track->sector_count > 0)
                uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
        } else {
            uft_format_add_sector(track, sec_arg, p->data + s->data_off, s->size,
                                  (uint8_t)cyl, (uint8_t)head);
        }

        if (track->sector_count > 0) {
            uft_sector_t *sec = &track->sectors[track->sector_count - 1];
            if (s->ddam) sec->deleted = true;
            /* uPD765 status: ST1 bit5 = CRC error in ID field, ST2 bit5 = CRC
             * error in data field. */
            if ((s->st1 & 0x20) || (s->st2 & 0x20))
                uft_sector_set_crc(sec, false);
        }
    }
    return UFT_OK;
}

static uft_error_t nfd_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    nfd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (!p->path) return UFT_ERROR_INVALID_STATE;

    FILE *f = fopen(p->path, "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* In-place: write each input sector back to its parsed data offset (the
     * data section is sequential, so the offset is stable). Matches by record
     * number R; sizes and offsets are bounds-checked. Both the in-memory buffer
     * and the file are updated so a subsequent read is consistent. */
    uft_error_t rc = UFT_OK;
    for (int i = 0; i < p->sec_count; i++) {
        nfd_sec_t *s = &p->secs[i];
        if ((int)s->c != cyl || (int)s->h != head) continue;
        if (s->data_off + s->size > p->size) continue;

        for (size_t ts = 0; ts < track->sector_count; ts++) {
            if (track->sectors[ts].id.sector == s->r) {
                const uint8_t *src = track->sectors[ts].data;
                if (src && track->sectors[ts].data_len >= s->size) {
                    memcpy(p->data + s->data_off, src, s->size);
                    if (fseek(f, (long)s->data_off, SEEK_SET) != 0 ||
                        fwrite(src, 1, s->size, f) != s->size) {
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

static const uft_plugin_feature_t uft_format_plugin_nfd_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_nfd = {
    .name = "NFD", .description = "T98-Next PC-98 (NFD)",
    .extensions = "nfd", .format = UFT_FORMAT_NFD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = nfd_probe, .open = nfd_open, .close = nfd_close,
    .read_track = nfd_read_track,
    .write_track = nfd_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_nfd_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_nfd_features) / sizeof(uft_format_plugin_nfd_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(nfd)
