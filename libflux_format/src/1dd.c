#include "uft/uft_error.h"
#include "flux_format/1dd.h"
#include <string.h>

static bool probe_1dd(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_1dd(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_1dd(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_1dd_plugin = {
    .name  = "1DD",
    .ext   = "1dd",
    .caps  = 3u,
    .probe = probe_1dd,
    .read  = read_1dd,
    .write = write_1dd,
};
