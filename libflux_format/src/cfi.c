#include "uft/uft_error.h"
#include "flux_format/cfi.h"
#include <string.h>

static bool probe_cfi(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_cfi(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_cfi(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_cfi_plugin = {
    .name  = "CFI",
    .ext   = "cfi",
    .caps  = 1u,
    .probe = probe_cfi,
    .read  = read_cfi,
    .write = write_cfi,
};
