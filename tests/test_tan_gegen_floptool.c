/**
 * @file test_tan_gegen_floptool.c
 * @brief `tan` gegen das floptool-Artefakt — eine Eigentuemer-
 *        Entscheidung, ausgefuehrt und offen benannt (MF-1104)
 *
 * ── Was hier entschieden wurde, und von wem ──────────────────────────
 *
 * **P3-365 hat diesen Schritt seit MF-1085 ausdruecklich NICHT gemacht**
 * und ihn als Eigentuemer-Entscheidung mit drei Wegen hingelegt. Der
 * Eigentuemer hat am 2026-09-13 Weg (a) gewaehlt: *"mach mit den
 * formaten weiter, ohne rueckfragen, bis alle t1b haben"*.
 *
 * Dieser Test fuehrt das aus. Er steht hier mit der Begruendung, die
 * P3-365 dagegen hatte, weil die weiterhin gilt.
 *
 * ── Die Sachlage, gemessen ───────────────────────────────────────────
 *
 * TAN (Tandy TRS-80) ist ein kopfloser Sektorabzug **in JV1-Anordnung**:
 * 10 Sektoren je Spur, 256 Byte, **eine** Seite, Sektornummern ab 0.
 * Belegt gegen MAMEs `jv1_format::formats[]` und Tim Manns
 * Formatbeschreibung.
 *
 * Damit ist `tests/corpus_free/floptool_jv1_80spuren.jv1` byteweise
 * **zugleich ein gueltiges TAN-Abbild** — dieselben 204 800 Byte, nur
 * eine andere Endung. Gemessen: 800 Marken `UFT-JV1 #NNNN`, eine je
 * Sektor, Rest 0xE5.
 *
 * **Deshalb wird KEINE zweite Datei angelegt.** Das waere dasselbe
 * Artefakt zweimal. Die Lage ist die von MF-1033 (`nanowasp`), wo zwei
 * unabhaengige Schreiber dieselben 409 600 Byte erzeugten und die
 * GLEICHHEIT der Beleg war, nicht eine Kopie. Das Manifest traegt einen
 * zweiten Eintrag auf dieselbe Datei.
 *
 * ── Was dieser Test BELEGT ───────────────────────────────────────────
 *
 * Dass UFTs `tan`-Leser ein JV1-angeordnetes Abbild von fremder Hand
 * richtig liest: 80 Zylinder auf EINEM Kopf, Sektornummern ab 0, und
 * jede der 800 Marken an ihrer eigenen Stelle.
 *
 * Das Abbild kommt aus MAMEs floptool und ist dort ueber einen
 * Zellstrom gelaufen (MF-1085) — ein Durchreichen der Bytes ist
 * ausgeschlossen.
 *
 * ── Was er NICHT belegt, und das ist der Kern von P3-365 ─────────────
 *
 * **Dass die Dateien, die im Feld als `.tan` kursieren, wirklich diese
 * Anordnung tragen.** Dafuer gibt es keine zweite Quelle. Der Beleg
 * sagt: "UFT liest ein JV1-angeordnetes Abbild richtig" — und genau das
 * steht seit MF-1085 schon bei `jv1`.
 *
 * Wer diese Stufe liest, liest also eine Aussage ueber den LESER, nicht
 * ueber das Format draussen. Das ist weniger, als eine T1b-Zeile sonst
 * verspricht, und es steht deshalb hier, im Manifest und in P3-365.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/uft_format_common.h"

extern const uft_format_plugin_t uft_format_plugin_tan;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif
#define ABBILD UFT_CORPUS_DIR "/floptool_jv1_80spuren.jv1"

static int zusagen = 0;
static int gefallen = 0;

static void pruefe(const char *was, int bedingung)
{
    zusagen++;
    if (bedingung) {
        printf("  [ok]  %s\n", was);
    } else {
        printf("  [ROT] %s\n", was);
        gefallen++;
    }
}

int main(void)
{
    FILE *roh = fopen(ABBILD, "rb");
    if (!roh) {
        printf("SKIP: %s fehlt\n", ABBILD);
        return 77;
    }
    if (fseek(roh, 0, SEEK_END) != 0) { fclose(roh); return 1; }
    long groesse = ftell(roh);
    printf("Abbild: %ld Byte\n", groesse);
    pruefe("204 800 Byte = 80 x 10 x 256, EINE Seite", groesse == 204800);

    uint8_t kopf[4096];
    if (fseek(roh, 0, SEEK_SET) != 0) { fclose(roh); return 1; }
    size_t gelesen = fread(kopf, 1, sizeof kopf, roh);
    int konfidenz = -1;
    bool erkannt = uft_format_plugin_tan.probe(kopf, gelesen,
                                               (size_t)groesse, &konfidenz);
    pruefe("Sonde erkennt das Abbild", erkannt);
    /* MF-729: erkannt ist allein die Dateigroesse. Eine hoehere Zahl
     * waere hier eine Zusage ohne Deckung — TAN hat keine Kennung. */
    pruefe("Konfidenz im Band \"nur die Groesse\" (30-49)",
           konfidenz >= 30 && konfidenz <= 49);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t e = uft_format_plugin_tan.open(&disk, ABBILD, true);
    pruefe("open gelingt", e == UFT_OK);
    if (e != UFT_OK) {
        fclose(roh);
        printf("\n%d von %d Zusagen gehalten\n", zusagen - gefallen, zusagen);
        return 1;
    }

    /* Die beiden Befunde aus MF-1026, an denen `tan` woertlich
     * gescheitert ist: eine erfundene zweite Seite und Sektor-IDs ab 1. */
    pruefe("80 Zylinder", disk.geometry.cylinders == 80);
    pruefe("EIN Kopf — keine erfundene zweite Seite (MF-1026)",
           disk.geometry.heads == 1);
    pruefe("10 Sektoren je Spur", disk.geometry.sectors == 10);
    pruefe("256 Byte je Sektor", disk.geometry.sector_size == 256);

    size_t treffer = 0, daneben = 0, ohne_daten = 0, id_ab_null = 0;
    for (unsigned cyl = 0; cyl < disk.geometry.cylinders; cyl++) {
        uft_track_t spur;
        memset(&spur, 0, sizeof spur);
        if (uft_format_plugin_tan.read_track(&disk, (int)cyl, 0, &spur)
            != UFT_OK) { daneben += 10; continue; }
        for (size_t s = 0; s < spur.sector_count; s++) {
            size_t nummer = cyl * 10 + s;
            char marke[24];
            snprintf(marke, sizeof marke, "UFT-JV1 #%04zu", nummer);

            const uint8_t *ist = spur.sectors[s].data;
            if (!ist || spur.sectors[s].data_len != 256) { ohne_daten++; continue; }
            if (memcmp(ist, marke, strlen(marke)) == 0) treffer++; else daneben++;
            if (spur.sectors[s].id.sector == (uint8_t)s) id_ab_null++;
        }
    }
    printf("  Marken an ihrer Stelle: %zu  abweichend: %zu  ohne Daten: %zu\n",
           treffer, daneben, ohne_daten);
    printf("  Sektornummern ab 0: %zu von 800\n", id_ab_null);

    pruefe("alle 800 Marken an ihrer eigenen Stelle", treffer == 800);
    pruefe("keine abweichend", daneben == 0);
    pruefe("Sektornummern beginnen bei 0, nicht bei 1 (MF-1026)",
           id_ab_null == 800);

    /* Gegenprobe: die Marke der NACHBARSPUR darf nirgends passen.
     * Ohne sie waere "800 von 800" auch dann gruen, wenn der Leser
     * jede Spur an dieselbe Stelle legte. */
    size_t falsch_gleich = 0;
    uft_track_t s1;
    memset(&s1, 0, sizeof s1);
    if (uft_format_plugin_tan.read_track(&disk, 1, 0, &s1) == UFT_OK) {
        for (size_t s = 0; s < s1.sector_count; s++) {
            char falsche[24];
            snprintf(falsche, sizeof falsche, "UFT-JV1 #%04zu", s); /* Spur 0 */
            if (s1.sectors[s].data &&
                memcmp(s1.sectors[s].data, falsche, strlen(falsche)) == 0)
                falsch_gleich++;
        }
    }
    printf("  Gegenprobe (Spur 1 gegen Spur-0-Marken): %zu gleich\n",
           falsch_gleich);
    pruefe("Spur 1 traegt NICHT die Marken von Spur 0", falsch_gleich == 0);

    uft_format_plugin_tan.close(&disk);
    fclose(roh);

    printf("\n%d von %d Zusagen gehalten\n", zusagen - gefallen, zusagen);
    return gefallen == 0 ? 0 : 1;
}
