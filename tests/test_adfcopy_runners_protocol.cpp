/**
 * @file test_adfcopy_runners_protocol.cpp
 * @brief Protocol-level unit tests for the 5 ADFCopy runner-factories (MF-252).
 *
 * Mirrors test_applesauce_runners_protocol but for ADFCopy's BINARY
 * protocol (single-byte commands, 1-byte status responses, 3-byte
 * READ_FLUX header + binary payload).
 *
 * Hardware-free — drives a ScriptedADFCopyTransport that asserts byte
 * sequences match and replays scripted response bytes.
 */
#include "hardware_providers/adfcopy_provider_v2.h"
#include "hardware_providers/adfcopy_serial_runners.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace uft::hal;

/* ─── Test harness ─────────────────────────────────────────────────── */

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

/* ─── ScriptedADFCopyTransport ─────────────────────────────────────── */

class ScriptedADFCopyTransport : public IADFCopyTransport {
public:
    struct WriteStage {
        std::vector<uint8_t> expected_bytes;
    };
    struct ReadStage {
        std::vector<uint8_t> bytes_to_return;
    };

    void expect_write(std::vector<uint8_t> bytes) {
        m_writes.push_back({std::move(bytes)});
    }
    void queue_read(std::vector<uint8_t> bytes) {
        m_reads.push_back({std::move(bytes)});
    }

    bool failed = false;
    std::string script_failure;

    int write_bytes(const std::vector<uint8_t>& data) override {
        if (m_writes.empty()) {
            failed = true;
            script_failure = "transport script exhausted at write";
            return -1;
        }
        const WriteStage stage = m_writes.front();
        m_writes.pop_front();
        if (stage.expected_bytes != data) {
            failed = true;
            script_failure = "write mismatch (expected " +
                             std::to_string(stage.expected_bytes.size()) +
                             " bytes, got " +
                             std::to_string(data.size()) + ")";
            return -1;
        }
        m_last_write = data;
        return static_cast<int>(data.size());
    }

    std::vector<uint8_t> read_bytes(uint32_t n_bytes,
                                    int /*timeout_ms*/ = 5000) override {
        if (m_reads.empty()) return {};
        std::vector<uint8_t> chunk = std::move(m_reads.front().bytes_to_return);
        m_reads.pop_front();
        if (chunk.size() > n_bytes) chunk.resize(n_bytes);
        return chunk;
    }

    std::string read_until_newline(int /*timeout_ms*/ = 3000) override {
        if (m_reads.empty()) return std::string();
        std::vector<uint8_t> chunk = std::move(m_reads.front().bytes_to_return);
        m_reads.pop_front();
        return std::string(chunk.begin(), chunk.end());
    }

    std::size_t pending_writes() const { return m_writes.size(); }
    const std::vector<uint8_t>& last_write() const { return m_last_write; }

private:
    std::deque<WriteStage> m_writes;
    std::deque<ReadStage>  m_reads;
    std::vector<uint8_t>   m_last_write;
};

/* ─── Detect runner ────────────────────────────────────────────────── */

TEST(detect_disk_present_no_wp_motor_on_flux_capable) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdGetStatus});
    tx->queue_read({static_cast<uint8_t>(
        kAdfcStatusDiskPresent | kAdfcStatusMotorOn | kAdfcStatusFluxCapable)});

    auto runner = make_adfcopy_detect_runner(tx);
    ADFCopyDetectResult r = runner();

    CHECK(!tx->failed);
    CHECK(r.disk_present);
    CHECK(!r.write_protected);
    CHECK(r.motor_on);
    CHECK(r.flux_capable);
}

TEST(detect_write_protected_disk) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdGetStatus});
    tx->queue_read({static_cast<uint8_t>(
        kAdfcStatusDiskPresent | kAdfcStatusWriteProt)});

    auto runner = make_adfcopy_detect_runner(tx);
    ADFCopyDetectResult r = runner();
    CHECK(r.disk_present);
    CHECK(r.write_protected);
    CHECK(!r.motor_on);
}

TEST(detect_null_transport) {
    ADFCopyTransportPtr null_tx;
    auto runner = make_adfcopy_detect_runner(null_tx);
    ADFCopyDetectResult r = runner();
    CHECK(r.transport_unavailable);
}

TEST(detect_timeout_returns_error) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdGetStatus});
    /* No queued read — read returns empty, read_status_byte returns 0xFF */
    auto runner = make_adfcopy_detect_runner(tx);
    ADFCopyDetectResult r = runner();
    CHECK(!r.disk_present);
    CHECK(!r.error_message.empty());
}

/* ─── Motor runner ─────────────────────────────────────────────────── */

TEST(motor_on_sends_INIT_expects_O) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdInit});
    tx->queue_read({kAdfcRspOk});

    auto runner = make_adfcopy_motor_runner(tx);
    ADFCopyMotorResult r = runner(true);
    CHECK(!tx->failed);
    CHECK(r.success);
}

TEST(motor_off_is_best_effort_no_command) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    /* No expected writes — motor-off must NOT send anything. */
    auto runner = make_adfcopy_motor_runner(tx);
    ADFCopyMotorResult r = runner(false);
    CHECK(r.success);
    CHECK(tx->pending_writes() == 0);
}

TEST(motor_on_error_response_propagated) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdInit});
    tx->queue_read({kAdfcRspError});

    auto runner = make_adfcopy_motor_runner(tx);
    ADFCopyMotorResult r = runner(true);
    CHECK(!r.success);
    CHECK(!r.error_message.empty());
}

