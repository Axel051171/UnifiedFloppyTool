/**
 * @file greaseweazle_provider_v2.cpp
 * @brief GreaseweazleProviderV2 implementation (MF-154 / P1.1).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps the uft_gw_* C-API into the Type-Driven HAL outcome
 * sum-types. It does NOT rewrite any protocol logic — every actual wire
 * interaction is delegated to the existing, production-tested C backend
 * in src/hal/uft_greaseweazle_full.c.
 *
 * Rule F-3 (multi-revolution preservation):
 *   uft_gw_read_track() / uft_gw_read_flux_simple() capture N complete
 *   revolutions of flux data indexed by uft_gw_flux_data_t::index_times[].
 *   All samples and all index timestamps are copied verbatim into the
 *   FluxCaptured::transitions_ns vector — no averaging, no pruning.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings; this is a runtime guard
 *   that catches programming mistakes during development.
 *
 * C-API contract:
 *   uft_gw_* function signatures are not modified. This file only calls
 *   them. If a function call fails, the error code is translated to a
 *   typed ProviderError and returned as a variant alternative.
 */

#include "greaseweazle_provider_v2.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

namespace uft::hal {

/* ────────────────────────────────────────────────────────────────────────
 *  Construction / lifecycle  (MF-171 P1.18: provider owns the handle)
 * ──────────────────────────────────────────────────────────────────────── */

GreaseweazleProviderV2::GreaseweazleProviderV2(uft_gw_device_t* handle) noexcept
    : m_handle(handle)
{
    /* Try to populate cached info if the caller already opened the
     * device. Failure is non-fatal — getters just stay at default. */
    if (m_handle) {
        uft_gw_info_t info{};
        if (uft_gw_get_info(m_handle, &info) == 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "v%u.%u",
                          static_cast<unsigned>(info.fw_major),
                          static_cast<unsigned>(info.fw_minor));
            m_firmware_version = buf;
            m_hw_model = info.hw_model;
        }
    }
}

GreaseweazleProviderV2::~GreaseweazleProviderV2()
{
    close();
}

bool GreaseweazleProviderV2::open(const char *port_path, std::string *err_out)
{
    if (!port_path || *port_path == '\0') {
        if (err_out) *err_out = "empty port path";
        return false;
    }
    if (m_handle) {
        if (err_out) *err_out = "device already open — call close() first";
        return false;
    }

    uft_gw_device_t *gw = nullptr;
    const int rc = uft_gw_open(port_path, &gw);
    if (rc != 0 || gw == nullptr) {
        if (err_out) {
            const char *msg = uft_gw_strerror(rc);
            *err_out = msg ? msg : "uft_gw_open failed";
        }
        return false;
    }
    m_handle = gw;

    /* Populate cached info — non-fatal if it fails. */
    uft_gw_info_t info{};
    if (uft_gw_get_info(m_handle, &info) == 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "v%u.%u",
                      static_cast<unsigned>(info.fw_major),
                      static_cast<unsigned>(info.fw_minor));
        m_firmware_version = buf;
        m_hw_model = info.hw_model;
    } else {
        m_firmware_version = "(unknown)";
        m_hw_model = 0;
    }
    return true;
}

void GreaseweazleProviderV2::close() noexcept
{
    if (m_handle) {
        uft_gw_close(m_handle);
        m_handle = nullptr;
    }
    m_firmware_version.clear();
    m_hw_model = 0;
}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helper — translate gw error code to ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

