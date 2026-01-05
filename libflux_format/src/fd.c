#include "uft/uft_error.h"
#include "flux_format/fd.h"
#include <string.h>

static bool probe_fd(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_fd(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_fd(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -38 /*ENOSYS*/;
}

const fluxfmt_plugin_t fluxfmt_fd_plugin = {
    .name  = "FD",
    .ext   = "fd",
    .caps  = 3u,
    .probe = probe_fd,
    .read  = read_fd,
    .write = write_fd,
};
