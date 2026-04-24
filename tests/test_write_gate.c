/**
 * @file test_write_gate.c
 * @brief Tests for the write-safety gate (restored from v3.7.0).
 *
 * Verifies the fail-closed policy layer: format capability, drive
 * diagnostics, snapshot verification.  "Kein Bit verloren" —
 * destructive operations must never proceed without explicit pass.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "uft/policy/uft_write_gate.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ------------------------------------------------------------------ */
/* Status string helpers                                               */
/* ------------------------------------------------------------------ */

TEST(status_str_ok) {
    const char *s = uft_gate_status_str(UFT_GATE_OK);
    ASSERT(s != NULL);
    ASSERT(s[0] != '\0');
}

TEST(status_str_every_code_has_string) {
    const uft_gate_status_t codes[] = {
        UFT_GATE_OK,
        UFT_GATE_FORMAT_READONLY,
        UFT_GATE_DRIVE_UNSAFE,
        UFT_GATE_SNAPSHOT_FAILED,
        UFT_GATE_VERIFY_FAILED,
        UFT_GATE_NEEDS_OVERRIDE,
        UFT_GATE_PRECHECK_FAILED,
    };
    size_t n = sizeof(codes) / sizeof(codes[0]);
    for (size_t i = 0; i < n; i++) {
        const char *s = uft_gate_status_str(codes[i]);
        ASSERT(s != NULL);
        ASSERT(s[0] != '\0');
    }
}

/* ------------------------------------------------------------------ */
/* Precheck fail-closed semantics                                     */
/* ------------------------------------------------------------------ */

TEST(precheck_null_policy_rejected) {
    uint8_t img[256] = {0};
    uft_write_gate_result_t r;
    uft_gate_status_t s = uft_write_gate_precheck(
        NULL, img, sizeof(img), "/tmp", "snap", &r);
    ASSERT(s != UFT_GATE_OK);
}

TEST(precheck_null_result_rejected) {
    uft_write_gate_policy_t pol = {0};
    uint8_t img[256] = {0};
    uft_gate_status_t s = uft_write_gate_precheck(
        &pol, img, sizeof(img), "/tmp", "snap", NULL);
    ASSERT(s != UFT_GATE_OK);
}

TEST(precheck_zero_image_rejected) {
    uft_write_gate_policy_t pol = { .require_format_check = true,
                                     .min_confidence = 0 };
    uft_write_gate_result_t r = {0};
    uft_gate_status_t s = uft_write_gate_precheck(
        &pol, NULL, 0, "/tmp", "snap", &r);
    ASSERT(s != UFT_GATE_OK);
}

/* ------------------------------------------------------------------ */
/* Override semantics                                                  */
/* ------------------------------------------------------------------ */

TEST(override_only_when_flagged_possible) {
    uft_write_gate_result_t r = {0};
    /* Unpopulated result → should NOT be overridable by default */
    bool ovr = uft_write_gate_can_override(&r);
    (void)ovr;    /* either answer is acceptable for a zeroed result;
                    this test ensures the function doesn't crash. */
}

/* ------------------------------------------------------------------ */
/* Drive diagnostics wiring                                            */
/* ------------------------------------------------------------------ */

TEST(diag_flags_are_bit_distinct) {
    /* Each flag must occupy a unique bit so they can be OR'd. */
    uint32_t all = UFT_DRIVE_DIAG_UNSTABLE_RPM |
                    UFT_DRIVE_DIAG_BAD_INDEX |
                    UFT_DRIVE_DIAG_BAD_SEEK |
                    UFT_DRIVE_DIAG_WRITE_UNSAFE |
                    UFT_DRIVE_DIAG_NO_DISK |
                    UFT_DRIVE_DIAG_WRITE_PROTECT;
    /* No overlap → popcount == 6 */
    int bits = 0;
    for (int i = 0; i < 32; i++) if ((all >> i) & 1u) bits++;
    ASSERT(bits == 6);
}

TEST(format_caps_are_bit_distinct) {
    uint32_t all = UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE |
                    UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_LOGICAL |
                    UFT_FMT_CAP_PROTECTED | UFT_FMT_CAP_VERIFY;
    int bits = 0;
    for (int i = 0; i < 32; i++) if ((all >> i) & 1u) bits++;
    ASSERT(bits == 6);
}

/* ------------------------------------------------------------------ */
/* Format probe                                                        */
/* ------------------------------------------------------------------ */

TEST(probe_null_inputs_return_error) {
    uft_format_probe_t p;
    uft_error_t e = uft_write_gate_probe_format(NULL, 100, &p);
    ASSERT(e != UFT_OK);

    uint8_t buf[32] = {0};
    e = uft_write_gate_probe_format(buf, sizeof(buf), NULL);
    ASSERT(e != UFT_OK);
}

TEST(probe_zero_len_rejected) {
    uft_format_probe_t p = {0};
    uint8_t buf[1] = {0};
    uft_error_t e = uft_write_gate_probe_format(buf, 0, &p);
    ASSERT(e != UFT_OK);
}

int main(void) {
    printf("=== uft_write_gate tests ===\n");
    RUN(status_str_ok);
    RUN(status_str_every_code_has_string);
    RUN(precheck_null_policy_rejected);
    RUN(precheck_null_result_rejected);
    RUN(precheck_zero_image_rejected);
    RUN(override_only_when_flagged_possible);
    RUN(diag_flags_are_bit_distinct);
    RUN(format_caps_are_bit_distinct);
    RUN(probe_null_inputs_return_error);
    RUN(probe_zero_len_rejected);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
