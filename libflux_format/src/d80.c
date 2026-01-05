    #include "uft/uft_error.h"
#include "flux_format/d80.h"
    #include <string.h>

    static bool probe_d80(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "SDOS";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_d80(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_d80(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_d80_plugin = {
        .name  = "D80",
        .ext   = "d80",
        .caps  = 1u,
        .probe = probe_d80,
        .read  = read_d80,
        .write = write_d80,
    };
