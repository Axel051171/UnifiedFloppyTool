    #include "uft/uft_error.h"
#include "flux_format/cwtool.h"
    #include <string.h>

    static bool probe_cwtool(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "cwtool raw data";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_cwtool(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_cwtool(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_cwtool_plugin = {
        .name  = "CWTOOL",
        .ext   = "cwtool",
        .caps  = 1u,
        .probe = probe_cwtool,
        .read  = read_cwtool,
        .write = write_cwtool,
    };
