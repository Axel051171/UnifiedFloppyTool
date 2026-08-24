/**
 * @file test_convert_leaves_no_ghost.c
 * @brief Eine gescheiterte Wandlung darf keine Datei hinterlassen (MF-545)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * Die Wandler-Schicht hatte an vierzehn Stellen dieselbe Bauart:
 *
 *     uft_error_t err = uftc_write_output_file(dst, buf, size);
 *     if (err == UFT_OK) { result->success = true; ... }
 *
 * `success` folgte allein daraus, dass sich die Datei ANLEGEN liess — nicht
 * daraus, dass etwas drinstand. Zweimal gemessen:
 *
 *   MF-538  HFE -> ADF: 160 von 160 Spuren gescheitert, kein Sektor
 *           platziert, 901120 Byte Nullen geschrieben, UFT_OK.
 *   MF-539  ADF -> HFE: 80 Spuren und 1760 Sektoren als "gewandelt"
 *           gemeldet fuer eine Datei ohne eine einzige Synchronmarke.
 *
 * Seit MF-545 geht das ueber `uftc_finish_or_refuse()`: wurde keine
 * einzige Spur gewandelt, wird NICHTS geschrieben.
 *
 * ── Und die Falle, die genau dadurch entstand ────────────────────────────
 *
 * Vorher ueberschrieb der Wandler die Zieldatei immer. Jetzt nicht mehr —
 * also bliebe ein Ergebnis eines FRUEHEREN Laufs liegen, mit passendem
 * Namen und plausibler Groesse. Wer nach dem Lauf ins Verzeichnis schaut,
 * sieht seine Datei und haelt sie fuer das Ergebnis.
 *
 * Das waere schlimmer als der Zustand davor: die Nulldatei war wenigstens
 * erkennbar leer. Deshalb loescht `uftc_finish_or_refuse()` bei Ablehnung
 * eine vorhandene Zieldatei.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Zwei Eigenschaften, und die zweite ist die, die man vergisst:
 *
 *   1. eine gescheiterte Wandlung meldet nicht UFT_OK
 *   2. sie laesst keine Datei zurueck — auch keine ALTE
 *
 * Gefahren wird `ADF -> HFE`, weil dieser Pfad seit MF-539 sicher ablehnt
 * (dem Baum fehlt ein AmigaDOS-Encoder) und damit einen verlaesslichen
 * Fehlschlag liefert, ohne dass der Test etwas beschaedigen muesste.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Gefahren wird ueber uft_convert_file(), den Engpass, den auch die
 * Oberflaeche benutzt — nicht ueber den internen Einstieg.
 *
 * Der erste Anlauf dieses Tests rief `uftc_convert_sectors_to_hfe()` direkt
 * und fand damit nur EINE der Ablehnungsarten. Es gibt aber mehrere Wege,
 * ohne Erfolg anzukommen: das Preflight-Tor weist ein ungeprueftes Paar ab,
 * ein Wandler lehnt frueh ab, der Zaehler-Baustein verweigert das
 * Schreiben. Nur der Engpass sieht sie alle — und nur er ist der Weg, den
 * ein Benutzer nimmt. */

#define ADF_SZ  901120
static uint8_t src[ADF_SZ];

static int failures;

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("=== Eine gescheiterte Wandlung hinterlaesst nichts (MF-545) ===\n");

    for (size_t i = 0; i < ADF_SZ; i++)
        src[i] = (uint8_t)(i * 5 + (i >> 9));

    const char *dst = "uft_ghost_out.hfe";

    /* Ein frueherer Lauf: eine Datei liegt schon da, plausibel gross. */
    {
        FILE *f = fopen(dst, "wb");
        if (!f) { printf("  Tempdatei nicht schreibbar\n"); return 2; }
        static uint8_t old[65536];
        memset(old, 0xAB, sizeof(old));
        fwrite(old, 1, sizeof(old), f);
        fclose(f);
    }
    long before = file_size(dst);
    printf("  vorher liegt da: %ld Byte (Ergebnis eines frueheren Laufs)\n",
           before);

    uft_convert_options_ext_t o;
    uft_convert_result_t r;
    memset(&o, 0, sizeof(o));
    memset(&r, 0, sizeof(r));
    o.accept_data_loss = true;

    /* Quelle als Datei, wie im echten Betrieb. */
    const char *src_path = "uft_ghost_in.adf";
    {
        FILE *f = fopen(src_path, "wb");
        if (!f) { printf("  Quelldatei nicht schreibbar\n"); return 2; }
        fwrite(src, 1, ADF_SZ, f);
        fclose(f);
    }

    uft_error_t e = uft_convert_file(src_path, dst, UFT_FORMAT_HFE, &o, &r);

    printf("  Wandlung liefert %d, %d Spuren gewandelt / %d gescheitert\n",
           (int)e, r.tracks_converted, r.tracks_failed);

    if (e == UFT_OK) {
        printf("  FAIL: die Wandlung meldet Erfolg\n");
        failures++;
    } else {
        printf("  ok   sie meldet Misserfolg\n");
    }
    if (r.success) {
        printf("  FAIL: result->success ist gesetzt\n");
        failures++;
    } else {
        printf("  ok   result->success ist nicht gesetzt\n");
    }

    long after = file_size(dst);
    if (after >= 0) {
        printf("  FAIL: es liegt weiterhin eine Datei da (%ld Byte)\n", after);
        if (after == before)
            printf("        und zwar die ALTE — sie sieht aus wie das "
                   "Ergebnis\n");
        failures++;
        remove(dst);
    } else {
        printf("  ok   keine Datei zurueckgelassen, auch nicht die alte\n");
    }

    if (r.warning_count > 0)
        printf("  Begruendung: %s\n", r.warnings[0]);
    else {
        printf("  FAIL: kein Wort dazu, warum es nicht ging\n");
        failures++;
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
