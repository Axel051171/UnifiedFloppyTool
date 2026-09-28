/* FDC profiles: the data rate, the rotation speed and the track length
 * describe the same track — rate x 60 / rpm / 8 bytes (FM: half of it).
 *
 * P3-644 (MF-1480): UFT_FDC_PC_360K said 300 kbit/s at 300 rpm with 6250
 * bytes per track; 300 000 x 60 / 300 / 8 = 7500. 300 kbit/s is the rate
 * of a 360K disk in a 1.2M drive turning at 360 rpm; at 300 rpm (the
 * 360K drive, which the profile's rpm and 6250 describe) it is 250. The
 * profile's own named source says so: greaseweazle 26690f89 (Unlicense),
 * data/diskdefs_ibm.cfg, `disk 360`: `rate = 250` (default rpm 300).
 *
 * One calculation, called for every profile the header lists, so a new
 * profile cannot start out inconsistent (MF-1177: the identity belongs in
 * one place, the test only calls it).
 */
#include "uft/formats/uft_fdc_gaps.h"

#include <stdio.h>

static int gruen = 0, rot = 0;

static unsigned rate_bits(uft_fdc_rate_t r)
{
    switch (r) {
    case UFT_FDC_RATE_500K: return 500000u;
    case UFT_FDC_RATE_300K: return 300000u;
    case UFT_FDC_RATE_250K: return 250000u;
    case UFT_FDC_RATE_1M:   return 1000000u;
    }
    return 0u;
}

static void pruefe(const uft_fdc_format_t *f)
{
    unsigned bytes = 0;
    if (f->rpm)
        bytes = rate_bits(f->data_rate) * 60u / (unsigned)f->rpm / 8u / (f->mfm ? 1u : 2u);
    int ok = bytes == (unsigned)f->track_bytes;
    printf("  [%s] %-28s rate %7u, %3d U/min, %s -> %5u Byte, Profil %5d\n",
           ok ? "ok " : "ROT", f->name, rate_bits(f->data_rate), f->rpm,
           f->mfm ? "MFM" : "FM ", bytes, f->track_bytes);
    if (ok) gruen++; else rot++;
}

int main(void)
{
    printf("FDC-Profile: Rate, Drehzahl und Spurlaenge beschreiben dieselbe Spur (MF-1480)\n");
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) pruefe(UFT_FDC_FORMATS[i]);
    pruefe(&UFT_FDC_CBM_1581);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
