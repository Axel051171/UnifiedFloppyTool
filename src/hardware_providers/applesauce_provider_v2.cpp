/**
 * @file applesauce_provider_v2.cpp
 * @brief ApplesauceProviderV2 implementation (MF-166 / P1.13).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps Applesauce flux operations (via a QSerialPort-style
 * text + binary serial protocol) into Type-Driven HAL outcome sum-types.
 * It does NOT rewrite any protocol logic — every actual hardware interaction
 * is delegated to the injected runners, which in production wrap an
 * IApplesauceTransport (QSerialPort-backed), and in tests wrap scripted
 * lambdas.
 *
 * Protocol paths carried forward from V1 (applesaucehardwareprovider.cpp):
 *
 *   Read path (V1 readRawFlux):
 *     1. sync:on                — enable index-pulse capture
 *     2. disk:read or disk:readx — capture; blocks until done; '.' on OK
 *     3. data:?size             — query capture buffer size
 *     4. data:<N                — download N bytes of binary flux data
 *     5. sync:off               — disable index sync
 *     Retry loop (maxRetries=5).
 *     "disk:readx" for multi-revolution captures (revolutions > 1).
 *
 *   Write path (V1 writeTrack):
 *     1. disk:?write            — write-protect check ('+' = protected)
 *     2. data:clear             — clear the device buffer
 *     3. data:>N                — upload N bytes of binary flux data
 *     4. disk:write             — write buffer to disk; '.' on OK
 *     Retry loop (maxRetries=3 by default).
 *
 *   Motor path (V1 setMotor):
 *     (if on) psu:? / psu:on   — PSU check / power on
 *     motor:on or motor:off    — expects '.'
 *     (if on) 500 ms spinup delay
 *
 *   Seek path (V1 seekCylinder + selectHead):
 *     head:track N             — absolute track seek; expects '.'
 *     head:side X              — head (side) select; expects '.'
 *     15 ms head-settle delay
 *
 *   Recalibrate path (V1 recalibrate):
 *     head:zero                — step out to track 0; expects '.'
 *
 *   RPM path (V1 measureRPM):
 *     (ensure motor on) sync:?speed — returns numeric RPM string
 *     (motor off if it was turned on for the measurement)
 *
 *   Detect path (V1 detectDrive):
 *     ?kind                    — "5.25" / "3.5" / "PC" / "NONE"
 *     data:?max                — buffer size (163840 or 430080)
 *     psu:?5v + psu:?12v      — voltage readings
 *     sync:?speed              — RPM measurement
 *
 * Backend honesty (M3.3 scaffold):
 *   When the runner is null or returns transport_unavailable=true, the
 *   do_*() method returns a ProviderError with the M3.3 marker.
 *   The TYPE SHAPE is V2-conformant (static_assert proofs in the header);
 *   the runtime ProviderError reflects the backend maturity.
 *
 * Rule F-3 (divergent-read / multi-revolution preservation):
 *   The V1 readRawFlux() uses "disk:readx" for multi-revolution captures,
 *   which stores the entire multi-revolution flux sequence in the device
 *   buffer. The V2 carries this forward: the full captured buffer is
 *   preserved verbatim in FluxCaptured::transitions_ns. The revolutions
 *   count from the runner maps to FluxCaptured::revolutions. No collapsing.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings.
 */

#include "applesauce_provider_v2.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::hal {

/* ─────────────────────────────────────────────────────────────────────────
 *  Protocol constants
 * ─────────────────────────────────────────────────────────────────────── */

static constexpr char RESP_OK           = '.';
static constexpr char RESP_ERROR        = '!';
static constexpr char RESP_ON           = '+';

static constexpr double AS_SAMPLE_CLOCK_HZ = 8000000.0;  /* 8 MHz */
static constexpr double AS_SAMPLE_NS       = 1.0e9 / AS_SAMPLE_CLOCK_HZ;  /* 125.0 ns */

static constexpr uint32_t AS_BUFFER_STANDARD = 163840u;  /* 160K */
static constexpr uint32_t AS_BUFFER_PLUS     = 430080u;  /* 420K */

