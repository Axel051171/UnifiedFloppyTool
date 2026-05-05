/**
 * @file greaseweazle_provider_v2.h
 * @brief GreaseweazleProviderV2 — mixin-composed V2 HAL provider (MF-154 / P1.1).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities (all backed by the existing uft_gw_* C-API):
 *   ReadsRawFlux    ✓  do_read_raw_flux()   → FluxOutcome
 *   WritesRawFlux   ✓  do_write_raw_flux()  → WriteOutcome
 *   ControlsMotor   ✓  do_set_motor()       → MotorOutcome
 *   SeeksHead       ✓  do_seek()            → SeekOutcome
 *   Recalibrates    ✓  do_recalibrate()     → SeekOutcome
 *   MeasuresRPM     ✓  do_measure_rpm()     → RpmOutcome
 *   DetectsDrive    ✓  do_detect_drive()    → DetectOutcome
 *
 * Intentionally omitted mixins:
 *   ReadsSectors    ✗  Greaseweazle reads raw flux; sector decoding is done
 *                      by the upstream analysis pipeline, not the HAL layer.
 *   WritesSectors   ✗  Same rationale — GW writes flux streams only.
 *
 * The V1 GreaseweazleHardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 *
 * Rule F-3: multi-revolution / divergent-read data is preserved verbatim
 * in FluxCaptured::transitions_ns and the per-revolution index boundaries.
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 * The ProviderError constructor throws std::logic_error on empty strings.
 *
 * Backend: the uft_gw_* C-API from include/uft/hal/uft_greaseweazle_full.h.
 * The C-API signatures are NEVER modified here — only wrapped.
 */
#ifndef GREASEWEAZLE_PROVIDER_V2_H
#define GREASEWEAZLE_PROVIDER_V2_H

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"
#include "uft/hal/uft_greaseweazle_full.h"

namespace uft::hal {

/**
 * @brief Greaseweazle V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"Greaseweazle", SpecStatus::VendorDocumented>
 *   ReadsRawFluxVia<GreaseweazleProviderV2>
 *   WritesRawFluxVia<GreaseweazleProviderV2>
 *   ControlsMotorVia<GreaseweazleProviderV2>
 *   SeeksHeadVia<GreaseweazleProviderV2>
 *   RecalibratesVia<GreaseweazleProviderV2>
 *   MeasuresRPMVia<GreaseweazleProviderV2>
 *   DetectsDriveVia<GreaseweazleProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class GreaseweazleProviderV2 final
    : public mixin::Identity<"Greaseweazle", SpecStatus::VendorDocumented>
    , public mixin::ReadsRawFluxVia<GreaseweazleProviderV2>
    , public mixin::WritesRawFluxVia<GreaseweazleProviderV2>
    , public mixin::ControlsMotorVia<GreaseweazleProviderV2>
    , public mixin::SeeksHeadVia<GreaseweazleProviderV2>
    , public mixin::RecalibratesVia<GreaseweazleProviderV2>
    , public mixin::MeasuresRPMVia<GreaseweazleProviderV2>
    , public mixin::DetectsDriveVia<GreaseweazleProviderV2>
{
public:
    /**
     * @brief Construct from an open uft_gw_device_t handle.
     *
     * The handle must remain valid for the lifetime of this provider.
     * Ownership is NOT transferred — call uft_gw_close() externally.
     * Passing nullptr is accepted for mock/test construction; every
     * do_* method returns HardwareDisconnected in that case.
     */
    explicit GreaseweazleProviderV2(uft_gw_device_t* handle) noexcept;

    /* Non-copyable, non-movable (holds a raw C handle). */
    GreaseweazleProviderV2(const GreaseweazleProviderV2&)            = delete;
    GreaseweazleProviderV2& operator=(const GreaseweazleProviderV2&) = delete;
    GreaseweazleProviderV2(GreaseweazleProviderV2&&)                 = delete;
    GreaseweazleProviderV2& operator=(GreaseweazleProviderV2&&)      = delete;

    ~GreaseweazleProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    WriteOutcome  do_write_raw_flux(const WriteFluxParams& w, const FluxStream& flux);
    MotorOutcome  do_set_motor     (bool on);
    SeekOutcome   do_seek          (int cylinder);
    SeekOutcome   do_recalibrate   ();
    RpmOutcome    do_measure_rpm   ();
    DetectOutcome do_detect_drive  ();

private:
    uft_gw_device_t* m_handle;  /**< Opaque GW device handle (not owned). */

    /** Translate a uft_gw_* error code to a ProviderError. */
    static ProviderError gw_err_to_provider_error(
        int gw_rc,
        const char* what,
        const char* why_prefix);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy WritesRawFlux");
static_assert(ControlsMotor<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy ControlsMotor");
static_assert(SeeksHead<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy SeeksHead");
static_assert(Recalibrates<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy Recalibrates");
static_assert(MeasuresRPM<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy MeasuresRPM");
static_assert(DetectsDrive<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy DetectsDrive");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsSectors<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must NOT satisfy ReadsSectors "
    "(GW reads flux; sector decode is upstream)");
static_assert(!WritesSectors<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must NOT satisfy WritesSectors "
    "(GW writes flux streams only)");

/* Composite predicates that must hold. */
static_assert(ImagesFlux<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy ImagesFlux");
static_assert(FullDriveControl<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy FullDriveControl");
static_assert(WritesAnything<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy WritesAnything");

}  // namespace uft::hal

#endif  // GREASEWEAZLE_PROVIDER_V2_H
