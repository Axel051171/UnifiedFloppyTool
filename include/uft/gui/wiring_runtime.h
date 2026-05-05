/**
 * @file wiring_runtime.h
 * @brief Runtime template for type-driven GUI action wiring.
 *
 * Refactor branch: refactor/type-driven-hal
 * Spec:           docs/REFACTOR_BRIEF.md §6  (rule H-3, H-4)
 * Task:           docs/REFACTOR_TASKS.md  P1.3
 *
 * ROLE
 * ----
 * `wire_action<Cap>(btn, provider, factory, on_outcome_handlers...)` is the
 * runtime half of the codegen contract. The codegen at
 * `tools/wiring_codegen.py` consumes `forms/*.actions.yaml` + `forms/*.ui`
 * and emits one `wire_action<cap::X>(...)` call per declared GUI action.
 *
 * This header makes that contract real:
 *   - If the bound provider type satisfies the capability concept, the
 *     button is enabled and `clicked` is connected. The connected slot
 *     calls `factory(provider)`, gets back the matching `*Outcome` variant,
 *     and `std::visit`s it across the supplied handlers.
 *   - If the bound provider type does NOT satisfy the concept, the button
 *     is disabled and a tooltip explains which capability is missing.
 *     Rule H-3: an action without a backing capability cannot be reached.
 *
 * Single template, dual branch via `if constexpr`. The brief promises that
 * shape; we keep that promise.
 *
 * WHY A `cap::` TAG STRUCT, NOT THE CONCEPT DIRECTLY
 * ---------------------------------------------------
 * C++20 concepts are NOT first-class type-template arguments — there is no
 * `template<concept C, ...>` syntax. Codegen-emitted code therefore needs a
 * non-template-template-parameter handle. Each tag below mirrors exactly
 * one concept from `include/uft/hal/concepts.h` and exposes:
 *
 *     CapTag::satisfied<P>   // constexpr bool — does P meet the concept?
 *     CapTag::name           // string_view — for tooltip / diagnostic
 *
 * The mirror is intentional. Adding a new concept requires adding both the
 * concept (foundation, P0 — protected during P1) AND a tag here AND the
 * matching entry in `tools/wiring_codegen.py`'s `KNOWN_CAPABILITIES`. The
 * `single-source-enforcer` agent + the foundation test gate keep the three
 * lists in sync.
 *
 * WHY INVOKE IS A GENERIC LAMBDA, NOT A `()->Outcome` CLOSURE
 * ------------------------------------------------------------
 * For a non-generic lambda `[provider]() { return provider->method(); }`,
 * the body is type-checked eagerly when the closure type is created — i.e.
 * BEFORE `wire_action` is even called. If `provider` lacks `method`, the
 * codegen-emitted code fails at the call-site of wire_action, not inside
 * the if-constexpr branch.
 *
 * To allow the false-branch (capability-not-satisfied) to compile cleanly
 * we accept invoke as a *generic* lambda whose operator() is itself a
 * template:
 *
 *     []<class P>(P *p) { return p->set_motor(true); }
 *
 * Generic-lambda bodies are template specializations, instantiated only
 * when called. The if-constexpr inside `wire_action` ensures the call only
 * happens when `Cap::satisfied<Provider>` — so the body is only
 * instantiated against types that actually have the method.
 *
 * THREADING CONTRACT
 * ------------------
 * `wire_action` connects the button's `clicked` signal to a lambda whose
 * receiver context is the button itself. Per `Qt::AutoConnection`, that
 * lambda runs on the GUI thread.
 *
 * Provider operations may block (USB I/O, subprocess invocation,
 * QSerialPort flush). The caller of `wire_action` is responsible for
 * supplying a `provider` whose `do_*` methods are non-blocking on the GUI
 * thread — the production pattern is the worker-thread routing landed in
 * MF-147 (`src/hardware_providers/hardwaremanager.cpp`), where the V2
 * provider lives on a `QThread` and its method calls are dispatched via
 * `QMetaObject::invokeMethod(... Qt::QueuedConnection ...)`.
 *
 * `wire_action` itself does NOT off-load. Doing so would either lock us
 * into one threading model or hide the off-load from the caller — both
 * pragmatic compromises we refuse.
 *
 * RULE F-3 (preserve forensic detail) IS THE VISITOR'S JOB
 * --------------------------------------------------------
 * `wire_action` doesn't summarize the outcome — it `std::visit`s. If a
 * caller fails to provide a handler for an alternative of the variant,
 * `std::visit` does not compile (no viable `operator()`). That is the
 * forensic guarantee: a new variant alternative added to e.g.
 * `MotorOutcome` later forces every wire-up site to handle it explicitly.
 *
 * Pure header file. No runtime cost beyond Qt's signal-slot machinery.
 */
#ifndef UFT_GUI_WIRING_RUNTIME_H
#define UFT_GUI_WIRING_RUNTIME_H

#include <QAbstractButton>
#include <QObject>
#include <QString>

#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