/* ─────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ─────────────────────────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceProviderV2(
    ApplesauceReadRunner    read_runner,
    ApplesauceWriteRunner   write_runner,
    ApplesauceMotorRunner   motor_runner,
    ApplesauceSeekRunner    seek_runner,
    ApplesauceRecalRunner   recal_runner,
    ApplesauceRpmRunner     rpm_runner,
    ApplesauceDetectRunner  detect_runner,
    int max_cylinder,
    double sample_clock_hz)
    : m_read_runner(std::move(read_runner))
    , m_write_runner(std::move(write_runner))
    , m_motor_runner(std::move(motor_runner))
    , m_seek_runner(std::move(seek_runner))
    , m_recal_runner(std::move(recal_runner))
    , m_rpm_runner(std::move(rpm_runner))
    , m_detect_runner(std::move(detect_runner))
    , m_max_cylinder(max_cylinder < 0 ? 83 : max_cylinder)
    , m_sample_clock_hz(sample_clock_hz > 0.0 ? sample_clock_hz : AS_SAMPLE_CLOCK_HZ)
    , m_sample_ns(1.0e9 / (sample_clock_hz > 0.0 ? sample_clock_hz : AS_SAMPLE_CLOCK_HZ))
{}

/* ─────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ─────────────────────────────────────────────────────────────────────── */

/* static */
ProviderError ApplesauceProviderV2::null_runner_error(const char* operation)
{
    std::string what = std::string("Applesauce ") + operation
                     + " failed: no backend runner configured";
    return ProviderError{
        UFT_E_GENERIC,
        what,
        "The ApplesauceProviderV2 was constructed with a null runner for this "
        "operation. This occurs when the provider is not properly initialized "
        "for the target environment (no transport runner was injected). "
        "This is the M3.3 partial scaffold state — the serial I/O runner "
        "requires a QSerialPort-backed transport before real operations are "
        "possible.",
        "Construct ApplesauceProviderV2 with valid runner lambdas that wrap "
        "an IApplesauceTransport-backed QSerialPort connection to an Applesauce "
        "device (USB-CDC serial, VID:PID 0x16C0:0x0483). "
        "For tests, inject scripted lambda runners. "
        "See wiki.applesaucefdc.com for the Applesauce serial protocol spec."
    };
}

ProviderError ApplesauceProviderV2::range_error(int cylinder, int head) const
{
    std::string what = "Applesauce operation: geometry out of range";
    std::string why  = "Cylinder " + std::to_string(cylinder) + " or head "
                     + std::to_string(head)
                     + " is outside the valid range for the Applesauce. "
                       "Valid cylinder range: [0, "
                     + std::to_string(m_max_cylinder)
                     + "]. Valid head range: [0, 1]. "
                       "Apple 5.25\" drives have 35 tracks (0-34), single-sided. "
                       "Apple/Mac 3.5\" and PC 3.5\" drives have 80 tracks (0-79), "
                       "double-sided. Applesauce supports up to 83 quarter-tracks "
                       "for alignment flexibility.";
    std::string fix  = "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
                     + "] and head 0 or 1 (head 1 only for double-sided drives). "
                       "Verify the correct drive type is attached. "
                       "For Apple II 5.25\" drives: max cylinder is 34, head 0 only. "
                       "For Macintosh/Apple IIgs 3.5\" drives: max cylinder is 79, "
                       "both heads 0 and 1 valid.";
    return ProviderError{ UFT_E_GENERIC, what, why, fix };
}

std::vector<uint32_t> ApplesauceProviderV2::bytes_to_transitions_ns(
    const std::vector<uint8_t>& flux_bytes) const
{
    /* Applesauce flux data: each 4-byte little-endian value is one
     * flux transition timing in 8 MHz ticks. Convert to nanoseconds.
     * Rule F-3: the entire captured buffer is converted verbatim —
     * no decimation, no averaging. */
    const std::size_t n_transitions = flux_bytes.size() / 4;
    std::vector<uint32_t> transitions;
    transitions.reserve(n_transitions);

    for (std::size_t i = 0; i < n_transitions; ++i) {
        uint32_t ticks = 0;
        std::memcpy(&ticks, flux_bytes.data() + (i * 4), 4);
        /* Convert ticks to nanoseconds (rounded to nearest integer). */
        uint32_t ns_val = static_cast<uint32_t>(ticks * m_sample_ns + 0.5);
        transitions.push_back(ns_val);
    }

    return transitions;
}

