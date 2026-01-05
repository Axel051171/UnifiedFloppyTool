    #include "uft/uft_error.h"
#include "flux_format/sbt.h"
    #include <string.h>

    static bool probe_sbt(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "BOOT";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_sbt(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_sbt(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -95 /*EOPNOTSUPP*/;
    }

    const fluxfmt_plugin_t fluxfmt_sbt_plugin = {
        .name  = "SBT",
        .ext   = "sbt",
        .caps  = 1u,
        .probe = probe_sbt,
        .read  = read_sbt,
        .write = write_sbt,
    };
