/**
 * @file xum1541_provider_v2.h
 * @brief XUM1541ProviderV2 — mixin-composed V2 HAL provider (MF-165 / P1.12).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities:
 *   ReadsSectors   v  do_read_sector()   -> SectorOutcome
 *   WritesSectors  v  do_write_sector()  -> WriteOutcome
 *   DetectsDrive   v  do_detect_drive()  -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   ReadsRawFlux    x  XUM1541 is a sector-level device. It communicates via
 *                      the CBM IEC/IEEE-488 bus using CBM DOS block commands
 *                      (U1/U2). It does NOT expose raw flux transitions. The
 *                      V1 readRawFlux() is explicitly not a real flux reader —
 *                      it calls readTrack() internally and emits an operationError
 *                      stating "Raw GCR read not yet supported. Nibtools
 *                      integration required for raw flux capture." This is a
 *                      silent stub, not a capability. Omitting this mixin is the
 *                      structurally honest choice (anti-pragmatism rule).
 *   WritesRawFlux   x  V1 writeRawFlux() is a pure Q_UNUSED stub: all three
 *                      arguments are Q_UNUSED'd and the function immediately
 *                      returns false with operationError("Raw flux write not yet
 *                      supported."). There is no real implementation path at all.
 *   ControlsMotor   x  V1 setMotor() is a silent stub: Q_UNUSED(on); return
 *                      m_connected. The code comment explains why: "CBM drives
 *                      spin up automatically when accessed." The IEC bus provides
 *                      no standalone motor-control primitive. Omitting this mixin
 *                      is the structurally honest choice.
 *   SeeksHead       x  V1 seekCylinder() only updates m_currentCylinder — it
 *                      does not issue any IEC command. The IEC protocol has no
 *                      standalone seek; the drive positions the head in response
 *                      to block-read (U1) or block-write (U2) commands. V1 code
 *                      comment: "On IEC, there is no explicit seek command."
 *                      Omitting this mixin is the structurally honest choice.
 *   Recalibrates    x  V1 recalibrate() calls sendDriveCommand("I") which sends
 *                      the CBM DOS Initialize command to the drive. While this
 *                      does cause a head seek to track 18 (the directory track),
 *                      it is NOT a recalibration to track 0. The Initialize
 *                      command's primary purpose is resetting CBM DOS state
 *                      after direct-buffer operations. It does not correspond to
 *                      what SeekTrack0Failed / SeekArrived{cylinder=0} semantics
 *                      require for Recalibrates. Mapping this would be misleading.
 *   MeasuresRPM     x  V1 measureRPM() returns a hardcoded constant (300.0) with
 *                      comment: "CBM drives run at approximately 300 RPM. Precise
 *                      measurement would require nibtools-style timing analysis."
 *                      This is a silent stub, not a real measurement.
 *
 * SpecStatus: VendorDocumented — The XUM1541/ZoomFloppy firmware and protocol
 *   are documented by RETRO Innovations (Jim Brain) and the OpenCBM project.
 *   The CBM IEC/IEEE-488 bus protocol is documented in the Commodore 1541
 *   User's Guide and Service Manual (1982). The U1/U2 block-read/write
 *   commands are documented in Commodore DOS manuals and the OpenCBM SDK.
 *   OpenCBM is open source: https://github.com/OpenCBM/OpenCBM.
 *   ZoomFloppy schematics: http://www.retroinnovations.com/zoomfloppy.html.
 *
 * Backend: single injectable runner (OpenCBM path) + detect runner.
 *   The V1 XUM1541HardwareProvider has two communication paths:
 *     1. OpenCBM library path (preferred, dynamically loaded at runtime)
 *        — full IEC bus access via cbm_listen/talk/raw_read/raw_write +
 *          U1/U2 CBM DOS block commands.
 *     2. Direct USB fallback — explicitly a structured stub (usbOpen()
 *        always returns false with a TODO comment; all USB transfer
 *        functions are Q_UNUSED stubs). This path was never implemented.
 *
 *   Since the direct-USB path is a never-implemented stub and the only
 *   REAL transport is OpenCBM, the V2 uses a single runner for sector
 *   operations rather than the FC5025 dual-runner pattern. The runner
 *   abstracts the OpenCBM IEC U1/U2 block commands. In production, wrap
 *   the OpenCBM-backed logic. In tests, inject a scripted runner lambda.
 *
 *   Backend honesty (M3.2 scaffold):
 *   The uft_xum1541.c C-HAL (M3.2 partial scaffold) is available but has
 *   13 stub functions that return UFT_ERR_NOT_IMPLEMENTED (USB I/O pending
 *   libusb wiring). The V2 provider does NOT call into this C-HAL because
 *   the V1's REAL path goes through the OpenCBM dynamic-library loader
 *   (xum1541_usb.h::OpenCbmLibrary), not through the uft_xum1541.c C-HAL.
 *   When the runner is null or fails, do_*() returns ProviderError with an
 *   M3.2 marker — same honest-scaffold pattern as SCP M3.1 and FC5025 P1.11.
 *
 *   Design choice — single runner vs dual runner:
 *     FC5025 used two runners (read_runner + detect_runner) because the
 *     detect path (fcimage --version probe vs USB VID:PID enumeration) is
 *     structurally different from the sector read path (CBW commands).
 *     XUM1541 uses TWO separate runner types as well because:
 *       (a) detect via OpenCBM cbm_driver_open + cbm_identify is structurally
 *           different from the U1/U2 block-read path, and
 *       (b) write uses the U2 command path which is distinct from the U1 read
 *           path (different retry logic + write-protect check + verify option).
 *     Two runners (Xum1541Runner for read, Xum1541WriteRunner for write) plus
 *     Xum1541DetectRunner for detect — three in total.
 *
 * Rule F-3 (multi-revolution / divergent-read preservation):
 *   XUM1541 is a sector-level device. The V1 readTrack() performs a per-sector
 *   retry loop, preserving partial data when some sectors succeed and others
 *   fail (goodSectors vs badSectors counts). When badSectors > 0 the partial
 *   sector data is NOT discarded — the track buffer contains the successful
 *   sectors and zeroed entries for failed ones. The V2 carries this forward:
 *   when the runner reports partial_sectors > 0, the outcome is SectorMarginal
 *   with ALL partial data preserved in SectorMarginal::divergent_reads. The
 *   two entries (good-sector data + a failed-sector sentinel) satisfy the
 *   conformance invariant SectorMarginal::divergent_reads.size() >= 2.
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * The V1 XUM1541HardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef XUM1541_PROVIDER_V2_H
#define XUM1541_PROVIDER_V2_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/* ─────────────────────────────────────────────────────────────────────────
 *  XUM1541 sector-read request / result
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parameters for a single XUM1541 track read request.
 *
 * Carries enough information for the OpenCBM IEC U1 block-read path.
 * The runner translates (cylinder, head) to CBM track number internally.
 */
