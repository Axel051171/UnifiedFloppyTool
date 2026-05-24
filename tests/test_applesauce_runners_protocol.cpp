/**
 * @file test_applesauce_runners_protocol.cpp
 * @brief Protocol-level unit tests for the 7 Applesauce runner-factories (MF-251).
 *
 * Drives a `ScriptedApplesauceTransport` that replays a programmed
 * sequence of expected-command / scripted-response pairs. Each test
 * exercises one runner, asserting:
 *   1. it sends the documented Applesauce commands in the correct order
 *   2. it parses each response correctly
 *   3. on protocol deviation, it returns a failure result with
 *      device_error = true and a populated error_message — never
 *      synthesised data
 *
 * Hardware-free. Validates the protocol layer (applesauce_serial_runners.cpp)
 * standalone, so a future bench-verification on real hardware
 * (docs/M3_APPLESAUCE_TRANSPORT.md §5) only needs to validate the
 * QSerialPort I/O layer + the real device's protocol conformance, not
 * the UFT-side parsing.
 *
 * Follows the same standalone-test pattern as test_applesauce_provider_v2.cpp
 * — no Qt MOC, no QObject, no QTest framework. Custom assertions.
 */
#include "hardware_providers/applesauce_provider_v2.h"
#include "hardware_providers/applesauce_serial_runners.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace uft::hal;

/* ─── Test harness (no Qt) ─────────────────────────────────────────── */

static int  g_tests_run    = 0;
static int  g_tests_passed = 0;
static bool g_test_failed  = false;

#define TEST(name) static void test_##name()
#define RUN(name)  do { \
    std::printf("  [TEST] %s ... ", #name); \
    g_test_failed = false; \
    g_tests_run++; \
    test_##name(); \
    if (!g_test_failed) { \
        std::printf("PASS\n"); g_tests_passed++; \
    } \
} while (0)
#define CHECK(cond) do { \
    if (!(cond)) { \
        std::printf("FAIL\n    at line %d: %s\n", __LINE__, #cond); \
        g_test_failed = true; return; \
    } \
} while (0)

/* ─── ScriptedApplesauceTransport ──────────────────────────────────── */

class ScriptedApplesauceTransport : public IApplesauceTransport {
public:
    struct Stage {
        std::string expected_command;
        std::string response_line;
    };

    void expect(const std::string& cmd, const std::string& resp) {
        m_stages.push_back({cmd, resp});
    }
    void expect_binary(std::vector<uint8_t> data) {
        m_binary_queue.push_back(std::move(data));
    }

    bool        failed = false;
    std::string script_failure;

    bool write_line(const std::string& command) override {
        if (m_stages.empty()) {
            failed = true;
            script_failure =
                "transport script exhausted at: '" + command + "'";
            return false;
        }
        const Stage stage = m_stages.front();
        m_stages.pop_front();
        if (stage.expected_command != command) {
            failed = true;
            script_failure =
                "expected '" + stage.expected_command +
                "', got '" + command + "'";
            return false;
        }
        m_pending_response     = stage.response_line;
        m_has_pending_response = true;
        return true;
    }

    std::string read_line(int /*timeout_ms*/ = 3000) override {
        if (!m_has_pending_response) return std::string();
        std::string out = m_pending_response;
        m_pending_response.clear();
        m_has_pending_response = false;
        return out;
    }

    int write_binary(const std::vector<uint8_t>& data) override {
        m_last_binary_write = data;
        return static_cast<int>(data.size());
    }

    std::vector<uint8_t> read_binary(uint32_t n_bytes,
                                     int /*timeout_ms*/ = 30000) override {
        if (m_binary_queue.empty()) return {};
        std::vector<uint8_t> chunk = std::move(m_binary_queue.front());
        m_binary_queue.pop_front();
        if (chunk.size() > n_bytes) chunk.resize(n_bytes);
        return chunk;
    }

    std::size_t pending_stages() const { return m_stages.size(); }
    const std::vector<uint8_t>& last_binary_write() const {
        return m_last_binary_write;
    }

private:
    std::deque<Stage> m_stages;
    std::deque<std::vector<uint8_t>> m_binary_queue;
    std::string m_pending_response;
    bool        m_has_pending_response = false;
    std::vector<uint8_t> m_last_binary_write;
};

static bool approx_eq(double a, double b, double tol = 1e-9) {
    return std::fabs(a - b) < tol;
}

/* ─── Detect runner tests ──────────────────────────────────────────── */

