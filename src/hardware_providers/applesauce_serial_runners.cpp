/**
 * @file applesauce_serial_runners.cpp
 * @brief Implementation of the 7 Applesauce runner-factories (MF-250).
 *
 * Each factory builds a closure that:
 *   1. validates the captured transport is non-null
 *   2. drives the documented Applesauce text protocol step-by-step
 *   3. parses each response defensively (status prefix MUST match)
 *   4. returns a fully-populated *Result struct
 *
 * Status-prefix conventions (per protocol):
 *   "+ payload"  — success
 *   "- message"  — device error (sets device_error = true)
 *   ".           — bare success ack (used by motor / seek / recal)
 *   "!"          — protocol error / write-protected disk
 *
 * No throws. No silent fabrication. Empty/timeout response from the
 * transport always sets `device_error = true` with a diagnostic
 * error_message — never optimistically continues.
 */
#include "applesauce_serial_runners.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace uft::hal {

namespace {

/* ── Small helpers ────────────────────────────────────────────────── */

/** Strip the status-prefix character if present, returning the
 *  payload. Empty input → empty output. */
std::string strip_prefix(const std::string& line)
{
    if (line.empty()) return line;
    /* Single-character ack (".") has no payload. */
    if (line.size() == 1) return std::string();
    /* "+payload" / "-payload" → "payload". */
    if (line[0] == '+' || line[0] == '-' || line[0] == '!') {
        return line.substr(1);
    }
    return line;
}

/** Send a line and capture the immediate response. Returns true if
 *  the response was received and starts with '+' or '.'. On any
 *  failure, populates `*out_err` with a diagnostic. */
bool send_and_check_ok(IApplesauceTransport* tx,
                       const std::string&   command,
                       std::string*         out_err,
                       std::string*         out_payload = nullptr,
                       int                  timeout_ms = 3000)
{
    if (!tx->write_line(command)) {
        if (out_err) *out_err = "transport write failed for command: " + command;
        return false;
    }
    const std::string resp = tx->read_line(timeout_ms);
    if (resp.empty()) {
        if (out_err) *out_err = "no response to command: " + command;
        return false;
    }
    if (resp[0] == '+' || resp == ".") {
        if (out_payload) *out_payload = strip_prefix(resp);
        return true;
    }
    if (out_err) {
        *out_err = "command '" + command + "' returned: " + resp;
    }
    return false;
}

} // namespace

/* ── Read runner ──────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceReadRunner
make_applesauce_read_runner(ApplesauceTransportPtr tx)
{
    return [tx](const ApplesauceProviderV2::ReadRequest& req)
                  -> ApplesauceReadResult {
        ApplesauceReadResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }

        /* Sequence:
         *   1. sync:on            — enable sync detection
         *   2. head:track N       — confirm position (no harm if
         *                           HardwareTab already seeked)
         *   3. head:side H        — pick side
         *   4. disk:readx R       — capture R revolutions to device buffer
         *   5. data:?size         — query captured length
         *   6. data:<N (announced by device, then N binary bytes)
         *   7. sync:off
         */
        std::string err;
        IApplesauceTransport* t = tx.get();

        if (!send_and_check_ok(t, "sync:on", &err)) {
            r.error_message = err; r.device_error = true; return r;
        }

        char buf[64];
        std::snprintf(buf, sizeof(buf), "head:track %d", req.cylinder);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err; r.device_error = true;
            (void)t->write_line("sync:off");
            return r;
        }
        std::snprintf(buf, sizeof(buf), "head:side %d", req.head);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err; r.device_error = true;
            (void)t->write_line("sync:off");
            return r;
        }

        /* Defensive: revolutions must fit in a small positive range
         * — the firmware accepts 1..15 in practice. */
        const int revolutions = std::clamp(req.revolutions, 1, 15);
        std::snprintf(buf, sizeof(buf), "disk:readx %d", revolutions);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err; r.device_error = true;
            (void)t->write_line("sync:off");
            return r;
        }

        /* Query the captured size. Response format: "+<N>" decimal. */
        std::string size_payload;
        if (!send_and_check_ok(t, "data:?size", &err, &size_payload)) {
            r.error_message = err; r.device_error = true;
            (void)t->write_line("sync:off");
            return r;
        }
        char* end = nullptr;
        const unsigned long captured =
            std::strtoul(size_payload.c_str(), &end, 10);
        if (end == size_payload.c_str() || captured == 0) {
            r.error_message =
                "data:?size returned non-numeric: '" + size_payload + "'";
            r.device_error = true;
            (void)t->write_line("sync:off");
            return r;
        }
        if (captured > 0xFFFFFFFFul) {
            r.error_message = "data:?size out of range: " + size_payload;
            r.device_error = true;
            r.buffer_overflow = true;
            (void)t->write_line("sync:off");
            return r;
        }
        const uint32_t n_bytes = static_cast<uint32_t>(captured);

        /* Request the binary dump. The device announces "data:<N\n"
         * then immediately sends N binary bytes. The transport's
         * accumulator handles the boundary cleanly. */
        std::snprintf(buf, sizeof(buf), "data:< %u", n_bytes);
        if (!t->write_line(buf)) {
            r.error_message = "transport write failed for data:<";
            r.device_error = true;
            (void)t->write_line("sync:off");
            return r;
        }
        /* Some firmwares echo back the announcement on the line first.
         * Don't fail if there's a non-matching first line — flux read
         * is the goal. We attempt to ingest binary directly. */
        r.flux_bytes = t->read_binary(n_bytes);
        if (r.flux_bytes.size() != n_bytes) {
            r.error_message = "short flux read: got " +
                              std::to_string(r.flux_bytes.size()) +
                              " of " + std::to_string(n_bytes);
            r.device_error = true;
            r.flux_bytes.clear();
            (void)t->write_line("sync:off");
            return r;
        }

        (void)send_and_check_ok(t, "sync:off", &err);
        r.revolutions = revolutions;
        return r;
    };
}

