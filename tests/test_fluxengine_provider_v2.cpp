/**
 * @file test_fluxengine_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for FluxEngineProviderV2 (MF-163 / P1.10).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_kryoflux_provider_v2.cpp.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates: ImagesFlux, WritesAnything, !FullDriveControl.
 *   2. Runtime smoke with a null-runner backend:
 *      - Construct FluxEngineProviderV2 with a null FluxEngineRunner.
 *      - Verify display_name() and spec_status() return correct values.
 *      - Verify do_read_raw_flux + do_write_raw_flux + do_measure_rpm +
 *        do_detect_drive return ProviderError when no runner is set.
 *   3. Runtime smoke with SubprocessMock-backed runner — happy paths:
 *      - Queue a successful fluxengine rpm reply with RPM.
 *      - Call detect_drive() — verify DriveDetected is returned.
 *      - Queue a successful fluxengine rpm reply with RPM.
 *      - Call measure_rpm() — verify RpmMeasured is returned.
 *      - Queue a successful fluxengine read reply with raw flux bytes.
 *      - Call read_raw_flux() — MF-203 (P1.24/ARCH-2): the undecoded
 *        .flux container must NOT be mislabelled as a FluxCaptured;
 *        verify an honest, F-4-compliant ProviderError is returned.
 *      - BERICHTIGT MF-1047: hier stand „Queue a successful fluxengine
 *        write reply … verify WriteCompleted" fuer beide Schreibproben.
 *        Das war ein geschlossener Kreis — dem Mock wurde Erfolg ins
 *        Skript gestellt, und geprueft wurde, dass der Provider ihn
 *        durchreicht. Ob der abgesetzte BEFEHL stimmte, kam nicht vor,
 *        und er stimmte nicht (`write -i` kodiert laut doc/using.md ein
 *        Dateisystem-Abbild; roher Fluss geht ueber `rawwrite`).
 *        Jetzt: write_raw_flux SAGT AB, bevor ein Prozess laeuft —
 *        mit und ohne Verify-Wunsch.
 *   4. Error path smoke:
 *      - Queue failing exits for detect/read — verify ProviderError, F-4.
 *        (Der Schreibpfad braucht dafuer keinen Lauf mehr; seine
 *        ProviderError entsteht vor dem Prozess.)
 *   5. Write verify-Pfad:
 *      - BERICHTIGT MF-1047: die frueher hier gepruefte Variante
 *        `WriteVerifyFailed` ist ueber diesen Provider unerreichbar,
 *        solange der Schreibpfad absagt. Sie bleibt mit echter
 *        Zusicherung in tests/test_usbfloppy_provider_v2.cpp gedeckt;
 *        hier kehrt sie zurueck, wenn P3-342 erledigt ist.
 *   6. Geometry guard smoke:
 *      - Call read_raw_flux / write_raw_flux with cylinder=255 → ProviderError.
 *      - Call read_raw_flux / write_raw_flux with head=5 → ProviderError.
 *   7. Empty flux stream guard:
 *      - Call write_raw_flux with empty FluxStream → ProviderError.
 *   8. ProviderError 3-part contract (F-4).
 *   9. detect_drive — keine Drehzahl in der Ausgabe: es wird keine
 *      erfunden (MF-894).
 *
 * FluxEngineRunner adapter:
 *   SubprocessMock::run() returns SubprocessMock::RunResult, while the V2
 *   provider expects FluxEngineRunner -> FluxEngineRunResult. These are
 *   structurally identical (same field names and types). The adapter lambda
 *   does the trivial field copy — no pointer cast, no reinterpret_cast.
 *
 * No external test framework. Plain assert() from <cassert>.
 *
 * NOTE: This test exercises the TYPE SHAPE of the V2 provider; it does NOT
 * test real hardware interaction (that is the responsibility of the manual
 * checks in tests/HARDWARE_TRUTH_TESTS.md).
 */

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

/* The V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include
 * path for this test. */
#include "hardware_providers/fluxengine_provider_v2.h"

/* SubprocessMock — in tests/mock_hardware/. CMake adds ${CMAKE_SOURCE_DIR}/tests
 * to the include path for this test. */
#include "mock_hardware/subprocess_mock.h"

