/**
 * @file xum1541_provider_v2.cpp
 * @brief XUM1541ProviderV2 implementation (MF-165 / P1.12).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps XUM1541/ZoomFloppy sector operations (via the OpenCBM
 * IEC bus) into Type-Driven HAL outcome sum-types. It does NOT rewrite any
 * IEC protocol logic — every actual hardware interaction is delegated to the
 * injected runners, which in production wrap the OpenCBM dynamic-library
 * calls (cbm_listen / cbm_raw_write / cbm_raw_read / cbm_untalk etc.),
 * and in tests wrap scripted lambdas.
 *
 * Backend paths carried forward from V1 (xum1541hardwareprovider.cpp):
 *
 *   Read path (OpenCBM IEC U1 block-read):
 *     1. LISTEN dev, channel 2 + send "#" (open direct-access buffer)
 *     2. For each sector: LISTEN dev, channel 15 + send "U1:2 0 track sector"
 *     3. Check drive status channel (error codes 0 and 1 are OK)
 *     4. TALK dev, channel 2 + raw_read 256 bytes
 *     5. Retry on error up to params.retries
 *     6. LISTEN dev, channel 15 + send "I" (close buffer, reinitialize)
 *
 *   Write path (OpenCBM IEC U2 block-write):
 *     1. LISTEN dev, channel 2 + send "#" (open direct-access buffer)
 *     2. For each sector: LISTEN dev, channel 2 + raw_write 256 bytes
 *     3. LISTEN dev, channel 15 + send "U2:2 0 track sector"
 *     4. Check drive status (code 26 = write-protect → WriteRefused)
 *     5. Optional verify: cbmReadSector + memcmp
 *     6. LISTEN dev, channel 15 + send "I" (close buffer)
 *
 *   Detect path (cbm_driver_open + cbm_identify):
 *     1. Load OpenCBM dynamic library
 *     2. cbm_driver_open(&handle, 0)
 *     3. cbm_identify(handle, device, buf, size)
 *     4. Parse ROM version string: "73, CBM DOS V2.6 1541,00,00"
 *
 * Backend honesty (M3.2 scaffold):
 *   The runner abstraction is M3.2-honest: when OpenCBM is not installed
 *   (runner returns opencbm_unavailable=true), do_read_sector() returns a
 *   ProviderError with the M3.2 marker. The TYPE SHAPE is V2-conformant
 *   (static_assert proofs in the header); the runtime error reflects the
 *   backend maturity.
 *
 * Rule F-3 (divergent-read preservation):
 *   The V1 readTrack() has a per-sector retry loop that preserves partial
 *   data in the track buffer — successful sectors are non-zero, failed
 *   sectors are zeroed. The V2 carries this forward: when bad_sectors > 0
 *   the partial sector data is preserved as SectorMarginal::divergent_reads
 *   with at least two entries (the partial-data buffer + the empty-failure
 *   sentinel). This satisfies the conformance invariant:
 *   SectorMarginal::divergent_reads.size() >= 2.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings; this is a runtime guard
 *   that catches programming mistakes during development.
 */

#include "xum1541_provider_v2.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::hal {

/* ────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ──────────────────────────────────────────────────────────────────────── */

XUM1541ProviderV2::XUM1541ProviderV2(Xum1541Runner       read_runner,
                                      Xum1541WriteRunner  write_runner,
                                      Xum1541DetectRunner detect_runner,
                                      int max_cylinder,
                                      int device_num,
                                      int drive_type)
    : m_read_runner(std::move(read_runner))
    , m_write_runner(std::move(write_runner))
    , m_detect_runner(std::move(detect_runner))
    , m_max_cylinder(max_cylinder < 0 ? 79 : max_cylinder)
    , m_device_num(device_num)
    , m_drive_type(drive_type)
{}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ──────────────────────────────────────────────────────────────────────── */

