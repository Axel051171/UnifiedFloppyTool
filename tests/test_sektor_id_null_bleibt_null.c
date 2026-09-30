/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_sektor_id_null_bleibt_null.c
 * @brief A recorded sector ID R = 0 reaches the model as 0, not as 1 (P3-549).
 *
 * Four plugins passed the recorded R through `R > 0 ? R - 1 : 0` into
 * uft_format_add_sector(), which adds 1 again. For R >= 1 that is the
 * identity; for R = 0 it yields 1 — so a track carrying R = 0 and R = 1
 * came out with two sectors named 1, and the first one's real name was
 * gone. Copy protection uses exactly such IDs. MF-1017 fixed the same
 * shape in jv3, and d77 carries its own fix (test_d77_spur_nach_kopf).
 *
 * FDI and NFD build a one-track image whose sectors carry R = 0, 1, 2; STX
 * and TD0 take a committed corpus file (hxcfe_pc160.stx, libdsk_uftk_pc720
 * .td0) and set the first sector's R from 1 to 0, with that 1 asserted as
 * the anchor. Each opens with the plugin under test and reads C0/H0.
 * Asserted: the IDs read are exactly the IDs recorded, in order.
 *
 * Layouts are the ones the neighbouring tests already use and anchor:
 *   FDI (ZX)  SAMdisk fdi.cpp — test_crc_befund_erreicht_modell
 *   NFD r0    T98-Next           — test_crc_befund_erreicht_modell
 *   STX       Pasti descriptor, R at +0x0A — test_crc_befund_erreicht_modell
 *   TD0       Teledisk headers, as the TD0 plugin reads them
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_common.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_fdi;
extern const uft_format_plugin_t uft_format_plugin_nfd;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_td0;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static const char *tmpdir(void)
{
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = getenv("TMP");
    if (!t || !*t) t = getenv("TEMP");
    return (t && *t) ? t : ".";
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    size_t w = fwrite(b, 1, n, f);
    fclose(f);
    return w == n;
}

/* Open with the plugin under test, read C0/H0, write the IDs as "0,1,2". */
static void ids_lesen(const uft_format_plugin_t *p, const char *pfad,
                      char *out, size_t on)
{
    uft_disk_t disk;
    uft_track_t tr;
    memset(&disk, 0, sizeof disk);
    memset(&tr, 0, sizeof tr);
    snprintf(out, on, "(open scheiterte)");
    if (p->open(&disk, pfad, true) != UFT_OK) return;
    snprintf(out, on, "(read_track scheiterte)");
    if (p->read_track(&disk, 0, 0, &tr) == UFT_OK) {
        out[0] = 0;
        for (size_t i = 0; i < tr.sector_count; i++) {
            size_t l = strlen(out);
            snprintf(out + l, on - l, "%s%u", i ? "," : "",
                     (unsigned)tr.sectors[i].id.sector);
        }
    }
    for (size_t i = 0; i < tr.sector_count; i++) free(tr.sectors[i].data);
    free(tr.sectors);
    p->close(&disk);
}

static void fall(const char *was, const uft_format_plugin_t *p,
                 const char *pfad, const char *soll)
{
    char ist[128], h[256];
    ids_lesen(p, pfad, ist, sizeof ist);
    remove(pfad);
    snprintf(h, sizeof h, "gelesen %s, erwartet %s", ist, soll);
    pruefe(was, strcmp(ist, soll) == 0, h);
}

/* ZX FDI: one track, three 256-byte sectors R = 0, 1, 2, CRC-ok bits set.
 * flag0 is the flag byte of the R = 0 sector: 0x02 takes the data path,
 * 0x40 ("no data", SAMdisk) the fill path — both carried the old shape. */
static void fdi(uint8_t flag0, const char *was)
{
    enum { DATEN = 64 };
    static uint8_t b[DATEN + 3 * 256];
    char pfad[600];
    memset(b, 0, sizeof b);
    memcpy(b, "FDI", 3);
    b[4] = 1; b[6] = 1; b[0x0A] = DATEN;
    b[14 + 6] = 3;
    for (int i = 0; i < 3; i++) {
        uint8_t *s = b + 21 + 7 * i;
        s[0] = 0; s[1] = 0; s[2] = (uint8_t)i; s[3] = 1;
        s[4] = i == 0 ? flag0 : 0x02;
        s[5] = (uint8_t)((256 * i) & 0xFF); s[6] = (uint8_t)((256 * i) >> 8);
        memset(b + DATEN + 256 * i, 0x60 + i, 256);
    }
    snprintf(pfad, sizeof pfad, "%s/uft_p3549.fdi", tmpdir());
    if (!schreibe(pfad, b, sizeof b)) { pruefe("FDI Pruefdatei", 0, pfad); return; }
    fall(was, &uft_format_plugin_fdi, pfad, "0,1,2");
}

/* NFD r0: one track, three 256-byte sectors. With `abgeschnitten` the
 * entries run R = 2, 1, 0 and the file ends 100 bytes into the last one,
 * so R = 0 takes the truncated-source path. */
