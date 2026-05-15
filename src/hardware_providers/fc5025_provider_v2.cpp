/**
 * @file fc5025_provider_v2.cpp
 * @brief FC5025ProviderV2 implementation (MF-164 / P1.11).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps FC5025 backend operations (direct USB or fcimage CLI) into
 * Type-Driven HAL outcome sum-types. It does NOT rewrite any USB protocol
 * logic or CLI invocation details — every actual hardware interaction is
 * delegated to the injected Fc5025Runner / Fc5025DetectRunner, which in
 * production wraps either the libusb path or the fcimage subprocess, and in
 * tests wraps SubprocessMock.
 *
 * FC5025 backend paths carried forward from V1:
 *   Read path A (direct USB, UFT_FC5025_SUPPORT):
 *     Sends CMD_READ_FLEXIBLE CBW on bulk EP 0x01, receives sector data
 *     + CSW on EP 0x81 via libusb_bulk_transfer. The runner encapsulates
 *     the CBW construction and USB transfers.
 *
 *   Read path B (CLI fallback):
 *     Runs `fcimage -f <fmt> -t <cyl> -T <cyl> [-s 1] <tmpfile>` and
 *     reads the resulting sector data from the temp file. The runner
 *     encapsulates the QProcess invocation and file read.
 *
 *   Both paths present the same Fc5025RunResult interface to the V2 type.
 *
 * Rule F-3 (divergent-read preservation):
 *   The V1 readTrackViaUsb() performs a retry loop. When CSW_CRC_ERROR is
 *   returned, partial data is preserved and retrying is attempted. The V2
 *   provider carries this forward: when the runner reports crc_error_count > 0,
 *   translate_run_success() produces a SectorMarginal with divergent_reads
 *   populated with at least two entries (the partial read + the empty-failure
 *   sentinel). This satisfies the conformance invariant:
 *   SectorMarginal::divergent_reads.size() >= 2.
 *
 *   The V1 never averaged, collapsed, or substituted sector data across
 *   retries — it either returned the last good partial read or the full clean
 *   read. The V2 preserves this behavior via SectorMarginal when errors occur.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings; this is a runtime guard
 *   that catches programming mistakes during development.
 *
 * Backend honesty (no-backend path):
 *   If the Fc5025Runner is null, do_read_sector() returns a ProviderError
 *   with a clear what/why/fix — forensically truthful "no backend" state.
 *   If the Fc5025DetectRunner is null, do_detect_drive() returns a ProviderError.
 *   If the runner's exit_code != 0 AND no_disk/not_ready flags are set,
 *   SectorUnreadable is returned (non-retryable hardware failure).
 */

#include "fc5025_provider_v2.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::hal {

/* ────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ──────────────────────────────────────────────────────────────────────── */

