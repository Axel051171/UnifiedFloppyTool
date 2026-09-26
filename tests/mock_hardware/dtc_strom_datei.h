/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file dtc_strom_datei.h
 * @brief Die Test-Attrappe tut, was DTC tut: sie schreibt den Strom in eine
 *        DATEI, und stdout bleibt das Protokoll (MF-1360, P3-562 Teil 2).
 *
 * Bis MF-1360 kam der KryoFlux-Rohstrom in den Tests ueber `stdout_reply`
 * an, und der Provider deutete stdout als Fluss. Ein echtes DTC schreibt
 * den Strom aber nach `<praefix>NN.S.raw` (KryoFlux-Handbuch: „test_23.1.raw
 * will be stream test_") und druckt auf stdout ein Protokoll. Der Test
 * haette den Unterschied nie sehen koennen, weil er ihn selbst verwischte.
 *
 * Diese Attrappe haelt die beiden Kanaele getrennt: `stdout_reply` bleibt
 * Text, der Inhalt der Stromdatei wird eigens vorgemerkt. Den Dateinamen
 * rechnet sie NICHT selbst, sondern fragt `KryoFluxProviderV2::
 * stream_file_path()` — dieselbe Funktion, die der Provider zum Lesen
 * benutzt (eine Groesse, eine Rechnung). Welche Spur und welche Seite DTC
 * meint, liest sie aus `-s` und `-g`, wie DTC selbst.
 */
#pragma once

#include "hardware_providers/kryoflux_provider_v2.h"
#include "subprocess_mock.h"

#include <deque>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace uft::tests::mocks {

class DtcStromAttrappe {
public:
    explicit DtcStromAttrappe(SubprocessMock& mock) : m_mock(mock) {}

    /** Beim naechsten erfolgreichen Lauf mit `-f<praefix>` schreibt die
     *  Attrappe diese Bytes dorthin, wo DTC seine Stromdatei ablegt.
     *  std::nullopt = DTC schreibt KEINE Datei. */
    void naechste_datei(std::optional<std::string> inhalt) {
        m_dateien.push_back(std::move(inhalt));
    }

    uft::hal::KryoFluxProviderV2::DtcRunner laeufer() {
        return [this](const std::vector<std::string>& argv,
                      const std::string& stdin_data) -> uft::hal::DtcRunResult {
            auto r = m_mock.run(argv, stdin_data);
            if (r.exit_code == 0 && !m_dateien.empty()) {
                std::optional<std::string> inhalt = std::move(m_dateien.front());
                m_dateien.pop_front();
                if (inhalt) schreibe(argv, *inhalt);
            }
            return uft::hal::DtcRunResult{ r.stdout_text, r.stderr_text,
                                           r.exit_code };
        };
    }

    /** Pfad, den DTC fuer diesen argv beschreiben wuerde, oder leer. */
    static std::string ziel(const std::vector<std::string>& argv) {
        std::string praefix;
        int spur = -1, seite = -1;
        for (const std::string& a : argv) {
            if (a.rfind("-f", 0) == 0 && a.size() > 2) praefix = a.substr(2);
            else if (a.rfind("-s", 0) == 0 && a.size() > 2) spur = std::stoi(a.substr(2));
            else if (a.rfind("-g", 0) == 0 && a.size() > 2) seite = std::stoi(a.substr(2));
        }
        if (praefix.empty() || spur < 0 || seite < 0) return {};
        return uft::hal::KryoFluxProviderV2::stream_file_path(praefix, spur, seite);
    }

private:
    static void schreibe(const std::vector<std::string>& argv,
                         const std::string& inhalt) {
        const std::string pfad = ziel(argv);
        if (pfad.empty()) return;
        std::ofstream f(pfad, std::ios::binary | std::ios::trunc);
        f.write(inhalt.data(), static_cast<std::streamsize>(inhalt.size()));
    }

    SubprocessMock& m_mock;
    std::deque<std::optional<std::string>> m_dateien;
};

} // namespace uft::tests::mocks