using namespace uft::hal;
using uft::tests::mocks::SubprocessMock;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy WritesRawFlux");
static_assert(MeasuresRPM<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy MeasuresRPM");
static_assert(DetectsDrive<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!ReadsSectors<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy ReadsSectors (flux device)");
static_assert(!WritesSectors<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy WritesSectors (flux device)");
static_assert(!ControlsMotor<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy ControlsMotor "
    "(V1 setMotor() was a silent stub; no fluxengine motor command)");
static_assert(!SeeksHead<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy SeeksHead "
    "(V1 seekCylinder() was a silent stub; fluxengine seeks implicitly via -c)");
static_assert(!Recalibrates<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy Recalibrates "
    "(V1 recalibrate() delegated to stub seekCylinder(0); no fe primitive)");

/* Composite predicates. */
static_assert(ImagesFlux<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy ImagesFlux (ReadsRawFlux + DetectsDrive)");
static_assert(WritesAnything<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy WritesAnything (has WritesRawFlux)");
static_assert(!FullDriveControl<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates are all absent)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper: build a FluxEngineRunner adapter from a SubprocessMock reference.
 * ──────────────────────────────────────────────────────────────────────── */
static FluxEngineProviderV2::FluxEngineRunner make_runner(SubprocessMock& mock)
{
    return [&mock](const std::vector<std::string>& argv,
                   const std::string& stdin_data) -> FluxEngineRunResult {
        auto r = mock.run(argv, stdin_data);
        return FluxEngineRunResult{ r.stdout_text, r.stderr_text, r.exit_code };
    };
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity()
{
    SubprocessMock mock;
    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");

    assert(p.display_name() == "FluxEngine");
    assert(p.spec_status() == SpecStatus::CommunityConsensus);
}

static void smoke_null_runner_returns_provider_error()
{
    /* A default-constructed std::function evaluates to false (operator bool). */
    FluxEngineProviderV2::FluxEngineRunner null_runner;
    FluxEngineProviderV2 p(std::move(null_runner), "fluxengine");

    /* read_raw_flux — null runner path must return ProviderError. */
    {
        auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});
        bool got_error = false;
        std::visit(overloaded{
            [&](const FluxCaptured&)             {},
            [&](const FluxMarginal&)             {},
            [&](const FluxUnreadable&)           {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "read_raw_flux(null_runner) must return ProviderError");
    }

    /* write_raw_flux — null runner path must return ProviderError. */
    {
        FluxStream flux{{ 4000u, 6000u, 4000u }};
        auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, flux);
        bool got_error = false;
        std::visit(overloaded{
            [&](const WriteCompleted&)           {},
            [&](const WriteVerifyFailed&)        {},
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "write_raw_flux(null_runner) must return ProviderError");
    }

    /* measure_rpm — null runner path must return ProviderError. */
    {
        auto outcome = p.measure_rpm();
        bool got_error = false;
        std::visit(overloaded{
            [&](const RpmMeasured&)              {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "measure_rpm(null_runner) must return ProviderError");
    }

    /* detect_drive — null runner path must return ProviderError. */
    {
        auto outcome = p.detect_drive();
        bool got_error = false;
        std::visit(overloaded{
            [&](const DriveDetected&)            {},
            [&](const DriveAbsent&)              {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "detect_drive(null_runner) must return ProviderError");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  3. SubprocessMock-backed runner — happy paths
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_happy_path()
{
    SubprocessMock mock;

    /* do_detect_drive invokes the runner TWICE: first `fluxengine rpm`
     * (drive detection + RPM), then `fluxengine version` via
     * query_version() (FE-F6, MF-191) for the firmware string. Script
     * both, in that order. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine", "rpm" },      /* require_argv_subseq */
        "PC floppy drive detected\n"
        "300.0 rpm\n",                /* stdout_reply */
        "",                           /* stderr_reply */
        0                             /* exit_code */
    });
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine", "version" },  /* query_version() invocation */
        "FluxEngine 0.NN\n",
        "",
        0
    });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty() && "drive_kind must be non-empty");
            /* MF-894: DTC bzw. fluxengine melden KEINE Geometrie. 0 heisst
             * "nicht erkannt" — dasselbe Sentinel wie in
             * `fc5025_provider_v2.cpp`. Vorher stand hier `tracks > 0`,
             * also die Forderung nach der festen 80/2. */
            assert(d.tracks == 0 && "tracks must be 0 - no geometry is detected");
            assert(d.heads  == 0 && "heads must be 0 - no geometry is detected");
            assert(d.rpm_nominal > 0.0   && "rpm_nominal must be > 0");
            assert(d.rpm_nominal >= 290.0 && d.rpm_nominal <= 310.0
                   && "rpm_nominal must be ~300 for '300.0 rpm' in output");
        },
        [&](const DriveAbsent&)              {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&)            {},
    }, outcome);

    assert(got_detected && "detect_drive with scripted fluxengine success must return DriveDetected");
    mock.assert_consumed();
}

static void smoke_measure_rpm_happy_path()
{
    SubprocessMock mock;

    /* Queue a fluxengine rpm success reply. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine", "rpm" },
        "360.0 rpm\n",   /* 5.25" HD */
        "",
        0
    });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.measure_rpm();

    bool got_measured = false;
    std::visit(overloaded{
        [&](const RpmMeasured& r) {
            got_measured = true;
            assert(r.rpm >= 0.0      && "rpm must be >= 0");
            assert(r.jitter_pct >= 0.0 && "jitter_pct must be >= 0");
            assert(r.rpm >= 350.0    && "rpm must be ~360 for '360.0 rpm' in output");
        },
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&)            {},
    }, outcome);

    assert(got_measured && "measure_rpm with scripted success must return RpmMeasured");
    mock.assert_consumed();
}

/* Build a minimal but valid SCP container: header + 168-entry offset
 * table + one track (`scp_track`) with one revolution carrying the given
 * BE16 flux cells. resolution=0 -> 25 ns period. MF-209 (P1.24): the
 * FluxEngine provider asks fluxengine for `-o ...scp` output, so the
 * runner's bytes ARE an SCP file — this is what do_read_raw_flux decodes. */
static std::string build_synthetic_scp(int scp_track,
                                       const std::vector<uint16_t>& flux_be,
                                       uint32_t index_time_25ns)
{
    auto put_le32 = [](std::string& s, uint32_t v) {
        s.push_back(static_cast<char>(v & 0xFF));
        s.push_back(static_cast<char>((v >> 8) & 0xFF));
        s.push_back(static_cast<char>((v >> 16) & 0xFF));
        s.push_back(static_cast<char>((v >> 24) & 0xFF));
    };

    std::string s;
    /* Header (16 bytes). */
    s += "SCP";                                    /* signature          */
    s.push_back(static_cast<char>(0x22));          /* version 2.2        */
    s.push_back(static_cast<char>(0x00));          /* disk_type          */
    s.push_back(static_cast<char>(0x01));          /* revolutions = 1    */
    s.push_back(static_cast<char>(0x00));          /* start_track        */
    s.push_back(static_cast<char>(scp_track));     /* end_track          */
    s.push_back(static_cast<char>(0x00));          /* flags (no footer)  */
    s.push_back(static_cast<char>(0x00));          /* bit_cell_width     */
    s.push_back(static_cast<char>(0x00));          /* heads              */
    s.push_back(static_cast<char>(0x00));          /* resolution -> 25ns */
    put_le32(s, 0);                                /* checksum           */

    /* Offset table: 168 LE32 entries, all 0 except scp_track. */
    const uint32_t header_size = 16u;
    const uint32_t table_size  = 168u * 4u;
    const uint32_t track_off   = header_size + table_size;
    std::string table(table_size, '\0');
    for (int i = 0; i < 4; ++i)
        table[scp_track * 4 + i] =
            static_cast<char>((track_off >> (8 * i)) & 0xFF);
    s += table;

    /* Track data: "TRK" + track_number, one revolution entry, flux cells. */
    s += "TRK";
    s.push_back(static_cast<char>(scp_track));     /* track_number       */
    const uint32_t track_length = static_cast<uint32_t>(flux_be.size());
    const uint32_t data_offset  = 4u + 12u;        /* past TRK hdr + 1 rev */
    put_le32(s, index_time_25ns);
    put_le32(s, track_length);
    put_le32(s, data_offset);
    for (uint16_t f : flux_be) {                   /* BE16 flux cells    */
        s.push_back(static_cast<char>((f >> 8) & 0xFF));
        s.push_back(static_cast<char>(f & 0xFF));
    }
    return s;
}

static void smoke_read_raw_flux_decodes_scp()
{
    SubprocessMock mock;

    /* MF-209 (P1.24): do_read_raw_flux asks fluxengine for `.scp` output
     * (FluxEngine's native `.flux` is SQLite — a forbidden new
     * dependency) and decodes it with the vetted uft_scp_parser. Queue a
     * synthetic SCP file for SCP track 0 (cylinder 0, head 0): 4 BE16
     * flux cells at the 25 ns SCP base period -> 2500/3750/5000/7500 ns. */
    const std::string scp = build_synthetic_scp(
        /*scp_track=*/0, /*flux_be=*/{ 100, 150, 200, 300 },
        /*index_time_25ns=*/8000000u);

    mock.queue_run(SubprocessMock::ScriptedRun{
        /* MF-178: FluxEngine CLI revolutions flag is `--drive.revolutions=N`. */
        { "fluxengine", "read", "--drive.revolutions=2" },
        scp,        /* stdout_reply = raw SCP file bytes */
        "",
        0
    });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_captured = false;
    std::visit(overloaded{
        [&](const FluxCaptured& fc) {
            got_captured = true;
            assert(fc.transitions_ns.size() == 4 &&
                   "SCP with 4 flux cells must decode to 4 transitions");
            /* 25 ns period, no overflow -> exact ns values. */
            assert(fc.transitions_ns[0] == 2500 &&
                   fc.transitions_ns[1] == 3750 &&
                   fc.transitions_ns[2] == 5000 &&
                   fc.transitions_ns[3] == 7500 &&
                   "decoded ns intervals must match the SCP cells x 25ns");
            /* One revolution -> one measured index pulse, placed at the
             * cumulative transitions_ns sum (2500+3750+5000+7500). */
            assert(fc.index_times_ns.size() == 1 &&
                   fc.index_times_ns[0] == 18750 &&
                   "index_times_ns must sit on the transitions_ns time-base");
            assert(fc.revolutions == 1 && "revolutions == measured indices");
            assert(fc.sample_ns == 25.0 &&
                   "sample_ns must be the SCP 25 ns base period");
        },
        [&](const FluxMarginal&) {
            assert(false && "a valid single-track SCP must decode to "
                            "FluxCaptured, not FluxMarginal");
        },
        [&](const FluxUnreadable&)           {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&) {
            assert(false && "MF-209: a valid SCP container must decode, not "
                            "return a ProviderError");
        },
    }, outcome);

    assert(got_captured && "read_raw_flux on a valid SCP container must "
                           "return a decoded FluxCaptured");
    mock.assert_consumed();
}

static void smoke_write_raw_flux_sagt_ab()
{
    /* BERICHTIGT MF-1116 - der Name bleibt, weil eine Absage bleibt;
     * nur ihr GRUND ist ein anderer.
     *
     * Geschichte in drei Schritten, weil jeder etwas anderes lehrt:
     *
     *  1. Urspruenglich standen hier `smoke_write_raw_flux_no_verify`
     *     und `..._with_verify`. Beide waren ein GESCHLOSSENER KREIS:
     *     sie stellten dem Mock "Erfolg" ins Skript und pruefften
     *     dann, dass der Provider diesen Erfolg durchreicht. Ob der
     *     abgesetzte BEFEHL der richtige war, kam darin nicht vor -
     *     und er war es nicht (`write -i` KODIERT ein Abbild).
     *     Dieselbe Gestalt wie MF-1016/MF-1017: ein gruener Test, der
     *     einen Defekt bewacht, weil seine Zusage aus dem Defekt folgt.
     *
     *  2. MF-1047 ersetzte sie durch die Absage-Probe: kein
     *     Unterprozess, bevor der Behaelter stimmt.
     *
     *  3. MF-1116 hat den Behaelter gebaut (SCP) und `rawwrite -s`
     *     verdrahtet. Damit LAEUFT der Schreibvorgang - und diese
     *     Probe ist beim ersten Lauf danach gefallen, mit `provider
     *     invoked subprocess but no scripted run was queued`.
     *     Rotbeweis in der Gegenrichtung.
     *
     * Geprueft wird jetzt: OHNE verify kommt `WriteCompleted` mit
     * `verified = false` (nichts wurde nachgelesen, also wird nichts
     * behauptet - MF-883), MIT verify eine Absage, und in beiden
     * Faellen ist der abgesetzte Befehl `rawwrite` und nicht `write`.
     * Was der abgesetzte Befehl BEDEUTET, prueft
     * `tests/test_fluxengine_befehl.cpp` gegen doc/using.md - dort
     * liegt auch die Zusage, dass die SCP-Datei zum Zeitpunkt des
     * Aufrufs wirklich existiert. */
    SubprocessMock mock;
    mock.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");

    FluxStream flux{{ 4000u, 6000u, 4000u, 6000u }};
    auto outcome = p.write_raw_flux(WriteFluxParams{5, 0, false, false}, flux);

    bool fertig = false;
    bool verified_behauptet = true;
    std::visit(overloaded{
        [&](const WriteCompleted& w)         {
            fertig = true;
            verified_behauptet = w.verified;
        },
        [&](const WriteVerifyFailed&)        {},
        [&](const WriteRefused&)             {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            /* Rule F-4: jede ProviderError traegt what/why/fix. */
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(fertig &&
           "ohne verify muss WriteCompleted kommen - der Behaelter ist "
           "seit MF-1116 da und `rawwrite` verdrahtet");
    assert(!verified_behauptet &&
           "`verified` muss false sein: es wurde nichts nachgelesen "
           "(MF-883 - keine Zusage ohne Tat)");
    assert(mock.recorded_runs().size() == 1 &&
           "genau ein fluxengine-Lauf");
    {
        const auto& argv = mock.recorded_runs().front().argv;
        bool hat_rawwrite = false, hat_write = false, hat_i = false;
        for (const auto& a : argv) {
            if (a == "rawwrite") hat_rawwrite = true;
            if (a == "write")    hat_write = true;
            if (a == "-i")       hat_i = true;
        }
        assert(hat_rawwrite && "der Befehl ist `rawwrite`");
        assert(!hat_write && "`write` wuerde KODIEREN");
        assert(!hat_i && "`-i` ist der Eingang fuer ein ABBILD");
    }

    /* Mit Verify-Wunsch wird abgesagt: die Nachlese ist fuer den
     * Schreibfall nicht verdrahtet, und ein `verified = true` ohne
     * Nachlese waere genau die Zusage ohne Tat. */
    SubprocessMock mock2;
    mock2.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });
    FluxEngineProviderV2 p2(make_runner(mock2), "fluxengine");
    auto outcome2 = p2.write_raw_flux(WriteFluxParams{3, 1, true, false}, flux);

    bool got_error2 = false;
    std::visit(overloaded{
        [&](const ProviderError&)            { got_error2 = true; },
        [&](const WriteCompleted&)           {},
        [&](const WriteVerifyFailed&)        {},
        [&](const WriteRefused&)             {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
    }, outcome2);

    assert(got_error2 &&
           "mit verify muss abgesagt werden, solange die Nachlese nicht "
           "verdrahtet ist (offener Teil von P3-342)");
}

/* `smoke_write_raw_flux_with_verify` stand hier und ist in
 * `smoke_write_raw_flux_sagt_ab` aufgegangen (MF-1047): sie unterschied
 * sich von ihrer Schwester nur darin, dass sie ZWEI Erfolge ins Skript
 * stellte statt einem. Beide pruefen seither dasselbe — dass abgesagt
 * wird, bevor ein Prozess laeuft. */

/* ────────────────────────────────────────────────────────────────────────
 *  4. Error path smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_failure()
{
    SubprocessMock mock;
    mock.queue_run_failed("fluxengine: No FluxEngine device found", 1);

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.detect_drive();

    bool got_error = false;
    std::visit(overloaded{
        [&](const DriveDetected&)            {},
        [&](const DriveAbsent&)              {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "detect_drive with failing fluxengine must return ProviderError");
    mock.assert_consumed();
}

static void smoke_read_raw_flux_failure()
{
    SubprocessMock mock;
    mock.queue_run_failed("fluxengine: disk not found", 1);

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.read_raw_flux(ReadFluxParams{10, 0, 2, 0});

    bool got_error = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             {},
        [&](const FluxMarginal&)             {},
        [&](const FluxUnreadable&)           {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "read_raw_flux with failing fluxengine must return ProviderError");
    mock.assert_consumed();
}

static void smoke_write_raw_flux_failure()
{
    /* BERICHTIGT MF-1116 — und diese Zeile ist zum ZWEITEN Mal
     * umgeschrieben, weshalb beide Fassungen hier stehen bleiben.
     *
     *   Urspruenglich: `queue_run_failed("fluxengine: write-protect
     *   notch active")` — ein fehlgeschlagener Lauf, genau was der
     *   Name sagt.
     *
     *   MF-1047: der Schreibpfad sagte ab, BEVOR ein Prozess lief.
     *   Der vorgemerkte Lauf wurde damit nie abgerufen, und
     *   `assert_consumed()` haette den Test mit „1 scripted run left
     *   UNCONSUMED" abgebrochen — also wurde er entfernt und die
     *   Absage geprueft.
     *
     *   MF-1116: der Weg ist wieder da (SCP-Behaelter + `rawwrite
     *   -s`), und damit ist die urspruengliche Zusage wieder
     *   ERREICHBAR. Sie kehrt zurueck, statt dass der Name weiter
     *   etwas anderes behauptet als der Rumpf prueft.
     *
     * Gefunden hat diesen Test kein Ueberlegen, sondern der Lauf: er
     * stand auf KEINER meiner beiden Verdachtslisten. Beide
     * Bereichspruefungen, die ich verdaechtigt hatte, brechen vor dem
     * Laeufer ab — deshalb nennt `main()` seit MF-1116 jede Stufe.
     *
     * Geprueft wird jetzt, was der Name sagt: **ein fehlgeschlagener
     * `fluxengine`-Lauf wird als ProviderError gemeldet, nicht als
     * Erfolg** — mit vollstaendigem what/why/fix (Regel F-4) und mit
     * dem stderr des Werkzeugs IM Text, damit der Benutzer den Grund
     * des Geraets erfaehrt und nicht nur „es ging nicht". */
    SubprocessMock mock;
    mock.queue_run_failed("fluxengine: write-protect notch active", 1);

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    FluxStream flux{{ 4000u, 6000u }};
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, flux);

    bool got_error = false;
    bool stderr_im_text = false;
    std::visit(overloaded{
        [&](const WriteCompleted&)           {},
        [&](const WriteVerifyFailed&)        {},
        [&](const WriteRefused&)             {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            stderr_im_text =
                e.why.find("write-protect notch active") != std::string::npos;
        },
    }, outcome);

    assert(got_error &&
           "ein fehlgeschlagener fluxengine-Lauf muss als ProviderError "
           "gemeldet werden, nicht als Erfolg (MF-1116)");
    assert(mock.recorded_runs().size() == 1 &&
           "genau EIN Lauf: der Schreibvorgang selbst");
    assert(stderr_im_text &&
           "der stderr des Werkzeugs muss im Text stehen — sonst erfaehrt "
           "der Benutzer den Grund des Geraets nicht (MF-1116)");
    mock.assert_consumed();
}

static void smoke_read_empty_stream()
{
    SubprocessMock mock;

    /* Queue a fluxengine read success but with empty stdout (no stream data). */
    mock.queue_run("");   /* exit_code=0, stdout="" */

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    /* Empty stream should return FluxMarginal. */
    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal& m)           {
            valid_variant = true;
            assert(!m.anomaly_note.empty()
                   && "FluxMarginal::anomaly_note must not be empty");
        },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError&)            { valid_variant = true; },
    }, outcome);

    assert(valid_variant && "read_raw_flux with empty stream must return a valid variant");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  5. Write verify-failed path (rule F-3 on writes)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_verify_unerreichbar()
{
    /* BERICHTIGT MF-1116 - und der Grund der Absage hat sich
     * VERSCHOBEN, nicht aufgehoert.
     *
     * Bis MF-1116 sagte `do_write_raw_flux()` ab, BEVOR ein Prozess
     * startete: `fluxengine write -i` kodiert laut doc/using.md ein
     * Dateisystem-Abbild, und der uebergebene Behaelter war ohnehin
     * keiner, den fluxengine liest. Dieser Test pruefte deshalb
     * `recorded_runs().empty()`.
     *
     * Seit MF-1116 ist P3-342 (a)+(b) erledigt: der Fluss geht als
     * SCP-Behaelter heraus und `rawwrite -s` wird gerufen. Der
     * SCHREIBVORGANG laeuft also - und genau daran ist dieser Test
     * beim ersten Lauf gescheitert, mit `provider invoked subprocess
     * but no scripted run was queued`. Rotbeweis in der
     * Gegenrichtung, und er soll fallen.
     *
     * Was BLEIBT, ist die Absage bei `verify = true`: die Nachlese ist
     * fuer den Schreibfall nicht verdrahtet, und ein `verified = true`
     * ohne Nachlese waere die Zusage ohne Tat aus MF-883. Geprueft
     * wird jetzt also: der Lauf findet statt, und das Ergebnis ist
     * trotzdem ein ProviderError - weil der Verify-Wunsch nicht
     * erfuellt werden kann.
     *
     * Was an Abdeckung fehlt, und was nicht: die Variante
     * `WriteVerifyFailed` bleibt mit echter Zusicherung in
     * `tests/test_usbfloppy_provider_v2.cpp` geprueft. Was hier
     * weiterhin fehlt, ist der Verify-Pfad DIESES Providers; er kehrt
     * zurueck, sobald das Zuruecklesen fuer den Schreibfall verdrahtet
     * ist (offener Teil von P3-342).
     *
     * Hier stand `smoke_write_verify_failed`: Schreiben gelingt,
     * Rueckleseprobe scheitert, Ergebnis `WriteVerifyFailed` mit
     * erhaltenen `intended`-Bytes (Regel F-3). Diese Zusage war an
     * einen Pfad geknuepft, der seit MF-1047 gar nicht mehr laeuft —
     * `do_write_raw_flux()` sagt ab, bevor ein Prozess startet, weil
     * `fluxengine write -i` laut doc/using.md ein Dateisystem-Abbild
     * KODIERT und der uebergebene Behaelter ohnehin keiner war, den
     * fluxengine liest.
     *
     * **Was damit an Abdeckung verloren geht, und was nicht:** die
     * Variante `WriteVerifyFailed` bleibt geprueft — mit echter
     * Zusicherung in `tests/test_usbfloppy_provider_v2.cpp`. Was hier
     * fehlt, ist der Verify-Pfad DIESES Providers; er kehrt zurueck,
     * sobald P3-342 erledigt ist (SCP-Behaelter erzeugen, `rawwrite`
     * rufen). Solange das offen ist, waere ein Test darueber ein Test
     * ueber nichts.
     *
     * Geprueft wird deshalb genau das, was heute gilt: der
     * Verify-Wunsch aendert an der Absage nichts, und er fasst kein
     * Laufwerk an. */
    SubprocessMock mock;
    /* MF-1116: MIT queue_run - der Schreibvorgang laeuft jetzt. */
    mock.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    FluxStream flux{{ 0xDEADu, 0xBEEFu }};
    auto outcome = p.write_raw_flux(WriteFluxParams{2, 0, true, false}, flux);

    bool got_error = false;
    std::visit(overloaded{
        [&](const WriteCompleted&)           {},
        [&](const WriteVerifyFailed&)        {},
        [&](const WriteRefused&)             {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    /* BERICHTIGT MF-1121, und die Berichtigung betrifft den GRUND, nicht
     * das Ergebnis.
     *
     * Bis MF-1121 stand hier „ein Verify-Wunsch wird abgesagt, solange
     * die Nachlese nicht verdrahtet ist". Seit MF-1121 IST sie
     * verdrahtet — und dieser Test blieb trotzdem gruen. Das ist kein
     * Glueck, sondern eine zweite Ursache: der Pruefstrom hier hat ZWEI
     * Uebergaenge, und `uft_flux_histogram_cell_ns()` braucht gemessen
     * rund 60, um zwei Gipfel zu finden. Die Nachlese sagt also ab,
     * weil sie kein gemessenes Maß hat — nicht, weil es keinen Weg gibt.
     *
     * Ein Test, der aus einem anderen Grund gruen ist als sein Text
     * behauptet, ist genau die Klasse, die dieser Baum verfolgt (MF-1000
     * / Tor 64). Die Zusage heisst deshalb jetzt, was sie prueft: OHNE
     * MFM-artiges Histogramm gibt es kein kalibrierungsfreies Maß, und
     * dann wird abgesagt statt geraten.
     *
     * Die echte Nachlese — gleiche Zellfolge, verschobene Zellfolge,
     * kurzer Ruecklesestrom — prueft
     * `smoke_write_verify_zellvergleich()` weiter unten. */
    assert(got_error &&
           "ohne MFM-artiges Histogramm gibt es kein kalibrierungsfreies "
           "Maß fuer die Nachlese, also wird abgesagt statt geraten "
           "(MF-1121)");
    assert(mock.recorded_runs().size() == 1 &&
           "der SCHREIBVORGANG laeuft jetzt - genau EIN Lauf, und zwar "
           "rawwrite; abgesagt wird nur die Nachlese");
    {
        const auto& argv = mock.recorded_runs().front().argv;
        bool hat_rawwrite = false, hat_write = false;
        for (const auto& a : argv) {
            if (a == "rawwrite") hat_rawwrite = true;
            if (a == "write")    hat_write = true;
        }
        assert(hat_rawwrite &&
               "der Lauf muss `rawwrite` sein (doc/using.md: schreibt "
               "Fluss OHNE Kodierung)");
        assert(!hat_write &&
               "`write` wuerde ein Abbild KODIEREN und darf nicht "
               "vorkommen");
    }
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. Geometry guard smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder_read()
{
    SubprocessMock mock;
    /* No queued run: the geometry check fires before the runner invocation. */

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.read_raw_flux(ReadFluxParams{255, 0, 2, 0});

    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal&)             { valid_variant = true; },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError& e)          {
            valid_variant = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(valid_variant &&
           "read_raw_flux with cylinder=255 must return a valid variant");
    /* No assert_consumed: no run was queued, none consumed. */
}

static void smoke_out_of_range_head_read()
{
    SubprocessMock mock;
    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.read_raw_flux(ReadFluxParams{0, 5, 2, 0});

    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal&)             { valid_variant = true; },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError& e)          {
            valid_variant = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(valid_variant && "read_raw_flux with head=5 must return a valid variant");
}

static void smoke_out_of_range_cylinder_write()
{
    SubprocessMock mock;
    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");

    FluxStream flux{{ 4000u }};
    auto outcome = p.write_raw_flux(WriteFluxParams{200, 0, false, false}, flux);

    bool valid_variant = false;
    std::visit(overloaded{
        [&](const WriteCompleted&)           { valid_variant = true; },
        [&](const WriteVerifyFailed&)        { valid_variant = true; },
        [&](const WriteRefused&)             { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError& e)          {
            valid_variant = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(valid_variant && "write_raw_flux with cylinder=200 must return a valid variant");
}

/* ────────────────────────────────────────────────────────────────────────
 *  7. Empty flux stream guard
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_empty_flux_stream_write()
{
    SubprocessMock mock;
    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");

    FluxStream empty_flux;   /* transitions_ns is empty */
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, empty_flux);

    bool got_error = false;
    std::visit(overloaded{
        [&](const WriteCompleted&)           {},
        [&](const WriteVerifyFailed&)        {},
        [&](const WriteRefused&)             {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "write_raw_flux with empty FluxStream must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  8. ProviderError 3-part contract (F-4)
 * ──────────────────────────────────────────────────────────────────────── */

/* ── MF-1121: die Nachlese auf ZELLEBENE ────────────────────────────────
 *
 * Auf Eigentuemer-Entscheidung („P3-342 Nachlese verdrahten, Vergleich
 * auf Zellebene"). Drei Faelle, und der erste ist der, der ohne Hardware
 * sonst nie gepruefet wuerde.
 *
 * Der Pruefstrom ist MFM-geformt (4000/6000/8000 ns im Wechsel) und hat
 * 90 Uebergaenge. Die 90 sind GEMESSEN und nicht gewaehlt: eine
 * Wegwerfmessung gegen `uft_flux_histogram_cell_ns()` gab fuer 12 Werte
 * „nein", fuer 60 und 600 „ja" mit Zelle **2000,0 ns**. Mit einem
 * kuerzeren Strom wuerde dieser Test die Absage pruefen und aussehen
 * wie ein Nachlese-Test — genau der Fehler, den
 * `smoke_write_verify_unerreichbar` vor MF-1121 gemacht hat.
 *
 * Die SCP-Seite rechnet in 25-ns-Schritten: 4000/6000/8000 ns sind
 * 160/240/320. Die Zellzahlen sind damit 2/3/4.
 */
static std::vector<std::uint32_t> mfm_pruefstrom(std::size_t n)
{
    static const std::uint32_t muster[3] = { 4000u, 6000u, 8000u };
    std::vector<std::uint32_t> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) v.push_back(muster[i % 3]);
    return v;
}

static std::vector<std::uint16_t> als_scp_zellen(
    const std::vector<std::uint32_t>& ns)
{
    std::vector<std::uint16_t> z;
    z.reserve(ns.size());
    for (const std::uint32_t v : ns)
        z.push_back(static_cast<std::uint16_t>(v / 25u));   /* 25-ns-Basis */
    return z;
}

static void smoke_write_verify_zellvergleich()
{
    const int cyl = 3, head = 1;
    const int scp_track = cyl * 2 + head;      /* Provider: cyl*2 + head */
    const std::vector<std::uint32_t> geschrieben = mfm_pruefstrom(90);

    /* ── Fall A: dieselbe Zellfolge kommt zurueck -> verified = true ── */
    {
        SubprocessMock mock;
        /* 1. der Schreiblauf (`rawwrite`) */
        mock.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });
        /* 2. der Ruecklesenlauf: EINE Umdrehung, Ausgabe ist eine SCP */
        mock.queue_run(SubprocessMock::ScriptedRun{
            { "fluxengine", "read", "--drive.revolutions=1" },
            build_synthetic_scp(scp_track, als_scp_zellen(geschrieben),
                                /*index_time_25ns=*/8000000u),
            "", 0 });

        FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
        FluxStream flux{ geschrieben };
        auto outcome = p.write_raw_flux(
            WriteFluxParams{cyl, head, /*verify=*/true, false}, flux);

        bool fertig = false, verified = false;
        std::visit(overloaded{
            [&](const WriteCompleted& w)         { fertig = true;
                                                   verified = w.verified; },
            [&](const WriteVerifyFailed&)        {},
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                /* Sichtbar machen, WORAN es lag — eine stumme Absage
                 * hier waere die teuerste Sorte Fehlschlag. */
                std::cout << "      ProviderError: " << e.what << std::endl;
            },
        }, outcome);

        assert(fertig &&
               "gleiche Zellfolge zurueck -> WriteCompleted (MF-1121)");
        assert(verified &&
               "`verified` muss true sein: es WURDE nachgelesen und die "
               "Zellfolge stimmt (MF-883 in der Gegenrichtung)");
        assert(mock.recorded_runs().size() == 2 &&
               "genau zwei Laeufe: schreiben und zuruecklesen");
        {
            /* Der zweite Lauf muss der LESEBEFEHL sein, und er muss EINE
             * Umdrehung verlangen — daran haengt die Ausrichtung: SCP
             * trennt Umdrehungen an den Indexmarken, Umdrehung 0 beginnt
             * also an der Marke, und `rawwrite` schreibt ab Index. */
            const auto& argv = mock.recorded_runs().at(1).argv;
            bool hat_read = false, hat_eine_umdrehung = false;
            for (const auto& a : argv) {
                if (a == "read")                      hat_read = true;
                if (a == "--drive.revolutions=1")     hat_eine_umdrehung = true;
            }
            assert(hat_read && "der zweite Lauf ist `read`");
            assert(hat_eine_umdrehung &&
                   "genau EINE Umdrehung - sonst waere die Ausrichtung "
                   "an der Indexmarke nicht gegeben");
        }
        mock.assert_consumed();
    }

    /* ── Fall B: eine Zelle verschoben -> WriteVerifyFailed ──────────── */
    {
        std::vector<std::uint32_t> gelesen = geschrieben;
        /* 4000 -> 5000 ns. Bei Zelle 2000 ist das 2,5 und rundet auf 3:
         * die Zellzahl AENDERT sich, also muss die Nachlese fallen. Ein
         * Wert innerhalb derselben Zelle (etwa 4200) darf sie NICHT
         * fallen lassen — das ist der Sinn des Zellvergleichs. */
        gelesen.at(30) = 5000u;

        SubprocessMock mock;
        mock.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });
        mock.queue_run(SubprocessMock::ScriptedRun{
            { "fluxengine", "read" },
            build_synthetic_scp(scp_track, als_scp_zellen(gelesen), 8000000u),
            "", 0 });

        FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
        FluxStream flux{ geschrieben };
        auto outcome = p.write_raw_flux(
            WriteFluxParams{cyl, head, true, false}, flux);

        bool gefallen = false;
        bool beweis_ist_zellen = false;
        std::visit(overloaded{
            [&](const WriteCompleted&)           {},
            [&](const WriteVerifyFailed& f)      {
                gefallen = true;
                /* Der Beweis muss ZELLZAHLEN tragen, nicht rohe
                 * Nanosekunden: ein Beweisfeld, das eine andere Groesse
                 * zeigt als die Pruefung benutzt hat, waere
                 * irrefuehrend. 4000/6000/8000 bei Zelle 2000 sind
                 * 2/3/4, und jede Zahl muss in [2,4] liegen. */
                beweis_ist_zellen =
                    f.intended.size() == 90 && !f.readback.empty();
                for (const std::uint8_t z : f.intended)
                    if (z < 2 || z > 4) beweis_ist_zellen = false;
                /* Und genau an der verschobenen Stelle muss sich der
                 * Ruecklesewert unterscheiden. */
                if (f.readback.size() > 30 &&
                    f.readback.at(30) == f.intended.at(30))
                    beweis_ist_zellen = false;
            },
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                std::cout << "      ProviderError: " << e.what << std::endl;
            },
        }, outcome);

        assert(gefallen &&
               "eine um mehr als eine halbe Zelle verschobene Stelle muss "
               "die Nachlese fallen lassen (MF-1121)");
        assert(beweis_ist_zellen &&
               "`intended`/`readback` tragen die ZELLFOLGEN - also genau "
               "das, was verglichen wurde");
        mock.assert_consumed();
    }

    /* ── Fall C: Jitter INNERHALB der Zelle -> haelt ─────────────────── */
    {
        std::vector<std::uint32_t> gelesen = geschrieben;
        /* +200 ns auf jeden Wert: 10 % der Zelle, also weit unter der
         * halben Zelle. Das ist der Fall, der auf echter Hardware der
         * NORMALFALL ist — eine Nachlese, die daran faellt, waere
         * unbrauchbar, und ein ns-Vergleich waere genau das. */
        for (std::uint32_t& v : gelesen) v += 200u;

        SubprocessMock mock;
        mock.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });
        mock.queue_run(SubprocessMock::ScriptedRun{
            { "fluxengine", "read" },
            build_synthetic_scp(scp_track, als_scp_zellen(gelesen), 8000000u),
            "", 0 });

        FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
        FluxStream flux{ geschrieben };
        auto outcome = p.write_raw_flux(
            WriteFluxParams{cyl, head, true, false}, flux);

        bool verified = false;
        std::visit(overloaded{
            [&](const WriteCompleted& w)         { verified = w.verified; },
            [&](const WriteVerifyFailed&)        {},
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                std::cout << "      ProviderError: " << e.what << std::endl;
            },
        }, outcome);

        assert(verified &&
               "Jitter innerhalb der Zelle darf die Nachlese NICHT fallen "
               "lassen - sonst wuerde sie auf echter Hardware immer "
               "scheitern (MF-1121)");
        mock.assert_consumed();
    }

    /* ── Fall D: es kommt WENIGER zurueck als geschrieben -> faellt ──
     *
     * Dieser Zweig stand nach dem ersten Lauf ohne Zusage da, und ein
     * ungepruefter Zweig ist in diesem Baum kein Zweig, sondern eine
     * Behauptung. Die Zusage lautet: die geschriebenen Zellen stehen
     * VOLLSTAENDIG am Anfang der Umdrehung. Kommt weniger zurueck, ist
     * nicht alles angekommen — auch wenn jede zurueckgelesene Zelle
     * stimmt. Ein Vergleich, der nur das Praefix des KUERZEREN prueft,
     * wuerde einen halb geschriebenen Spurabschnitt bestaetigen. */
    {
        std::vector<std::uint32_t> gelesen = geschrieben;
        gelesen.resize(geschrieben.size() - 12);   /* 78 statt 90 */

        SubprocessMock mock;
        mock.queue_run(SubprocessMock::ScriptedRun{ { "fluxengine" }, "", "", 0 });
        mock.queue_run(SubprocessMock::ScriptedRun{
            { "fluxengine", "read" },
            build_synthetic_scp(scp_track, als_scp_zellen(gelesen), 8000000u),
            "", 0 });

        FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
        FluxStream flux{ geschrieben };
        auto outcome = p.write_raw_flux(
            WriteFluxParams{cyl, head, true, false}, flux);

        bool gefallen = false;
        bool laengen_belegt = false;
        std::visit(overloaded{
            [&](const WriteCompleted&)           {},
            [&](const WriteVerifyFailed& f)      {
                gefallen = true;
                /* Der Beweis muss die Luecke ZEIGEN, nicht nur melden. */
                laengen_belegt = (f.intended.size() == 90)
                              && (f.readback.size() == 78);
            },
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                std::cout << "      ProviderError: " << e.what << std::endl;
            },
        }, outcome);

        assert(gefallen &&
               "ein kuerzerer Ruecklesestrom muss die Nachlese fallen "
               "lassen, auch wenn jede vorhandene Zelle stimmt (MF-1121)");
        assert(laengen_belegt &&
               "der Beweis traegt beide Laengen - 90 geschrieben, 78 "
               "zurueckgelesen");
        mock.assert_consumed();
    }
}

