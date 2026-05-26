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

/* ─────────────────────────────────────────────────────────────────────
 *  Test 6 — read_flux 2-revolution happy path: full samdisk wire dance.
 *
 *  Steps (5 USB exchanges after open):
 *    1. CMD_SIDE      [0x8D,1, side=0,        cksum]  + [0x8D, PR_OK]
 *    2. CMD_READ_FLUX [0xA0,2, revs=2,ff=0x01,cksum]  + [0xA0, PR_OK]
 *    3. CMD_GET_FLUX_INFO [0xA1,0, cksum]    + [0xA1, PR_OK]
 *       + 40-byte rev_index (rev 0 count=4, rev 1 count=2, rest 0)
 *    4. CMD_SENDRAM_USB rev 0 [offset=0,len=8] + 8 bytes flux data
 *    5. CMD_SENDRAM_USB rev 1 [offset=8,len=4] + 4 bytes flux data
 *
 *  Expected: 6 transitions, each = sample_BE * 25 ns.
 *    rev 0 samples (16-bit BE): 0x0010, 0x0020, 0x0030, 0x0040
 *      → ns: 16*25=400, 32*25=800, 48*25=1200, 64*25=1600
 *    rev 1 samples (16-bit BE): 0x0050, 0x0060
 *      → ns: 80*25=2000, 96*25=2400
 * ───────────────────────────────────────────────────────────────────── */
TEST(read_flux_two_revs_happy_path) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);

    uint8_t pkt[16];
    size_t  n;

    /* (1) CMD_SIDE param=0 */
    uint8_t side_param = 0;
    n = build_scp_packet(pkt, 0x8D, &side_param, 1);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t side_ack[2] = { 0x8D, 0x4F };
    usb_mock_queue_bulk_in(0x81, side_ack, 2, 0);

    /* (2) CMD_READ_FLUX [revs=2, ff_Index=0x01] */
    uint8_t rf_params[2] = { 2, 0x01 };
    n = build_scp_packet(pkt, 0xA0, rf_params, 2);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t rf_ack[2] = { 0xA0, 0x4F };
    usb_mock_queue_bulk_in(0x81, rf_ack, 2, 0);

    /* (3) CMD_GET_FLUX_INFO (no params) + 40-byte rev_index payload. */
    n = build_scp_packet(pkt, 0xA1, NULL, 0);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t gfi_ack[2] = { 0xA1, 0x4F };
    usb_mock_queue_bulk_in(0x81, gfi_ack, 2, 0);

    /* rev_index: 5 pairs of [index_time_be32, flux_count_be32].
     * We zero everything then set count[0]=4, count[1]=2. */
    uint8_t rev_index[40] = {0};
    /* rev 0 count at offset 4 (4 bytes BE): 0x00,0x00,0x00,0x04 */
    rev_index[4*1 + 3] = 0x04;
    /* rev 1 count at offset 12 (4 bytes BE): 0x00,0x00,0x00,0x02 */
    rev_index[8 + 4 + 3] = 0x02;
    usb_mock_queue_bulk_in(0x81, rev_index, sizeof(rev_index), 0);

    /* (4) CMD_SENDRAM_USB rev 0: [offset=0, length=8] + 8 bytes flux. */
    uint8_t sendram0[8] = {
        0x00,0x00,0x00,0x00,   /* offset = 0 (BE) */
        0x00,0x00,0x00,0x08    /* length = 8 (BE) */
    };
    n = build_scp_packet(pkt, 0xA9, sendram0, 8);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t s0_ack[2] = { 0xA9, 0x4F };
    usb_mock_queue_bulk_in(0x81, s0_ack, 2, 0);
    /* Flux samples for rev 0 (16-bit BE): 0x0010, 0x0020, 0x0030, 0x0040 */
    uint8_t flux0[8] = { 0x00,0x10, 0x00,0x20, 0x00,0x30, 0x00,0x40 };
    usb_mock_queue_bulk_in(0x81, flux0, sizeof(flux0), 0);

    /* (5) CMD_SENDRAM_USB rev 1: [offset=8, length=4] + 4 bytes flux. */
    uint8_t sendram1[8] = {
        0x00,0x00,0x00,0x08,   /* offset = 8 */
        0x00,0x00,0x00,0x04    /* length = 4 */
    };
    n = build_scp_packet(pkt, 0xA9, sendram1, 8);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t s1_ack[2] = { 0xA9, 0x4F };
    usb_mock_queue_bulk_in(0x81, s1_ack, 2, 0);
    /* Flux samples for rev 1 (16-bit BE): 0x0050, 0x0060 */
    uint8_t flux1[4] = { 0x00,0x50, 0x00,0x60 };
    usb_mock_queue_bulk_in(0x81, flux1, sizeof(flux1), 0);

    usb_mock_queue_close();

    /* Drive the C-HAL */
    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uint32_t out_flux[16] = {0};
    size_t   out_count = 999;
    uft_error_t err = uft_scp_direct_read_flux(
        ctx, /*side=*/0, /*revs=*/2,
        out_flux, /*capacity=*/16, &out_count);

    ASSERT(err == UFT_OK);
    ASSERT(out_count == 6);

    /* Decoded ns values: sample * 25 */
    ASSERT(out_flux[0] == 0x10 * 25);   /* 400 */
    ASSERT(out_flux[1] == 0x20 * 25);   /* 800 */
    ASSERT(out_flux[2] == 0x30 * 25);   /* 1200 */
    ASSERT(out_flux[3] == 0x40 * 25);   /* 1600 */
    ASSERT(out_flux[4] == 0x50 * 25);   /* 2000 */
    ASSERT(out_flux[5] == 0x60 * 25);   /* 2400 */

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 7 — read_flux handles 0x0000 overflow markers correctly.
 *
 *  Per samdisk SuperCardPro.cpp ReadFlux():
 *    0x0000 marker → accumulator += 0x10000 (cell overflow)
 *    non-zero      → emit (accumulator + sample) * 25 ns; reset accum
 *
 *  Sample stream (rev 0): 0x0000, 0x0010, 0x0000, 0x0000, 0x0020
 *  Expected transitions: 2 (after each non-zero)
 *    1st emit: (0x10000 + 0x0010) * 25 = 65552 * 25 = 1638800 ns
 *    2nd emit: (0x10000 + 0x10000 + 0x0020) * 25 = 131104 * 25 = 3277600 ns
 *  Trailing accumulator (no non-zero terminator) is dropped — never
 *  fabricated as a final sample. Forensic.
 * ───────────────────────────────────────────────────────────────────── */
