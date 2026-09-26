/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_laeufer_argv.cpp
 * @brief Was die QProcess-Laeufer dem gerufenen Werkzeug wirklich
 *        uebergeben — gemessen an einem echten Prozess (MF-1359, P3-562).
 *
 * Die Provider bauen `argv` nach der execve-Konvention: an Stelle 0 steht
 * der Programmname (`kryoflux_provider_v2.cpp`, `{ m_dtc_binary, "-i0" }`;
 * `fluxengine_provider_v2.cpp`, `{ m_fe_binary, "version" }`). QProcess
 * nimmt das Programm aber ueber `setProgram()` und die Argumente ueber
 * `setArguments()` getrennt. Wurde `argv` unveraendert weitergereicht,
 * sah das Werkzeug `dtc dtc -i0` bzw. `fluxengine fluxengine version`.
 *
 * Die Mock-Tests konnten das nicht sehen: der SubprocessMock nimmt `argv`
 * nur auf und startet nichts. Dieser Test startet deshalb einen ECHTEN
 * Prozess — `tests/argv_echo/uft_argv_echo.c` an der Stelle des
 * Werkzeugs — und liest zurueck, was dort ankam.
 *
 * Der FC5025-Laeufer ist ANDERS gebaut: er legt seine Argumente selbst an,
 * ohne Programmnamen. Zusage 5 haelt fest, dass die Behebung dort nichts
 * abschneidet — ein pauschales Abschneiden in `run_subprocess()` haette
 * genau dort `-f` verschluckt.
 */
#include "hardware_providers/qprocess_subprocess_runner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>
#include <string>
#include <vector>

#ifndef UFT_ARGV_ECHO
#error "UFT_ARGV_ECHO muss den Pfad des Test-Werkzeugs nennen (tests/CMakeLists.txt)"
#endif

using namespace uft::hal;

static int g_fehler = 0;
static int g_zusagen = 0;

#define PRUEFE(bed, text)                                                  \
    do {                                                                   \
        ++g_zusagen;                                                       \
        if (!(bed)) {                                                      \
            ++g_fehler;                                                    \
            std::printf("[ROT] %s:%d: %s\n", __FILE__, __LINE__, text);    \
        } else {                                                           \
            std::printf("[ok]  %s\n", text);                               \
        }                                                                  \
    } while (0)

static QString g_protokoll;

/* Liest, was das Werkzeug zuletzt gesehen hat. `vorhanden` sagt, ob es
 * ueberhaupt lief — eine fehlende Datei ist etwas anderes als eine leere
 * Argumentliste. */
static std::vector<std::string> gesehen(bool* vorhanden)
{
    std::vector<std::string> out;
    QFile f(g_protokoll);
    *vorhanden = f.open(QIODevice::ReadOnly);
    if (!*vorhanden) return out;
    const QList<QByteArray> zeilen = f.readAll().split('\n');
    for (const QByteArray& z : zeilen) {
        if (!z.isEmpty()) out.emplace_back(z.constData(), size_t(z.size()));
    }
    return out;
}

static void vergessen() { QFile::remove(g_protokoll); }

static std::string zeige(const std::vector<std::string>& v)
{
    std::string s = "{";
    for (size_t i = 0; i < v.size(); ++i) s += (i ? " " : "") + v[i];
    return s + "}";
}

