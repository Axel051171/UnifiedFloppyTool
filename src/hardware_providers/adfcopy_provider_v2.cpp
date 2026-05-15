/**
 * @file adfcopy_provider_v2.cpp
 * @brief ADFCopyProviderV2 implementation (MF-167 / P1.14).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps ADFCopy flux operations (via a QSerialPort-style binary
 * serial protocol) into Type-Driven HAL outcome sum-types. It does NOT
 * rewrite any protocol logic — every actual hardware interaction is
 * delegated to the injected runners, which in production wrap an
 * IADFCopyTransport (QSerialPort-backed), and in tests wrap scripted lambdas.
 *
 * Protocol paths carried forward from V1 (adfcopyhardwareprovider.cpp):
 *
 *   Flux read path (V1 readRawFlux → cmdReadFlux):
 *     1. ADFC_CMD_READ_FLUX (0x06) + track(1) + revolutions(1)
 *     2. Read 3-byte header: status(1) + length_hi(1) + length_lo(1)
 *     3. Read binary flux payload (length bytes)
 *        (rejected if status == 'E' or 'D', or if length <= 0 or > 524288)
 *
 *   Motor on path (V1 setMotor(true) → cmdInit):
 *     1. ADFC_CMD_INIT (0x01)
 *     2. Wait for status byte 'O' (ADFC_RSP_OK)
 *     (Motor-off in V1: sets m_motorOn=false, NO serial command.)
 *
 *   Seek path (V1 seekCylinder → cmdSeek):
 *     1. ADFC_CMD_SEEK (0x02) + track(1)   [track = cylinder*2 + head]
 *     2. Wait for status byte 'O'
 *
 *   Recalibrate path (V1 recalibrate → seekCylinder(0) → cmdSeek):
 *     1. ADFC_CMD_SEEK (0x02) + 0x00       [track 0 = cylinder 0, head 0]
 *     2. Wait for status byte 'O'
 *
 *   Detect path (V1 detectDrive → cmdGetStatus):
 *     1. ADFC_CMD_GET_STATUS (0x0B)
 *     2. Read 1-byte status bitmask
 *        Bit 0: ADFC_STATUS_DISK_PRESENT
 *        Bit 1: ADFC_STATUS_WRITE_PROT
 *        Bit 2: ADFC_STATUS_MOTOR_ON
 *        Bit 3: ADFC_STATUS_FLUX_CAPABLE
 *
 * Backend honesty:
 *   When the runner is null or returns transport_unavailable=true, the
 *   do_*() method returns a ProviderError. The TYPE SHAPE is V2-conformant
 *   (static_assert proofs in the header); the runtime ProviderError reflects
 *   the backend state.
 *
 * Rule F-3 (divergent-read / multi-revolution preservation):
 *   The V1 readRawFlux() passes the revolutions parameter to ADFC_CMD_READ_FLUX
 *   and stores the full binary flux payload verbatim. The V2 carries this
 *   forward: the complete device response is stored in ADFCopyReadResult::flux_bytes
 *   without truncation or decimation. bytes_to_transitions_ns() converts
 *   every byte pair verbatim — no collapsing.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings.
 */

#include "adfcopy_provider_v2.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::hal {

/* ─────────────────────────────────────────────────────────────────────────
 *  Protocol constants (from adfcopyhardwareprovider.h)
 * ─────────────────────────────────────────────────────────────────────── */

static constexpr uint8_t RESP_OK     = 'O';  /* ADFC_RSP_OK */
static constexpr uint8_t RESP_ERROR  = 'E';  /* ADFC_RSP_ERROR */
static constexpr uint8_t RESP_NODISK = 'D';  /* ADFC_RSP_NODISK */

static constexpr double ADFC_SAMPLE_CLOCK_HZ = 40000000.0;  /* 40 MHz */
static constexpr double ADFC_SAMPLE_NS       = 1.0e9 / ADFC_SAMPLE_CLOCK_HZ;  /* 25.0 ns */

/* Amiga standard geometry */
static constexpr int ADFC_STD_CYLINDERS = 80;
static constexpr int ADFC_HEADS         = 2;

/* ─────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ─────────────────────────────────────────────────────────────────────── */

