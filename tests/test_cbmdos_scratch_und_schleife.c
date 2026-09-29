/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_cbmdos_scratch_und_schleife.c
 * @brief CBM DOS directory: type byte $00 with a name is a SCRATCHED file,
 *        $80 is a visible DEL file, and a directory chain that points back
 *        is a finding, not 683 copies (P3-664, MF-1501).
 *
 * REFERENCE: docs/format_specs/commodore/D64.TXT (Peter Schepers), directory
 * entry, byte $02:
 *
 *     $00 - Scratched (deleted file entry)
 *      80 - DEL
 *      81 - SEQ ...
 *
 * and libcbmimage (GPL-2, executed in the t4 review, not registered) which
 * treats only type byte 0 as deleted (dir.c:179) and reports "Loop
 * detected" on its own test image simpletest-loop.d64.
 *
 * What src/fs/uft_cbmdos.c did before MF-1501:
 *   - `$00` was skipped as "never used" — a scratched file vanished with
 *     its name and its first data sector (measured in the review: SINGLER
 *     on Fast Hack'em, four entries of OpenCBM's test.d64);
 *   - `$80`/`$C0` were listed as DELETED — measured: 28 visible `DEL<`
 *     entries of filleddk.d64 reported as deleted files;
 *   - the chain walk stopped only after 683 steps: simpletest-loop.d64
 *     gave 5192 entries where libcbmimage gives 76 and "Loop detected".
 *
 * The CI cases change single bytes of tests/corpus_free/vice_c1541_35trk.d64
 * (VICE c1541, cross-tool): one entry "UFT MARKER", type 0x82, first data
 * sector 17/0, in directory sector 18/1 at 0x16600.
 */
#include "uft/fs/uft_cbmdos.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

#define DIR_18_1 0x16600L

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static uint8_t *orig;
static size_t orig_n;

static const char *kopie(const char *name, void (*aendern)(uint8_t *))
{
    static char pfad[600];
    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(pfad, sizeof pfad, "%s/%s", t, name);
    uint8_t *b = malloc(orig_n);
    if (!b) return NULL;
    memcpy(b, orig, orig_n);
    aendern(b);
    FILE *f = fopen(pfad, "wb");
    if (!f) { free(b); return NULL; }
    size_t w = fwrite(b, 1, orig_n, f);
    fclose(f);
    free(b);
    return w == orig_n ? pfad : NULL;
}

static void typ_80(uint8_t *b) { b[DIR_18_1 + 2] = 0x80; }
static void typ_00(uint8_t *b) { b[DIR_18_1 + 2] = 0x00; }
static void nie_benutzt(uint8_t *b) { memset(b + DIR_18_1 + 2, 0, 30); }
static void schleife(uint8_t *b) { b[DIR_18_1 + 0] = 18; b[DIR_18_1 + 1] = 1; }

int main(void)
{
    char pfad[600], h[200];
    uft_cbmdos_dir_t d;

    printf("CBM DOS: $00 gescratcht, $80 sichtbar DEL, Kettenschleife (MF-1501)\n");
    snprintf(pfad, sizeof pfad, "%s/vice_c1541_35trk.d64", UFT_CORPUS_DIR);
    FILE *f = fopen(pfad, "rb");
    if (!f) { pruefe("Korpusdatei", 0, pfad); return 1; }
    fseek(f, 0, SEEK_END);
    orig_n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    orig = malloc(orig_n);
    if (!orig || fread(orig, 1, orig_n, f) != orig_n) { fclose(f); pruefe("lesen", 0, NULL); return 1; }
    fclose(f);
    if (orig[DIR_18_1 + 2] != 0x82) { pruefe("Korpus traegt 0x82 bei 18/1", 0, NULL); return 1; }

    /* 1. $80: a DEL file the directory SHOWS — not a deleted one. */
    const char *p = kopie("uft_mf1501_80.d64", typ_80);
    memset(&d, 0, sizeof d);
    uft_error_t rc = p ? uft_cbmdos_read_directory(p, &d) : UFT_ERR_IO;
    snprintf(h, sizeof h, "rc=%d, %d Eintraege, geloescht=%d, Typ=%d, deleted_count=%d",
             (int)rc, d.entry_count, d.entry_count ? (int)d.entries[0].deleted : -1,
             d.entry_count ? (int)d.entries[0].type : -1, d.deleted_count);
    pruefe("$80 ist eine sichtbare DEL-Datei, keine geloeschte (D64.TXT)",
           rc == UFT_OK && d.entry_count == 1 && !d.entries[0].deleted &&
           d.entries[0].type == UFT_CBMDOS_DEL && d.deleted_count == 0, h);
    uft_cbmdos_free(&d);
    if (p) remove(p);

    /* 2. $00 with name and pointers intact: SCRATCHED — kept, marked. */
    p = kopie("uft_mf1501_00.d64", typ_00);
    memset(&d, 0, sizeof d);
    rc = p ? uft_cbmdos_read_directory(p, &d) : UFT_ERR_IO;
    snprintf(h, sizeof h, "rc=%d, %d Eintraege, deleted_count=%d", (int)rc, d.entry_count, d.deleted_count);
    pruefe("$00 mit Namen ist eine gescratchte Datei und bleibt sichtbar",
           rc == UFT_OK && d.entry_count == 1 && d.deleted_count == 1 &&
           d.entries[0].deleted && strcmp(d.entries[0].name, "UFT MARKER") == 0 &&
           d.entries[0].track == 17 && d.entries[0].sector == 0, h);
    uft_cbmdos_free(&d);
    if (p) remove(p);

    /* 3. An entry whose 30 bytes are all zero was never written: absent. */
    p = kopie("uft_mf1501_leer.d64", nie_benutzt);
    memset(&d, 0, sizeof d);
    rc = p ? uft_cbmdos_read_directory(p, &d) : UFT_ERR_IO;
    snprintf(h, sizeof h, "rc=%d, %d Eintraege, deleted_count=%d", (int)rc, d.entry_count, d.deleted_count);
    pruefe("eine nie beschriebene Zeile (30 Nullbytes) erscheint nicht",
           rc == UFT_OK && d.entry_count == 0 && d.deleted_count == 0, h);
    uft_cbmdos_free(&d);
    if (p) remove(p);

    /* 4. 18/1 points back at itself: the entry once, and the loop said. */
    p = kopie("uft_mf1501_schleife.d64", schleife);
    memset(&d, 0, sizeof d);
    rc = p ? uft_cbmdos_read_directory(p, &d) : UFT_ERR_IO;
    snprintf(h, sizeof h, "rc=%d, %d Eintraege, Schleife gemeldet=%d",
             (int)rc, d.entry_count, (int)d.chain_loop);
    pruefe("Kettenschleife: der Eintrag einmal, nicht 683-mal",
           rc == UFT_OK && d.entry_count == 1, h);
    pruefe("Kettenschleife: gemeldet, nicht still abgeschnitten",
           rc == UFT_OK && d.chain_loop, h);
    uft_cbmdos_free(&d);
    if (p) remove(p);

    /* 5. The unchanged image: one closed PRG, no loop. */
    memset(&d, 0, sizeof d);
    rc = uft_cbmdos_read_directory(pfad, &d);
    pruefe("unveraendert: ein PRG, keine Schleife",
           rc == UFT_OK && d.entry_count == 1 && !d.chain_loop &&
           d.entries[0].type == UFT_CBMDOS_PRG && !d.entries[0].deleted, NULL);
    uft_cbmdos_free(&d);

    free(orig);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
