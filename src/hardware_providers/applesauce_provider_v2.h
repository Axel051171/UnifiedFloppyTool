/**
 * @file applesauce_provider_v2.h
 * @brief ApplesauceProviderV2 — mixin-composed V2 HAL provider (MF-166 / P1.13).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * V1 audit summary (applesaucehardwareprovider.cpp, 1330 LOC):
 *
 *   The Applesauce text protocol uses USB-CDC serial at 115200 8N1.
 *   Commands are ASCII strings terminated by '\n'; responses are a
 *   single status character ('.' = OK, '!' = error, '?' = unknown,
 *   '+' = on, '-' = off, 'v' = no power) followed by optional text.
 *   Large data transfers (flux capture, flux upload) use a binary-bulk
 *   mode: after the command handshake the device either streams N bytes
 *   or accepts N bytes without further framing.
 *
 *   REAL implementations found in V1 (all guarded by AS_SERIAL_AVAILABLE):
 *     - setMotor(bool)     → "motor:on" / "motor:off", reads '.' response
 *                            Spins up delay (500 ms). REAL.
 *     - seekCylinder(int)  → "head:track N", reads '.' response.
 *                            Head settle delay (15 ms). REAL.
 *     - selectHead(int)    → "head:side N", reads '.' response. REAL.
 *     - readRawFlux()      → sync:on → disk:read / disk:readx → data:?size
 *                            → data:<N (binary download). Retry loop (5×).
 *                            Multi-revolution via "disk:readx". REAL.
 *     - writeTrack()       → disk:?write (write-protect check)
 *                            → data:clear → data:>N (binary upload)
 *                            → disk:write. Retry loop. REAL.
 *     - writeRawFlux()     → delegates to writeTrack(). REAL.
 *     - recalibrate()      → "head:zero", reads '.' response.
 *                            Moves to track 0. REAL.
 *     - measureRPM()       → "sync:?speed", parses numeric response.
 *                            Requires motor on. REAL.
 *     - detectDrive()      → "?kind" + "data:?max" + "psu:?5v" + "psu:?12v"
 *                            + calls measureRPM(). REAL.
 *
 *   ALL V1 methods are REAL serial dialogs — no silent stubs. This is
 *   the only V2 provider so far with a 100% capability hit rate.
 *
 * Capabilities claimed:
 *   Identity<"Applesauce", VendorDocumented>
 *   ReadsRawFlux      v  do_read_raw_flux()   -> FluxOutcome
 *   WritesRawFlux     v  do_write_raw_flux()  -> WriteOutcome
 *   ControlsMotor     v  do_set_motor()       -> MotorOutcome
 *   SeeksHead         v  do_seek()            -> SeekOutcome
 *   Recalibrates      v  do_recalibrate()     -> SeekOutcome
 *   MeasuresRPM       v  do_measure_rpm()     -> RpmOutcome
 *   DetectsDrive      v  do_detect_drive()    -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   ReadsSectors    x  Applesauce is a flux device; sector decode happens
 *                      in the upstream analysis pipeline, not the HAL layer.
 *                      V1 has no readSector() — it exposes readRawFlux()
 *                      and readTrack() (which itself calls readRawFlux()).
 *   WritesSectors   x  Same rationale as ReadsSectors. V1 writes at the
 *                      raw-flux level only.
 *
 * SpecStatus: VendorDocumented — Applesauce protocol documented at
 *   wiki.applesaucefdc.com by Evolution Interactive / John Googin.
 *   Cross-checked against V1 applesaucehardwareprovider.cpp which was
 *   written against that spec.
 *
 * Transport abstraction:
 *   The V2 uses an IApplesauceTransport raw-write/read interface rather
 *   than a single "dialog" function. This is the right fit because the
 *   Applesauce protocol has two fundamentally different modes:
 *     (a) Line mode: send ASCII command + '\n', read back one ASCII
 *         response line (terminated by '\n').
 *     (b) Binary bulk mode: after a command exchange the device either
 *         streams N bytes of raw flux data, or accepts N bytes of raw
 *         flux data without newline framing.
 *   A single "dialog" abstraction cannot express mode (b) cleanly
 *   because the bulk transfer is not line-terminated.
 *
 *   IApplesauceTransport provides:
 *     write_line(cmd)         — send ASCII command + '\n'
 *     read_line(timeout_ms)   — read until '\n', return stripped line
 *     write_binary(data)      — write raw bytes (for flux upload)
 *     read_binary(n_bytes)    — read exactly n raw bytes (for flux download)
 *
 *   In production: wrap a QSerialPort (connected Applesauce device).
 *   In tests: a tiny adapter wraps SerialMock.
 *
 * Backend honesty (M3.3 scaffold):
 *   The Applesauce C-HAL (uft_applesauce.h / uft_applesauce.c) is an
 *   M3.3 partial scaffold — 7 functions return UFT_ERR_NOT_IMPLEMENTED
 *   (serial I/O pending). The V2 provider does NOT use the C-HAL for
 *   I/O — it uses the injected IApplesauceTransport, which is the REAL
 *   path in production (QSerialPort) and the scripted path in tests
 *   (SerialMock). When the transport is null or the device returns '!'
 *   on any command, do_*() returns a ProviderError with the M3.3 marker
 *   explaining the state.
 *
 * Rule F-3 (multi-revolution / divergent-read preservation):
 *   The V1 readRawFlux() preserves multi-revolution flux data verbatim:
 *     - "disk:read"  → single revolution
 *     - "disk:readx" → extended (multi-revolution, index-synced)
 *   The V2 carries this forward: when revolutions > 1, the transport
 *   sends "disk:readx" and the full captured flux buffer (which may
 *   contain multiple revolutions as measured by the 8 MHz clock) is
 *   stored verbatim in FluxCaptured::transitions_ns. The V1 retry loop
 *   (maxRetries=5) is also carried forward — the runner retries the
 *   entire read on error. No single-sample collapsing.
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * The V1 ApplesauceHardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef APPLESAUCE_PROVIDER_V2_H
#define APPLESAUCE_PROVIDER_V2_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/* ─────────────────────────────────────────────────────────────────────────
 *  IApplesauceTransport — the raw serial I/O interface
 *
 *  Separating the protocol logic from the transport allows:
 *    - Production: QSerialPort-backed transport.
 *    - Tests:      SerialMock-backed transport adapter.
 *
 *  Both line-mode (ASCII commands) and binary-bulk mode (flux bytes)
 *  are represented. The timeout argument is advisory — implementations
 *  should honor it but may return earlier on error.
 * ─────────────────────────────────────────────────────────────────────── */