ADFCopyProviderV2::ADFCopyProviderV2(
    ADFCopyReadRunner    read_runner,
    ADFCopyMotorRunner   motor_runner,
    ADFCopySeekRunner    seek_runner,
    ADFCopyRecalRunner   recal_runner,
    ADFCopyDetectRunner  detect_runner,
    int    max_cylinder,
    double sample_clock_hz)
    : m_read_runner(std::move(read_runner))
    , m_motor_runner(std::move(motor_runner))
    , m_seek_runner(std::move(seek_runner))
    , m_recal_runner(std::move(recal_runner))
    , m_detect_runner(std::move(detect_runner))
    , m_max_cylinder(max_cylinder < 0 ? 83 : max_cylinder)
    , m_sample_clock_hz(sample_clock_hz > 0.0 ? sample_clock_hz : ADFC_SAMPLE_CLOCK_HZ)
    , m_sample_ns(1.0e9 / (sample_clock_hz > 0.0 ? sample_clock_hz : ADFC_SAMPLE_CLOCK_HZ))
    , m_current_head(0)
{}

/* ─────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ─────────────────────────────────────────────────────────────────────── */

/* static */
ProviderError ADFCopyProviderV2::null_runner_error(const char* operation)
{
    std::string what = std::string("ADFCopy ") + operation
                     + " failed: no backend runner configured";
    return ProviderError{
        UFT_E_GENERIC,
        what,
        "The ADFCopyProviderV2 was constructed with a null runner for this "
        "operation. This occurs when the provider is not properly initialized "
        "for the target environment (no transport runner was injected). "
        "ADFCopy uses a binary serial protocol over USB-CDC at 115200 8N1 "
        "(Teensy/PJRC VID:PID 0x16C0:0x0483). The runner requires a connected "
        "QSerialPort before real operations are possible.",
        "Construct ADFCopyProviderV2 with valid runner lambdas that wrap an "
        "IADFCopyTransport-backed QSerialPort connection to an ADFCopy or "
        "ADF-Drive device (USB-CDC serial, PJRC Teensy VID:PID 0x16C0:0x0483). "
        "For tests, inject scripted lambda runners. "
        "The device responds to ADFC_CMD_PING (0x00) with a version string "
        "to confirm identity before issuing other commands."
    };
}

ProviderError ADFCopyProviderV2::range_error(int cylinder, int head) const
{
    std::string what = "ADFCopy operation: geometry out of range";
    std::string why  = "Cylinder " + std::to_string(cylinder) + " or head "
                     + std::to_string(head)
                     + " is outside the valid range for ADFCopy. "
                       "Valid cylinder range: [0, "
                     + std::to_string(m_max_cylinder)
                     + "]. Valid head range: [0, 1]. "
                       "ADFCopy uses Amiga track encoding: "
                       "track = cylinder*2 + head (track range 0..159 for "
                       "standard 80-cylinder Amiga DD disks). "
                       "The V1 firmware allows up to cylinder 83 "
                       "(ADFC_MAX_CYLINDERS) for over-scan alignment.";
    std::string fix  = "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
                     + "] and head 0 or 1. "
                       "Amiga DD disks: 80 cylinders (0-79), both heads. "
                       "Amiga HD disks: same geometry, 22 sectors per track. "
                       "Verify the disk is properly inserted and the drive is "
                       "an Amiga-compatible 3.5\" mechanism.";
    return ProviderError{ UFT_E_GENERIC, what, why, fix };
}

std::vector<uint32_t> ADFCopyProviderV2::bytes_to_transitions_ns(
    const std::vector<uint8_t>& flux_bytes) const
{
    /* ADFCopy flux data: each 2-byte big-endian value is one flux transition
     * timing in 40 MHz ticks (25 ns per tick). Convert to nanoseconds.
     * Rule F-3: the entire captured buffer is converted verbatim —
     * no decimation, no averaging, no resampling. */
    const std::size_t n_transitions = flux_bytes.size() / 2;
    std::vector<uint32_t> transitions;
    transitions.reserve(n_transitions);

    for (std::size_t i = 0; i < n_transitions; ++i) {
        uint16_t ticks = static_cast<uint16_t>(
            (static_cast<uint16_t>(flux_bytes[i * 2]) << 8) |
             static_cast<uint16_t>(flux_bytes[i * 2 + 1]));
        /* Convert ticks to nanoseconds (rounded to nearest integer). */
        uint32_t ns_val = static_cast<uint32_t>(ticks * m_sample_ns + 0.5);
        transitions.push_back(ns_val);
    }

    return transitions;
}

