/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_ufi_mountinfo.c
 * @brief A mounted USB floppy — or a mounted partition of it — is refused
 *        before UFT writes to it (P3-669, MF-1516). Runs on every platform:
 *        the check is a pure function over the TEXT of mountinfo.
 *
 * REFERENCE: proc_pid_mountinfo(5) — field (3) "major:minor: the value of
 * st_dev for files on this filesystem", field (5) the mount point, "the end
 * of the optional fields is marked by a single hyphen". The first case is
 * the page's own example line.
 *
 * What is NOT measured here and cannot be without hardware: the kernel's
 * O_EXCL refusal (open(2): "If the block device is in use by the system
 * (e.g., mounted), open() fails with the error EBUSY") on a real device.
 */
#include "uft/hal/ufi_mountinfo.h"

#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <sys/stat.h>
#include <sys/sysmacros.h>
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

/* Partition table of a fake machine: sdb = 8:16 with sdb1 = 8:17; sdc =
 * 8:32; sdk = 8:160 (a whole disk whose number starts like sdb's). */
static bool eltern(unsigned maj, unsigned min, unsigned *pmaj, unsigned *pmin, void *ctx)
{
    (void)ctx;
    if (maj == 8 && min == 17) { *pmaj = 8; *pmin = 16; return true; }
    return false;
}

static const char *BEISPIEL =
    "36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue\n";

static const char *MASCHINE =
    "22 1 8:2 / / rw,relatime shared:1 - ext4 /dev/sda2 rw\n"
    "40 22 8:17 / /media/axel/FLOPPY rw,nosuid,nodev shared:30 master:7 - vfat /dev/sdb1 rw,uid=1000\n"
    "41 22 8:160 / /media/axel/BIG\\040DISK rw - ext4 /dev/sdk rw\n"
    "kaputte zeile ohne felder\n"
    "42 22 0:45 / /proc rw - proc proc rw\n";

int main(void)
{
    char p[128];
    printf("UFI: eingehaengtes Laufwerk wird erkannt (MF-1516)\n");

    p[0] = 0;
    pruefe("proc(5)-Beispielzeile: 98:0 ist eingehaengt, auf /mnt2",
           uft_ufi_mountinfo_belegt(BEISPIEL, 98, 0, NULL, NULL, p, sizeof p) &&
           strcmp(p, "/mnt2") == 0, p);
    pruefe("proc(5)-Beispielzeile: 98:1 ist es nicht",
           !uft_ufi_mountinfo_belegt(BEISPIEL, 98, 1, NULL, NULL, NULL, 0), NULL);

    p[0] = 0;
    pruefe("sdb (8:16) selbst nicht eingehaengt, aber sdb1 (8:17): belegt",
           uft_ufi_mountinfo_belegt(MASCHINE, 8, 16, eltern, NULL, p, sizeof p) &&
           strcmp(p, "/media/axel/FLOPPY") == 0, p);
    pruefe("ohne Partitionsaufloesung zaehlt nur die genaue Nummer",
           !uft_ufi_mountinfo_belegt(MASCHINE, 8, 16, NULL, NULL, NULL, 0), NULL);
    pruefe("8:160 ist nicht 8:16 und nicht 8:1 (ganze Zahl, kein Teilstring)",
           !uft_ufi_mountinfo_belegt(MASCHINE, 8, 1, eltern, NULL, NULL, 0) &&
           uft_ufi_mountinfo_belegt(MASCHINE, 8, 160, eltern, NULL, NULL, 0), NULL);
    pruefe("sdc (8:32) ist frei",
           !uft_ufi_mountinfo_belegt(MASCHINE, 8, 32, eltern, NULL, NULL, 0), NULL);

    p[0] = 0;
    uft_ufi_mountinfo_belegt(MASCHINE, 8, 160, eltern, NULL, p, sizeof p);
    pruefe("Einhaengepunkt roh, mit der Oktalflucht aus mountinfo",
           strcmp(p, "/media/axel/BIG\\040DISK") == 0, p);

    char klein[8];
    uft_ufi_mountinfo_belegt(MASCHINE, 8, 16, eltern, NULL, klein, sizeof klein);
    pruefe("zu kleiner Puffer: abgeschnitten und terminiert",
           strlen(klein) == sizeof klein - 1, klein);

    pruefe("leerer Text und NULL: nicht belegt, kein Absturz",
           !uft_ufi_mountinfo_belegt("", 8, 16, eltern, NULL, NULL, 0) &&
           !uft_ufi_mountinfo_belegt(NULL, 8, 16, eltern, NULL, NULL, 0), NULL);
    pruefe("letzte Zeile ohne Zeilenende wird gelesen",
           uft_ufi_mountinfo_belegt("1 1 7:3 / /x rw - vfat /dev/loop3 rw", 7, 3, NULL, NULL, NULL, 0),
           NULL);

#ifdef __linux__
    /* On a real kernel: the device of "/" IS mounted. This reaches the real
     * /proc/self/mountinfo, and — if "/" sits on a partition — the sysfs
     * resolver: the whole disk must count as occupied too. */
    {
        struct stat st;
        char h[160];
        if (stat("/", &st) == 0) {
            unsigned ma = major(st.st_dev), mi = minor(st.st_dev);
            p[0] = 0;
            int r = uft_ufi_linux_belegt(ma, mi, p, sizeof p);
            snprintf(h, sizeof h, "st_dev %u:%u -> %d (%s)", ma, mi, r, p);
            pruefe("Linux: das Geraet von / ist eingehaengt (echtes mountinfo)", r == 1, h);

            char sp[96];
            snprintf(sp, sizeof sp, "/sys/dev/block/%u:%u/partition", ma, mi);
            FILE *pf = fopen(sp, "r");
            if (pf) {
                fclose(pf);
                unsigned pa = 0, pb = 0;
                snprintf(sp, sizeof sp, "/sys/dev/block/%u:%u/../dev", ma, mi);
                FILE *df = fopen(sp, "r");
                if (df && fscanf(df, "%u:%u", &pa, &pb) == 2) {
                    r = uft_ufi_linux_belegt(pa, pb, NULL, 0);
                    snprintf(h, sizeof h, "ganzes Laufwerk %u:%u -> %d", pa, pb, r);
                    pruefe("Linux: das ganze Laufwerk unter / gilt als belegt (sysfs)", r == 1, h);
                }
                if (df) fclose(df);
            } else {
                printf("  [---] / liegt nicht auf einer Partition (%u:%u) - sysfs-Teil entfaellt\n", ma, mi);
            }
        }
    }
#else
    printf("  [---] kein Linux - der Teil gegen das echte mountinfo entfaellt\n");
#endif

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
