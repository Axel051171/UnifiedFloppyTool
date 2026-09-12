/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_kryoflux_dtc_befehl.cpp
 * @brief Der DTC-Befehl gegen das Handbuch des Urhebers (MF-1046)
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 *
 * KryoFlux Manual, (c) 2009-2024 KryoFlux Products and Services Ltd,
 * Abschnitt „DTC offers the following command line options".
 * Kanal *Spec* nach MF-695: die Beschreibung wird GELESEN, es wird
 * keine Zeile Code uebernommen — DTC ist proprietaer.
 *
 * Die hier gepruefte Bedeutung jeder Option, woertlich aus dem Handbuch:
 *
 *     -f<name> : set filename
 *     -i<type> : set image type
 *     -d<id>   : select drive (default 0)
 *     -s<trk>  : set start track (default at least 0)
 *     -e<trk>  : set end track (default at most 83)
 *     -b<trk>  : set side 1/b track0 physical position (default 0)
 *     -g<side> : set single sided mode
 *                0=side 0, 1=side 1, 2=both sides
 *     -k<step> : set track distance
 *                1=80 tracks, 2=40 tracks (default 1)
 *     -t<try>  : set number of retries per track, min 1 (default 5)
 *     -p       : create path
 *     -c<mode> : read calibration mode
 *
 * ── Der Befund, der diesen Test ausgeloest hat ───────────────────────
 *
 * `KryoFluxProviderV2::build_read_argv()` baut den Befehl, den der
 * PRODUKTIONS-Lesepfad absetzt (`do_read_raw_flux`, ueber
 * `hardwaretab.cpp:842` mit einem echten QProcess-Laeufer seit MF-256).
 * Er lautete:
 *
 *     dtc -c2 -d0 -s<kopf> -b<zylinder> -e<zylinder> -f<praefix> -i0
 *
 * Gegen das Handbuch gehalten sind davon zwei Argumente an der
 * falschen Option, und eines fehlt:
 *
 *   * `-s<kopf>`      — `-s` ist die START-SPUR, nicht die Seite.
 *                       Kopf 1 setzte damit Startspur 1.
 *   * `-b<zylinder>`  — `-b` ist die physische Track-0-Position von
 *                       SEITE 1, ein Justageparameter. Dort landete die
 *                       Zylindernummer.
 *   * `-g` fehlte     — ohne Seitenwahl gilt „default auto", also
 *                       BEIDE Seiten.
 *
 * Zylinder 10 / Kopf 1 zu lesen hiess damit: Spuren 1 bis 10, beide
 * Seiten, mit der Track-0-Position von Seite 1 auf 10 verstellt.
 *
 * **Das Tueckische ist, dass jedes falsche Argument eine GUELTIGE andere
 * Option trifft.** DTC haette den Befehl angenommen und etwas anderes
 * getan — keine Fehlermeldung, kein Abbruch. Genau die Gestalt, gegen
 * die der Grundsatz „Keine stille Veraenderung" steht.
 *
 * ── Was dieser Test NICHT prueft ─────────────────────────────────────
 *
 * Er prueft nicht, was ein echtes DTC mit dem Befehl tut. Das waere eine
 * Messung am Geraet (es gibt keines, MF-310) oder am echten DTC (der
 * liegt nicht im Baum). Geprueft wird, ob die gebaute Befehlszeile die
 * Bedeutung traegt, die das Handbuch ihren Optionen gibt — das ist eine
 * reine Funktion von (Zylinder, Kopf, Praefix) und ohne Hardware
 * abnehmbar.
 *
 * Beobachtet wird ueber `SubprocessMock::recorded_runs()`: der Provider
 * bekommt einen Laeufer untergeschoben, der die argv mitschreibt,
 * statt einen Prozess zu starten.
 */
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

#include "hardware_providers/kryoflux_provider_v2.h"
#include "mock_hardware/subprocess_mock.h"

/* Der ZWEITE Befehlsbauer im Baum — die C-Huelle. Bis MF-1046 war er
 * `static` und damit nicht abnehmbar; genau deshalb blieben dort fuenf
 * Abweichungen unbemerkt. */