ProviderError GreaseweazleProviderV2::gw_err_to_provider_error(
    int gw_rc,
    const char* what_msg,
    const char* why_prefix)
{
    const char* gw_msg = uft_gw_strerror(gw_rc);

    std::string why = why_prefix;
    why += ": ";
    why += (gw_msg ? gw_msg : "unknown error");
    why += " (code ";
    why += std::to_string(gw_rc);
    why += ")";

    /* Classify the fix hint based on the error code. */
    std::string fix;
    switch (gw_rc) {
    case UFT_GW_ERR_NOT_CONNECTED:
        fix = "Check USB cable and re-connect the Greaseweazle device.";
        break;
    case UFT_GW_ERR_NO_INDEX:
        fix = "Ensure a floppy disk is inserted and the drive motor is running; "
              "check index hole sensor and drive belt.";
        break;
    case UFT_GW_ERR_NO_TRK0:
        fix = "Drive did not find track 0. Check stepper motor and track-0 sensor. "
              "Try recalibrating or power-cycling the drive.";
        break;
    case UFT_GW_ERR_OVERFLOW:
        fix = "Flux buffer overflowed. Reduce the number of requested revolutions "
              "or increase USB transfer bandwidth.";
        break;
    case UFT_GW_ERR_UNDERFLOW:
        fix = "Flux buffer underflowed during write. Ensure the source flux stream "
              "is complete and retry the write operation.";
        break;
    case UFT_GW_ERR_WRPROT:
        fix = "Disk is write-protected. Remove the write-protect tab and retry.";
        break;
    case UFT_GW_ERR_BOOTLOADER:
        fix = "Device is in bootloader mode. Disconnect, hold the drive-ID button, "
              "re-plug, and allow the main firmware to start.";
        break;
    case UFT_GW_ERR_TIMEOUT:
        fix = "Operation timed out. Check USB connection and that the drive "
              "is powered and spinning.";
        break;
    default:
        fix = "Check USB connection, power supply, and that a compatible "
              "floppy disk is inserted. Consult the Greaseweazle documentation.";
        break;
    }

    return ProviderError{
        UFT_ERR_HARDWARE,
        std::string(what_msg),
        std::move(why),
        std::move(fix)
    };
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_raw_flux
 *
 *  Maps to: ReadsRawFlux concept / read_raw_flux(ReadFluxParams) mixin.
 *
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions).
 *
 *  Rule F-3: uft_gw_read_track() returns a uft_gw_flux_data_t that
 *  carries sample_count flux timing samples and index_count index-to-index
 *  timestamps. ALL samples are converted to nanoseconds and stored in
 *  FluxCaptured::transitions_ns. The index timestamps are converted
 *  ticks→ns and stored in FluxCaptured::index_times_ns — they are NOT
 *  discarded (MF-194: this previously claimed "appended after the sample
 *  data", which the code never did — they now have a real typed home).
 *
 *  The revolutions parameter from ReadFluxParams maps to
 *  uft_gw_read_track()'s revolutions argument exactly.
 * ──────────────────────────────────────────────────────────────────────── */

FluxOutcome GreaseweazleProviderV2::do_read_raw_flux(const ReadFluxParams& p)
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    const uint8_t cyl  = static_cast<uint8_t>(p.cylinder);
    const uint8_t head = static_cast<uint8_t>(p.head);
    const uint8_t revs = static_cast<uint8_t>(p.revolutions > 0 ? p.revolutions : 2);

    uft_gw_flux_data_t* flux = nullptr;
    int rc = uft_gw_read_track(m_handle, cyl, head, revs, &flux);

    if (rc != UFT_GW_OK || flux == nullptr) {
        if (flux) uft_gw_flux_free(flux);
        /* Distinguish physically-unreadable from disconnection errors. */
        if (rc == UFT_GW_ERR_NO_INDEX) {
            return FluxUnreadable{
                CHS{p.cylinder, p.head},
                "No index pulse detected — disk may be absent, motor not running, "
                "or index hole sensor faulty"
            };
        }
        if (rc == UFT_GW_ERR_NOT_CONNECTED) {
            return HardwareDisconnected{
                "greaseweazle",
                "Lost connection during flux read"
            };
        }
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle flux read failed",
            "uft_gw_read_track returned error");
    }

    /* Obtain sample frequency from the device for unit conversion. */
    uint32_t sample_freq = flux->sample_freq;
    if (sample_freq == 0) {
        sample_freq = uft_gw_get_sample_freq(m_handle);
    }
    if (sample_freq == 0) {
        sample_freq = UFT_GW_SAMPLE_FREQ_HZ;  /* Fallback: 72 MHz */
    }

    const double sample_ns = (sample_freq > 0)
        ? (1.0e9 / static_cast<double>(sample_freq))
        : (1.0e9 / 72000000.0);

    /* Rule F-3: copy ALL flux timing samples verbatim (ticks → ns). */
    FluxCaptured captured;
    captured.position  = CHS{p.cylinder, p.head};
    captured.revolutions = static_cast<int>(flux->index_count > 0
                                            ? flux->index_count
                                            : revs);
    captured.sample_ns = sample_ns;
    captured.quality   = QualityFlag::None;

    if (flux->samples && flux->sample_count > 0) {
        captured.transitions_ns.reserve(flux->sample_count);
        for (uint32_t i = 0; i < flux->sample_count; ++i) {
            /* Convert ticks to nanoseconds using the inline helper from
             * uft_greaseweazle_full.h — no loss of information. */
            captured.transitions_ns.push_back(
                uft_gw_ticks_to_ns(flux->samples[i], sample_freq));
        }
    }

    /* Rule F-3: preserve the per-revolution index boundaries the
     * firmware reported. uft_gw_flux_data_t::index_times[] holds
     * index_count cumulative-tick timestamps; convert each to ns with
     * the SAME scale already applied to the samples and store them in
     * FluxCaptured::index_times_ns. Do NOT synthesise entries — if the
     * firmware reported none, leave the vector empty (an honest
     * "boundaries unknown", per the FluxCaptured contract). */
    if (flux->index_times && flux->index_count > 0) {
        captured.index_times_ns.reserve(flux->index_count);
        for (uint8_t r = 0; r < flux->index_count; ++r) {
            captured.index_times_ns.push_back(
                uft_gw_ticks_to_ns(flux->index_times[r], sample_freq));
        }
    }

    uft_gw_flux_free(flux);

    /* Validate: if we captured samples but none appeared, report marginal. */
    if (captured.transitions_ns.empty()) {
        return FluxMarginal{
            CHS{p.cylinder, p.head},
            {},
            "Flux capture returned no transitions despite a successful read ACK"
        };
    }

    return captured;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_write_raw_flux
 *
 *  Maps to: WritesRawFlux concept / write_raw_flux(WriteFluxParams, FluxStream).
 *
 *  V1 equivalent: writeRawFlux(cylinder, head, fluxData) plus the
 *  encode-then-write path in writeTrack().
 *
 *  FluxStream::transitions_ns carries timing in nanoseconds. Convert to
 *  device ticks before calling uft_gw_write_track().
 *
 *  Write-protect detection: uft_gw_is_write_protected() is checked
 *  before writing; if true, WriteRefused is returned immediately so the
 *  caller can surface a forensic-correct message without attempting the
 *  write.
 * ──────────────────────────────────────────────────────────────────────── */

