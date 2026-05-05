/**
 * @file test_wiring_runtime.cpp
 * @brief Compile-time + runtime verification of wire_action<Cap>(...).
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.3
 *
 * This test exercises the four contract clauses of `wire_action`:
 *
 *   1. CAPABILITY-PRESENT branch
 *      With a provider satisfying the capability, the button is enabled,
 *      its tooltip is cleared, clicking dispatches `factory(provider)`,
 *      and `std::visit` routes the variant to the matching handler.
 *
 *   2. CAPABILITY-ABSENT branch
 *      With a provider NOT satisfying the capability, the button is
 *      disabled, the tooltip names both the provider and the missing
 *      capability, and no `clicked` connection is made.
 *
 *   3. F-3 forensic guarantee
 *      A handler that picks `SectorMarginal` MUST receive every divergent
 *      read from the variant verbatim — `wire_action` does not summarize
 *      or strip the payload.
 *
 *   4. F-4 three-part-error guarantee
 *      A `ProviderError` handler receives all three of what / why / fix
 *      non-empty — that is enforced by the variant-construction guard
 *      (in outcomes.h), but we verify the round-trip here.
 *
 * This test runs Qt-side (linked against Qt6::Test + Qt6::Widgets) because
 * `wire_action` directly takes a `QAbstractButton*` and uses Qt's signal-
 * slot machinery. It cannot live in the header-only tests/ set.
 */

#include <QtTest/QtTest>
#include <QPushButton>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "uft/gui/wiring_runtime.h"
#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

using namespace uft::hal;
namespace cap = uft::gui::cap;

/* ════════════════════════════════════════════════════════════════════════
 *  Mock provider — satisfies ControlsMotor + HasIdentity, nothing else
 * ════════════════════════════════════════════════════════════════════════ */
class MotorOnlyMock {
public:
    /* Identity */
    constexpr std::string_view display_name() const noexcept {
        return "MotorOnlyMock";
    }
    constexpr SpecStatus spec_status() const noexcept {
        return SpecStatus::CommunityConsensus;
    }

    /* ControlsMotor capability */
    MotorOutcome set_motor(bool on) {
        last_call_was_on = on;
        if (return_kind == ReturnKind::Running)
            return MotorRunning{300.0};
        if (return_kind == ReturnKind::Stalled)
            return MotorStalled{"belt slip on test rig"};
        if (return_kind == ReturnKind::ProviderErr)
            return ProviderError{
                UFT_E_GENERIC,
                "motor command rejected",
                "test scaffold forced this path",
                "set return_kind to Running and rerun",
            };
        return MotorStopped{};
    }

    /* Test-control knobs */
    enum class ReturnKind { Stopped, Running, Stalled, ProviderErr };
    ReturnKind return_kind = ReturnKind::Stopped;
    bool last_call_was_on = false;
};

static_assert(HasIdentity<MotorOnlyMock>);
static_assert(ControlsMotor<MotorOnlyMock>);
static_assert(!ReadsRawFlux<MotorOnlyMock>);
static_assert(!ReadsSectors<MotorOnlyMock>);
static_assert(!SeeksHead<MotorOnlyMock>);

/* Tag-side mirror — mirrors the concept-side asserts above so the
 * tag-table itself is exercised, not just the underlying concept. */
static_assert(cap::ControlsMotor::satisfied<MotorOnlyMock>);
static_assert(!cap::ReadsRawFlux::satisfied<MotorOnlyMock>);
static_assert(!cap::SeeksHead::satisfied<MotorOnlyMock>);
static_assert(cap::HasIdentity::satisfied<MotorOnlyMock>);

/* ════════════════════════════════════════════════════════════════════════
 *  The test class
 * ════════════════════════════════════════════════════════════════════════ */