TEST(detect_525_drive_populates_full_result) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("?kind",       "+5.25");
    tx->expect("data:?max",   "+163840");
    tx->expect("?vers",       "+as-firmware-1.42");
    tx->expect("?pcb",        "+rev-c");
    tx->expect("sync:?speed", "+300.10");

    auto runner = make_applesauce_detect_runner(tx);
    ApplesauceDetectResult r = runner();

    CHECK(!tx->failed);
    CHECK(tx->pending_stages() == 0);
    CHECK(r.found);
    CHECK(r.drive_kind == "5.25");
    CHECK(r.tracks == 35);
    CHECK(r.heads == 1);
    CHECK(r.buffer_size == 163840u);
    CHECK(r.firmware == "as-firmware-1.42");
    CHECK(r.pcb_revision == "rev-c");
    CHECK(approx_eq(r.rpm, 300.10));
}

TEST(detect_no_drive_returns_not_found) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("?kind", "+NONE");
    auto runner = make_applesauce_detect_runner(tx);
    ApplesauceDetectResult r = runner();

    CHECK(!tx->failed);
    CHECK(!r.found);
    CHECK(!r.error_message.empty());
}

TEST(detect_null_transport_is_unavailable) {
    ApplesauceTransportPtr null_tx;
    auto runner = make_applesauce_detect_runner(null_tx);
    ApplesauceDetectResult r = runner();
    CHECK(r.transport_unavailable);
    CHECK(!r.found);
}

/* ─── Motor runner tests ───────────────────────────────────────────── */

TEST(motor_on_with_psu_already_enabled) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("psu:?",    "+1");
    tx->expect("motor:on", ".");
    auto runner = make_applesauce_motor_runner(tx);
    ApplesauceMotorResult r = runner(true);
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(!r.psu_was_enabled);
}

TEST(motor_on_with_psu_off_enables_psu_first) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("psu:?",    "+0");
    tx->expect("psu:on",   ".");
    tx->expect("motor:on", ".");
    auto runner = make_applesauce_motor_runner(tx);
    ApplesauceMotorResult r = runner(true);
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(r.psu_was_enabled);
}

TEST(motor_off_skips_psu_check) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("motor:off", ".");
    auto runner = make_applesauce_motor_runner(tx);
    ApplesauceMotorResult r = runner(false);
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(!r.psu_was_enabled);
}

TEST(motor_device_error_propagated) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("motor:off", "-stalled");
    auto runner = make_applesauce_motor_runner(tx);
    ApplesauceMotorResult r = runner(false);
    CHECK(!r.success);
    CHECK(!r.error_message.empty());
}

/* ─── Seek runner tests ────────────────────────────────────────────── */

TEST(seek_sends_track_then_side) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("head:track 17", ".");
    tx->expect("head:side 1",   ".");
    auto runner = make_applesauce_seek_runner(tx);
    ApplesauceProviderV2::SeekRequest req;
    req.cylinder = 17;
    req.head     = 1;
    ApplesauceSeekResult r = runner(req);
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(r.cylinder_reached == 17);
}

TEST(seek_rejects_negative_cylinder) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    auto runner = make_applesauce_seek_runner(tx);
    ApplesauceProviderV2::SeekRequest req;
    req.cylinder = -1;
    ApplesauceSeekResult r = runner(req);
    CHECK(!r.success);
    CHECK(!r.error_message.empty());
    CHECK(tx->pending_stages() == 0);
}

/* ─── Recalibrate runner test ──────────────────────────────────────── */

TEST(recal_sends_head_zero) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("head:zero", ".");
    auto runner = make_applesauce_recal_runner(tx);
    ApplesauceSeekResult r = runner();
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(r.cylinder_reached == 0);
}

/* ─── RPM runner tests ─────────────────────────────────────────────── */

TEST(rpm_parses_numeric_response) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("sync:?speed", "+299.85");
    auto runner = make_applesauce_rpm_runner(tx);
    ApplesauceRpmResult r = runner();
    CHECK(!tx->failed);
    CHECK(approx_eq(r.rpm, 299.85));
    CHECK(!r.non_numeric_response);
}

TEST(rpm_rejects_non_numeric_response) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("sync:?speed", "+nodata");
    auto runner = make_applesauce_rpm_runner(tx);
    ApplesauceRpmResult r = runner();
    CHECK(r.rpm == 0.0);
    CHECK(r.non_numeric_response);
    CHECK(!r.error_message.empty());
}

/* ─── Read runner tests ────────────────────────────────────────────── */

