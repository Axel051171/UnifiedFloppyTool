/**
 * @file usbfloppy_provider_v2.cpp
 * @brief USBFloppyProviderV2 implementation (MF-168 / P1.15).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps USB Floppy UFI sector operations into Type-Driven HAL
 * outcome sum-types. It does NOT rewrite any UFI protocol logic — every
 * actual hardware interaction is delegated to the injected runners (which
 * in production wrap the UFI C-HAL functions from ufi.h).
 *
 * UFI backend paths carried forward from V1:
 *   Read path:
 *     uft_ufi_read_sectors(path, lba, count, buffer, buffer_size, &diag)
 *     LBA = (cylinder * 2 + head) * spt
 *     spt = 18 for 1.44M (2880 LBAs), 9 for 720K (1440 LBAs), else 18.
 *
 *   Write path:
 *     uft_ufi_write_sectors(path, lba, count, buffer, buffer_size, &diag)
 *     count = min(data.size() / block_size, spt)
 *
 *   Detect path:
 *     uft_ufi_inquiry(path, vendor, product, rev, &diag)
 *     uft_ufi_read_capacity(path, &total_lba, &block_size, &diag)
 *
 * Rule F-3 (divergent-read preservation):
 *   The V1 readTrack() does a single UFI read (no retry loop). When the
 *   runner returns bad_sectors > 0 with non-empty sector_bytes (partial
 *   read), the V2 produces SectorMarginal with divergent_reads having at
 *   least 2 entries: the partial data + a failed-sector sentinel. No data
 *   is collapsed, averaged, or discarded.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings.
 *
 * Backend honesty:
 *   If any runner is null, do_*() returns ProviderError with a clear
 *   what/why/fix. If the runner reports backend_unavailable, the error
 *   explains that uft_ufi_backend_init() was not called. If device_error
 *   is set, the error explains that the device path is invalid or unavailable.
 */

#include "usbfloppy_provider_v2.h"

#include <algorithm>
#include <string>
#include <vector>

namespace uft::hal {

/* ────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ──────────────────────────────────────────────────────────────────────── */

USBFloppyProviderV2::USBFloppyProviderV2(UsbFloppyReadRunner   read_runner,
                                          UsbFloppyWriteRunner  write_runner,
                                          UsbFloppyDetectRunner detect_runner,
                                          std::string           device_path,
                                          int                   max_cylinders)
    : m_read_runner(std::move(read_runner))
    , m_write_runner(std::move(write_runner))
    , m_detect_runner(std::move(detect_runner))
    , m_device_path(std::move(device_path))
    , m_max_cylinders(max_cylinders < 0 ? 79 : max_cylinders)
    , m_total_lba(0)
    , m_block_size(0)
{
}

/* ────────────────────────────────────────────────────────────────────────
 *  Public utilities
 * ──────────────────────────────────────────────────────────────────────── */

void USBFloppyProviderV2::set_device_path(const std::string& path) noexcept
{
    m_device_path = path;
}

void USBFloppyProviderV2::set_geometry(uint32_t total_lba, uint32_t block_size) noexcept
{
    m_total_lba  = total_lba;
    m_block_size = block_size;
}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ──────────────────────────────────────────────────────────────────────── */

/* static */
ProviderError USBFloppyProviderV2::null_runner_error(const char* operation)
{
    std::string what = std::string("USB Floppy ") + operation
                     + " failed: no backend runner configured";
    return ProviderError{
        UFT_E_GENERIC,
        what,
        "The USBFloppyProviderV2 was constructed with a null runner for this "
        "operation. This occurs when the provider is not properly initialized "
        "for the target environment (no UFI read/write/detect runner was "
        "provided).",
        "Construct USBFloppyProviderV2 with a valid runner that wraps the UFI "
        "C-HAL functions (uft_ufi_read_sectors, uft_ufi_write_sectors, "
        "uft_ufi_inquiry) from include/uft/hal/ufi.h. Call uft_ufi_backend_init() "
        "before the first runner invocation to register the platform backend. "
        "In tests, inject a scripted lambda runner. See usbfloppy_provider_v2.h "
        "for the production runner template."
    };
}

ProviderError USBFloppyProviderV2::range_error(int cylinder, int head) const
{
    std::string what = "USB Floppy read/write: geometry out of range";
    std::string why  = "Cylinder " + std::to_string(cylinder) + " or head "
                     + std::to_string(head)
                     + " is outside the valid range for the USB Floppy drive. "
                       "Valid cylinder range: [0, "
                     + std::to_string(m_max_cylinders)
                     + "]. Valid head range: [0, 1].";
    std::string fix  = "Pass cylinder in range [0, " + std::to_string(m_max_cylinders)
                     + "] and head 0 or 1. Standard 3.5\" HD floppy drives support "
                       "80 cylinders (0-79) and 2 heads. Call detect_drive() first to "
                       "verify the drive geometry before issuing read/write requests.";
    return ProviderError{ UFT_E_GENERIC, what, why, fix };
}

/* static */
SectorOutcome USBFloppyProviderV2::translate_read_failure(
    const UsbFloppyReadResult& result,
    int cylinder, int head, int retries)
{
    /* Backend not registered — configuration error */
    if (result.backend_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy sector read failed: UFI backend not available",
            "The UFI C-HAL backend is not registered. uft_ufi_backend_init() "
            "must be called before any UFI operation. The platform-specific "
            "backend (Linux SG_IO or Windows SCSI_PASS_THROUGH) must be "
            "compiled in and registered.",
            "Call uft_ufi_backend_init() at application startup before constructing "
            "the USB Floppy provider runner. On Linux, ensure the sg driver module "
            "is loaded ('sudo modprobe sg'). On Windows, ensure the application "
            "runs with sufficient privileges to open the SCSI pass-through device "
            "(\\\\.\\A:). Consult ufi.h for the backend registration API."
        };
    }