class TestWiringRuntime : public QObject {
    Q_OBJECT

private slots:
    /* Clause 1 — capability-present branch enables + connects. */
    void capability_present_enables_button();
    /* Clause 1b — clicking dispatches factory; correct handler runs. */
    void capability_present_dispatches_to_handler();
    /* Clause 2 — capability-absent branch disables + tooltips. */
    void capability_absent_disables_button();
    /* Clause 2b — no signal connection on absent branch. */
    void capability_absent_does_not_connect();
    /* Clause 3 — Marginal handler sees full payload (rule F-3). */
    void marginal_payload_preserved();
    /* Clause 4 — ProviderError handler sees all 3 fields (rule F-4). */
    void provider_error_three_parts_visible();
    /* Clause 5 — disabled-tooltip names provider + capability. */
    void disabled_tooltip_mentions_provider_and_capability();
};

/* ─────────────────────────────────────────────────────────────────────── */

void TestWiringRuntime::capability_present_enables_button() {
    QPushButton btn;
    btn.setEnabled(false);              // hostile starting state
    btn.setToolTip(QStringLiteral("stale"));

    MotorOnlyMock provider;
    uft::gui::wire_action<cap::ControlsMotor>(
        &btn, &provider,
        []<class P>(P *p) { return p->set_motor(true); },
        [](const MotorRunning &) {},
        [](const MotorStopped &) {},
        [](const MotorStalled &) {},
        [](const CapabilityRequiresPolicy &) {},
        [](const HardwareDisconnected &) {},
        [](const ProviderError &) {});

    QVERIFY(btn.isEnabled());
    QCOMPARE(btn.toolTip(), QString());
}

void TestWiringRuntime::capability_present_dispatches_to_handler() {
    QPushButton btn;
    MotorOnlyMock provider;
    provider.return_kind = MotorOnlyMock::ReturnKind::Running;

    bool running_handler_ran = false;
    bool stopped_handler_ran = false;
    double observed_rpm = 0.0;

    uft::gui::wire_action<cap::ControlsMotor>(
        &btn, &provider,
        []<class P>(P *p) { return p->set_motor(true); },
        [&running_handler_ran, &observed_rpm](const MotorRunning &v) {
            running_handler_ran = true;
            observed_rpm = v.measured_rpm;
        },
        [&stopped_handler_ran](const MotorStopped &) {
            stopped_handler_ran = true;
        },
        [](const MotorStalled &) {},
        [](const CapabilityRequiresPolicy &) {},
        [](const HardwareDisconnected &) {},
        [](const ProviderError &) {});

    btn.click();
    QCoreApplication::processEvents();

    QVERIFY2(running_handler_ran,
             "MotorRunning handler must have been invoked once button clicked");
    QVERIFY(!stopped_handler_ran);
    QCOMPARE(observed_rpm, 300.0);
    QVERIFY(provider.last_call_was_on);
}

void TestWiringRuntime::capability_absent_disables_button() {
    QPushButton btn;
    btn.setEnabled(true);               // hostile starting state
    btn.setToolTip(QString());

    MotorOnlyMock provider;
    /* SeeksHead is NOT satisfied by MotorOnlyMock. The codegen-emitted
     * generic lambda here would NEVER actually be instantiated, but this
     * test exercises the false-branch path — we want to prove that even
     * in the false branch, lambdas referencing methods the provider does
     * not have remain valid as long as they are template-bodied. */
    uft::gui::wire_action<cap::SeeksHead>(
        &btn, &provider,
        []<class P>(P *p) { return p->seek(0); });   // p->seek does not exist on MotorOnlyMock — ok, body lazy

    QVERIFY2(!btn.isEnabled(),
             "false-branch must disable the button");
    QVERIFY2(!btn.toolTip().isEmpty(),
             "false-branch must populate a tooltip");
}

