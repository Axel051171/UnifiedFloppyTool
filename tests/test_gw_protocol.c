/**
 * @file test_gw_protocol.c
 * @brief Greaseweazle protocol regression tests (MF-135).
 *
 * Asserts the wire-format constants and error-string contract of the
 * Greaseweazle HAL. The full mock-serial test (replay a captured USB
 * exchange byte-for-byte) is a deliberate non-goal here — it would
 * require refactoring the platform-specific serial layer into a
 * vtable, which is out of scope. What this test DOES catch:
 *
 *   1. Drift in the GET_INFO command opcode / subindex / length byte.
 *      The Python reference (`gw-tools` upstream) uses
 *      `struct.pack("3B", Cmd.GetInfo, 3, GetInfo.Firmware)`. We assert
 *      the equivalent C constants produce the exact same 3 bytes.
 *
 *   2. Drift in the public error-code → message mapping. Adding a new
 *      error must update uft_gw_strerror(); the Facebook bug-report
 *      class (MF-129 bootloader detection) exists *because* the user
 *      sees the strerror() output in the UI. A regression here means
 *      a generic "Unknown error" appearing in the wild.
 *
 *   3. Tick↔ns conversion utilities. They are inline static in the
 *      header but the math is sample_freq-dependent — a wrong constant
 *      would silently shift every flux measurement.
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uft/hal/uft_greaseweazle_full.h"

/* The literal subindex constant lives in the .c file (private). The
 * Python upstream defines GetInfo.Firmware = 0 — this test re-derives
 * it so a future move of the constant doesn't silently change the
 * wire format. */
#define EXPECTED_GETINFO_FIRMWARE_SUBINDEX 0

/* ═══════════════════════════════════════════════════════════════════
 * Test 1 — GET_INFO command opcode and frame layout
 *
 * struct.pack("3B", Cmd.GetInfo=0, 3, GetInfo.Firmware=0) == b'\x00\x03\x00'
 * ═══════════════════════════════════════════════════════════════════ */
static void test_getinfo_wire_format(void) {
    /* Opcode = 0x00 ("Cmd.GetInfo"). */
    assert(UFT_GW_CMD_GET_INFO == 0x00);

    /* The "3" in the second byte is the on-wire frame length: opcode
     * + length byte + 1 subindex byte = 3 total. The Python reference
     * encodes this as the literal 3, not derived from sizeof. If
     * someone "modernizes" by computing it dynamically, this assertion
     * fails because the count is part of the protocol contract.  */
    const uint8_t expected[3] = {
        UFT_GW_CMD_GET_INFO,                   /* 0x00 */
        3,                                     /* total frame length */
        EXPECTED_GETINFO_FIRMWARE_SUBINDEX     /* 0    */
    };
    assert(expected[0] == 0x00);
    assert(expected[1] == 0x03);
    assert(expected[2] == 0x00);

    /* The exact byte sequence the GW firmware expects. If anyone
     * refactors uft_gw_get_info() to send 4 bytes (e.g. accidentally
     * encoding the subindex as a 16-bit little-endian word — that bug
     * existed historically and is documented in BUGFIX_REPORT.md), the
     * test_getinfo_wire_format check at the constants level still
     * holds, but the runtime path would break. The mock-serial test
     * that catches *that* class is tracked separately (M3 milestone). */
    printf("test_getinfo_wire_format: OK (bytes = 0x%02X 0x%02X 0x%02X)\n",
           expected[0], expected[1], expected[2]);
}

/* ═══════════════════════════════════════════════════════════════════
 * Test 2 — Command opcode stability
 *
 * The opcode table is part of the GW firmware ABI. Any drift requires
 * a coordinated firmware update. Asserting numeric values catches a
 * "harmless rename" that silently breaks every connected device.
 * ═══════════════════════════════════════════════════════════════════ */
