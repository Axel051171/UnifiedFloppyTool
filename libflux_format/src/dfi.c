    #include "uft/uft_error.h"
#include "flux_format/dfi.h"
    #include <string.h>

    static bool probe_dfi(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "DFER";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_dfi(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_dfi(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_dfi_plugin = {
        .name  = "DFI",
        .ext   = "dfi",
        .caps  = 1u,
        .probe = probe_dfi,
        .read  = read_dfi,
        .write = write_dfi,
    };
