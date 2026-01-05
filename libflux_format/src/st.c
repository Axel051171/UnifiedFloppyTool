#include "uft/uft_error.h"
#include "flux_format/st.h"
#include <string.h>

static bool probe_st(const uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return false;
    return false;
}

static int read_st(FILE *fp, ufm_disk_t *out)
{
    (void)fp; (void)out; return -38 /*ENOSYS*/;
}

static int write_st(FILE *fp, const ufm_disk_t *in)
{
    (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
}

const fluxfmt_plugin_t fluxfmt_st_plugin = {
    .name  = "ST",
    .ext   = "st",
    .caps  = 1u,
    .probe = probe_st,
    .read  = read_st,
    .write = write_st,
};