struct Xum1541ReadRequest {
    int cylinder     = 0;  /**< Physical cylinder number (0-based). */
    int head         = 0;  /**< Head (side) number: 0 or 1. */
    int retries      = 3;  /**< Max retry count per sector. */
    int device_num   = 8;  /**< CBM device number (4-30, default 8). */
    int drive_type   = 0;  /**< uft_cbm_drive_t value (0 = auto). */
};

/**
 * @brief Result of a single XUM1541 track read.
 *
 * The runner fills this in. Maps to the V1's TrackData return value
 * with goodSectors / badSectors split.
 *
 * Rule F-3: when bad_sectors > 0, sector_bytes contains the partial
 * data received for the good sectors; the caller must preserve ALL
 * of it, not just the final good-sector bytes.
 */
struct Xum1541ReadResult {
    /** Raw sector data bytes: numSectors * 256. Good sectors are populated;
     *  failed-sector slots are zeroed. */
    std::vector<uint8_t> sector_bytes;

    /** Number of sectors that read successfully. */
    int good_sectors = 0;

    /** Number of sectors that failed all retries. */
    int bad_sectors = 0;

    /** Total sectors expected on this track (from drive geometry table). */
    int total_sectors = 0;

    /** CBM track number used (cylinder + 1, adjusted for 1571 side 1). */
    int cbm_track = 0;