/* static */
FluxOutcome ADFCopyProviderV2::translate_read_failure(
    const ADFCopyReadResult& r, int cylinder, int head)
{
    /* Transport not available — ProviderError */
    if (r.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy flux read failed: serial transport not available",
            "The ADFCopyProviderV2 flux read requires a QSerialPort connection "
            "to an ADFCopy or ADF-Drive device (USB-CDC serial, 115200 8N1). "
            "The transport runner reported that the serial port is not available. "
            "Cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head) + ".",
            "Connect an ADFCopy/ADF-Drive device (Teensy-based Amiga floppy "
            "controller, PJRC VID:PID 0x16C0:0x0483) to a USB port. "
            "Verify the device appears in your OS serial port list. "
            "Open the serial connection (115200 8N1 USB-CDC) before "
            "constructing ADFCopyProviderV2. Issue ADFC_CMD_PING (0x00) to "
            "confirm the device responds with an 'ADF-Copy' or 'ADF-Drive' "
            "version string before reading flux."
        };
    }

    /* No disk present */
    if (r.no_disk) {
        FluxUnreadable u;
        u.position = CHS{cylinder, head};
        u.physical_reason = "ADFCopy flux read failed: no disk detected. "
            "ADFC_RSP_NODISK ('D') response received from the device for "
            "cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + ". The drive reports no disk is inserted, the disk is not "
              "fully seated, or the disk has been ejected since the last "
              "command.";
        return u;
    }

    /* Device returned error status */
    if (r.device_error) {
        std::string why = "The ADFCopy device returned an error response "
            "(ADFC_RSP_ERROR='E') during the flux read sequence for cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head) + ". ";
        if (!r.error_message.empty()) {
            why += "Device error: " + r.error_message + ". ";
        }
        why += "This typically indicates a drive hardware fault, a timing "
               "issue with the Amiga floppy mechanism, or the disk is not "
               "spinning at the correct 300 RPM speed for flux capture.";
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy device error during flux read for C"
                + std::to_string(cylinder) + " H" + std::to_string(head),
            why,
            "Verify the disk drive is spinning and the disk is properly "
            "inserted. Ensure the motor has been activated (ADFC_CMD_INIT) "
            "before issuing flux read commands. Check for dirty drive heads "
            "— an Amiga DD drive with dirty heads may fail to capture clean "
            "flux transitions. Retry the read; transient errors often recover "
            "on the second attempt."
        };
    }

    /* General error */
    std::string why = "The ADFCopy flux read for cylinder "
        + std::to_string(cylinder) + " head " + std::to_string(head)
        + " failed. ";
    if (!r.error_message.empty()) {
        why += "Error: " + r.error_message;
    } else {
        why += "No additional error detail was provided by the runner.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "ADFCopy flux read failed for C" + std::to_string(cylinder)
            + " H" + std::to_string(head),
        why,
        "Verify the ADFCopy is connected and the serial transport is open. "
        "Ensure the drive motor has been activated (ADFC_CMD_INIT) and a "
        "disk is inserted. Try the read again — transient timing failures "
        "can often be recovered with a retry. If the error persists, check "
        "the disk surface condition and the drive head alignment."
    };
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_read_raw_flux
 *
 *  Maps to: ReadsRawFlux concept / read_raw_flux(ReadFluxParams) mixin.
 *
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions) in
 *  adfcopyhardwareprovider.cpp → cmdReadFlux(track, revolutions).
 *
 *  Protocol:
 *    ADFC_CMD_READ_FLUX (0x06) + track(1) + revolutions(1)
 *    → 3-byte header: status(1) + length_hi(1) + length_lo(1)
 *    → binary flux payload (length bytes)
 *
 *  Rule F-3: the full flux buffer (all revolutions captured by the device)
 *  is preserved verbatim in FluxCaptured::transitions_ns. No collapsing.
 * ─────────────────────────────────────────────────────────────────────── */

