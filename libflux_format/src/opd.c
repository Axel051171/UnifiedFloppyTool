#include "uft/uft_error.h"
#include "flux_format/opd.h"
#include <string.h>

static bool probe_opd(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_opd(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_opd(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_opd_plugin = {
    .name  = "OPD",
    .ext   = "opd",
    .caps  = 3u,
    .probe = probe_opd,
    .read  = read_opd,
    .write = write_opd,
};