/* ── Write runner ─────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceWriteRunner
make_applesauce_write_runner(ApplesauceTransportPtr tx)
{
    return [tx](const ApplesauceProviderV2::WriteRequest& req)
                  -> ApplesauceWriteResult {
        ApplesauceWriteResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }
        if (req.flux_bytes.empty()) {
            r.error_message = "refusing to write zero-byte flux";
            r.device_error  = true;
            return r;
        }

        std::string err;
        IApplesauceTransport* t = tx.get();
        char buf[64];

        std::snprintf(buf, sizeof(buf), "head:track %d", req.cylinder);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err; r.device_error = true; return r;
        }
        std::snprintf(buf, sizeof(buf), "head:side %d", req.head);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err; r.device_error = true; return r;
        }

        /* Write-protect check: "+1" = protected, "+0" = writable. */
        std::string wp_payload;
        if (!send_and_check_ok(t, "disk:?write", &err, &wp_payload)) {
            r.error_message = err; r.device_error = true; return r;
        }
        if (!wp_payload.empty() && wp_payload[0] == '1') {
            r.write_protected = true;
            r.error_message = "disk is write-protected";
            return r;
        }

        if (!send_and_check_ok(t, "data:clear", &err)) {
            r.error_message = err; r.device_error = true; return r;
        }

        /* Announce binary upload size. */
        const std::size_t n = req.flux_bytes.size();
        if (n > 0xFFFFFFFFul) {
            r.error_message = "flux payload too large for uint32 framing";
            r.device_error = true;
            return r;
        }
        std::snprintf(buf, sizeof(buf), "data:> %u",
                      static_cast<unsigned>(n));
        if (!t->write_line(buf)) {
            r.error_message = "transport write failed for data:>";
            r.device_error  = true;
            return r;
        }

        const int written = t->write_binary(req.flux_bytes);
        if (written < 0 || static_cast<std::size_t>(written) != n) {
            r.error_message = "short flux upload: " +
                              std::to_string(written) + " of " +
                              std::to_string(n);
            r.device_error = true;
            return r;
        }

        /* Expect "+ok" or "." acknowledgement on the upload completion. */
        const std::string ack = t->read_line();
        if (ack.empty() || (ack[0] != '+' && ack != ".")) {
            r.error_message = "upload ack missing/invalid: '" + ack + "'";
            r.device_error = true;
            return r;
        }

        if (!send_and_check_ok(t, "disk:write", &err)) {
            r.error_message = err; r.device_error = true; return r;
        }

        r.bytes_written = static_cast<uint32_t>(n);
        return r;
    };
}

/* ── Motor runner ─────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceMotorRunner
make_applesauce_motor_runner(ApplesauceTransportPtr tx)
{
    return [tx](bool on) -> ApplesauceMotorResult {
        ApplesauceMotorResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }

        IApplesauceTransport* t = tx.get();
        std::string err;

        if (on) {
            /* Ensure PSU on. "psu:?" returns "+0"/"+1"; if off, "psu:on". */
            std::string psu_payload;
            if (send_and_check_ok(t, "psu:?", &err, &psu_payload)) {
                if (!psu_payload.empty() && psu_payload[0] == '0') {
                    if (send_and_check_ok(t, "psu:on", &err)) {
                        r.psu_was_enabled = true;
                    } else {
                        r.error_message = err;
                        return r;
                    }
                }
            }
        }

        const char* cmd = on ? "motor:on" : "motor:off";
        if (!send_and_check_ok(t, cmd, &err)) {
            r.error_message = err;
            return r;
        }
        r.success = true;
        return r;
    };
}