struct IApplesauceTransport {
    virtual ~IApplesauceTransport() = default;

    /**
     * @brief Send an ASCII command line (without the trailing '\n').
     *
     * The implementation appends '\n' before sending.
     * @return true on success, false on write error.
     */
    virtual bool write_line(const std::string& command) = 0;

    /**
     * @brief Read one response line (up to '\n').
     *
     * Returns the line content WITHOUT the trailing '\n'. Returns an
     * empty string on timeout or transport error. The caller checks the
     * first character to determine the status code.
     * @param timeout_ms Maximum wait time in milliseconds.
     */
    virtual std::string read_line(int timeout_ms = 3000) = 0;

    /**
     * @brief Write raw binary bytes to the device (for flux upload).
     *
     * Used by the "data:>N" command sequence to stream flux bytes.
     * @return Number of bytes actually written, or -1 on error.
     */
    virtual int write_binary(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Read exactly n_bytes of raw binary data (for flux download).
     *
     * Used after the "data:<N" command sequence to download flux bytes.
     * Returns a vector that may be shorter than n_bytes on timeout.
     * @param n_bytes  Exact byte count to receive.
     * @param timeout_ms Maximum wait time in milliseconds.
     */
    virtual std::vector<uint8_t> read_binary(uint32_t n_bytes,
                                              int timeout_ms = 30000) = 0;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceReadResult — result of a flux read operation
 *
 *  Carries everything the runner captured. Analogous to Xum1541ReadResult
 *  but for flux-level data.
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an Applesauce flux read operation.
 *
 * The V1 readRawFlux() path:
 *   1. sync:on
 *   2. disk:read or disk:readx (revolutions > 1)
 *   3. data:?size
 *   4. data:<N (binary download)
 *   5. sync:off
 *
 * flux_bytes contains the raw binary data downloaded from the device
 * buffer. Each 32-bit little-endian value is one flux transition timing
 * in 8 MHz ticks (125 ns per tick). The V2 converts to nanoseconds
 * in do_read_raw_flux() — this struct carries the raw bytes to allow
 * the test layer to inject arbitrary flux patterns.
 *
 * Rule F-3: when retries > 0, the last successful capture is the one
 * stored here. The number of revolutions captured by "disk:readx" may
 * vary; the entire buffer is stored verbatim.
 */
struct ApplesauceReadResult {
    /** Raw flux bytes downloaded from the device buffer.
     *  Each 4 bytes (little-endian) = one flux transition timing in
     *  8 MHz ticks. Empty when the read failed. */
    std::vector<uint8_t> flux_bytes;

    /** Number of 8 MHz ticks in one complete revolution (index-to-index).
     *  0 when the drive has no index hole (e.g. some Apple 5.25" drives
     *  that don't have an index pulse). */
    uint32_t revolution_ticks = 0;

    /** Number of revolutions captured (from the "disk:readx" command).
     *  1 for single-revolution "disk:read", >= 1 for "disk:readx". */
    int revolutions = 1;

    /** Number of retries consumed (0 = no retry needed). */
    int retries_used = 0;

    /** Human-readable error message when flux_bytes is empty. */
    std::string error_message;

    /** True if the transport was not available (M3.3 scaffold state). */
    bool transport_unavailable = false;

    /** True if the device returned '!' (error) on the read command. */
    bool device_error = false;

    /** True if the flux buffer was larger than the device's max buffer. */
    bool buffer_overflow = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceWriteResult — result of a flux write operation
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an Applesauce flux write operation.
 *
 * The V1 writeTrack() path:
 *   1. disk:?write (write-protect check: '+' = protected)
 *   2. data:clear
 *   3. data:>N (binary upload of flux bytes)
 *   4. disk:write
 *
 * Rule F-3: the V1 had a retry loop (maxRetries). retries_used carries
 * that forward for the audit trail.
 */
struct ApplesauceWriteResult {
    /** Number of bytes written (= flux_bytes.size() on success). */
    uint32_t bytes_written = 0;

    /** Number of retries consumed. */
    int retries_used = 0;

    /** Human-readable error message when bytes_written == 0. */
    std::string error_message;

    /** True if the disk is write-protected ("disk:?write" returned '+'). */
    bool write_protected = false;

    /** True if the transport was not available. */
    bool transport_unavailable = false;

    /** True if the device returned '!' on any write command. */
    bool device_error = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceDetectResult — result of a drive detect operation
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an Applesauce drive detect operation.
 *
 * The V1 detectDrive() path:
 *   - "?kind"     → "5.25" / "3.5" / "PC" / "NONE"
 *   - "data:?max" → buffer size (163840 standard, 430080 AS+)
 *   - "psu:?5v"   → voltage reading (for HardwareInfo notes)
 *   - "psu:?12v"  → voltage reading
 *
 * drive_kind contains the raw "?kind" response. tracks and heads are
 * derived from it (5.25 → 35 tracks, 1 head; 3.5/PC → 80 tracks, 2 heads).
 */
struct ApplesauceDetectResult {
    /** True if a drive was detected ("?kind" returned anything != "NONE"). */
    bool found = false;

    /** Raw drive kind string: "5.25", "3.5", "PC", or "NONE". */
    std::string drive_kind;

    /** Buffer size in bytes: 163840 (standard) or 430080 (AS+). */
    uint32_t buffer_size = 163840;

    /** Firmware version string (from "?vers"). */
    std::string firmware;

    /** PCB revision string (from "?pcb"). */
    std::string pcb_revision;

    /** Number of tracks for the detected drive type. */
    int tracks = 0;

    /** Number of heads for the detected drive type. */
    int heads = 0;

    /** Measured RPM (from "sync:?speed"). 0.0 if measurement failed. */
    double rpm = 0.0;

    /** Human-readable error message when found == false. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceMotorResult — result of a motor control operation
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an Applesauce motor on/off operation.
 *
 * The V1 setMotor() path:
 *   (if on) ensurePowerOn() → psu:? / psu:on
 *   motor:on or motor:off → expects '.' response
 *   (if on) QThread::msleep(500) for spinup
 */
struct ApplesauceMotorResult {
    /** True if the command succeeded ('.' response). */
    bool success = false;

    /** True if the PSU was turned on as part of the motor-on sequence. */
    bool psu_was_enabled = false;

    /** Human-readable error message when success == false. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceSeekResult — result of a seek or recalibrate operation
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an Applesauce head seek or recalibrate operation.
 *
 * Seek:       "head:track N" → expects '.'
 * Recalibrate: "head:zero"   → expects '.'
 */
struct ApplesauceSeekResult {
    /** True if the command succeeded ('.' response). */
    bool success = false;

    /** The cylinder/track number reached (for seek: the requested one;
     *  for recalibrate: 0). */
    int cylinder_reached = 0;

    /** Human-readable error message when success == false. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceRpmResult — result of an RPM measurement
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an Applesauce RPM measurement.
 *
 * The V1 measureRPM() path:
 *   ensurePowerOn() → (motor on if needed) → "sync:?speed" → parse double
 *   (motor off if it was turned on for the measurement)
 */
struct ApplesauceRpmResult {
    /** Measured RPM. 0.0 if measurement failed or device has no index pulse. */
    double rpm = 0.0;

    /** Human-readable error message when rpm == 0.0 due to error. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;

    /** True if "sync:?speed" returned a non-numeric string. */
    bool non_numeric_response = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ApplesauceProviderV2 — the V2 mixin-composed provider
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Applesauce V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"Applesauce", SpecStatus::VendorDocumented>
 *   ReadsRawFluxVia<ApplesauceProviderV2>
 *   WritesRawFluxVia<ApplesauceProviderV2>
 *   ControlsMotorVia<ApplesauceProviderV2>
 *   SeeksHeadVia<ApplesauceProviderV2>
 *   RecalibratesVia<ApplesauceProviderV2>
 *   MeasuresRPMVia<ApplesauceProviderV2>
 *   DetectsDriveVia<ApplesauceProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class ApplesauceProviderV2 final
    : public mixin::Identity<"Applesauce", SpecStatus::VendorDocumented>
    , public mixin::ReadsRawFluxVia<ApplesauceProviderV2>
    , public mixin::WritesRawFluxVia<ApplesauceProviderV2>
    , public mixin::ControlsMotorVia<ApplesauceProviderV2>
    , public mixin::SeeksHeadVia<ApplesauceProviderV2>
    , public mixin::RecalibratesVia<ApplesauceProviderV2>
    , public mixin::MeasuresRPMVia<ApplesauceProviderV2>
    , public mixin::DetectsDriveVia<ApplesauceProviderV2>
{
public:
    /**
     * @brief Applesauce flux read runner function type.
     *
     * Accepts cylinder, head, revolutions. Returns an ApplesauceReadResult.
     *
     * The runner encapsulates the full Applesauce read protocol:
     *   sync:on → disk:read or disk:readx → data:?size → data:<N → sync:off
     * including the retry loop. In production, wrap the IApplesauceTransport.
     * In tests, inject a scripted lambda that returns pre-built flux bytes.
     */
    struct ReadRequest {
        int cylinder     = 0;
        int head         = 0;
        int revolutions  = 2;
        int max_retries  = 5;
    };
    using ApplesauceReadRunner = std::function<ApplesauceReadResult(const ReadRequest&)>;

    /**
     * @brief Applesauce flux write runner function type.
     *
     * Accepts cylinder, head, flux bytes, retries. Returns ApplesauceWriteResult.
     *
     * The runner encapsulates the full Applesauce write protocol:
     *   disk:?write → data:clear → data:>N → disk:write
     * including the retry loop.
     */
    struct WriteRequest {
        int cylinder    = 0;
        int head        = 0;
        int max_retries = 3;
        std::vector<uint8_t> flux_bytes;
    };
    using ApplesauceWriteRunner = std::function<ApplesauceWriteResult(const WriteRequest&)>;

    /**
     * @brief Applesauce motor control runner function type.
     *
     * Accepts a bool (true = on, false = off). Returns ApplesauceMotorResult.
     *
     * The runner encapsulates: ensurePowerOn() → motor:on / motor:off.
     */
    using ApplesauceMotorRunner = std::function<ApplesauceMotorResult(bool on)>;

    /**
     * @brief Applesauce seek runner function type.
     *
     * Accepts a cylinder number. Returns ApplesauceSeekResult.
     * Sends "head:track N" (and "head:side X" if needed).
     */
    struct SeekRequest {
        int cylinder = 0;
        int head     = 0;
    };
    using ApplesauceSeekRunner = std::function<ApplesauceSeekResult(const SeekRequest&)>;

    /**
     * @brief Applesauce recalibrate runner function type.
     *
     * Sends "head:zero". Returns ApplesauceSeekResult with cylinder_reached=0.
     */
    using ApplesauceRecalRunner = std::function<ApplesauceSeekResult()>;

    /**
     * @brief Applesauce RPM measurement runner function type.
     *
     * Sends "sync:?speed". Returns ApplesauceRpmResult.
     */
    using ApplesauceRpmRunner = std::function<ApplesauceRpmResult()>;

    /**
     * @brief Applesauce drive detect runner function type.
     *
     * Sends "?kind", "data:?max", "psu:?5v", "psu:?12v".
     * Returns ApplesauceDetectResult.
     */
    using ApplesauceDetectRunner = std::function<ApplesauceDetectResult()>;

    /**
     * @brief Construct from injectable runners.
     *
     * @param read_runner     Callable for flux reads. If null, every
     *                        do_read_raw_flux() returns a ProviderError.
     * @param write_runner    Callable for flux writes. If null, every
     *                        do_write_raw_flux() returns a ProviderError.
     * @param motor_runner    Callable for motor control. If null, every
     *                        do_set_motor() returns a ProviderError.
     * @param seek_runner     Callable for head seeks. If null, every
     *                        do_seek() returns a ProviderError.
     * @param recal_runner    Callable for recalibrate. If null, every
     *                        do_recalibrate() returns a ProviderError.
     * @param rpm_runner      Callable for RPM measurement. If null, every
     *                        do_measure_rpm() returns a ProviderError.
     * @param detect_runner   Callable for drive detection. If null, every
     *                        do_detect_drive() returns a ProviderError.
     * @param max_cylinder    Maximum cylinder index (inclusive). Default 83
     *                        (Applesauce V1 allowed up to 83 quarter-tracks).
     * @param sample_clock_hz Applesauce sample clock in Hz. Default 8 000 000
     *                        (8 MHz, 125 ns per tick).
     */
    explicit ApplesauceProviderV2(ApplesauceReadRunner    read_runner,
                                   ApplesauceWriteRunner   write_runner,
                                   ApplesauceMotorRunner   motor_runner,
                                   ApplesauceSeekRunner    seek_runner,
                                   ApplesauceRecalRunner   recal_runner,
                                   ApplesauceRpmRunner     rpm_runner,
                                   ApplesauceDetectRunner  detect_runner,
                                   int max_cylinder       = 83,
                                   double sample_clock_hz = 8000000.0);

    /* Non-copyable (holds std::function + state). */
    ApplesauceProviderV2(const ApplesauceProviderV2&)            = delete;
    ApplesauceProviderV2& operator=(const ApplesauceProviderV2&) = delete;

    /* Movable. */
    ApplesauceProviderV2(ApplesauceProviderV2&&)            = default;
    ApplesauceProviderV2& operator=(ApplesauceProviderV2&&) = default;

    ~ApplesauceProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    WriteOutcome  do_write_raw_flux(const WriteFluxParams& w, const FluxStream& flux);
    MotorOutcome  do_set_motor     (bool on);
    SeekOutcome   do_seek          (int cylinder);
    SeekOutcome   do_recalibrate   ();
    RpmOutcome    do_measure_rpm   ();
    DetectOutcome do_detect_drive  ();

private:
    ApplesauceReadRunner    m_read_runner;
    ApplesauceWriteRunner   m_write_runner;
    ApplesauceMotorRunner   m_motor_runner;
    ApplesauceSeekRunner    m_seek_runner;
    ApplesauceRecalRunner   m_recal_runner;
    ApplesauceRpmRunner     m_rpm_runner;
    ApplesauceDetectRunner  m_detect_runner;
    int                     m_max_cylinder;
    double                  m_sample_clock_hz;

    /** sample_ns = 1e9 / sample_clock_hz (e.g. 125.0 for 8 MHz). */
    double                  m_sample_ns;

    /** Return a ProviderError for a null-runner condition. */
    static ProviderError null_runner_error(const char* operation);

    /** Return a ProviderError for a geometry range violation. */
    ProviderError range_error(int cylinder, int head) const;

    /**
     * @brief Convert raw flux bytes (8 MHz ticks LE32) to transition_ns vector.
     *
     * Each 4-byte little-endian value is one flux transition timing in
     * 8 MHz ticks. Converts to nanoseconds by multiplying by sample_ns.
     * Rule F-3: the entire captured buffer is preserved verbatim.
     */
    std::vector<uint32_t> bytes_to_transitions_ns(
        const std::vector<uint8_t>& flux_bytes) const;

    /**
     * @brief Convert FluxStream transitions_ns back to 8 MHz tick LE32 bytes.
     *
     * Used by do_write_raw_flux() to convert the caller's ns-based stream
     * to the device's native tick representation.
     */
    std::vector<uint8_t> transitions_ns_to_bytes(
        const std::vector<uint32_t>& transitions_ns) const;

    /** Translate a failed ApplesauceReadResult to a FluxOutcome. */
    static FluxOutcome translate_read_failure(const ApplesauceReadResult& r,
                                               int cylinder, int head);

    /** Translate a successful ApplesauceWriteResult to a WriteOutcome. */
    static WriteOutcome translate_write_result(const ApplesauceWriteResult& r,
                                                int cylinder, int head,
                                                std::size_t flux_bytes_size);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy ReadsRawFlux "
    "(Applesauce reads raw flux via the disk:read/disk:readx command)");
static_assert(WritesRawFlux<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy WritesRawFlux "
    "(Applesauce writes raw flux via the data:>N + disk:write command sequence)");
static_assert(ControlsMotor<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy ControlsMotor "
    "(Applesauce sends motor:on / motor:off commands via the serial protocol)");
static_assert(SeeksHead<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy SeeksHead "
    "(Applesauce sends head:track N to seek to a cylinder)");
static_assert(Recalibrates<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy Recalibrates "
    "(Applesauce sends head:zero to recalibrate to track 0)");
static_assert(MeasuresRPM<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy MeasuresRPM "
    "(Applesauce queries sync:?speed for index-to-index RPM measurement)");
static_assert(DetectsDrive<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy DetectsDrive "
    "(Applesauce queries ?kind + data:?max + psu:?5v/12v for drive detection)");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsSectors<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must NOT satisfy ReadsSectors "
    "(Applesauce is a flux device; sector decode is upstream pipeline. "
    "V1 has no readSector() — only readRawFlux() and readTrack() which "
    "internally calls readRawFlux().)");
static_assert(!WritesSectors<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must NOT satisfy WritesSectors "
    "(Applesauce writes at the raw-flux level only. The V1 writeTrack() "
    "takes raw flux bytes, not decoded sector data.)");

/* Composite predicates. */
static_assert(ImagesFlux<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");
static_assert(WritesAnything<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy WritesAnything "
    "(has WritesRawFlux)");
static_assert(FullDriveControl<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy FullDriveControl "
    "(has ControlsMotor + SeeksHead + Recalibrates — all backed by real "
    "Applesauce serial commands motor:on/off, head:track N, head:zero)");

}  // namespace uft::hal

#endif  // APPLESAUCE_PROVIDER_V2_H
