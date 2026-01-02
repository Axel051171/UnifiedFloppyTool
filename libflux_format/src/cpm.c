#include "uft/uft_error.h"
#include "flux_format/cpm.h"
#include <string.h>

static bool probe_cpm(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_cpm(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_cpm(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_cpm_plugin = {
    .name  = "CPM",
    .ext   = "cpm",
    .caps  = 3u,
    .probe = probe_cpm,
    .read  = read_cpm,
    .write = write_cpm,
};