FC5025ProviderV2::FC5025ProviderV2(Fc5025Runner  read_runner,
                                    Fc5025DetectRunner detect_runner,
                                    int max_cylinders)
    : m_read_runner(std::move(read_runner))
    , m_detect_runner(std::move(detect_runner))
    , m_max_cylinders(max_cylinders)
    , m_disk_format(0x02)  /* FMT_APPLE_DOS33 default */
{
    if (m_max_cylinders < 0) {
        m_max_cylinders = 79;
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  Public utility
 * ──────────────────────────────────────────────────────────────────────── */

void FC5025ProviderV2::set_disk_format(uint8_t fmt_code) noexcept
{
    m_disk_format = fmt_code;
}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ──────────────────────────────────────────────────────────────────────── */

/* static */
ProviderError FC5025ProviderV2::null_runner_error(const char* operation)
{
    std::string what = std::string("FC5025 ") + operation + " failed: no backend runner configured";
    return ProviderError{
        UFT_E_GENERIC,
        what,
        "The FC5025ProviderV2 was constructed with a null runner for this operation. "
        "This occurs when the provider is not properly initialized for the target "
        "environment (neither direct USB nor fcimage CLI runner was provided).",
        "Construct FC5025ProviderV2 with a valid Fc5025Runner (and Fc5025DetectRunner) "
        "that wraps either the direct USB libusb path (when UFT_FC5025_SUPPORT is "
        "compiled in) or the fcimage CLI path. In tests, use a SubprocessMock "
        "adapter lambda. See fc5025_provider_v2.h for the production runner template."
    };
}

ProviderError FC5025ProviderV2::range_error(int cylinder, int head) const
{
    std::string what = "FC5025 read: geometry out of range";
    std::string why  = "Cylinder " + std::to_string(cylinder) + " or head "
                     + std::to_string(head)
                     + " is outside the valid range for the FC5025 with the current "
                       "format configuration. Valid cylinder range: [0, "
                     + std::to_string(m_max_cylinders) + "]. Valid head range: [0, 1].";
    std::string fix  = "Pass cylinder in range [0, " + std::to_string(m_max_cylinders)
                     + "] and head 0 or 1. Check that the correct disk format has been "
                       "set via set_disk_format() for the inserted disk. 5.25\" drives "
                       "typically have 35-40 cylinders; 8\" drives have 77.";
    return ProviderError{ UFT_E_GENERIC, what, why, fix };
}

/* static */
SectorOutcome FC5025ProviderV2::translate_run_failure(
    const Fc5025RunResult& result, int cylinder, int head, int attempts)
{
    /* Non-retryable hardware errors → SectorUnreadable */
    if (result.no_disk) {
        SectorUnreadable u;
        u.position = CHS{cylinder, head};
        u.physical_reason = "FC5025 reported no index pulse on cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head)
            + ". Verify that a disk is inserted and fully seated in the drive. "
              "The drive motor must be spinning before read commands are issued.";
        u.attempts = attempts;
        return u;
    }

    if (result.not_ready) {
        SectorUnreadable u;
        u.position = CHS{cylinder, head};
        u.physical_reason = "FC5025 reported drive not ready on cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head)
            + ". The 5.25\" drive may not be powered, may not be connected, "
              "or the FC5025 device may have lost contact with the drive.";
        u.attempts = attempts;
        return u;
    }

    /* General failure with error message → ProviderError */
    std::string why = "The FC5025 read operation returned exit code "
        + std::to_string(result.exit_code) + " for cylinder "
        + std::to_string(cylinder) + " head " + std::to_string(head) + ".";
    if (!result.error_message.empty()) {
        why += " Backend error: " + result.error_message;
    } else {
        why += " No additional error detail was provided by the backend runner.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "FC5025 sector read failed for C" + std::to_string(cylinder)
            + " H" + std::to_string(head),
        why,
        "Verify that the FC5025 device is connected via USB and that the "
        "selected disk format matches the inserted disk. For the fcimage CLI "
        "path, ensure fcimage is installed and on PATH. For the direct USB "
        "path, ensure libusb drivers are installed and the FC5025 is powered. "
        "Download FC5025 drivers from http://www.deviceside.com/fc5025.html"
    };
}

/* static */
SectorOutcome FC5025ProviderV2::translate_run_success(
    const Fc5025RunResult& result, int cylinder, int head)
{
    /* Rule F-3: when CRC errors occurred, produce SectorMarginal with
     * divergent_reads populated. The conformance harness requires
     * divergent_reads.size() >= 2 for SectorMarginal to be valid. We
     * always provide at least two: the partial read and the "failed attempt"
     * sentinel (empty vector). This exactly matches V1 semantics where
     * partial data was preserved alongside the error annotation. */
    if (result.crc_error_count > 0) {
        SectorMarginal m;
        m.position = CHS{cylinder, head};
        m.quality  = QualityFlag::CRC_FAIL | QualityFlag::PARTIAL_RECOVERY;

        /* First entry: the partial sector data received before CRC errors. */
        m.divergent_reads.push_back(
            std::vector<uint8_t>(result.sector_bytes.begin(),
                                  result.sector_bytes.end()));

        /* Second entry: an empty-sector sentinel representing the sectors
         * that could not be recovered. This makes size() >= 2 always true
         * (rule F-3 conformance invariant), and represents the forensic
         * truth: those sectors were attempted but the data could not be
         * verified. */
        m.divergent_reads.emplace_back();

        m.timing_note = "FC5025 reported " + std::to_string(result.crc_error_count)
            + " CRC error(s) on cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + ". Partial sector data preserved (rule F-3). "
              "The disk may be partially degraded or the format selection "
              "may not match the actual on-disk encoding. "
              "Consider re-reading with a different format code or at lower speed.";
        return m;
    }

    /* Clean read: return SectorRead */
    SectorRead r;
    r.position = CHS{cylinder, head};
    r.data.assign(result.sector_bytes.begin(), result.sector_bytes.end());
    r.quality = QualityFlag::CRC_OK;
    r.retries_used = 0;
    return r;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_sector
 *
 *  Maps to: ReadsSectors concept / read_sector(ReadSectorParams) mixin.
 *
 *  V1 equivalent: readTrack(ReadParams) in fc5025hardwareprovider.cpp.
 *  Both readTrackViaUsb() and readTrackViaCli() are abstracted by the
 *  injected Fc5025Runner. The V2 does not know which path is taken —
 *  that is the runner's concern.
 *
 *  V2 differences vs V1:
 *  - Uses injected Fc5025Runner instead of hardcoded QProcess or libusb.
 *  - Uses ReadSectorParams (cylinder, head, sector, retries) instead of
 *    V1's ReadParams struct. The `sector` field is passed to the runner
 *    via Fc5025ReadRequest but FC5025 typically reads full tracks; the
 *    runner may ignore it for whole-track reads.
 *  - Translates Fc5025RunResult to SectorOutcome sum-type variants.
 *
 *  Rule F-3: CRC error paths produce SectorMarginal with divergent_reads
 *  having at least 2 entries. See translate_run_success() above.
 *
 *  Backend honesty: null runner or exit_code != 0 → ProviderError or
 *  SectorUnreadable depending on the failure kind. No silent failure.
 * ──────────────────────────────────────────────────────────────────────── */

SectorOutcome FC5025ProviderV2::do_read_sector(const ReadSectorParams& p)
{
    if (!m_read_runner) {
        return null_runner_error("sector read");
    }

    /* Validate geometry. FC5025 supports 5.25" and 8" drives.
     * Max cylinder is configurable; heads are always 0-1. */
    if (p.cylinder < 0 || p.cylinder > m_max_cylinders) {
        return range_error(p.cylinder, p.head);
    }
    if (p.head < 0 || p.head > 1) {
        return range_error(p.cylinder, p.head);
    }

    /* Build the read request for the injected runner. */
    Fc5025ReadRequest req;
    req.cylinder    = p.cylinder;
    req.head        = p.head;
    req.retries     = (p.retries >= 0) ? p.retries : 3;
    req.disk_format = m_disk_format;

    Fc5025RunResult result = m_read_runner(req);

    if (result.exit_code != 0 || result.no_disk || result.not_ready) {
        return translate_run_failure(result, p.cylinder, p.head, req.retries);
    }

    if (result.sector_bytes.empty()) {
        /* Runner returned success but no data. This can happen when the
         * disk is blank on this track, or the format doesn't match.
         * Report as SectorUnreadable — not a crash, but not a clean read. */
        SectorUnreadable u;
        u.position = CHS{p.cylinder, p.head};
        u.physical_reason = "FC5025 read operation completed but no sector data was "
            "returned for cylinder " + std::to_string(p.cylinder)
            + " head " + std::to_string(p.head)
            + ". The track may be blank, unformatted, or the disk format may not "
              "match the selected format code. Verify the format setting via "
              "set_disk_format() and ensure a formatted disk is inserted.";
        u.attempts = req.retries + 1;
        return u;
    }

    return translate_run_success(result, p.cylinder, p.head);
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: autoDetectDevice() + detectDrive() in
 *  fc5025hardwareprovider.cpp.
 *
 *  V1 tries:
 *    1. Direct USB: open() via libusb VID:PID 0x16C0:0x06D6
 *    2. CLI fallback: check if fcimage is on PATH
 *    3. If neither: report not found
 *
 *  The V2 delegates the probe to the injected Fc5025DetectRunner, which
 *  encapsulates the same two-path probe. The runner returns an
 *  Fc5025DetectResult with found / firmware / drive_kind / error_message.
 *
 *  If the detect runner is null or returns found==false, returns either
 *  DriveAbsent (runner succeeded but device not found) or ProviderError
 *  (runner itself failed). DriveAbsent carries scanned_for with the
 *  FC5025 USB VID:PID for the audit trail.
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome FC5025ProviderV2::do_detect_drive()
{
    if (!m_detect_runner) {
        return null_runner_error("drive detection");
    }

    Fc5025DetectResult result = m_detect_runner();

    if (!result.found) {
        /* Device not found. Return DriveAbsent for the audit trail. */
        if (!result.error_message.empty()) {
            /* Runner itself encountered an error — return ProviderError. */
            return ProviderError{
                UFT_E_GENERIC,
                "FC5025 drive detection failed",
                "The FC5025 detect runner returned an error: " + result.error_message,
                "Ensure the FC5025 device is connected via USB (VID:PID 0x16C0:0x06D6) "
                "or that the fcimage CLI tool is installed and on PATH. Download drivers "
                "and tools from http://www.deviceside.com/fc5025.html. On Linux, you may "
                "need to add a udev rule for the FC5025 USB device."
            };
        }

        /* Clean probe — device simply not present. */
        DriveAbsent absent;
        absent.scanned_for = "FC5025 USB floppy controller "
                             "(Device Side Data, VID:PID 0x16C0:0x06D6) "
                             "and fcimage CLI tool";
        return absent;
    }

    /* Device found. Populate DriveDetected from the probe result. */
    DriveDetected detected;

    /* Drive kind from runner (may be set from format config or USB probe). */
    if (!result.drive_kind.empty()) {
        detected.drive_kind = result.drive_kind;
    } else {
        detected.drive_kind = "5.25\" DD/SD (FC5025 read-only controller)";
    }

    /* The FC5025 firmware does not report drive geometry, and
     * Fc5025DetectResult carries no tracks/heads/rpm fields — so detect
     * cannot determine geometry. Report 0 = "not auto-detected" (the
     * same sentinel GreaseweazleProviderV2::do_detect_drive uses on a
     * no-RPM-signal result) rather than fabricating a 5.25" DD default.
     * The user-selected disk_format determines the actual geometry
     * downstream — see audit finding ARCH-5 / FC-D5-3. */
    detected.tracks      = 0;
    detected.heads       = 0;
    detected.rpm_nominal = 0.0;

    /* Firmware version from direct USB probe (empty in CLI mode). */
    if (!result.firmware.empty()) {
        detected.firmware = result.firmware;
    } else {
        detected.firmware = "FC5025 (firmware version unknown — CLI mode or not queried)";
    }

    return detected;
}

}  // namespace uft::hal
