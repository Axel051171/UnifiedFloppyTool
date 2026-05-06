/**
 * @file test_mock_hardware.cpp
 * @brief Smoke test for the three transport mocks (P1.6).
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.6
 *
 * The mocks live in tests/mock_hardware/ and are consumed by the
 * conformance harness (P1.5) + per-provider tests (P1.8+). This file
 * is the contract: if it fails, ALL conformance results that depend on
 * a mock are invalid.
 *
 * Coverage matrix per mock:
 *
 *   - happy path (script → exercise → verify recorded sends + reply)
 *   - assert_consumed() on a fully-drained queue passes
 *   - assert_consumed() on a non-empty queue throws
 *   - read-past-end throws (forensic-gap signal, not a hang)
 *   - injected error / failure path returns the right shape
 *
 * Header-only test, no Qt deps. Joins _HEADER_ONLY_CPP_TESTS.
 */

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "mock_hardware/byte_buffer.h"
#include "mock_hardware/usb_loopback_mock.h"
#include "mock_hardware/subprocess_mock.h"
#include "mock_hardware/serial_mock.h"

using namespace uft::tests::mocks;

/* Lightweight in-test assertion that increments a counter and reports
 * the failed expression by source-text.  Identical idiom to the rest
 * of the header-only tests in this repo (test_hal_foundation etc.). */
static int g_errors = 0;
/* Variadic so braced-initializer-lists with commas (`ByteVec{a,b,c}`)
 * appearing inside the assertion expression do not split into multiple
 * macro args. */
