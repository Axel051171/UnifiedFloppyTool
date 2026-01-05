#include "uft/uft_error.h"
#include "flux_format/mgt.h"
#include <string.h>

static bool probe_mgt(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_mgt(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_mgt(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_mgt_plugin = {
    .name  = "MGT",
    .ext   = "mgt",
    .caps  = 3u,
    .probe = probe_mgt,
    .read  = read_mgt,
    .write = write_mgt,
};