extern "C" {
#include "uft/hal/uft_kryoflux.h"
}

using namespace uft::hal;
using uft::tests::mocks::SubprocessMock;

static int g_fail = 0;

#define PRUEFE(bed, text)                                                   \
    do {                                                                    \
        if (!(bed)) {                                                       \
            std::printf("  FAIL @ %d: %s\n", __LINE__, (text));             \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

static KryoFluxProviderV2::DtcRunner make_runner(SubprocessMock& mock) {
    return [&mock](const std::vector<std::string>& argv,
                   const std::string& stdin_data) -> DtcRunResult {
        auto r = mock.run(argv, stdin_data);
        return DtcRunResult{ r.stdout_text, r.stderr_text, r.exit_code };
    };
}

static bool hat(const std::vector<std::string>& argv, const std::string& tok) {
    return std::find(argv.begin(), argv.end(), tok) != argv.end();
}

/** Traegt die argv irgendein Argument, das mit `praefix` beginnt? */
static bool hat_praefix(const std::vector<std::string>& argv,
                        const std::string& praefix) {
    for (const auto& a : argv) {
        if (a.size() >= praefix.size() &&
            a.compare(0, praefix.size(), praefix) == 0) {
            return true;
        }
    }
    return false;
}

static std::string zeige(const std::vector<std::string>& argv) {
    std::string s;
    for (const auto& a : argv) { s += a; s += ' '; }
    return s;
}

/* Ein minimaler, gueltiger KryoFlux-Strom: drei Flussbytes, ein
 * Index-OOB und ein End-OOB mit stimmiger Stromposition. Der Inhalt ist
 * hier Nebensache — geprueft wird die Befehlszeile, nicht der Dekoder
 * (dafuer gibt es tests/test_kryoflux_stream.c). */
static std::string pruefstrom() {
    static const unsigned char b[] = {
        0x40, 0x50, 0x60,
        0x0D, 0x02, 0x0C, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0x90, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
        0x70,
        0x0D, 0x03, 0x08, 0x00,
            0x04, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
    };
    return std::string(reinterpret_cast<const char*>(b), sizeof(b));
}

/* ────────────────────────────────────────────────────────────────────
 * Der Lesebefehl des Produktionspfades
 * ──────────────────────────────────────────────────────────────────── */

static std::vector<std::string> lese_argv(int zylinder, int kopf) {
    SubprocessMock mock;
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "dtc" }, pruefstrom(), "", 0
    });
    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    (void)p.read_raw_flux(ReadFluxParams{ zylinder, kopf, 2, 0 });
    assert(!mock.recorded_runs().empty());
    return mock.recorded_runs().front().argv;
}

static void die_spurwahl_nennt_die_spur()
{
    /* Handbuch: -s<trk> set start track, -e<trk> set end track.
     * Eine einzelne Spur heisst also Start == Ende == Zylinder. */
    const auto argv = lese_argv(10, 0);

    PRUEFE(hat(argv, "-s10"),
           "-s muss die START-SPUR tragen (Handbuch: set start track), "
           "also den Zylinder");
    PRUEFE(hat(argv, "-e10"),
           "-e muss die END-SPUR tragen, also denselben Zylinder");

    if (g_fail) std::printf("    argv war: %s\n", zeige(argv).c_str());
}

static void die_seitenwahl_nennt_die_seite()
{
    /* Handbuch: -g<side>  0=side 0, 1=side 1, 2=both sides.
     * Ohne -g gilt „default auto" — also BEIDE Seiten, und dann ist
     * eine Aufnahme von „Kopf 1" keine Aufnahme von Kopf 1. */
    const auto argv0 = lese_argv(0, 0);
    PRUEFE(hat(argv0, "-g0"), "Kopf 0 muss -g0 setzen (Seite 0)");

    const auto argv1 = lese_argv(0, 1);
    PRUEFE(hat(argv1, "-g1"), "Kopf 1 muss -g1 setzen (Seite 1)");
}