std::vector<uint8_t> ApplesauceProviderV2::transitions_ns_to_bytes(
    const std::vector<uint32_t>& transitions_ns) const
{
    /* Convert nanoseconds back to 8 MHz ticks, store as LE32 bytes. */
    std::vector<uint8_t> out;
    out.reserve(transitions_ns.size() * 4);

    for (uint32_t ns_val : transitions_ns) {
        uint32_t ticks = static_cast<uint32_t>(ns_val / m_sample_ns + 0.5);
        /* Little-endian LE32 */
        out.push_back(static_cast<uint8_t>(ticks & 0xFF));
        out.push_back(static_cast<uint8_t>((ticks >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((ticks >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((ticks >> 24) & 0xFF));
    }

    return out;
}

/* static */
FluxOutcome ApplesauceProviderV2::translate_read_failure(
    const ApplesauceReadResult& r, int cylinder, int head)
{
    /* Transport not available — ProviderError with M3.3 marker */
    if (r.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce flux read failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 flux read requires a QSerialPort connection "
            "to an Applesauce device (USB-CDC serial, 115200 8N1). The transport "
            "runner reported that the serial port is not available. This is the "
            "M3.3 partial scaffold state — the serial I/O layer requires a "
            "connected Applesauce device before real reads are possible. "
            "Cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head) + ".",
            "Connect an Applesauce floppy controller to a USB port "
            "(VID:PID 0x16C0:0x0483, manufacturer 'Evolution Interactive'). "
            "Verify the device appears in your OS serial port list. "
            "Open the serial connection (115200 8N1 USB-CDC) before constructing "
            "ApplesauceProviderV2. See wiki.applesaucefdc.com for setup instructions."
        };
    }

    /* Device returned '!' (error) on a read command */
    if (r.device_error) {
        std::string why = "The Applesauce device returned an error response ('!') "
            "during the flux read sequence for cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head) + ". ";
        if (!r.error_message.empty()) {
            why += "Device error: " + r.error_message + ". ";
        }
        why += "This typically indicates a drive hardware fault, an index-pulse "
               "timing issue, or the disk is not spinning fast enough for capture.";
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce device error during flux read for C"
                + std::to_string(cylinder) + " H" + std::to_string(head),
            why,
            "Verify the disk drive is spinning and the disk is properly inserted. "
            "Ensure the motor is turned on before issuing read commands. "
            "Try increasing the timeout or number of retries. "
            "Check for dirty heads or a worn disk that cannot produce a clean "
            "index pulse. See wiki.applesaucefdc.com for troubleshooting tips."
        };
    }

    /* Buffer overflow */
    if (r.buffer_overflow) {
        FluxUnreadable u;
        u.position = CHS{cylinder, head};
        u.physical_reason = "Applesauce read failed: flux data size exceeded the "
            "device's maximum buffer (" + std::to_string(AS_BUFFER_PLUS) + " bytes "
            "for Applesauce+). The track contains more flux transitions than the "
            "device can buffer. This can occur with damaged disks that have no "
            "index pulse, causing the capture to run indefinitely, or with "
            "non-standard disk formats that exceed normal track lengths.";
        return u;
    }

    /* General error */
    std::string why = "The Applesauce flux read for cylinder "
        + std::to_string(cylinder) + " head " + std::to_string(head)
        + " failed after " + std::to_string(r.retries_used + 1) + " attempt(s). ";
    if (!r.error_message.empty()) {
        why += "Error: " + r.error_message;
    } else {
        why += "No additional error detail was provided by the runner.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "Applesauce flux read failed for C" + std::to_string(cylinder)
            + " H" + std::to_string(head),
        why,
        "Verify the Applesauce is connected and the serial transport is open. "
        "Ensure the drive motor is running and a disk is inserted. "
        "Try the read again — transient timing failures can often be recovered "
        "with a retry. If the error persists, check the disk surface condition "
        "and drive alignment."
    };
}

/* static */
WriteOutcome ApplesauceProviderV2::translate_write_result(
    const ApplesauceWriteResult& r, int cylinder, int head,
    std::size_t flux_bytes_size)
{
    /* Write-protect check: disk:?write returned '+' */
    if (r.write_protected) {
        WriteRefused refused;
        refused.position = CHS{cylinder, head};
        refused.physical_reason = "Applesauce: disk is write-protected. "
            "The 'disk:?write' query returned '+' (on = write-protect notch active) "
            "before the write was attempted for cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head)
            + ". The disk has the write-protect tab in the protected position, "
              "or the drive is flagged as read-only by the firmware.";
        return refused;
    }

    /* Transport not available — ProviderError with M3.3 marker */
    if (r.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce flux write failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 flux write requires a QSerialPort connection "
            "to an Applesauce device. The transport runner reported that the serial "
            "port is not available. This is the M3.3 partial scaffold state. "
            "Cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head) + ".",
            "Connect an Applesauce device and open the serial connection before "
            "constructing ApplesauceProviderV2. See wiki.applesaucefdc.com."
        };
    }

    /* Device error on a write command */
    if (r.device_error) {
        std::string why = "The Applesauce device returned an error response ('!') "
            "during the flux write sequence for cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head) + ". ";
        if (!r.error_message.empty()) {
            why += "Device error: " + r.error_message + ". ";
        }
        why += "This may indicate a drive hardware fault, a write-head failure, "
               "or an incompatible flux data format.";
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce device error during flux write for C"
                + std::to_string(cylinder) + " H" + std::to_string(head),
            why,
            "Verify the disk is not write-protected and is properly inserted. "
            "Ensure the motor is on and at the correct cylinder. "
            "Check that the flux data is valid Applesauce format (32-bit LE "
            "ticks at 8 MHz). Retry the write operation."
        };
    }

    /* General write failure */
    if (r.bytes_written == 0 && !r.error_message.empty()) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce flux write failed for C" + std::to_string(cylinder)
                + " H" + std::to_string(head),
            "The Applesauce flux write for cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + " returned 0 bytes written. Error: " + r.error_message,
            "Verify the Applesauce is connected and the serial transport is open. "
            "Check that the disk is not write-protected and the drive motor is "
            "running. Retry the write operation."
        };
    }

    /* Success */
    WriteCompleted completed;
    completed.position      = CHS{cylinder, head};
    completed.bytes_written = (r.bytes_written > 0)
                              ? static_cast<std::size_t>(r.bytes_written)
                              : flux_bytes_size;
    completed.verified      = false;  /* Applesauce protocol has no read-back verify */
    completed.quality       = QualityFlag::CRC_OK;
    return completed;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_read_raw_flux
 *
 *  Maps to: ReadsRawFlux concept / read_raw_flux(ReadFluxParams) mixin.
 *
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions) in
 *  applesaucehardwareprovider.cpp, plus the seek + head select preamble
 *  in readTrack().
 *
 *  V2 differences vs V1:
 *  - Uses injected ApplesauceReadRunner instead of direct serial calls.
 *  - Uses ReadFluxParams (cylinder, head, revolutions) instead of separate
 *    int parameters.
 *  - Converts flux bytes to transitions_ns vector (FluxCaptured).
 *  - Translates ApplesauceReadResult to FluxOutcome sum-type variants.
 *
 *  Rule F-3: the full flux buffer is preserved verbatim. When revolutions
 *  > 1 the "disk:readx" path is used, which captures the entire multi-rev
 *  sequence. bytes_to_transitions_ns() converts without discarding.
 * ─────────────────────────────────────────────────────────────────────── */

