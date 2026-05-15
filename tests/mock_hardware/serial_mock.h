/**
 * @file serial_mock.h
 * @brief QSerialPort-style transport mock — line + raw I/O without Qt.
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.6
 *
 * Used by V2 providers whose backend layer talks line-based or raw
 * bytes over a serial port (ApplesauceProviderV2, ADFCopyProviderV2 —
 * both P1.13/P1.14 tasks).
 *
 * Why not just use a real `QSerialPort` and connect it to a virtual
 * port? Because:
 *   1. Virtual-port setup differs per OS (com0com on Windows, socat on
 *      Linux, no equivalent on macOS) — non-deterministic.
 *   2. Conformance tests are header-only; pulling Qt in here forces the
 *      whole conformance harness into the Qt-test loop, dragging in
 *      `hardware_providers` and ruining isolation.
 *   3. Real QSerialPort is asynchronous (signals), the mock is
 *      synchronous — easier to reason about in conformance tests.
 *
 * V2 providers that use this mock should template their backend layer
 * on a Transport type so both `QSerialPort` and `SerialMock` can be
 * substituted. The interface this mock provides is intentionally a
 * subset of QSerialPort:
 *   - write(QByteArray-shape)        → write(const ByteVec &)
 *   - readAll()                      → read_all()
 *   - readLine() / canReadLine()     → read_line()
 *   - waitForBytesWritten()          → no-op (synchronous)
 *
 * Pure C++20, no Qt. Header-only.
 */
#ifndef UFT_TESTS_MOCK_HARDWARE_SERIAL_MOCK_H
#define UFT_TESTS_MOCK_HARDWARE_SERIAL_MOCK_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "byte_buffer.h"

namespace uft::tests::mocks {

/**
 * @brief Synchronous, scripted serial-port mock.
 */
class SerialMock {
public:
    /* ── Test scenario API ────────────────────────────────────────────── */

    /** Append bytes to the internal RX buffer — they will be returned
     *  by `read_all()` / `read_line()` calls in order. */
    void queue_read_bytes(ByteVec bytes) {
        m_rx.insert(m_rx.end(), bytes.begin(), bytes.end());
    }
    void queue_read_bytes(std::initializer_list<std::uint8_t> bytes) {
        m_rx.insert(m_rx.end(), bytes.begin(), bytes.end());
    }

    /** Append a line — newline ('\n') is added if not already present.
     *  Convenience for line-based protocols (Applesauce text protocol). */
    void queue_read_line(const std::string &line) {
        m_rx.insert(m_rx.end(), line.begin(), line.end());
        if (line.empty() || line.back() != '\n') {
            m_rx.push_back('\n');
        }
    }

    /** Force the next write/read to fail (returns -1 from write,
     *  empty from reads). One-shot. */
    void inject_failure() { m_pending_failure = true; }

    /* ── Recorded TX — for post-hoc inspection ────────────────────────── */
    const RecordedSends &tx_log() const noexcept { return m_tx_log; }

    /** No remaining RX = test consumed everything; if the provider
     *  expected MORE bytes than scripted, `read_line()` already threw. */
    void assert_consumed() const {
        if (!m_rx.empty()) {
            throw std::logic_error(
                "SerialMock: RX buffer still contains " +
                std::to_string(m_rx.size()) +
                " unread bytes at end of test. The provider read fewer "
                "bytes than the test scripted; either queue less, or "
                "exercise more provider code.");
        }
    }

    /* ── Backend API (called by V2 provider's do_* methods) ───────────── */

    /**
     * @return number of bytes "written" (= data.size()), or -1 if
     *         a failure was injected.
     */
    int write(const ByteVec &data) {
        if (m_pending_failure) {
            m_pending_failure = false;
            return -1;
        }
        m_tx_log.record(data);
        return static_cast<int>(data.size());
    }
    int write(const std::uint8_t *data, std::size_t len) {
        if (m_pending_failure) {
            m_pending_failure = false;
            return -1;
        }
        if (data == nullptr) {
            throw std::invalid_argument("SerialMock::write: null data pointer");
        }
        m_tx_log.record(data, len);
        return static_cast<int>(len);
    }

    /**
     * Drains the entire RX buffer. Returns empty vector if RX is empty
     * or if a failure was injected.
     */
    ByteVec read_all() {
        if (m_pending_failure) {
            m_pending_failure = false;
            return {};
        }
        ByteVec out;
        std::swap(out, m_rx);
        return out;
    }

    /**
     * Reads up to (and including) the next '\n'. Returns the line
     * WITHOUT the trailing newline. Throws if the RX buffer is empty
     * AND no newline is ever queued — a forensic-gap signal.
     */
    std::string read_line() {
        if (m_pending_failure) {
            m_pending_failure = false;
            return {};
        }
        auto it = std::find(m_rx.begin(), m_rx.end(), std::uint8_t{'\n'});
        if (it == m_rx.end()) {
            throw std::out_of_range(
                "SerialMock::read_line(): no newline in RX buffer. "
                "Either queue another line via queue_read_line(), or "
                "stop the provider earlier.");
        }
        std::string line(m_rx.begin(), it);
        m_rx.erase(m_rx.begin(), it + 1);  /* drop the newline */
        return line;
    }

    /** Number of bytes currently buffered (post-write, pre-read). */
    std::size_t bytes_available() const noexcept { return m_rx.size(); }
    bool can_read_line() const noexcept {
        return std::find(m_rx.begin(), m_rx.end(), std::uint8_t{'\n'}) != m_rx.end();
    }

private:
    ByteVec m_rx;          /* what the provider will read */
    RecordedSends m_tx_log; /* what the provider has written */
    bool m_pending_failure = false;
};

}  // namespace uft::tests::mocks

#endif  // UFT_TESTS_MOCK_HARDWARE_SERIAL_MOCK_H
