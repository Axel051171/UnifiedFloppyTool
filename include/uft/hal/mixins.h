/**
 * @file mixins.h
 * @brief Capability mixin templates — Type-Driven HAL foundation (refactor/type-driven-hal).
 *
 * A provider in the new architecture is the COMPOSITION of capability
 * mixins, not a subclass of one big base class. Each mixin contributes
 * exactly one capability method. The provider inherits ONLY the mixins
 * for capabilities the hardware really has — anything missing is
 * absent at the type level, not "returns false at runtime".
 *
 *   class GreaseweazleProvider final
 *     : public mixin::Identity<"Greaseweazle", SpecStatus::VendorDocumented>
 *     , public mixin::ReadsRawFluxVia<gw_backend_t>
 *     , public mixin::WritesRawFluxVia<gw_backend_t>
 *     , public mixin::ControlsMotorVia<gw_backend_t>
 *     , public mixin::SeeksHeadVia<gw_backend_t>
 *     , public mixin::DetectsDriveVia<gw_backend_t> { ... };
 *
 *   class FC5025Provider final
 *     : public mixin::Identity<"FC5025", SpecStatus::VendorDocumented>
 *     , public mixin::ReadsSectorsVia<fc5025_backend_t>
 *     , public mixin::DetectsDriveVia<fc5025_backend_t> { ... read-only };
 *
 * Each `Vias<Backend>` mixin is a CRTP-style template that calls a
 * static method on Backend (e.g. `Backend::do_read_raw_flux(handle, params)`),
 * which is where the backend's actual C-API call lives. The mixin's job
 * is to (a) provide the right method name and signature, (b) satisfy
 * the matching concept, (c) not leak backend types into the provider
 * surface. Every public method returns the canonical Outcome variant.
 *
 * Pure header — no impact on V1 code. Concrete `Backend` types live
 * with their providers and supply the C-API bindings.
 *
 * Why CRTP-with-Backend rather than function-pointer-template:
 *   - Different backends have different opaque-handle types
 *     (uft_gw_device_t*, uft_scp_direct_ctx_t*, libusb_device_handle*,
 *     QSerialPort*, QProcess*). A single function-pointer template
 *     parameter cannot express that polymorphism cleanly.
 *   - The Backend trait class can carry handle type, lifecycle helpers
 *     (open/close), and the per-capability `do_*` methods together.
 *     Adding a capability is "add one static method on Backend".
 */
#ifndef UFT_HAL_MIXINS_H
#define UFT_HAL_MIXINS_H

#include <algorithm>
#include <array>
#include <string_view>

#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

