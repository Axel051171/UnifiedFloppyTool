    #include "uft/uft_error.h"
#include "flux_format/mfi.h"
    #include <string.h>

    static bool probe_mfi(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "MESSFLOPPYIMAGE";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_mfi(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_mfi(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -38 /*ENOSYS*/;
    }

    const fluxfmt_plugin_t fluxfmt_mfi_plugin = {
        .name  = "MFI",
        .ext   = "mfi",
        .caps  = 3u,
        .probe = probe_mfi,
        .read  = read_mfi,
        .write = write_mfi,
    };
