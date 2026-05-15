/**
 * @file usbfloppy_provider_v2.h
 * @brief USBFloppyProviderV2 — mixin-composed V2 HAL provider (MF-168 / P1.15).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities:
 *   ReadsSectors   v  do_read_sector()   -> SectorOutcome
 *   WritesSectors  v  do_write_sector()  -> WriteOutcome
 *   DetectsDrive   v  do_detect_drive()  -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   ReadsRawFlux    x  USB Floppy delivers decoded sectors only. The UFI
 *                      command set (USB Mass Storage Class UFI Subclass,
 *                      "USB Floppy Interface Command Specification Rev. 1.0")
 *                      defines READ(10), WRITE(10), INQUIRY, etc. — all
 *                      sector-level SCSI-like commands. There is no opcode
 *                      for raw magnetic flux capture. The V1 provider has no
 *                      readRawFlux() path at all. The compile error on
 *                      `provider.read_raw_flux(...)` IS the documentation
 *                      (anti-pragmatism rule). This parallels FC5025 P1.11.
 *   WritesRawFlux   x  Same rationale as ReadsRawFlux. No flux write opcode
 *                      in the UFI command set. The USB floppy controller
 *                      (Panasonic/TEAC chipset on most USB adapters) handles
 *                      MFM encoding internally; the host only issues logical
 *                      sector addresses.
 *   ControlsMotor   x  The UFI START_STOP UNIT command (0x1B) can request
 *                      motor start/stop, but the V1 provider does not expose
 *                      it and the UFI C-HAL (`ufi.h`) has no `uft_ufi_start_stop()`
 *                      function. Per the anti-pragmatism rule: "either the
 *                      hardware can do it — then it gets a mixin — or it
 *                      cannot." The current backend surface does not expose it.
 *                      START_STOP can be added as a future capability when
 *                      `uft_ufi_start_stop()` is added to the C-HAL. Omitting
 *                      now is the honest choice.
 *   SeeksHead       x  The UFI command set abstracts head positioning: READ(10)
 *                      and WRITE(10) take LBA addresses and the controller
 *                      firmware handles CHS translation and seeking internally.
 *                      There is no standalone SEEK command in the UFI spec.
 *                      The V1 has no seekCylinder() path. Omitting this mixin
 *                      is the structurally honest choice.
 *   Recalibrates    x  No SEEK_TO_TRACK_0 equivalent in the UFI command set.
 *                      The USB floppy controller recalibrates implicitly on
 *                      power-up and on TEST UNIT READY. Omitting is correct.
 *   MeasuresRPM     x  The UFI command set has no RPM measurement opcode.
 *                      USB floppy drives (1.44M DSHD) always run at 300 RPM
 *                      for 3.5" and the controller does not expose timing data.
 *                      Any implementation would be a hardcoded constant — the
 *                      silent-stub pattern that the refactor explicitly rejects.
 *
 * SpecStatus: VendorDocumented — The UFI command set is documented in:
 *   "Universal Serial Bus Mass Storage Class UFI Command Specification Rev.1.0"
 *   published by the USB Implementers Forum (USB-IF), 1998. The specification
 *   is publicly available. The SCSI-like opcodes (INQUIRY, TEST UNIT READY,
 *   READ CAPACITY, READ(10), WRITE(10)) are the subset used by this provider.
 *
 * Backend: injectable runner pattern (two runners: sector-read + write, detect).
 *   The V1 USBFloppyHardwareProvider calls UFI C-HAL functions directly with a
 *   device path string (`const char*`). The UFI C-HAL (`ufi.c`) uses a vtable
 *   backend (`uft_ufi_ops_t`) registered by `uft_ufi_backend_init()`. In CI
 *   environments without a platform backend (no SG_IO on Linux or no Windows
 *   SCSI_PASS_THROUGH), calling the UFI functions results in `ensure_backend()`
 *   returning `UFT_ERR_NOT_IMPLEMENTED`.
 *
 *   Rather than calling the UFI C-HAL functions directly (which would require
 *   a platform-specific backend to be registered at test time), the V2 uses
 *   injectable runners that abstract the UFI path. In production, the caller
 *   provides runners that wrap the UFI C-HAL calls. In tests, scripted lambdas
 *   provide deterministic results without requiring a platform backend.
 *
 *   This mirrors XUM1541ProviderV2 (P1.12) and FC5025ProviderV2 (P1.11) —
 *   the same pattern for sector-level providers with C-HAL backends.
 *
 *   Runners:
 *     UsbFloppyReadRunner  — wraps uft_ufi_read_sectors()
 *     UsbFloppyWriteRunner — wraps uft_ufi_write_sectors()
 *     UsbFloppyDetectRunner — wraps uft_ufi_inquiry() + uft_ufi_read_capacity()
 *
 *   Note: The V1 does LBA calculation (cylinder * 2 + head) * spt internally.
 *   The V2 runner receives the same ReadSectorParams/WriteSectorParams and the
 *   runner is responsible for the LBA translation (or the production wrapper
 *   replicates the V1's spt logic). This keeps the provider agnostic of the
 *   UFI-level geometry mapping.
 *
 * Rule F-3 (divergent-read preservation):
 *   The V1 readTrack() issues one UFI read per track and either succeeds or
 *   fails — no multi-revolution retry loop. When the read fails, partial data
 *   (if any) is not preserved by the V1. The V2 `do_read_sector()` carries the
 *   V1 semantics forward: on success → SectorRead; on failure → ProviderError
 *   or SectorUnreadable (no-disk / not-ready cases). If the runner returns
 *   partial data (partial_sectors > 0 alongside an error), SectorMarginal is
 *   returned with divergent_reads.size() >= 2 (rule F-3). The V2 never
 *   collapses, substitutes, or discards whatever the backend delivers.
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * The V1 USBFloppyHardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef USBFLOPPY_PROVIDER_V2_H
#define USBFLOPPY_PROVIDER_V2_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/* ─────────────────────────────────────────────────────────────────────────
 *  USB Floppy sector-read request / result
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parameters for a single USB Floppy track read request.
 *
 * Carries cylinder, head, sector-count, and geometry hints so the runner
 * can calculate the LBA address and issue the UFI READ(10) command. The
 * runner encapsulates the CHS-to-LBA mapping (the V1's formula:
 * lba = (cylinder * 2 + head) * spt).
 */
