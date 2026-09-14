/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_advanced_guete_ohne_messung.c
 * @brief Rotbeweis zu MF-1129 — wer nicht messen kann, sagt ab statt
 *        eine Guete zu erfinden
 *
 * ── Der Befund, gemessen am Vorzustand ────────────────────────────────────
 *
 * `uft_advanced_get_track_quality()` hatte fuer jede Spur, die es nicht
 * analysieren konnte, die HOECHSTE Guete 1.0 eingetragen und `UFT_OK`
 * gemeldet — mit dem Kommentar "Default to good". Ein entschuldigender
 * Kommentar macht einen erfundenen Wert nicht zu einer Schaetzung.
 *
 * `uft_advanced_get_stats()` hat diesen Wert dann gemittelt, und die
 * Rechnung geht restlos auf: `uft_advanced_analyze_disk()` faellt fuer
 * jedes Format ausser D64/G64/SCP auf `cyls = 35, heads = 1` zurueck,
 * meldet also **35** Spuren; der alte Pfad legte 35
 * `uft_track_quality_t` an, liess sie mit dem Vorgabewert fuellen und
 * teilte `35.0 / 35` — **genau 1.0**. Eine Diskette, von der keine
 * einzige Spur gelesen wurde, bekam die Durchschnittsguete 100 %.
 *
 * Dazu zwei Ersatzwerte auf demselben Rueckweg:
 *
 *   - schlug `calloc` fehl: `average_quality = 0.95;` mit dem Kommentar
 *     "Estimate without per-track data";
 *   - war `track_count == 0`: `average_quality = 1.0;` — keine Daten,
 *     bestes Urteil.
 *
 * Und ein dritter Fehler im selben `if`: der Waechter `track_count > 0`
 * prueft den Wert VOR dem zweiten `uft_advanced_analyze_disk()`-Aufruf,
 * der `track_count` neu schreibt. Geprueft wurde ein veralteter Wert und
 * danach durch den neuen geteilt.
 *
 * ── Warum dieser Test in BEIDE Richtungen messen muss ─────────────────────
 *
 * Ein Test, der nur die Absage prueft, ist gruen, wenn die Funktion
 * **immer** absagt — dann waere aus einem erfundenen Wert eine
 * unbrauchbare Funktion geworden, und das ist keine Verbesserung
 * (Klasse MF-444: eine Mess-API, die nicht messen kann, wird entfernt,
 * nicht liegen gelassen). Abschnitt 3 oeffnet deshalb ein ECHTES Abbild
 * aus dem Korpus.
 *
 * **Und die Gegenrichtung hat einen Befund geliefert, nicht eine
 * Bestaetigung.** Die erste Fassung dieses Tests verlangte dort
 * `measured_tracks > 0` und wurde ROT. Gemessen, in drei Schritten:
 *
 *   1. `using_v3` war 0, weil der v3-Pfad an `UFT_ADV_USE_V3_PARSERS`
 *      haengt und `g_config` als `{0}` startet. Der Test setzt den
 *      Schalter jetzt selbst.
 *   2. Danach war die Geometrie echt (42 Spuren statt des Rueckfalls
 *      35), `measured_tracks` aber weiter 0 — weil `get_stats` und
 *      `get_track_quality` ihren Handler ueber
 *      `uft_format_get_handler(handle->format_id)` holten, einen
 *      Typwechsel ohne Umrechnung zwischen `internal_format_t` und
 *      `uft_format_t`. Eine **G64** fragte damit den **IMG**-Handler,
 *      eine D64 den **RAW**-, eine SCP den **ADF**-Handler. Behoben in
 *      MF-1129 mit EINER Zuordnungsstelle (`advanced_handler()`).
 *   3. Und dann blieb es trotzdem 0, aus dem eigentlichen Grund:
 *      `uft_format_handler_t::read_track` existiert
 *      (`include/uft/uft_formats_extended.h:56`) und wird von **keiner
 *      einzigen** Datei in `src/` gesetzt. Die drei erreichbaren
 *      Handler in `src/formats/uft_v3_bridge.c` fuehren `open`, `close`
 *      und `get_geometry`, sonst nichts; die 93 Dateien mit
 *      `.read_track` gehoeren zum ANDEREN Typ `uft_format_plugin_t`.
 *
 * `uft_advanced_get_track_quality()` konnte also **nie** messen, und die
 * erfundene 1.0 mit `UFT_OK` war das Einzige, was sie lebendig aussehen
 * liess — sie hat den fehlenden Lesepfad verdeckt. Was damit geschieht,
 * ist eine Eigentuemerentscheidung (MF-1077). Abschnitt 3 nagelt den
 * Zustand deshalb als **Rotbeweis in Wartestellung** fest: er verlangt
 * `read_track == NULL` an allen drei Handlern. Wird einer verdrahtet,
 * faellt die Zusage und zwingt denselben Commit, die echte
 * Messrichtung aufzunehmen.
 *
 * Ohne Korpus ueberspringt sich Abschnitt 3 **benannt** (MF-598: ein
 * stiller Skip ist ein gruenes Licht ohne Messung).
 *
 * ── Was hier ausdruecklich NICHT geprueft wird ────────────────────────────
 *
 * `uft_track_quality_t` traegt kein Feld `is_measured`, und es wird
 * absichtlich keines angehaengt: der Typ ist im Baum DREIMAL definiert
 * (Enum in `include/uft/core/uft_track_base.h`, diese Struktur in
 * `include/uft/uft_advanced_mode.h`, eine ANDERE Struktur in
 * `include/uft/uft_track.h`) und alle drei haengen am selben Waechter
 * `UFT_TRACK_QUALITY_T_DEFINED`. Der Nenner steht deshalb in
 * `uft_advanced_stats_t.measured_tracks`, einer Struktur mit genau EINER
 * Definition. Der Dreifach-Typ selbst ist ein eigener offener Punkt.
 *
 * Ebenso nicht geprueft: dass `stats->total_tracks` bei einem
 * nicht-v3-Handle **35** meldet. Diese 35 ist der fest verdrahtete
 * Rueckfall in `uft_advanced_analyze_disk()` und damit selbst ein
 * erfundener Wert derselben Klasse — er wird hier festgenagelt, damit
 * eine Aenderung auffaellt, aber NICHT als richtig behauptet.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_advanced_mode.h"
