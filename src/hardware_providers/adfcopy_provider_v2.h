/**
 * @file adfcopy_provider_v2.h
 * @brief ADFCopyProviderV2 — mixin-composed V2 HAL provider (MF-167 / P1.14).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * V1 audit summary (adfcopyhardwareprovider.cpp, 1448 LOC):
 *
 *   ADF-Copy is a Teensy-based (PJRC, VID:PID 0x16C0:0x0483) USB-CDC serial
 *   controller for Amiga 3.5" floppy drives. The protocol uses binary framing:
 *   a 1-byte command byte followed by a variable payload. Responses use a
 *   1-byte status byte (ADFC_RSP_OK='O', ADFC_RSP_ERROR='E', etc.) plus a
 *   binary payload where applicable.
 *
 *   REAL implementations found in V1 (all guarded by ADFC_SERIAL_AVAILABLE):
 *     - setMotor(bool on)      → (on) sends ADFC_CMD_INIT (0x01), waits 'O'.
 *                                 This spins up the motor AND homes the head.
 *                                 Motor-off only sets m_motorOn flag — no serial
 *                                 command. The motor-ON path IS a real serial
 *                                 dialog. REAL (motor-on only).
 *     - seekCylinder(int)     → ADFC_CMD_SEEK (0x02) + 1-byte track number
 *                                 (track = cylinder*2 + head), waits 'O'.
 *                                 Range check [0, ADFC_MAX_CYLINDERS=83]. REAL.
 *     - recalibrate()         → delegates to seekCylinder(0) → ADFC_CMD_SEEK.
 *                                 REAL (via seek to track 0).
 *     - readRawFlux(c,h,revs) → ADFC_CMD_READ_FLUX (0x06) + track + revolutions,
 *                                 reads 3-byte header (status+len16) then binary
 *                                 flux payload. REAL.
 *     - cmdGetStatus()         → ADFC_CMD_GET_STATUS (0x0B), reads 1-byte status
 *                                 bitmask. Used by detectDrive(). REAL.
 *     - detectDrive()          → calls cmdGetStatus() (real serial), then emits
 *                                 hardcoded Amiga DD geometry. REAL (status query).
 *
 *   SILENT STUBS found in V1 (omitted as capabilities):
 *     - measureRPM()           → returns constant 300.0, NO serial dialog.
 *                                 OMIT MeasuresRPM.
 *     - writeRawFlux(c,h,flux) → always returns false, emits error
 *                                 "Raw flux writing is not supported by ADF-Copy
 *                                 hardware". OMIT WritesRawFlux.
 *     - selectHead(int)        → sets m_currentHead only, NO serial command.
 *                                 Head is encoded in track number for all commands.
 *                                 This is an internal detail, not a V2 capability.
 *
 * Capabilities claimed (V1-audit-driven):
 *   Identity<"ADFCopy", CommunityConsensus>
 *   ReadsRawFlux      v  do_read_raw_flux()   -> FluxOutcome
 *   ControlsMotor     v  do_set_motor()       -> MotorOutcome
 *   SeeksHead         v  do_seek()            -> SeekOutcome
 *   Recalibrates      v  do_recalibrate()     -> SeekOutcome
 *   DetectsDrive      v  do_detect_drive()    -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   WritesRawFlux   x  V1 writeRawFlux() explicitly returns false with
 *                      "Raw flux writing is not supported by ADF-Copy hardware".
 *                      Honest audit: hardware cannot do this.
 *   ReadsSectors    x  ADFCopy is a flux device at the HAL layer. Sector
 *                      decode (Amiga MFM: 11 sectors / track, 512 bytes each)
 *                      happens in the upstream analysis pipeline.
 *   WritesSectors   x  Same rationale as ReadsSectors. The V1 writeTrack()
 *                      takes MFM-decoded sector data — this is upstream
 *                      pipeline output, not a HAL capability.
 *   MeasuresRPM     x  V1 measureRPM() returns constant 300.0 with no serial
 *                      dialog. This is a silent stub — it does not measure
 *                      anything. Honest audit: not a real capability.
 *
 * SpecStatus: CommunityConsensus — ADF-Copy is a DIY open-source Teensy-based
 *   project with community-documented protocol. No official vendor spec exists.
 *   Protocol reverse-engineered from the open-source firmware and the V1 Qt
 *   provider implementation. Cross-checked against the binary framing constants
 *   in adfcopyhardwareprovider.h (ADFC_CMD_* / ADFC_RSP_*).
 *
 * Transport abstraction:
 *   Option B (IADFCopyTransport interface) — mirrors Applesauce P1.13 exactly.
 *   ADF-Copy uses a binary serial protocol (not ASCII line-mode):
 *     - Commands: 1-byte command + variable binary payload.
 *     - Responses: 1-byte status byte + optional binary payload.
 *   This is different from Applesauce's ASCII line-mode + binary bulk,
 *   but the abstraction shape is identical: write_bytes / read_bytes.
 *
 *   IADFCopyTransport provides:
 *     write_bytes(data)             — write raw bytes to the device
 *     read_bytes(n, timeout_ms)     — read exactly n bytes with timeout
 *     read_until_newline(timeout_ms) — read text response (for PING/version)
 *
 *   In production: wrap a QSerialPort (connected ADFCopy device).
 *   In tests: injectable lambda runners (same as Applesauce pattern).
 *
 * Rule F-3 (multi-revolution / divergent-read preservation):
 *   The V1 readRawFlux() passes the revolutions parameter to ADFC_CMD_READ_FLUX,
 *   and the full flux payload returned by the device is stored verbatim. The V2
 *   carries this forward: ADFCopyReadResult::flux_bytes holds the complete
 *   device response without truncation. No single-sample collapsing.
 *   The ADFCopy flux format: raw 16-bit timing values in 25ns units (40 MHz
 *   clock), consistent with the SCP format it targets (ADFC_CMD_READ_SCP).
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * Backend honesty (Qt-only, no C-HAL):
 *   Unlike Greaseweazle (uft_gw_* C-API) or SCP (uft_scp_direct.c), there is
 *   NO C-HAL backend for ADFCopy. The V1 is Qt-only (QSerialPort). The V2
 *   wraps the V1 protocol code via the IADFCopyTransport abstraction.
 *   When the transport is null or returns an error, do_*() returns ProviderError.
 *
 * The V1 ADFCopyHardwareProvider is NOT deleted here (task P1.17).
 */
