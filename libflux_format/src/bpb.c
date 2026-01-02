#include "uft/uft_error.h"
#include "flux_format/bpb.h"
#include <string.h>

static bool probe_bpb(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_bpb(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_bpb(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_bpb_plugin = {
    .name  = "BPB",
    .ext   = "bpb",
    .caps  = 1u,
    .probe = probe_bpb,
    .read  = read_bpb,
    .write = write_bpb,
};