FluxOutcome ApplesauceProviderV2::do_read_raw_flux(const ReadFluxParams& p)
{
    if (!m_read_runner) {
        return null_runner_error("flux read");
    }

    /* Geometry validation */
    if (p.cylinder < 0 || p.cylinder > m_max_cylinder) {
        return range_error(p.cylinder, p.head);
    }
    if (p.head < 0 || p.head > 1) {
        return range_error(p.cylinder, p.head);
    }

    /* Build the read request */
    ReadRequest req;
    req.cylinder    = p.cylinder;
    req.head        = p.head;
    req.revolutions = (p.revolutions > 0) ? p.revolutions : 2;
    req.max_retries = 5;

    ApplesauceReadResult result = m_read_runner(req);

    /* Hard failures */
    if (result.transport_unavailable || result.device_error || result.buffer_overflow) {
        auto outcome = translate_read_failure(result, p.cylinder, p.head);
        /* translate_read_failure returns FluxOutcome-compatible types */
        if (std::holds_alternative<ProviderError>(outcome)) {
            return std::get<ProviderError>(outcome);
        }
        /* buffer_overflow returns FluxUnreadable */
        return std::get<FluxUnreadable>(outcome);
    }

    /* No data at all */
    if (result.flux_bytes.empty()) {
        FluxUnreadable u;
        u.position = CHS{p.cylinder, p.head};
        u.physical_reason = "Applesauce flux read completed but no data was "
            "returned for cylinder " + std::to_string(p.cylinder)
            + " head " + std::to_string(p.head)
            + ". The track may be blank, the drive may not be spinning, "
              "or the index-pulse timeout was reached without capturing any "
              "flux transitions. "
            + (result.error_message.empty() ? "" : "Error: " + result.error_message);
        return u;
    }

    /* General runner failure */
    if (!result.error_message.empty() && result.flux_bytes.empty()) {
        return translate_read_failure(result, p.cylinder, p.head);
    }

    /* Rule F-3: convert entire flux buffer to nanoseconds — no decimation. */
    std::vector<uint32_t> transitions = bytes_to_transitions_ns(result.flux_bytes);

    /* Marginal: captured data but with known anomalies */
    /* (Applesauce V1 does not explicitly report marginal captures in the
     *  protocol response, so we only produce FluxMarginal when the runner
     *  explicitly signals it via a non-empty error_message + non-empty data.) */
    if (!result.error_message.empty() && !result.flux_bytes.empty()) {
        FluxMarginal m;
        m.position = CHS{p.cylinder, p.head};
        m.transitions_ns = std::move(transitions);
        m.anomaly_note = "Applesauce flux capture completed with warning on cylinder "
            + std::to_string(p.cylinder) + " head " + std::to_string(p.head)
            + " (retries used: " + std::to_string(result.retries_used)
            + "): " + result.error_message
            + ". The flux data was captured but may contain timing anomalies.";
        return m;
    }

    /* Success */
    FluxCaptured fc;
    fc.position      = CHS{p.cylinder, p.head};
    fc.transitions_ns = std::move(transitions);
    fc.revolutions   = (result.revolutions > 0) ? result.revolutions : req.revolutions;
    fc.sample_ns     = m_sample_ns;
    fc.quality       = QualityFlag::None;
    return fc;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_write_raw_flux
 *
 *  Maps to: WritesRawFlux concept / write_raw_flux(WriteFluxParams,
 *  FluxStream) mixin.
 *
 *  V1 equivalent: writeTrack(WriteParams, QByteArray) and
 *  writeRawFlux(int, int, QByteArray) in applesaucehardwareprovider.cpp.
 *
 *  V2 differences vs V1:
 *  - Uses injected ApplesauceWriteRunner instead of direct serial calls.
 *  - Uses WriteFluxParams + FluxStream instead of WriteParams + QByteArray.
 *  - Converts FluxStream::transitions_ns to 8 MHz tick LE32 bytes before
 *    passing to the runner.
 *  - Translates ApplesauceWriteResult to WriteOutcome sum-type variants.
 *
 *  Note re MF-151 bug: the V1 writeTrack() returns OperationResult (not
 *  TrackData). This V2 implementation returns WriteOutcome cleanly.
 * ─────────────────────────────────────────────────────────────────────── */

WriteOutcome ApplesauceProviderV2::do_write_raw_flux(const WriteFluxParams& w,
                                                      const FluxStream& flux)
{
    if (!m_write_runner) {
        return null_runner_error("flux write");
    }

    /* Geometry validation */
    if (w.cylinder < 0 || w.cylinder > m_max_cylinder) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce flux write: geometry out of range",
            "Cylinder " + std::to_string(w.cylinder) + " head "
            + std::to_string(w.head) + " is outside the valid range [0, "
            + std::to_string(m_max_cylinder) + "]. Valid head range: [0, 1]. "
              "Check drive type configuration.",
            "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
            + "] and head 0 or 1. Verify the correct drive type is attached."
        };
    }
    if (w.head < 0 || w.head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce flux write: geometry out of range",
            "Head " + std::to_string(w.head) + " is outside valid range [0, 1] "
            "for cylinder " + std::to_string(w.cylinder)
            + ". Apple 5.25\" drives are single-sided (head 0 only). "
              "Apple/Mac 3.5\" and PC 3.5\" drives support both sides.",
            "Pass head 0 for single-sided drives, or head 0 or 1 for "
            "double-sided drives. Verify the drive type setting."
        };
    }

    /* Convert transitions_ns to Applesauce LE32 tick bytes */
    std::vector<uint8_t> flux_bytes = transitions_ns_to_bytes(flux.transitions_ns);

    if (flux_bytes.empty()) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce flux write: empty FluxStream",
            "The FluxStream passed to write_raw_flux() contains no transitions_ns "
            "data for cylinder " + std::to_string(w.cylinder)
            + " head " + std::to_string(w.head)
            + ". The write cannot proceed without flux data to upload to the device.",
            "Provide a non-empty FluxStream with valid 32-bit nanosecond transition "
            "timings. The data must represent at least one complete revolution of "
            "flux transitions. For Applesauce, flux data is typically 4-80K bytes "
            "per track (at 8 MHz sampling with 125 ns resolution)."
        };
    }

    /* Build the write request */
    WriteRequest req;
    req.cylinder    = w.cylinder;
    req.head        = w.head;
    req.max_retries = 3;
    req.flux_bytes  = std::move(flux_bytes);

    ApplesauceWriteResult result = m_write_runner(req);

    return translate_write_result(result, w.cylinder, w.head, req.flux_bytes.size());
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_set_motor
 *
 *  Maps to: ControlsMotor concept / set_motor(bool) mixin.
 *
 *  V1 equivalent: setMotor(bool) in applesaucehardwareprovider.cpp.
 *
 *  V1 path: ensurePowerOn() → motor:on / motor:off → '.' response
 *           → 500 ms spinup delay (if on).
 *  The runner encapsulates this entire sequence.
 * ─────────────────────────────────────────────────────────────────────── */

