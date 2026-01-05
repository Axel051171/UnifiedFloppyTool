#include "uft/uft_error.h"
#include "flux_format/mbd.h"
#include <string.h>

static bool probe_mbd(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_mbd(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_mbd(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_mbd_plugin = {
    .name  = "MBD",
    .ext   = "mbd",
    .caps  = 3u,
    .probe = probe_mbd,
    .read  = read_mbd,
    .write = write_mbd,
};
