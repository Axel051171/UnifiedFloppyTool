/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_laeufer_argv.cpp
 * @brief Was die QProcess-Laeufer dem gerufenen Werkzeug wirklich
 *        uebergeben — gemessen an einem echten Prozess (MF-1362, P3-562).
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
#include <QFileInfo>
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

    /* 6 — MF-1363 / P3-562 Teil 2: der ganze Lesepfad ohne Geraet.
     *     Provider -> echter Laeufer -> echter Prozess -> Stromdatei ->
     *     Dekoder. Das Werkzeug schreibt im DTC-Modus einen ECHTEN
     *     KryoFlux-Strom aus dem Korpus (hxcfe, MF-1024) nach
     *     `<praefix>00.0.raw` und druckt auf stdout nur Protokolltext.
     *     Gegen den Vorzustand dekodierte der Provider diesen Text. */
    {
        const QString strom = QString::fromUtf8(UFT_CORPUS_DIR) +
                              QStringLiteral("/hxcfe_kfx_t00.0.raw");
        if (!QFile::exists(strom)) {
            std::printf("[ROT] Korpus-Strom fehlt: %s\n", strom.toUtf8().constData());
            ++g_fehler; ++g_zusagen;
        } else {
            qputenv("UFT_ARGV_ECHO_DTC_STROM", strom.toLocal8Bit());
            vergessen();
            KryoFluxProviderV2 kf(make_kryoflux_qprocess_runner(werkzeug()));
            const FluxOutcome o = kf.read_raw_flux(ReadFluxParams{ 0, 0, 2, 0 });
            qunsetenv("UFT_ARGV_ECHO_DTC_STROM");

            const FluxCaptured* fc = std::get_if<FluxCaptured>(&o);
            if (const ProviderError* e = std::get_if<ProviderError>(&o))
                std::printf("      ProviderError: %s | %s\n", e->what.c_str(), e->why.c_str());
            PRUEFE(fc != nullptr,
                   "6a der Strom aus der DATEI wird zu FluxCaptured — nicht "
                   "das Protokoll auf stdout");
            if (fc) {
                std::printf("      %zu Uebergaenge, %zu Indexmarken\n",
                            fc->transitions_ns.size(), fc->index_times_ns.size());
                /* MF-1024: „gemessen je Strom 10 OOB-Bloecke und 3
                 * Indexmarken" — ein Wert aus dem Korpusbefund, nicht aus
                 * diesem Lauf. */
                PRUEFE(fc->index_times_ns.size() == 3,
                       "6b die drei Indexmarken des Korpus-Stroms kommen an");
                PRUEFE(fc->transitions_ns.size() > 1000,
                       "6c es kommen Flusswechsel an, nicht ein paar Bytes Text");
            }
        }
    }

    /* 7 — MF-1438 / P3-589 (b), issue #34: die FC5025-Erkennung meldete
     *     ein Laufwerk, sobald das WERKZEUG da war. Sie fragt jetzt
     *     `fcdrives` (FC5025-Treiberpaket v1309, `cmd/fcdrives.c`: eine
     *     Zeile „<id>\t<Beschreibung>" je Laufwerk, Exit 0; ohne Geraet
     *     Exit 1 ohne Ausgabe). Das Echo-Werkzeug steht als `fcimage` UND
     *     `fcdrives` im selben Ordner; gesucht wird `fcdrives` neben dem
     *     konfigurierten `fcimage`. */
    {
        const QString suffix = QFileInfo(QString::fromUtf8(UFT_ARGV_ECHO)).suffix();
        const QString ext = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
        const QString fcimage = tmp.filePath(QStringLiteral("fcimage") + ext);
        const QString fcdrives = tmp.filePath(QStringLiteral("fcdrives") + ext);
        const bool kopiert =
            QFile::copy(QString::fromUtf8(UFT_ARGV_ECHO), fcimage) &&
            QFile::copy(QString::fromUtf8(UFT_ARGV_ECHO), fcdrives);
        PRUEFE(kopiert, "7 Vorbereitung: Echo-Werkzeug als fcimage und fcdrives");
        SubprocessRunnerConfig cfg = werkzeug();
        cfg.binary = fcimage;

        /* 7a — genau #34: kein Geraet, aber das Werkzeug gibt seinen
         *      Nutzungstext aus (Exit 1). */
        qputenv("UFT_ARGV_ECHO_STDOUT",
                "Usage: fcimage [-d device] -f format outfile\n");
        qputenv("UFT_ARGV_ECHO_RC", "1");
        const Fc5025DetectResult a = make_fc5025_detect_qprocess_runner(cfg)();
        std::printf("      ohne Geraet: found=%d drive=\"%s\" error=\"%s\"\n",
                    (int)a.found, a.drive_kind.c_str(), a.error_message.c_str());
        PRUEFE(!a.found && a.error_message.empty(),
               "7a kein FC5025 angeschlossen: NICHT gefunden (DriveAbsent), "
               "auch wenn das Werkzeug antwortet — issue #34");

        /* 7b — ein Laufwerk: seine Beschreibung ist die des Treibers. */
        qputenv("UFT_ARGV_ECHO_STDOUT", "001/002\tFC5025 test drive\n");
        qputenv("UFT_ARGV_ECHO_RC", "0");
        const Fc5025DetectResult b = make_fc5025_detect_qprocess_runner(cfg)();
        PRUEFE(b.found && b.drive_kind == "FC5025 test drive",
               "7b angeschlossen: gefunden, Laufwerksart aus fcdrives, nicht "
               "erfunden");

        /* 7c — fcdrives fehlt: kein Laufwerk, und der Grund steht da. */
        QFile::remove(fcdrives);
        const Fc5025DetectResult c = make_fc5025_detect_qprocess_runner(cfg)();
        PRUEFE(!c.found && c.error_message.find("fcdrives") != std::string::npos,
               "7c ohne fcdrives: nicht gefunden, und die Meldung nennt fcdrives");
        qunsetenv("UFT_ARGV_ECHO_STDOUT");
        qunsetenv("UFT_ARGV_ECHO_RC");
    }

    std::printf("test_laeufer_argv: %d von %d Zusagen gehalten\n",
                g_zusagen - g_fehler, g_zusagen);
    return g_fehler == 0 ? 0 : 1;
}