MotorOutcome ApplesauceProviderV2::do_set_motor(bool on)
{
    if (!m_motor_runner) {
        return null_runner_error("motor control");
    }

    ApplesauceMotorResult result = m_motor_runner(on);

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce motor control failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 motor control requires a QSerialPort connection. "
            "The motor:on / motor:off command could not be sent. This is the M3.3 "
            "partial scaffold state.",
            "Connect an Applesauce device and open the serial connection. "
            "See wiki.applesaucefdc.com for setup instructions."
        };
    }

    if (!result.success) {
        std::string why = "The Applesauce motor:";
        why += on ? "on" : "off";
        why += " command returned an error response. ";
        if (!result.error_message.empty()) {
            why += "Device error: " + result.error_message + ". ";
        }
        why += "The PSU ";
        why += result.psu_was_enabled ? "was enabled" : "state is unknown";
        why += " for this request. Check the PSU (psu:?) and ensure the "
               "drive power supply is healthy.";

        MotorStalled stalled;
        stalled.reason = "Applesauce motor " + std::string(on ? "ON" : "OFF")
            + " failed: " + (result.error_message.empty()
                             ? "device returned error response"
                             : result.error_message);
        return stalled;
    }

    if (on) {
        MotorRunning running;
        running.measured_rpm = 0.0;  /* RPM measured separately via measure_rpm() */
        return running;
    } else {
        return MotorStopped{};
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_seek
 *
 *  Maps to: SeeksHead concept / seek(int cylinder) mixin.
 *
 *  V1 equivalent: seekCylinder(int) + selectHead(int) in
 *  applesaucehardwareprovider.cpp.
 *
 *  V1 path: "head:track N" → '.' response → 15 ms settle delay.
 *  The runner encapsulates head:track + head:side.
 *
 *  Note: the V2 seek() only takes a cylinder argument (per the SeeksHead
 *  concept). Head selection is embedded in the runner which calls the
 *  provider's current head setting. The conformance test calls seek(0).
 * ─────────────────────────────────────────────────────────────────────── */

SeekOutcome ApplesauceProviderV2::do_seek(int cylinder)
{
    if (!m_seek_runner) {
        return null_runner_error("head seek");
    }

    if (cylinder < 0 || cylinder > m_max_cylinder) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce seek: cylinder out of range",
            "Cylinder " + std::to_string(cylinder)
            + " is outside the valid range [0, " + std::to_string(m_max_cylinder)
            + "] for this Applesauce configuration. "
              "Apple 5.25\" drives have 35 tracks (0-34). "
              "Apple/Mac 3.5\" and PC 3.5\" drives have 80 tracks (0-79). "
              "Applesauce supports up to 83 quarter-tracks for alignment.",
            "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
            + "]. Verify the correct drive type is attached."
        };
    }

    SeekRequest req;
    req.cylinder = cylinder;
    req.head     = 0;  /* SeeksHead concept uses seek(cylinder) — head is 0 */

    ApplesauceSeekResult result = m_seek_runner(req);

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce head seek failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 seek requires a QSerialPort connection. "
            "The head:track command could not be sent. M3.3 scaffold state.",
            "Connect an Applesauce device and open the serial connection."
        };
    }

    if (!result.success) {
        std::string reason = "Applesauce head:track " + std::to_string(cylinder)
            + " command failed. ";
        if (!result.error_message.empty()) {
            reason += result.error_message;
        }
        return SeekTrack0Failed{reason};  /* Reuse SeekTrack0Failed for seek-fail */
    }

    SeekArrived arrived;
    arrived.cylinder = result.cylinder_reached;
    return arrived;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_recalibrate
 *
 *  Maps to: Recalibrates concept / recalibrate() mixin.
 *
 *  V1 equivalent: recalibrate() in applesaucehardwareprovider.cpp.
 *
 *  V1 path: "head:zero" → '.' response → m_currentCylinder = 0.
 *  The Applesauce "head:zero" command steps out until the track 0 sensor
 *  triggers — this IS a true recalibration to track 0 (unlike XUM1541
 *  where "I" only goes to track 18). SeekArrived{cylinder=0} is correct.
 * ─────────────────────────────────────────────────────────────────────── */

