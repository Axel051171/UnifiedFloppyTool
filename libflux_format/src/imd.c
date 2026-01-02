#include "uft/uft_error.h"
#include "flux_format/imd.h"
#include "fmt_util.h"

#include "flux_core.h"
#include "flux_logical.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

static bool probe_imd(const uint8_t *buf, size_t len)
{
    if (!buf || len < 4) return false;
    return memcmp(buf, "IMD ", 4) == 0;
}

static bool read_u16_le_file(FILE *fp, uint16_t *out)
{
    uint8_t b[2];
    if (!fmt_read_exact(fp, b, 2)) return false;
    *out = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    return true;
}

static uint32_t secsize_from_n(uint8_t n)
{
    if (n > 7) return 0;
    return (uint32_t)128u << n;
}

/* IMD Track mode values (ImageDisk):
 * 0=500kbps FM, 1=300kbps FM, 2=250kbps FM,
 * 3=500kbps MFM,4=300kbps MFM,5=250kbps MFM.
 */
static int imd_infer_mode_mfm(const ufm_disk_t *d)
{
    if (!d || !d->logical) return 5; /* default DD */
    const ufm_logical_image_t *li = d->logical;
    /* crude but practical: PC/HD images tend to be 18x512 */
    if (li->spt >= 18 && li->sector_size == 512) return 3;
    return 5;
}

/* IMD type byte: 0=no data,
 * 1=normal,2=compressed,3=deleted,4=del+compressed,5=bad,6=bad+compressed.
 */
static void imd_type_to_flags(uint8_t type_minus1, ufm_sec_flags_t *flags, bool *compressed)
{
    *compressed = (type_minus1 & 1u) != 0u;
    *flags = UFM_SEC_OK;
    switch (type_minus1) {
        case 2: /* deleted */
        case 3: /* deleted + compressed */
            *flags = (ufm_sec_flags_t)(*flags | UFM_SEC_DELETED_DAM);
            break;
        case 4: /* bad */
        case 5: /* bad + compressed */
            *flags = (ufm_sec_flags_t)(*flags | UFM_SEC_BAD_CRC);
            break;
        default:
            break;
    }
}

static int read_imd(FILE *fp, ufm_disk_t *out)
{
    if (!fp || !out) return -EINVAL;

    /* Verify signature */
    uint8_t prefix[4];
    if (fseek(fp, 0, SEEK_SET) != 0) return -EIO;
    if (!fmt_read_exact(fp, prefix, sizeof(prefix))) return -EIO;
    if (memcmp(prefix, "IMD ", 4) != 0) return -EINVAL;

    /* Skip ASCII comment until 0x1A */
    if (fseek(fp, 0, SEEK_SET) != 0) return -EIO;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 0x1A) break;
    }
    if (ch != 0x1A) return -EINVAL;

    ufm_disk_init(out);
    fmt_set_label(out, "IMD");
    if (ufm_disk_attach_logical(out) != 0) return -ENOMEM;

    uint16_t max_cyl = 0, max_head = 0;

    for (;;) {
        uint8_t th[5];
        size_t n = fread(th, 1, sizeof(th), fp);
        if (n == 0) break; /* EOF */
        if (n != sizeof(th)) return -EIO;

        const uint8_t mode    = th[0];
        const uint8_t cyl     = th[1];
        const uint8_t headraw = th[2];
        const uint8_t secs    = th[3];
        const uint8_t size_n  = th[4];

        (void)mode;

        if (secs == 0 || secs > 64) return -EINVAL;
        uint8_t head = (uint8_t)(headraw & 1u);

        if ((uint16_t)cyl > max_cyl) max_cyl = cyl;
        if ((uint16_t)head > max_head) max_head = head;

        uint8_t *rmap = (uint8_t*)malloc(secs);
        uint8_t *cmap = NULL;
        uint8_t *hmap = NULL;
        uint16_t *nmap = NULL;
        if (!rmap) return -ENOMEM;

        if (!fmt_read_exact(fp, rmap, secs)) { free(rmap); return -EIO; }

        if (headraw & 0x80u) {
            cmap = (uint8_t*)malloc(secs);
            if (!cmap) { free(rmap); return -ENOMEM; }
            if (!fmt_read_exact(fp, cmap, secs)) { free(rmap); free(cmap); return -EIO; }
        }
        if (headraw & 0x40u) {
            hmap = (uint8_t*)malloc(secs);
            if (!hmap) { free(rmap); free(cmap); return -ENOMEM; }
            if (!fmt_read_exact(fp, hmap, secs)) { free(rmap); free(cmap); free(hmap); return -EIO; }
        }
        if (size_n == 0xFFu) {
            nmap = (uint16_t*)calloc(secs, sizeof(uint16_t));
            if (!nmap) { free(rmap); free(cmap); free(hmap); return -ENOMEM; }
            for (uint8_t i = 0; i < secs; ++i) {
                uint16_t v;
                if (!read_u16_le_file(fp, &v)) { free(rmap); free(cmap); free(hmap); free(nmap); return -EIO; }
                nmap[i] = v;
            }
        }

        for (uint8_t i = 0; i < secs; ++i) {
            int t = fgetc(fp);
            if (t == EOF) { free(rmap); free(cmap); free(hmap); free(nmap); return -EIO; }
            if (t == 0) continue; /* no data */

            const uint8_t type_minus1 = (uint8_t)(t - 1);
            bool compressed = false;
            ufm_sec_flags_t flags;
            imd_type_to_flags(type_minus1, &flags, &compressed);

            const uint16_t scyl = cmap ? cmap[i] : cyl;
            const uint16_t shd  = hmap ? hmap[i] : head;
            const uint16_t ssec = rmap[i];

            uint32_t sec_len;
            if (nmap) {
                sec_len = nmap[i];
            } else {
                sec_len = secsize_from_n(size_n & 7u);
            }
            if (sec_len == 0) { free(rmap); free(cmap); free(hmap); free(nmap); return -EINVAL; }

            uint8_t *data = (uint8_t*)malloc(sec_len);
            if (!data) { free(rmap); free(cmap); free(hmap); free(nmap); return -ENOMEM; }

            if (compressed) {
                int fill = fgetc(fp);
                if (fill == EOF) { free(data); free(rmap); free(cmap); free(hmap); free(nmap); return -EIO; }
                memset(data, (uint8_t)fill, sec_len);
            } else {
                if (!fmt_read_exact(fp, data, sec_len)) { free(data); free(rmap); free(cmap); free(hmap); free(nmap); return -EIO; }
            }

            ufm_sector_t *s = ufm_logical_add_sector_ref(out->logical, scyl, shd, ssec, data, sec_len, flags);
            free(data);
            if (!s) { free(rmap); free(cmap); free(hmap); free(nmap); return -ENOMEM; }
        }

        free(rmap); free(cmap); free(hmap); free(nmap);
    }

    out->cyls = (uint16_t)(max_cyl + 1u);
    out->heads = (uint16_t)(max_head + 1u);
    if (out->logical) {
        out->logical->cyls = out->cyls;
        out->logical->heads = out->heads;
        /* best-effort: infer constant sector size/spt from first track */
        if (out->logical->count) {
            out->logical->sector_size = out->logical->sectors[0].size;
        }
    }

    return 0;
}

