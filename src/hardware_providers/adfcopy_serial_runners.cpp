/**
 * @file adfcopy_serial_runners.cpp
 * @brief Implementation of the 5 ADFCopy runner-factories (MF-252).
 *
 * Defensive: every exit path populates the *Result struct; never throws,
 * never silently fabricates flux bytes.
 */
#include "adfcopy_serial_runners.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace uft::hal {

namespace {

/** Wait for a single status byte. Returns 0xFF on timeout. */
uint8_t read_status_byte(IADFCopyTransport* tx, int timeout_ms = 5000)
{
    auto bytes = tx->read_bytes(1, timeout_ms);
    if (bytes.size() != 1) return 0xFF;
    return bytes[0];
}

/** Send a single command byte. Returns false on transport error. */
bool send_cmd(IADFCopyTransport* tx, uint8_t cmd)
{
    return tx->write_bytes({cmd}) == 1;
}

/** Compute ADFCopy track number (cylinder * 2 + head, Amiga convention). */
int compute_track_byte(int cylinder, int head)
{
    return cylinder * 2 + (head & 1);
}

} // namespace

/* ── Detect runner (GET_STATUS) ───────────────────────────────────── */

ADFCopyProviderV2::ADFCopyDetectRunner
make_adfcopy_detect_runner(ADFCopyTransportPtr tx)
{
    return [tx]() -> ADFCopyDetectResult {
        ADFCopyDetectResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "ADFCopy transport not constructed";
            return r;
        }
        IADFCopyTransport* t = tx.get();

        if (!send_cmd(t, kAdfcCmdGetStatus)) {
            r.error_message = "GET_STATUS write failed";
            return r;
        }
        const uint8_t status = read_status_byte(t);
        if (status == 0xFF) {
            r.error_message = "GET_STATUS response timeout";
            return r;
        }
        r.disk_present    = (status & kAdfcStatusDiskPresent)  != 0;
        r.write_protected = (status & kAdfcStatusWriteProt)    != 0;
        r.motor_on        = (status & kAdfcStatusMotorOn)      != 0;
        r.flux_capable    = (status & kAdfcStatusFluxCapable)  != 0;
        return r;
    };
}

/* ── Motor runner (INIT) ──────────────────────────────────────────── */

ADFCopyProviderV2::ADFCopyMotorRunner
make_adfcopy_motor_runner(ADFCopyTransportPtr tx)
{
    return [tx](bool on) -> ADFCopyMotorResult {
        ADFCopyMotorResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "ADFCopy transport not constructed";
            return r;
        }
        if (!on) {
            /* Protocol has no dedicated motor-off command in V1. Best-effort
             * mark as success — the firmware spins down on its own after
             * idle timeout. */
            r.success = true;
            return r;
        }
        IADFCopyTransport* t = tx.get();
        if (!send_cmd(t, kAdfcCmdInit)) {
            r.error_message = "INIT write failed";
            return r;
        }
        const uint8_t rsp = read_status_byte(t);
        if (rsp != kAdfcRspOk) {
            r.error_message = "INIT did not return 'O' (got 0x" +
                              std::to_string(int(rsp)) + ")";
            return r;
        }
        r.success = true;
        return r;
    };
}

/* ── Seek runner (SEEK) ───────────────────────────────────────────── */

ADFCopyProviderV2::ADFCopySeekRunner
make_adfcopy_seek_runner(ADFCopyTransportPtr tx)
{
    return [tx](const ADFCopyProviderV2::SeekRequest& req)
                  -> ADFCopySeekResult {
        ADFCopySeekResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "ADFCopy transport not constructed";
            return r;
        }
        if (req.cylinder < 0 || req.current_head < 0 || req.current_head > 1) {
            r.error_message = "invalid cylinder/head parameters";
            return r;
        }
        const int track = compute_track_byte(req.cylinder, req.current_head);
        if (track < 0 || track > 255) {
            r.error_message = "track byte out of range";
            return r;
        }

        IADFCopyTransport* t = tx.get();
        if (t->write_bytes({kAdfcCmdSeek,
                            static_cast<uint8_t>(track)}) != 2) {
            r.error_message = "SEEK write failed";
            return r;
        }
        const uint8_t rsp = read_status_byte(t);
        if (rsp != kAdfcRspOk) {
            r.error_message = "SEEK did not return 'O' (got 0x" +
                              std::to_string(int(rsp)) + ")";
            return r;
        }
        r.success          = true;
        r.cylinder_reached = req.cylinder;
        return r;
    };
}