static void die_kopfjustage_bleibt_unberuehrt()
{
    /* Handbuch: -b<trk> set side 1/b track0 physical position.
     * Das ist ein JUSTAGE-Parameter fuer schiefe Laufwerke, kein
     * Spurwaehler. Eine Zylindernummer dort verstellt die
     * Mechanik-Annahme fuer Seite 1 — und zwar still. */
    const auto argv = lese_argv(40, 0);

    PRUEFE(!hat_praefix(argv, "-b"),
           "-b (Track-0-Position von Seite 1) darf nicht gesetzt werden — "
           "es ist keine Spurwahl");

    if (g_fail) std::printf("    argv war: %s\n", zeige(argv).c_str());
}

static void was_schon_richtig_war_bleibt()
{
    /* Diese drei standen von Anfang an richtig da. Sie werden
     * mitgeprueft, damit eine Berichtigung sie nicht aus Versehen
     * mitnimmt — die Gestalt von MF-519/MF-529. */
    const auto argv = lese_argv(3, 0);

    PRUEFE(hat(argv, "-i0"),
           "-i0 = image type STREAM (Handbuch: set image type)");
    PRUEFE(hat(argv, "-d0"),
           "-d0 = select drive 0 (Handbuch: select drive)");
    PRUEFE(hat_praefix(argv, "-f"),
           "-f<name> muss den Dateinamen setzen (Handbuch: set filename)");
}

/* ────────────────────────────────────────────────────────────────────
 * Der ZWEITE Befehlsbauer: die C-Huelle
 *
 * `uft_kf_build_capture_command()` (bis MF-1046 `static
 * build_dtc_command`) baut den Befehl, den `uft_kf_capture_track()`
 * absetzt — erreichbar ueber `uft_hal_unified.c:901`. Er ist damit
 * genauso Produktion wie der C++-Bauer darueber, und er war genauso
 * falsch, nur anders. Zwei Umsetzungen derselben Sache, auseinander
 * gedriftet: die Gestalt von MF-1015 und MF-1026.
 * ──────────────────────────────────────────────────────────────────── */

static std::string c_befehl(int track, int side, bool doppelschritt)
{
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != nullptr);
    uft_kf_set_dtc_path(cfg, "/kein/dtc/hier/dtc");   /* Pfad, nicht Fund */
    uft_kf_set_double_step(cfg, doppelschritt);
    /* Das Temp-Verzeichnis setzt `create()` selbst (get_temp_directory());
     * einen Setzer dafuer gibt es nicht. Der Test schreibt den Pfad
     * deshalb NICHT vor — geprueft wird seine Form: das -f-Praefix muss
     * auf `track` enden, weil `uft_kf_capture_track()` danach
     * `<temp_dir>[/]trackNN.S.raw` oeffnet. */

    char cmd[2048] = { 0 };
    const int len = uft_kf_build_capture_command(cfg, track, side,
                                                  cmd, sizeof(cmd));
    (void)len;
    uft_kf_config_destroy(cfg);
    return std::string(cmd);
}