namespace uft::gui {

/* ════════════════════════════════════════════════════════════════════════
 *  Capability tags  (mirror of uft::hal:: concepts)
 *
 *  Each tag has the SAME NAME as the concept it mirrors. They live in a
 *  different namespace to avoid identifier collision when both headers
 *  are visible.
 *
 *  Adding a new tag requires:
 *    1. matching concept added to include/uft/hal/concepts.h (foundation)
 *    2. tag added here (this file)
 *    3. entry added to KNOWN_CAPABILITIES in tools/wiring_codegen.py
 *  The `single-source-enforcer` agent verifies the three are in sync.
 * ════════════════════════════════════════════════════════════════════════ */
namespace cap {

#define UFT_DEFINE_CAP_TAG(Name)                                              \
    struct Name {                                                             \
        template<class P>                                                     \
        static constexpr bool satisfied = ::uft::hal::Name<P>;                \
        static constexpr std::string_view name = #Name;                       \
    }

UFT_DEFINE_CAP_TAG(HasIdentity);
UFT_DEFINE_CAP_TAG(ReadsSectors);
UFT_DEFINE_CAP_TAG(ReadsRawFlux);
UFT_DEFINE_CAP_TAG(WritesSectors);
UFT_DEFINE_CAP_TAG(WritesRawFlux);
UFT_DEFINE_CAP_TAG(ControlsMotor);
UFT_DEFINE_CAP_TAG(SeeksHead);
UFT_DEFINE_CAP_TAG(Recalibrates);
UFT_DEFINE_CAP_TAG(MeasuresRPM);
UFT_DEFINE_CAP_TAG(DetectsDrive);

UFT_DEFINE_CAP_TAG(ImagesFlux);
UFT_DEFINE_CAP_TAG(ImagesSectors);
UFT_DEFINE_CAP_TAG(WritesAnything);
UFT_DEFINE_CAP_TAG(FullDriveControl);

#undef UFT_DEFINE_CAP_TAG

}  // namespace cap

/* ════════════════════════════════════════════════════════════════════════
 *  Internal helper — render a not-supported tooltip.
 *
 *  Pulled out so the disabled-branch is a single line and we have ONE
 *  spelling for the tooltip text (auditors / translators look here).
 * ════════════════════════════════════════════════════════════════════════ */
namespace detail {

template<class CapTag, class Provider>
inline QString not_supported_tooltip(const Provider *provider) {
    QString name;
    if constexpr (::uft::hal::HasIdentity<Provider>) {
        const auto sv = provider->display_name();
        name = QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
    } else {
        name = QStringLiteral("the current provider");
    }
    return QObject::tr("Not supported by %1 (requires %2)")
        .arg(name)
        .arg(QString::fromUtf8(CapTag::name.data(),
                               static_cast<int>(CapTag::name.size())));
}

}  // namespace detail

/* ════════════════════════════════════════════════════════════════════════
 *  wire_action  —  THE template
 *
 *  @tparam CapTag    A tag type from `uft::gui::cap::*` that mirrors a
 *                    concept from `uft::hal::*`.
 *  @tparam Provider  A provider type (V2). Need NOT satisfy CapTag —
 *                    the if-constexpr decides which branch to instantiate.
 *  @tparam Factory   A *generic* lambda of shape `[]<class P>(P *p) -> Outcome`.
 *                    Generic-ness is required so the body is checked
 *                    LAZILY. See "WHY INVOKE IS A GENERIC LAMBDA" above.
 *  @tparam Handlers  Zero or more concrete-typed lambdas, each accepting
 *                    one alternative of the Outcome variant by const ref.
 *                    `std::visit` enforces that EVERY alternative has a
 *                    handler — missing one fails to compile.
 *
 *  Lifetime:
 *    - btn must outlive the connection. The connect uses `btn` as receiver,
 *      so destroying btn auto-disconnects.
 *    - provider must outlive every click-dispatched call. Lifetime
 *      management is the caller's; typical owner is HardwareManager.
 * ════════════════════════════════════════════════════════════════════════ */
template<class CapTag, class Provider, class Factory, class... Handlers>
inline void wire_action(QAbstractButton *btn,
                        Provider *provider,
                        Factory &&factory,
                        Handlers &&...handlers)
{
    static_assert(std::is_class_v<Provider>,
                  "wire_action: Provider must be a concrete provider type, "
                  "not a base-class pointer. Templated wire-up is per "
                  "concrete provider; runtime polymorphism would defeat "
                  "the type-driven design.");

    if constexpr (CapTag::template satisfied<Provider>) {
        /* ── true branch — provider satisfies the capability ─────────── */
        btn->setEnabled(true);
        btn->setToolTip(QString());

        QObject::connect(
            btn, &QAbstractButton::clicked, btn,
            [provider,
             factory  = std::forward<Factory>(factory),
             ... handlers = std::forward<Handlers>(handlers)]() mutable {
                auto outcome = factory(provider);
                std::visit(::uft::hal::overloaded{handlers...}, outcome);
            });
    } else {
        /* ── false branch — capability missing → disable + explain ──── */
        btn->setEnabled(false);
        btn->setToolTip(detail::not_supported_tooltip<CapTag>(provider));

        /* Explicitly suppress unused-warnings; we want the parameters in
         * scope so the false branch type-checks identically to the true
         * branch (same `[[nodiscard]]` propagation, same const-correctness
         * of the calling code). */
        (void)factory;
        ((void)handlers, ...);
    }
}

}  // namespace uft::gui

#endif  // UFT_GUI_WIRING_RUNTIME_H