static void nfd(int abgeschnitten, const char *was)
{
    enum { TAB = 163 * 26 * 16, KOPF = 0x120 + TAB };
    static uint8_t b[KOPF + 3 * 256];
    char pfad[600];
    memset(b, 0, sizeof b);
    memcpy(b, "T98FDDIMAGE.R0", 14);
    b[0x110] = (uint8_t)KOPF; b[0x111] = (uint8_t)(KOPF >> 8);
    b[0x112] = (uint8_t)(KOPF >> 16);
    b[0x115] = 1;
    for (int slot = 0; slot < 163 * 26; slot++) b[0x120 + 16 * slot] = 0xFF;
    for (int i = 0; i < 3; i++) {
        uint8_t *e = b + 0x120 + 16 * i;
        e[0] = 0; e[1] = 0; e[2] = (uint8_t)(abgeschnitten ? 2 - i : i); e[3] = 1;
        e[4] = 1; e[10] = 0x90;
    }
    for (int i = 0; i < 3; i++) memset(b + KOPF + 256 * i, 0x40 + i, 256);
    snprintf(pfad, sizeof pfad, "%s/uft_p3549.nfd", tmpdir());
    size_t n = abgeschnitten ? KOPF + 2 * 256 + 100 : sizeof b;
    if (!schreibe(pfad, b, n)) { pruefe("NFD Pruefdatei", 0, pfad); return; }
    fall(was, &uft_format_plugin_nfd, pfad, abgeschnitten ? "2,1,0" : "0,1,2");
}

/* Load a committed corpus file; the caller changes one byte and writes it. */
static uint8_t *korpus(const char *name, long *n)
{
    char quelle[600];
    snprintf(quelle, sizeof quelle, "%s/%s", UFT_CORPUS_DIR, name);
    FILE *f = fopen(quelle, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (*n > 64) ? (uint8_t *)malloc((size_t)*n) : NULL;
    if (b && fread(b, 1, (size_t)*n, f) != (size_t)*n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

/* Set R at offset `off` from 1 to 0, write, open, compare. */
static void mit_r_null(const char *name, long off, const char *endung,
                       const uft_format_plugin_t *p, const char *was,
                       const char *soll)
{
    char pfad[600], anker[160];
    long n = 0;
    uint8_t *b = korpus(name, &n);
    if (!b) { pruefe(was, 0, "Korpusdatei nicht lesbar"); return; }
    /* the anchor this case rests on: the first sector says R = 1 there */
    snprintf(anker, sizeof anker, "%s Anker: erster Sektor traegt R = 1 bei Offset %ld",
             endung, off);
    pruefe(anker, off < n && b[off] == 1, "Korpusdatei hat eine andere Lage");
    if (off < n) b[off] = 0;
    snprintf(pfad, sizeof pfad, "%s/uft_p3549.%s", tmpdir(), endung);
    int ok = schreibe(pfad, b, (size_t)n);
    free(b);
    if (!ok) { pruefe(was, 0, pfad); return; }
    fall(was, p, pfad, soll);
}

/* TD0 (Teledisk, libdsk_uftk_pc720.td0 — signature "TD", uncompressed):
 * 12-byte file header; if bit 7 of byte 7 is set, a comment block of
 * 10 + length bytes (length at 14..15); then a 4-byte track header, then
 * the first 6-byte sector header C H R N flags crc — R at +2. */
static long td0_erstes_r(void)
{
    long n = 0;
    uint8_t *b = korpus("libdsk_uftk_pc720.td0", &n);
    if (!b) return -1;
    long off = 12;
    if (b[7] & 0x80) off += 10 + (long)(b[14] | (b[15] << 8));
    free(b);
    return off + 4 + 2;
}

int main(void)
{
    printf("Sektor-ID 0 bleibt 0 (P3-549)\n");
    fdi(0x02, "FDI: R = 0,1,2 kommt als 0,1,2 an (Datenpfad)");
    fdi(0x40, "FDI: R = 0 ohne Daten bleibt 0 (Fuellpfad)");
    nfd(0, "NFD: R = 0,1,2 kommt als 0,1,2 an");
    nfd(1, "NFD: R = 0 mit abgeschnittenen Daten bleibt 0 (Fuellpfad)");
    /* STX: hxcfe_pc160.stx, descriptor 0 at 32, R at +0x0A; R = 1..8 */
    mit_r_null("hxcfe_pc160.stx", 42, "stx", &uft_format_plugin_stx,
               "STX: R = 0 bleibt 0 (neben R = 2..8)", "0,2,3,4,5,6,7,8");
    /* TD0: libdsk writes a PC 720K track as R = 1..9 */
    mit_r_null("libdsk_uftk_pc720.td0", td0_erstes_r(), "td0",
               &uft_format_plugin_td0,
               "TD0: R = 0 bleibt 0 (neben R = 2..9)", "0,2,3,4,5,6,7,8,9");
    printf("%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
