    #include "uft/uft_error.h"
#include "flux_format/a2r.h"
    #include <string.h>

    static bool probe_a2r(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "A2R2";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_a2r(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_a2r(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_a2r_plugin = {
        .name  = "A2R",
        .ext   = "a2r",
        .caps  = 1u,
        .probe = probe_a2r,
        .read  = read_a2r,
        .write = write_a2r,
    };