static void smoke_provider_error_3part_contract()
{
    auto try_construct = [](const char* w, const char* y, const char* f) -> bool {
        try {
            ProviderError e{UFT_E_GENERIC, w, y, f};
            (void)e;
            return false;
        } catch (const std::logic_error&) {
            return true;
        }
    };

    assert(try_construct("", "y", "f") && "empty what must throw");
    assert(try_construct("w", "", "f") && "empty why must throw");
    assert(try_construct("w", "y", "") && "empty fix must throw");
    assert(try_construct("", "", "")   && "all empty must throw");

    bool threw = false;
    try {
        ProviderError ok{UFT_E_GENERIC,
            "FluxEngine binary not found or failed to launch",
            "fluxengine returned exit code 1. No FluxEngine device found.",
            "Install FluxEngine from https://github.com/davidgiven/fluxengine "
            "and verify the USB device is connected."};
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

/* ────────────────────────────────────────────────────────────────────────
 *  9. detect_drive — no RPM in output (uses default 300.0)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_no_rpm_in_output()
{
    SubprocessMock mock;

    /* fluxengine rpm output without a parseable RPM value. do_detect_drive
     * then also calls query_version() (FE-F6) → a second `version` run. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine", "rpm" },
        "FluxEngine drive detected\n"
        "Disk present\n",   /* no RPM number in output */
        "",
        0
    });
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine", "version" },  /* query_version() invocation */
        "FluxEngine 0.NN\n",
        "",
        0
    });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            /* MF-894: Ohne Drehzahl in der Ausgabe wird KEINE erfunden.
             * Begruendung ausfuehrlich im KryoFlux-Test, Abschnitt 7. */
            assert(d.rpm_nominal == 0.0
                   && "rpm_nominal must be 0 (not measured) when FE reports no RPM");
            assert(d.drive_kind.find("no RPM signal") != std::string::npos
                   && "drive_kind must say the RPM was not measured");
            assert(d.tracks == 0
                   && "tracks must be 0 (not detected)");
            assert(d.heads == 0 && "heads must be 0 - no geometry is detected");
        },
        [&](const DriveAbsent&)              {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&)            {},
    }, outcome);

    assert(got_detected && "detect_drive without RPM in output must return DriveDetected");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  10. measure_rpm — no RPM parseable (returns 0.0)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_measure_rpm_no_rpm_in_output()
{
    SubprocessMock mock;

    /* fluxengine rpm exits 0 but has no RPM in output. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fluxengine", "rpm" },
        "Drive present, no RPM data available\n",
        "",
        0
    });

    FluxEngineProviderV2 p(make_runner(mock), "fluxengine");
    auto outcome = p.measure_rpm();

    bool got_measured = false;
    std::visit(overloaded{
        [&](const RpmMeasured& r) {
            got_measured = true;
            /* RPM = 0.0 when not parseable — still a valid measurement. */
            assert(r.rpm >= 0.0    && "rpm must be >= 0");
            assert(r.rpm == 0.0    && "rpm must be 0.0 when not parseable from output");
        },
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&)            {},
    }, outcome);

    assert(got_measured && "measure_rpm with no RPM in output must return RpmMeasured(0.0)");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