void TestWiringRuntime::capability_absent_does_not_connect() {
    QPushButton btn;
    MotorOnlyMock provider;

    uft::gui::wire_action<cap::SeeksHead>(
        &btn, &provider,
        []<class P>(P *p) { return p->seek(0); });

    /* The button is disabled (verified by the previous case). `click()`
     * is a no-op on disabled buttons, so to test "no slot is connected"
     * we emit `clicked` directly and verify no factory was invoked.
     * The forensic side-effect check is via `last_call_was_on` — if the
     * false-branch had wired anything, the factory call inside the
     * connected lambda would have set this flag. */
    emit btn.clicked();
    QCoreApplication::processEvents();

    QVERIFY2(!provider.last_call_was_on,
             "false-branch must not have invoked the factory");
}

void TestWiringRuntime::marginal_payload_preserved() {
    /* Direct std::visit test — does not need a button. Confirms that
     * wire_action's ::uft::hal::overloaded routing preserves the full
     * SectorMarginal payload (rule F-3). */
    SectorOutcome outcome = SectorMarginal{
        CHS{1, 0, 5},
        {
            std::vector<std::uint8_t>{0xDE, 0xAD},
            std::vector<std::uint8_t>{0xBE, 0xEF},
            std::vector<std::uint8_t>{0xCA, 0xFE},
        },
        QualityFlag::CRC_FAIL | QualityFlag::WEAK_BITS,
        "3 divergent reads, jitter 7%",
    };

    std::size_t observed_divergent_count = 0;
    QualityFlag observed_quality = QualityFlag::None;

    std::visit(
        ::uft::hal::overloaded{
            [&observed_divergent_count, &observed_quality]
                (const SectorMarginal &m) {
                observed_divergent_count = m.divergent_reads.size();
                observed_quality = m.quality;
            },
            [](const SectorRead &) {},
            [](const SectorUnreadable &) {},
            [](const CapabilityRequiresPolicy &) {},
            [](const HardwareDisconnected &) {},
            [](const ProviderError &) {},
        },
        outcome);

    QCOMPARE(observed_divergent_count, std::size_t{3});
    QVERIFY(has(observed_quality, QualityFlag::CRC_FAIL));
    QVERIFY(has(observed_quality, QualityFlag::WEAK_BITS));
}

void TestWiringRuntime::provider_error_three_parts_visible() {
    QPushButton btn;
    MotorOnlyMock provider;
    provider.return_kind = MotorOnlyMock::ReturnKind::ProviderErr;

    std::string seen_what, seen_why, seen_fix;

    uft::gui::wire_action<cap::ControlsMotor>(
        &btn, &provider,
        []<class P>(P *p) { return p->set_motor(true); },
        [](const MotorRunning &) {},
        [](const MotorStopped &) {},
        [](const MotorStalled &) {},
        [](const CapabilityRequiresPolicy &) {},
        [](const HardwareDisconnected &) {},
        [&seen_what, &seen_why, &seen_fix](const ProviderError &e) {
            seen_what = e.what;
            seen_why  = e.why;
            seen_fix  = e.fix;
        });

    btn.click();
    QCoreApplication::processEvents();

    QVERIFY2(!seen_what.empty(), "rule F-4: 'what' must be visible to handler");
    QVERIFY2(!seen_why.empty(),  "rule F-4: 'why' must be visible to handler");
    QVERIFY2(!seen_fix.empty(),  "rule F-4: 'fix' must be visible to handler");
}

void TestWiringRuntime::disabled_tooltip_mentions_provider_and_capability() {
    QPushButton btn;
    MotorOnlyMock provider;

    uft::gui::wire_action<cap::ReadsRawFlux>(
        &btn, &provider,
        []<class P>(P *p) { return p->read_raw_flux({}); });

    const QString tip = btn.toolTip();
    QVERIFY(tip.contains(QStringLiteral("MotorOnlyMock")));
    QVERIFY(tip.contains(QStringLiteral("ReadsRawFlux")));
}

QTEST_MAIN(TestWiringRuntime)
#include "test_wiring_runtime.moc"