    /** Human-readable error message when the read operation itself fails
     *  (e.g. IEC LISTEN failed, OpenCBM not available). */
    std::string error_message;

    /** True when OpenCBM was not available (runner could not open the driver). */
    bool opencbm_unavailable = false;

    /** True when the track number is out of range for the drive type. */
    bool invalid_track = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  XUM1541 sector-write request / result
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parameters for a single XUM1541 track write request.
 */
struct Xum1541WriteRequest {
    int cylinder     = 0;  /**< Physical cylinder number (0-based). */
    int head         = 0;  /**< Head (side) number: 0 or 1. */
    int retries      = 3;  /**< Max retry count per sector. */
    int device_num   = 8;  /**< CBM device number (4-30, default 8). */
    int drive_type   = 0;  /**< uft_cbm_drive_t value (0 = auto). */
    bool verify      = true; /**< Read back and verify after write. */

    /** Data to write: must be numSectors * 256 bytes for this track. */
    std::vector<uint8_t> sector_bytes;
};

/**
 * @brief Result of a single XUM1541 track write.
 */
struct Xum1541WriteResult {
    /** Number of sectors successfully written (and verified, if verify=true). */
    int sectors_written = 0;

    /** Total sectors attempted. */
    int total_sectors = 0;

    /** Retries actually consumed. */
    int retries_used = 0;

    /** CBM track number used. */
    int cbm_track = 0;

    /** Human-readable error when the write fails. */
    std::string error_message;

    /** True if the disk reports write-protect (CBM status 26). */
    bool write_protected = false;

    /** True when OpenCBM was not available. */
    bool opencbm_unavailable = false;

    /** True when track / data size validation failed. */
    bool invalid_params = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  XUM1541 detect request / result
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Result of an XUM1541 detect / probe operation.
 *
 * The runner attempts cbm_driver_open + cbm_identify. If OpenCBM is not
 * installed, found = false and error_message explains what to install.
 */
struct Xum1541DetectResult {
    /** True if an XUM1541/ZoomFloppy device was found via OpenCBM. */
    bool found = false;

    /** CBM drive type string (e.g. "Commodore 1541"). */
    std::string drive_type_name;

    /** Number of tracks for the detected drive type. */
    int tracks = 0;

    /** Number of sides for the detected drive type. */
    int heads = 1;

    /** Firmware version string (from XUM1541 USB firmware query). */
    std::string firmware;

    /** Transport used: "OpenCBM" or "USB direct" or "" if unavailable. */
    std::string transport;

