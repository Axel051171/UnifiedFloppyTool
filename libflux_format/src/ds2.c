#include "uft/uft_error.h"
#include "flux_format/ds2.h"
#include <string.h>

static bool probe_ds2(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_ds2(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_ds2(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_ds2_plugin = {
    .name  = "DS2",
    .ext   = "ds2",
    .caps  = 1u,
    .probe = probe_ds2,
    .read  = read_ds2,
    .write = write_ds2,
};