/* ─── Seek runner ──────────────────────────────────────────────────── */

TEST(seek_sends_SEEK_with_track_byte) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    /* cylinder=17 head=1 → track = 17*2+1 = 35 */
    tx->expect_write({kAdfcCmdSeek, 35});
    tx->queue_read({kAdfcRspOk});

    auto runner = make_adfcopy_seek_runner(tx);
    ADFCopyProviderV2::SeekRequest req;
    req.cylinder = 17;
    req.current_head = 1;
    ADFCopySeekResult r = runner(req);
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(r.cylinder_reached == 17);
}

TEST(seek_rejects_negative_cylinder_without_sending) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    auto runner = make_adfcopy_seek_runner(tx);
    ADFCopyProviderV2::SeekRequest req;
    req.cylinder = -1;
    ADFCopySeekResult r = runner(req);
    CHECK(!r.success);
    CHECK(tx->pending_writes() == 0);
}

TEST(seek_max_cylinder_83_head_1_is_track_167) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdSeek, 167});
    tx->queue_read({kAdfcRspOk});
    auto runner = make_adfcopy_seek_runner(tx);
    ADFCopyProviderV2::SeekRequest req;
    req.cylinder = 83;
    req.current_head = 1;
    ADFCopySeekResult r = runner(req);
    CHECK(r.success);
}

/* ─── Recalibrate runner ───────────────────────────────────────────── */

TEST(recal_sends_SEEK_zero) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    tx->expect_write({kAdfcCmdSeek, 0});
    tx->queue_read({kAdfcRspOk});

    auto runner = make_adfcopy_recal_runner(tx);
    ADFCopySeekResult r = runner();
    CHECK(!tx->failed);
    CHECK(r.success);
    CHECK(r.cylinder_reached == 0);
}

/* ─── Read runner ──────────────────────────────────────────────────── */

TEST(read_full_flux_round_trip) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    /* cylinder=0 head=0 → track=0, revs=2 */
    tx->expect_write({kAdfcCmdReadFlux, 0, 2});
    /* 3-byte header: status=OK, length_hi=0x04, length_lo=0x00 → 1024 */
    tx->queue_read({kAdfcRspOk, 0x04, 0x00});
    /* 1024 fake flux bytes */
    tx->queue_read(std::vector<uint8_t>(1024, 0xAB));

    auto runner = make_adfcopy_read_runner(tx);
    ADFCopyProviderV2::ReadRequest req;
    req.cylinder = 0;
    req.head = 0;
    req.revolutions = 2;
    ADFCopyReadResult r = runner(req);

    CHECK(!tx->failed);
    CHECK(!r.device_error);
    CHECK(r.flux_bytes.size() == 1024);
    CHECK(r.flux_bytes[0] == 0xAB);
    CHECK(r.revolutions == 2);
}

TEST(read_no_disk_returns_no_disk_flag) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    /* req has default revolutions=2 */
    tx->expect_write({kAdfcCmdReadFlux, 0, 2});
    tx->queue_read({kAdfcRspNoDisk, 0, 0});

    auto runner = make_adfcopy_read_runner(tx);
    ADFCopyProviderV2::ReadRequest req;
    ADFCopyReadResult r = runner(req);

    CHECK(r.device_error);
    CHECK(r.no_disk);
    CHECK(r.flux_bytes.empty());
}

TEST(read_short_payload_returns_device_error_no_synthesis) {
    auto tx = std::make_shared<ScriptedADFCopyTransport>();
    /* req has default revolutions=2 */
    tx->expect_write({kAdfcCmdReadFlux, 0, 2});
    tx->queue_read({kAdfcRspOk, 0x04, 0x00});  /* announces 1024 */
    tx->queue_read(std::vector<uint8_t>(512, 0xCC));  /* only 512 */

    auto runner = make_adfcopy_read_runner(tx);
    ADFCopyProviderV2::ReadRequest req;
    ADFCopyReadResult r = runner(req);

    CHECK(r.device_error);
    CHECK(r.flux_bytes.empty());  /* never synthesises */
    CHECK(!r.error_message.empty());
}

TEST(read_null_transport_unavailable) {
    ADFCopyTransportPtr null_tx;
    auto runner = make_adfcopy_read_runner(null_tx);
    ADFCopyProviderV2::ReadRequest req;
    ADFCopyReadResult r = runner(req);
    CHECK(r.transport_unavailable);
    CHECK(r.flux_bytes.empty());
}

/* ─── Main ─────────────────────────────────────────────────────────── */

int main() {
    std::printf("=== ADFCopy runners protocol tests (MF-252) ===\n");

    RUN(detect_disk_present_no_wp_motor_on_flux_capable);
    RUN(detect_write_protected_disk);
    RUN(detect_null_transport);
    RUN(detect_timeout_returns_error);

    RUN(motor_on_sends_INIT_expects_O);
    RUN(motor_off_is_best_effort_no_command);
    RUN(motor_on_error_response_propagated);

    RUN(seek_sends_SEEK_with_track_byte);
    RUN(seek_rejects_negative_cylinder_without_sending);
    RUN(seek_max_cylinder_83_head_1_is_track_167);

    RUN(recal_sends_SEEK_zero);

    RUN(read_full_flux_round_trip);
    RUN(read_no_disk_returns_no_disk_flag);
    RUN(read_short_payload_returns_device_error_no_synthesis);
    RUN(read_null_transport_unavailable);

    std::printf("\nResults: %d passed, %d failed (of %d)\n",
                g_tests_passed, g_tests_run - g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