namespace uft::hal::mixin {

/* ───────────────────────────────────────────────────────────────────────
 *  fixed_string — non-type template parameter for compile-time names
 *
 *  C++20 allows class types as NTTPs. We need a wrapper around a const
 *  char[] so providers can write `Identity<"Greaseweazle">` directly.
 * ─────────────────────────────────────────────────────────────────────── */

template<std::size_t N>
struct fixed_string {
    char value[N]{};
    constexpr fixed_string(const char (&s)[N]) {
        std::copy_n(s, N, value);
    }
    constexpr std::string_view view() const noexcept {
        /* N includes the trailing NUL — substring it off. */
        return std::string_view{value, N - 1};
    }
    static constexpr std::size_t length = N - 1;
};

/* ───────────────────────────────────────────────────────────────────────
 *  Identity mixin — every provider must have one
 *
 *  Provides display_name() + spec_status(). These are constexpr — no
 *  runtime cost, no virtual dispatch. The provider's identity is part
 *  of its type.
 * ─────────────────────────────────────────────────────────────────────── */

template<fixed_string Name, SpecStatus Status = SpecStatus::CommunityConsensus>
struct Identity {
    constexpr std::string_view display_name() const noexcept { return Name.view(); }
    constexpr SpecStatus       spec_status()  const noexcept { return Status;     }
};

/* ───────────────────────────────────────────────────────────────────────
 *  Backend-handle access (CRTP helper)
 *
 *  Every mixin needs to reach into the derived provider for the actual
 *  backend handle. Convention: provider exposes `Backend& backend()` /
 *  `const Backend& backend() const`. Mixins use the helper below to do
 *  CRTP-style downcasting safely.
 *
 *  Provider-side example:
 *      Backend  m_backend;
 *      Backend&       backend()       { return m_backend; }
 *      const Backend& backend() const { return m_backend; }
 *
 *  Mixin-side: `cap::self(*this).backend().do_read_raw_flux(...)`.
 * ─────────────────────────────────────────────────────────────────────── */

namespace detail {
/** CRTP-downcast helper: reinterprets `*this` as the derived provider. */
template<class Mixin, class Self>
constexpr Self& as_self(Mixin& m) noexcept {
    return static_cast<Self&>(m);
}
template<class Mixin, class Self>
constexpr const Self& as_self(const Mixin& m) noexcept {
    return static_cast<const Self&>(m);
}
}  // namespace detail

/* ───────────────────────────────────────────────────────────────────────
 *  Capability mixins — one per concept
 *
 *  Each takes a Backend trait class. Backend must provide static
 *  do_<capability>(handle, params) returning the matching Outcome.
 *
 *  The mixins below use a simpler pattern: each mixin holds NO state
 *  itself; instead it expects the derived provider to supply
 *  `Backend& backend()`. CRTP via void-return-type-trick is avoided —
 *  the derived provider must inherit BOTH the mixin and provide the
 *  backend() accessor.
 *
 *  This means: mixins.h is a CONTRACT. Providers implement it.
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * Helper macro — defines a mixin Mxxx<Backend> with method `name`
 * returning OutcomeType, calling Backend::do_name(...).
 *
 * The macro is a deliberate choice: keeps the file dense and prevents
 * subtle copy-paste drift between similar mixins. Each mixin is six
 * lines plus a comment; a macro avoids those six lines being subtly
 * different across 9 capabilities.
 */
#define UFT_HAL_DEFINE_MIXIN_1ARG(MixinName, MethodName, OutcomeType, ParamType) \
    template<class Backend>                                                       \
    class MixinName {                                                             \
    public:                                                                       \
        OutcomeType MethodName(const ParamType& p) {                              \
            return static_cast<Backend*>(this)->do_##MethodName(p);               \
        }                                                                         \
        OutcomeType MethodName(const ParamType& p) const {                        \
            return static_cast<const Backend*>(this)->do_##MethodName(p);         \
        }                                                                         \
    }

#define UFT_HAL_DEFINE_MIXIN_0ARG(MixinName, MethodName, OutcomeType)             \
    template<class Backend>                                                       \
    class MixinName {                                                             \
    public:                                                                       \
        OutcomeType MethodName() {                                                \
            return static_cast<Backend*>(this)->do_##MethodName();                \
        }                                                                         \
        OutcomeType MethodName() const {                                          \
            return static_cast<const Backend*>(this)->do_##MethodName();          \
        }                                                                         \
    }

/* ── Read mixins ──────────────────────────────────────────────────── */
UFT_HAL_DEFINE_MIXIN_1ARG(ReadsSectorsVia,  read_sector,   SectorOutcome, ReadSectorParams);
UFT_HAL_DEFINE_MIXIN_1ARG(ReadsRawFluxVia,  read_raw_flux, FluxOutcome,   ReadFluxParams);

/* ── Write mixins ────────────────────────────────────────────────── */
template<class Backend>
class WritesSectorsVia {
public:
    WriteOutcome write_sector(const WriteSectorParams& w, const SectorPayload& payload) {
        return static_cast<Backend*>(this)->do_write_sector(w, payload);
    }
};

template<class Backend>
class WritesRawFluxVia {
public:
    WriteOutcome write_raw_flux(const WriteFluxParams& w, const FluxStream& flux) {
        return static_cast<Backend*>(this)->do_write_raw_flux(w, flux);
    }
};

/* ── Drive control mixins ───────────────────────────────────────── */
template<class Backend>
class ControlsMotorVia {
public:
    MotorOutcome set_motor(bool on) {
        return static_cast<Backend*>(this)->do_set_motor(on);
    }
};

template<class Backend>
class SeeksHeadVia {
public:
    SeekOutcome seek(int cylinder) {
        return static_cast<Backend*>(this)->do_seek(cylinder);
    }
};

UFT_HAL_DEFINE_MIXIN_0ARG(RecalibratesVia, recalibrate, SeekOutcome);

/* ── Diagnostic mixins ───────────────────────────────────────────── */
UFT_HAL_DEFINE_MIXIN_0ARG(MeasuresRPMVia,  measure_rpm,  RpmOutcome);
UFT_HAL_DEFINE_MIXIN_0ARG(DetectsDriveVia, detect_drive, DetectOutcome);

#undef UFT_HAL_DEFINE_MIXIN_1ARG
#undef UFT_HAL_DEFINE_MIXIN_0ARG

/* ───────────────────────────────────────────────────────────────────────
 *  Provider composition contract
 *
 *  A Provider class must:
 *    1. Inherit `Identity<...>` for name + spec_status.
 *    2. Inherit one `*Vias<Self>` for each capability it has, where
 *       Self is the provider class itself (CRTP).
 *    3. Implement a `do_<method>(...)` private method per inherited
 *       capability mixin, returning the matching Outcome.
 *    4. Optionally hold a backend handle (libusb_device_handle*,
 *       uft_gw_device_t*, QSerialPort, QProcess, ...). The do_* methods
 *       call into it.
 *
 *  Example (full):
 *
 *    class GreaseweazleProvider final
 *      : public mixin::Identity<"Greaseweazle", SpecStatus::VendorDocumented>
 *      , public mixin::ReadsRawFluxVia<GreaseweazleProvider>
 *      , public mixin::WritesRawFluxVia<GreaseweazleProvider>
 *      , public mixin::ControlsMotorVia<GreaseweazleProvider>
 *      , public mixin::SeeksHeadVia<GreaseweazleProvider>
 *      , public mixin::RecalibratesVia<GreaseweazleProvider>
 *      , public mixin::MeasuresRPMVia<GreaseweazleProvider>
 *      , public mixin::DetectsDriveVia<GreaseweazleProvider>
 *    {
 *    public:
 *      explicit GreaseweazleProvider(uft_gw_device_t* h) : m_handle(h) {}
 *
 *      // Backend bindings — called by the mixins via CRTP downcast.
 *      FluxOutcome   do_read_raw_flux(const ReadFluxParams& p);
 *      WriteOutcome  do_write_raw_flux(const WriteFluxParams& w, const FluxStream& f);
 *      MotorOutcome  do_set_motor(bool on);
 *      SeekOutcome   do_seek(int cyl);
 *      SeekOutcome   do_recalibrate();
 *      RpmOutcome    do_measure_rpm();
 *      DetectOutcome do_detect_drive();
 *
 *    private:
 *      uft_gw_device_t* m_handle;
 *    };
 *
 *  Note: FC5025Provider would simply OMIT the WritesXxx/ControlsMotor/
 *  SeeksHead/Recalibrates/MeasuresRPM mixins. Calling write_raw_flux on
 *  it is then a compile error, not a runtime no-op. That is the H-2
 *  rule structurally enforced.
 * ─────────────────────────────────────────────────────────────────────── */

}  // namespace uft::hal::mixin

#endif  // UFT_HAL_MIXINS_H
