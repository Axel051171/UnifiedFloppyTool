/**
 * @file tests/test_scp_direct_usb_mock.c
 * @brief Tier-2.5 USB-mock integration test for SCP-Direct (MF-270).
 *
 * Compiles src/hal/uft_scp_direct.c against the libusb-mock shim
 * (tests/usb_mock/libusb_mock.h) and scripts a full open → seek →
 * close exchange. Verifies:
 *   - The bytes the C-HAL puts on the wire match the SCP-SDK packet
 *     format exactly: [CMD, LEN, params..., CHECKSUM = sum + 0x4A].
 *   - The C-HAL accepts the scripted [CMD_ECHO, PR_OK] response and
 *     reports UFT_OK to its caller.
 *   - VID/PID 0x16D0:0x0F8C is used (MF-212 / MF-261 verified value).
 *
 * NO HARDWARE NEEDED. The whole exchange happens in-process; the
 * mock asserts every queued exchange is consumed exactly once.
 */

#include "usb_mock/libusb_mock.h"
#include "uft/hal/uft_scp_direct.h"
#include "uft/uft_error.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* SCP packet format helper:
 *   [CMD, LEN, params..., CHECKSUM]
 *   CHECKSUM = (CMD + LEN + sum(params) + 0x4A) & 0xff
 */
static size_t build_scp_packet(uint8_t *out, uint8_t cmd,
                                const uint8_t *params, size_t param_len)
{
    out[0] = cmd;
    out[1] = (uint8_t)param_len;
    if (param_len && params) {
        memcpy(out + 2, params, param_len);
    }
    uint8_t cs = (uint8_t)(0x4A + cmd + (uint8_t)param_len);
    for (size_t i = 0; i < param_len; i++) {
        cs = (uint8_t)(cs + params[i]);
    }
    out[2 + param_len] = cs;
    return 3 + param_len;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 1 — happy-path open + close, no commands sent.
 * ───────────────────────────────────────────────────────────────────── */
TEST(open_close_happy_path) {
    usb_mock_reset();
    /* Production code calls libusb_open_device_with_vid_pid(VID, PID).
     * MF-212 / MF-261 confirmed VID=0x16D0, PID=0x0F8C. */
    usb_mock_queue_open(0x16D0, 0x0F8C, /*open_succeeds=*/1);
    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    uft_error_t err = uft_scp_direct_open(&ctx);
    ASSERT(err == UFT_OK);
    ASSERT(ctx != NULL);

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 2 — open fails when device not present.
 * ───────────────────────────────────────────────────────────────────── */
TEST(open_fails_no_device) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, /*open_succeeds=*/0);

    uft_scp_direct_ctx_t *ctx = NULL;
    uft_error_t err = uft_scp_direct_open(&ctx);
    ASSERT(err != UFT_OK);
    ASSERT(ctx == NULL);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 3 — seek sends correct CMD_STEPTO packet + accepts ack.
 * ───────────────────────────────────────────────────────────────────── */
TEST(seek_emits_stepto_packet) {
    usb_mock_reset();

    /* open() */
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);

    /* seek(track=42) → sends CMD_STEPTO (0x89) + 1-byte param (42).
     * Packet = [0x89, 0x01, 0x2A, CHECKSUM]
     * CHECKSUM = (0x4A + 0x89 + 0x01 + 0x2A) & 0xff = 0xE4 */
    uint8_t expected_tx[4];
    uint8_t param = 42;
    size_t  tx_len = build_scp_packet(expected_tx, 0x89, &param, 1);
    /* sanity: that's a 4-byte packet (cmd+len+1param+checksum). */
    ASSERT(tx_len == 4);
    usb_mock_queue_bulk_out(0x01 /*OUT*/, expected_tx, tx_len, 0 /*ACK*/);

    /* Device echoes [CMD_ECHO=0x89, PR_OK=0x4F]. */
    uint8_t response[2] = { 0x89, 0x4F };
    usb_mock_queue_bulk_in(0x81 /*IN*/, response, 2, 0);

    /* close() */
    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uft_error_t err = uft_scp_direct_seek(ctx, 42);
    ASSERT(err == UFT_OK);

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 4 — seek fails when device reports non-OK status.
 *
 *  The SCP responds with [CMD_ECHO=0x89, BAD_STATUS=0xFF]. The C-HAL
 *  must NOT silently treat that as success — Prinzip 1.
 * ───────────────────────────────────────────────────────────────────── */
TEST(seek_fails_when_device_reports_bad_status) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);

    uint8_t expected_tx[4];
    uint8_t param = 7;
    build_scp_packet(expected_tx, 0x89, &param, 1);
    usb_mock_queue_bulk_out(0x01, expected_tx, 4, 0);

    /* Device echoes CMD correctly but reports BAD status byte. */
    uint8_t response[2] = { 0x89, 0xFF };
    usb_mock_queue_bulk_in(0x81, response, 2, 0);

    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uft_error_t err = uft_scp_direct_seek(ctx, 7);
    ASSERT(err != UFT_OK);   /* honest failure, not silent success */

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 5 — bulk-out timeout surfaces as IO error.
 * ───────────────────────────────────────────────────────────────────── */
TEST(seek_fails_on_bulk_out_timeout) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);

    uint8_t expected_tx[4];
    uint8_t param = 3;
    build_scp_packet(expected_tx, 0x89, &param, 1);
    /* Script the bulk-out to FAIL with LIBUSB_ERROR_TIMEOUT (-7). */
    usb_mock_queue_bulk_out(0x01, expected_tx, 4, -7 /*TIMEOUT*/);

    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uft_error_t err = uft_scp_direct_seek(ctx, 3);
    ASSERT(err != UFT_OK);   /* timeout → I/O error, not silent OK */

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

int main(void)
{
    printf("=== SCP-Direct USB-Mock Integration Tests (MF-270) ===\n");
    RUN(open_close_happy_path);
    RUN(open_fails_no_device);
    RUN(seek_emits_stepto_packet);
    RUN(seek_fails_when_device_reports_bad_status);
    RUN(seek_fails_on_bulk_out_timeout);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    printf("Total bulk transfers: %zu\n", usb_mock_total_transfers());
    return _fail ? 1 : 0;
}
