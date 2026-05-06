/**
 * @file byte_buffer.h
 * @brief Shared scripted-byte-stream helper for the three transport mocks.
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.6
 *
 * The contract is the same across `usb_loopback_mock`, `subprocess_mock`,
 * and `serial_mock`:
 *
 *   1. Test code QUEUES a scripted reply (and optionally an expected
 *      input) BEFORE the provider exercises the mock.
 *   2. Provider exercises the mock — every read drains one queue entry,
 *      every write either matches the expected input or is recorded
 *      verbatim for later assertion.
 *   3. Test code calls `assert_consumed()` to verify the queue is fully
 *      drained — a non-empty queue at end-of-test means the provider
 *      ran less code than the test expected, which is a forensic gap.
 *
 * Pure C++20, no Qt, no external dependencies. Header-only.
 */
#ifndef UFT_TESTS_MOCK_HARDWARE_BYTE_BUFFER_H
#define UFT_TESTS_MOCK_HARDWARE_BYTE_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::tests::mocks {

/* ════════════════════════════════════════════════════════════════════════
 *  ByteVec — alias for the scripted/recorded byte sequences.
 * ════════════════════════════════════════════════════════════════════════ */
using ByteVec = std::vector<std::uint8_t>;

/**
 * @brief Scripted reply queue.
 *
 * A FIFO of byte sequences. The mock pops entries when its read API is
 * called. If the test reads more than was queued, `pop_front()` throws
 * `std::out_of_range` — that is a genuine forensic-gap signal: the
 * provider asked the transport for more data than the test scripted,
 * which means the test does not actually cover the call site.
 */
class ScriptedReplies {
public:
    void push(ByteVec bytes) { m_queue.push_back(std::move(bytes)); }
    void push(std::initializer_list<std::uint8_t> bytes) {
        m_queue.emplace_back(bytes.begin(), bytes.end());
    }

    ByteVec pop() {
        if (m_queue.empty()) {
            throw std::out_of_range(
                "ScriptedReplies::pop(): provider asked for more data than the "
                "test queued. Either queue another reply via queue_response()/"
                "queue_read_line()/queue_subprocess_run(), or stop the provider "
                "earlier in the test.");
        }
        ByteVec out = std::move(m_queue.front());
        m_queue.pop_front();
        return out;
    }

    bool empty() const noexcept { return m_queue.empty(); }
    std::size_t size() const noexcept { return m_queue.size(); }
    void clear() noexcept { m_queue.clear(); }

    /** Throws if not empty — call this at end-of-test. */
    void assert_consumed(const char *mock_label) const {
        if (!m_queue.empty()) {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                "%s: %zu scripted reply(s) left UNCONSUMED at end of test. "
                "Either the provider exercised fewer paths than scripted, or "
                "the test queued too many replies.",
                mock_label, m_queue.size());
            throw std::logic_error(msg);
        }
    }

private:
    std::deque<ByteVec> m_queue;
};

/**
 * @brief Recorded send-log.
 *
 * The mock appends to this every time the provider sends bytes. Tests
 * inspect after the fact (e.g. "did the provider send the
 * GW-init-handshake?"). Independent from the reply queue — replies are
 * scripted per call, sends are post-recorded en bloc.
 */
class RecordedSends {
public:
    void record(const std::uint8_t *data, std::size_t len) {
        m_log.emplace_back(data, data + len);
    }
    void record(const ByteVec &v) { m_log.push_back(v); }

    bool empty() const noexcept { return m_log.empty(); }
    std::size_t count() const noexcept { return m_log.size(); }
    const ByteVec &at(std::size_t i) const { return m_log.at(i); }
    const std::deque<ByteVec> &all() const noexcept { return m_log; }
    void clear() noexcept { m_log.clear(); }

    /** Concatenated bytes across all recorded sends — handy for
     *  protocol-stream comparisons where boundaries between writes
     *  are immaterial. */
    ByteVec concatenated() const {
        ByteVec out;
        std::size_t total = 0;
        for (const auto &v : m_log) total += v.size();
        out.reserve(total);
        for (const auto &v : m_log)
            out.insert(out.end(), v.begin(), v.end());
        return out;
    }

private:
    std::deque<ByteVec> m_log;
};

}  // namespace uft::tests::mocks

#endif  // UFT_TESTS_MOCK_HARDWARE_BYTE_BUFFER_H