TEST(read_flux_handles_overflow_accumulation) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);

    uint8_t pkt[16];
    size_t  n;

    /* SIDE 0 */
    uint8_t side_param = 0;
    n = build_scp_packet(pkt, 0x8D, &side_param, 1);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t side_ack[2] = { 0x8D, 0x4F };
    usb_mock_queue_bulk_in(0x81, side_ack, 2, 0);

    /* READ_FLUX [revs=2, ff_Index=0x01] — min revs=2 */
    uint8_t rf_params[2] = { 2, 0x01 };
    n = build_scp_packet(pkt, 0xA0, rf_params, 2);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t rf_ack[2] = { 0xA0, 0x4F };
    usb_mock_queue_bulk_in(0x81, rf_ack, 2, 0);

    /* GET_FLUX_INFO: rev 0 has 5 samples, rev 1 has 0. */
    n = build_scp_packet(pkt, 0xA1, NULL, 0);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t gfi_ack[2] = { 0xA1, 0x4F };
    usb_mock_queue_bulk_in(0x81, gfi_ack, 2, 0);

    uint8_t rev_index[40] = {0};
    rev_index[4 + 3] = 0x05;    /* rev 0 count = 5 */
    /* rev 1 count = 0 → loop skips, no SENDRAM_USB for rev 1 */
    usb_mock_queue_bulk_in(0x81, rev_index, sizeof(rev_index), 0);

    /* SENDRAM_USB rev 0: offset=0 length=10 (5 samples × 2 bytes) */
    uint8_t sendram0[8] = {
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x0A
    };
    n = build_scp_packet(pkt, 0xA9, sendram0, 8);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t s0_ack[2] = { 0xA9, 0x4F };
    usb_mock_queue_bulk_in(0x81, s0_ack, 2, 0);
    /* Flux samples (BE16): 0000, 0010, 0000, 0000, 0020 */
    uint8_t flux0[10] = {
        0x00,0x00, 0x00,0x10, 0x00,0x00, 0x00,0x00, 0x00,0x20
    };
    usb_mock_queue_bulk_in(0x81, flux0, sizeof(flux0), 0);

    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uint32_t out_flux[16] = {0};
    size_t   out_count = 999;
    uft_error_t err = uft_scp_direct_read_flux(
        ctx, 0, 2, out_flux, 16, &out_count);

    ASSERT(err == UFT_OK);
    ASSERT(out_count == 2);
    ASSERT(out_flux[0] == (0x10000u + 0x0010u) * 25u);    /* 1638800 */
    ASSERT(out_flux[1] == (0x20000u + 0x0020u) * 25u);    /* 3277600 */

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 8 — single revolution requested → INVALID_ARG (samdisk-clamp
 *  semantics rejected at HAL boundary; honest, no silent coercion).
 * ───────────────────────────────────────────────────────────────────── */
TEST(read_flux_rejects_single_revolution) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);
    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uint32_t out_flux[8] = {0};
    size_t   out_count = 999;
    /* revs=1 < UFT_SCP_MIN_REVOLUTIONS (2). */
    uft_error_t err = uft_scp_direct_read_flux(
        ctx, 0, 1, out_flux, 8, &out_count);
    ASSERT(err == UFT_ERR_INVALID_ARG);
    /* Critical: out_count must NOT have been modified to a plausible
     * number that a caller might mistake for a real sample count. */
    ASSERT(out_count == 999);

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 9 — accumulator is reset PER REVOLUTION (regression-guard).
 *
 *  Rev 0 ends with an unterminated 0x0000 (overflow marker without a
 *  subsequent non-zero). Rev 1 starts with 0x0010.
 *
 *  Correct (samdisk-equivalent) behaviour:
 *    - Rev 0: 0x0010 emits, then 0x0000 accumulates 0x10000, end-of-rev
 *      drops the accumulator. → 1 emit: 0x10 * 25 = 400 ns.
 *    - Rev 1 starts fresh accum=0: 0x0010 emits 0x10 * 25 = 400 ns.
 *    Total: 2 emits, both = 400 ns.
 *
 *  Buggy "persist across boundary" behaviour would emit:
 *    - Rev 0: 0x10*25=400
 *    - Rev 1: (0x10000+0x10)*25=1638800  ← WRONG
 *  This test catches that regression.
 * ───────────────────────────────────────────────────────────────────── */