struct UsbFloppyReadRequest {
    int cylinder     = 0;   /**< Physical cylinder number (0-based). */
    int head         = 0;   /**< Head (side) number: 0 or 1. */
    int sector       = -1;  /**< -1 = full track. Specific sector if >= 0. */
    int retries      = 3;   /**< Max retry count on I/O errors. */
    /** Device path string (e.g. "/dev/sg0" on Linux, "\\\\.\\A:" on Win32).
     *  Populated by the V2 from m_device_path before invoking the runner. */
    std::string device_path;
    /** Hint: total LBAs on disk (from prior read_capacity). 0 = unknown.
     *  Runner uses this to determine sectors-per-track (spt). */
    uint32_t total_lba   = 0;
    /** Hint: bytes per sector (from prior read_capacity). 0 = assume 512. */
    uint32_t block_size  = 0;
};

/**
 * @brief Result of a single USB Floppy track read.
 *
 * The runner fills this in. Maps to the V1's TrackData + return value.
 *
 * Rule F-3: when partial_sectors > 0, sector_bytes contains whatever the
 * UFI delivered before the error; the caller must preserve ALL of it.
 */
struct UsbFloppyReadResult {
    /** Raw sector data bytes as delivered by the UFI READ(10) command.
     *  Full track: spt * block_size bytes. Partial: fewer bytes. */
    std::vector<uint8_t> sector_bytes;

    /** Number of sectors successfully read. */
    int good_sectors = 0;

    /** Number of sectors that failed. Non-zero + non-empty sector_bytes
     *  → SectorMarginal (rule F-3). */
    int bad_sectors = 0;

    /** Total sectors expected on this track. 0 = unknown. */
    int total_sectors = 0;

    /** Human-readable error message on failure. */
    std::string error_message;

    /** True when the UFI backend returned "no medium / not ready" SENSE key.
     *  Non-retryable — surface as SectorUnreadable immediately. */
    bool no_disk = false;

    /** True when the UFI C-HAL backend is not registered (g_ops == NULL).
     *  Returned when uft_ufi_backend_init() was not called or failed. */
    bool backend_unavailable = false;

