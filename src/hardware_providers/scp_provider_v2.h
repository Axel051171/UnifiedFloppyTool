/**
 * @file scp_provider_v2.h
 * @brief SCPProviderV2 — mixin-composed V2 HAL provider (MF-161 / P1.8).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities (backed by the existing uft_scp_direct_* C-API scaffold):
 *   ReadsRawFlux    v  do_read_raw_flux()  -> FluxOutcome
 *   WritesRawFlux   v  do_write_raw_flux() -> WriteOutcome
 *   DetectsDrive    v  do_detect_drive()   -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   ReadsSectors    x  SCP is a flux device; sector decoding happens in the
 *                      upstream analysis pipeline, not the HAL layer.
 *   WritesSectors   x  Same rationale as ReadsSectors.
 *   ControlsMotor   x  uft_scp_direct.h (M3.1 scaffold) does not expose a
 *                      standalone motor-control command. The SCP protocol's
 *                      MTRAON/MTRAOFF commands exist in the V1 QSerialPort
 *                      layer (scphardwareprovider.cpp), but the uft_scp_direct
 *                      C-API used by this V2 provider does not include them.
 *                      Add this mixin only when the C-API exposes it.
 *   SeeksHead       x  Same rationale — STEPTO exists in the V1 layer but
 *                      uft_scp_direct_seek() is the only seek primitive and
 *                      it uses a combined track_index (cylinder*2 + side),
 *                      not the concept's (cylinder) + separate head-select
 *                      signature. Wrapping this would require a shim that
 *                      contradicts the anti-pragmatism rule.
 *   Recalibrates    x  SEEK0 is available in the V1 layer but absent from
 *                      uft_scp_direct.h. Omit until the C-API surface covers it.
 *   MeasuresRPM     x  SCP computes RPM from GETFLUXINFO index timing, but
 *                      uft_scp_direct.h has no dedicated measure_rpm primitive.
 *                      The do_detect_drive() path measures RPM internally as
 *                      part of drive detection; a standalone MeasuresRPM mixin
 *                      would require exposing that sub-operation through the
 *                      C-API first.
 *
 * Backend status (forensically honest per CLAUDE.md M3.1):
 *   uft_scp_direct_open(), uft_scp_direct_read_flux(), and
 *   uft_scp_direct_write_flux() all return UFT_ERR_NOT_IMPLEMENTED until the
 *   libusb layer is wired. The do_* methods here translate that return code
 *   into a ProviderError with an M3.1 scaffold marker — the shape is V2-
 *   conformant and the runtime error is forensically truthful. The
 *   static_assert that the concepts hold proves the TYPE SHAPE is correct;
 *   the runtime ProviderError reflects the BACKEND MATURITY.
 *
 * SpecStatus: VendorDocumented — SCP protocol is publicly documented in the
 *   SuperCard Pro SDK v1.7 (cbmstuff.com, December 2015). Cross-checked
 *   against samdisk/scp.cpp and the UFT V1 scphardwareprovider.cpp.
 *
 * Rule F-3: multi-revolution / divergent-read data is preserved verbatim in
 *   FluxCaptured::transitions_ns and revolutions count. The V1 provider's
 *   retry loop (maxRetries=5, preserving separate revolution captures) is
 *   carried forward — no collapsing to a single sample.
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * The V1 SCPHardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef SCP_PROVIDER_V2_H
#define SCP_PROVIDER_V2_H

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"
#include "uft/hal/uft_scp_direct.h"

namespace uft::hal {

/**
 * @brief SuperCard Pro V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"SuperCard Pro", SpecStatus::VendorDocumented>
 *   ReadsRawFluxVia<SCPProviderV2>
 *   WritesRawFluxVia<SCPProviderV2>
 *   DetectsDriveVia<SCPProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class SCPProviderV2 final
    : public mixin::Identity<"SuperCard Pro", SpecStatus::VendorDocumented>
    , public mixin::ReadsRawFluxVia<SCPProviderV2>
    , public mixin::WritesRawFluxVia<SCPProviderV2>
    , public mixin::DetectsDriveVia<SCPProviderV2>
{
public:
    /**
     * @brief Construct from an open uft_scp_direct_ctx_t handle.
     *
     * The handle must remain valid for the lifetime of this provider.
     * Ownership is NOT transferred — call uft_scp_direct_close() externally.
     * Passing nullptr is accepted for mock/test construction; every do_*
     * method returns a ProviderError("SCP USB I/O pending (M3.1)") in
     * that case, which is also the same error returned by the scaffold
     * when the handle is non-null but USB not yet wired.
     */
    explicit SCPProviderV2(uft_scp_direct_ctx_t* handle) noexcept;

    /* Non-copyable, non-movable (holds a raw C handle). */
    SCPProviderV2(const SCPProviderV2&)            = delete;
    SCPProviderV2& operator=(const SCPProviderV2&) = delete;
    SCPProviderV2(SCPProviderV2&&)                 = delete;
    SCPProviderV2& operator=(SCPProviderV2&&)      = delete;

    ~SCPProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    WriteOutcome  do_write_raw_flux(const WriteFluxParams& w, const FluxStream& flux);
    DetectOutcome do_detect_drive  ();

private:
    uft_scp_direct_ctx_t* m_handle;  /**< Opaque SCP context handle (not owned). */

    /** Translate a uft_error_t return from scp_direct into a ProviderError. */
    static ProviderError scp_err_to_provider_error(
        uft_error_t rc,
        const char* what_msg,
        const char* why_prefix);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<SCPProviderV2>,
    "SCPProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<SCPProviderV2>,
    "SCPProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<SCPProviderV2>,
    "SCPProviderV2 must satisfy WritesRawFlux");
static_assert(DetectsDrive<SCPProviderV2>,
    "SCPProviderV2 must satisfy DetectsDrive");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsSectors<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy ReadsSectors "
    "(SCP reads flux; sector decode is upstream)");
static_assert(!WritesSectors<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy WritesSectors "
    "(SCP writes flux streams only)");
static_assert(!ControlsMotor<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy ControlsMotor "
    "(uft_scp_direct.h M3.1 does not expose motor control; "
    "add when the C-API surface covers it)");
static_assert(!SeeksHead<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy SeeksHead "
    "(uft_scp_direct_seek uses combined track_index, not (cylinder, head); "
    "a shim would violate anti-pragmatism rule)");
static_assert(!Recalibrates<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy Recalibrates "
    "(SEEK0 is absent from uft_scp_direct.h M3.1; "
    "add when the C-API exposes it)");
static_assert(!MeasuresRPM<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy MeasuresRPM "
    "(no dedicated measure_rpm primitive in uft_scp_direct.h M3.1; "
    "RPM is computed internally by do_detect_drive)");

/* Composite predicates. */
static_assert(ImagesFlux<SCPProviderV2>,
    "SCPProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and WritesRawFlux)");

}  // namespace uft::hal

#endif  // SCP_PROVIDER_V2_H