#include "uft/uft_formats_extended.h"

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                      \
    do {                                                                       \
        if (bed) {                                                             \
            gruen++;                                                           \
            printf("   [ok ] %s\n", (text));                                   \
        } else {                                                               \
            rot++;                                                             \
            printf("   [ROT] %s\n", (text));                                   \
        }                                                                      \
        assert(bed);                                                           \
    } while (0)

/* Ein Handle, hinter dem NICHTS steht. Die Struktur ist oeffentlich und
 * durchsichtig (`include/uft/uft_advanced_mode.h`), also braucht dieser
 * Test keinen Nachbau des Produktionspfades — er benutzt ihn. */
static void handle_leer(uft_advanced_handle_t *h) {
    memset(h, 0, sizeof(*h));
    h->using_v3 = false;     /* nichts zu analysieren */
    h->v3_handle = NULL;
    h->handle = NULL;
    h->format_id = 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * 1) Die Messfunktion sagt ab, statt 1.0 zu behaupten
 * ══════════════════════════════════════════════════════════════════════ */
static void absage_je_spur(void) {
    uft_advanced_handle_t h;
    handle_leer(&h);

    /* 0xAA vorbelegen: wuerde die Funktion das Feld nicht anfassen,
     * bliebe hier Muell stehen und der Test faende es. */
    uft_track_quality_t q;
    memset(&q, 0xAA, sizeof(q));

    const uft_error_t e = uft_advanced_get_track_quality(&h, 7, 1, &q);

    ZUSAGE(e != UFT_OK,
           "ohne Analysepfad ist der Rueckgabewert NICHT UFT_OK");
    ZUSAGE(q.quality == 0.0,
           "die Guete bleibt 0.0 statt des alten Vorgabewerts 1.0");
    ZUSAGE(q.is_weak == false && q.has_errors == false &&
           q.error_count == 0 && q.recovered_bits == 0,
           "alle uebrigen Felder sind durch das memset genullt");
    ZUSAGE(q.cylinder == 7 && q.head == 1,
           "Zylinder und Kopf werden trotzdem gesetzt (die Frage ist "
           "beantwortet, nur nicht die Guete)");

    /* Nullzeiger auf beiden Wegen */
    ZUSAGE(uft_advanced_get_track_quality(NULL, 0, 0, &q) != UFT_OK,
           "Nullhandle wird abgewiesen");
    ZUSAGE(uft_advanced_get_track_quality(&h, 0, 0, NULL) != UFT_OK,
           "Nullausgabe wird abgewiesen");
}

/* ══════════════════════════════════════════════════════════════════════
 * 2) Die Statistik erfindet keinen Durchschnitt
 * ══════════════════════════════════════════════════════════════════════ */