static SubprocessRunnerConfig werkzeug()
{
    SubprocessRunnerConfig cfg;
    cfg.binary = QString::fromUtf8(UFT_ARGV_ECHO);
    cfg.timeout_ms = 30000;
    return cfg;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("[ROT] kein Temp-Verzeichnis\n");
        return 1;
    }
    g_protokoll = tmp.filePath("argv.txt");
    qputenv("UFT_ARGV_ECHO_OUT", g_protokoll.toLocal8Bit());

    bool lief = false;

    /* 1 — KryoFlux-Laeufer, direkt. */
    {
        vergessen();
        auto dtc = make_kryoflux_qprocess_runner(werkzeug());
        const DtcRunResult r = dtc({ "dtc", "-c2", "-d0", "-i0" }, "");
        const auto v = gesehen(&lief);
        std::printf("      KryoFlux sah %s (rc %d)\n", zeige(v).c_str(), r.exit_code);
        PRUEFE(lief && r.exit_code == 0,
               "1a KryoFlux-Laeufer startet das Werkzeug");
        PRUEFE(v == std::vector<std::string>({ "-c2", "-d0", "-i0" }),
               "1b DTC sieht genau {-c2 -d0 -i0} — nicht noch einmal den "
               "Programmnamen davor");
    }

    /* 2 — FluxEngine-Laeufer, direkt. */
    {
        vergessen();
        auto fe = make_fluxengine_qprocess_runner(werkzeug());
        const FluxEngineRunResult r = fe({ "fluxengine", "read", "-c", "ibm" }, "");
        const auto v = gesehen(&lief);
        std::printf("      FluxEngine sah %s (rc %d)\n", zeige(v).c_str(), r.exit_code);
        PRUEFE(lief && r.exit_code == 0,
               "2a FluxEngine-Laeufer startet das Werkzeug");
        PRUEFE(v == std::vector<std::string>({ "read", "-c", "ibm" }),
               "2b fluxengine sieht genau {read -c ibm}");
    }

    /* 3 — der ganze Produktivweg: Provider -> Laeufer -> Prozess. Die
     *     Argumente baut hier der PROVIDER, nicht der Test. */
    {
        vergessen();
        KryoFluxProviderV2 kf(make_kryoflux_qprocess_runner(werkzeug()));
        (void)kf.detect_drive();
        const auto v = gesehen(&lief);
        std::printf("      KryoFlux-Erkennung sah %s\n", zeige(v).c_str());
        PRUEFE(lief && v == std::vector<std::string>({ "-i0" }),
               "3a KryoFluxProviderV2::detect_drive() kommt als `dtc -i0` an");

        vergessen();
        FluxEngineProviderV2 fe(make_fluxengine_qprocess_runner(werkzeug()));
        (void)fe.query_version();
        const auto w = gesehen(&lief);
        std::printf("      FluxEngine-Versionsfrage sah %s\n", zeige(w).c_str());
        PRUEFE(lief && w == std::vector<std::string>({ "version" }),
               "3b FluxEngineProviderV2::query_version() kommt als "
               "`fluxengine version` an");
    }

    /* 4 — ohne Programmnamen: das erste Argument waere sonst still
     *     verschluckt worden. Absagen, und zwar BEVOR ein Prozess laeuft. */
    {
        vergessen();
        auto dtc = make_kryoflux_qprocess_runner(werkzeug());
        const DtcRunResult r = dtc({ "-i0" }, "");
        (void)gesehen(&lief);
        PRUEFE(r.exit_code != 0 && !lief,
               "4a KryoFlux: argv ohne Programmnamen wird abgesagt, das "
               "Werkzeug laeuft nicht");
        PRUEFE(!r.stderr_text.empty(), "4b ... und die Absage nennt einen Grund");

        vergessen();
        auto fe = make_fluxengine_qprocess_runner(werkzeug());
        const FluxEngineRunResult s = fe({}, "");
        (void)gesehen(&lief);
        PRUEFE(s.exit_code != 0 && !lief,
               "4c FluxEngine: leeres argv wird abgesagt, das Werkzeug "
               "laeuft nicht");
    }

    /* 5 — FC5025 legt seine Argumente OHNE Programmnamen an; dort darf
     *     nichts abgeschnitten werden. */
    {
        vergessen();
        auto fc = make_fc5025_read_qprocess_runner(werkzeug());
        Fc5025ReadRequest req;
        req.cylinder = 3;
        (void)fc(req);
        const auto v = gesehen(&lief);
        std::printf("      fcimage sah %s\n", zeige(v).c_str());
        PRUEFE(lief && !v.empty() && v[0] == "-f",
               "5 fcimage sieht als erstes Argument `-f` — der FC5025-Laeufer "
               "bleibt unberuehrt");
    }

    std::printf("test_laeufer_argv: %d von %d Zusagen gehalten\n",
                g_zusagen - g_fehler, g_zusagen);
    return g_fehler == 0 ? 0 : 1;
}