#define UFT_CHECK(...)                                                   \
    do {                                                                 \
        if (!static_cast<bool>(__VA_ARGS__)) {                           \
            ++g_errors;                                                  \
            std::fprintf(stderr,                                         \
                "[mock_hardware] FAIL %s:%d  %s\n",                      \
                __FILE__, __LINE__, #__VA_ARGS__);                       \
        }                                                                \
    } while (0)

/* ────────────────────────────────────────────────────────────────────
 *  UsbLoopbackMock
 * ──────────────────────────────────────────────────────────────────── */
static void test_usb_loopback_happy_path() {
    UsbLoopbackMock m;
    m.queue_bulk_reply({ 0x42, 0x43 });

    /* Provider sends a header byte ... */
    std::uint8_t header[] = { 0xAA };
    int rc = m.bulk_out(/*ep=*/0x01, header, 1);
    UFT_CHECK(rc == 1);
    UFT_CHECK(m.last_bulk_endpoint() == 0x01);

    /* ... then reads the scripted reply. */
    std::uint8_t buf[8] = {};
    int n = m.bulk_in(/*ep=*/0x82, buf, sizeof(buf));
    UFT_CHECK(n == 2);
    UFT_CHECK(buf[0] == 0x42 && buf[1] == 0x43);
    UFT_CHECK(m.last_bulk_endpoint() == 0x82);

    /* Recorded sends match. */
    UFT_CHECK(m.bulk_out_log().count() == 1);
    UFT_CHECK(m.bulk_out_log().at(0) == ByteVec{0xAA});

    /* assert_consumed passes when drained. */
    bool threw = false;
    try { m.assert_consumed(); }
    catch (...) { threw = true; }
    UFT_CHECK(!threw);
}

static void test_usb_loopback_unconsumed_queue_throws() {
    UsbLoopbackMock m;
    m.queue_bulk_reply({ 0x01 });
    bool threw = false;
    try { m.assert_consumed(); }
    catch (const std::logic_error &) { threw = true; }
    UFT_CHECK(threw);
}

static void test_usb_loopback_read_past_end_throws() {
    UsbLoopbackMock m;
    std::uint8_t buf[4] = {};
    bool threw = false;
    try { m.bulk_in(0x82, buf, sizeof(buf)); }
    catch (const std::out_of_range &) { threw = true; }
    UFT_CHECK(threw);
}

static void test_usb_loopback_injected_error() {
    UsbLoopbackMock m;
    m.inject_error(-7);              /* libusb-style negative rc */
    std::uint8_t hdr[] = { 0xFF };
    int rc = m.bulk_out(0x01, hdr, 1);
    UFT_CHECK(rc == -7);
    /* Error is one-shot — next call works. */
    rc = m.bulk_out(0x01, hdr, 1);
    UFT_CHECK(rc == 1);
}

static void test_usb_loopback_control_transfer_directionality() {
    UsbLoopbackMock m;

    /* host→device (bm_request_type bit 7 = 0): payload recorded */
    std::uint8_t out[] = { 0xDE, 0xAD };
    int rc = m.control_transfer(/*type=*/0x40, /*req=*/0x01,
                                /*val=*/0x0000, /*idx=*/0,
                                out, 2);
    UFT_CHECK(rc == 2);
    UFT_CHECK(m.control_out_log().count() == 1);
    UFT_CHECK(m.control_out_log().at(0) == ByteVec{0xDE, 0xAD});

    /* device→host (bit 7 = 1): drains queue */
    m.queue_control_reply({ 0xBE, 0xEF });
    std::uint8_t in[16] = {};
    rc = m.control_transfer(/*type=*/0xC0, /*req=*/0x02,
                            /*val=*/0, /*idx=*/0,
                            in, sizeof(in));
    UFT_CHECK(rc == 2);
    UFT_CHECK(in[0] == 0xBE && in[1] == 0xEF);

    /* Snapshot of the last header is captured. */
    UFT_CHECK(m.last_control().request == 0x02);
    UFT_CHECK(m.last_control().request_type == 0xC0);
}

/* ────────────────────────────────────────────────────────────────────
 *  SubprocessMock
 * ──────────────────────────────────────────────────────────────────── */
static void test_subprocess_happy_path() {
    SubprocessMock m;
    m.queue_run("DTC v1.7\n");

    auto rr = m.run({ "/usr/bin/dtc", "-i0", "-fkfx" }, /*stdin=*/"");
    UFT_CHECK(rr.exit_code == 0);
    UFT_CHECK(rr.stdout_text == "DTC v1.7\n");
    UFT_CHECK(m.recorded_runs().size() == 1);
    UFT_CHECK(m.recorded_runs().at(0).argv.at(0) == "/usr/bin/dtc");

    bool threw = false;
    try { m.assert_consumed(); }
    catch (...) { threw = true; }
    UFT_CHECK(!threw);
}

static void test_subprocess_argv_subseq_match() {
    SubprocessMock m;
    SubprocessMock::ScriptedRun r;
    r.require_argv_subseq = { "fluxengine", "read" };
    r.stdout_reply = "OK";
    r.exit_code = 0;
    m.queue_run(std::move(r));

    auto rr = m.run({ "fluxengine", "--quiet", "read", "ibm.360" });
    UFT_CHECK(rr.stdout_text == "OK");
    UFT_CHECK(rr.exit_code == 0);
}

static void test_subprocess_argv_subseq_mismatch_throws() {
    SubprocessMock m;
    SubprocessMock::ScriptedRun r;
    r.require_argv_subseq = { "fluxengine", "write" };  /* "write" required */
    m.queue_run(std::move(r));

    bool threw = false;
    try {
        m.run({ "fluxengine", "read" });   /* mismatch */
    } catch (const std::logic_error &) {
        threw = true;
    }
    UFT_CHECK(threw);
}

static void test_subprocess_failure_path() {
    SubprocessMock m;
    m.queue_run_failed("ERROR: device not found\n", /*exit=*/2);

    auto rr = m.run({ "fcimage" });
    UFT_CHECK(rr.exit_code == 2);
    UFT_CHECK(rr.stderr_text == "ERROR: device not found\n");
}

static void test_subprocess_unconsumed_queue_throws() {
    SubprocessMock m;
    m.queue_run("never used");
    bool threw = false;
    try { m.assert_consumed(); }
    catch (const std::logic_error &) { threw = true; }
    UFT_CHECK(threw);
}

/* ────────────────────────────────────────────────────────────────────
 *  SerialMock
 * ──────────────────────────────────────────────────────────────────── */
static void test_serial_line_protocol() {
    SerialMock m;
    m.queue_read_line("READY");
    m.queue_read_line("OK 12345");

    /* Provider writes a command, reads two response lines. */
    int rc = m.write({ 'i', 'd', '?', '\n' });
    UFT_CHECK(rc == 4);

    UFT_CHECK(m.can_read_line());
    UFT_CHECK(m.read_line() == "READY");
    UFT_CHECK(m.read_line() == "OK 12345");

    /* TX recorded. */
    UFT_CHECK(m.tx_log().count() == 1);
    UFT_CHECK(m.tx_log().at(0) == ByteVec{'i', 'd', '?', '\n'});

    bool threw = false;
    try { m.assert_consumed(); }
    catch (...) { threw = true; }
    UFT_CHECK(!threw);
}

static void test_serial_read_all_drains() {
    SerialMock m;
    m.queue_read_bytes({ 0x10, 0x20, 0x30 });

    auto v = m.read_all();
    UFT_CHECK(v == ByteVec{0x10, 0x20, 0x30});
    UFT_CHECK(m.bytes_available() == 0);
}

static void test_serial_read_line_without_newline_throws() {
    SerialMock m;
    m.queue_read_bytes({ 'a', 'b' });   /* no newline */
    bool threw = false;
    try { m.read_line(); }
    catch (const std::out_of_range &) { threw = true; }
    UFT_CHECK(threw);
}

static void test_serial_injected_failure() {
    SerialMock m;
    m.inject_failure();
    int rc = m.write({ 0xAA });
    UFT_CHECK(rc == -1);
    /* One-shot: next write succeeds. */
    rc = m.write({ 0xBB });
    UFT_CHECK(rc == 1);
}

static void test_serial_unread_buffer_throws_on_assert() {
    SerialMock m;
    m.queue_read_bytes({ 0x99 });
    bool threw = false;
    try { m.assert_consumed(); }
    catch (const std::logic_error &) { threw = true; }
    UFT_CHECK(threw);
}

/* ──────────────────────────────────────────────────────────────────── */
int main() {
    /* USB */
    test_usb_loopback_happy_path();
    test_usb_loopback_unconsumed_queue_throws();
    test_usb_loopback_read_past_end_throws();
    test_usb_loopback_injected_error();
    test_usb_loopback_control_transfer_directionality();

    /* Subprocess */
    test_subprocess_happy_path();
    test_subprocess_argv_subseq_match();
    test_subprocess_argv_subseq_mismatch_throws();
    test_subprocess_failure_path();
    test_subprocess_unconsumed_queue_throws();

    /* Serial */
    test_serial_line_protocol();
    test_serial_read_all_drains();
    test_serial_read_line_without_newline_throws();
    test_serial_injected_failure();
    test_serial_unread_buffer_throws_on_assert();

    std::printf("test_mock_hardware: %d errors\n", g_errors);
    return g_errors == 0 ? 0 : 1;
}