TEST(read_full_flux_round_trip) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("sync:on",       ".");
    tx->expect("head:track 0",  ".");
    tx->expect("head:side 0",   ".");
    tx->expect("disk:readx 2",  ".");
    tx->expect("data:?size",    "+1024");
    tx->expect("data:< 1024",   ".");
    std::vector<uint8_t> flux(1024, 0x42);
    tx->expect_binary(std::move(flux));
    tx->expect("sync:off",      ".");

    auto runner = make_applesauce_read_runner(tx);
    ApplesauceProviderV2::ReadRequest req;
    req.cylinder    = 0;
    req.head        = 0;
    req.revolutions = 2;
    ApplesauceReadResult r = runner(req);

    CHECK(!tx->failed);
    CHECK(!r.device_error);
    CHECK(!r.transport_unavailable);
    CHECK(r.flux_bytes.size() == 1024);
    CHECK(r.flux_bytes[0] == 0x42);
    CHECK(r.revolutions == 2);
}

TEST(read_short_flux_returns_device_error_no_synthesis) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("sync:on",       ".");
    tx->expect("head:track 0",  ".");
    tx->expect("head:side 0",   ".");
    tx->expect("disk:readx 1",  ".");
    tx->expect("data:?size",    "+1024");
    tx->expect("data:< 1024",   ".");
    std::vector<uint8_t> short_flux(512, 0xAA);
    tx->expect_binary(std::move(short_flux));
    tx->expect("sync:off",      ".");

    auto runner = make_applesauce_read_runner(tx);
    ApplesauceProviderV2::ReadRequest req;
    ApplesauceReadResult r = runner(req);

    CHECK(r.device_error);
    CHECK(r.flux_bytes.empty());  /* never synthesises */
    CHECK(!r.error_message.empty());
}

/* ─── Write runner tests ───────────────────────────────────────────── */

TEST(write_full_flux_round_trip) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    std::vector<uint8_t> payload(256, 0x55);
    tx->expect("head:track 3", ".");
    tx->expect("head:side 0",  ".");
    tx->expect("disk:?write",  "+0");
    tx->expect("data:clear",   ".");
    tx->expect("data:> 256",   ".");
    tx->expect("disk:write",   ".");

    auto runner = make_applesauce_write_runner(tx);
    ApplesauceProviderV2::WriteRequest req;
    req.cylinder   = 3;
    req.head       = 0;
    req.flux_bytes = payload;
    ApplesauceWriteResult r = runner(req);

    CHECK(!tx->failed);
    CHECK(!r.write_protected);
    CHECK(!r.device_error);
    CHECK(r.bytes_written == 256u);
    CHECK(tx->last_binary_write().size() == 256);
    CHECK(tx->last_binary_write()[0] == 0x55);
}

TEST(write_protected_disk_aborts_cleanly) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    tx->expect("head:track 0", ".");
    tx->expect("head:side 0",  ".");
    tx->expect("disk:?write",  "+1");
    auto runner = make_applesauce_write_runner(tx);
    ApplesauceProviderV2::WriteRequest req;
    req.flux_bytes = {0x42};
    ApplesauceWriteResult r = runner(req);

    CHECK(r.write_protected);
    CHECK(r.bytes_written == 0u);
    CHECK(!r.error_message.empty());
}

TEST(write_zero_bytes_refused) {
    auto tx = std::make_shared<ScriptedApplesauceTransport>();
    auto runner = make_applesauce_write_runner(tx);
    ApplesauceProviderV2::WriteRequest req;
    ApplesauceWriteResult r = runner(req);
    CHECK(r.device_error);
    CHECK(r.bytes_written == 0u);
    CHECK(tx->pending_stages() == 0);
}

/* ─── Main ─────────────────────────────────────────────────────────── */

int main() {
    std::printf("=== Applesauce runners protocol tests (MF-251) ===\n");

    RUN(detect_525_drive_populates_full_result);
    RUN(detect_no_drive_returns_not_found);
    RUN(detect_null_transport_is_unavailable);

    RUN(motor_on_with_psu_already_enabled);
    RUN(motor_on_with_psu_off_enables_psu_first);
    RUN(motor_off_skips_psu_check);
    RUN(motor_device_error_propagated);

    RUN(seek_sends_track_then_side);
    RUN(seek_rejects_negative_cylinder);

    RUN(recal_sends_head_zero);

    RUN(rpm_parses_numeric_response);
    RUN(rpm_rejects_non_numeric_response);

    RUN(read_full_flux_round_trip);
    RUN(read_short_flux_returns_device_error_no_synthesis);

    RUN(write_full_flux_round_trip);
    RUN(write_protected_disk_aborts_cleanly);
    RUN(write_zero_bytes_refused);

    std::printf("\nResults: %d passed, %d failed (of %d)\n",
                g_tests_passed, g_tests_run - g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
