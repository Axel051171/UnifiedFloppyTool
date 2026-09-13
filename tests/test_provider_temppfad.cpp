/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_provider_temppfad.cpp
 * @brief Die Pfade, die an fremde Prozesse gehen, sind abgeleitet — nicht
 *        fest verdrahtet (MF-1108)
 *
 * ── Der Befund ───────────────────────────────────────────────────────
 *
 * `FluxEngineProviderV2` und `KryoFluxProviderV2` bauen einen Pfad und
 * uebergeben ihn als Argument an ein FREMDES Programm (`fluxengine`
 * bzw. `dtc`). Bis MF-1108 stand dort ein Literal:
 *
 *     "/tmp/uft_fe_" + cyl + "_" + head + ".scp"
 *     "/tmp/uft_kf_" + cyl + "_" + head
 *
 * Unter Windows gibt es `/tmp` nicht. Und der Kommentar daneben sagte,
 * „in production the runner must use a real temp file" — gemessen tut
 * `make_fluxengine_qprocess_runner()` / `make_kryoflux_qprocess_runner()`
 * genau das NICHT: beide reichen `argv` unveraendert an QProcess weiter
 * (`qprocess_subprocess_runner.cpp:112` bzw. `:129`). Der Pfad kommt
 * also wirklich beim fremden Prozess an.
 *
 * Die richtige Fassung lag dabei im selben Baum und sogar in derselben
 * Datei: `make_fc5025_read_qprocess_runner()` legt mit
 * `QDir::tempPath()` eine echte `QTemporaryFile` an und liest sie
 * zurueck (`qprocess_subprocess_runner.cpp:227/271`), und
 * `src/hal/uft_kryoflux_dtc.c:304` fuehrt seit MF-1046
 * `get_temp_directory()` mit `GetTempPathA`, und
 * `uft_format_convert_dispatch.c:1076` seit MF-507 einen
 * `#ifdef _WIN32`-Zweig. Zensus ueber `git ls-files`: **drei von sechs**
 * Wegen waren richtig, drei nicht — Gestalt von MF-519/MF-529, hier
 * ueber drei Dateien verteilt.
 *
 * ── Warum dieser Test so und nicht einfacher gebaut ist ──────────────
 *
 * Die naheliegende Zusage waere: „der Pfad liegt unter
 * `std::filesystem::temp_directory_path()`". Die ist auf Linux — wo
 * die CI-Matrix laeuft — AUCH VOR der Korrektur gruen, weil dort
 * `temp_directory_path()` gerade `/tmp` ist. Das waere ein Tor, das
 * auf der Plattform, auf der es laeuft, nicht rot werden kann
 * (MF-1000 / Tor 64).
 *
 * Geprueft wird deshalb die EIGENSCHAFT, die den Unterschied ausmacht:
 * ist der Pfad ABGELEITET? `std::filesystem::temp_directory_path()`
 * liest `TMPDIR`/`TMP`/`TEMP`. Der Test setzt die Umgebung auf zwei
 * verschiedene Verzeichnisse und verlangt, dass der Pfad MITWANDERT.
 *
 * **Rotbeweis, gemessen:** derselbe Test gegen die Fassung aus dem
 * git-Objekt (`git show HEAD:…`, nicht aus dem Arbeitsbaum — MF-1096
 * Sperre 3) uebersetzt und ausgefuehrt: **9 von 27 Zusagen fallen** (der Test zaehlt sie selbst),
 * gegen den Arbeitsbaum 0. Gemessen unter Windows/MinGW 13.1.0.
 *
 * **Und was daran HERGELEITET und nicht gemessen ist, gehoert dazu
 * gesagt:** dass er auch unter Linux rot werden KANN. Der Test
 * vergleicht zwei Laeufe derselben Plattform mit verschiedener
 * Umgebung; ein Stringliteral ist auf jeder Plattform konstant, also
 * kann `pfad_a != pfad_b` fuer ein Literal nirgends zutreffen. Das ist
 * eine Eigenschaft des Codes, keine der Plattform. Nachgemessen unter
 * Linux ist es nicht: die WSL2-Umgebung dieser Maschine hat keinen
 * C++-Uebersetzer (`cc1plus` fehlt), und ein Paket nachzuinstallieren
 * war nicht beauftragt.
 *
 * ── Was NICHT geprueft wird ──────────────────────────────────────────
 *
 * Ob unter dem Pfad je eine Datei entsteht. Sie entsteht nicht: kein
 * Laeufer legt sie an, und kein Laeufer liest sie zurueck. Das ist der
 * offene Teil von P3-342 und ausdruecklich NICHT Gegenstand dieser
 * Zusagen. Der SCHREIBpfad von FluxEngine ist zudem seit MF-1047
 * unerreichbar (unbedingte Absage davor) und wird hier gar nicht
 * angefasst.
 *
 * Hardware gibt es nicht (MF-310); gemessen wird die argv, eine reine
 * Funktion ihrer Eingaben.
 */
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "hardware_providers/fluxengine_provider_v2.h"
#include "hardware_providers/kryoflux_provider_v2.h"
#include "mock_hardware/subprocess_mock.h"

