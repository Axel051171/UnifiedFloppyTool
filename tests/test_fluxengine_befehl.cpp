/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_fluxengine_befehl.cpp
 * @brief Der fluxengine-Befehl gegen die Doku des Urhebers (MF-1047)
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 *
 * `doc/using.md` aus davidgiven/fluxengine, woertlich:
 *
 *     - `fluxengine read -c <profile> <options> -s <flux source>
 *        -o <image output>`
 *
 *       Reads flux (possibly from a disk) and decodes it into a file
 *       system image.
 *
 *     - `fluxengine rawwrite -s <flux source> -d <flux destination>`
 *
 *       Reads flux from a file and writes it (possibly to a disk)
 *       without doing any encoding.
 *
 *     $ fluxengine read -c brother240 -s drive:0 -o brother.img
 *       --copy-flux-to=brother.flux
 *
 * Kanal *Spec* nach MF-695: gelesen, keine Zeile Code uebernommen.
 *
 * ── Der Befund ───────────────────────────────────────────────────────
 *
 * UFT setzte fuer eine FLUSS-Aufnahme ab:
 *
 *     fluxengine read -c <profil> -s drive:0 --tracks=cNhM
 *                     --drive.revolutions=N -o /tmp/uft_fe_N_M.scp
 *
 * `-o` ist laut Doku der **Ausgang fuer das dekodierte
 * Dateisystem-Abbild**, nicht fuer Fluss; der Fluss geht ueber
 * `--copy-flux-to=`. In den Beispielen des Urhebers stehen beide
 * **nebeneinander**. UFT bat also um ein dekodiertes Abbild und las
 * das Ergebnis anschliessend als SCP-Flussbehaelter.
 *
 * **Warum das nicht auffiel:** der vorhandene Audit
 * (`audit/fluxengine/REPORT.md`) meldet fuer genau diese Zeile
 * „**PASS (recalled)** — 8/8 tokens". Geprueft wurde die **Tokenform**
 * gegen eine aus dem Gedaechtnis geschriebene Erwartung
 * (`extract_ref.py`, „PROVENANCE: grade = recalled"), nicht die
 * **Bedeutung** gegen das Dokument. `-o` ist eine gueltige Flagge —
 * sie meint nur etwas anderes. Dieselbe Gestalt wie bei DTC in
 * MF-1046: jedes falsche Argument traf eine gueltige andere Option.
 *
 * Zum Vergleich, und das gehoert dazu: der **KryoFlux**-Audit hat
 * dieselbe Frage NICHT durchgewinkt. Er fuehrte die Lesezeile als
 * `UNVERIFIED (needs-source)` und benannte den Defekt woertlich
 * („KF-D1-1: UFT passes the head number through DTC's `-s`"). Er hat
 * nach der Quelle gefragt; MF-1046 hat sie geholt. Das Risiko sitzt
 * nicht bei „needs-source", sondern bei „PASS (recalled)".
 *
 * ── Was dieser Test prueft, und was NICHT ────────────────────────────
 *
 * Geprueft wird die Bedeutung der Optionen gegen die Beschreibung des
 * Urhebers. Die argv ist eine reine Funktion ihrer Eingaben und damit
 * ohne Hardware abnehmbar (MF-310); ein echtes `fluxengine` liegt nicht
 * im Baum (`audit/fluxengine/` haelt einen Mock).
 *
 * NICHT geprueft: was ein echtes `fluxengine` tut.
 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <variant>
#include <type_traits>
#include <algorithm>

#include "hardware_providers/fluxengine_provider_v2.h"
#include "mock_hardware/subprocess_mock.h"

using namespace uft::hal;
using uft::tests::mocks::SubprocessMock;

static int g_fail = 0;

#define PRUEFE(bed, text)                                               \
    do {                                                                \
        if (!(bed)) {                                                   \
            std::printf("  FAIL @ %d: %s\n", __LINE__, (text));         \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

static FluxEngineProviderV2::FluxEngineRunner make_runner(SubprocessMock& m) {
    return [&m](const std::vector<std::string>& argv,
                const std::string& stdin_data) -> FluxEngineRunResult {
        auto r = m.run(argv, stdin_data);
        return FluxEngineRunResult{ r.stdout_text, r.stderr_text, r.exit_code };
    };
}

static bool hat_praefix(const std::vector<std::string>& argv,
                        const std::string& p) {
    for (const auto& a : argv)
        if (a.size() >= p.size() && a.compare(0, p.size(), p) == 0) return true;
    return false;
}

static bool hat(const std::vector<std::string>& argv, const std::string& t) {
    return std::find(argv.begin(), argv.end(), t) != argv.end();
}

static std::string zeige(const std::vector<std::string>& argv) {
    std::string s;
    for (const auto& a : argv) { s += a; s += ' '; }
    return s;
}

/** Das Argument, das unmittelbar auf `flagge` folgt (leer, wenn keins). */
static std::string wert_nach(const std::vector<std::string>& argv,
                             const std::string& flagge) {
    for (size_t i = 0; i + 1 < argv.size(); ++i)
        if (argv[i] == flagge) return argv[i + 1];
    return std::string();
}

/* Ein SCP-Kopf reicht: geprueft wird die Befehlszeile, nicht der
 * Dekoder (dafuer gibt es tests/test_fluxengine_provider_v2.cpp). */
static std::string scp_puffer() {
    std::string s(0x10, '\0');
    s[0] = 'S'; s[1] = 'C'; s[2] = 'P';
    return s;
}

static std::vector<std::string> lese_argv(int zylinder, int kopf) {
    SubprocessMock mock;
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine" }, scp_puffer(), "", 0 });
    FluxEngineProviderV2 p(make_runner(mock), "fluxengine", 79, "ibm");
    (void)p.read_raw_flux(ReadFluxParams{ zylinder, kopf, 1, 0 });
    assert(!mock.recorded_runs().empty());
    return mock.recorded_runs().front().argv;
}

/* ────────────────────────────────────────────────────────────────────
 * 1. Die Fluss-Aufnahme
 * ──────────────────────────────────────────────────────────────────── */

static void fluss_geht_ueber_copy_flux_to()
{
    const auto argv = lese_argv(3, 1);

    /* Das ist der Kern. Doku: „--copy-flux-to=<datei>" kopiert den
     * FLUSS; „-o <image output>" nimmt das DEKODIERTE Abbild. UFT
     * braucht den Fluss. */
    PRUEFE(hat_praefix(argv, "--copy-flux-to"),
           "eine Fluss-Aufnahme muss --copy-flux-to setzen "
           "(Doku: -o ist der Ausgang fuer das Dateisystem-Abbild)");

    /* Und die Gegenprobe, ohne die die erste zu wenig sagt: `-o` darf
     * NICHT auf dieselbe Datei zeigen. Genau das war der Defekt — der
     * Provider bat um ein dekodiertes Abbild an der Stelle, an der er
     * hinterher einen Flussbehaelter zu lesen erwartete. */
    std::string fluss;
    for (const auto& a : argv) {
        const std::string pfx = "--copy-flux-to=";
        if (a.size() > pfx.size() && a.compare(0, pfx.size(), pfx) == 0) {
            fluss = a.substr(pfx.size());
        }
    }
    PRUEFE(!fluss.empty(), "--copy-flux-to braucht einen Wert");
    PRUEFE(wert_nach(argv, "-o") != fluss,
           "-o darf nicht auf die Flussdatei zeigen — es ist der Ausgang "
           "fuer das dekodierte Abbild");

    if (g_fail) std::printf("    argv war: %s\n", zeige(argv).c_str());
}

static void die_quelle_ist_das_laufwerk()
{
    /* Doku: `-s <flux source>` — beim Lesen von der Diskette also das
     * Laufwerk. Das stand von Anfang an richtig da und wird
     * mitgeprueft, damit die Berichtigung es nicht mitnimmt
     * (MF-519/MF-529). */
    const auto argv = lese_argv(0, 0);
    PRUEFE(hat(argv, "read"), "das Unterkommando ist `read`");
    PRUEFE(wert_nach(argv, "-s") == "drive:0",
           "-s muss die Flussquelle nennen: drive:0");
    PRUEFE(wert_nach(argv, "-c") == "ibm",
           "-c laedt das Profil BEIM NAMEN");
}

static void die_spurwahl_folgt_der_dokumentierten_form()
{
    /* Doku: --tracks='c0h0 c1h0 c3-5h1' — „cNhM" ist die
     * dokumentierte Schreibweise fuer eine einzelne Spur. */
    const auto argv = lese_argv(7, 1);
    PRUEFE(hat(argv, "--tracks=c7h1"),
           "--tracks muss die dokumentierte Form cNhM tragen");
}

/* ────────────────────────────────────────────────────────────────────
 * 2. Das Fluss-Schreiben
 * ──────────────────────────────────────────────────────────────────── */

static void schreiben_sagt_ab_statt_das_falsche_kommando_abzusetzen()
{
    /* Doku, woertlich:
     *   `fluxengine rawwrite -s <flux source> -d <flux destination>`
     *   „Reads flux from a file and writes it (possibly to a disk)
     *    without doing any encoding."
     *
     * `write -i <datei>` dagegen KODIERT ein Dateisystem-Abbild. UFT
     * uebergab dort rohe Flusswoerter — im guenstigen Fall bricht
     * fluxengine ab, im unguenstigen kodiert es sie als Abbild und
     * schreibt Unsinn auf die Diskette.
     *
     * Seit MF-1047 sagt der Schreibpfad AB, statt einen Befehl
     * abzusetzen, den die Doku fuer etwas anderes vorsieht — und mit
     * einem Behaelter, den fluxengine nicht liest. Die Absage muss
     * erfolgen, BEVOR ein Prozess laeuft. */
    SubprocessMock mock;
    FluxEngineProviderV2 p(make_runner(mock), "fluxengine", 79, "ibm");

    FluxStream fs;
    fs.transitions_ns = { 100u, 120u, 100u, 140u };

    const auto ergebnis = p.write_raw_flux(WriteFluxParams{ 5, 0, false }, fs);

    PRUEFE(mock.recorded_runs().empty(),
           "der Schreibpfad darf keinen fluxengine-Prozess starten, "
           "solange er keinen lesbaren Flussbehaelter erzeugen kann");

    bool ist_fehler = false;
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, ProviderError>) ist_fehler = true;
    }, ergebnis);
    PRUEFE(ist_fehler,
           "der Schreibpfad muss einen ProviderError liefern, nicht "
           "Erfolg melden (MF-883: keine Zusage ohne Tat)");
}

int main()
{
    std::printf("=== FluxEngine: Befehl gegen die Doku (MF-1047) ===\n");

    fluss_geht_ueber_copy_flux_to();
    die_quelle_ist_das_laufwerk();
    die_spurwahl_folgt_der_dokumentierten_form();
    schreiben_sagt_ab_statt_das_falsche_kommando_abzusetzen();

    if (g_fail == 0) {
        std::printf("test_fluxengine_befehl: 0 Fehler\n");
    } else {
        std::printf("test_fluxengine_befehl: %d Fehler\n", g_fail);
    }
    std::printf("\nNICHT geprueft: was ein echtes `fluxengine` tut. Es liegt\n"
                "nicht im Baum, und Hardware gibt es nicht (MF-310).\n"
                "Geprueft ist die Bedeutung der Optionen gegen doc/using.md\n"
                "des Urhebers.\n");
    return g_fail == 0 ? 0 : 1;
}
