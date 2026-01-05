#include "uft/uft_error.h"
#include "flux_format/s24.h"
#include <string.h>

static bool probe_s24(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_s24(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_s24(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_s24_plugin = {
    .name  = "S24",
    .ext   = "s24",
    .caps  = 1u,
    .probe = probe_s24,
    .read  = read_s24,
    .write = write_s24,
};