    /** Error message when found == false (e.g. "OpenCBM not installed"). */
    std::string error_message;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  XUM1541ProviderV2 — the V2 mixin-composed provider
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief XUM1541/ZoomFloppy V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"XUM1541", SpecStatus::VendorDocumented>
 *   ReadsSectorsVia<XUM1541ProviderV2>
 *   WritesSectorsVia<XUM1541ProviderV2>
 *   DetectsDriveVia<XUM1541ProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class XUM1541ProviderV2 final
    : public mixin::Identity<"XUM1541", SpecStatus::VendorDocumented>
    , public mixin::ReadsSectorsVia<XUM1541ProviderV2>
    , public mixin::WritesSectorsVia<XUM1541ProviderV2>
    , public mixin::DetectsDriveVia<XUM1541ProviderV2>
{
public:
    /**
     * @brief XUM1541 sector read runner function type.
     *
     * Accepts an Xum1541ReadRequest (cylinder, head, retries, device, type).
     * Returns an Xum1541ReadResult (sector_bytes, good/bad sector counts, etc.).
     *
     * In production, wrap the OpenCBM dynamic-library path:
     *   auto runner = [&cbm_funcs, &cbm_handle, &device_num, &drive_type]
     *       (const Xum1541ReadRequest& req) -> Xum1541ReadResult {
     *       // cbm_listen + "#" + U1 block-read loop, OpenCBM-backed
     *       ...
     *   };
     *
     * In tests, inject a scripted lambda:
     *   auto runner = [](const Xum1541ReadRequest&) -> Xum1541ReadResult {
     *       Xum1541ReadResult r;
     *       r.sector_bytes = std::vector<uint8_t>(256, 0xAA);
     *       r.good_sectors = 1;
     *       r.total_sectors = 1;
     *       r.cbm_track = 1;
     *       return r;
     *   };
     */
    using Xum1541Runner = std::function<Xum1541ReadResult(const Xum1541ReadRequest&)>;

    /**
     * @brief XUM1541 sector write runner function type.
     *
     * Accepts an Xum1541WriteRequest (cylinder, head, retries, device, type,
     * verify, sector_bytes). Returns an Xum1541WriteResult.
     */
    using Xum1541WriteRunner = std::function<Xum1541WriteResult(const Xum1541WriteRequest&)>;

    /**
     * @brief XUM1541 detect runner function type.
     *
     * Probes for a connected XUM1541/ZoomFloppy device via OpenCBM.
     * Returns an Xum1541DetectResult.
     *
     * In production, tries cbm_driver_open + cbm_identify.
     * In tests, return a scripted Xum1541DetectResult directly:
     *   auto detect_runner = []() -> Xum1541DetectResult {
     *       return Xum1541DetectResult{true, "Commodore 1541", 35, 1, "", "OpenCBM", ""};
     *   };
     */
    using Xum1541DetectRunner = std::function<Xum1541DetectResult()>;

    /**
     * @brief Construct from injectable runners.
     *
     * @param read_runner    Callable for sector reads. If null, every
     *                       do_read_sector() returns a ProviderError.
     * @param write_runner   Callable for sector writes. If null, every
     *                       do_write_sector() returns a ProviderError.
     * @param detect_runner  Callable for drive detection. If null, every
     *                       do_detect_drive() returns a ProviderError.
     * @param max_cylinder   Maximum cylinder index supported (inclusive).
     *                       Default 79 (1581: 80 tracks). 1541/1571: use 34.
     * @param device_num     CBM device number to request for reads/writes
     *                       when not overridden by the request. Default 8.
     * @param drive_type     Drive type hint (0 = auto). Passed to runners.
     */
    explicit XUM1541ProviderV2(Xum1541Runner       read_runner,
                                Xum1541WriteRunner  write_runner,
                                Xum1541DetectRunner detect_runner,
                                int max_cylinder = 79,
                                int device_num   = 8,
                                int drive_type   = 0);

    /* Non-copyable (holds std::function + state). */
    XUM1541ProviderV2(const XUM1541ProviderV2&)            = delete;
    XUM1541ProviderV2& operator=(const XUM1541ProviderV2&) = delete;

    /* Movable. */
    XUM1541ProviderV2(XUM1541ProviderV2&&)            = default;
    XUM1541ProviderV2& operator=(XUM1541ProviderV2&&) = default;

    ~XUM1541ProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    SectorOutcome do_read_sector (const ReadSectorParams& p);
    WriteOutcome  do_write_sector(const WriteSectorParams& w, const SectorPayload& payload);
    DetectOutcome do_detect_drive();

private:
    Xum1541Runner       m_read_runner;    /**< Sector-read runner (injected). */
    Xum1541WriteRunner  m_write_runner;   /**< Sector-write runner (injected). */
    Xum1541DetectRunner m_detect_runner;  /**< Drive-detect runner (injected). */
    int                 m_max_cylinder;  /**< Maximum valid cylinder index. */
    int                 m_device_num;    /**< Default CBM device number. */
    int                 m_drive_type;    /**< Drive type hint (0=auto). */

    /** Return a ProviderError for a null-runner condition. */
    static ProviderError null_runner_error(const char* operation);

    /** Return a ProviderError for a geometry range violation. */
    ProviderError range_error(int cylinder, int head) const;

    /**
     * @brief Translate a failed Xum1541ReadResult into a SectorOutcome.
     *
     * Covers: opencbm_unavailable, invalid_track, IEC LISTEN failures,
     * and partial reads where all sectors failed.
     */
    static SectorOutcome translate_read_failure(const Xum1541ReadResult& result,
                                                 int cylinder, int head);

    /**
     * @brief Translate a successful (or partial) Xum1541ReadResult into
     *        a SectorRead or SectorMarginal.
     *
     * Rule F-3: when bad_sectors > 0 the partial sector data is preserved
     * in SectorMarginal::divergent_reads with at least 2 entries (the
     * partial-data entry + the failed-sector sentinel).
     */
    static SectorOutcome translate_read_result(const Xum1541ReadResult& result,
                                                int cylinder, int head);

    /**
     * @brief Translate an Xum1541WriteResult into a WriteOutcome.
     */
    static WriteOutcome translate_write_result(const Xum1541WriteResult& result,
                                                int cylinder, int head,
                                                const SectorPayload& payload);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy HasIdentity");
static_assert(ReadsSectors<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy ReadsSectors "
    "(XUM1541 reads CBM DOS sectors via IEC U1 block-read command)");
static_assert(WritesSectors<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy WritesSectors "
    "(XUM1541 writes CBM DOS sectors via IEC U2 block-write command)");
static_assert(DetectsDrive<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy DetectsDrive "
    "(XUM1541 can be detected via OpenCBM cbm_driver_open + cbm_identify)");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsRawFlux<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy ReadsRawFlux "
    "(XUM1541 is a sector-level IEC device; V1 readRawFlux() explicitly "
    "states 'Raw GCR read not yet supported. Nibtools integration required.' "
    "— this is a silent stub, not a capability)");
static_assert(!WritesRawFlux<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy WritesRawFlux "
    "(V1 writeRawFlux() is a pure Q_UNUSED stub: returns false immediately "
    "with 'Raw flux write not yet supported.' — not a capability)");
static_assert(!ControlsMotor<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy ControlsMotor "
    "(V1 setMotor() is Q_UNUSED(on); return m_connected — the IEC bus has "
    "no standalone motor control; drives spin up automatically on access)");
static_assert(!SeeksHead<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy SeeksHead "
    "(V1 seekCylinder() only updates m_currentCylinder; the IEC bus has no "
    "explicit seek command — head position is set implicitly by U1/U2 commands)");
static_assert(!Recalibrates<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy Recalibrates "
    "(V1 recalibrate() sends the CBM DOS Initialize command 'I' which moves "
    "to track 18, NOT track 0 — semantics do not match SeekArrived{cylinder=0})");
static_assert(!MeasuresRPM<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy MeasuresRPM "
    "(V1 measureRPM() returns hardcoded 300.0 — NOT a real hardware measurement; "
    "comment states 'Precise measurement would require nibtools-style timing')");

/* Composite predicates. */
static_assert(ImagesSectors<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy ImagesSectors "
    "(has both ReadsSectors and DetectsDrive)");
static_assert(WritesAnything<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy WritesAnything "
    "(has WritesSectors)");
static_assert(!FullDriveControl<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates are all absent — IEC bus "
    "abstracts drive mechanics internally)");
static_assert(!ImagesFlux<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy ImagesFlux "
    "(XUM1541 is sector-level; no flux capture capability)");

}  // namespace uft::hal

#endif  // XUM1541_PROVIDER_V2_H