static void statistik_ohne_messung(void) {
    uft_advanced_handle_t h;
    handle_leer(&h);

    uft_advanced_stats_t s;
    memset(&s, 0xAA, sizeof(s));

    uft_advanced_get_stats(&h, &s);

    printf("   total_tracks=%d measured_tracks=%d average_quality=%.4f\n",
           s.total_tracks, s.measured_tracks, s.average_quality);

    ZUSAGE(s.measured_tracks == 0,
           "measured_tracks ist 0 — es wurde nichts gemessen");
    ZUSAGE(s.average_quality == 0.0,
           "average_quality ist 0.0 statt der alten 1.0 (35 erfundene "
           "Einsen durch 35 Spuren)");
    ZUSAGE(s.average_quality != 0.95,
           "und auch nicht der alte Ersatzwert 0.95");
    ZUSAGE(isfinite(s.average_quality),
           "average_quality ist eine endliche Zahl (kein NaN aus 0/0)");

    /* Der fest verdrahtete Geometrie-Rueckfall wird festgenagelt, NICHT
     * als richtig behauptet — siehe Dateikopf. */
    ZUSAGE(s.total_tracks == 35,
           "total_tracks meldet den Rueckfall 35 (eigener offener Punkt, "
           "hier nur festgehalten)");

    /* Die Kernaussage in einem Satz: eine Zahl ohne Nenner ist keine
     * Auskunft. Wer average_quality liest, muss measured_tracks lesen. */
    ZUSAGE(!(s.measured_tracks == 0 && s.average_quality > 0.0),
           "es gibt keinen Zustand 'kein Nenner, aber ein Mittelwert'");

    uft_advanced_get_stats(NULL, &s);   /* darf nicht abstuerzen */
    uft_advanced_get_stats(&h, NULL);
    gruen++;
    printf("   [ok ] Nullzeiger auf beiden Wegen stuerzen nicht ab\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * 3) Gegenrichtung: an einem ECHTEN Abbild MUSS gemessen werden
 * ══════════════════════════════════════════════════════════════════════ */
static const char *KORPUS[] = {
    "tests/corpus/c64pp_aliensyndrome.g64",
    "tests/corpus/c64pp_bountybob.g64",
    "tests/corpus/gw_amigados.scp",
    "../tests/corpus/c64pp_aliensyndrome.g64",
    "../tests/corpus/gw_amigados.scp",
};

static void echte_messung(void) {
    const char *pfad = NULL;
    for (size_t i = 0; i < sizeof(KORPUS) / sizeof(KORPUS[0]); i++) {
        FILE *f = fopen(KORPUS[i], "rb");
        if (f) { fclose(f); pfad = KORPUS[i]; break; }
    }
    if (!pfad) {
        printf("   [uebersprungen] kein Korpus-Abbild gefunden — die "
               "Gegenrichtung ist damit UNGEMESSEN, nicht gruen\n");
        return;
    }

    /* Der v3-Pfad haengt an einem Schalter, und `g_config` ist
     * `{0}` — ohne dieses Setzen nimmt `uft_advanced_open()` fuer
     * KEIN Format einen v3-Parser, setzt `h->handle` nicht (der
     * Rueckfall steht dort nur als Kommentar „Could fall back to
     * standard parser here") und gibt ein leeres Handle heraus.
     *
     * Gemessen beim ersten Lauf dieses Tests: eine echte G64 kam mit
     * `using_v3 = 0` zurueck, und `measured_tracks` blieb 0. Der Test
     * ist daran ROT geworden, und das war richtig — genau so sieht
     * eine Mess-API aus, die nicht messen kann (MF-444). Die Ursache
     * war nicht MF-1129, sondern die Vorgabe. Sie steht hier, damit
     * die Gegenrichtung wirklich messbar ist, statt den Anspruch zu
     * senken. */
    uft_advanced_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.flags = UFT_ADV_USE_V3_PARSERS;
    uft_advanced_set_config(&cfg);

    uft_advanced_handle_t *h = NULL;
    if (uft_advanced_open(pfad, &h) != UFT_OK || !h) {
        printf("   [uebersprungen] %s liess sich nicht oeffnen — "
               "Gegenrichtung UNGEMESSEN\n", pfad);
        return;
    }
    if (!h->using_v3) {
        printf("   [ROT] %s wurde geoeffnet, aber using_v3=0 trotz "
               "gesetztem UFT_ADV_USE_V3_PARSERS — der v3-Parser hat "
               "die Datei abgewiesen (format_id=%d)\n",
               pfad, h->format_id);
        rot++;
        uft_advanced_close(h);
        return;
    }
    printf("   Abbild: %s (using_v3=%d, format_id=%d)\n",
           pfad, (int)h->using_v3, h->format_id);

    uft_advanced_stats_t s;
    memset(&s, 0xAA, sizeof(s));
    uft_advanced_get_stats(h, &s);

    printf("   total_tracks=%d measured_tracks=%d average_quality=%.4f\n",
           s.total_tracks, s.measured_tracks, s.average_quality);

    /* Die Geometrie ist ECHT — 42 Spuren aus dem Abbild, nicht der
     * Rueckfall 35. Das trennt diesen Abschnitt von Abschnitt 2. */
    ZUSAGE(s.total_tracks > 35,
           "die Spurzahl kommt aus dem Abbild (nicht der Rueckfall 35)");
    ZUSAGE(s.measured_tracks <= s.total_tracks,
           "es werden nie mehr Spuren gemessen als es gibt");
    ZUSAGE(isfinite(s.average_quality) &&
           s.average_quality >= 0.0 && s.average_quality <= 1.0,
           "der Mittelwert ist endlich und im Wertebereich 0..1");

    /* Und je Spur: keine erfundene Guete, auch nicht an einem echten
     * Abbild. */
    uft_track_quality_t q;
    memset(&q, 0xAA, sizeof(q));
    const uft_error_t qe = uft_advanced_get_track_quality(h, 0, 0, &q);
    printf("   get_track_quality(0,0) -> %d, quality=%.4f\n",
           (int)qe, q.quality);
    ZUSAGE(qe != UFT_OK,
           "auch an einem echten Abbild wird nicht 1.0/UFT_OK erfunden");
    ZUSAGE(q.quality == 0.0,
           "die Guete bleibt 0.0");

    /* ── Und hier steht der eigentliche Befund, festgenagelt ─────────
     *
     * `measured_tracks` ist 0, obwohl das Abbild geoeffnet ist, der
     * v3-Parser laeuft und die Geometrie echt ist. Der Grund ist
     * STRUKTURELL und hat mit MF-1129 nichts zu tun:
     *
     *   `uft_advanced_get_track_quality()` liest ueber
     *   `uft_format_handler_t::read_track`. Dieses Feld gibt es
     *   (`include/uft/uft_formats_extended.h:56`) — und gemessen ueber
     *   `git ls-files` setzt es KEINE EINZIGE Datei in `src/`. Die drei
     *   erreichbaren Handler in `src/formats/uft_v3_bridge.c` fuehren
     *   `open`, `close` und `get_geometry`, sonst nichts. Die 93 Dateien
     *   mit `.read_track` gehoeren zum ANDEREN Typ,
     *   `uft_format_plugin_t`.
     *
     * Die Funktion konnte also NIE messen. Die erfundene 1.0 mit
     * `UFT_OK` war das Einzige, was sie lebendig aussehen liess — sie
     * hat den fehlenden Lesepfad VERDECKT. Das ist die Klasse MF-444
     * („eine Mess-API, die nicht messen kann, ist kein unfertiges
     * Feature"), und was damit geschieht, ist eine
     * Eigentuemerentscheidung (MF-1077: nicht loeschen, sondern die
     * Auswahl liefern).
     *
     * Die Zusage unten ist ein ROTBEWEIS IN WARTESTELLUNG: sobald
     * jemand `read_track` an einem dieser Handler verdrahtet, faellt
     * sie — und zwingt denselben Commit, die Gegenrichtung oben
     * (`measured_tracks > 0`) mit aufzunehmen. Ohne sie waere dieser
     * Test gruen, waehrend die Messfunktion still nichts tut. */
    extern uft_format_handler_t uft_d64_v3_handler;
    extern uft_format_handler_t uft_g64_v3_handler;
    extern uft_format_handler_t uft_scp_v3_handler;

    ZUSAGE(uft_g64_v3_handler.read_track == NULL &&
           uft_d64_v3_handler.read_track == NULL &&
           uft_scp_v3_handler.read_track == NULL,
           "ALLE drei v3-Handler haben read_track == NULL — deshalb ist "
           "measured_tracks 0. Faellt diese Zusage, ist ein Lesepfad "
           "verdrahtet und die Gegenrichtung gehoert in denselben Commit");
    ZUSAGE(s.measured_tracks == 0,
           "und genau deshalb ist measured_tracks hier 0 — die Zahl "
           "LUEGT nicht, sie sagt 'nicht gemessen'");

    uft_advanced_close(h);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("MF-1129 — wer nicht messen kann, sagt ab\n");

    printf("\n1) Absage je Spur statt Vorgabewert 1.0\n");
    absage_je_spur();

    printf("\n2) Statistik ohne Messung: kein erfundener Durchschnitt\n");
    statistik_ohne_messung();

    printf("\n3) Gegenrichtung an einem echten Abbild\n");
    echte_messung();

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