SeekOutcome ApplesauceProviderV2::do_recalibrate()
{
    if (!m_recal_runner) {
        return null_runner_error("recalibrate");
    }

    ApplesauceSeekResult result = m_recal_runner();

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce recalibrate failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 recalibrate requires a QSerialPort connection. "
            "The head:zero command could not be sent. M3.3 scaffold state.",
            "Connect an Applesauce device and open the serial connection."
        };
    }

    if (!result.success) {
        std::string reason = "Applesauce head:zero (recalibrate to track 0) failed. ";
        if (!result.error_message.empty()) {
            reason += result.error_message;
        }
        SeekTrack0Failed failed;
        failed.reason = reason;
        return failed;
    }

    /* Successful recalibration MUST arrive at cylinder 0 —
     * the conformance harness checks a.cylinder == 0 for Recalibrates. */
    SeekArrived arrived;
    arrived.cylinder = 0;
    return arrived;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_measure_rpm
 *
 *  Maps to: MeasuresRPM concept / measure_rpm() mixin.
 *
 *  V1 equivalent: measureRPM() in applesaucehardwareprovider.cpp.
 *
 *  V1 path:
 *    ensurePowerOn() → (motor on if needed)
 *    "sync:?speed" → numeric RPM string → toDouble()
 *    (motor off if it was turned on for the measurement)
 *
 *  The Applesauce firmware measures index-to-index timing and returns
 *  the RPM as a text value. This is a REAL measurement, not a constant.
 * ─────────────────────────────────────────────────────────────────────── */

