#include "uft/uft_error.h"
#include "flux_format/stream.h"
#include <string.h>

static bool probe_stream(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_stream(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_stream(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_stream_plugin = {
    .name  = "STREAM",
    .ext   = "stream",
    .caps  = 1u,
    .probe = probe_stream,
    .read  = read_stream,
    .write = write_stream,
};