#ifndef ADFCOPY_PROVIDER_V2_H
#define ADFCOPY_PROVIDER_V2_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/* ─────────────────────────────────────────────────────────────────────────
 *  IADFCopyTransport — the raw serial I/O interface
 *
 *  ADF-Copy uses a binary framing protocol over USB-CDC serial (115200 8N1).
 *  Commands: 1 command byte + variable binary payload.
 *  Responses: 1 status byte + optional binary payload.
 *  Exception: PING (0x00) response is a text version string terminated by '\n'.
 *
 *  Separating the protocol logic from the transport allows:
 *    - Production: QSerialPort-backed transport.
 *    - Tests:      injectable lambda runners (scripted byte sequences).
 * ─────────────────────────────────────────────────────────────────────── */

struct IADFCopyTransport {
    virtual ~IADFCopyTransport() = default;

    /**
     * @brief Write raw bytes to the device (command + payload).
     *
     * @return Number of bytes actually written, or -1 on error.
     */
    virtual int write_bytes(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Read exactly n_bytes of raw binary data.
     *
     * Used to read fixed-size response payloads (status byte, length fields,
     * binary flux data). Returns a vector that may be shorter than n_bytes
     * on timeout.
     *
     * @param n_bytes    Exact byte count to receive.
     * @param timeout_ms Maximum wait time in milliseconds.
     */
    virtual std::vector<uint8_t> read_bytes(uint32_t n_bytes,
                                             int timeout_ms = 5000) = 0;

    /**
     * @brief Read a text response line terminated by '\n'.
     *
     * Used exclusively by the PING command response (version string).
     * Returns the line content WITH or WITHOUT the trailing '\n' — callers
     * should trim. Returns an empty string on timeout or transport error.
     *
     * @param timeout_ms Maximum wait time in milliseconds.
     */
    virtual std::string read_until_newline(int timeout_ms = 3000) = 0;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ADFCopyReadResult — result of a flux read operation
 *
 *  V1 cmdReadFlux() path:
 *    1. ADFC_CMD_READ_FLUX (0x06) + track(1) + revolutions(1)
 *    2. Read 3-byte header: status(1) + length_hi(1) + length_lo(1)
 *    3. Read binary flux payload (length bytes)
 *
 *  flux_bytes contains the raw binary data downloaded from the device.
 *  The ADFCopy firmware outputs raw 16-bit timing values in 25ns units
 *  (consistent with its SCP capture mode). Each 2-byte big-endian value
 *  is one flux transition timing.
 *
 *  Rule F-3: the full device flux response is stored verbatim — no
 *  decimation, no averaging. When revolutions > 1, the complete multi-
 *  revolution sequence from the device is preserved.
 * ─────────────────────────────────────────────────────────────────────── */
struct ADFCopyReadResult {
    /** Raw flux bytes from the device.
     *  Format: consecutive 16-bit big-endian values, each = one flux
     *  transition timing in 25ns units (40 MHz ADFCopy sample clock).
     *  Empty when the read failed. */
    std::vector<uint8_t> flux_bytes;

    /** Number of revolutions captured (passed to ADFC_CMD_READ_FLUX). */
    int revolutions = 1;

    /** Human-readable error message when flux_bytes is empty. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;

    /** True if the device returned ADFC_RSP_ERROR ('E') or ADFC_RSP_NODISK ('D'). */
    bool device_error = false;

    /** True if the device returned ADFC_RSP_NODISK ('D'). */
    bool no_disk = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ADFCopyMotorResult — result of a motor control (INIT) operation
 *
 *  V1 setMotor(true) path:
 *    1. ADFC_CMD_INIT (0x01)
 *    2. Wait for ADFC_RSP_OK ('O')
 *
 *  Note: motor-off in V1 sets m_motorOn=false with NO serial command.
 *  The runner still reports success=true for motor-off (best-effort).
 * ─────────────────────────────────────────────────────────────────────── */
struct ADFCopyMotorResult {
    /** True if the INIT command succeeded (status='O'). */
    bool success = false;

    /** Human-readable error message when success == false. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ADFCopySeekResult — result of a seek or recalibrate operation
 *
 *  V1 cmdSeek(track) path:
 *    1. ADFC_CMD_SEEK (0x02) + track(1)
 *    2. Wait for ADFC_RSP_OK ('O')
 *
 *  Track number = cylinder * 2 + head (Amiga convention: 0..159 for 80c×2h).
 * ─────────────────────────────────────────────────────────────────────── */
struct ADFCopySeekResult {
    /** True if the SEEK command succeeded (status='O'). */
    bool success = false;

    /** The cylinder reached (for seek: the requested one; for recal: 0). */
    int cylinder_reached = 0;

    /** Human-readable error message when success == false. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ADFCopyDetectResult — result of a drive detection (GET_STATUS) operation
 *
 *  V1 detectDrive() path:
 *    1. ADFC_CMD_GET_STATUS (0x0B)
 *    2. Read 1-byte status bitmask (ADFC_STATUS_DISK_PRESENT, etc.)
 *
 *  Geometry is always Amiga standard: 80 cylinders, 2 heads.
 *  The firmware version is obtained via ADFC_CMD_PING at connection time.
 * ─────────────────────────────────────────────────────────────────────── */
struct ADFCopyDetectResult {
    /** True if ADFC_STATUS_DISK_PRESENT bit is set. */
    bool disk_present = false;

    /** True if ADFC_STATUS_WRITE_PROT bit is set. */
    bool write_protected = false;

    /** True if ADFC_STATUS_MOTOR_ON bit is set. */
    bool motor_on = false;

    /** True if ADFC_STATUS_FLUX_CAPABLE bit is set (ADF-Drive firmware). */
    bool flux_capable = false;

    /** Firmware version string (from PING response at connection time).
     *  May be empty if the runner did not capture it. */
    std::string firmware;

    /** Human-readable error message when the detection failed. */
    std::string error_message;

    /** True if the transport was not available. */
    bool transport_unavailable = false;
};

/* ─────────────────────────────────────────────────────────────────────────
 *  ADFCopyProviderV2 — the V2 mixin-composed provider
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief ADFCopy V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"ADFCopy", SpecStatus::CommunityConsensus>
 *   ReadsRawFluxVia<ADFCopyProviderV2>
 *   ControlsMotorVia<ADFCopyProviderV2>
 *   SeeksHeadVia<ADFCopyProviderV2>
 *   RecalibratesVia<ADFCopyProviderV2>
 *   DetectsDriveVia<ADFCopyProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class ADFCopyProviderV2 final
    : public mixin::Identity<"ADFCopy", SpecStatus::CommunityConsensus>
    , public mixin::ReadsRawFluxVia<ADFCopyProviderV2>
    , public mixin::ControlsMotorVia<ADFCopyProviderV2>
    , public mixin::SeeksHeadVia<ADFCopyProviderV2>
    , public mixin::RecalibratesVia<ADFCopyProviderV2>
    , public mixin::DetectsDriveVia<ADFCopyProviderV2>
{
public:
    /**
     * @brief ADFCopy flux read runner function type.
     *
     * Accepts cylinder, head, revolutions. Returns an ADFCopyReadResult.
     *
     * The runner encapsulates the full ADFCopy read protocol:
     *   ADFC_CMD_READ_FLUX + track + revolutions → 3-byte header → binary payload
     * In production, wrap the IADFCopyTransport.
     * In tests, inject a scripted lambda that returns pre-built flux bytes.
     */
    struct ReadRequest {
        int cylinder    = 0;
        int head        = 0;
        int revolutions = 2;
    };
    using ADFCopyReadRunner = std::function<ADFCopyReadResult(const ReadRequest&)>;

    /**
     * @brief ADFCopy motor control runner function type.
     *
     * Accepts a bool (true = on via INIT, false = off / best-effort).
     * Returns ADFCopyMotorResult.
     *
     * Motor-ON: ADFC_CMD_INIT (0x01) → waits for 'O'.
     * Motor-OFF: best-effort (no dedicated serial command in protocol).
     */
    using ADFCopyMotorRunner = std::function<ADFCopyMotorResult(bool on)>;

    /**
     * @brief ADFCopy seek runner function type.
     *
     * Accepts cylinder and current_head. Computes track = cylinder*2 + head,
     * sends ADFC_CMD_SEEK (0x02) + track byte, waits for 'O'.
     * Returns ADFCopySeekResult.
     */
    struct SeekRequest {
        int cylinder     = 0;
        int current_head = 0;
    };
    using ADFCopySeekRunner = std::function<ADFCopySeekResult(const SeekRequest&)>;

    /**
     * @brief ADFCopy recalibrate runner function type.
     *
     * Sends ADFC_CMD_SEEK (0x02) with track=0 (cylinder 0, head 0).
     * Returns ADFCopySeekResult with cylinder_reached=0.
     */
    using ADFCopyRecalRunner = std::function<ADFCopySeekResult()>;

    /**
     * @brief ADFCopy drive detect runner function type.
     *
     * Sends ADFC_CMD_GET_STATUS (0x0B), reads 1-byte bitmask.
     * Returns ADFCopyDetectResult.
     */
    using ADFCopyDetectRunner = std::function<ADFCopyDetectResult()>;

    /**
     * @brief Construct from injectable runners.
     *
     * @param read_runner     Callable for flux reads. If null, every
     *                        do_read_raw_flux() returns a ProviderError.
     * @param motor_runner    Callable for motor control. If null, every
     *                        do_set_motor() returns a ProviderError.
     * @param seek_runner     Callable for head seeks. If null, every
     *                        do_seek() returns a ProviderError.
     * @param recal_runner    Callable for recalibrate. If null, every
     *                        do_recalibrate() returns a ProviderError.
     * @param detect_runner   Callable for drive detection. If null, every
     *                        do_detect_drive() returns a ProviderError.
     * @param max_cylinder    Maximum cylinder index (inclusive). Default 83
     *                        (matches ADFC_MAX_CYLINDERS in V1).
     * @param sample_clock_hz ADFCopy sample clock in Hz. Default 40,000,000
     *                        (40 MHz, 25 ns per tick — matches SCP resolution).
     */
    explicit ADFCopyProviderV2(ADFCopyReadRunner    read_runner,
                                ADFCopyMotorRunner   motor_runner,
                                ADFCopySeekRunner    seek_runner,
                                ADFCopyRecalRunner   recal_runner,
                                ADFCopyDetectRunner  detect_runner,
                                int    max_cylinder    = 83,
                                double sample_clock_hz = 40000000.0);

    /* Non-copyable (holds std::function + state). */
    ADFCopyProviderV2(const ADFCopyProviderV2&)            = delete;
    ADFCopyProviderV2& operator=(const ADFCopyProviderV2&) = delete;

    /* Movable. */
    ADFCopyProviderV2(ADFCopyProviderV2&&)            = default;
    ADFCopyProviderV2& operator=(ADFCopyProviderV2&&) = default;

    ~ADFCopyProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    MotorOutcome  do_set_motor     (bool on);
    SeekOutcome   do_seek          (int cylinder);
    SeekOutcome   do_recalibrate   ();
    DetectOutcome do_detect_drive  ();

private:
    ADFCopyReadRunner    m_read_runner;
    ADFCopyMotorRunner   m_motor_runner;
    ADFCopySeekRunner    m_seek_runner;
    ADFCopyRecalRunner   m_recal_runner;
    ADFCopyDetectRunner  m_detect_runner;
    int                  m_max_cylinder;
    double               m_sample_clock_hz;

    /** sample_ns = 1e9 / sample_clock_hz (e.g. 25.0 for 40 MHz). */
    double               m_sample_ns;

    /** Current head (0 or 1). Updated by do_seek(); used by seek runner. */
    int                  m_current_head = 0;

    /** Return a ProviderError for a null-runner condition. */
    static ProviderError null_runner_error(const char* operation);

    /** Return a ProviderError for a geometry range violation. */
    ProviderError range_error(int cylinder, int head) const;

    /**
     * @brief Convert raw flux bytes (16-bit big-endian 25ns ticks) to
     *        transition_ns vector.
     *
     * Each 2-byte big-endian value is one flux transition timing in
     * ADFCopy 40 MHz ticks (25 ns per tick). Converts to nanoseconds.
     * Rule F-3: the entire captured buffer is preserved verbatim.
     */
    std::vector<uint32_t> bytes_to_transitions_ns(
        const std::vector<uint8_t>& flux_bytes) const;

    /** Translate a failed ADFCopyReadResult to a FluxOutcome. */
    static FluxOutcome translate_read_failure(const ADFCopyReadResult& r,
                                               int cylinder, int head);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy ReadsRawFlux "
    "(ADFCopy reads raw flux via ADFC_CMD_READ_FLUX / 0x06 with "
    "2-byte 25ns-resolution timing values at 40 MHz sample clock)");
static_assert(ControlsMotor<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy ControlsMotor "
    "(ADFCopy motor-on via ADFC_CMD_INIT / 0x01 which spins up the motor "
    "and homes the head)");
static_assert(SeeksHead<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy SeeksHead "
    "(ADFCopy sends ADFC_CMD_SEEK / 0x02 + track byte to seek to a cylinder; "
    "track = cylinder*2 + head, Amiga convention)");
static_assert(Recalibrates<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy Recalibrates "
    "(ADFCopy sends ADFC_CMD_SEEK / 0x02 with track=0 to recalibrate to "
    "cylinder 0, head 0)");
static_assert(DetectsDrive<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy DetectsDrive "
    "(ADFCopy sends ADFC_CMD_GET_STATUS / 0x0B and reads a 1-byte bitmask "
    "for disk-present, write-protect, motor-on, flux-capable flags)");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!WritesRawFlux<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy WritesRawFlux "
    "(V1 writeRawFlux() explicitly returns false with 'Raw flux writing is "
    "not supported by ADF-Copy hardware'. Hardware audit confirms omission.)");
static_assert(!ReadsSectors<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy ReadsSectors "
    "(ADFCopy is a flux device at the HAL layer; Amiga MFM sector decode "
    "— 11 sectors per track, 512 bytes each — is the upstream pipeline's "
    "responsibility, not the HAL's.)");
static_assert(!WritesSectors<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy WritesSectors "
    "(V1 writeTrack() takes MFM-decoded sector data — this is an upstream "
    "pipeline output, not a raw HAL capability. The V2 HAL layer does not "
    "write sector data.)");
static_assert(!MeasuresRPM<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy MeasuresRPM "
    "(V1 measureRPM() returns constant 300.0 with no serial dialog — it is "
    "a silent stub, not a real measurement. Hardware audit: omit mixin.)");

/* Composite predicates. */
static_assert(ImagesFlux<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");
static_assert(FullDriveControl<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy FullDriveControl "
    "(ControlsMotor via ADFC_CMD_INIT + SeeksHead via ADFC_CMD_SEEK + "
    "Recalibrates via ADFC_CMD_SEEK with track=0)");
static_assert(!WritesAnything<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy WritesAnything "
    "(no write capability at the HAL level: WritesRawFlux is omitted "
    "because the hardware does not support raw flux write; WritesSectors "
    "is omitted because sector encoding is upstream pipeline)");
static_assert(!ImagesSectors<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy ImagesSectors "
    "(flux device; sector decode is upstream pipeline, not in the HAL layer)");

}  // namespace uft::hal

#endif  // ADFCOPY_PROVIDER_V2_H
