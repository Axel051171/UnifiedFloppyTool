#include "uft/uft_error.h"
#include "flux_format/lif.h"
#include <string.h>

static bool probe_lif(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_lif(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_lif(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_lif_plugin = {
    .name  = "LIF",
    .ext   = "lif",
    .caps  = 3u,
    .probe = probe_lif,
    .read  = read_lif,
    .write = write_lif,
};
