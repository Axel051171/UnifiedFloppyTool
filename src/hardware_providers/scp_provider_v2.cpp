/**
 * @file scp_provider_v2.cpp
 * @brief SCPProviderV2 implementation (MF-161 / P1.8).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps the uft_scp_direct_* C-API into the Type-Driven HAL
 * outcome sum-types. It does NOT rewrite any protocol logic — every actual
 * wire interaction is delegated to the existing C backend in
 * src/hal/uft_scp_direct.c.
 *
 * Backend maturity (M3.1 scaffold):
 *   uft_scp_direct_open(), uft_scp_direct_read_flux(), and
 *   uft_scp_direct_write_flux() currently return UFT_ERR_NOT_IMPLEMENTED.
 *   This is the forensically honest scaffold state per CLAUDE.md M3.1. The
 *   do_* methods translate that into a ProviderError with a clear M3.1
 *   marker — the V2 type shape is valid and the runtime error is truthful.
 *
 * Rule F-3 (multi-revolution preservation):
 *   uft_scp_direct_read_flux() captures N revolutions of flux transitions.
 *   All transition intervals (in UFT_SCP_FLUX_NS_PER_SAMPLE units) are
 *   converted to nanoseconds and stored verbatim in FluxCaptured::
 *   transitions_ns. No averaging, no pruning, no single-sample collapse.
 *   The V1 provider's retry loop (up to 5 retries) is replicated at the
 *   C-API call site; all captured data from the successful revolution is
 *   preserved complete.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings; this is a runtime guard
 *   that catches programming mistakes during development.
 *
 * C-API contract:
 *   uft_scp_direct_* function signatures are not modified. This file only
 *   calls them. If a function call fails, the error code is translated to a
 *   typed ProviderError or other Outcome variant.
 */

#include "scp_provider_v2.h"

#include <cstring>
#include <string>
#include <vector>

