#include "uft/uft_error.h"
#include "flux_format/pdi.h"
#include <string.h>

static bool probe_pdi(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_pdi(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_pdi(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_pdi_plugin = {
    .name  = "PDI",
    .ext   = "pdi",
    .caps  = 1u,
    .probe = probe_pdi,
    .read  = read_pdi,
    .write = write_pdi,
};
