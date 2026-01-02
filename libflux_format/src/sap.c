    #include "uft/uft_error.h"
#include "flux_format/sap.h"
    #include <string.h>

    static bool probe_sap(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        static const char sig[] = "SYSTEME D'ARCHIVAGE PUKALL S.A.P. (c) Alexandre PUKALL Avril 1998";
const size_t n = sizeof(sig) - 1;
if (len < n) return false;
return memcmp(buf, sig, n) == 0;
    }

    static int read_sap(FILE *fp, ufm_disk_t *out)
    {
        (void)fp; (void)out; return -38 /*ENOSYS*/;
    }

    static int write_sap(FILE *fp, const ufm_disk_t *in)
    {
        (void)fp; (void)in; return -38 /*ENOSYS*/;
    }

    const fluxfmt_plugin_t fluxfmt_sap_plugin = {
        .name  = "SAP",
        .ext   = "sap",
        .caps  = 3u,
        .probe = probe_sap,
        .read  = read_sap,
        .write = write_sap,
    };