using namespace uft::hal;
using uft::tests::mocks::SubprocessMock;

static int g_fail = 0;
static int g_zusagen = 0;

#define PRUEFE(bed, text)                                               \
    do {                                                                \
        ++g_zusagen;                                                    \
        if (!(bed)) {                                                   \
            std::printf("  FAIL @ %d: %s\n", __LINE__, (text));         \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

/* ── Temp-Umgebung setzen ──────────────────────────────────────────── */

/* Das ECHTE Temp-Verzeichnis, einmal vor der ersten Umbiegung gemerkt.
 * Ohne das rechnet die zweite Pruefung ihr Arbeitsverzeichnis aus der
 * bereits umgebogenen Umgebung — und `temp_directory_path()` wirft, wenn
 * das dort genannte Verzeichnis inzwischen geloescht ist. Beim ersten
 * Lauf ist genau das passiert; der Abbruch war zugleich der Beleg, dass
 * die Umbiegung ueberhaupt durchschlaegt. */
static std::filesystem::path g_basis;

static void setze_temp(const std::string& pfad)
{
#ifdef _WIN32
    _putenv_s("TMP", pfad.c_str());
    _putenv_s("TEMP", pfad.c_str());
    _putenv_s("TMPDIR", pfad.c_str());
#else
    setenv("TMPDIR", pfad.c_str(), 1);
    setenv("TMP",    pfad.c_str(), 1);
    setenv("TEMP",   pfad.c_str(), 1);
#endif
}

/* ── argv der LESEwege ─────────────────────────────────────────────── */

static std::string scp_puffer()
{
    std::string s(0x10, '\0');
    s[0] = 'S'; s[1] = 'C'; s[2] = 'P';
    return s;
}

static std::vector<std::string> fe_lese_argv(int zyl, int kopf)
{
    SubprocessMock mock;
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine" }, scp_puffer(), "", 0 });
    FluxEngineProviderV2 p(
        [&mock](const std::vector<std::string>& argv,
                const std::string& sin) -> FluxEngineRunResult {
            auto r = mock.run(argv, sin);
            return FluxEngineRunResult{ r.stdout_text, r.stderr_text,
                                        r.exit_code };
        },
        "fluxengine", 79, "ibm");
    (void)p.read_raw_flux(ReadFluxParams{ zyl, kopf, 1, 0 });
    assert(!mock.recorded_runs().empty());
    return mock.recorded_runs().front().argv;
}

static std::vector<std::string> kf_lese_argv(int zyl, int kopf)
{
    SubprocessMock mock;
    mock.queue_run(SubprocessMock::ScriptedRun{ { "dtc" }, "", "", 0 });
    KryoFluxProviderV2 p(
        [&mock](const std::vector<std::string>& argv,
                const std::string& sin) -> DtcRunResult {
            auto r = mock.run(argv, sin);
            return DtcRunResult{ r.stdout_text, r.stderr_text, r.exit_code };
        },
        "dtc");
    (void)p.read_raw_flux(ReadFluxParams{ zyl, kopf, 1, 0 });
    assert(!mock.recorded_runs().empty());
    return mock.recorded_runs().front().argv;
}

/** Das erste Argument, das `nadel` enthaelt (leer, wenn keins). */
static std::string arg_mit(const std::vector<std::string>& argv,
                           const std::string& nadel)
{
    for (const auto& a : argv)
        if (a.find(nadel) != std::string::npos) return a;
    return std::string();
}

static bool liegt_unter(const std::string& pfad, const std::string& marke)
{
    return !pfad.empty() && pfad.find(marke) != std::string::npos;
}

/* ── 1. FluxEngine: der Flusspfad wandert mit der Umgebung ─────────── */

static void fluxengine_pfad_ist_abgeleitet()
{
    namespace fs = std::filesystem;

    const fs::path a = g_basis / "uft_mf1108_a";
    const fs::path b = g_basis / "uft_mf1108_b";
    std::error_code ec;
    fs::create_directories(a, ec);
    fs::create_directories(b, ec);

    setze_temp(a.string());
    const std::string pfad_a = arg_mit(fe_lese_argv(3, 1), "uft_fe_3_1");

    setze_temp(b.string());
    const std::string pfad_b = arg_mit(fe_lese_argv(3, 1), "uft_fe_3_1");

    PRUEFE(!pfad_a.empty() && !pfad_b.empty(),
           "der Lesepfad muss ueberhaupt in der argv stehen");

    /* Der Kern. Ein Literal wandert nicht. */
    PRUEFE(pfad_a != pfad_b,
           "der Pfad muss der Temp-Umgebung folgen — ein fest "
           "verdrahtetes \"/tmp/...\" tut das nicht (MF-1108)");

    PRUEFE(liegt_unter(pfad_a, "uft_mf1108_a"),
           "der Pfad muss im gesetzten Temp-Verzeichnis liegen (Lauf a)");
    PRUEFE(liegt_unter(pfad_b, "uft_mf1108_b"),
           "der Pfad muss im gesetzten Temp-Verzeichnis liegen (Lauf b)");

    fs::remove_all(a, ec);
    fs::remove_all(b, ec);
}

/* ── 2. KryoFlux: dasselbe fuer das dtc-Praefix ────────────────────── */

static void kryoflux_praefix_ist_abgeleitet()
{
    namespace fs = std::filesystem;

    const fs::path a = g_basis / "uft_mf1108_c";
    const fs::path b = g_basis / "uft_mf1108_d";
    std::error_code ec;
    fs::create_directories(a, ec);
    fs::create_directories(b, ec);

    setze_temp(a.string());
    const std::string pfad_a = arg_mit(kf_lese_argv(5, 0), "uft_kf_5_0");

    setze_temp(b.string());
    const std::string pfad_b = arg_mit(kf_lese_argv(5, 0), "uft_kf_5_0");

    PRUEFE(!pfad_a.empty() && !pfad_b.empty(),
           "das dtc-Praefix muss ueberhaupt in der argv stehen");
    PRUEFE(pfad_a != pfad_b,
           "das dtc-Praefix muss der Temp-Umgebung folgen (MF-1108)");
    PRUEFE(liegt_unter(pfad_a, "uft_mf1108_c"),
           "das Praefix muss im gesetzten Temp-Verzeichnis liegen (Lauf a)");
    PRUEFE(liegt_unter(pfad_b, "uft_mf1108_d"),
           "das Praefix muss im gesetzten Temp-Verzeichnis liegen (Lauf b)");

    fs::remove_all(a, ec);
    fs::remove_all(b, ec);
}

/* ── 3. Gegenprobe: kein Literal mehr in der argv ──────────────────── */

static void kein_literal_mehr_in_der_argv()
{
    namespace fs = std::filesystem;

    /* Ein Temp-Verzeichnis, das SICHER nicht "/tmp" heisst. Danach darf
     * in keiner argv mehr ein Argument stehen, das "/tmp/uft_" traegt —
     * das waere das alte Literal. Diese Zusage ist auf Linux genauso
     * scharf wie auf Windows, weil die Umgebung umgebogen ist. */
    const fs::path z = g_basis / "uft_mf1108_e";
    std::error_code ec;
    fs::create_directories(z, ec);
    setze_temp(z.string());

    const std::vector<std::vector<std::string>> laeufe = {
        fe_lese_argv(0, 0), kf_lese_argv(0, 0)
    };
    for (const auto& argv : laeufe) {
        for (const auto& arg : argv) {
            PRUEFE(arg.find("/tmp/uft_") == std::string::npos,
                   "kein Argument darf noch das feste \"/tmp/uft_\" tragen");
        }
    }

    fs::remove_all(z, ec);
}

int main()
{
    std::printf("=== Provider-Temppfade sind abgeleitet (MF-1108) ===\n");

    fluxengine_pfad_ist_abgeleitet();
    kryoflux_praefix_ist_abgeleitet();
    kein_literal_mehr_in_der_argv();

    if (g_fail == 0)
        std::printf("test_provider_temppfad: %d Zusagen, 0 Fehler\n",
                    g_zusagen);
    else
        std::printf("test_provider_temppfad: %d Zusagen, %d Fehler\n",
                    g_zusagen, g_fail);

    std::printf("\nNICHT geprueft: ob unter diesen Pfaden je eine Datei\n"
                "entsteht. Sie entsteht nicht — kein Laeufer legt sie an\n"
                "und keiner liest sie zurueck (offener Teil von P3-342).\n");
    return g_fail == 0 ? 0 : 1;
}
