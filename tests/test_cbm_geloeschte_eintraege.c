/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_cbm_geloeschte_eintraege.c
 * @brief Ein gescratchter Eintrag ist Bestand, kein Nichts (MF-909)
 *
 * ── Zwei Tueren, zwei Antworten auf denselben Eintrag ─────────────────────
 *
 * Der Baum liest CBM-DOS-Verzeichnisse an ZWEI Stellen, und sie
 * behandeln einen geloeschten Eintrag entgegengesetzt:
 *
 *   `src/fs/uft_cbmdos.c`  — `if (typ == UFT_CBMDOS_DEL) {
 *                              out->deleted_count++; continue; }`
 *                            zaehlt ihn und WIRFT IHN WEG.
 *   `src/formats/d64/uft_d64_parser_v3.c` — nimmt ihn auf, sobald
 *                            `ftype != 0 && first_track > 0`, und
 *                            KENNZEICHNET IHN NICHT.
 *
 * Beides ist falsch, und zwar in entgegengesetzte Richtungen: die eine
 * Tuer verschweigt, die andere behauptet. Fuer ein Werkzeug, dessen
 * erster Satz „Kein Bit verloren. Keine stille Veraenderung." lautet,
 * ist ein gescratchter Eintrag **Bestand** — er traegt Namen, Typ und
 * die Zeiger auf seine Datenbloecke, und genau das will ein Forensiker
 * sehen.
 *
 * ── Und niemand sah den Zaehler ───────────────────────────────────────────
 *
 * `uft_cbmdos_dir_t.deleted_count` gibt es seit MF-889 — gemessen
 * ueber `git ls-files` wird er ausserhalb von `uft_cbmdos.c` **nirgends
 * gelesen**. Der Explorer-Reiter, der seit MF-889 das D64-Verzeichnis
 * anzeigt, zeigt also weder die geloeschten Eintraege noch ihre Zahl.
 *
 * ── Das Pruefstueck ───────────────────────────────────────────────────────
 *
 * `tests/corpus_free/vice_c1541_35trk.d64` (VICE `c1541`) traegt genau
 * einen Eintrag: Typbyte **0x82** (geschlossen, PRG), Name „UFT MARKER",
 * erster Datensektor Spur 17/0, 1 Block — im ersten Verzeichnissektor
 * bei 0x16600 (Spur 18, Sektor 1; 17 Spuren zu 21 Sektoren = 357
 * Bloecke, plus einer).
 *
 * Wird eine CBM-DOS-Datei gescratcht, bleibt der Eintrag stehen und nur
 * die Typkennung faellt auf DEL. Der Test setzt deshalb **ein einziges
 * Byte** — 0x82 auf 0x80 — und erhaelt damit genau das, was eine
 * geloeschte Datei auf einer echten Diskette hinterlaesst: Name und
 * Zeiger intakt, Typ DEL.
 *
 * Das Korpus-Abbild selbst wird nicht angefasst; der Test arbeitet auf
 * einer Kopie.
 */

#include "uft/fs/uft_cbmdos.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR muss vom Bau gesetzt werden"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* Spur 18, Sektor 1 — der erste Verzeichnissektor. */
#define DIR_OFFSET  (358u * 256u)
#define TYP_OFFSET  (DIR_OFFSET + 2u)

/**
 * @brief Kopiert das Korpus-Abbild und setzt das Typbyte des ersten
 *        Verzeichniseintrags.
 * @param typ 0x82 = geschlossen/PRG (wie im Korpus), 0x80 = gescratcht
 * @return Pfad der Kopie, oder NULL
 */
