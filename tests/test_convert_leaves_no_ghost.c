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

/* Legt eine Datei mit vorgegebenem Inhalt an. */
static int put(const char *path, const uint8_t *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, n, f);
    fclose(f);
    return (w == n) ? 0 : -1;
}

/* Ein Durchgang: alte Datei hinlegen, wandeln, nachsehen was uebrig ist.
 *
 * @param label     was dieser Durchgang zeigen soll
 * @param src_path  Quelldatei (muss schon existieren)
 * @param dst_fmt   Zielformat
 * @param consent   accept_data_loss
 */
static void one_round(const char *label, const char *src_path,
                      uft_format_t dst_fmt, bool consent)
{
    const char *dst = "uft_ghost_out.hfe";

    /* Ein frueherer Lauf: eine Datei liegt schon da, plausibel gross. */
    static uint8_t old[65536];
    memset(old, 0xAB, sizeof(old));
    if (put(dst, old, sizeof(old)) != 0) {
        printf("  %s: Tempdatei nicht schreibbar\n", label);
        failures++;
        return;
    }
    long before = file_size(dst);

    /* ── MF-568: RICHTIGER Optionstyp ────────────────────────────────────
     *
     * Hier stand `uft_convert_options_ext_t`, waehrend
     * `uft_convert_file()` `uft_convert_options_t` nimmt. C hat das nur
     * als Warnung gemeldet, und die Layouts sind verschieden — gemessen:
     *
     *     uft_convert_options_t     96 Byte, accept_data_loss @ 72
     *     uft_convert_options_ext_t 104 Byte, accept_data_loss @ 64
     *
     * Das Einverstaendnis wurde also auf Offset 64 gesetzt und auf 72
     * gelesen: als NICHT gegeben. Der Test war gruen, weil nichts lief. */
    uft_convert_options_t o;
    uft_convert_result_t r;
    memset(&o, 0, sizeof(o));
    memset(&r, 0, sizeof(r));
    o.accept_data_loss = consent;

    uft_error_t e = uft_convert_file(src_path, dst, dst_fmt, &o, &r);

    printf("  %s\n", label);
    printf("    vorher %ld Byte da; Wandlung liefert %d, %d Spuren "
           "gewandelt / %d gescheitert\n",
           before, (int)e, r.tracks_converted, r.tracks_failed);

    if (e == UFT_OK || r.success) {
        printf("    FAIL: sie meldet Erfolg\n");
        failures++;
    } else {
        printf("    ok   sie meldet Misserfolg\n");
    }

    long after = file_size(dst);
    if (after >= 0) {
        printf("    FAIL: es liegt weiterhin eine Datei da (%ld Byte)\n",
               after);
        if (after == before)
            printf("          und zwar die ALTE — sie sieht aus wie das "
                   "Ergebnis\n");
        failures++;
        remove(dst);
    } else {
        printf("    ok   keine Datei zurueckgelassen, auch nicht die alte\n");
    }

    if (r.warning_count > 0)
        printf("    Begruendung: %s\n", r.warnings[0]);
    else {
        printf("    FAIL: kein Wort dazu, warum es nicht ging\n");
        failures++;
    }
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

    /* ── Fall 1: das Tor weist ab, bevor irgendetwas anfaengt ────────────
     *
     * Der urspruengliche Test hatte NUR diesen Fall — und er wusste es
     * nicht. Die Begruendung lautete „No conversion path from DSK to
     * HFE": die 901120-Byte-Datei mit Endung .adf wurde als DSK erkannt,
     * das Paar gibt es nicht, die Wandlung fing nie an.
     *
     * Der Fall ist es wert, gepinnt zu werden — aber er ist NICHT der,
     * um den es MF-545 ging. */
    const char *src_adf = "uft_ghost_in.adf";
    if (put(src_adf, src, ADF_SZ) != 0) {
        printf("  Quelldatei nicht schreibbar\n");
        return 2;
    }
    one_round("Fall 1 — abgewiesen, bevor etwas anfaengt:",
              src_adf, UFT_FORMAT_HFE, true);

    /* ── Fall 2: der Wandler LAEUFT und bringt nichts zustande ───────────
     *
     * Das ist der Fall, den MF-545 gebaut hat: `uftc_finish_or_refuse()`
     * greift, wenn `tracks_converted == 0` — also NACHDEM der Wandler
     * angefangen hat, seine Ausgabedatei anzulegen.
     *
     * Dafuer muss die Quelle als etwas erkannt werden, das einen offenen
     * Weg hat. Zwei Anlaeufe zuvor scheiterten daran, und zwar still:
     *
     *   901120 Byte mit Endung `.adf`  -> erkannt als DSK
     *      512 Byte mit Endung `.img`  -> erkannt als DSK
     *
     * Die Erkennung geht ueber den INHALT, nicht ueber die Endung, und
     * ein roher Sektorstrom ohne Signatur wird DSK. Beide Anlaeufe
     * meldeten „No conversion path from DSK to HFE" — der Wandler lief
     * nie an, und der Test war trotzdem gruen.
     *
     * Genommen wird darum eine ECHTE HFE aus dem Korpus: sie traegt die
     * Signatur „HXCPICFE", wird also sicher als HFE erkannt, und
     * HFE -> IMG ist angeboten. Abgeschnitten hinter dem Kopf findet der
     * Wandler keine einzige vollstaendige Spur — er faengt an und muss am
     * Ende zurueckziehen, samt der Datei, die schon dalag. */
    const char *src_hfe = "uft_ghost_in.hfe";
    {
        FILE *in = fopen(UFT_CORPUS_DIR "/gw_amigados.hfe", "rb");
        if (!in) {
            printf("  Fall 2 uebersprungen: gw_amigados.hfe nicht da\n");
            printf("  FAIL: ohne diese Datei misst Fall 2 nichts\n");
            failures++;
        } else {
            static uint8_t head[1024];
            size_t n = fread(head, 1, sizeof(head), in);
            fclose(in);
            if (n < 512) {
                printf("  FAIL: Korpusdatei zu klein (%zu Byte)\n", n);
                failures++;
            } else if (put(src_hfe, head, n) != 0) {
                printf("  FAIL: abgeschnittene HFE nicht schreibbar\n");
                failures++;
            } else {
                one_round("Fall 2 — der Wandler laeuft und bringt nichts "
                          "zustande:", src_hfe, UFT_FORMAT_IMG, true);
            }
        }
    }

    remove(src_adf);
    remove(src_hfe);

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