    /* Device error — path empty or could not be opened */
    if (result.device_error) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy sector read failed: device path not accessible",
            "The USB floppy device could not be opened. Either the device path "
            "was empty, the device is not connected, or the process lacks "
            "permission to open the SCSI generic device. "
            + (result.error_message.empty()
               ? std::string("No additional error detail from the UFI backend.")
               : "Backend error: " + result.error_message),
            "Verify that a USB floppy drive is connected. On Linux, check that "
            "'/dev/sg0' (or whichever path is configured) exists and that the user "
            "has read/write permission (add to 'dialout' or 'disk' group, or use "
            "a udev rule). On Windows, verify the drive letter is accessible. "
            "Call set_device_path() with the correct OS device path."
        };
    }

    /* No medium / not ready — non-retryable */
    if (result.no_disk) {
        SectorUnreadable u;
        u.position = CHS{cylinder, head};
        u.physical_reason = "USB Floppy reported no medium or not ready for cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head)
            + ". The drive may not contain a disk or the disk is not fully inserted. "
              "UFI SENSE: medium not present (SCSI sense key 0x02, ASC 0x3A).";
        u.attempts = retries + 1;
        return u;
    }

    /* General I/O failure */
    std::string why = "The UFI READ(10) command returned an error for cylinder "
        + std::to_string(cylinder) + " head " + std::to_string(head) + ".";
    if (!result.error_message.empty()) {
        why += " Backend error: " + result.error_message;
    } else {
        why += " No additional error detail from the UFI backend.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "USB Floppy sector read failed for C" + std::to_string(cylinder)
            + " H" + std::to_string(head),
        why,
        "Verify that the USB floppy drive is connected and a disk is inserted. "
        "Check that the device path is correct (set_device_path()). "
        "Ensure uft_ufi_backend_init() was called at startup. "
        "On Linux, verify sg driver permissions. On Windows, run with admin "
        "privileges or check the USB floppy drive firmware for UFI compliance."
    };
}

