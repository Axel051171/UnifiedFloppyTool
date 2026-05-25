/**
 * @file tests/test_xum1541_usb_mock.c
 * @brief Tier-2.5 USB-mock integration test for XUM1541 (MF-270).
 *
 * Compiles src/hal/uft_xum1541.c against the libusb-mock shim and
 * scripts the open-cycle exchange. The XUM1541 protocol adds
 * libusb_control_transfer (INIT + SHUTDOWN) on top of bulk-transfer.
 */

#include "usb_mock/libusb_mock.h"
#include "uft/hal/uft_xum1541.h"
#include "uft/uft_error.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ─────────────────────────────────────────────────────────────────────
 *  Test 1 — open succeeds when device present.
 * ───────────────────────────────────────────────────────────────────── */
TEST(open_happy_path) {
    usb_mock_reset();
    /* VID/PID confirmed in include/uft/hal/uft_xum1541.h: 0x16D0:0x0504. */
    usb_mock_queue_open(0x16D0, 0x0504, /*open_succeeds=*/1);
    /* INIT control transfer returns 8 bytes of device info. */
    uint8_t device_info[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    usb_mock_queue_control_in(0xC0, /*CTRL_INIT*/1, device_info, 8);

    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(cfg != NULL);

    uft_error_t err = uft_xum_open(cfg, 0);
    ASSERT(err == UFT_OK);
    ASSERT(uft_xum_is_connected(cfg) == true);

    /* close: SHUTDOWN control transfer + release + close + exit. */
    uint8_t shutdown_reply = 0;
    usb_mock_queue_control_in(0xC0, /*CTRL_SHUTDOWN*/3, &shutdown_reply, 1);
    usb_mock_queue_close();

    uft_xum_close(cfg);
    ASSERT(uft_xum_is_connected(cfg) == false);
    ASSERT(usb_mock_remaining_exchanges() == 0);
    uft_xum_config_destroy(cfg);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 2 — open fails when device not present.
 * ───────────────────────────────────────────────────────────────────── */
TEST(open_fails_no_device) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0504, /*open_succeeds=*/0);

    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(cfg != NULL);

    uft_error_t err = uft_xum_open(cfg, 0);
    ASSERT(err != UFT_OK);
    ASSERT(uft_xum_is_connected(cfg) == false);
    /* Error message should be present (3-part F-4 contract). */
    const char *errmsg = uft_xum_get_error(cfg);
    ASSERT(errmsg && strlen(errmsg) > 0);
    ASSERT(usb_mock_remaining_exchanges() == 0);
    uft_xum_config_destroy(cfg);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Test 3 — open is idempotent (second call returns OK without I/O).
 * ───────────────────────────────────────────────────────────────────── */
TEST(open_idempotent) {
    usb_mock_reset();
    usb_mock_queue_open(0x16D0, 0x0504, 1);
    uint8_t device_info[8] = {0};
    usb_mock_queue_control_in(0xC0, 1, device_info, 8);

    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(cfg != NULL);
    ASSERT(uft_xum_open(cfg, 0) == UFT_OK);

    /* Second open() must NOT touch USB — no further exchanges queued.
     * The contract says "idempotent" (see uft_xum_open header). */
    ASSERT(uft_xum_open(cfg, 0) == UFT_OK);

    uint8_t s = 0;
    usb_mock_queue_control_in(0xC0, 3, &s, 1);
    usb_mock_queue_close();
    uft_xum_close(cfg);
    ASSERT(usb_mock_remaining_exchanges() == 0);
    uft_xum_config_destroy(cfg);
}

int main(void)
{
    printf("=== XUM1541 USB-Mock Integration Tests (MF-270) ===\n");
    RUN(open_happy_path);
    RUN(open_fails_no_device);
    RUN(open_idempotent);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    printf("Total USB transfers: %zu\n", usb_mock_total_transfers());
    return _fail ? 1 : 0;
}