TEST(read_flux_resets_accumulator_per_revolution) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0F8C, 1);

    uint8_t pkt[16];
    size_t  n;

    /* SIDE 0 */
    uint8_t side_param = 0;
    n = build_scp_packet(pkt, 0x8D, &side_param, 1);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t side_ack[2] = { 0x8D, 0x4F };
    usb_mock_queue_bulk_in(0x81, side_ack, 2, 0);

    /* READ_FLUX [revs=2, ff_Index] */
    uint8_t rf_params[2] = { 2, 0x01 };
    n = build_scp_packet(pkt, 0xA0, rf_params, 2);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t rf_ack[2] = { 0xA0, 0x4F };
    usb_mock_queue_bulk_in(0x81, rf_ack, 2, 0);

    /* GET_FLUX_INFO: rev 0 count=2, rev 1 count=1. */
    n = build_scp_packet(pkt, 0xA1, NULL, 0);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t gfi_ack[2] = { 0xA1, 0x4F };
    usb_mock_queue_bulk_in(0x81, gfi_ack, 2, 0);

    uint8_t rev_index[40] = {0};
    rev_index[4 + 3]      = 0x02;    /* rev 0 count = 2 */
    rev_index[8 + 4 + 3]  = 0x01;    /* rev 1 count = 1 */
    usb_mock_queue_bulk_in(0x81, rev_index, sizeof(rev_index), 0);

    /* SENDRAM_USB rev 0: 4 bytes (2 samples) */
    uint8_t sendram0[8] = { 0,0,0,0,  0,0,0,4 };
    n = build_scp_packet(pkt, 0xA9, sendram0, 8);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t s0_ack[2] = { 0xA9, 0x4F };
    usb_mock_queue_bulk_in(0x81, s0_ack, 2, 0);
    /* Rev 0 samples: 0x0010, 0x0000   ← trailing overflow, no terminator */
    uint8_t flux0[4] = { 0x00,0x10, 0x00,0x00 };
    usb_mock_queue_bulk_in(0x81, flux0, sizeof(flux0), 0);

    /* SENDRAM_USB rev 1: 2 bytes (1 sample), offset=4 */
    uint8_t sendram1[8] = { 0,0,0,4,  0,0,0,2 };
    n = build_scp_packet(pkt, 0xA9, sendram1, 8);
    usb_mock_queue_bulk_out(0x01, pkt, n, 0);
    uint8_t s1_ack[2] = { 0xA9, 0x4F };
    usb_mock_queue_bulk_in(0x81, s1_ack, 2, 0);
    /* Rev 1 sample: 0x0010 (a CLEAN 0x10 — NOT (0x10000+0x10) which is
     * what a buggy persist-across-boundary would produce). */
    uint8_t flux1[2] = { 0x00,0x10 };
    usb_mock_queue_bulk_in(0x81, flux1, sizeof(flux1), 0);

    usb_mock_queue_close();

    uft_scp_direct_ctx_t *ctx = NULL;
    ASSERT(uft_scp_direct_open(&ctx) == UFT_OK);

    uint32_t out_flux[8] = {0};
    size_t   out_count = 999;
    uft_error_t err = uft_scp_direct_read_flux(
        ctx, 0, 2, out_flux, 8, &out_count);
    ASSERT(err == UFT_OK);
    ASSERT(out_count == 2);
    ASSERT(out_flux[0] == 0x10u * 25u);   /* Rev 0: 400 ns */
    ASSERT(out_flux[1] == 0x10u * 25u);   /* Rev 1: 400 ns — NOT 1638800 */

    uft_scp_direct_close(ctx);
    ASSERT(usb_mock_remaining_exchanges() == 0);
}

int main(void)
{
    printf("=== SCP-Direct USB-Mock Integration Tests (MF-270 + MF-276) ===\n");
    RUN(open_close_happy_path);
    RUN(open_fails_no_device);
    RUN(seek_emits_stepto_packet);
    RUN(seek_fails_when_device_reports_bad_status);
    RUN(seek_fails_on_bulk_out_timeout);
    RUN(read_flux_two_revs_happy_path);
    RUN(read_flux_handles_overflow_accumulation);
    RUN(read_flux_rejects_single_revolution);
    RUN(read_flux_resets_accumulator_per_revolution);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    printf("Total bulk transfers: %zu\n", usb_mock_total_transfers());
    return _fail ? 1 : 0;
}
