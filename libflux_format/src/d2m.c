    #include "uft/uft_error.h"
#include "flux_format/d2m.h"
    #include <string.h>

    static bool probe_d2m(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const uint8_t sig[] = { 0x01, 0x01 };
const size_t n = sizeof(sig);
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_d2m(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_d2m(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -38 /*ENOSYS*/;
    }

    const fluxfmt_plugin_t fluxfmt_d2m_plugin = {
        .name  = "D2M",
        .ext   = "d2m",
        .caps  = 3u,
        .probe = probe_d2m,
        .read  = read_d2m,
        .write = write_d2m,
    };