/* static */
SectorOutcome USBFloppyProviderV2::translate_read_result(
    const UsbFloppyReadResult& result,
    int cylinder, int head)
{
    /* Rule F-3: when bad_sectors > 0 and we have partial data, produce
     * SectorMarginal with divergent_reads.size() >= 2. The two entries are:
     * [0] the partial sector data (bytes received before error),
     * [1] an empty sentinel representing the failed sectors.
     * This satisfies the conformance invariant and preserves the forensic
     * truth: some sectors were read, some were not. */
    if (result.bad_sectors > 0) {
        SectorMarginal m;
        m.position = CHS{cylinder, head};
        m.quality  = QualityFlag::CRC_FAIL | QualityFlag::PARTIAL_RECOVERY;

        /* Entry [0]: partial data from the sectors that did read. */
        m.divergent_reads.push_back(
            std::vector<uint8_t>(result.sector_bytes.begin(),
                                  result.sector_bytes.end()));

        /* Entry [1]: empty sentinel for the failed sectors. The conformance
         * invariant requires divergent_reads.size() >= 2, and this represents
         * the forensic truth that some sectors could not be recovered. */
        m.divergent_reads.emplace_back();

        m.timing_note = "USB Floppy: " + std::to_string(result.bad_sectors)
            + " of " + std::to_string(result.total_sectors)
            + " sector(s) failed to read on cylinder " + std::to_string(cylinder)
            + " head " + std::to_string(head)
            + ". Partial sector data preserved (rule F-3). "
              "The disk may be partially degraded or the UFI READ(10) encountered "
              "a recoverable media error. Consider re-reading this track.";
        if (!result.error_message.empty()) {
            m.timing_note += " Backend message: " + result.error_message;
        }
        return m;
    }

    /* Clean read: return SectorRead */
    SectorRead r;
    r.position    = CHS{cylinder, head};
    r.data.assign(result.sector_bytes.begin(), result.sector_bytes.end());
    r.quality     = QualityFlag::CRC_OK;
    r.retries_used = 0;
    return r;
}

/* static */
WriteOutcome USBFloppyProviderV2::translate_write_result(
    const UsbFloppyWriteResult& result,
    int cylinder, int head,
    const SectorPayload& payload)
{
    /* Write-protect */
    if (result.write_protected) {
        WriteRefused refused;
        refused.position = CHS{cylinder, head};
        refused.physical_reason = "USB Floppy write refused: disk is write-protected. "
            "The write-protect tab on the 3.5\" disk is in the protected position "
            "(hole open). Slide the tab to the unprotected position (hole closed) "
            "and retry. SCSI sense key 0x07 (DATA_PROTECT), ASC 0x27.";
        return refused;
    }

    /* Backend not available */
    if (result.backend_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy sector write failed: UFI backend not available",
            "The UFI C-HAL backend is not registered. uft_ufi_backend_init() "
            "must be called before any UFI operation.",
            "Call uft_ufi_backend_init() at application startup. Ensure the "
            "platform-specific backend is compiled in (Linux SG_IO or Windows "
            "SCSI_PASS_THROUGH). See ufi.h for the backend registration API."
        };
    }

    /* Device error */
    if (result.device_error) {
        std::string why = "The USB floppy device could not be opened for writing.";
        if (!result.error_message.empty()) {
            why += " Backend error: " + result.error_message;
        }
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy sector write failed: device not accessible",
            why,
            "Verify that the USB floppy drive is connected and the device path "
            "is correct (set_device_path()). On Linux, verify sg permissions. "
            "On Windows, ensure admin privileges."
        };
    }

    /* Verify failed: read-back differed from written data (rule F-3) */
    if (result.verify_failed) {
        WriteVerifyFailed vf;
        vf.position      = CHS{cylinder, head};
        vf.bytes_written = result.sectors_written
                           * (result.sectors_written > 0 ? payload.bytes.size()
                                                          / std::max(1, result.total_sectors) : 0);
        /* Rule F-3: both samples preserved — do not collapse */
        vf.intended  = result.intended;
        vf.readback  = result.readback;
        /* If the runner didn't populate intended/readback, use payload as intended */
        if (vf.intended.empty()) {
            vf.intended.assign(payload.bytes.begin(), payload.bytes.end());
        }
        return vf;
    }

    /* General write failure */
    if (result.sectors_written == 0 && result.total_sectors > 0) {
        std::string why = "The UFI WRITE(10) command failed for cylinder "
            + std::to_string(cylinder) + " head " + std::to_string(head) + ".";
        if (!result.error_message.empty()) {
            why += " Backend error: " + result.error_message;
        }
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy sector write failed for C" + std::to_string(cylinder)
                + " H" + std::to_string(head),
            why,
            "Check that the disk is not write-protected, is properly inserted, "
            "and the drive is functioning. Verify the device path and UFI backend "
            "registration. On Windows, run with administrator privileges."
        };
    }

    /* Success */
    WriteCompleted done;
    done.position      = CHS{cylinder, head};
    done.bytes_written = payload.bytes.size();
    done.verified      = false;  /* Runner sets verify_failed if verify was requested */
    done.quality       = QualityFlag::CRC_OK;
    return done;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_sector
 *
 *  Maps to: ReadsSectors concept / read_sector(ReadSectorParams) mixin.
 *
 *  V1 equivalent: readTrack(ReadParams) in usbfloppyhardwareprovider.cpp.
 *  The V1 calls uft_ufi_read_sectors() directly with the device path.
 *  The V2 delegates to the injected UsbFloppyReadRunner.
 *
 *  V2 differences vs V1:
 *  - Uses injected runner instead of direct UFI C-HAL calls.
 *  - Uses ReadSectorParams (cylinder, head, sector, retries).
 *  - Translates UsbFloppyReadResult to SectorOutcome sum-type variants.
 *  - Enforces geometry bounds (range_error on out-of-range cylinder/head).
 *
 *  Rule F-3: partial reads produce SectorMarginal with divergent_reads >= 2.
 *  Backend honesty: null runner or failure → ProviderError or SectorUnreadable.
 * ──────────────────────────────────────────────────────────────────────── */

