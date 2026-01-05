#include "uft/uft_error.h"
#include "flux_format/do.h"
#include <string.h>

static bool probe_do(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_do(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -95 /*EOPNOTSUPP*/;
}

static int write_do(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_do_plugin = {
    .name  = "DO",
    .ext   = "do",
    .caps  = 3u,
    .probe = probe_do,
    .read  = read_do,
    .write = write_do,
};