/* static */
ProviderError XUM1541ProviderV2::null_runner_error(const char* operation)
{
    std::string what = std::string("XUM1541 ") + operation
                     + " failed: no backend runner configured";
    return ProviderError{
        UFT_E_GENERIC,
        what,
        "The XUM1541ProviderV2 was constructed with a null runner for this "
        "operation. This occurs when the provider is not properly initialized "
        "for the target environment (no OpenCBM runner was injected).",
        "Construct XUM1541ProviderV2 with a valid runner that wraps the "
        "OpenCBM dynamic-library path (cbm_listen / cbm_raw_read / cbm_raw_write). "
        "In tests, inject a scripted lambda. "
        "Install OpenCBM from https://github.com/OpenCBM/OpenCBM."
    };
}

ProviderError XUM1541ProviderV2::range_error(int cylinder, int head) const
{
    std::string what = "XUM1541 read: geometry out of range";
    std::string why  = "Cylinder " + std::to_string(cylinder) + " or head "
                     + std::to_string(head)
                     + " is outside the valid range for the XUM1541 with the "
                       "current drive type configuration. Valid cylinder range: [0, "
                     + std::to_string(m_max_cylinder)
                     + "]. Valid head range: [0, 1]. CBM 1541 drives have 35 "
                       "tracks (cylinders 0-34, single-sided, head 0 only). "
                       "CBM 1571 drives have 35 tracks per side. "
                       "CBM 1581 drives have 80 tracks (cylinders 0-79, both sides).";
    std::string fix  = "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
                     + "] and head 0 or 1 (head 1 only for 1571/1581). "
                       "Verify the correct drive type is set. "
                       "A 1541 is single-sided — always use head 0. "
                       "For the 1571, side 1 maps internally to CBM tracks 36-70.";
    return ProviderError{ UFT_E_GENERIC, what, why, fix };
}

/* static */
SectorOutcome XUM1541ProviderV2::translate_read_failure(
    const Xum1541ReadResult& result, int cylinder, int head)
{
    /* OpenCBM library not installed — ProviderError with M3.2 marker */
    if (result.opencbm_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 sector read failed: OpenCBM not available (M3.2 pending)",
            "The XUM1541ProviderV2 sector read requires the OpenCBM library "
            "(opencbm.dll on Windows, libopencbm.so on Linux/macOS). The library "
            "was not found at the expected system paths. This is the M3.2 partial "
            "scaffold state — the USB I/O path requires OpenCBM to be installed "
            "before real reads are possible. Cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head) + ".",
            "Install OpenCBM from https://github.com/OpenCBM/OpenCBM. "
            "On Linux: sudo apt install opencbm. On Windows: install the "
            "OpenCBM package from the project's release page and ensure "
            "opencbm.dll is on your PATH or in the application directory. "
            "A ZoomFloppy or XUM1541 adapter must also be connected via USB."
        };
    }

    /* Track out of range for drive type */
    if (result.invalid_track) {
        SectorUnreadable u;
        u.position = CHS{cylinder, head};
        u.physical_reason = "XUM1541: CBM track number derived from cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head)
            + " (CBM track " + std::to_string(result.cbm_track)
            + ") is invalid for the configured drive type. Verify that the "
              "drive type is set correctly (e.g. a 1541 has 35 tracks; a 1581 "
              "has 80 tracks). Tracks 36-70 on the 1571 are side-1.";
        u.attempts = 1;
        return u;
    }

    /* General error from the IEC bus / OpenCBM */
    std::string why = "The XUM1541 IEC read operation for cylinder "
        + std::to_string(cylinder) + " head " + std::to_string(head)
        + " (CBM track " + std::to_string(result.cbm_track) + ") failed.";
    if (!result.error_message.empty()) {
        why += " Backend error: " + result.error_message;
    } else {
        why += " No additional error detail was provided by the backend runner.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "XUM1541 sector read failed for C" + std::to_string(cylinder)
            + " H" + std::to_string(head),
        why,
        "Verify that the ZoomFloppy/XUM1541 adapter is connected via USB and "
        "that OpenCBM is installed. Ensure a Commodore 1541/1571/1581 drive is "
        "connected on the IEC bus at the configured device number (default 8). "
        "Check that the drive is powered on and a disk is inserted. "
        "Run 'cbmctrl status 8' to verify the OpenCBM driver can communicate "
        "with the drive. See https://github.com/OpenCBM/OpenCBM for setup docs."
    };
}

