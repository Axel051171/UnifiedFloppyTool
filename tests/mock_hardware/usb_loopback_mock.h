/**
 * @file usb_loopback_mock.h
 * @brief libusb-style transport mock — scripted bulk + control transfers.
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.6
 *
 * Used by V2 providers whose backend layer talks to a libusb-style USB
 * device (SCPProviderV2, FC5025ProviderV2, XUM1541ProviderV2,
 * USBFloppyProviderV2 — all P1.8+ tasks).
 *
 * The mock is the BACKEND that the V2 provider's `do_*()` methods call
 * INSTEAD of a real `libusb_device_handle*`. P1.8+ provider migrators
 * decide how their concrete V2 wraps this mock — typically by
 * templating the provider on a Transport type, or by passing a pointer
 * to an `IUsbTransport` interface that both libusb and this mock
 * satisfy.
 *
 * What this mock does NOT do:
 *   - It does NOT enumerate devices. Tests construct it directly.
 *   - It does NOT block. Every transfer returns synchronously.
 *   - It does NOT touch real USB. Calling `bulk_in()` on a fresh mock
 *     with an empty queue throws — a forensic-gap signal, not a hang.
 */
#ifndef UFT_TESTS_MOCK_HARDWARE_USB_LOOPBACK_MOCK_H
#define UFT_TESTS_MOCK_HARDWARE_USB_LOOPBACK_MOCK_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

#include "byte_buffer.h"

namespace uft::tests::mocks {

/**
 * @brief libusb-style transport mock.
 *
 * Surface (matches typical libusb wrapper layer):
 *   - bulk_out(endpoint, data, len)         — sends bytes, recorded
 *   - bulk_in(endpoint, out, max_len)       — reads next scripted reply
 *   - control_transfer(req_type, req, val, idx, data, len) — same model
 *
 * Endpoint argument is captured but not strictly enforced — tests can
 * ignore it or assert against the recorded value.
 */
class UsbLoopbackMock {
public:
    /* ── Test scenario API ────────────────────────────────────────────── */

    /** Queue a reply that the next bulk_in() will return. */
    void queue_bulk_reply(ByteVec reply) {
        m_bulk_replies.push(std::move(reply));
    }
    void queue_bulk_reply(std::initializer_list<std::uint8_t> reply) {
        m_bulk_replies.push(reply);
    }

    /** Queue a reply for the next control_transfer() (IN direction). */
    void queue_control_reply(ByteVec reply) {
        m_control_replies.push(std::move(reply));
    }

    /** Force the NEXT bulk_in/bulk_out/control_transfer call to fail
     *  with the given negative error code (libusb-style). One-shot. */
    void inject_error(int rc) { m_pending_error = rc; }

    /** Inspect what the provider sent (in call order). */
    const RecordedSends &bulk_out_log() const noexcept { return m_bulk_out_log; }
    const RecordedSends &control_out_log() const noexcept { return m_control_out_log; }
    int last_bulk_endpoint() const noexcept { return m_last_bulk_endpoint; }

    /** Verify queues are drained at end-of-test. */
    void assert_consumed() const {
        m_bulk_replies.assert_consumed("UsbLoopbackMock::bulk_replies");
        m_control_replies.assert_consumed("UsbLoopbackMock::control_replies");
    }

    /* ── Backend API (called by V2 provider's do_* methods) ───────────── */

    /**
     * @return number of bytes "transferred" (= len), or negative on
     *         injected error. Zero is impossible — the provider should
     *         not call with len==0; if it does, that's a real bug.
     */
    int bulk_out(int endpoint, const std::uint8_t *data, std::size_t len) {
        if (m_pending_error) {
            int rc = m_pending_error;
            m_pending_error = 0;
            return rc;
        }
        if (data == nullptr) {
            throw std::invalid_argument(
                "UsbLoopbackMock::bulk_out: null data pointer");
        }
        m_last_bulk_endpoint = endpoint;
        m_bulk_out_log.record(data, len);
        return static_cast<int>(len);
    }

    /**
     * @return number of bytes returned (≤ max_len), or negative on
     *         injected error. The reply is the next entry in
     *         the bulk-replies queue. Throws if the queue is empty.
     */
    int bulk_in(int endpoint, std::uint8_t *out, std::size_t max_len) {
        if (m_pending_error) {
            int rc = m_pending_error;
            m_pending_error = 0;
            return rc;
        }
        if (out == nullptr) {
            throw std::invalid_argument(
                "UsbLoopbackMock::bulk_in: null out pointer");
        }
        m_last_bulk_endpoint = endpoint;
        ByteVec reply = m_bulk_replies.pop();   /* may throw out_of_range */
        std::size_t n = reply.size() < max_len ? reply.size() : max_len;
        if (n > 0) std::memcpy(out, reply.data(), n);
        return static_cast<int>(n);
    }

    /**
     * libusb_control_transfer-shaped API. The OUT path records the
     * payload; the IN path drains the control-replies queue.
     *
     * @param request_type bm_request_type byte (direction encoded in bit 7).
     * @param data         when bit 7 of request_type is 0 (host→device),
     *                     this is INPUT bytes recorded by the mock.
     *                     When bit 7 is 1 (device→host), this is the
     *                     OUTPUT buffer the mock fills from the queue.
     * @param length       buffer length on either path.
     * @return bytes transferred or negative error.
     */
    int control_transfer(std::uint8_t request_type,
                         std::uint8_t request,
                         std::uint16_t value,
                         std::uint16_t index,
                         std::uint8_t *data,
                         std::uint16_t length) {
        if (m_pending_error) {
            int rc = m_pending_error;
            m_pending_error = 0;
            return rc;
        }
        m_last_control = ControlSnapshot{request_type, request, value, index};
        const bool dir_in = (request_type & 0x80u) != 0u;

        if (!dir_in) {
            /* host → device: record the payload */
            if (length > 0 && data != nullptr) {
                m_control_out_log.record(data, length);
            }
            return length;
        } else {
            /* device → host: fill from queue */
            if (length == 0 || data == nullptr) {
                return 0;
            }
            ByteVec reply = m_control_replies.pop();
            std::size_t n = reply.size() < length ? reply.size() : length;
            if (n > 0) std::memcpy(data, reply.data(), n);
            return static_cast<int>(n);
        }
    }

    /* ── Snapshot of last control-transfer header — for assertions ───── */
    struct ControlSnapshot {
        std::uint8_t  request_type = 0;
        std::uint8_t  request = 0;
        std::uint16_t value = 0;
        std::uint16_t index = 0;
    };
    const ControlSnapshot &last_control() const noexcept { return m_last_control; }

private:
    ScriptedReplies m_bulk_replies;
    ScriptedReplies m_control_replies;
    RecordedSends m_bulk_out_log;
    RecordedSends m_control_out_log;
    int m_last_bulk_endpoint = -1;
    ControlSnapshot m_last_control;
    int m_pending_error = 0;
};

}  // namespace uft::tests::mocks

#endif  // UFT_TESTS_MOCK_HARDWARE_USB_LOOPBACK_MOCK_H