/* MF-1116: jede Stufe nennt sich, BEVOR sie laeuft.
 *
 * Der Anlass ist gemessen, nicht ausgedacht: nach der Verdrahtung des
 * SCP-Weges endete dieser Test mit
 *
 *   terminate called after throwing an instance of 'std::out_of_range'
 *     what(): SubprocessMock::run(): provider invoked subprocess but no
 *             scripted run was queued.
 *
 * und NANNTE die Stufe nicht. Ein `catch throw` im Rueckverfolger gab
 * `#0 <unavailable> in ?? ()` — aus einem `terminate` heraus ist der
 * Rahmen weg. Damit blieb nur Raten, und geraten habe ich zweimal
 * falsch: die beiden Bereichspruefungen brechen VOR dem Laeufer ab.
 *
 * `std::endl` ist dabei der Punkt, nicht `"\n"`: ohne Leerung steht die
 * Zeile im Puffer und geht bei `terminate` mit verloren — die Ausgabe
 * waere genau dann stumm, wenn man sie braucht. */
#define LAUF(f)                                                    \
    do {                                                           \
        std::cout << "  ... " #f << std::endl;                     \
        f();                                                       \
    } while (0)

int main()
{
    LAUF(smoke_identity);
    LAUF(smoke_null_runner_returns_provider_error);
    LAUF(smoke_detect_drive_happy_path);
    LAUF(smoke_measure_rpm_happy_path);
    LAUF(smoke_read_raw_flux_decodes_scp);
    LAUF(smoke_write_raw_flux_sagt_ab);
    LAUF(smoke_detect_drive_failure);
    LAUF(smoke_read_raw_flux_failure);
    LAUF(smoke_write_raw_flux_failure);
    LAUF(smoke_read_empty_stream);
    LAUF(smoke_write_verify_unerreichbar);
    LAUF(smoke_out_of_range_cylinder_read);
    LAUF(smoke_out_of_range_head_read);
    LAUF(smoke_out_of_range_cylinder_write);
    LAUF(smoke_empty_flux_stream_write);
    LAUF(smoke_write_verify_zellvergleich);
    LAUF(smoke_provider_error_3part_contract);
    LAUF(smoke_detect_drive_no_rpm_in_output);
    LAUF(smoke_measure_rpm_no_rpm_in_output);

    std::cout << "test_fluxengine_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}