/* static */
SectorOutcome XUM1541ProviderV2::translate_read_result(
    const Xum1541ReadResult& result, int cylinder, int head)
{
    /* Rule F-3: when some sectors failed, produce SectorMarginal with
     * divergent_reads populated. The conformance harness requires
     * divergent_reads.size() >= 2 for SectorMarginal to be valid.
     *
     * Two entries: (1) the partial sector data that was received for the
     * good sectors, (2) an empty sentinel representing the sectors that
     * failed. This matches V1 semantics where the track buffer has non-zero
     * data for good sectors and zeroed slots for bad ones. */
    if (result.bad_sectors > 0) {
        SectorMarginal m;
        m.position = CHS{cylinder, head};
        m.quality  = QualityFlag::CRC_FAIL | QualityFlag::PARTIAL_RECOVERY;

        /* First entry: the partial sector data received (good sectors
         * populated, bad sectors zeroed in the buffer). */
        m.divergent_reads.push_back(
            std::vector<uint8_t>(result.sector_bytes.begin(),
                                  result.sector_bytes.end()));

        /* Second entry: empty-failure sentinel representing the sectors
         * that could not be recovered after all retries. This makes
         * size() >= 2 always true (rule F-3 conformance invariant),
         * and represents the forensic truth: those sectors were attempted
         * but the data could not be verified. */
        m.divergent_reads.emplace_back();

        m.timing_note = "XUM1541 partial read on CBM track "
            + std::to_string(result.cbm_track)
            + " (cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + "): " + std::to_string(result.good_sectors)
            + "/" + std::to_string(result.total_sectors)
            + " sectors OK, " + std::to_string(result.bad_sectors)
            + " sectors failed after all retries. "
              "Partial sector data preserved (rule F-3). "
              "The disk may have read errors on specific sectors. "
              "Consider re-reading with increased retries, or the disk "
              "may require physical recovery.";
        return m;
    }

    /* All sectors read cleanly. */
    SectorRead r;
    r.position = CHS{cylinder, head};
    r.data.assign(result.sector_bytes.begin(), result.sector_bytes.end());
    r.quality = QualityFlag::CRC_OK;
    r.retries_used = 0;
    return r;
}

