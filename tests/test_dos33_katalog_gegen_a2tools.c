/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_dos33_katalog_gegen_a2tools.c
 * @brief The DOS 3.3 catalog as a2tools writes and lists it (P3-384).
 *
 * The fixture `tests/corpus_free/a2tools_dos33_filled.do` has two hands,
 * and the manifest names both:
 *
 *   - OWN hand: the empty skeleton — VTOC at T17 S0 and the catalog chain
 *     S15 -> S1 on track 17 (`scripts/mk_dos33_leer.py`). a2tools cannot
 *     format (P3-384, MF-1207), so the skeleton cannot come from it.
 *   - FOREIGN hand: everything in it — a2tools (catseye/a2tools
 *     @ 52ad81cc, GPL-2.0-or-later) wrote the five files, allocated their
 *     sectors in the VTOC free map, filled the catalog entries and the
 *     T/S lists, and deleted one again:
 *
 *       a2tools in t    <img> MARKER  marker.txt
 *       a2tools in b.0803 <img> BINDATA bin.bin
 *       a2tools in t    <img> GROSS   big.txt
 *       a2tools in -r t <img> RAWX    marker.txt
 *       a2tools in t    <img> WEG     marker.txt
 *       a2tools del     <img> WEG
 *
 * Every expectation below is a2tools' own `dir` output on that file —
 * NOT read off the image by this reader:
 *
 *       Disk Volume 254, Free Blocks: 367
 *         T 002 MARKER
 *         B 005 BINDATA
 *         T 152 GROSS
 *         T 002 RAWX
 *
 * except WEG, which a2tools does not list: its expectation comes from the
 * command sequence (same content as MARKER, so the same 2 sectors).
 *
 * What the skeleton alone decides — and therefore does NOT count as a
 * foreign check — is where the catalog chain starts and runs. The volume
 * number 254 is also own hand; a2tools only prints it back.
 */
#include "uft/fs/uft_dos33.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static const uft_dos33_entry_t *finde(const uft_dos33_catalog_t *c, const char *name)
{
    for (int i = 0; i < c->entry_count; i++)
        if (strcmp(c->entries[i].name, name) == 0) return &c->entries[i];
    return NULL;
}

static void erwarte(const uft_dos33_catalog_t *c, const char *name, char typ,
                    unsigned sektoren)
{
    char was[96], h[160];
    const uft_dos33_entry_t *e = finde(c, name);
    snprintf(was, sizeof was, "%c %03u %s (a2tools dir)", typ, sektoren, name);
    if (!e) { pruefe(was, 0, "fehlt"); return; }
    snprintf(h, sizeof h, "gelesen %c %03u, geloescht=%d, gesperrt=%d",
             e->type_char, (unsigned)e->sectors, (int)e->deleted, (int)e->locked);
    pruefe(was, e->type_char == typ && e->sectors == sektoren && !e->deleted && !e->locked, h);
}

int main(void)
{
    const char *pfad = UFT_CORPUS_DIR "/a2tools_dos33_filled.do";
    char h[160];
    uft_dos33_catalog_t c;

    printf("DOS 3.3: Katalog gegen a2tools (P3-384)\n");
    uft_error_t rc = uft_dos33_read_catalog(pfad, &c);
    snprintf(h, sizeof h, "rc=%d", (int)rc);
    pruefe("Katalog lesbar", rc == UFT_OK, h);
    if (rc != UFT_OK) { printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    snprintf(h, sizeof h, "Volume %u", (unsigned)c.volume);
    pruefe("Disk Volume 254", c.volume == 254, h);
    snprintf(h, sizeof h, "%u frei", c.free_sectors);
    pruefe("Free Blocks: 367", c.free_sectors == 367u, h);

    erwarte(&c, "MARKER", 'T', 2);
    erwarte(&c, "BINDATA", 'B', 5);
    erwarte(&c, "GROSS", 'T', 152);
    erwarte(&c, "RAWX", 'T', 2);

    int lebend = c.entry_count - c.deleted_count;
    snprintf(h, sizeof h, "%d lebend, %d geloescht", lebend, c.deleted_count);
    pruefe("vier lebende Eintraege, wie a2tools sie listet", lebend == 4, h);

    /* WEG: deleted by `a2tools del`, kept as a finding, not dropped. */
    const uft_dos33_entry_t *weg = finde(&c, "WEG");
    snprintf(h, sizeof h, "%s", weg ? "gefunden" : "fehlt");
    pruefe("WEG steht als geloeschter Eintrag da (2 Sektoren)",
           weg && weg->deleted && weg->sectors == 2 && c.deleted_count == 1, h);
    /* a2tools' del keeps the name and does not move the T/S track into
     * name byte 30 (a2tools.c:520-539) — so no original track is known. */
    pruefe("WEG: keine erfundene Ursprungsspur (0xFF)",
           weg && weg->ts_track == 0xFFu, NULL);

    uft_dos33_free(&c);

    /* The reader must say no. A zeroed image has no VTOC. */
    static uint8_t leer[UFT_DOS33_IMAGE_SIZE];
    memset(leer, 0, sizeof leer);
    rc = uft_dos33_read_catalog_mem(leer, sizeof leer, &c);
    pruefe("Nullabbild wird abgewiesen", rc != UFT_OK && c.entry_count == 0, NULL);

    /* The chain brake: the last catalog sector (T17 S1) pointed back at the
     * first (T17 S15) is a loop — a finding, not an endless read and not
     * an empty catalog. */
    static uint8_t kopie[UFT_DOS33_IMAGE_SIZE + 1];
    FILE *f = fopen(pfad, "rb");
    size_t n = f ? fread(kopie, 1, sizeof kopie, f) : 0;
    if (f) fclose(f);
    if (n == UFT_DOS33_IMAGE_SIZE) {
        uint8_t *s1 = kopie + (17u * 16u + 1u) * 256u;
        s1[1] = 17; s1[2] = 15;
        rc = uft_dos33_read_catalog_mem(kopie, n, &c);
        snprintf(h, sizeof h, "rc=%d, %d Eintraege", (int)rc, c.entry_count);
        pruefe("Katalogschleife wird als Fehler gemeldet", rc != UFT_OK && c.entries == NULL, h);
    } else {
        pruefe("Kopie fuer die Schleifenprobe", 0, "Datei nicht lesbar");
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
