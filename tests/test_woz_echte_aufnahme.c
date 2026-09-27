/* SPDX-License-Identifier: MIT */
/**
 * @file test_woz_echte_aufnahme.c
 * @brief WOZ aus echter Aufnahme gegen WOZ aus eigener Erzeugung
 *
 * ── DIE LUECKE, DIE DAS SCHLIESST ────────────────────────────────────
 *
 * Der Korpus trug auf der SCHUTZ-Achse bisher sieben Abbilder: zwei
 * C64-G64 (eines geschuetzt, eines als Negativkontrolle) und fuenf
 * Atari-ST-CopyLock-Dateien. **Apple II: null.** Und das einzige
 * WOZ-Abbild war `to_woz2_uftk_dos33.woz` — von UFT SELBST erzeugt.
 *
 * Ein selbst erzeugtes Abbild kann eine Frage nicht beantworten: was
 * macht der Leser mit einer Diskette, die er nicht geschrieben hat?
 * Gemessen ist der Unterschied drastisch — die Bitzahl je Spur:
 *
 *     to_woz2_uftk_dos33.woz     35 Spuren, EINE Laenge (50624), Spanne 0
 *     wozaday_gauntlet_e7.woz    35 Spuren, 28 Laengen, Spanne 119
 *     wozaday_copy2plus_52.woz   35 Spuren, 33 Laengen, Spanne 161
 *
 * Eine echte Aufnahme hat auf JEDER Spur eine andere Bitzahl, weil
 * Drehzahl und Schreibvorgang der Originaldiskette streuen. Ein
 * erzeugtes Abbild hat genau eine. Wer nur gegen Letzteres prueft, hat
 * den Normalfall nie gesehen.
 *
 * ── HERKUNFT UND LIZENZ ─────────────────────────────────────────────
 *
 * Beide Aufnahmen stammen aus 4ams `woz-a-day`-Sammlung auf archive.org
 * (Applesauce, WOZ2). Es ist urheberrechtlich geschuetzter Code OHNE
 * angegebene Lizenz, also **LOCAL-ONLY**: `tests/corpus/` ist gitignored
 * (`.gitignore:153`), nichts davon wird weitergegeben, Herkunft und
 * SHA-256 stehen im Manifest. Kanal: Daten/Fixture (MF-695) — dieselbe
 * Behandlung wie `tests/corpus/c64pp_*.g64`.
 *
 * ── WAS DIESER TEST ZUSICHERT — UND WAS NICHT ───────────────────────
 *
 *  1. UFT oeffnet eine echte Applesauce-Aufnahme und liefert Spuren.
 *  2. Die Bitzahlen STREUEN — der Leser glaettet sie nicht auf einen
 *     Wert. Eine Glaettung waere erfundene Gleichmaessigkeit.
 *  3. Das erzeugte Kontrollabbild streut NICHT. Ohne diese Zeile
 *     koennte Punkt 2 auch bei einem kaputten Leser zufaellig gruen
 *     sein.
 *
 * **Ausdruecklich NICHT zugesichert:** dass der Kopierschutz erkannt
 * wird. 4ams Beschreibung nennt fuer Gauntlet „the E7 bitstream", das
 * ist eine benannte Referenz — aber ob UFTs Schutzerkennung darauf
 * anspringt, ist eine eigene Messung und steht hier nicht. Wer sie
 * fuehrt, findet in dieser Datei das Abbild dafuer.
 */

#include "uft/uft_format_plugin.h"   /* vor uft_core.h */
#include "uft/uft_core.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR ""
#endif
#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

static int g_pass = 0, g_fail = 0;

#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; \
    printf("  FEHLGESCHLAGEN Zeile %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static void spur_frei(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    if (t->raw_data && t->owns_data) free(t->raw_data);
    memset(t, 0, sizeof(*t));
}

