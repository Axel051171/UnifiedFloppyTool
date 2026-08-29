/**
 * @file test_adf_directory_crosstool.c
 * @brief Der ADF-Leser gegen ein fremd erzeugtes Abbild (MF-685)
 *
 * Spur A, zweite Tür. Und sie sah beim Messen anders aus als geplant:
 *
 * **Der Leser existiert längst.** `src/fs/uft_amigados.c` kann
 * Wurzelverzeichnis, Unterverzeichnisse, Blockketten, Hard- und
 * Softlinks, Extraktion. Zu bauen war nichts. Was fehlte, war der
 * Abgleich: `tests/test_amiga_extract.c` und `test_amigados_cycle.c`
 * bauen ihre ADFs **synthetisch im Speicher** (`build_ofs_adf()`), und
 * `git grep xdftool_dd_ofs -- tests/` findet zehn Dateien — **keine**
 * davon prüft den Dateisystem-Leser.
 *
 * Das ist genau die Lage, die `docs/VERIFICATION_TIERS.md` T3 nennt:
 * „ein synthetischer Test ohne Abgleich gegen eine autoritative
 * Quelle". Ein selbstgebautes ADF beweist, dass der Leser liest, was
 * derselbe Kopf geschrieben hat.
 *
 * ── Die Referenz ─────────────────────────────────────────────────────────
 *
 * `tests/corpus_manifest/manifest.json` hält den Erzeugungsbefehl fest:
 *
 *     Werkzeug: amitools xdftool
 *     Befehl:   xdftool <img> create
 *                       + format "UFTCORPUS" ofs
 *                       + write marker.txt
 *
 * Daraus: Datenträgername `UFTCORPUS`, Dateisystem OFS, genau eine
 * Datei namens `marker.txt`. Alles drei steht im Befehl, nicht im
 * Abbild.
 *
 * ── Die Kalibrierung: roh oder gepolstert? ───────────────────────────────
 *
 * Der floptool-Fund (MF-684) hat eine Fehlerklasse benannt, die nicht
 * auf floptool beschränkt ist: ein Werkzeug meldete `254 Byte`, wo die
 * Datei 127 hat — die Sektorkapazität statt des Inhalts. Ein
 * Differenzlauf gegen so einen Wert misst die Polsterung, nicht das
 * Format, und wird grün, sobald der eigene Leser auffüllt. Also gerade
 * dann, wenn er etwas erfindet.
 *
 * Darum steht die Längenprüfung hier nicht am Rand, sondern im
 * Mittelpunkt. Ein OFS-Datenblock fasst 488 Nutzbytes. Meldet dieser
 * Leser **488**, ist er gepolstert; meldet er **127**, liest er die
 * Länge aus dem Dateikopf, wo sie hingehört.
 *
 * Die 127 sind dreifach gestützt und keine davon ist dieses Abbild:
 *   1. der Scout hat `adfrescue` gegen dieselbe Datei laufen lassen —
 *      `marker.txt`, 127 Byte, byteidentisch zur xdftool-Extraktion;
 *   2. die D64-Zwillingsdatei trägt denselben UFT-Text, und ihr
 *      Verzeichniseintrag meldet 1 Block (MF-683);
 *   3. `docs/ORACLES.md` führt 127 seit MF-684 als den wahren Wert.
 *
 * ── Was hier NICHT geprüft wird ──────────────────────────────────────────
 *
 * FFS, Unterverzeichnisse, Hard- und Softlinks. Das Korpus-Abbild ist
 * ein flaches OFS mit einer Datei; mehr kann es nicht bezeugen. Diese
 * Fälle als hier geprüft auszugeben wäre dieselbe Unehrlichkeit, gegen
 * die dieser Test antritt — sie stehen als benannte Grenze in
 * `docs/OPEN_ITEMS.md`, nicht als stille Lücke.
 */

#include "uft/fs/uft_amigados.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

