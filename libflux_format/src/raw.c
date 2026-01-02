#include "uft/uft_error.h"
#include "flux_format/raw.h"
#include "flux_logical.h"
#include "fmt_util.h"

#include <errno.h>
#include <string.h>

static bool probe_raw(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    /* RAW/IMG has no magic; always a last-resort. */
    (void)buf; (void)len;
    return true;
}

static int read_raw(FILE *fp, ufm_disk_t *out)
{
    if (!fp || !out) return -22; /*EINVAL*/

    if (fseek(fp, 0, SEEK_END) != 0) return -errno;
    long szl = ftell(fp);
    if (szl < 0) return -errno;
    if (fseek(fp, 0, SEEK_SET) != 0) return -errno;

    const uint32_t sz = (uint32_t)szl;
    if (sz == 0 || (sz % 512u) != 0u) return -22;
    const uint32_t sectors = sz / 512u;

    int rc = fmt_ufm_alloc_geom(out, 1, 1);
    if (rc < 0) return rc;
    rc = ufm_disk_attach_logical(out);
    if (rc < 0) return rc;

    ufm_logical_image_t *li = (ufm_logical_image_t*)out->logical;
    li->cyls = 1;
    li->heads = 1;
    li->spt = (sectors > 0xFFFFu) ? 0 : (uint16_t)sectors;
    li->sector_size = 512;
    if (li->spt == 0) return -22;
    rc = ufm_logical_reserve(li, sectors);
    if (rc < 0) return rc;

    uint8_t buf[512];
    for (uint32_t i = 0; i < sectors; i++)
    {
        if (!fmt_read_exact(fp, buf, sizeof(buf))) return -5; /*EIO*/
        rc = ufm_logical_add_sector(li, 0, 0, (uint16_t)(i + 1u), buf, sizeof(buf), UFM_SEC_OK);
        if (rc < 0) return rc;
    }
    fmt_set_label(out, "RAW");
    return 0;
}

static int write_raw(FILE *fp, const ufm_disk_t *in)
{
    if (!fp || !in || !in->logical) return -22;
    const ufm_logical_image_t *li = (const ufm_logical_image_t*)in->logical;
    if (li->sector_size != 0 && li->sector_size != 512u) return -22;

    for (uint32_t i = 0; i < li->count; i++)
    {
        const ufm_sector_t *s = &li->sectors[i];
        if (!s->data || s->size != 512u) return -22;
        if (fwrite(s->data, 1, 512, fp) != 512) return -5;
    }
    return 0;
}

const fluxfmt_plugin_t fluxfmt_raw_plugin = {
    .name  = "RAW",
    .ext   = "raw",
    .caps  = FLUXFMT_CAN_READ | FLUXFMT_CAN_WRITE,
    .probe = probe_raw,
    .read  = read_raw,
    .write = write_raw,
};
