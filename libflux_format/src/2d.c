#include "uft/uft_error.h"
#include "flux_format/2d.h"
#include <string.h>

static bool probe_2d(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_2d(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_2d(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_2d_plugin = {
    .name  = "2D",
    .ext   = "2d",
    .caps  = 1u,
    .probe = probe_2d,
    .read  = read_2d,
    .write = write_2d,
};