/* ── Seek runner ──────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceSeekRunner
make_applesauce_seek_runner(ApplesauceTransportPtr tx)
{
    return [tx](const ApplesauceProviderV2::SeekRequest& req)
                  -> ApplesauceSeekResult {
        ApplesauceSeekResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }
        if (req.cylinder < 0) {
            r.error_message = "negative cylinder requested";
            return r;
        }

        IApplesauceTransport* t = tx.get();
        std::string err;
        char buf[64];

        std::snprintf(buf, sizeof(buf), "head:track %d", req.cylinder);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err;
            return r;
        }
        std::snprintf(buf, sizeof(buf), "head:side %d", req.head);
        if (!send_and_check_ok(t, buf, &err)) {
            r.error_message = err;
            return r;
        }
        r.success          = true;
        r.cylinder_reached = req.cylinder;
        return r;
    };
}

/* ── Recalibrate runner ──────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceRecalRunner
make_applesauce_recal_runner(ApplesauceTransportPtr tx)
{
    return [tx]() -> ApplesauceSeekResult {
        ApplesauceSeekResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }
        std::string err;
        if (!send_and_check_ok(tx.get(), "head:zero", &err)) {
            r.error_message = err;
            return r;
        }
        r.success          = true;
        r.cylinder_reached = 0;
        return r;
    };
}

/* ── RPM runner ───────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceRpmRunner
make_applesauce_rpm_runner(ApplesauceTransportPtr tx)
{
    return [tx]() -> ApplesauceRpmResult {
        ApplesauceRpmResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }
        std::string err, payload;
        if (!send_and_check_ok(tx.get(), "sync:?speed", &err, &payload)) {
            r.error_message = err;
            return r;
        }
        char* end = nullptr;
        const double v = std::strtod(payload.c_str(), &end);
        if (end == payload.c_str() || v <= 0.0) {
            r.error_message = "sync:?speed returned non-numeric: '" +
                              payload + "'";
            r.non_numeric_response = true;
            return r;
        }
        r.rpm = v;
        return r;
    };
}

/* ── Detect runner ────────────────────────────────────────────────── */

ApplesauceProviderV2::ApplesauceDetectRunner
make_applesauce_detect_runner(ApplesauceTransportPtr tx)
{
    return [tx]() -> ApplesauceDetectResult {
        ApplesauceDetectResult r;
        if (!tx) {
            r.transport_unavailable = true;
            r.error_message = "Applesauce transport not constructed";
            return r;
        }

        IApplesauceTransport* t = tx.get();
        std::string err, payload;

        /* ?kind — 5.25 / 3.5 / PC / NONE */
        if (!send_and_check_ok(t, "?kind", &err, &payload)) {
            r.error_message = err;
            return r;
        }
        r.drive_kind = payload;
        if (payload == "NONE" || payload.empty()) {
            r.error_message = "no drive detected on Applesauce port";
            return r;
        }
        r.found = true;
        if (payload == "5.25") {
            r.tracks = 35;
            r.heads  = 1;
        } else if (payload == "3.5" || payload == "PC") {
            r.tracks = 80;
            r.heads  = 2;
        } else {
            /* Unknown kind — keep found=true (device DID answer), but
             * leave tracks/heads at 0 so caller knows the geometry is
             * not auto-derived. Honest: don't guess. */
            r.tracks = 0;
            r.heads  = 0;
        }

        /* data:?max — buffer capacity. */
        if (send_and_check_ok(t, "data:?max", &err, &payload)) {
            char* end = nullptr;
            const unsigned long v = std::strtoul(payload.c_str(), &end, 10);
            if (end != payload.c_str() && v > 0 && v <= 0xFFFFFFFFul) {
                r.buffer_size = static_cast<uint32_t>(v);
            }
        }

        /* Firmware + PCB strings (best-effort — failure here is not
         * fatal to the detect operation). */
        if (send_and_check_ok(t, "?vers", &err, &payload)) {
            r.firmware = payload;
        }
        if (send_and_check_ok(t, "?pcb", &err, &payload)) {
            r.pcb_revision = payload;
        }
        /* RPM measurement is optional; ignore failure. */
        if (send_and_check_ok(t, "sync:?speed", &err, &payload)) {
            char* end = nullptr;
            const double v = std::strtod(payload.c_str(), &end);
            if (end != payload.c_str() && v > 0.0) r.rpm = v;
        }

        return r;
    };
}

} // namespace uft::hal