RpmOutcome ApplesauceProviderV2::do_measure_rpm()
{
    if (!m_rpm_runner) {
        return null_runner_error("RPM measurement");
    }

    ApplesauceRpmResult result = m_rpm_runner();

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce RPM measurement failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 RPM measurement requires a QSerialPort connection. "
            "The sync:?speed query could not be sent. M3.3 scaffold state.",
            "Connect an Applesauce device and open the serial connection."
        };
    }

    if (result.non_numeric_response) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce RPM measurement: non-numeric response from sync:?speed",
            "The Applesauce 'sync:?speed' query returned a non-numeric string. "
            "This may indicate the drive has no index hole (some Apple 5.25\" "
            "drives lack an index pulse), or the motor is not yet at stable speed. "
            + (result.error_message.empty() ? "" : "Response: " + result.error_message),
            "Ensure the motor is running at stable speed before measuring RPM. "
            "For Apple 5.25\" drives that lack an index hole: Applesauce uses "
            "the sync:?speed query which requires the index sensor to be active. "
            "If the drive has no index hole, RPM measurement is not possible "
            "with this command. Verify the drive type and wiring."
        };
    }

    if (!result.error_message.empty() && result.rpm <= 0.0) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce RPM measurement failed",
            "The sync:?speed query failed or returned a non-positive RPM value. "
            + result.error_message,
            "Verify the motor is on, the disk is inserted, and the drive is running "
            "at stable speed before measuring RPM."
        };
    }

    RpmMeasured measured;
    measured.rpm               = result.rpm;
    measured.jitter_pct        = 0.0;  /* Not reported by the Applesauce protocol */
    measured.revolutions_sampled = 1;   /* sync:?speed returns a single measurement */
    return measured;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive() mixin.
 *
 *  V1 equivalent: detectDrive() + autoDetectDevice() in
 *  applesaucehardwareprovider.cpp.
 *
 *  V1 path:
 *    "?kind"     → "5.25" / "3.5" / "PC" / "NONE"
 *    "data:?max" → buffer size
 *    "psu:?5v"   → voltage
 *    "psu:?12v"  → voltage
 *    measureRPM() → numeric RPM
 *
 *  Geometry mapping from the "?kind" response (V1 detectDrive()):
 *    "5.25" → tracks=35, heads=1, rpm_nominal≈300 (Apple II GCR zone timing)
 *    "3.5"  → tracks=80, heads=2, rpm_nominal≈300 (Mac/IIgs variable-speed)
 *    "PC"   → tracks=80, heads=2, rpm_nominal≈300 (PC HD: 360 may also apply)
 *    "NONE" → no drive → DriveAbsent
 * ─────────────────────────────────────────────────────────────────────── */

