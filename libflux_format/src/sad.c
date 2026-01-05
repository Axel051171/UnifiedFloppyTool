    #include "uft/uft_error.h"
#include "flux_format/sad.h"
    #include <string.h>

    static bool probe_sad(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "Aley's disk backup";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_sad(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_sad(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -38 /*ENOSYS*/;
    }

    const fluxfmt_plugin_t fluxfmt_sad_plugin = {
        .name  = "SAD",
        .ext   = "sad",
        .caps  = 3u,
        .probe = probe_sad,
        .read  = read_sad,
        .write = write_sad,
    };