/* ── Recalibrate runner ───────────────────────────────────────────── */

ADFCopyProviderV2::ADFCopyRecalRunner
make_adfcopy_recal_runner(ADFCopyTransportPtr tx)
{
    return [tx]() -> ADFCopySeekResult {
        ADFCopySeekResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "ADFCopy transport not constructed";
            return r;
        }
        IADFCopyTransport* t = tx.get();
        if (t->write_bytes({kAdfcCmdSeek, 0}) != 2) {
            r.error_message = "RECAL (SEEK 0) write failed";
            return r;
        }
        const uint8_t rsp = read_status_byte(t);
        if (rsp != kAdfcRspOk) {
            r.error_message = "RECAL did not return 'O' (got 0x" +
                              std::to_string(int(rsp)) + ")";
            return r;
        }
        r.success          = true;
        r.cylinder_reached = 0;
        return r;
    };
}

/* ── Read runner (READ_FLUX) ──────────────────────────────────────── */

ADFCopyProviderV2::ADFCopyReadRunner
make_adfcopy_read_runner(ADFCopyTransportPtr tx)
{
    return [tx](const ADFCopyProviderV2::ReadRequest& req)
                  -> ADFCopyReadResult {
        ADFCopyReadResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "ADFCopy transport not constructed";
            return r;
        }
        if (req.cylinder < 0 || req.head < 0 || req.head > 1) {
            r.device_error = true;
            r.error_message = "invalid cylinder/head parameters";
            return r;
        }
        const int track = compute_track_byte(req.cylinder, req.head);
        if (track < 0 || track > 255) {
            r.device_error = true;
            r.error_message = "track byte out of range";
            return r;
        }
        const int revs = std::clamp(req.revolutions, 1, 15);

        IADFCopyTransport* t = tx.get();
        if (t->write_bytes({kAdfcCmdReadFlux,
                            static_cast<uint8_t>(track),
                            static_cast<uint8_t>(revs)}) != 3) {
            r.device_error = true;
            r.error_message = "READ_FLUX write failed";
            return r;
        }

        /* 3-byte header: status, length_hi, length_lo (big-endian). */
        auto header = t->read_bytes(3, 10000);
        if (header.size() != 3) {
            r.device_error = true;
            r.error_message = "READ_FLUX header short read (" +
                              std::to_string(header.size()) + " of 3)";
            return r;
        }
        if (header[0] == kAdfcRspNoDisk) {
            r.device_error = true;
            r.no_disk = true;
            r.error_message = "no disk in drive";
            return r;
        }
        if (header[0] != kAdfcRspOk) {
            r.device_error = true;
            r.error_message = "READ_FLUX status not 'O' (got 0x" +
                              std::to_string(int(header[0])) + ")";
            return r;
        }
        const uint32_t length = (uint32_t(header[1]) << 8) | header[2];
        if (length == 0) {
            r.device_error = true;
            r.error_message = "READ_FLUX length=0";
            return r;
        }

        /* Read the binary flux payload. */
        r.flux_bytes = t->read_bytes(length, 30000);
        if (r.flux_bytes.size() != length) {
            r.device_error = true;
            r.error_message = "READ_FLUX payload short: " +
                              std::to_string(r.flux_bytes.size()) +
                              " of " + std::to_string(length);
            r.flux_bytes.clear();
            return r;
        }
        r.revolutions = revs;
        return r;
    };
}

} // namespace uft::hal