WriteOutcome GreaseweazleProviderV2::do_write_raw_flux(
    const WriteFluxParams& w, const FluxStream& flux)
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    const uint8_t cyl  = static_cast<uint8_t>(w.cylinder);
    const uint8_t head = static_cast<uint8_t>(w.head);

    /* Check write-protect pin before touching the media. */
    if (uft_gw_is_write_protected(m_handle)) {
        return WriteRefused{
            CHS{w.cylinder, w.head},
            "Disk is write-protected (write-protect notch/tab is engaged)"
        };
    }

    if (flux.transitions_ns.empty()) {
        return ProviderError{
            UFT_ERR_INVALID_ARG,
            "Empty flux stream passed to write_raw_flux",
            "The FluxStream::transitions_ns vector is empty; there is no data to write.",
            "Provide a non-empty flux stream with at least one transition interval."
        };
    }

    /* Obtain sample frequency for nanosecond → tick conversion. */
    uint32_t sample_freq = uft_gw_get_sample_freq(m_handle);
    if (sample_freq == 0) {
        sample_freq = UFT_GW_SAMPLE_FREQ_HZ;
    }

    /* Convert ns → ticks. */
    std::vector<uint32_t> ticks;
    ticks.reserve(flux.transitions_ns.size());
    for (uint32_t ns : flux.transitions_ns) {
        ticks.push_back(uft_gw_ns_to_ticks(ns, sample_freq));
    }

    int rc = uft_gw_write_track(
        m_handle,
        cyl, head,
        ticks.data(),
        static_cast<uint32_t>(ticks.size()));

    if (rc == UFT_GW_ERR_WRPROT) {
        return WriteRefused{
            CHS{w.cylinder, w.head},
            "Write rejected by firmware: disk is write-protected"
        };
    }
    if (rc == UFT_GW_ERR_NOT_CONNECTED) {
        return HardwareDisconnected{
            "greaseweazle",
            "Lost connection during flux write"
        };
    }
    if (rc != UFT_GW_OK) {
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle flux write failed",
            "uft_gw_write_track returned error");
    }

    WriteCompleted done;
    done.position      = CHS{w.cylinder, w.head};
    done.bytes_written = ticks.size() * sizeof(uint32_t);
    done.verified      = false;  /* GW writes in flux mode; read-back verify is
                                  * handled by the caller via a subsequent read. */
    done.quality       = QualityFlag::None;
    return done;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_set_motor
 *
 *  Maps to: ControlsMotor concept / set_motor(bool).
 *
 *  V1 equivalent: setMotor(bool on).
 *
 *  uft_gw_set_motor() handles the CMD_MOTOR protocol and the 500 ms
 *  spin-up wait internally in the C backend.
 * ──────────────────────────────────────────────────────────────────────── */