/**
 * Misst, was der Leser WIRKLICH liefert.
 *
 * Die erste Fassung fragte nach `raw_bits` und bekam ueberall 0 — nicht
 * weil die Abbilder leer sind, sondern weil `uft_woz_plugin.c` das Feld
 * nie setzt (gemessen: 0 Nennungen). Sie meldete daraufhin „0 Spuren"
 * und haette die Fehlersuche an die falsche Stelle geschickt. Der
 * Unterschied zwischen „liefert nichts" und „liefert etwas anderes als
 * gefragt" ist der ganze Befund.
 *
 * @param out_bits_gesetzt  auf 1, wenn IRGENDEINE Spur `raw_bits` traegt
 * @return Zahl der Spuren mit Sektoren
 */
static int messen(const char *pfad, int *out_sektoren, int *out_bits_gesetzt,
                  int *out_roh_spuren)
{
    *out_sektoren = 0; *out_bits_gesetzt = 0; *out_roh_spuren = 0;
    int spuren = 0;

    uft_disk_t *disk = uft_disk_open(pfad, true);
    if (!disk) {
        printf("     [oeffnen] uft_disk_open lieferte NULL fuer %s\n", pfad);
        return 0;
    }
    printf("     [oeffnen] Plugin \"%s\", %u Zylinder, %u Koepfe\n",
           uft_disk_plugin(disk) ? uft_disk_plugin(disk)->name : "(keins)",
           (unsigned)disk->geometry.cylinders,
           (unsigned)disk->geometry.heads);

    for (unsigned c = 0; c < disk->geometry.cylinders && c < 100u; c++) {
        uft_track_t tr;
        memset(&tr, 0, sizeof(tr));
        if (uft_disk_plugin(disk)->read_track(disk, (int)c, 0, &tr) != UFT_OK) {
            spur_frei(&tr);
            continue;
        }
        if (tr.sector_count) { spuren++; *out_sektoren += (int)tr.sector_count; }
        if (tr.raw_data && tr.raw_size) (*out_roh_spuren)++;
        if (tr.raw_bits) *out_bits_gesetzt = 1;
        spur_frei(&tr);
    }
    uft_disk_close(disk);
    return spuren;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_woz_echte_aufnahme ===\n\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("uft_register_all_formats scheiterte\n");
        return 1;
    }

    /* ── Kontrolle: das von UFT selbst erzeugte Abbild ──────────────── */
    char kpfad[1024];
    snprintf(kpfad, sizeof kpfad, "%s/to_woz2_uftk_dos33.woz", UFT_CORPUS_DIR);
    int k_sek = 0, k_bits = 0, k_roh = 0;
    const int k_spuren = messen(kpfad, &k_sek, &k_bits, &k_roh);
    printf("KONTROLLE to_woz2_uftk_dos33.woz : %d Spuren mit Sektoren, "
           "%d Sektoren, %d Spuren mit Rohdaten, raw_bits %s\n",
           k_spuren, k_sek, k_roh, k_bits ? "gesetzt" : "NICHT gesetzt");
    CHECK(k_spuren > 0, "das Kontrollabbild liefert keine Spuren mit "
          "Sektoren — dann vergleicht dieser Test nichts");
    CHECK(k_sek == 560, "die Kontrolle liefert %d Sektoren, erwartet 560 "
          "(35 Spuren x 16, DOS 3.3)", k_sek);

    /* ── Die echten Aufnahmen ───────────────────────────────────────── */
    if (UFT_CORPUS_RESTRICTED_DIR[0] == '\0') {
        printf("\nSKIP: UFT_CORPUS_RESTRICTED_DIR nicht gesetzt — die "
               "echten Aufnahmen sind LOCAL-ONLY.\n");
        printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
        return g_fail ? 1 : 77;      /* SKIP_RETURN_CODE, MF-598 */
    }

    static const char *ECHT[] = {
        "wozaday_gauntlet_e7.woz",
        "wozaday_copy2plus_52.woz",
    };
    int geoeffnet = 0, bits_irgendwo = 0;
    for (size_t i = 0; i < sizeof ECHT / sizeof ECHT[0]; i++) {
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", UFT_CORPUS_RESTRICTED_DIR, ECHT[i]);
        int sek = 0, bits = 0, roh = 0;
        const int spuren = messen(p, &sek, &bits, &roh);
        if (!spuren && !roh) {
            printf("  (uebersprungen, nichts geliefert: %s)\n", ECHT[i]);
            continue;
        }
        geoeffnet++;
        if (bits) bits_irgendwo = 1;
        printf("ECHT %-26s : %d Spuren mit Sektoren, %d Sektoren, "
               "%d mit Rohdaten, raw_bits %s\n",
               ECHT[i], spuren, sek, roh, bits ? "gesetzt" : "NICHT gesetzt");

        /* Eine echte Aufnahme MUSS durch denselben Leser gehen wie das
         * erzeugte Abbild — sonst ist der Leser an seiner eigenen Ausgabe
         * geeicht und an nichts sonst. */
        CHECK(spuren >= 30,
              "%s liefert nur %d Spuren mit Sektoren, erwartet mindestens 30",
              ECHT[i], spuren);
    }

    /* BERICHTIGT (MF-1472; MF-1468 war hier rot in CI): hier stand ein CHECK, das
     * fiel, wenn keine Aufnahme aufging. Das war falsch.
     *
     * `tests/corpus/` ist GITIGNORED (.gitignore:153) — in CI existiert
     * das Verzeichnis nicht. Der Wächter oben prueft nur, ob das MAKRO
     * leer ist; CMake setzt es aber immer, auch auf einen Pfad, den es
     * dort nicht gibt. Folge: der Test fiel in CI in 0,02 s, und das
     * ASan-Tor meldete ihn als „NEU unter ASan fehlgeschlagen" — ein
     * Befund, der nach einem Speicherfehler aussah und keiner war.
     *
     * Ein fehlender LOCAL-ONLY-Korpus ist ein SKIP, kein Fehlschlag.
     * Sichtbar bleibt er trotzdem: Rueckgabe 77 (MF-598), nie 0. Die
     * Kontrolle oben hat vorher gemessen und gilt weiter — dieser Test
     * meldet also nicht „bestanden", wenn er nur die Haelfte lief. */
    if (geoeffnet == 0) {
        printf("\nSKIP: keine der echten Aufnahmen geoeffnet — sie sind "
               "LOCAL-ONLY und liegen in %s\n", UFT_CORPUS_RESTRICTED_DIR);
        printf("Die Kontrolle ist gelaufen: %d bestanden, %d "
               "fehlgeschlagen\n", g_pass, g_fail);
        return g_fail ? 1 : 77;      /* SKIP_RETURN_CODE, MF-598 */
    }

    /* ── FESTNAGELUNG des Verlusts (P3-634, MF-1468) ─────────────────────────────
     *
     * Ein WOZ speichert die Bitzahl JE SPUR ausdruecklich im TRKS-Block —
     * gemessen an diesen Dateien: 51505..51624 bzw. 51050..51211, also
     * 28 bzw. 33 verschiedene Werte. `uft_woz_plugin.c` setzt `raw_bits`
     * NIE (0 Nennungen im Quelltext). Damit wirft UFT eine Zahl weg, die
     * das Format LIEFERT — anders als bei G64/G71, wo der Behaelter nur
     * eine Bytezahl fuehrt und `bytes * 8` erfundene Bits waeren.
     *
     * Die Zusicherung steht UMGEDREHT: sie faellt, sobald jemand
     * `raw_bits` setzt. Das ist kein Schutz des Verlusts, sondern seine
     * Sichtbarkeit — wer ihn behebt, wird hierher geschickt und traegt
     * die neue Zusicherung ein. */
    CHECK(k_bits == 0 && bits_irgendwo == 0,
          "raw_bits ist jetzt gesetzt (Kontrolle %d, echte %d) — P3-634 ist "
          "damit behoben. Diese Festnagelung gehoert dann ersetzt durch "
          "eine Messung der Streuung: erzeugt genau 1 Laenge, echt 28 "
          "bzw. 33", k_bits, bits_irgendwo);

    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
