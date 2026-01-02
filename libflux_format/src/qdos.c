    #include "uft/uft_error.h"
#include "flux_format/qdos.h"
    #include <string.h>

    static bool probe_qdos(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "QL5A";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_qdos(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_qdos(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -38 /*ENOSYS*/;
    }

    const fluxfmt_plugin_t fluxfmt_qdos_plugin = {
        .name  = "QDOS",
        .ext   = "qdos",
        .caps  = 3u,
        .probe = probe_qdos,
        .read  = read_qdos,
        .write = write_qdos,
    };