    /** True when the device path was empty or the device could not be opened. */
    bool device_error = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  USB Floppy sector-write request / result
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parameters for a single USB Floppy track write request.
 */
struct UsbFloppyWriteRequest {
    int cylinder     = 0;   /**< Physical cylinder number (0-based). */
    int head         = 0;   /**< Head (side) number: 0 or 1. */
    int sector       = -1;  /**< -1 = full track. Specific sector if >= 0. */
    int retries      = 3;   /**< Max retry count on I/O errors. */
    bool verify      = true; /**< Read back and verify after write. */

    /** Device path string. */
    std::string device_path;

    /** Geometry hints (from prior detect or read_capacity). */
    uint32_t total_lba  = 0;
    uint32_t block_size = 0;

    /** Data to write. */
    std::vector<uint8_t> sector_bytes;
};

/**
 * @brief Result of a single USB Floppy track write.
 */
struct UsbFloppyWriteResult {
    /** Number of sectors successfully written. */
    int sectors_written = 0;

    /** Total sectors attempted. */
    int total_sectors = 0;

    /** Human-readable error message on failure. */
    std::string error_message;

    /** True when the drive reports write-protect (SCSI sense 07/27/00). */
    bool write_protected = false;

    /** True when the UFI backend is not registered. */
    bool backend_unavailable = false;

    /** True when the device path was empty or device could not be opened. */
    bool device_error = false;

    /** True when verify was requested and the read-back differed from written data.
     *  Populate `intended` and `readback` when this is true (rule F-3). */
    bool verify_failed = false;

    /** The data that was written (for verify_failed path, rule F-3). */
    std::vector<uint8_t> intended;

    /** The data read back after write (for verify_failed path, rule F-3). */
    std::vector<uint8_t> readback;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  USB Floppy detect request / result
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of a USB Floppy detect / probe operation.
 *
 * The runner wraps uft_ufi_inquiry() + uft_ufi_read_capacity() to fill
 * in vendor/product strings and disk geometry.
 */
struct UsbFloppyDetectResult {
    /** True if the UFI INQUIRY and TEST UNIT READY commands succeeded. */
    bool found = false;

    /** Vendor string from INQUIRY (8 bytes, trimmed). */
    std::string vendor;

    /** Product string from INQUIRY (16 bytes, trimmed). */
    std::string product;

    /** Firmware revision from INQUIRY (4 bytes, trimmed). */
    std::string revision;

    /** Total LBA count from READ CAPACITY (0 = not queried). */
    uint32_t total_lba = 0;

    /** Block size in bytes from READ CAPACITY (0 = not queried; assume 512). */
    uint32_t block_size = 0;

    /** True when the UFI backend is not registered. */
    bool backend_unavailable = false;

    /** True when the device path was empty or could not be opened. */
    bool device_error = false;

    /** True when TEST UNIT READY returned "not ready / no medium". */
    bool no_disk = false;

