/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_v3_scratch_und_schleife.c
 * @brief The second CBM directory door, src/formats/d64/uft_d64_parser_v3.c:
 *        type byte $00 with a name is a SCRATCHED file, $80 a visible DEL
 *        file, and a chain pointing back is reported (P3-668, MF-1515).
 *
 * REFERENCE: docs/format_specs/commodore/D64.TXT, directory entry byte $02:
 *     $00 - Scratched (deleted file entry)
 *      80 - DEL
 * — the same reference and the same cases as
 * tests/test_cbmdos_scratch_und_schleife.c for the first door (MF-1501).
 *
 * What this reader did before MF-1515:
 *   - `if (ftype == 0) continue;` (comment: "Empty entry") — a scratched file
 *     vanished with its name and first data sector;
 *   - no `deleted` field at all, so it could not have marked one;
 *   - the chain walk stopped only after 20 sectors: a sector pointing at
 *     itself gave the same entry 20 times, with no report.
 * The MF-909 comment in src/fs/uft_cbmdos.c said of this file „dort steht
 * jetzt dieselbe Antwort"; `git log -S MF-909` on this file finds no commit
 * — the sentence was never true (BERICHTIGT there).
 *
 * Reach, said plainly: `directory` and `file_count` of this parser have NO
 * reader — not in the file, not outside it (git grep; d64_disk_v3_t exists
 * only in the .c file, the v3 bridge passes geometry and sectors). The fix
 * makes an unread list right (class P3-204); this test is its first
 * reader.
 *
 * The cases change single bytes of tests/corpus_free/vice_c1541_35trk.d64
 * (VICE c1541, cross-tool): one entry "UFT MARKER", type 0x82, first data
 * sector 17/0, in directory sector 18/1 at 0x16600.
 */
#include "../src/formats/d64/uft_d64_parser_v3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

#define DIR_18_1 0x16600u

static int gruen = 0, rot = 0;
static d64_disk_v3_t disk;          /* ~600 KB: static, not on the stack */
static uint8_t orig[D64_SIZE_35];
static uint8_t img[D64_SIZE_35];

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static bool lies(void)
{
    d64_params_t params;
    d64_get_default_params(&params);
    d64_disk_free(&disk);
    memset(&disk, 0, sizeof disk);
    return d64_parse(img, sizeof img, &disk, &params);
}

int main(void)
{
    char pfad[600], h[200];
    printf("D64 v3: $00 gescratcht, $80 sichtbar DEL, Kettenschleife (MF-1515)\n");

    snprintf(pfad, sizeof pfad, "%s/vice_c1541_35trk.d64", UFT_CORPUS_DIR);
    FILE *f = fopen(pfad, "rb");
    if (!f || fread(orig, 1, sizeof orig, f) != sizeof orig) {
        if (f) fclose(f);
        pruefe("Korpusdatei lesbar", 0, pfad);
        return 1;
    }
    fclose(f);
    if (orig[DIR_18_1 + 2] != 0x82) { pruefe("Korpus traegt 0x82 bei 18/1", 0, NULL); return 1; }

    /* 0. unchanged: one closed PRG, not deleted, no loop */
    memcpy(img, orig, sizeof img);
    bool ok = lies();
    snprintf(h, sizeof h, "parse=%d, %u Eintraege", (int)ok, (unsigned)disk.file_count);
    pruefe("unveraendert: ein PRG, nicht geloescht, keine Schleife",
           ok && disk.file_count == 1 && !disk.directory[0].deleted &&
           disk.directory[0].file_type == 0x82 && !disk.dir_chain_loop, h);

    /* 1. $80: a DEL file the directory SHOWS */
    memcpy(img, orig, sizeof img);
    img[DIR_18_1 + 2] = 0x80;
    ok = lies();
    snprintf(h, sizeof h, "%u Eintraege, geloescht=%d", (unsigned)disk.file_count,
             disk.file_count ? (int)disk.directory[0].deleted : -1);
    pruefe("$80 ist eine sichtbare DEL-Datei, nicht geloescht (D64.TXT)",
           ok && disk.file_count == 1 && !disk.directory[0].deleted &&
           disk.directory[0].closed, h);

    /* 2. $00 with name and pointers intact: SCRATCHED, kept and marked */
    memcpy(img, orig, sizeof img);
    img[DIR_18_1 + 2] = 0x00;
    ok = lies();
    snprintf(h, sizeof h, "%u Eintraege, Name='%s', Start %u/%u, geloescht=%d",
             (unsigned)disk.file_count, disk.directory[0].filename,
             (unsigned)disk.directory[0].first_track, (unsigned)disk.directory[0].first_sector,
             (int)disk.directory[0].deleted);
    pruefe("$00 mit Namen ist eine gescratchte Datei und bleibt sichtbar",
           ok && disk.file_count == 1 && disk.directory[0].deleted &&
           /* case-insensitive: this reader maps PETSCII to lower case
            * ("uft marker"), uft_cbmdos to upper case — a separate
            * question, not decided here */
           strcasecmp(disk.directory[0].filename, "UFT MARKER") == 0 &&
           disk.directory[0].first_track == 17 && disk.directory[0].first_sector == 0, h);

    /* 3. a row whose 30 bytes from the type byte are all zero: never written */
    memcpy(img, orig, sizeof img);
    memset(img + DIR_18_1 + 2, 0, 30);
    ok = lies();
    snprintf(h, sizeof h, "%u Eintraege", (unsigned)disk.file_count);
    pruefe("eine nie beschriebene Zeile (30 Nullbytes) erscheint nicht",
           ok && disk.file_count == 0, h);

    /* 4. 18/1 points back at itself */
    memcpy(img, orig, sizeof img);
    img[DIR_18_1 + 0] = 18;
    img[DIR_18_1 + 1] = 1;
    ok = lies();
    snprintf(h, sizeof h, "%u Eintraege, Schleife gemeldet=%d",
             (unsigned)disk.file_count, (int)disk.dir_chain_loop);
    pruefe("Kettenschleife: der Eintrag einmal, nicht zwanzigmal",
           ok && disk.file_count == 1, h);
    pruefe("Kettenschleife: gemeldet, nicht still abgeschnitten",
           ok && disk.dir_chain_loop, h);

    d64_disk_free(&disk);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