/* Was der Erzeugungsbefehl zusagt. */
#define ERWARTET_VOLUME   "UFTCORPUS"
#define ERWARTET_DATEI    "marker.txt"
#define ERWARTET_BYTES    127u
/* Was ein gepolsterter Leser stattdessen meldete: OFS-Nutzlast je Block. */
#define OFS_BLOCK_NUTZ    488u

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

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
    printf("ADF-Leser gegen das xdftool-Abbild (MF-685)\n\n");

    char pfad[1024];
    snprintf(pfad, sizeof(pfad), "%s/xdftool_dd_ofs.adf", UFT_CORPUS_DIR);
    FILE *f = fopen(pfad, "rb");
    if (!f) {
        printf("FEHLER: %s fehlt.\n", pfad);
        printf("Die Datei liegt im FREIEN Korpus und ist im Baum.\n");
        return 1;
    }
    fclose(f);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    if (!ctx) { printf("FEHLER: kein Kontext\n"); return 1; }

    if (uft_amiga_open_file(ctx, pfad, NULL) != 0) {
        printf("  FAIL das Abbild liess sich nicht oeffnen\n");
        uft_amiga_destroy(ctx);
        printf("\nFEHLGESCHLAGEN (1 Abweichungen)\n");
        return 1;
    }

    /* 1: Der Datentraegername steht im Formatier-Befehl. */
    PRUEFE(gleich_ohne_fall(ctx->volume_name, ERWARTET_VOLUME),
           "Datentraeger heisst \"%s\", der xdftool-Befehl sagt \"%s\"",
           ctx->volume_name, ERWARTET_VOLUME);
    printf("  ok   Datentraeger \"%s\"\n", ctx->volume_name);

    /* 2: ein `write`, also eine Datei im Wurzelverzeichnis. */
    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    int rc = uft_amiga_load_root(ctx, &dir);
    PRUEFE(rc == 0, "Wurzelverzeichnis nicht lesbar (rc=%d)", rc);

    if (rc == 0) {
        PRUEFE(dir.count == 1,
               "%zu Eintraege, der Befehl schrieb GENAU EINE Datei",
               dir.count);

        if (dir.count >= 1) {
            const uft_amiga_entry_t *e = &dir.entries[0];

            PRUEFE(gleich_ohne_fall(e->name, ERWARTET_DATEI),
                   "Datei heisst \"%s\", geschrieben wurde \"%s\"",
                   e->name, ERWARTET_DATEI);
            PRUEFE(e->is_file && !e->is_dir,
                   "\"%s\" muss eine Datei sein, kein Verzeichnis", e->name);

            /* ── Die Kalibrierung ─────────────────────────────────────
             *
             * Hier entscheidet sich, ob dieser Leser die Laenge aus dem
             * Dateikopf nimmt oder die belegte Blockkapazitaet meldet.
             * Die zweite Pruefung ist die wichtigere: sie nennt den
             * konkreten falschen Wert beim Namen, damit ein spaeterer
             * Leser die Fehlerklasse an der Meldung erkennt und nicht
             * erst nachrechnen muss. */
            PRUEFE(e->size == ERWARTET_BYTES,
                   "Groesse ist %u Byte, die Datei hat %u — drei "
                   "unabhaengige Haende sagen %u (adfrescue, D64-Zwilling, "
                   "ORACLES.md)", e->size, ERWARTET_BYTES, ERWARTET_BYTES);

            PRUEFE(e->size != OFS_BLOCK_NUTZ,
                   "Groesse ist genau die OFS-Blocknutzlast (%u) — der "
                   "Leser meldet die POLSTERUNG statt des Inhalts. Das ist "
                   "die Fehlerklasse aus MF-684, diesmal auf der "
                   "Amiga-Seite", OFS_BLOCK_NUTZ);

            if (e->size == ERWARTET_BYTES)
                printf("  ok   1 Datei \"%s\", %u Byte (roh, nicht "
                       "gepolstert)\n", e->name, e->size);
            /* `blocks` NICHT als Ergebnis ausgeben.
             *
             * Beim ersten Lauf stand hier "belegt 0 Bloecke" — als waere
             * das eine Messung. Nachgemessen: `uft_amiga_entry_t.blocks`
             * wird in `src/fs/uft_amigados.c` NIRGENDS zugewiesen. Es ist
             * ein Ergebnisfeld ohne Erzeuger und meldet immer 0.
             *
             * Eine 127-Byte-OFS-Datei belegt mindestens einen Datenblock;
             * 0 ist also nicht bloss ungenau, sondern falsch. Der Test
             * schreibt den falschen Wert NICHT fest — das wuerde den
             * Fehler einfrieren — und druckt ihn auch nicht als Zahl,
             * weil eine gedruckte Null wie eine Messung aussieht. Er sagt,
             * was Sache ist. Siehe FS-1 in docs/OPEN_ITEMS.md. */
            printf("  --   `blocks` ist unbelegt (FS-1): Feld ohne "
                   "Erzeuger, meldet immer 0\n");
        }
        uft_amiga_free_dir(&dir);
    }

    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