static bool enthaelt(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

static void die_c_huelle_nennt_dieselben_optionen()
{
    const std::string c = c_befehl(7, 1, false);

    /* Handbuch: -s<trk> Startspur, -e<trk> Endspur — beide die Spur. */
    PRUEFE(enthaelt(c, "-s7"), "C-Huelle: -s muss die Spur tragen");
    PRUEFE(enthaelt(c, "-e7"), "C-Huelle: -e muss die Spur tragen");

    /* Handbuch: -g<side> 0=side 0, 1=side 1, 2=both. */
    PRUEFE(enthaelt(c, "-g1"), "C-Huelle: -g muss die Seite tragen");

    /* Handbuch: -f<name> ist der Dateiname. Frueher stand dort die
     * Formatzahl. Das Praefix muss zu dem passen, was
     * uft_kf_capture_track() danach oeffnet: <temp_dir>[/]trackNN.S.raw */
    PRUEFE(enthaelt(c, "track\""),
           "C-Huelle: das -f-Praefix muss auf `track` enden — DTC haengt "
           "NN.S.raw an, und genau das oeffnet uft_kf_capture_track()");
    PRUEFE(enthaelt(c, "-f\""),
           "C-Huelle: -f muss einen NAMEN tragen (frueher stand dort die "
           "Formatzahl)");

    /* Handbuch: -i<type> ist der BILDTYP. Frueher stand dort die Spur. */
    PRUEFE(enthaelt(c, "-i0"), "C-Huelle: -i0 = Bildtyp STREAM");

    /* Handbuch: -p ist ein Schalter OHNE Argument. */
    PRUEFE(!enthaelt(c, "-p\""), "C-Huelle: -p nimmt kein Argument");

    if (g_fail) std::printf("    Befehl war: %s\n", c.c_str());
}

static void die_c_huelle_haelt_die_reihenfolge()
{
    /* Handbuch, „IMPORTANT NOTE on command line parameters order":
     * -f/-s/-e/-g/-k sind „image local" und muessen VOR dem Bildtyp
     * stehen, sonst wirken sie nicht auf ihn. */
    const std::string c = c_befehl(7, 0, false);
    const auto pos_i = c.find("-i0");
    PRUEFE(pos_i != std::string::npos, "C-Huelle: -i fehlt");
    if (pos_i == std::string::npos) return;

    for (const char *lokal : { "-f", "-s7", "-e7", "-g0" }) {
        const auto p = c.find(lokal);
        PRUEFE(p != std::string::npos && p < pos_i,
               "C-Huelle: bild-lokale Option muss VOR -i stehen");
    }
}

static void der_spurabstand_steht_an_seiner_option()
{
    /* Handbuch: -k<step> set track distance, 1=80 tracks, 2=40 tracks.
     * Frueher trug -g den Schritt — und -g ist die Seitenwahl. */
    const std::string ohne = c_befehl(7, 0, false);
    const std::string mit  = c_befehl(7, 0, true);

    PRUEFE(!enthaelt(ohne, "-k2"),
           "ohne Doppelschritt darf kein -k2 gesetzt sein");
    PRUEFE(enthaelt(mit, "-k2"),
           "mit Doppelschritt muss -k2 gesetzt sein (40 Spuren)");
    PRUEFE(enthaelt(mit, "-g0"),
           "der Doppelschritt darf die Seitenwahl nicht ueberschreiben");
}

/* ────────────────────────────────────────────────────────────────────
 * Die Schreibseite
 *
 * Sie war schwerer betroffen als die Leseseite: VIER von sieben
 * Argumenten lagen an der falschen Option, und jede davon ist eine
 * gueltige andere. „Spur 5, Seite 0, Einzelschritt" haette damit die
 * Spuren 0 bis 5 auf SEITE 1 geschrieben.
 *
 * Der Schreiber hat weiterhin keinen Aufrufer (MF-1045, P3-204).
 * Geprueft wird trotzdem, weil eine bekannt falsche Befehlszeile
 * stehen zu lassen schlechter ist, als sie zu berichtigen — und weil
 * derjenige, der ihn eines Tages verdrahtet, sich darauf verlassen
 * koennen muss.
 * ──────────────────────────────────────────────────────────────────── */

static std::string schreibbefehl(int track, int side, bool doppelschritt)
{
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != nullptr);
    uft_kf_set_dtc_path(cfg, "/kein/dtc/hier/dtc");
    uft_kf_set_double_step(cfg, doppelschritt);

    char cmd[2048] = { 0 };
    const int len = uft_kf_build_write_command(cfg, track, side,
                                                cmd, sizeof(cmd));
    uft_kf_config_destroy(cfg);
    return len < 0 ? std::string() : std::string(cmd);
}

static void der_schreibbefehl_trifft_die_richtige_seite()
{
    /* Der teuerste Einzelfehler des Vorzustands: der Spurabstand stand
     * in der Seitenwahl. Mit Einzelschritt ergab das `-g1` — Seite 0
     * war ueber diesen Weg NIE erreichbar. */
    const std::string s0 = schreibbefehl(5, 0, false);
    const std::string s1 = schreibbefehl(5, 1, false);

    PRUEFE(enthaelt(s0, "-g0"), "Seite 0 muss -g0 setzen");
    PRUEFE(enthaelt(s1, "-g1"), "Seite 1 muss -g1 setzen");

    /* Und die Spur darf nicht mehr in der Startspur die Seite ersetzen. */
    PRUEFE(enthaelt(s0, "-s5") && enthaelt(s0, "-e5"),
           "die Spur gehoert in -s und -e");

    /* -w muss dabei sein, sonst ist es kein Schreibbefehl. */
    PRUEFE(enthaelt(s0, "-w"), "der Schreibbefehl braucht -w");

    if (g_fail) std::printf("    Befehl war: %s\n", s0.c_str());
}