static const char *kopie_mit_typ(const char *name, uint8_t typ)
{
    static char pfad[512];
    char quelle[512];
    snprintf(quelle, sizeof(quelle), "%s/vice_c1541_35trk.d64", UFT_CORPUS_DIR);
    snprintf(pfad, sizeof(pfad), "%s", name);

    FILE *q = fopen(quelle, "rb");
    if (!q) return NULL;
    fseek(q, 0, SEEK_END);
    long n = ftell(q);
    fseek(q, 0, SEEK_SET);
    uint8_t *puffer = malloc((size_t)n);
    if (!puffer) { fclose(q); return NULL; }
    size_t gelesen = fread(puffer, 1, (size_t)n, q);
    fclose(q);
    if (gelesen != (size_t)n) { free(puffer); return NULL; }

    /* Erst pruefen, dass der Korpus traegt, was der Kopf behauptet. */
    if (puffer[TYP_OFFSET] != 0x82) { free(puffer); return NULL; }
    puffer[TYP_OFFSET] = typ;

    FILE *z = fopen(pfad, "wb");
    if (!z) { free(puffer); return NULL; }
    size_t w = fwrite(puffer, 1, (size_t)n, z);
    fclose(z);
    free(puffer);
    return (w == (size_t)n) ? pfad : NULL;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  1. WAECHTER: der unveraenderte Korpus liefert eine normale Datei.
 *
 *  Zuerst festhalten, dass das Pruefstueck stimmt — sonst sagt der Rest
 *  nichts.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(waechter_korpus_unveraendert)
{
    const char *p = kopie_mit_typ("uft_cbm_normal.d64", 0x82);
    ASSERT(p != NULL);

    uft_cbmdos_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_cbmdos_read_directory(p, &dir) == UFT_OK);
    ASSERT(dir.entry_count == 1);
    ASSERT(dir.deleted_count == 0);
    ASSERT(strcmp(dir.entries[0].name, "UFT MARKER") == 0);
    ASSERT(dir.entries[0].deleted == false);
    uft_cbmdos_free(&dir);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  2. Ein gescratchter Eintrag wird GEZEIGT, nicht verschwiegen.
 *
 *  Heute: `uft_cbmdos.c` zaehlt ihn und wirft ihn weg — `entry_count`
 *  ist 0, der Name „UFT MARKER" erreicht den Aufrufer nie.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(geloeschter_eintrag_wird_gezeigt)
{
    const char *p = kopie_mit_typ("uft_cbm_del.d64", 0x80);
    ASSERT(p != NULL);

    uft_cbmdos_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_cbmdos_read_directory(p, &dir) == UFT_OK);

    ASSERT(dir.entry_count == 1);
    ASSERT(dir.deleted_count == 1);
    ASSERT(strcmp(dir.entries[0].name, "UFT MARKER") == 0);
    ASSERT(dir.entries[0].deleted == true);
    ASSERT(dir.entries[0].type == UFT_CBMDOS_DEL);

    /* Die Zeiger auf die Datenbloecke bleiben — das ist der Grund,
     * warum ein Forensiker den Eintrag sehen will. */
    ASSERT(dir.entries[0].track == 17);
    ASSERT(dir.entries[0].sector == 0);

    uft_cbmdos_free(&dir);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  3. WAECHTER: ein NIE BENUTZTER Eintrag bleibt draussen.
 *
 *  Typbyte 0x00 heisst „diese Zeile wurde nie beschrieben" — anders als
 *  DEL, das eine geloeschte Datei bezeichnet. Wer beides gleich
 *  behandelt, erfindet Eintraege.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(waechter_nie_benutzt_bleibt_draussen)
{
    const char *p = kopie_mit_typ("uft_cbm_leer.d64", 0x00);
    ASSERT(p != NULL);

    uft_cbmdos_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    /* Ohne einen einzigen gueltigen Eintrag lehnt der Leser seit MF-889
     * ab — das ist richtig so und wird hier festgehalten. */
    uft_error_t rc = uft_cbmdos_read_directory(p, &dir);
    if (rc == UFT_OK) {
        ASSERT(dir.entry_count == 0);
        ASSERT(dir.deleted_count == 0);
        uft_cbmdos_free(&dir);
    }
    remove(p);
}

int main(void)
{
    printf("CBM DOS: ein gescratchter Eintrag ist Bestand (MF-909)\n");
    RUN(waechter_korpus_unveraendert);
    RUN(geloeschter_eintrag_wird_gezeigt);
    RUN(waechter_nie_benutzt_bleibt_draussen);
    printf("\n  %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
