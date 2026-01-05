    #include "uft/uft_error.h"
#include "flux_format/dti.h"
    #include <string.h>

    static bool probe_dti(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "H2G2";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_dti(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_dti(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -38 /*ENOSYS*/;
    }

    const fluxfmt_plugin_t fluxfmt_dti_plugin = {
        .name  = "DTI",
        .ext   = "dti",
        .caps  = 3u,
        .probe = probe_dti,
        .read  = read_dti,
        .write = write_dti,
    };