FluxOutcome ADFCopyProviderV2::do_read_raw_flux(const ReadFluxParams& p)
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

    /* Update current head state (embedded in track number for ADFCopy) */
    m_current_head = p.head;

    /* Build the read request */
    ReadRequest req;
    req.cylinder    = p.cylinder;
    req.head        = p.head;
    req.revolutions = (p.revolutions > 0) ? p.revolutions : 2;

    ADFCopyReadResult result = m_read_runner(req);

    /* Hard failures */
    if (result.transport_unavailable || result.device_error || result.no_disk) {
        auto outcome = translate_read_failure(result, p.cylinder, p.head);
        if (std::holds_alternative<ProviderError>(outcome)) {
            return std::get<ProviderError>(outcome);
        }
        return std::get<FluxUnreadable>(outcome);
    }

    /* No data at all */
    if (result.flux_bytes.empty()) {
        FluxUnreadable u;
        u.position = CHS{p.cylinder, p.head};
        u.physical_reason = "ADFCopy flux read completed but no data was "
            "returned for cylinder " + std::to_string(p.cylinder)
            + " head " + std::to_string(p.head)
            + ". The track may be blank, the drive may not be spinning, "
              "or the flux capture yielded zero timing transitions. "
            + (result.error_message.empty()
               ? ""
               : "Error: " + result.error_message);
        return u;
    }

    /* General runner failure with error message but no data */
    if (!result.error_message.empty() && result.flux_bytes.empty()) {
        return translate_read_failure(result, p.cylinder, p.head);
    }

    /* Rule F-3: convert entire flux buffer to nanoseconds — no decimation.
     * ADFCopy 16-bit big-endian 40 MHz ticks → 25 ns per tick. */
    std::vector<uint32_t> transitions = bytes_to_transitions_ns(result.flux_bytes);

    /* Marginal: data captured but with known anomaly (non-empty error_message
     * AND non-empty flux_bytes — runner reports partial success). */
    if (!result.error_message.empty() && !result.flux_bytes.empty()) {
        FluxMarginal m;
        m.position       = CHS{p.cylinder, p.head};
        m.transitions_ns = std::move(transitions);
        m.anomaly_note   = "ADFCopy flux capture completed with warning on "
            "cylinder " + std::to_string(p.cylinder)
            + " head " + std::to_string(p.head)
            + ": " + result.error_message
            + ". The flux data was captured but may contain timing anomalies.";
        return m;
    }

    /* Success */
    FluxCaptured fc;
    fc.position       = CHS{p.cylinder, p.head};
    fc.transitions_ns = std::move(transitions);
    fc.revolutions    = (result.revolutions > 0) ? result.revolutions : req.revolutions;
    fc.sample_ns      = m_sample_ns;
    fc.quality        = QualityFlag::None;
    return fc;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_set_motor
 *
 *  Maps to: ControlsMotor concept / set_motor(bool) mixin.
 *
 *  V1 equivalent: setMotor(bool) in adfcopyhardwareprovider.cpp.
 *
 *  V1 motor-ON path: cmdInit() → ADFC_CMD_INIT (0x01) → wait 'O'.
 *  V1 motor-OFF path: sets m_motorOn=false only, NO serial command.
 *
 *  Motor-on activates the drive motor AND seeks to track 0 (INIT homes
 *  the head). Motor-off is best-effort: the runner reports success=true
 *  for motor-off even without a dedicated serial command, since the
 *  ADFCopy protocol has no explicit motor-off command.
 * ─────────────────────────────────────────────────────────────────────── */