MotorOutcome GreaseweazleProviderV2::do_set_motor(bool on)
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    int rc = uft_gw_set_motor(m_handle, on);
    if (rc != UFT_GW_OK) {
        return gw_err_to_provider_error(
            rc,
            on ? "Failed to turn motor ON" : "Failed to turn motor OFF",
            "uft_gw_set_motor returned error");
    }

    return on ? MotorOutcome{MotorRunning{0.0}} : MotorOutcome{MotorStopped{}};
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_seek
 *
 *  Maps to: SeeksHead concept / seek(int cylinder).
 *
 *  V1 equivalent: seekCylinder(int cylinder) + selectHead(m_currentHead).
 *
 *  The V1 code sent CMD_SEEK followed by CMD_HEAD. uft_gw_seek() in the
 *  C backend issues the same sequence (seek to cylinder, then re-assert
 *  the currently-selected head). The head selection is implicit in the
 *  device state; this V2 wraps the cylinder-seek only, as the concept
 *  requires `seek(int cylinder) -> SeekOutcome`.
 *
 *  UFT_GW_ACK_NO_TRK0 maps to SeekTrack0Failed for forensic accuracy.
 * ──────────────────────────────────────────────────────────────────────── */

SeekOutcome GreaseweazleProviderV2::do_seek(int cylinder)
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    if (cylinder < 0 || cylinder > UFT_GW_MAX_CYLINDERS) {
        return ProviderError{
            UFT_ERR_INVALID_ARG,
            "Cylinder number out of range",
            std::string("Requested cylinder ") + std::to_string(cylinder) +
                " is outside the valid range [0, " +
                std::to_string(UFT_GW_MAX_CYLINDERS) + "].",
            "Pass a cylinder value between 0 and " +
                std::to_string(UFT_GW_MAX_CYLINDERS) + "."
        };
    }

    int rc = uft_gw_seek(m_handle, static_cast<uint8_t>(cylinder));

    if (rc == UFT_GW_ERR_NO_TRK0) {
        return SeekTrack0Failed{
            "Greaseweazle reported no track-0 signal during seek to cylinder " +
            std::to_string(cylinder) +
            ". Drive stepper or track-0 sensor may be faulty."
        };
    }
    if (rc == UFT_GW_ERR_NOT_CONNECTED) {
        return HardwareDisconnected{
            "greaseweazle",
            "Lost connection during seek"
        };
    }
    if (rc != UFT_GW_OK) {
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle seek failed",
            "uft_gw_seek returned error");
    }

    return SeekArrived{cylinder};
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_recalibrate
 *
 *  Maps to: Recalibrates concept / recalibrate().
 *
 *  V1 equivalent: recalibrate() → seekCylinder(0).
 *
 *  uft_gw_recalibrate() in the C backend seeks to track 0 and verifies
 *  the track-0 sensor. SeekTrack0Failed is returned when the sensor
 *  does not assert, which is the forensically-correct diagnosis.
 * ──────────────────────────────────────────────────────────────────────── */

