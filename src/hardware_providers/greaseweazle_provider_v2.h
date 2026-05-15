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
 * in FluxCaptured::transitions_ns; the per-revolution index boundaries
 * are preserved (ticks→ns) in FluxCaptured::index_times_ns (MF-194).
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

#include <optional>
#include <string>

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
     * @brief Default-construct an unopened provider.
     *
     * Use `open()` to attach to a Greaseweazle device on a serial port,
     * or construct via the legacy handle-taking ctor below.
     */
    GreaseweazleProviderV2() noexcept = default;

    /**
     * @brief Construct from an already-open uft_gw_device_t handle.
     *
     * SEMANTICS CHANGED IN MF-171 (P1.18): the provider now TAKES
     * OWNERSHIP of the handle. The destructor (or close()) calls
     * uft_gw_close() — the caller MUST NOT close the handle
     * externally. Passing nullptr is accepted for mock/test
     * construction; every do_* method returns HardwareDisconnected
     * in that case.
     *
     * The conformance harness + per-provider tests pass nullptr; their
     * usage is unchanged by the new ownership rule (close() with a
     * null handle is a no-op).
     *
     * MF-199 (P1.20): the optional `drive_unit` binds the Greaseweazle
     * bus unit (0 = DS0, 1 = DS1). The provider issues
     * uft_gw_select_drive() once, lazily, before the first bus
     * operation. An out-of-range unit or a select failure surfaces as a
     * ProviderError on the first do_* call — never silently ignored.
     * The parameter is defaulted, so existing one-arg call sites (the
     * conformance harness, per-provider tests passing nullptr) are
     * unchanged.
     */
    explicit GreaseweazleProviderV2(uft_gw_device_t* handle,
                                    int drive_unit = 0) noexcept;

    /**
     * @brief Open a Greaseweazle device on the given serial port.
     *
     * On success the device is owned by this provider; close() or the
     * destructor releases it. Sets `firmware_version()` and
     * `hardware_model()` from `uft_gw_get_info()` upon successful open.
     *
     * @param port_path  serial-port path (Linux: /dev/ttyACMx,
     *                   Windows: COMx, macOS: /dev/tty.usbmodemXXX).
     * @param err_out    optional out-param populated with a
     *                   human-readable error string on failure.
     * @return true if device opened, false otherwise. On false, the
     *         provider remains in the unopened state — `is_open()`
     *         returns false and every do_* method returns
     *         HardwareDisconnected.
     */
    bool open(const char *port_path, std::string *err_out = nullptr);

    /**
     * @brief open() overload that also binds the bus drive unit (0 or 1).
     *
     * Equivalent to open(port_path, err_out) followed by
     * set_drive_unit(drive_unit). MF-199 (P1.20).
     */
    bool open(const char *port_path, int drive_unit,
              std::string *err_out = nullptr);

    /**
     * @brief Set the Greaseweazle bus unit (0 = DS0, 1 = DS1) for
     *        subsequent operations.
     *
     * Records the unit and clears the lazy-select latch so the next bus
     * operation re-asserts it via uft_gw_select_drive(). The range is
     * validated lazily on the next do_* call (a typed ProviderError),
     * never silently clamped. MF-199 (P1.20).
     */
    void set_drive_unit(int unit) noexcept;

    /** Currently-configured bus unit (0 or 1). Default 0. */
    int drive_unit() const noexcept { return m_drive_unit; }

    /** Close the device if open. No-op if not. Idempotent. */
    void close() noexcept;

    /** True iff `open()` succeeded and `close()` has not been called. */
    bool is_open() const noexcept { return m_handle != nullptr; }

    /* MF-202 (P1.22): `raw_handle()` — the V1-shape C-API escape hatch —
     * is removed. Its only consumers, FluxCaptureJob and FluxWriteJob,
     * were migrated to the V2 outcome surface in P1.20 / P1.21; every
     * uft_gw_* call now lives behind the do_* methods of this class. */

    /** Firmware version string, e.g. "v1.4". Empty before open(). */
    const std::string &firmware_version() const noexcept {
        return m_firmware_version;
    }

    /** Hardware model byte (F1=1, F7=7, V4=4, …). Zero before open(). */
    int hardware_model() const noexcept { return m_hw_model; }

    /* Non-copyable, non-movable (holds a raw C handle). */
    GreaseweazleProviderV2(const GreaseweazleProviderV2&)            = delete;
    GreaseweazleProviderV2& operator=(const GreaseweazleProviderV2&) = delete;
    GreaseweazleProviderV2(GreaseweazleProviderV2&&)                 = delete;
    GreaseweazleProviderV2& operator=(GreaseweazleProviderV2&&)      = delete;

    /** Closes the handle if still open. */
    ~GreaseweazleProviderV2();

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    WriteOutcome  do_write_raw_flux(const WriteFluxParams& w, const FluxStream& flux);
    MotorOutcome  do_set_motor     (bool on);
    SeekOutcome   do_seek          (int cylinder);
    SeekOutcome   do_recalibrate   ();
    RpmOutcome    do_measure_rpm   ();
    DetectOutcome do_detect_drive  ();

private:
    uft_gw_device_t *m_handle = nullptr;     /**< Opaque GW device handle, OWNED. */
    std::string      m_firmware_version;     /**< Populated by open(). */
    int              m_hw_model = 0;         /**< Populated by open(). */
    int              m_drive_unit = 0;       /**< Bus unit 0/1 (MF-199), ctor/setter. */
    bool             m_drive_selected = false; /**< Lazy uft_gw_select_drive latch. */

    /** Translate a uft_gw_* error code to a ProviderError. */
    static ProviderError gw_err_to_provider_error(
        int gw_rc,
        const char* what,
        const char* why_prefix);

    /**
     * @brief Ensure uft_gw_select_drive() has run for m_drive_unit.
     *
     * Returns std::nullopt on success (or once already latched), or a
     * populated ProviderError (out-of-range unit, or a select-drive
     * C-API failure) that the caller converts into the right Outcome
     * variant. Idempotent once latched. MF-199 (P1.20).
     */
    std::optional<ProviderError> ensure_drive_selected();
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