MotorOutcome ADFCopyProviderV2::do_set_motor(bool on)
{
    if (!m_motor_runner) {
        return null_runner_error("motor control");
    }

    ADFCopyMotorResult result = m_motor_runner(on);

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy motor control failed: serial transport not available",
            "The ADFCopyProviderV2 motor control requires a QSerialPort "
            "connection to an ADFCopy device. The ADFC_CMD_INIT (0x01) "
            "command could not be sent — the serial transport is unavailable.",
            "Connect an ADFCopy/ADF-Drive device and open the serial "
            "connection (115200 8N1 USB-CDC, PJRC VID:PID 0x16C0:0x0483) "
            "before calling set_motor(). Issue ADFC_CMD_PING first to "
            "confirm the device is responding."
        };
    }

    if (!result.success) {
        std::string why = "The ADFCopy ADFC_CMD_INIT (0x01) command failed — "
            "motor ";
        why += on ? "ON" : "OFF";
        why += " sequence returned an error response. ";
        if (!result.error_message.empty()) {
            why += "Device error: " + result.error_message + ". ";
        }
        why += "The INIT command spins up the drive motor and seeks to track 0. "
               "A failure here may indicate a drive hardware fault, no drive "
               "power supply, or a loose cable between the ADFCopy and the "
               "Amiga floppy mechanism.";

        MotorStalled stalled;
        stalled.reason = "ADFCopy motor " + std::string(on ? "ON" : "OFF")
            + " (ADFC_CMD_INIT) failed: "
            + (result.error_message.empty()
               ? "device returned error response"
               : result.error_message);
        return stalled;
    }

    if (on) {
        MotorRunning running;
        running.measured_rpm = 0.0;  /* RPM not measured (V1 returns constant 300.0 stub) */
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
 *  V1 equivalent: seekCylinder(int) in adfcopyhardwareprovider.cpp.
 *
 *  V1 path:
 *    track = cylinder * 2 + m_currentHead
 *    ADFC_CMD_SEEK (0x02) + track(1) → wait 'O'
 *
 *  The runner uses the stored m_current_head (updated in do_read_raw_flux
 *  and set to 0 for recalibrate). The concept only provides cylinder.
 * ─────────────────────────────────────────────────────────────────────── */

SeekOutcome ADFCopyProviderV2::do_seek(int cylinder)
{
    if (!m_seek_runner) {
        return null_runner_error("head seek");
    }

    if (cylinder < 0 || cylinder > m_max_cylinder) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy seek: cylinder out of range",
            "Cylinder " + std::to_string(cylinder)
            + " is outside the valid range [0, " + std::to_string(m_max_cylinder)
            + "] for this ADFCopy configuration. "
              "ADFCopy uses Amiga track encoding (track = cylinder*2 + head), "
              "giving a track range of 0..159 for standard 80-cylinder Amiga "
              "DD/HD disks. The firmware supports up to cylinder 83 for "
              "over-scan head alignment.",
            "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
            + "]. Verify the attached drive is an Amiga-compatible 3.5\" "
              "mechanism with 80 cylinders (standard DD: DSDD, HD: DSHD). "
              "Cylinders 80-83 are available for alignment margin only."
        };
    }

    SeekRequest req;
    req.cylinder     = cylinder;
    req.current_head = m_current_head;

    ADFCopySeekResult result = m_seek_runner(req);

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy head seek failed: serial transport not available",
            "The ADFCopyProviderV2 seek requires a QSerialPort connection. "
            "The ADFC_CMD_SEEK (0x02) command could not be sent — the serial "
            "transport is unavailable. Requested cylinder: "
            + std::to_string(cylinder) + ".",
            "Connect an ADFCopy/ADF-Drive device and open the serial connection "
            "before calling seek(). Verify the device is powered and responding "
            "to ADFC_CMD_PING (0x00)."
        };
    }

    if (!result.success) {
        std::string reason = "ADFCopy ADFC_CMD_SEEK to cylinder "
            + std::to_string(cylinder)
            + " (track " + std::to_string(cylinder * 2 + m_current_head)
            + ") failed. ";
        if (!result.error_message.empty()) {
            reason += result.error_message;
        }
        return SeekTrack0Failed{reason};
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
 *  V1 equivalent: recalibrate() in adfcopyhardwareprovider.cpp.
 *
 *  V1 path:
 *    m_currentHead = 0
 *    seekCylinder(0) → cmdSeek(0) → ADFC_CMD_SEEK + 0x00 → wait 'O'
 *
 *  The conformance harness requires SeekArrived.cylinder == 0 for
 *  a successful recalibrate. The runner seeks to track 0 (cylinder 0,
 *  head 0) using ADFC_CMD_SEEK with track=0.
 * ─────────────────────────────────────────────────────────────────────── */

SeekOutcome ADFCopyProviderV2::do_recalibrate()
{
    if (!m_recal_runner) {
        return null_runner_error("recalibrate");
    }

    /* Reset head to 0 (mirrors V1 recalibrate() which sets m_currentHead=0) */
    m_current_head = 0;

    ADFCopySeekResult result = m_recal_runner();

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy recalibrate failed: serial transport not available",
            "The ADFCopyProviderV2 recalibrate requires a QSerialPort connection. "
            "The ADFC_CMD_SEEK (0x02) with track=0 command could not be sent — "
            "the serial transport is unavailable.",
            "Connect an ADFCopy/ADF-Drive device and open the serial connection "
            "before calling recalibrate(). Verify the device is powered and "
            "responding to ADFC_CMD_PING (0x00)."
        };
    }

    if (!result.success) {
        std::string reason = "ADFCopy ADFC_CMD_SEEK to track 0 (recalibrate to "
            "cylinder 0, head 0) failed. ";
        if (!result.error_message.empty()) {
            reason += result.error_message;
        }
        SeekTrack0Failed failed;
        failed.reason = reason;
        return failed;
    }

    /* Successful recalibration MUST arrive at cylinder 0 —
     * the conformance harness checks arrived.cylinder == 0 for Recalibrates. */
    SeekArrived arrived;
    arrived.cylinder = 0;
    return arrived;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive() mixin.
 *
 *  V1 equivalent: detectDrive() in adfcopyhardwareprovider.cpp.
 *
 *  V1 path:
 *    ADFC_CMD_GET_STATUS (0x0B) → read 1-byte bitmask
 *    Then emits hardcoded Amiga DD geometry (80 cylinders, 2 heads, 300 RPM).
 *    The firmware version is available from the PING response (stored in
 *    m_fwVersion after connection). The runner should include it.
 *
 *  DriveDetected is always Amiga DD geometry for ADFCopy (the device
 *  specifically targets Amiga 3.5" floppy mechanisms). If the FLUX_CAPABLE
 *  flag is set, the product is "ADF-Drive (FLUX)".
 * ─────────────────────────────────────────────────────────────────────── */

DetectOutcome ADFCopyProviderV2::do_detect_drive()
{
    if (!m_detect_runner) {
        return null_runner_error("drive detection");
    }

    ADFCopyDetectResult result = m_detect_runner();

    if (result.transport_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy drive detection failed: serial transport not available",
            "The ADFCopyProviderV2 drive detection requires a QSerialPort "
            "connection to an ADFCopy device. The ADFC_CMD_GET_STATUS (0x0B) "
            "query could not be sent — the serial transport is unavailable.",
            "Connect an ADFCopy/ADF-Drive device (Teensy-based, PJRC "
            "VID:PID 0x16C0:0x0483) to a USB port and open the serial "
            "connection (115200 8N1 USB-CDC) before calling detect_drive(). "
            "Issue ADFC_CMD_PING (0x00) to confirm the device is responding."
        };
    }

    if (!result.error_message.empty() && !result.disk_present) {
        return ProviderError{
            UFT_E_GENERIC,
            "ADFCopy drive detection failed",
            "The ADFCopy detection runner reported an error: "
            + result.error_message
            + ". The ADFC_CMD_GET_STATUS (0x0B) query may have timed out or "
              "the device returned an error response.",
            "Verify the ADFCopy device is connected and powered. "
            "Issue ADFC_CMD_PING (0x00) to confirm serial communication. "
            "Ensure the device firmware is v1.100 or later (ADF-Copy or "
            "ADF-Drive). If the problem persists, power-cycle the device."
        };
    }

    if (!result.disk_present) {
        /* Clean probe — no disk inserted */
        DriveAbsent absent;
        absent.scanned_for = "ADFCopy/ADF-Drive Amiga floppy controller "
                             "(Teensy-based, PJRC VID:PID 0x16C0:0x0483, "
                             "USB-CDC serial 115200 8N1) — "
                             "ADFC_CMD_GET_STATUS returned "
                             "ADFC_STATUS_DISK_PRESENT=0 (no disk inserted)";
        return absent;
    }

    /* Drive/disk detected — populate DriveDetected */
    DriveDetected detected;

    /* Amiga 3.5" DD geometry — ADFCopy always targets Amiga mechanisms */
    detected.drive_kind = result.flux_capable
                          ? "Amiga 3.5\" DD/HD (MFM, double-sided) via ADF-Drive (FLUX)"
                          : "Amiga 3.5\" DD/HD (MFM, double-sided) via ADF-Copy";
    detected.tracks     = ADFC_STD_CYLINDERS;
    detected.heads      = ADFC_HEADS;
    detected.rpm_nominal = 300.0;  /* Amiga standard: 300 RPM */

    /* Firmware and status info */
    std::string product = result.flux_capable ? "ADF-Drive (FLUX)" : "ADF-Copy";

    if (!result.firmware.empty()) {
        detected.firmware = product + " FW " + result.firmware
                          + " (40 MHz / 25 ns, Teensy USB-CDC)";
    } else {
        detected.firmware = product + " (40 MHz / 25 ns, firmware version unknown, "
                          "Teensy USB-CDC, VID:PID 0x16C0:0x0483)";
    }

    /* Append write-protect note if active */
    if (result.write_protected) {
        detected.firmware += " [WRITE-PROTECTED]";
    }

    return detected;
}

}  // namespace uft::hal