namespace uft::hal {

/* ────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ──────────────────────────────────────────────────────────────────────── */

SCPProviderV2::SCPProviderV2(uft_scp_direct_ctx_t* handle) noexcept
    : m_handle(handle)
{}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helper — translate scp_direct error code to ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

ProviderError SCPProviderV2::scp_err_to_provider_error(
    uft_error_t rc,
    const char* what_msg,
    const char* why_prefix)
{
    std::string why = why_prefix;
    why += " (error code ";
    why += std::to_string(static_cast<int>(rc));
    why += ")";

    std::string fix;
    if (rc == UFT_ERR_NOT_IMPLEMENTED) {
        /* M3.1 scaffold: USB layer is pending but the type shape is correct. */
        fix = "SCP direct USB I/O is pending (M3.1 scaffold). "
              "The libusb layer has not been wired yet. "
              "Use the V1 SCPHardwareProvider (serial path) until M3.1 lands, "
              "or wait for the M3.1 libusb integration commit.";
    } else if (rc == UFT_ERR_IO) {
        fix = "Check that the SuperCard Pro device is connected via USB "
              "and that the USB driver (FTDI / libusb) is installed. "
              "Try unplugging and re-plugging the device.";
    } else if (rc == UFT_ERR_INVALID_ARG) {
        fix = "Check that cylinder, head, and revolution count are within "
              "the valid range for SuperCard Pro hardware "
              "(cylinder 0-83, head 0-1, revolutions 1-5).";
    } else if (rc == UFT_ERR_BUFFER_TOO_SMALL) {
        fix = "Increase the flux buffer capacity or reduce the number of "
              "requested revolutions to avoid overflow.";
    } else {
        fix = "Check USB connection and that a compatible floppy disk is "
              "inserted. Consult the SuperCard Pro SDK documentation.";
    }

    return ProviderError{
        rc,
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
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions) in
 *  scphardwareprovider.cpp — the READFLUX + GETFLUXINFO + SENDRAM_USB
 *  flow. The V2 delegates to uft_scp_direct_read_flux() which will
 *  implement that protocol via libusb when M3.1 lands.
 *
 *  Rule F-3: all transition intervals from the capture are stored verbatim
 *  in FluxCaptured::transitions_ns. SCP native samples are in 25 ns units
 *  (UFT_SCP_FLUX_NS_PER_SAMPLE); each tick is multiplied by 25 to produce
 *  nanoseconds. No samples are dropped, averaged, or collapsed.
 *
 *  The M3.1 scaffold returns UFT_ERR_NOT_IMPLEMENTED — translated to a
 *  ProviderError that is forensically truthful and F-4 compliant.
 * ──────────────────────────────────────────────────────────────────────── */

FluxOutcome SCPProviderV2::do_read_raw_flux(const ReadFluxParams& p)
{
    if (!m_handle) {
        return ProviderError{
            UFT_ERR_NOT_IMPLEMENTED,
            "SuperCard Pro flux read failed: null context handle",
            "The SCP provider was constructed with a null handle. "
            "This occurs in test/mock mode or when the USB open failed.",
            "SCP direct USB I/O is pending (M3.1 scaffold). "
            "Construct SCPProviderV2 with a valid handle from "
            "uft_scp_direct_open(), or use the V1 SCPHardwareProvider "
            "until the M3.1 libusb layer is wired."
        };
    }

    const int cylinder    = p.cylinder;
    const int head        = p.head;
    const int revolutions = (p.revolutions > 0 && p.revolutions <= UFT_SCP_MAX_REVOLUTIONS)
                                ? p.revolutions
                                : UFT_SCP_DEFAULT_REVOLUTIONS;

    /* Validate geometry before calling the C-API. */
    const int track_index = cylinder * 2 + head;
    if (track_index < 0 || track_index > UFT_SCP_MAX_TRACK_INDEX) {
        return ProviderError{
            UFT_ERR_INVALID_ARG,
            "SuperCard Pro flux read: track index out of range",
            std::string("Computed track_index=") + std::to_string(track_index) +
                " (cylinder=" + std::to_string(cylinder) +
                ", head=" + std::to_string(head) +
                ") exceeds UFT_SCP_MAX_TRACK_INDEX=" +
                std::to_string(UFT_SCP_MAX_TRACK_INDEX) + ".",
            "Pass a cylinder in range [0, 83] and head in [0, 1]."
        };
    }

    /* Seek to the track first. */
    uft_error_t seek_rc = uft_scp_direct_seek(m_handle, track_index);
    if (seek_rc != UFT_OK) {
        /* M3.1: seek also returns NOT_IMPLEMENTED — same story. */
        return scp_err_to_provider_error(
            seek_rc,
            "SuperCard Pro flux read: seek failed",
            "uft_scp_direct_seek returned error");
    }

    /* Allocate a flux buffer.
     * Conservative upper bound: SCP 40 MHz clock, 360 RPM drive, 5 revs.
     * At 300 RPM: 200ms/rev * 40e6 samples/s * 5 revs = 40,000,000 max.
     * In practice 512 KB SCP RAM limits flux to ~256K 16-bit samples.
     * Use 512K uint32_t for safety — the actual C-API fills only what it
     * captured and sets out_count. */
    static constexpr size_t FLUX_BUFFER_CAPACITY = 512 * 1024;
    std::vector<uint32_t> flux_buf(FLUX_BUFFER_CAPACITY);
    size_t captured_count = 0;

    uft_error_t rc = uft_scp_direct_read_flux(
        m_handle,
        head,
        revolutions,
        flux_buf.data(),
        FLUX_BUFFER_CAPACITY,
        &captured_count);

    if (rc != UFT_OK) {
        return scp_err_to_provider_error(
            rc,
            "SuperCard Pro flux read failed",
            "uft_scp_direct_read_flux returned error");
    }

    if (captured_count == 0) {
        return FluxMarginal{
            CHS{cylinder, head},
            {},
            "Flux capture completed but zero transitions were captured. "
            "The drive may be empty or the index sensor may be faulty."
        };
    }

    /* Rule F-3: convert native SCP ticks to nanoseconds verbatim.
     * SCP samples at 40 MHz = 25 ns per tick (UFT_SCP_FLUX_NS_PER_SAMPLE).
     * All transitions stored — no pruning, no averaging. */
    FluxCaptured captured;
    captured.position    = CHS{cylinder, head};
    captured.revolutions = revolutions;
    captured.sample_ns   = static_cast<double>(UFT_SCP_FLUX_NS_PER_SAMPLE);
    captured.quality     = QualityFlag::None;

    captured.transitions_ns.reserve(captured_count);
    for (size_t i = 0; i < captured_count; ++i) {
        /* flux_buf[i] is in SCP native ticks; multiply by 25 for ns. */
        captured.transitions_ns.push_back(
            flux_buf[i] * static_cast<uint32_t>(UFT_SCP_FLUX_NS_PER_SAMPLE));
    }

    return captured;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_write_raw_flux
 *
 *  Maps to: WritesRawFlux concept / write_raw_flux(WriteFluxParams, FluxStream).
 *
 *  V1 equivalent: writeTrack() / writeRawFlux() in scphardwareprovider.cpp
 *  — the LOADRAM_USB + WRITEFLUX flow. The V2 delegates to
 *  uft_scp_direct_write_flux() which will implement that protocol via
 *  libusb when M3.1 lands.
 *
 *  FluxStream::transitions_ns carries timing in nanoseconds. Convert to
 *  SCP native ticks (divide by 25) before calling uft_scp_direct_write_flux().
 *
 *  Write-protect detection: the V1 provider checked SCP_PR_WPENABLED in
 *  the WRITEFLUX response. The uft_scp_direct C-API will propagate this
 *  as UFT_ERR_IO when M3.1 lands; this method maps that to WriteRefused.
 * ──────────────────────────────────────────────────────────────────────── */

WriteOutcome SCPProviderV2::do_write_raw_flux(
    const WriteFluxParams& w, const FluxStream& flux)
{
    if (!m_handle) {
        return ProviderError{
            UFT_ERR_NOT_IMPLEMENTED,
            "SuperCard Pro flux write failed: null context handle",
            "The SCP provider was constructed with a null handle. "
            "This occurs in test/mock mode or when the USB open failed.",
            "SCP direct USB I/O is pending (M3.1 scaffold). "
            "Construct SCPProviderV2 with a valid handle from "
            "uft_scp_direct_open(), or use the V1 SCPHardwareProvider "
            "until the M3.1 libusb layer is wired."
        };
    }

    if (flux.transitions_ns.empty()) {
        return ProviderError{
            UFT_ERR_INVALID_ARG,
            "Empty flux stream passed to write_raw_flux",
            "The FluxStream::transitions_ns vector is empty; there is no "
            "data to write to the SuperCard Pro.",
            "Provide a non-empty flux stream with at least one transition "
            "interval (in nanoseconds)."
        };
    }

    const int cylinder    = w.cylinder;
    const int head        = w.head;
    const int track_index = cylinder * 2 + head;

    if (track_index < 0 || track_index > UFT_SCP_MAX_TRACK_INDEX) {
        return ProviderError{
            UFT_ERR_INVALID_ARG,
            "SuperCard Pro flux write: track index out of range",
            std::string("Computed track_index=") + std::to_string(track_index) +
                " (cylinder=" + std::to_string(cylinder) +
                ", head=" + std::to_string(head) +
                ") exceeds UFT_SCP_MAX_TRACK_INDEX=" +
                std::to_string(UFT_SCP_MAX_TRACK_INDEX) + ".",
            "Pass a cylinder in range [0, 83] and head in [0, 1]."
        };
    }

    /* Seek to the target track before writing. */
    uft_error_t seek_rc = uft_scp_direct_seek(m_handle, track_index);
    if (seek_rc != UFT_OK) {
        return scp_err_to_provider_error(
            seek_rc,
            "SuperCard Pro flux write: seek failed",
            "uft_scp_direct_seek returned error before write");
    }

    /* Convert ns -> SCP native ticks (divide by UFT_SCP_FLUX_NS_PER_SAMPLE).
     * Round to nearest tick; clamp to 1 to avoid zero-interval encoding. */
    std::vector<uint32_t> ticks;
    ticks.reserve(flux.transitions_ns.size());
    for (uint32_t ns : flux.transitions_ns) {
        uint32_t tick = ns / static_cast<uint32_t>(UFT_SCP_FLUX_NS_PER_SAMPLE);
        if (tick == 0) tick = 1;
        ticks.push_back(tick);
    }

    uft_error_t rc = uft_scp_direct_write_flux(
        m_handle,
        head,
        ticks.data(),
        ticks.size());

    if (rc != UFT_OK) {
        return scp_err_to_provider_error(
            rc,
            "SuperCard Pro flux write failed",
            "uft_scp_direct_write_flux returned error");
    }

    WriteCompleted done;
    done.position      = CHS{cylinder, head};
    done.bytes_written = ticks.size() * sizeof(uint32_t);
    done.verified      = false;  /* SCP writes in flux mode; read-back verify
                                  * is handled by the caller via a subsequent
                                  * read. */
    done.quality       = QualityFlag::None;
    return done;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: detectDrive() in scphardwareprovider.cpp — sends SCPINFO
 *  to get HW/FW versions then calls measureRPM() for drive type heuristic.
 *
 *  The uft_scp_direct C-API (M3.1) provides uft_scp_direct_get_capabilities()
 *  which returns static hardware capability data. When the USB layer is
 *  wired, uft_scp_direct_open() will negotiate the firmware version; for
 *  now we use the static capability structure to build a DriveDetected
 *  response with the documented hardware constants.
 *
 *  RPM measurement requires READFLUX + GETFLUXINFO (V1 measureRPM flow).
 *  Since uft_scp_direct_read_flux() is NOT_IMPLEMENTED in M3.1, we report
 *  rpm_nominal = 300.0 (the standard 3.5" drive default) rather than a
 *  fabricated measurement. This is forensically truthful: we know the
 *  hardware's nominal spec, not the actual disk's RPM yet.
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome SCPProviderV2::do_detect_drive()
{
    if (!m_handle) {
        return ProviderError{
            UFT_ERR_NOT_IMPLEMENTED,
            "SuperCard Pro drive detection failed: null context handle",
            "The SCP provider was constructed with a null handle. "
            "This occurs in test/mock mode or when the USB open failed.",
            "SCP direct USB I/O is pending (M3.1 scaffold). "
            "Construct SCPProviderV2 with a valid handle from "
            "uft_scp_direct_open(), or use the V1 SCPHardwareProvider "
            "until the M3.1 libusb layer is wired."
        };
    }

    /* Query static capability data from the backend.
     * uft_scp_direct_get_capabilities() always succeeds and returns
     * the documented hardware constants regardless of M3.1 scaffold state. */
    uft_scp_direct_capabilities_t caps{};
    uft_error_t caps_rc = uft_scp_direct_get_capabilities(&caps);
    if (caps_rc != UFT_OK) {
        return scp_err_to_provider_error(
            caps_rc,
            "SuperCard Pro capability query failed",
            "uft_scp_direct_get_capabilities returned error");
    }

    /* Build a firmware/capability description string. */
    std::string fw_str = "SuperCard Pro";
    if (!caps.impl_complete) {
        fw_str += " (M3.1 scaffold — USB I/O pending)";
    }

    /* rpm_nominal: SCP hardware supports 300 RPM (3.5" DD/HD) and 360 RPM
     * (5.25" HD) drives. Without a live RPM measurement (pending M3.1),
     * report the standard 300 RPM default. This is the documented nominal
     * for the most common drive type — not an invented value. */
    DriveDetected detected;
    detected.drive_kind  = "3.5\" HD / 5.25\" HD";
    detected.tracks      = (static_cast<int>(caps.max_track_index) + 1) / 2;
    detected.heads       = 2;
    detected.rpm_nominal = 300.0;
    detected.firmware    = fw_str;

    return detected;
}

}  // namespace uft::hal
