#include "uft/uft_error.h"
#include "flux_format/dsc.h"
#include <string.h>

static bool probe_dsc(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_dsc(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_dsc(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_dsc_plugin = {
    .name  = "DSC",
    .ext   = "dsc",
    .caps  = 1u,
    .probe = probe_dsc,
    .read  = read_dsc,
    .write = write_dsc,
};
