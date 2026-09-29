/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file ufi_mountinfo.c
 * @brief Mount check over the text of /proc/self/mountinfo (P3-669,
 *        MF-1516). Reference and purpose: include/uft/hal/ufi_mountinfo.h.
 */
#include "uft/hal/ufi_mountinfo.h"

#include <stdlib.h>
#include <string.h>

/* One whitespace-separated token starting at *p; returns its start and
 * length, advances *p past it. Stops at the end of the line. */
static const char *token(const char **p, const char *zeilenende, size_t *len)
{
    const char *s = *p;
    while (s < zeilenende && (*s == ' ' || *s == '\t')) s++;
    const char *e = s;
    while (e < zeilenende && *e != ' ' && *e != '\t') e++;
    *p = e;
    *len = (size_t)(e - s);
    return s;
}

/* "maj:min" as a whole token, digits only — "8:160" is not "8:16". */
static bool geraetenummer(const char *s, size_t len, unsigned *maj, unsigned *min)
{
    char buf[32];
    if (len == 0 || len >= sizeof buf) return false;
    memcpy(buf, s, len);
    buf[len] = '\0';
    char *doppel = strchr(buf, ':');
    if (!doppel || doppel == buf || doppel[1] == '\0') return false;
    for (char *c = buf; *c; c++)
        if (c != doppel && (*c < '0' || *c > '9')) return false;
    *doppel = '\0';
    *maj = (unsigned)strtoul(buf, NULL, 10);
    *min = (unsigned)strtoul(doppel + 1, NULL, 10);
    return true;
}

bool uft_ufi_mountinfo_belegt(const char *text, unsigned maj, unsigned min,
                              uft_ufi_eltern_fn eltern, void *ctx,
                              char *punkt, size_t punkt_len)
{
    if (!text) return false;

    const char *zeile = text;
    while (*zeile) {
        const char *ende = strchr(zeile, '\n');
        if (!ende) ende = zeile + strlen(zeile);

        const char *p = zeile;
        size_t l1, l2, l3, l4, l5;
        token(&p, ende, &l1);                       /* (1) mount ID  */
        token(&p, ende, &l2);                       /* (2) parent ID */
        const char *mm = token(&p, ende, &l3);      /* (3) major:minor */
        token(&p, ende, &l4);                       /* (4) root */
        const char *mp = token(&p, ende, &l5);      /* (5) mount point */

        unsigned a, b;
        if (l1 && l2 && l5 && geraetenummer(mm, l3, &a, &b)) {
            bool treffer = (a == maj && b == min);
            if (!treffer && eltern) {
                unsigned pa, pb;
                if (eltern(a, b, &pa, &pb, ctx) && pa == maj && pb == min)
                    treffer = true;
            }
            if (treffer) {
                if (punkt && punkt_len) {
                    size_t n = l5 < punkt_len - 1 ? l5 : punkt_len - 1;
                    memcpy(punkt, mp, n);
                    punkt[n] = '\0';
                }
                return true;
            }
        }
        zeile = (*ende == '\n') ? ende + 1 : ende;
    }
    return false;
}

#ifdef __linux__
#include <stdio.h>
#include <unistd.h>

/* partition -> whole disk. /sys/dev/block/<maj>:<min> is a symlink into
 * .../block/<disk>/<part>; a partition carries a "partition" file, and the
 * directory above it is the whole disk, whose "dev" holds "maj:min". */
static bool sysfs_eltern(unsigned maj, unsigned min, unsigned *pmaj,
                         unsigned *pmin, void *ctx)
{
    (void)ctx;
    char p[128];
    snprintf(p, sizeof p, "/sys/dev/block/%u:%u/partition", maj, min);
    if (access(p, F_OK) != 0) return false;
    snprintf(p, sizeof p, "/sys/dev/block/%u:%u/../dev", maj, min);
    FILE *f = fopen(p, "r");
    if (!f) return false;
    unsigned a = 0, b = 0;
    int n = fscanf(f, "%u:%u", &a, &b);
    fclose(f);
    if (n != 2) return false;
    *pmaj = a;
    *pmin = b;
    return true;
}

/* /proc files report size 0 — read until EOF into a growing buffer. */
static char *lies_mountinfo(void)
{
    FILE *f = fopen("/proc/self/mountinfo", "r");
    if (!f) return NULL;
    size_t cap = 16384, n = 0;
    char *b = malloc(cap);
    while (b) {
        size_t r = fread(b + n, 1, cap - n - 1, f);
        n += r;
        if (r == 0) break;
        if (n + 1 >= cap) {
            char *g = realloc(b, cap * 2);
            if (!g) { free(b); b = NULL; break; }
            b = g;
            cap *= 2;
        }
    }
    int fehler = ferror(f);
    fclose(f);
    if (!b) return NULL;
    if (fehler) { free(b); return NULL; }
    b[n] = '\0';
    return b;
}

int uft_ufi_linux_belegt(unsigned maj, unsigned min, char *punkt, size_t punkt_len)
{
    char *mi = lies_mountinfo();
    if (!mi) return -1;
    bool belegt = uft_ufi_mountinfo_belegt(mi, maj, min, sysfs_eltern, NULL,
                                           punkt, punkt_len);
    free(mi);
    return belegt ? 1 : 0;
}
#endif /* __linux__ */