SeekOutcome GreaseweazleProviderV2::do_recalibrate()
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    int rc = uft_gw_recalibrate(m_handle);

    if (rc == UFT_GW_ERR_NO_TRK0) {
        return SeekTrack0Failed{
            "Recalibration failed: track-0 sensor did not assert after stepping. "
            "Drive stepper motor or track-0 sensor may require service."
        };
    }
    if (rc == UFT_GW_ERR_NOT_CONNECTED) {
        return HardwareDisconnected{
            "greaseweazle",
            "Lost connection during recalibration"
        };
    }
    if (rc != UFT_GW_OK) {
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle recalibration failed",
            "uft_gw_recalibrate returned error");
    }

    return SeekArrived{0};
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_measure_rpm
 *
 *  Maps to: MeasuresRPM concept / measure_rpm().
 *
 *  V1 equivalent: measureRPM() — reads 3 index pulses (revs=3) via a
 *  short READ_FLUX capture and computes RPM from the average index-to-
 *  index interval.
 *
 *  uft_gw_read_flux_simple(revs=3) + uft_gw_get_index_times() replicates
 *  the V1 logic without duplicating it here. The C backend owns the
 *  protocol logic.
 *
 *  jitter_pct: computed from the spread of index intervals. If only one
 *  interval is available, jitter is reported as 0.0 (not determinable).
 * ──────────────────────────────────────────────────────────────────────── */

