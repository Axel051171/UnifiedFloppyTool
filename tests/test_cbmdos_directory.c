/**
 * @file test_cbmdos_directory.c
 * @brief Was steht auf der Diskette? (MF-683)
 *
 * Phase 1: die Tür des D64-Lesers. Bis hierher liest UFT ein D64 in
 * Sektoren und sagt einem Menschen **nicht**, welche Dateien darauf
 * sind. `git grep` nach einem CBM-DOS-Verzeichnisleser: nichts.
 * `src/fs/` kennt AmigaDOS und FAT12, kein CBM.
 *
 * ── Die Referenz, und warum sie keine ist, die wir uns selbst geben ──────
 *
 * Rotbeweis-zuerst heißt hier: die Erwartung stammt aus dem **Befehl**,
 * der das Abbild erzeugt hat, nicht aus dem Abbild. `tests/corpus_manifest/
 * manifest.json` hält ihn wörtlich fest:
 *
 *     Werkzeug: VICE 3.10 c1541 (release 3.10.0, GTK3VICE-3.10-win64)
 *     Befehl:   c1541 -format "uftcorpus,42" d64 <img>
 *                     -write marker.txt "uft marker"
 *
 * Daraus folgen vier Aussagen, die dieses Abbild erfüllen MUSS, ohne
 * dass irgendjemand es dafür aufmachen musste:
 *
 *   1. Diskname `UFTCORPUS`   (aus `-format "uftcorpus,42"`)
 *   2. Disk-ID  `42`          (dito)
 *   3. genau EINE Datei       (ein `-write`)
 *   4. deren Name `UFT MARKER` (zweites Argument von `-write`)
 *
 * Das ist der Unterschied zwischen einer Prüfung und einem Zirkelschluss.
 * Ein Test, der das Verzeichnis liest und dann behauptet, das Gelesene
 * sei richtig, prüft nur, dass zweimal dasselbe herauskommt. Diese vier
 * Werte kennt der Test, bevor er die Datei anfasst.
 *
 * ── Warum c1541 hier nicht läuft, und warum das genügt ───────────────────
 *
 * `c1541` ist in dieser Umgebung nicht installiert — gemessen, und es
 * steht auch nicht in `docs/ORACLES.md`. Ein Oracle, das man nicht
 * ausführen kann, ist trotzdem eine Referenz, wenn seine Ausgabe
 * **eingefroren** vorliegt: das Manifest ist genau das. Der Befehl steht
 * dort seit der Korpus-Aufnahme, zusammen mit dem SHA-256 des
 * Ergebnisses. Ändert sich die Datei, ändert sich der Hash, und
 * `test_corpus_d64` schlägt an.
 *
 * ── PETSCII, und was der Test dazu NICHT annimmt ─────────────────────────
 *
 * CBM-DOS speichert Namen in PETSCII, mit 0xA0 aufgefüllt. Die
 * Umwandlung nach ASCII ist für den druckbaren Bereich eine
 * Groß-/Kleinschreibungs-Frage — der Test vergleicht darum
 * groß-unempfindlich und schneidet die Füllbytes ab. Er schreibt keine
 * bestimmte Umwandlungstabelle fest; das wäre eine zweite Behauptung in
 * einem Test, der eine prüfen soll.
 */

#include "uft/uft_core.h"
#include "uft/fs/uft_cbmdos.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

/* Vergleicht ohne Ruecksicht auf Gross-/Kleinschreibung. */
static bool gleich_ohne_fall(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return false;
        a++; b++;
    }
    return *a == *b;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("CBM-DOS-Verzeichnis gegen den c1541-Befehl (MF-683)\n\n");

    char pfad[1024];
    snprintf(pfad, sizeof(pfad), "%s/vice_c1541_35trk.d64", UFT_CORPUS_DIR);
    FILE *f = fopen(pfad, "rb");
    if (!f) {
        printf("FEHLER: %s fehlt.\n", pfad);
        printf("Die Datei liegt im FREIEN Korpus und ist im Baum. Fehlt\n");
        printf("sie, ist der Baum kaputt, nicht der Test.\n");
        return 1;
    }
    fclose(f);

    uft_cbmdos_dir_t dir;
    memset(&dir, 0, sizeof(dir));

    uft_error_t rc = uft_cbmdos_read_directory(pfad, &dir);
    PRUEFE(rc == UFT_OK, "Verzeichnis nicht lesbar (rc=%d)", (int)rc);
    if (rc != UFT_OK) {
        printf("\nFEHLGESCHLAGEN (%d Abweichungen)\n", fehler + 1);
        return 1;
    }

    /* 1+2: Diskname und ID stehen im Formatier-Befehl. */
    PRUEFE(gleich_ohne_fall(dir.disk_name, "UFTCORPUS"),
           "Diskname ist \"%s\", der c1541-Befehl sagt \"uftcorpus\"",
           dir.disk_name);
    PRUEFE(gleich_ohne_fall(dir.disk_id, "42"),
           "Disk-ID ist \"%s\", der c1541-Befehl sagt \"42\"", dir.disk_id);
    printf("  ok   Diskname \"%s\", ID \"%s\"\n", dir.disk_name, dir.disk_id);

    /* 3: ein -write, also eine Datei. */
    PRUEFE(dir.entry_count == 1,
           "%d Eintraege gefunden, der Befehl schrieb GENAU EINE Datei",
           dir.entry_count);

    /* 4: der Name stammt aus dem zweiten Argument von -write. */
    if (dir.entry_count >= 1) {
        const uft_cbmdos_entry_t *e = &dir.entries[0];
        PRUEFE(gleich_ohne_fall(e->name, "UFT MARKER"),
               "Dateiname ist \"%s\", der c1541-Befehl schrieb "
               "\"uft marker\"", e->name);
        PRUEFE(e->blocks > 0,
               "eine geschriebene Datei belegt mindestens einen Block, "
               "gemeldet sind %u", e->blocks);
        printf("  ok   1 Datei: \"%s\", %u Bloecke, Typ %s\n",
               e->name, e->blocks, uft_cbmdos_type_name(e->type));
    }

    /* Und die Zusage, die dieses Werkzeug von jedem Leser verlangt: das
     * Verzeichnis zu lesen darf die Datei nicht anfassen. Ein Leser, der
     * schreibt, ist in einem Forensik-Werkzeug ein Fehler, kein
     * Schoenheitsfehler. */
    FILE *g = fopen(pfad, "rb");
    long groesse = 0;
    if (g) { fseek(g, 0, SEEK_END); groesse = ftell(g); fclose(g); }
    PRUEFE(groesse == 174848,
           "die Abbilddatei ist nach dem Lesen %ld Byte gross, erwartet "
           "sind 174848 (35 Spuren)", groesse);

    uft_cbmdos_free(&dir);

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
