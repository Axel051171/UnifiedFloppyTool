    #include "uft/uft_error.h"
#include "flux_format/scl.h"
    #include <string.h>

    static bool probe_scl(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "SINCLAIR";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_scl(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_scl(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_scl_plugin = {
        .name  = "SCL",
        .ext   = "scl",
        .caps  = 1u,
        .probe = probe_scl,
        .read  = read_scl,
        .write = write_scl,
    };