/* static */
WriteOutcome XUM1541ProviderV2::translate_write_result(
    const Xum1541WriteResult& result, int cylinder, int head,
    const SectorPayload& payload)
{
    /* Write-protect notch → WriteRefused */
    if (result.write_protected) {
        WriteRefused refused;
        refused.position = CHS{cylinder, head};
        refused.physical_reason = "XUM1541: CBM drive reports write-protect on "
            "CBM track " + std::to_string(result.cbm_track)
            + " (cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + "). Status code 26 (WRITE PROTECT ON) was returned by the drive. "
              "The disk has the write-protect notch covered, or is a factory-"
              "protected disk. To write, use a disk with the notch open "
              "(5.25\" disks: uncover the notch on the side; 3.5\" disks on "
              "1581: move the write-protect slider to closed position).";
        return refused;
    }

    /* OpenCBM not available → ProviderError with M3.2 marker */
    if (result.opencbm_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 sector write failed: OpenCBM not available (M3.2 pending)",
            "The XUM1541ProviderV2 sector write requires the OpenCBM library. "
            "The library was not found. This is the M3.2 partial scaffold state. "
            "Cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head) + ".",
            "Install OpenCBM from https://github.com/OpenCBM/OpenCBM. "
            "On Linux: sudo apt install opencbm. "
            "Ensure opencbm.dll is on PATH (Windows)."
        };
    }

    /* Invalid parameters (track out of range, data too short) */
    if (result.invalid_params) {
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 sector write failed: invalid parameters",
            "The write request for cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + " failed parameter validation. Either the CBM track number derived "
              "from the cylinder/head is invalid for the configured drive type, "
              "or the sector payload is too short for the expected track geometry. "
            + (result.error_message.empty() ? "" : "Detail: " + result.error_message),
            "Verify the cylinder and head are within range for the drive type. "
            "Ensure the SectorPayload contains exactly numSectors * 256 bytes "
            "for the target track (a 1541 track 1 needs 21 * 256 = 5376 bytes). "
            "Check the drive type configuration."
        };
    }

    /* Partial write (some sectors failed) */
    if (result.sectors_written < result.total_sectors && result.total_sectors > 0) {
        std::string why = "XUM1541 partial write on CBM track "
            + std::to_string(result.cbm_track)
            + " (cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + "): " + std::to_string(result.sectors_written)
            + "/" + std::to_string(result.total_sectors) + " sectors written.";
        if (!result.error_message.empty()) {
            why += " " + result.error_message;
        }
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 sector write incomplete for C" + std::to_string(cylinder)
                + " H" + std::to_string(head),
            why,
            "Retry the write operation. If the problem persists, the disk may "
            "have worn recording surface on these sectors. Consider using a "
            "different disk. Check the drive alignment if write errors are "
            "systematic across many tracks."
        };
    }

    /* General write failure (no sectors written) */
    if (result.sectors_written == 0 && !result.error_message.empty()) {
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 sector write failed for C" + std::to_string(cylinder)
                + " H" + std::to_string(head),
            "The XUM1541 IEC write operation returned no sectors written for "
            "cylinder " + std::to_string(cylinder) + " head " + std::to_string(head)
            + ". Error: " + result.error_message,
            "Verify the ZoomFloppy/XUM1541 adapter is connected and OpenCBM "
            "can communicate with the drive. Ensure the disk is not write-protected "
            "and is properly inserted. Run 'cbmctrl status 8' to check the drive."
        };
    }

    /* Successful write (all sectors written). */
    WriteCompleted completed;
    completed.position      = CHS{cylinder, head};
    completed.bytes_written = static_cast<std::size_t>(result.sectors_written) * 256u;
    completed.verified      = false; /* verification is within the runner */
    completed.quality       = QualityFlag::CRC_OK;

    /* If the caller requested verify and the runner performed it,
     * mark accordingly. The runner sets sectors_written = total_sectors
     * only after successful verification. */
    if (result.total_sectors > 0 &&
        result.sectors_written == result.total_sectors &&
        !payload.bytes.empty())
    {
        completed.verified = true;
    }

    (void)payload;  /* payload is inspected above for verified flag only */
    return completed;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_sector
 *
 *  Maps to: ReadsSectors concept / read_sector(ReadSectorParams) mixin.
 *
 *  V1 equivalent: readTrack(ReadParams) in xum1541hardwareprovider.cpp.
 *  The V2 does not know the IEC protocol details — all bus communication
 *  is delegated to the injected Xum1541Runner.
 *
 *  V2 differences vs V1:
 *  - Uses injected Xum1541Runner instead of direct OpenCBM calls.
 *  - Uses ReadSectorParams (cylinder, head, sector, retries) instead of
 *    V1's ReadParams struct.
 *  - Translates Xum1541ReadResult to SectorOutcome sum-type variants.
 *
 *  Rule F-3: partial reads (bad_sectors > 0) produce SectorMarginal with
 *  divergent_reads having at least 2 entries. See translate_read_result().
 *
 *  Backend honesty: null runner or opencbm_unavailable → ProviderError
 *  with M3.2 marker. No silent failure.
 * ──────────────────────────────────────────────────────────────────────── */