static int write_imd(FILE *fp, const ufm_disk_t *in)
{
    if (!fp || !in || !in->logical) return -EINVAL;
    const ufm_logical_image_t *li = in->logical;
    if (li->cyls == 0 || li->heads == 0 || li->sector_size == 0 || li->spt == 0) return -EINVAL;

    /* Header + comment */
    const char *comment = "IMD 1.18: flux_preservation_architect\n";
    if (fseek(fp, 0, SEEK_SET) != 0) return -EIO;
    if (fwrite(comment, 1, strlen(comment), fp) != strlen(comment)) return -EIO;
    fputc(0x1A, fp);

    const int mode = imd_infer_mode_mfm(in);
    const uint8_t size_n = (uint8_t)(li->sector_size == 128 ? 0 :
                                    li->sector_size == 256 ? 1 :
                                    li->sector_size == 512 ? 2 :
                                    li->sector_size == 1024 ? 3 :
                                    li->sector_size == 2048 ? 4 :
                                    li->sector_size == 4096 ? 5 : 2);

    for (uint16_t c = 0; c < li->cyls; ++c) {
        for (uint16_t h = 0; h < li->heads; ++h) {
            uint8_t th[5];
            th[0] = (uint8_t)mode;
            th[1] = (uint8_t)c;
            th[2] = (uint8_t)h;
            th[3] = (uint8_t)li->spt;
            th[4] = size_n;
            if (fwrite(th, 1, sizeof(th), fp) != sizeof(th)) return -EIO;

            /* rmap: 1..spt */
            for (uint16_t r = 1; r <= li->spt; ++r) {
                fputc((int)r, fp);
            }

            for (uint16_t r = 1; r <= li->spt; ++r) {
                const ufm_sector_t *s = ufm_logical_find_const(li, c, h, r);
                uint8_t type = 1; /* normal */
                if (!s) {
                    fputc(0, fp);
                    continue;
                }
                if (s->flags & UFM_SEC_DELETED_DAM) type = 3;
                if (s->flags & UFM_SEC_BAD_CRC) type = 5;
                fputc(type, fp);
                const uint8_t *data = (s->data && s->size == li->sector_size) ? s->data : NULL;
                if (!data) {
                    /* deterministic zero-fill */
                    for (uint32_t i = 0; i < li->sector_size; ++i) fputc(0, fp);
                } else {
                    if (fwrite(data, 1, li->sector_size, fp) != li->sector_size) return -EIO;
                }
            }
        }
    }

    return 0;
}

const fluxfmt_plugin_t fluxfmt_imd_plugin = {
    .name  = "IMD",
    .ext   = "imd",
    .caps  = 3u,
    .probe = probe_imd,
    .read  = read_imd,
    .write = write_imd,
};
