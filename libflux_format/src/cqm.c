    #include "uft/uft_error.h"
#include "flux_format/cqm.h"
    #include <string.h>

    static bool probe_cqm(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const uint8_t sig[] = { 0x43, 0x51, 0x14 };
const size_t n = sizeof(sig);
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_cqm(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_cqm(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_cqm_plugin = {
        .name  = "CQM",
        .ext   = "cqm",
        .caps  = 1u,
        .probe = probe_cqm,
        .read  = read_cqm,
        .write = write_cqm,
    };