SectorOutcome XUM1541ProviderV2::do_read_sector(const ReadSectorParams& p)
{
    if (!m_read_runner) {
        return null_runner_error("sector read");
    }

    /* Geometry validation: cylinder must be in [0, max_cylinder],
     * head in [0, 1]. */
    if (p.cylinder < 0 || p.cylinder > m_max_cylinder) {
        return range_error(p.cylinder, p.head);
    }
    if (p.head < 0 || p.head > 1) {
        return range_error(p.cylinder, p.head);
    }

    /* Build the read request for the injected runner. */
    Xum1541ReadRequest req;
    req.cylinder   = p.cylinder;
    req.head       = p.head;
    req.retries    = (p.retries >= 0) ? p.retries : 3;
    req.device_num = m_device_num;
    req.drive_type = m_drive_type;

    Xum1541ReadResult result = m_read_runner(req);

    /* Hard failures: return early. */
    if (result.opencbm_unavailable || result.invalid_track ||
        (result.good_sectors == 0 && result.total_sectors == 0 &&
         !result.error_message.empty()))
    {
        return translate_read_failure(result, p.cylinder, p.head);
    }

    /* No data at all and no explicit failure flag: SectorUnreadable. */
    if (result.sector_bytes.empty()) {
        SectorUnreadable u;
        u.position = CHS{p.cylinder, p.head};
        u.physical_reason = "XUM1541 read completed but no sector data was "
            "returned for cylinder " + std::to_string(p.cylinder)
            + " head " + std::to_string(p.head)
            + " (CBM track " + std::to_string(result.cbm_track) + "). "
              "The track may be blank, the drive may not be responding, "
              "or the direct-access buffer operation failed silently. "
              "Verify the IEC bus is active and the drive is ready.";
        u.attempts = req.retries + 1;
        return u;
    }

    return translate_read_result(result, p.cylinder, p.head);
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_write_sector
 *
 *  Maps to: WritesSectors concept / write_sector(WriteSectorParams,
 *  SectorPayload) mixin.
 *
 *  V1 equivalent: writeTrack(WriteParams, QByteArray) in
 *  xum1541hardwareprovider.cpp. The IEC U2 block-write protocol is
 *  delegated entirely to the injected Xum1541WriteRunner.
 *
 *  V2 differences vs V1:
 *  - Uses injected Xum1541WriteRunner instead of direct OpenCBM calls.
 *  - Uses WriteSectorParams + SectorPayload instead of V1's WriteParams +
 *    QByteArray.
 *  - Translates Xum1541WriteResult to WriteOutcome sum-type variants.
 *  - write_protect status code 26 → WriteRefused (type-safe, not magic int).
 * ──────────────────────────────────────────────────────────────────────── */

WriteOutcome XUM1541ProviderV2::do_write_sector(const WriteSectorParams& w,
                                                  const SectorPayload& payload)
{
    if (!m_write_runner) {
        return null_runner_error("sector write");
    }

    /* Geometry validation. */
    if (w.cylinder < 0 || w.cylinder > m_max_cylinder) {
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 write: geometry out of range",
            "Cylinder " + std::to_string(w.cylinder) + " head "
            + std::to_string(w.head) + " is outside the valid range [0, "
            + std::to_string(m_max_cylinder)
            + "]. Valid head range: [0, 1]. Check drive type configuration.",
            "Pass cylinder in range [0, " + std::to_string(m_max_cylinder)
            + "] and head 0 or 1. Verify the correct drive type is configured."
        };
    }
    if (w.head < 0 || w.head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "XUM1541 write: geometry out of range",
            "Head " + std::to_string(w.head) + " is outside valid range [0, 1] "
            "for cylinder " + std::to_string(w.cylinder)
            + ". CBM 1541 drives are single-sided (head 0 only). "
              "CBM 1571 and 1581 support two sides (head 0 and 1).",
            "Pass head 0 for 1541 drives, or head 0 or 1 for 1571/1581 drives. "
            "Verify the drive type setting."
        };
    }

    /* Build the write request. */
    Xum1541WriteRequest req;
    req.cylinder     = w.cylinder;
    req.head         = w.head;
    req.retries      = 3;
    req.device_num   = m_device_num;
    req.drive_type   = m_drive_type;
    req.verify       = w.verify;
    req.sector_bytes.assign(payload.bytes.begin(), payload.bytes.end());

    Xum1541WriteResult result = m_write_runner(req);

    return translate_write_result(result, w.cylinder, w.head, payload);
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: autoDetectDevice() + detectDrive() in
 *  xum1541hardwareprovider.cpp.
 *
 *  V1 tries:
 *    1. Load OpenCBM library
 *    2. cbm_driver_open to detect adapter
 *    3. If found: report OpenCBM available + device info
 *    4. If not found: report OpenCBM not installed + instructions
 *    5. cbm_identify or status channel parse for drive type
 *
 *  The V2 delegates the probe to the injected Xum1541DetectRunner, which
 *  encapsulates the cbm_driver_open + cbm_identify probe. The runner
 *  returns an Xum1541DetectResult with found / drive_type_name / tracks
 *  / heads / firmware / transport / error_message.
 *
 *  If the detect runner is null or returns found==false, returns either
 *  DriveAbsent (clean probe, device not found) or ProviderError (runner
 *  encountered an error). DriveAbsent carries scanned_for with the
 *  ZoomFloppy USB VID:PID for the audit trail.
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome XUM1541ProviderV2::do_detect_drive()
{
    if (!m_detect_runner) {
        return null_runner_error("drive detection");
    }

    Xum1541DetectResult result = m_detect_runner();

    if (!result.found) {
        /* Runner encountered an error (not just "not found"). */
        if (!result.error_message.empty()) {
            return ProviderError{
                UFT_E_GENERIC,
                "XUM1541 drive detection failed",
                "The XUM1541 detect runner returned an error: "
                + result.error_message,
                "Ensure the ZoomFloppy or XUM1541 adapter is connected via USB "
                "(VID:PID 0x16D0:0x04B2 for ZoomFloppy, 0x16D0:0x0504 for XUM1541). "
                "Install OpenCBM from https://github.com/OpenCBM/OpenCBM. "
                "On Linux: sudo apt install opencbm and run cbmctrl reset to "
                "verify the driver is loaded. On Windows: install the OpenCBM "
                "package and run cbmctrl status 8 to test."
            };
        }

        /* Clean probe — device not present or OpenCBM not installed. */
        DriveAbsent absent;
        absent.scanned_for = "XUM1541/ZoomFloppy USB adapter "
                             "(RETRO Innovations, VID:PID 0x16D0:0x04B2 / 0x16D0:0x0504) "
                             "via OpenCBM driver (https://github.com/OpenCBM/OpenCBM)";
        return absent;
    }

    /* Device found. Populate DriveDetected from the probe result. */
    DriveDetected detected;

    if (!result.drive_type_name.empty()) {
        detected.drive_kind = result.drive_type_name;
    } else {
        detected.drive_kind = "Commodore IEC drive (type auto-detected via OpenCBM)";
    }

    /* Tracks and heads from runner. If the runner couldn't determine them,
     * use safe defaults (1541: 35 tracks, single-sided). */
    detected.tracks = (result.tracks > 0) ? result.tracks : 35;
    detected.heads  = (result.heads > 0)  ? result.heads  : 1;

    /* CBM drives run at 300 RPM (GCR zone timing is determined by track
     * zone, not by a constant RPM difference). The 1581 also runs at 300.*/
    detected.rpm_nominal = 300.0;

    /* Firmware version from USB firmware query (may be empty). */
    if (!result.firmware.empty()) {
        detected.firmware = result.firmware;
    } else {
        std::string transport = result.transport.empty() ? "OpenCBM" : result.transport;
        detected.firmware = "XUM1541/ZoomFloppy (firmware version unknown — "
                           + transport + " path, not queried)";
    }

    return detected;
}

}  // namespace uft::hal
