#include "uft/uft_error.h"
#include "flux_format/d88.h"
#include <string.h>

static bool probe_d88(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_d88(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_d88(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_d88_plugin = {
    .name  = "D88",
    .ext   = "d88",
    .caps  = 3u,
    .probe = probe_d88,
    .read  = read_d88,
    .write = write_d88,
};
