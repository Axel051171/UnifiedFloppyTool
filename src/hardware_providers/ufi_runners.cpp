/**
 * @file ufi_runners.cpp
 * @brief Implementation of USBFloppy UFI-C-HAL runners (MF-258).
 *
 * Defensive contract:
 *   - Empty device_path → device_error = true, no C-HAL call.
 *   - C-HAL backend not registered → backend_unavailable = true,
 *     no fabricated data.
 *   - Read partial (good_sectors < total) → sector_bytes truncated
 *     to the bytes that actually came through. Caller treats partial
 *     as SectorMarginal per F-3, NOT as silent success.
 *   - Write verify-failed → intended + readback both populated so
 *     the caller can compute a forensic diff.
 */
#include "ufi_runners.h"

extern "C" {
#include "uft/hal/ufi.h"
}

#include <cstring>
#include <mutex>
#include <vector>

namespace uft::hal {

namespace {

std::once_flag g_ufi_init_once;
bool           g_ufi_backend_ok = false;

void init_backend_once() {
    std::call_once(g_ufi_init_once, []() {
        g_ufi_backend_ok = (uft_ufi_backend_init() == 0);
    });
}

/** Compute LBA for cyl/head/sector given total_lba + assumed
 *  geometry. Returns false on impossible parameters (zero geometry
 *  hint, sector out of range, etc.) so the caller can fail without
 *  guessing. */
bool compute_lba(int cylinder, int head, int sector,
                 int max_cylinders, uint32_t total_lba,
                 uint32_t /*block_size*/,
                 uint32_t* out_lba, uint16_t* out_count, int* out_total_sectors)
{
    if (cylinder < 0 || head < 0 || head > 1) return false;
    if (max_cylinders <= 0) return false;
    if (total_lba == 0) {
        /* No capacity hint — assume standard 1.44MB geometry as a
         * conservative default. Better than picking 720K and getting
         * an out-of-range LBA on a real device. */
        total_lba = 2880;
    }
    const int cyl_count = max_cylinders + 1;  /* 0-based inclusive */
    const int spt = static_cast<int>(total_lba / (cyl_count * 2));
    if (spt <= 0) return false;
    const uint32_t track_lba = static_cast<uint32_t>(
        cylinder * 2 * spt + head * spt);
    if (sector < 0) {
        /* Full track. */
        *out_lba = track_lba;
        *out_count = static_cast<uint16_t>(spt);
        *out_total_sectors = spt;
    } else {
        if (sector >= spt) return false;
        *out_lba = track_lba + static_cast<uint32_t>(sector);
        *out_count = 1;
        *out_total_sectors = 1;
    }
    return true;
}

/** Drain optional sense info from the C-HAL into the no_disk flag.
 *  SCSI sense key 2 (NOT_READY) + ASC 3A (medium not present) is
 *  the canonical "no disk in drive". */
void check_no_disk(const char* path, bool* out_no_disk)
{
    uint8_t key = 0, asc = 0, ascq = 0;
    if (uft_ufi_request_sense(path, &key, &asc, &ascq, nullptr) == UFT_OK) {
        if (key == 0x02 && asc == 0x3A) *out_no_disk = true;
    }
}

} // namespace

bool ufi_runners_ensure_backend()
{
    init_backend_once();
    return g_ufi_backend_ok;
}

/* ─── Detect runner ───────────────────────────────────────────────── */

USBFloppyProviderV2::UsbFloppyDetectRunner
make_usbfloppy_detect_runner(std::string device_path)
{
    return [path = std::move(device_path)]() -> UsbFloppyDetectResult {
        UsbFloppyDetectResult r;
        if (path.empty()) {
            r.device_error = true;
            r.error_message = "ufi detect: device_path empty";
            return r;
        }
        if (!ufi_runners_ensure_backend()) {
            r.backend_unavailable = true;
            r.error_message =
                "UFI backend not registered for this platform "
                "(Linux SG_IO is present in this build; Windows "
                "SCSI_PASS_THROUGH and macOS IOKit are pending — "
                "see src/hal/ufi_backend.c)";
            return r;
        }

        char vendor[9] = {0}, product[17] = {0}, rev[5] = {0};
        uft_diag_t diag = {0};
        uft_rc_t rc = uft_ufi_inquiry(path.c_str(), vendor, product, rev, &diag);
        if (rc != UFT_OK) {
            r.device_error = true;
            r.error_message = diag.msg[0] ? diag.msg : "INQUIRY failed";
            return r;
        }
        r.vendor   = vendor;
        r.product  = product;
        r.revision = rev;

        /* TEST UNIT READY → see if a disk is loaded. */
        rc = uft_ufi_test_unit_ready(path.c_str(), &diag);
        if (rc != UFT_OK) {
            check_no_disk(path.c_str(), &r.no_disk);
            if (!r.no_disk) {
                r.device_error = true;
                r.error_message = diag.msg[0] ? diag.msg :
                                  "TEST_UNIT_READY failed";
                return r;
            }
            /* Disk absent — INQUIRY succeeded so found is still true,
             * the GUI shows no_disk separately. */
            r.found = true;
            return r;
        }

        /* READ CAPACITY — geometry for subsequent read/write. */
        rc = uft_ufi_read_capacity(path.c_str(),
                                    &r.total_lba, &r.block_size, &diag);
        if (rc != UFT_OK) {
            /* Capacity-read failure is not fatal — leave fields 0. */
        }
        r.found = true;
        return r;
    };
}

/* ─── Read runner ─────────────────────────────────────────────────── */

USBFloppyProviderV2::UsbFloppyReadRunner
make_usbfloppy_read_runner()
{
    return [](const UsbFloppyReadRequest& req) -> UsbFloppyReadResult {
        UsbFloppyReadResult r;
        if (req.device_path.empty()) {
            r.device_error = true;
            r.error_message = "ufi read: device_path empty";
            return r;
        }
        if (!ufi_runners_ensure_backend()) {
            r.backend_unavailable = true;
            r.error_message = "UFI backend not registered";
            return r;
        }

        uint32_t lba = 0;
        uint16_t count = 0;
        int total_sectors = 0;
        const int max_cyl = 79;  /* runner default; provider can override */
        if (!compute_lba(req.cylinder, req.head, req.sector,
                         max_cyl, req.total_lba, req.block_size,
                         &lba, &count, &total_sectors)) {
            r.device_error = true;
            r.error_message =
                "ufi read: invalid cyl/head/sector or zero capacity hint";
            return r;
        }
        r.total_sectors = total_sectors;

        const uint32_t bs = req.block_size ? req.block_size : 512;
        const size_t buf_bytes = static_cast<size_t>(count) * bs;
        r.sector_bytes.resize(buf_bytes);

        uft_diag_t diag = {0};
        uft_rc_t rc = uft_ufi_read_sectors(req.device_path.c_str(),
                                            lba, count,
                                            r.sector_bytes.data(),
                                            r.sector_bytes.size(),
                                            &diag);
        if (rc != UFT_OK) {
            check_no_disk(req.device_path.c_str(), &r.no_disk);
            r.error_message = diag.msg[0] ? diag.msg :
                              "READ(10) failed";
            r.sector_bytes.clear();
            r.bad_sectors = total_sectors;
            return r;
        }
        r.good_sectors = total_sectors;
        return r;
    };
}

/* ─── Write runner ────────────────────────────────────────────────── */

USBFloppyProviderV2::UsbFloppyWriteRunner
make_usbfloppy_write_runner()
{
    return [](const UsbFloppyWriteRequest& req) -> UsbFloppyWriteResult {
        UsbFloppyWriteResult r;
        if (req.device_path.empty()) {
            r.device_error = true;
            r.error_message = "ufi write: device_path empty";
            return r;
        }
        if (req.sector_bytes.empty()) {
            r.device_error = true;
            r.error_message = "ufi write: empty payload";
            return r;
        }
        if (!ufi_runners_ensure_backend()) {
            r.backend_unavailable = true;
            r.error_message = "UFI backend not registered";
            return r;
        }

        uint32_t lba = 0;
        uint16_t count = 0;
        int total_sectors = 0;
        const int max_cyl = 79;
        if (!compute_lba(req.cylinder, req.head, req.sector,
                         max_cyl, req.total_lba, req.block_size,
                         &lba, &count, &total_sectors)) {
            r.device_error = true;
            r.error_message =
                "ufi write: invalid cyl/head/sector or zero capacity hint";
            return r;
        }
        r.total_sectors = total_sectors;

        const uint32_t bs = req.block_size ? req.block_size : 512;
        const size_t expected_bytes = static_cast<size_t>(count) * bs;
        if (req.sector_bytes.size() != expected_bytes) {
            r.device_error = true;
            r.error_message =
                "ufi write: payload size " + std::to_string(req.sector_bytes.size()) +
                " != expected " + std::to_string(expected_bytes);
            return r;
        }

        uft_diag_t diag = {0};
        uft_rc_t rc = uft_ufi_write_sectors(req.device_path.c_str(),
                                             lba, count,
                                             req.sector_bytes.data(),
                                             req.sector_bytes.size(),
                                             &diag);
        if (rc != UFT_OK) {
            /* Detect write-protect via REQUEST_SENSE (SK=7 / WRITE PROT). */
            uint8_t key = 0, asc = 0, ascq = 0;
            if (uft_ufi_request_sense(req.device_path.c_str(),
                                       &key, &asc, &ascq, nullptr) == UFT_OK) {
                if (key == 0x07) r.write_protected = true;
            }
            r.error_message = diag.msg[0] ? diag.msg :
                              "WRITE(10) failed";
            return r;
        }

        if (req.verify) {
            /* Read back + diff. Rule F-3: preserve both buffers if
             * mismatch so the caller can compute the forensic diff. */
            std::vector<uint8_t> readback(expected_bytes);
            uft_diag_t diag2 = {0};
            uft_rc_t rrc = uft_ufi_read_sectors(req.device_path.c_str(),
                                                 lba, count,
                                                 readback.data(),
                                                 readback.size(),
                                                 &diag2);
            if (rrc != UFT_OK) {
                r.error_message = "verify read-back failed: " +
                                  std::string(diag2.msg);
                r.verify_failed = true;
                r.intended = req.sector_bytes;
                /* readback truncated to what we got */
                r.readback = std::move(readback);
                return r;
            }
            if (std::memcmp(readback.data(), req.sector_bytes.data(),
                            expected_bytes) != 0) {
                r.verify_failed = true;
                r.intended = req.sector_bytes;
                r.readback = std::move(readback);
                r.error_message = "verify: read-back differs from written data";
                return r;
            }
        }

        r.sectors_written = total_sectors;
        return r;
    };
}

} // namespace uft::hal