SectorOutcome USBFloppyProviderV2::do_read_sector(const ReadSectorParams& p)
{
    if (!m_read_runner) {
        return null_runner_error("sector read");
    }

    /* Geometry validation */
    if (p.cylinder < 0 || p.cylinder > m_max_cylinders) {
        return range_error(p.cylinder, p.head);
    }
    if (p.head < 0 || p.head > 1) {
        return range_error(p.cylinder, p.head);
    }

    /* Build the read request */
    UsbFloppyReadRequest req;
    req.cylinder    = p.cylinder;
    req.head        = p.head;
    req.sector      = p.sector;
    req.retries     = (p.retries >= 0) ? p.retries : 3;
    req.device_path = m_device_path;
    req.total_lba   = m_total_lba;
    req.block_size  = m_block_size;

    UsbFloppyReadResult result = m_read_runner(req);

    /* Failure conditions */
    if (result.backend_unavailable || result.device_error || result.no_disk
            || (result.good_sectors == 0 && result.bad_sectors == 0
                && result.sector_bytes.empty() && !result.error_message.empty()))
    {
        return translate_read_failure(result, p.cylinder, p.head, req.retries);
    }

    /* Empty data returned but no error flags — unreadable track */
    if (result.sector_bytes.empty() && result.good_sectors == 0) {
        SectorUnreadable u;
        u.position = CHS{p.cylinder, p.head};
        u.physical_reason = "USB Floppy read completed but no sector data was returned "
            "for cylinder " + std::to_string(p.cylinder)
            + " head " + std::to_string(p.head)
            + ". The track may be blank, unformatted, or the UFI READ(10) returned "
              "success with zero bytes. Verify that a formatted disk is inserted.";
        u.attempts = req.retries + 1;
        return u;
    }

    return translate_read_result(result, p.cylinder, p.head);
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_write_sector
 *
 *  Maps to: WritesSectors concept / write_sector(WriteSectorParams, payload).
 *
 *  V1 equivalent: writeTrack(WriteParams, data) in usbfloppyhardwareprovider.cpp.
 *  The V1 calls uft_ufi_write_sectors() directly. The V2 delegates to the
 *  injected UsbFloppyWriteRunner.
 *
 *  V2 differences vs V1:
 *  - Uses injected runner instead of direct UFI C-HAL calls.
 *  - Uses WriteSectorParams + SectorPayload.
 *  - Translates UsbFloppyWriteResult to WriteOutcome sum-type variants.
 *  - Enforces geometry bounds.
 * ──────────────────────────────────────────────────────────────────────── */

WriteOutcome USBFloppyProviderV2::do_write_sector(const WriteSectorParams& w,
                                                   const SectorPayload& payload)
{
    if (!m_write_runner) {
        /* WriteOutcome cannot hold ProviderError? It can — ProviderError is
         * in WriteOutcome. */
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy sector write failed: no backend runner configured",
            "The USBFloppyProviderV2 was constructed with a null write runner. "
            "This occurs when the provider was initialized without a write runner.",
            "Construct USBFloppyProviderV2 with a valid UsbFloppyWriteRunner that "
            "wraps uft_ufi_write_sectors() from include/uft/hal/ufi.h. "
            "In tests, inject a scripted lambda runner. See usbfloppy_provider_v2.h "
            "for the production write runner template."
        };
    }

    /* Geometry validation */
    if (w.cylinder < 0 || w.cylinder > m_max_cylinders) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy write: geometry out of range",
            "Cylinder " + std::to_string(w.cylinder) + " or head "
                + std::to_string(w.head)
                + " is outside the valid range for the USB Floppy drive. "
                  "Valid cylinder range: [0, " + std::to_string(m_max_cylinders)
                + "]. Valid head range: [0, 1].",
            "Pass cylinder in range [0, " + std::to_string(m_max_cylinders)
                + "] and head 0 or 1. Call detect_drive() first to verify geometry."
        };
    }
    if (w.head < 0 || w.head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy write: head out of range",
            "Head " + std::to_string(w.head)
                + " is outside the valid range [0, 1] for the USB Floppy drive.",
            "Pass head 0 (front/bottom) or 1 (back/top) for a double-sided disk."
        };
    }

    /* Build the write request */
    UsbFloppyWriteRequest req;
    req.cylinder     = w.cylinder;
    req.head         = w.head;
    req.sector       = w.sector;
    req.retries      = 3;
    req.verify       = w.verify;
    req.device_path  = m_device_path;
    req.total_lba    = m_total_lba;
    req.block_size   = m_block_size;
    req.sector_bytes.assign(payload.bytes.begin(), payload.bytes.end());

    UsbFloppyWriteResult result = m_write_runner(req);

    return translate_write_result(result, w.cylinder, w.head, payload);
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: detectDrive() + autoDetectDevice() + connect() in
 *  usbfloppyhardwareprovider.cpp.
 *
 *  V1 approach:
 *    detectDrive():
 *      1. If m_devicePath empty, default to /dev/sg0 or \\\\.\\A:
 *      2. Call uft_ufi_inquiry() → populate vendor/product strings
 *      3. Emit driveDetected(info) with hardcoded 80-track, 2-head geometry
 *
 *    connect():
 *      1. Call uft_ufi_test_unit_ready() — check disk inserted
 *      2. Call uft_ufi_read_capacity() — get LBA count + block size
 *      3. Store total_lba, block_size for readTrack()
 *
 *  The V2 collapses both into do_detect_drive() via the injected
 *  UsbFloppyDetectRunner, which wraps INQUIRY + TEST UNIT READY +
 *  READ CAPACITY. On success, geometry is stored in m_total_lba and
 *  m_block_size for subsequent do_read_sector() / do_write_sector() calls.
 *
 *  Drive geometry is inferred from total_lba (same logic as V1):
 *    2880 LBAs → 3.5" HD 1.44M  (80 cyl × 2 heads × 18 spt × 512 B)
 *    1440 LBAs → 3.5" DD 720K   (80 cyl × 2 heads × 9 spt × 512 B)
 *    5760 LBAs → 3.5" ED 2.88M  (80 cyl × 2 heads × 36 spt × 512 B)
 *    else      → 3.5" HD assumed (80 cyl × 2 heads × 18 spt × 512 B)
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome USBFloppyProviderV2::do_detect_drive()
{
    if (!m_detect_runner) {
        return null_runner_error("drive detection");
    }

    UsbFloppyDetectResult result = m_detect_runner();

    /* Backend not available */
    if (result.backend_unavailable) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy drive detection failed: UFI backend not available",
            "The UFI C-HAL backend is not registered. uft_ufi_backend_init() "
            "must be called before any UFI operation including INQUIRY.",
            "Call uft_ufi_backend_init() at application startup to register the "
            "platform backend (Linux SG_IO or Windows SCSI_PASS_THROUGH). "
            "See ufi.h for backend registration API (uft_ufi_set_backend)."
        };
    }

    /* Device error — path missing or inaccessible */
    if (result.device_error) {
        std::string why = "The USB floppy device could not be opened for INQUIRY.";
        if (!result.error_message.empty()) {
            why += " Backend error: " + result.error_message;
        }
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy drive detection failed: device path not accessible",
            why,
            "Verify that a USB floppy drive is connected. Set the device path "
            "via set_device_path() before calling detect_drive(). "
            "On Linux, the default path is '/dev/sg0' or '/dev/sg1'. "
            "On Windows, use '\\\\.\\A:'. Ensure the process has permission "
            "to open SCSI pass-through devices."
        };
    }

    /* No medium / not ready */
    if (result.no_disk) {
        DriveAbsent absent;
        absent.scanned_for = "USB Floppy drive (UFI/USB Mass Storage) at '"
                           + (m_device_path.empty()
                              ? std::string("(no path set)")
                              : m_device_path) + "'";
        return absent;
    }

    /* Drive probe returned not found with an error message */
    if (!result.found && !result.error_message.empty()) {
        return ProviderError{
            UFT_E_GENERIC,
            "USB Floppy drive detection failed",
            "The USB Floppy detect runner returned an error: "
                + result.error_message,
            "Verify that the USB floppy drive is connected and powered. "
            "Check the device path (set_device_path()). "
            "On Linux, ensure the usb-storage kernel module is loaded and "
            "that the user has read/write permission on the sg device. "
            "On Windows, verify the drive letter is accessible with admin privileges."
        };
    }

    /* Clean probe — device simply not present (no error message) */
    if (!result.found) {
        DriveAbsent absent;
        absent.scanned_for = "USB Floppy drive (UFI/USB Mass Storage) at '"
                           + (m_device_path.empty()
                              ? std::string("(no path set — set_device_path() required)")
                              : m_device_path) + "'";
        return absent;
    }

    /* Drive detected — populate DriveDetected and update geometry cache */
    m_total_lba  = result.total_lba;
    m_block_size = result.block_size ? result.block_size : 512u;

    DriveDetected detected;

    /* Vendor + product string — same as V1's m_vendorInfo */
    std::string vp = result.vendor;
    if (!result.product.empty()) {
        if (!vp.empty()) vp += " ";
        vp += result.product;
    }
    if (vp.empty()) vp = "USB Floppy Drive (vendor not reported)";

    /* Infer geometry from total_lba — mirrors V1's spt calculation */
    int spt   = 18;
    int heads = 2;
    int cyls  = 80;
    double rpm = 300.0;
    std::string format_str = "3.5\" HD 1.44M";

    if (result.total_lba == 2880) {
        spt = 18; format_str = "3.5\" HD 1.44M"; /* 80 × 2 × 18 × 512 = 1.44MB */
    } else if (result.total_lba == 1440) {
        spt = 9;  format_str = "3.5\" DD 720K";  /* 80 × 2 × 9 × 512 = 720KB  */
    } else if (result.total_lba == 5760) {
        spt = 36; format_str = "3.5\" ED 2.88M"; /* 80 × 2 × 36 × 512 = 2.88MB */
    } else if (result.total_lba > 0) {
        /* Non-standard geometry: compute spt from total LBAs */
        spt = static_cast<int>(result.total_lba / (cyls * heads));
        if (spt < 1) spt = 18;
        format_str = "3.5\" USB Floppy (" + std::to_string(result.total_lba) + " LBAs)";
    }
    (void)spt;  /* spt stored in runners via m_total_lba for LBA calculation */

    detected.drive_kind  = vp + " — " + format_str;
    detected.tracks      = cyls;
    detected.heads       = heads;
    detected.rpm_nominal = rpm;

    /* Firmware from INQUIRY revision field */
    if (!result.revision.empty()) {
        detected.firmware = result.revision;
    } else {
        detected.firmware = "USB Floppy (firmware revision not reported by INQUIRY)";
    }

    return detected;
}

}  // namespace uft::hal
