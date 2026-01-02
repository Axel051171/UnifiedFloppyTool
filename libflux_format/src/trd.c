#include "uft/uft_error.h"
#include "flux_format/trd.h"
#include <string.h>

static bool probe_trd(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_trd(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_trd(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_trd_plugin = {
    .name  = "TRD",
    .ext   = "trd",
    .caps  = 1u,
    .probe = probe_trd,
    .read  = read_trd,
    .write = write_trd,
};