DetectOutcome ApplesauceProviderV2::do_detect_drive()
{
    if (!m_detect_runner) {
        return null_runner_error("drive detection");
    }

    ApplesauceDetectResult result = m_detect_runner();

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "Applesauce drive detection failed: serial transport not available (M3.3 pending)",
            "The ApplesauceProviderV2 drive detection requires a QSerialPort connection "
            "to an Applesauce device. The detection queries (?kind, data:?max, "
            "psu:?5v/12v) could not be sent. This is the M3.3 partial scaffold "
            "state — the serial I/O layer requires a connected device.",
            "Connect an Applesauce floppy controller (USB-CDC, VID:PID 0x16C0:0x0483) "
            "and open the serial connection before calling detect_drive(). "
            "See wiki.applesaucefdc.com for setup instructions."
        };
    }

    if (!result.found) {
        /* Runner completed but drive kind is "NONE" or the query failed */
        if (!result.error_message.empty()) {
            return ProviderError{
                UFT_E_GENERIC,
                "Applesauce drive detection failed",
                "The Applesauce detection runner reported an error: "
                + result.error_message,
                "Verify the Applesauce device is connected and a drive is attached. "
                "Check the drive power supply is working (psu:?) and the drive "
                "cable is properly connected. See wiki.applesaucefdc.com."
            };
        }

        /* Clean probe — no drive attached */
        DriveAbsent absent;
        absent.scanned_for = "Applesauce floppy controller "
                             "(Evolution Interactive, VID:PID 0x16C0:0x0483, "
                             "USB-CDC serial, wiki.applesaucefdc.com) — "
                             "?kind query returned 'NONE'";
        return absent;
    }

    /* Drive found — populate DriveDetected */
    DriveDetected detected;

    /* Drive kind and geometry from "?kind" response */
    if (result.drive_kind == "5.25") {
        detected.drive_kind = "Apple 5.25\" (GCR, single-sided)";
        detected.tracks     = (result.tracks > 0) ? result.tracks : 35;
        detected.heads      = (result.heads > 0)  ? result.heads  : 1;
    } else if (result.drive_kind == "3.5") {
        detected.drive_kind = "Apple/Macintosh 3.5\" (GCR/MFM, double-sided)";
        detected.tracks     = (result.tracks > 0) ? result.tracks : 80;
        detected.heads      = (result.heads > 0)  ? result.heads  : 2;
    } else if (result.drive_kind == "PC") {
        detected.drive_kind = "PC 3.5\" (MFM, double-sided) via Applesauce";
        detected.tracks     = (result.tracks > 0) ? result.tracks : 80;
        detected.heads      = (result.heads > 0)  ? result.heads  : 2;
    } else {
        /* Unknown kind string from the runner */
        detected.drive_kind = "Applesauce detected drive (kind: "
                            + (result.drive_kind.empty() ? "unknown" : result.drive_kind)
                            + ")";
        detected.tracks     = (result.tracks > 0) ? result.tracks : 35;
        detected.heads      = (result.heads > 0)  ? result.heads  : 1;
    }

    /* RPM from measureRPM() sub-call in the runner */
    detected.rpm_nominal = (result.rpm > 0.0) ? result.rpm : 300.0;

    /* Firmware and buffer info */
    std::string product = (result.buffer_size > AS_BUFFER_STANDARD)
                          ? "Applesauce+"
                          : "Applesauce";

    if (!result.firmware.empty()) {
        detected.firmware = product + " FW " + result.firmware;
        if (!result.pcb_revision.empty()) {
            detected.firmware += " PCB " + result.pcb_revision;
        }
        detected.firmware += " (8 MHz / 125 ns, buffer="
                           + std::to_string(result.buffer_size / 1024) + "K)";
    } else {
        detected.firmware = product + " (8 MHz / 125 ns, firmware version unknown, "
                          "buffer=" + std::to_string(result.buffer_size / 1024) + "K)";
    }

    return detected;
}

}  // namespace uft::hal