static void der_schreibbefehl_verwechselt_spur_und_wiederholung_nicht()
{
    /* Handbuch: -t<try> ist die Zahl der Wiederholungen je Spur,
     * „min 1". Vorher stand dort die Spurnummer — Spur 0 ergab `-t0`,
     * unter dem dokumentierten Minimum, Spur 79 ergab 79 Versuche. */
    const std::string s = schreibbefehl(0, 0, false);

    PRUEFE(!enthaelt(s, "-t0"),
           "-t0 unterschreitet das dokumentierte Minimum (min 1)");
    PRUEFE(enthaelt(s, "-s0") && enthaelt(s, "-e0"),
           "Spur 0 gehoert in -s0/-e0, nicht in -t");

    /* Der Spurabstand hat seine eigene Option. */
    const std::string mit = schreibbefehl(5, 0, true);
    PRUEFE(enthaelt(mit, "-k2"), "Doppelschritt gehoert in -k2");
    PRUEFE(enthaelt(mit, "-g0"),
           "der Doppelschritt darf die Seitenwahl nicht ueberschreiben");
}

static void der_schreibbefehl_weist_unsinn_ab()
{
    /* Waechter: eine Befehlszeile fuer eine Spur, die es nicht gibt,
     * oder fuer eine dritte Seite, waere eine erfundene Angabe. */
    PRUEFE(schreibbefehl(-1, 0, false).empty(),
           "negative Spur muss abgewiesen werden");
    PRUEFE(schreibbefehl(5, 2, false).empty(),
           "eine dritte Seite gibt es nicht");

    char puffer[16];
    uft_kf_config_t *cfg = uft_kf_config_create();
    PRUEFE(uft_kf_build_write_command(NULL, 0, 0, puffer, sizeof(puffer)) < 0,
           "NULL-Konfiguration muss abgewiesen werden");
    PRUEFE(uft_kf_build_write_command(cfg, 0, 0, NULL, 16) < 0,
           "NULL-Puffer muss abgewiesen werden");
    uft_kf_config_destroy(cfg);
}

int main()
{
    std::printf("=== KryoFlux: DTC-Befehl gegen das Handbuch (MF-1046) ===\n");

    die_spurwahl_nennt_die_spur();
    die_seitenwahl_nennt_die_seite();
    die_kopfjustage_bleibt_unberuehrt();
    was_schon_richtig_war_bleibt();

    std::printf("--- der zweite Bauer: die C-Huelle ---\n");
    die_c_huelle_nennt_dieselben_optionen();
    die_c_huelle_haelt_die_reihenfolge();
    der_spurabstand_steht_an_seiner_option();

    std::printf("--- die Schreibseite ---\n");
    der_schreibbefehl_trifft_die_richtige_seite();
    der_schreibbefehl_verwechselt_spur_und_wiederholung_nicht();
    der_schreibbefehl_weist_unsinn_ab();

    if (g_fail == 0) {
        std::printf("test_kryoflux_dtc_befehl: 0 Fehler — die Befehlszeile "
                    "traegt die Bedeutung, die das Handbuch ihren Optionen "
                    "gibt.\n");
    } else {
        std::printf("test_kryoflux_dtc_befehl: %d Fehler\n", g_fail);
    }
    std::printf("\nNICHT geprueft: was ein echtes DTC mit dem Befehl tut.\n"
                "Dieses Projekt hat keine Hardware (MF-310), und der echte\n"
                "DTC liegt nicht im Baum (tools/hw_simulators/ haelt nur\n"
                "Simulatoren). Geprueft ist die Bedeutung der Optionen\n"
                "gegen das Handbuch des Urhebers.\n");
    return g_fail == 0 ? 0 : 1;
}
