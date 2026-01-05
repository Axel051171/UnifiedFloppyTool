#include "uft/uft_error.h"
#include "flux_format/adf.h"
#include "flux_logical.h"
#include "fmt_util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static bool probe_adf(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    /* ADF has no strong magic. We only do a weak heuristic on bootblock.
     * AmigaDOS bootblock begins with "DOS" and a checksum, but it may be
     * non-DOS or protected. So: accept if it looks plausible.
     */
    if (len < 4) return false;
    if (buf[0] == 'D' && buf[1] == 'O' && buf[2] == 'S') return true;
    return true; /* weak probe */
}

typedef struct adf_geom_guess {
    uint32_t bytes;
    uint16_t cyls;
    uint16_t heads;
    uint16_t spt;
    uint16_t ssize;
} adf_geom_guess_t;

static bool adf_guess_geom(uint32_t bytes, adf_geom_guess_t *g)
{
    if (!g) return false;
    /* Common ADF sizes.
     * DD: 80c*2h*11s*512 = 901120
     * DD 720K: 80*2*9*512 = 737280
     * HD: 80*2*22*512 = 1802240
     */
    static const adf_geom_guess_t tbl[] = {
        { 901120u, 80, 2, 11, 512 },
        { 737280u, 80, 2,  9, 512 },
        {1802240u, 80, 2, 22, 512 },
    };
    for (size_t i = 0; i < sizeof(tbl)/sizeof(tbl[0]); i++)
    {
        if (tbl[i].bytes == bytes)
        {
            *g = tbl[i];
            return true;
        }
    }
    return false;
}

static int read_adf(FILE *fp, ufm_disk_t *out)
{
    if (!fp || !out) return -EINVAL;

    if (fseek(fp, 0, SEEK_END) != 0) return -errno;
    long szl = ftell(fp);
    if (szl < 0) return -errno;
    if (fseek(fp, 0, SEEK_SET) != 0) return -errno;

    const uint32_t sz = (uint32_t)szl;
    if ((sz % 512u) != 0u) return -EINVAL;

    adf_geom_guess_t g;
    if (!adf_guess_geom(sz, &g))
    {
        /* Unknown ADF variant. Still accept: assume 80*2 and infer spt. */
        g.cyls = 80;
        g.heads = 2;
        g.ssize = 512;
        uint32_t sectors = sz / 512u;
        uint32_t tracks = (uint32_t)g.cyls * (uint32_t)g.heads;
        if (tracks == 0) return -EINVAL;
        g.spt = (uint16_t)(sectors / tracks);
        if ((uint32_t)g.spt * tracks != sectors || g.spt == 0) return -EINVAL;
    }

    int rc = fmt_ufm_alloc_geom(out, g.cyls, g.heads);
    if (rc < 0) return rc;
    rc = ufm_disk_attach_logical(out);
    if (rc < 0) return rc;

    ufm_logical_image_t *li = (ufm_logical_image_t*)out->logical;
    li->cyls = g.cyls;
    li->heads = g.heads;
    li->spt = g.spt;
    li->sector_size = g.ssize;
    if ((rc = ufm_logical_reserve(li, (uint32_t)g.cyls * (uint32_t)g.heads * (uint32_t)g.spt)) < 0)
        return rc;

    uint8_t buf[512];
    for (uint16_t cyl = 0; cyl < g.cyls; cyl++)
    {
        for (uint16_t head = 0; head < g.heads; head++)
        {
            for (uint16_t sec = 1; sec <= g.spt; sec++)
            {
                if (!fmt_read_exact(fp, buf, sizeof(buf))) return -EIO;
                rc = ufm_logical_add_sector(li, cyl, head, sec, buf, sizeof(buf), UFM_SEC_OK);
                if (rc < 0) return rc;
            }
        }
    }

    fmt_set_label(out, "ADF");
    return 0;
}

static int write_adf(FILE *fp, const ufm_disk_t *in)
{
    if (!fp || !in) return -EINVAL;
    if (!in->logical) return -EINVAL;
    const ufm_logical_image_t *li = (const ufm_logical_image_t*)in->logical;

    if (li->sector_size != 0 && li->sector_size != 512u) return -EINVAL;
    const uint16_t cyls = li->cyls ? li->cyls : in->cyls;
    const uint16_t heads = li->heads ? li->heads : in->heads;
    const uint16_t spt = li->spt;
    if (cyls == 0 || heads == 0 || spt == 0) return -EINVAL;

    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));
    for (uint16_t cyl = 0; cyl < cyls; cyl++)
    {
        for (uint16_t head = 0; head < heads; head++)
        {
            for (uint16_t sec = 1; sec <= spt; sec++)
            {
                const ufm_sector_t *s = ufm_logical_find_const(li, cyl, head, sec);
                if (!s)
                {
                    if (fwrite(zero, 1, sizeof(zero), fp) != sizeof(zero)) return -EIO;
                    continue;
                }
                if (s->size != 512u || !s->data) return -EINVAL;
                if (fwrite(s->data, 1, 512, fp) != 512) return -EIO;
            }
        }
    }
    return 0;
}

const fluxfmt_plugin_t fluxfmt_adf_plugin = {
    .name  = "ADF",
    .ext   = "adf",
    .caps  = FLUXFMT_CAN_READ | FLUXFMT_CAN_WRITE,
    .probe = probe_adf,
    .read  = read_adf,
    .write = write_adf,
};