RpmOutcome GreaseweazleProviderV2::do_measure_rpm()
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    /* Capture 3 index pulses — enough for 2 revolution intervals. */
    uft_gw_flux_data_t* flux = nullptr;
    int rc = uft_gw_read_flux_simple(m_handle, 3, &flux);

    if (rc != UFT_GW_OK || flux == nullptr) {
        if (flux) uft_gw_flux_free(flux);
        if (rc == UFT_GW_ERR_NO_INDEX) {
            return ProviderError{
                UFT_ERR_HARDWARE,
                "RPM measurement failed: no index pulse detected",
                "The drive produced no index pulse during the measurement capture window. "
                "The motor may not be running or no disk is inserted.",
                "Turn the motor on before measuring RPM and ensure a disk is in the drive."
            };
        }
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle RPM measurement failed",
            "uft_gw_read_flux_simple returned error");
    }

    /* Retrieve index-to-index intervals. */
    uint32_t index_times[UFT_GW_MAX_REVOLUTIONS + 1] = {};
    int index_count = uft_gw_get_index_times(
        m_handle, index_times,
        static_cast<int>(UFT_GW_MAX_REVOLUTIONS + 1));

    uft_gw_flux_free(flux);

    uint32_t sample_freq = uft_gw_get_sample_freq(m_handle);
    if (sample_freq == 0) sample_freq = UFT_GW_SAMPLE_FREQ_HZ;

    if (index_count < 1) {
        return ProviderError{
            UFT_ERR_HARDWARE,
            "RPM measurement produced no index intervals",
            "The flux capture succeeded but uft_gw_get_index_times() returned "
            "zero index-to-index timestamps. The drive may have a faulty index sensor.",
            "Inspect the drive's index hole sensor and belt. Re-seat the disk."
        };
    }

    /* Compute average RPM from all available intervals. */
    uint64_t total_ticks = 0;
    uint32_t min_ticks   = index_times[0];
    uint32_t max_ticks   = index_times[0];

    for (int i = 0; i < index_count; ++i) {
        total_ticks += index_times[i];
        if (index_times[i] < min_ticks) min_ticks = index_times[i];
        if (index_times[i] > max_ticks) max_ticks = index_times[i];
    }

    double avg_ticks = static_cast<double>(total_ticks) / index_count;
    double rpm       = (avg_ticks > 0.0)
        ? (60.0 * static_cast<double>(sample_freq)) / avg_ticks
        : 0.0;

    /* Jitter: (max - min) / avg as a percentage. */
    double jitter_pct = (avg_ticks > 0.0 && index_count >= 2)
        ? (static_cast<double>(max_ticks - min_ticks) / avg_ticks) * 100.0
        : 0.0;

    return RpmMeasured{rpm, jitter_pct, index_count};
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: detectDrive() — sends CMD_GET_INFO to read firmware info,
 *  then calls measureRPM() to determine drive type from RPM.
 *
 *  uft_gw_get_info() + a single measure_rpm() call replicate the V1 logic.
 *  The firmware info is used to build drive_kind; rpm_nominal is set from
 *  the measured RPM.
 *
 *  Heuristic: 250–350 RPM → 3.5" HD (300 RPM nominal),
 *             350–420 RPM → 5.25" HD (360 RPM nominal).
 *             Outside range → "Unknown" but still reported as DriveDetected
 *             (the drive IS there — the RPM is just unexpected).
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome GreaseweazleProviderV2::do_detect_drive()
{
    if (!m_handle || !uft_gw_is_connected(m_handle)) {
        return HardwareDisconnected{
            "greaseweazle",
            "Device handle is null or device is not connected"
        };
    }

    uft_gw_info_t info{};
    int rc = uft_gw_get_info(m_handle, &info);
    if (rc != UFT_GW_OK) {
        if (rc == UFT_GW_ERR_BOOTLOADER) {
            return ProviderError{
                UFT_ERR_HARDWARE,
                "Greaseweazle is in bootloader mode",
                "The device responded with a bootloader-mode signature. "
                "Main firmware is not running; no drive detection is possible.",
                "Disconnect the device, hold the drive-ID button down, re-plug, "
                "and wait for the main firmware to start (see docs/HARDWARE.md)."
            };
        }
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle device info query failed",
            "uft_gw_get_info returned error");
    }

    /* Build firmware string, e.g. "v1.4 (F7)" */
    std::string fw_str = "v";
    fw_str += std::to_string(info.fw_major);
    fw_str += ".";
    fw_str += std::to_string(info.fw_minor);
    if (info.hw_model == 7)       fw_str += " (F7)";
    else if (info.hw_model >= 4)  fw_str += " (V" + std::to_string(info.hw_model) + ".x)";
    else                          fw_str += " (F" + std::to_string(info.hw_model) + ")";

    /* Measure RPM to detect drive type. */
    double rpm = 0.0;
    {
        RpmOutcome rpm_outcome = do_measure_rpm();
        std::visit(overloaded{
            [&](const RpmMeasured& r)            { rpm = r.rpm;              },
            [&](const CapabilityRequiresPolicy&) { /* ignore — use 0 */      },
            [&](const HardwareDisconnected&)     { /* handled by caller */   },
            [&](const ProviderError&)            { /* no disk or no index */ },
        }, rpm_outcome);
    }

    /* Classify drive type from RPM (same heuristic as V1). */
    std::string drive_kind;
    double rpm_nominal = 300.0;
    if (rpm > 250.0 && rpm < 350.0) {
        drive_kind  = "3.5\" HD";
        rpm_nominal = 300.0;
    } else if (rpm > 350.0 && rpm < 420.0) {
        drive_kind  = "5.25\" HD";
        rpm_nominal = 360.0;
    } else if (rpm > 0.0) {
        drive_kind  = "Unknown (measured " + std::to_string(static_cast<int>(rpm + 0.5)) + " RPM)";
        rpm_nominal = rpm;
    } else {
        drive_kind  = "Unknown (no RPM signal)";
        rpm_nominal = 0.0;
    }

    DriveDetected detected;
    detected.drive_kind  = drive_kind;
    detected.tracks      = 80;
    detected.heads       = 2;
    detected.rpm_nominal = rpm_nominal;
    detected.firmware    = fw_str;

    return detected;
}

}  // namespace uft::hal
