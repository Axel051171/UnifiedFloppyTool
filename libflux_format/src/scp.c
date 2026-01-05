#include "uft/uft_error.h"
#include "flux_format/scp.h"
#include "fmt_util.h"

#include <string.h>
#include <errno.h>

static bool probe_scp(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    if (len < 3) return false;
    return (buf[0] == 'S' && buf[1] == 'C' && buf[2] == 'P');
}

static int read_scp(FILE *fp, ufm_disk_t *out)
{
    if (!fp || !out) return -EINVAL;

    /* Minimal SCP header parse (SuperCard Pro flux container).
     * Full flux extraction lands in a later sprint; for now we capture geometry + meta.
     */
    uint8_t h[16];
    if (fseek(fp, 0, SEEK_SET) != 0) return -EIO;
    if (!fmt_read_exact(fp, h, sizeof(h))) return -EIO;
    if (!(h[0] == 'S' && h[1] == 'C' && h[2] == 'P')) return -EINVAL;

    uint8_t revolutions = h[5];
    uint8_t start_track = h[6];
    uint8_t end_track   = h[7];
    uint8_t flags       = h[8];
    uint8_t heads_mode  = h[10];

    if (revolutions == 0 || revolutions > 10) return -EINVAL;
    if (end_track < start_track) return -EINVAL;
    if (heads_mode > 2) return -EINVAL;
    if (flags & (1u << 6)) return -ENOTSUP; /* extended/non-floppy */

    uint16_t heads = (heads_mode == 0) ? 2u : 1u;
    /* Track numbering is usually interleaved (cyl*2+head). */
    uint16_t cyls  = (uint16_t)((end_track / 2u) + 1u);

    ufm_disk_init(out);
    fmt_set_label(out, "SCP");
    int rc = fmt_ufm_alloc_geom(out, cyls, heads);
    if (rc < 0) return rc;

    out->hw.type = UFM_HW_SCP;
    out->hw.sample_clock_hz = 40000000u; /* 25ns ticks */
    out->retry.read_attempts = 0;
    out->retry.read_success  = 0;
    (void)revolutions;
    return 0;
}

static int write_scp(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_scp_plugin = {
    .name  = "SCP",
    .ext   = "scp",
    .caps  = 1u,
    .probe = probe_scp,
    .read  = read_scp,
    .write = write_scp,
};