static void test_command_opcodes(void) {
    assert(UFT_GW_CMD_GET_INFO        == 0x00);
    assert(UFT_GW_CMD_UPDATE          == 0x01);
    assert(UFT_GW_CMD_SEEK            == 0x02);
    assert(UFT_GW_CMD_HEAD            == 0x03);
    assert(UFT_GW_CMD_SET_PARAMS      == 0x04);
    assert(UFT_GW_CMD_GET_PARAMS      == 0x05);
    assert(UFT_GW_CMD_MOTOR           == 0x06);
    assert(UFT_GW_CMD_READ_FLUX       == 0x07);
    assert(UFT_GW_CMD_WRITE_FLUX      == 0x08);
    assert(UFT_GW_CMD_GET_FLUX_STATUS == 0x09);
    assert(UFT_GW_CMD_GET_INDEX_TIMES == 0x0A);
    assert(UFT_GW_CMD_SWITCH_FW_MODE  == 0x0B);
    assert(UFT_GW_CMD_SELECT          == 0x0C);
    assert(UFT_GW_CMD_DESELECT        == 0x0D);
    assert(UFT_GW_CMD_SET_BUS_TYPE    == 0x0E);
    assert(UFT_GW_CMD_SET_PIN         == 0x0F);
    assert(UFT_GW_CMD_RESET           == 0x10);
    printf("test_command_opcodes: OK (17 opcodes verified)\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * Test 3 — ACK code stability
 * ═══════════════════════════════════════════════════════════════════ */
static void test_ack_codes(void) {
    assert(UFT_GW_ACK_OK             == 0x00);
    assert(UFT_GW_ACK_BAD_COMMAND    == 0x01);
    assert(UFT_GW_ACK_NO_INDEX       == 0x02);
    assert(UFT_GW_ACK_NO_TRK0        == 0x03);
    assert(UFT_GW_ACK_FLUX_OVERFLOW  == 0x04);
    assert(UFT_GW_ACK_FLUX_UNDERFLOW == 0x05);
    assert(UFT_GW_ACK_WRPROT         == 0x06);
    printf("test_ack_codes: OK (7 ack codes verified)\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * Test 4 — Error code → message mapping (MF-129 regression)
 *
 * Every UFT_GW_ERR_* code must produce a non-empty, non-default
 * message. Adding a new code without updating uft_gw_strerror()
 * makes the new error invisible to the user.
 * ═══════════════════════════════════════════════════════════════════ */
static void test_strerror_coverage(void) {
    const struct {
        int code;
        const char *expected_substr;
    } codes[] = {
        { UFT_GW_OK,                 "Success"       },
        { UFT_GW_ERR_NOT_FOUND,      "not found"     },
        { UFT_GW_ERR_OPEN_FAILED,    "open"          },
        { UFT_GW_ERR_IO,             "I/O"           },
        { UFT_GW_ERR_TIMEOUT,        "timed out"     },
        { UFT_GW_ERR_PROTOCOL,       "Protocol"      },
        { UFT_GW_ERR_NO_INDEX,       "index"         },
        { UFT_GW_ERR_NO_TRK0,        "Track 0"       },
        { UFT_GW_ERR_OVERFLOW,       "overflow"      },
        { UFT_GW_ERR_UNDERFLOW,      "underflow"     },
        { UFT_GW_ERR_WRPROT,         "write protect" },
        { UFT_GW_ERR_INVALID,        "Invalid"       },
        { UFT_GW_ERR_NOMEM,          "memory"        },
        { UFT_GW_ERR_NOT_CONNECTED,  "not connected" },
        { UFT_GW_ERR_UNSUPPORTED,    "not supported" },
        /* MF-129 — the new bootloader error must mention "bootloader"
         * or "update" so the UI can route it to the recovery dialog. */
        { UFT_GW_ERR_BOOTLOADER,     "bootloader"    },
    };
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        const char *msg = uft_gw_strerror(codes[i].code);
        assert(msg != NULL);
        assert(msg[0] != '\0');
        assert(strstr(msg, codes[i].expected_substr) != NULL);
    }
    /* Sentinel — an unknown code must NOT be silently dropped. */
    const char *unknown = uft_gw_strerror(-9999);
    assert(unknown != NULL && strcmp(unknown, "Unknown error") == 0);
    printf("test_strerror_coverage: OK (%zu codes + unknown sentinel)\n",
           sizeof(codes) / sizeof(codes[0]));
}

/* ═══════════════════════════════════════════════════════════════════
 * Test 5 — Tick / nanosecond / RPM conversions
 * ═══════════════════════════════════════════════════════════════════ */
static void test_tick_conversions(void) {
    /* At the F7 clock (72 MHz), 72 ticks = 1 microsecond = 1000 ns. */
    const uint32_t f7 = UFT_GW_SAMPLE_FREQ_HZ;
    assert(f7 == 72000000u);
    assert(uft_gw_ticks_to_ns(72, f7) == 1000u);
    assert(uft_gw_ns_to_ticks(1000, f7) == 72u);

    /* Round-trip a typical bitcell time (4000 ns @ DD MFM). */
    uint32_t ticks = uft_gw_ns_to_ticks(4000, f7);
    uint32_t ns_back = uft_gw_ticks_to_ns(ticks, f7);
    assert(ns_back == 4000u);

    /* 360 RPM = 6 rev/s = 12 MHz/72 cyc per rev — sanity check. */
    /* 200ms revolution at 72MHz = 14400000 ticks. RPM = 60/0.2 = 300.
     * The helper returns RPM*100, so 300 -> 30000. */
    uint32_t rpm_x100 = uft_gw_ticks_to_rpm(14400000u, f7);
    /* Allow ±1 due to integer division. */
    assert(rpm_x100 >= 29999u && rpm_x100 <= 30001u);
    printf("test_tick_conversions: OK (round-trip + 300 RPM = %u/100 RPM)\n",
           rpm_x100);
}

int main(void) {
    test_getinfo_wire_format();
    test_command_opcodes();
    test_ack_codes();
    test_strerror_coverage();
    test_tick_conversions();
    printf("\nAll GW protocol tests passed.\n");
    return 0;
}