    /** Human-readable error message when found == false. */
    std::string error_message;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  USBFloppyProviderV2 — the V2 mixin-composed provider
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief USB Floppy Drive V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"USB Floppy", SpecStatus::VendorDocumented>
 *   ReadsSectorsVia<USBFloppyProviderV2>
 *   WritesSectorsVia<USBFloppyProviderV2>
 *   DetectsDriveVia<USBFloppyProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class USBFloppyProviderV2 final
    : public mixin::Identity<"USB Floppy", SpecStatus::VendorDocumented>
    , public mixin::ReadsSectorsVia<USBFloppyProviderV2>
    , public mixin::WritesSectorsVia<USBFloppyProviderV2>
    , public mixin::DetectsDriveVia<USBFloppyProviderV2>
{
public:
    /**
     * @brief USB Floppy sector read runner function type.
     *
     * Accepts a UsbFloppyReadRequest (cylinder, head, retries, device path,
     * geometry hints). Returns a UsbFloppyReadResult.
     *
     * In production, wrap the UFI C-HAL:
     *   auto runner = [path, &total_lba, &block_size]
     *       (const UsbFloppyReadRequest& req) -> UsbFloppyReadResult {
     *       uft_diag_t diag = {};
     *       int spt = (req.total_lba == 2880) ? 18 :
     *                 (req.total_lba == 1440) ? 9 : 18;
     *       uint32_t lba = (uint32_t)(req.cylinder * 2 + req.head) * (uint32_t)spt;
     *       std::vector<uint8_t> buf(spt * req.block_size, 0);
     *       UsbFloppyReadResult r;
     *       r.total_sectors = spt;
     *       if (uft_ufi_read_sectors(req.device_path.c_str(), lba, (uint16_t)spt,
     *               buf.data(), buf.size(), &diag) == 0) {
     *           r.sector_bytes = std::move(buf);
     *           r.good_sectors = spt;
     *       } else {
     *           r.error_message = diag.msg;
     *       }
     *       return r;
     *   };
     *
     * In tests, inject a scripted lambda returning predetermined results.
     */
    using UsbFloppyReadRunner  = std::function<UsbFloppyReadResult(const UsbFloppyReadRequest&)>;

    /**
     * @brief USB Floppy sector write runner function type.
     *
     * Accepts a UsbFloppyWriteRequest (cylinder, head, retries, verify,
     * device path, geometry hints, sector_bytes). Returns UsbFloppyWriteResult.
     */
    using UsbFloppyWriteRunner = std::function<UsbFloppyWriteResult(const UsbFloppyWriteRequest&)>;

    /**
     * @brief USB Floppy detect runner function type.
     *
     * Probes via uft_ufi_inquiry() + uft_ufi_test_unit_ready() +
     * uft_ufi_read_capacity() for the device at the configured path.
     * Returns a UsbFloppyDetectResult.
     *
     * In production:
     *   auto detect_runner = [path]() -> UsbFloppyDetectResult {
     *       uft_diag_t diag = {};
     *       char vendor[9]={}, product[17]={}, rev[5]={};
     *       UsbFloppyDetectResult r;
     *       if (uft_ufi_inquiry(path, vendor, product, rev, &diag) != 0) {
     *           r.error_message = diag.msg;
     *           return r;
     *       }
     *       r.vendor = vendor; r.product = product; r.revision = rev;
     *       uint32_t lba=0, bs=0;
     *       uft_ufi_read_capacity(path, &lba, &bs, &diag);
     *       r.total_lba = lba; r.block_size = bs;
     *       r.found = true;
     *       return r;
     *   };
     *
     * In tests, return a scripted UsbFloppyDetectResult directly.
     */
    using UsbFloppyDetectRunner = std::function<UsbFloppyDetectResult()>;

    /**
     * @brief Construct from injectable runners.
     *
     * @param read_runner    Callable for sector reads. If null, every
     *                       do_read_sector() returns a ProviderError.
     * @param write_runner   Callable for sector writes. If null, every
     *                       do_write_sector() returns a ProviderError.
     * @param detect_runner  Callable for drive detection. If null, every
     *                       do_detect_drive() returns a ProviderError.
     * @param device_path    The OS device path (e.g. "/dev/sg0", "\\\\.\\A:").
     *                       Populated into requests before invoking the runner.
     *                       May be empty initially; set_device_path() can update.
     * @param max_cylinders  Maximum cylinder index (inclusive). Default 79.
     */
    explicit USBFloppyProviderV2(UsbFloppyReadRunner   read_runner,
                                  UsbFloppyWriteRunner  write_runner,
                                  UsbFloppyDetectRunner detect_runner,
                                  std::string           device_path  = "",
                                  int                   max_cylinders = 79);

    /* Non-copyable (holds std::function + state). */
    USBFloppyProviderV2(const USBFloppyProviderV2&)            = delete;
    USBFloppyProviderV2& operator=(const USBFloppyProviderV2&) = delete;

    /* Movable. */
    USBFloppyProviderV2(USBFloppyProviderV2&&)            = default;
    USBFloppyProviderV2& operator=(USBFloppyProviderV2&&) = default;

    ~USBFloppyProviderV2() = default;

    /**
     * @brief Update the OS device path used for subsequent operations.
     *
     * The V2 equivalent of V1's setDevicePath(). The new path is included
     * in all runner requests from this point on.
     *
     * @param path  OS device path (e.g. "/dev/sg0", "\\\\.\\A:").
     */
    void set_device_path(const std::string& path) noexcept;

    /**
     * @brief Update the geometry hints used by the runners.
     *
     * Called after a successful detect operation to give the runners
     * accurate geometry for LBA calculation. Mirrors V1's connect() which
     * stored total_lba and block_size after uft_ufi_read_capacity().
     */
    void set_geometry(uint32_t total_lba, uint32_t block_size) noexcept;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    SectorOutcome do_read_sector (const ReadSectorParams& p);
    WriteOutcome  do_write_sector(const WriteSectorParams& w, const SectorPayload& payload);
    DetectOutcome do_detect_drive();

private:
    UsbFloppyReadRunner   m_read_runner;    /**< Sector-read runner (injected). */
    UsbFloppyWriteRunner  m_write_runner;   /**< Sector-write runner (injected). */
    UsbFloppyDetectRunner m_detect_runner;  /**< Drive-detect runner (injected). */
    std::string           m_device_path;   /**< OS device path. */
    int                   m_max_cylinders; /**< Maximum valid cylinder index. */
    uint32_t              m_total_lba;     /**< Geometry hint from detect. */
    uint32_t              m_block_size;    /**< Block size from detect (0 = assume 512). */

    /** Return a ProviderError for a null-runner condition. */
    static ProviderError null_runner_error(const char* operation);

    /** Return a ProviderError for a geometry range violation. */
    ProviderError range_error(int cylinder, int head) const;

    /**
     * @brief Translate a failed UsbFloppyReadResult into a SectorOutcome.
     *
     * Covers: backend_unavailable, device_error, no_disk, and I/O failures.
     */
    static SectorOutcome translate_read_failure(const UsbFloppyReadResult& result,
                                                 int cylinder, int head,
                                                 int retries);

    /**
     * @brief Translate a successful (or partial) UsbFloppyReadResult into a
     *        SectorRead or SectorMarginal.
     *
     * Rule F-3: when bad_sectors > 0, partial sector data is preserved in
     * SectorMarginal::divergent_reads with at least 2 entries.
     */
    static SectorOutcome translate_read_result(const UsbFloppyReadResult& result,
                                                int cylinder, int head);

    /**
     * @brief Translate a UsbFloppyWriteResult into a WriteOutcome.
     */
    static WriteOutcome translate_write_result(const UsbFloppyWriteResult& result,
                                                int cylinder, int head,
                                                const SectorPayload& payload);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy HasIdentity");
static_assert(ReadsSectors<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy ReadsSectors "
    "(USB Floppy reads sectors via UFI READ(10) over USB Mass Storage)");
static_assert(WritesSectors<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy WritesSectors "
    "(USB Floppy writes sectors via UFI WRITE(10) over USB Mass Storage)");
static_assert(DetectsDrive<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy DetectsDrive "
    "(USB Floppy can be detected via UFI INQUIRY + TEST UNIT READY)");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsRawFlux<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy ReadsRawFlux "
    "(UFI command set delivers decoded sectors only — no flux capture opcode; "
    "parallels FC5025 P1.11 hard rule)");
static_assert(!WritesRawFlux<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy WritesRawFlux "
    "(no flux write opcode in UFI spec; MFM encoding is internal to controller)");
static_assert(!ControlsMotor<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy ControlsMotor "
    "(UFI START_STOP UNIT opcode exists in the spec but is not exposed by the "
    "current UFI C-HAL surface: uft_ufi_start_stop() does not exist in ufi.h. "
    "Anti-pragmatism rule: omit until the backend surface exposes it cleanly)");
static_assert(!SeeksHead<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy SeeksHead "
    "(UFI command set has no standalone seek opcode; head positioning is "
    "abstracted into READ(10)/WRITE(10) LBA addressing by controller firmware)");
static_assert(!Recalibrates<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy Recalibrates "
    "(no SEEK_TO_TRACK_0 equivalent in UFI spec; USB floppy controllers "
    "recalibrate implicitly on power-up and TEST UNIT READY)");
static_assert(!MeasuresRPM<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy MeasuresRPM "
    "(UFI has no RPM measurement opcode; 3.5\" USB floppies always run at "
    "300 RPM and the controller does not expose timing data — any implementation "
    "would be a hardcoded constant, the silent-stub pattern the refactor rejects)");

/* Composite predicates. */
static_assert(ImagesSectors<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy ImagesSectors "
    "(has both ReadsSectors and DetectsDrive)");
static_assert(WritesAnything<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy WritesAnything "
    "(has WritesSectors via UFI WRITE(10))");
static_assert(!FullDriveControl<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all absent — UFI abstracts "
    "drive mechanics at the firmware level)");
static_assert(!ImagesFlux<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy ImagesFlux "
    "(USB Floppy is sector-only; no flux capture capability in UFI spec)");

}  // namespace uft::hal

#endif  // USBFLOPPY_PROVIDER_V2_H
