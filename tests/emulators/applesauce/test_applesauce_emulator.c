/**
 * @file tests/emulators/applesauce/test_applesauce_emulator.c
 * @brief Firmware-realistic tests for the Applesauce emulator (4/9).
 *
 * Six groups:
 *   A. Lifecycle & reset
 *   B. Power/motor/sync state sequencing
 *   C. Commands & queries (head, disk:?write, ?kind/?vers/?pcb, data:?*)
 *   D. Capture + binary download pipeline
 *   E. Forensic-safety (write refusal, desync wedge, timeout drill)
 *   F. Flux generator (Apple GCR structure + defects + HAL SSOT xcheck)
 *
 * Determinism: every generator call takes an explicit seed; two runs of
 * this binary produce byte-identical results. The HAL SSOT cross-check
 * links src/hal/uft_applesauce.c (stub build — only the pure-math tick
 * functions are called) so the emulator's 8 MHz assumption can never
 * silently drift from the HAL's.
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"
#include "uft/hal/uft_applesauce.h"

#include <stdio.h>
#include <string.h>

/* Two-sided float compare — avoids a libm (fabs) dependency so the test
 * links cleanly under CI's LINK_LIBRARIES_ONLY_TARGETS (MF-305). */
static int approx(double a, double b, double eps) {
    double d = a - b;
    return d < eps && d > -eps;
}

static int g_pass = 0, g_fail = 0;

#define ASSERT(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; \
        printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define TEST(name) printf("  [TEST] %-58s ... ", name)
#define DONE()     printf("OK\n")

/* Convenience: run one line, return class, capture response text. */
static as_fw_resp_class_t line(as_fw_t *fw, const char *cmd, char *out) {
    return as_fw_process_line(fw, cmd, out, AS_FW_LINE_MAX);
}

/* ─── A. Lifecycle ──────────────────────────────────────────────────── */

static void group_a(void) {
    printf("\n-- A. Lifecycle & reset --\n");
    char r[AS_FW_LINE_MAX];

    TEST("reset_zeroes_to_power_on");
    as_fw_t fw;
    as_fw_reset(&fw);
    ASSERT(fw.state == AS_FW_STATE_POWER_ON);
    ASSERT(fw.drive_kind == AS_FW_KIND_NONE);
    ASSERT(fw.current_track == -1);
    ASSERT(fw.cmd_count == 0);
    DONE();

    TEST("power_on_defaults_is_525_bench_ready");
    as_fw_power_on_defaults(&fw);
    ASSERT(fw.drive_kind == AS_FW_KIND_525);
    ASSERT(fw.disk_present && fw.index_present && !fw.write_protected);
    ASSERT(fw.buffer_max == AS_FW_BUFFER_PLUS);
    DONE();

    TEST("null_context_is_silent_not_crash");
    ASSERT(as_fw_process_line(NULL, "psu:on", r, sizeof(r)) == AS_FW_RESP_SILENT);
    ASSERT(line(&fw, NULL, r) == AS_FW_RESP_SILENT);
    DONE();

    TEST("vers_pcb_configurable");
    as_fw_set_version(&fw, "Applesauce 2.10", "2.2");
    ASSERT(line(&fw, "?vers", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+Applesauce 2.10") == 0);
    ASSERT(line(&fw, "?pcb", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+2.2") == 0);
    DONE();
}

/* ─── B. Power sequencing ───────────────────────────────────────────── */

static void group_b(void) {
    printf("\n-- B. Power/motor/sync sequencing --\n");
    char r[AS_FW_LINE_MAX];
    as_fw_t fw;

    TEST("motor_before_psu_is_no_power");
    as_fw_reset(&fw);
    fw.drive_kind = AS_FW_KIND_525;
    ASSERT(line(&fw, "motor:on", r) == AS_FW_RESP_NO_POWER);
    ASSERT(strcmp(r, "v") == 0);
    ASSERT(fw.state == AS_FW_STATE_POWER_ON);
    DONE();

    TEST("psu_motor_sync_climbs_state");
    ASSERT(line(&fw, "psu:on", r) == AS_FW_RESP_ACK);
    ASSERT(fw.state == AS_FW_STATE_PSU_ON);
    ASSERT(line(&fw, "motor:on", r) == AS_FW_RESP_ACK);
    ASSERT(fw.state == AS_FW_STATE_MOTOR_ON);
    ASSERT(line(&fw, "sync:on", r) == AS_FW_RESP_ACK);
    ASSERT(fw.state == AS_FW_STATE_SYNC_ON);
    DONE();

    TEST("sync_before_motor_is_error");
    as_fw_reset(&fw);
    line(&fw, "psu:on", r);
    ASSERT(line(&fw, "sync:on", r) == AS_FW_RESP_ERROR);
    ASSERT(r[0] == '-');
    DONE();

    TEST("motor_off_drops_back_to_psu");
    as_fw_power_on_defaults(&fw);
    line(&fw, "psu:on", r); line(&fw, "motor:on", r); line(&fw, "sync:on", r);
    ASSERT(line(&fw, "motor:off", r) == AS_FW_RESP_ACK);
    ASSERT(fw.state == AS_FW_STATE_PSU_ON);
    ASSERT(!fw.sync_on && !fw.motor_on);
    DONE();

    TEST("psu_query_reports_truth");
    as_fw_reset(&fw);
    ASSERT(line(&fw, "psu:?", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+off") == 0);
    line(&fw, "psu:on", r);
    ASSERT(line(&fw, "psu:?", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+on") == 0);
    DONE();
}

/* ─── C. Commands & queries ─────────────────────────────────────────── */

static void group_c(void) {
    printf("\n-- C. Commands & queries --\n");
    char r[AS_FW_LINE_MAX];
    as_fw_t fw;
    as_fw_power_on_defaults(&fw);
    line(&fw, "psu:on", r);

    TEST("head_track_in_range_acks");
    ASSERT(line(&fw, "head:track 17", r) == AS_FW_RESP_ACK);
    ASSERT(fw.current_track == 17);
    DONE();

    TEST("head_track_out_of_range_errors");
    ASSERT(line(&fw, "head:track 999", r) == AS_FW_RESP_ERROR);
    ASSERT(fw.current_track == 17);  /* unchanged */
    DONE();

    TEST("head_track_nonnumeric_errors");
    ASSERT(line(&fw, "head:track x9", r) == AS_FW_RESP_ERROR);
    DONE();

    TEST("head_side_and_zero");
    ASSERT(line(&fw, "head:side 1", r) == AS_FW_RESP_ACK);
    ASSERT(fw.current_side == 1);
    ASSERT(line(&fw, "head:zero", r) == AS_FW_RESP_ACK);
    ASSERT(fw.current_track == 0);
    DONE();

    TEST("kind_reports_configured_drive");
    as_fw_set_drive_kind(&fw, AS_FW_KIND_35);
    ASSERT(line(&fw, "?kind", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+3.5") == 0);
    DONE();

    TEST("disk_write_query_truthful");
    as_fw_set_write_protected(&fw, true);
    ASSERT(line(&fw, "disk:?write", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+protected") == 0);
    as_fw_set_write_protected(&fw, false);
    line(&fw, "disk:?write", r);
    ASSERT(strcmp(r, "+writable") == 0);
    DONE();

    TEST("data_max_reports_buffer");
    ASSERT(line(&fw, "data:?max", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(strcmp(r, "+430080") == 0);
    DONE();

    TEST("sync_speed_needs_index");
    line(&fw, "motor:on", r); line(&fw, "sync:on", r);
    ASSERT(line(&fw, "sync:?speed", r) == AS_FW_RESP_OK_PAYLOAD);
    ASSERT(r[0] == '+');
    as_fw_set_index_present(&fw, false);
    ASSERT(line(&fw, "sync:?speed", r) == AS_FW_RESP_ERROR);
    DONE();

    TEST("unknown_command_is_question");
    ASSERT(line(&fw, "frobnicate", r) == AS_FW_RESP_UNKNOWN);
    ASSERT(strcmp(r, "?") == 0);
    DONE();
}

/* ─── D. Capture + binary download ──────────────────────────────────── */

static void group_d(void) {
    printf("\n-- D. Capture + binary download pipeline --\n");
    char r[AS_FW_LINE_MAX];
    as_fw_t fw;

    /* Build a real synthetic 5.25" capture to stream. */
    uft_as_flux_params_t p = { .seed = 0xA5A5A5A5u, .track = 17, .side = 0,
                               .volume = 254, .defects = UFT_AS_DEFECT_NONE,
                               .weak_jitter_pct = 0 };
    uft_as_flux_capture_t cap;
    ASSERT(uft_as_flux_gen_a2_525(&p, &cap) == UFT_AS_FLUX_GEN_OK);

    as_fw_power_on_defaults(&fw);
    as_fw_load_capture(&fw, cap.bytes, cap.bytes_len);
    line(&fw, "psu:on", r); line(&fw, "motor:on", r); line(&fw, "sync:on", r);

    TEST("readx_requires_synced_disk_index");
    ASSERT(line(&fw, "disk:readx 3", r) == AS_FW_RESP_ACK);
    ASSERT(fw.state == AS_FW_STATE_CAPTURED);
    ASSERT(fw.captured_len == cap.bytes_len);
    ASSERT(fw.captured_revs == 3);
    DONE();

    TEST("data_size_matches_capture");
    ASSERT(line(&fw, "data:?size", r) == AS_FW_RESP_OK_PAYLOAD);
    char expect[AS_FW_LINE_MAX];
    snprintf(expect, sizeof(expect), "+%zu", cap.bytes_len);
    ASSERT(strcmp(r, expect) == 0);
    DONE();

    TEST("binary_download_streams_exact_bytes");
    char sz[AS_FW_LINE_MAX];
    snprintf(sz, sizeof(sz), "data:< %zu", cap.bytes_len);
    ASSERT(line(&fw, sz, r) == AS_FW_RESP_BINARY);
    ASSERT(fw.state == AS_FW_STATE_BINARY_TX);
    uint8_t buf[512];
    size_t total = 0, n;
    while ((n = as_fw_pop_binary(&fw, buf, sizeof(buf))) > 0) total += n;
    ASSERT(total == cap.bytes_len);
    ASSERT(fw.state == AS_FW_STATE_CAPTURED);   /* auto-returns when done */
    DONE();

    TEST("streamed_bytes_are_capture_verbatim");
    /* Re-download and compare against the source buffer byte-for-byte. */
    snprintf(sz, sizeof(sz), "data:< %zu", cap.bytes_len);
    line(&fw, sz, r);
    size_t off = 0; int mism = 0;
    while ((n = as_fw_pop_binary(&fw, buf, sizeof(buf))) > 0) {
        if (memcmp(buf, cap.bytes + off, n) != 0) mism = 1;
        off += n;
    }
    ASSERT(mism == 0);
    DONE();

    TEST("data_clear_drops_capture");
    ASSERT(line(&fw, "data:clear", r) == AS_FW_RESP_ACK);
    ASSERT(fw.captured_len == 0);
    ASSERT(fw.state == AS_FW_STATE_SYNC_ON);
    DONE();

    TEST("readx_without_disk_errors");
    as_fw_set_disk_present(&fw, false);
    ASSERT(line(&fw, "disk:readx 1", r) == AS_FW_RESP_ERROR);
    DONE();

    uft_as_flux_gen_free(&cap);
}

/* ─── E. Forensic safety ────────────────────────────────────────────── */

static void group_e(void) {
    printf("\n-- E. Forensic-safety guards --\n");
    char r[AS_FW_LINE_MAX];
    as_fw_t fw;
    as_fw_power_on_defaults(&fw);
    line(&fw, "psu:on", r);

    TEST("disk_write_always_refused");
    ASSERT(line(&fw, "disk:write", r) == AS_FW_RESP_PROTOCOL);
    ASSERT(strcmp(r, "!") == 0);
    DONE();

    TEST("data_upload_always_refused");
    ASSERT(line(&fw, "data:> 512", r) == AS_FW_RESP_PROTOCOL);
    DONE();

    TEST("host_line_during_binary_wedges_device");
    uft_as_flux_params_t p = { .seed = 1, .track = 0, .side = 0,
                               .volume = 254, .defects = 0, .weak_jitter_pct = 0 };
    uft_as_flux_capture_t cap;
    uft_as_flux_gen_a2_525(&p, &cap);
    as_fw_load_capture(&fw, cap.bytes, cap.bytes_len);
    line(&fw, "motor:on", r); line(&fw, "sync:on", r); line(&fw, "disk:readx 1", r);
    char sz[AS_FW_LINE_MAX];
    snprintf(sz, sizeof(sz), "data:< %zu", cap.bytes_len);
    line(&fw, sz, r);
    ASSERT(fw.state == AS_FW_STATE_BINARY_TX);
    /* Send a command instead of draining → desync wedge. */
    ASSERT(line(&fw, "psu:?", r) == AS_FW_RESP_SILENT);
    ASSERT(fw.state == AS_FW_STATE_ERROR);
    /* Sticky: even valid commands stay silent until reset. */
    ASSERT(line(&fw, "?vers", r) == AS_FW_RESP_SILENT);
    as_fw_reset(&fw);
    ASSERT(fw.state == AS_FW_STATE_POWER_ON);
    uft_as_flux_gen_free(&cap);
    DONE();

    TEST("silent_next_drills_host_timeout");
    as_fw_power_on_defaults(&fw);
    as_fw_set_silent_next(&fw, 2);
    ASSERT(line(&fw, "psu:on", r) == AS_FW_RESP_SILENT);
    ASSERT(line(&fw, "psu:on", r) == AS_FW_RESP_SILENT);
    ASSERT(line(&fw, "psu:on", r) == AS_FW_RESP_ACK);  /* 3rd is live */
    DONE();

    TEST("over_length_line_is_protocol_error");
    char longline[AS_FW_LINE_MAX + 8];
    memset(longline, 'x', sizeof(longline) - 1);
    longline[sizeof(longline) - 1] = '\0';
    ASSERT(line(&fw, longline, r) == AS_FW_RESP_PROTOCOL);
    DONE();
}

/* ─── F. Flux generator + HAL SSOT ──────────────────────────────────── */

static void group_f(void) {
    printf("\n-- F. Flux generator + HAL SSOT cross-check --\n");

    TEST("hal_ssot_sample_clock_agrees");
    /* The generator's 8 MHz assumption must equal the HAL's. */
    ASSERT((uint32_t)uft_as_get_sample_clock() == UFT_AS_FLUX_GEN_SAMPLE_HZ);
    /* 8 ticks @ 125 ns = 1000 ns. */
    ASSERT(approx(uft_as_ticks_to_ns(8), 1000.0, 1e-6));
    ASSERT(uft_as_ns_to_ticks(1000.0) == 8);
    DONE();

    TEST("hal_format_name_table_live");
    ASSERT(strcmp(uft_as_format_name(UFT_AS_FMT_DOS33), "") != 0);
    ASSERT(uft_as_format_name((uft_as_format_t)9999) != NULL);
    DONE();

    TEST("clean_525_track_is_deterministic");
    uft_as_flux_params_t p = { .seed = 0xDEADBEEFu, .track = 17, .side = 0,
                               .volume = 254, .defects = 0, .weak_jitter_pct = 0 };
    uft_as_flux_capture_t a, b;
    ASSERT(uft_as_flux_gen_a2_525(&p, &a) == UFT_AS_FLUX_GEN_OK);
    ASSERT(uft_as_flux_gen_a2_525(&p, &b) == UFT_AS_FLUX_GEN_OK);
    ASSERT(a.bytes_len == b.bytes_len);
    ASSERT(a.bytes_len > 0 && memcmp(a.bytes, b.bytes, a.bytes_len) == 0);
    DONE();

    TEST("clean_525_has_16_sectors_and_is_medium_safe");
    ASSERT(a.sector_count == 16);
    ASSERT(uft_as_flux_gen_count_unsafe(&a) == 0);
    DONE();

    TEST("clean_525_nibbles_show_address_prologs");
    /* Decode the flux back to disk nibbles and count D5 AA 96 address
     * prologs — one per sector on a healthy DOS 3.3 track. */
    static uint8_t nib[200000];
    size_t nn = uft_as_flux_gen_decode_nibbles(&a, UFT_AS_FLUX_GEN_CELL_525_NS,
                                               nib, sizeof(nib));
    int prologs = 0;
    for (size_t i = 0; i + 2 < nn; i++)
        if (nib[i] == 0xD5 && nib[i+1] == 0xAA && nib[i+2] == 0x96) prologs++;
    ASSERT(nn > 0);
    ASSERT(prologs == 16);
    DONE();

    TEST("checksum_defect_changes_payload");
    uft_as_flux_params_t pd = p;
    pd.defects = UFT_AS_DEFECT_CHECKSUM_ERROR;
    uft_as_flux_capture_t c;
    ASSERT(uft_as_flux_gen_a2_525(&pd, &c) == UFT_AS_FLUX_GEN_OK);
    /* A corrupted checksum must alter the byte stream vs the clean run. */
    ASSERT(c.bytes_len != a.bytes_len ||
           memcmp(c.bytes, a.bytes, a.bytes_len) != 0);
    ASSERT(uft_as_flux_gen_count_unsafe(&c) == 0);  /* still medium-safe */
    uft_as_flux_gen_free(&c);
    DONE();

    TEST("weak_bits_stay_within_10pct_jitter");
    uft_as_flux_params_t pw = p;
    pw.defects = UFT_AS_DEFECT_WEAK_BITS;
    pw.weak_jitter_pct = 10;
    uft_as_flux_capture_t w;
    ASSERT(uft_as_flux_gen_a2_525(&pw, &w) == UFT_AS_FLUX_GEN_OK);
    ASSERT(uft_as_flux_gen_count_unsafe(&w) == 0);  /* guard holds */
    uft_as_flux_gen_free(&w);
    DONE();

    TEST("out_of_spec_track_refused");
    uft_as_flux_params_t pb = p;
    pb.track = 99;  /* > 34 for 5.25" */
    uft_as_flux_capture_t bad;
    ASSERT(uft_as_flux_gen_a2_525(&pb, &bad) == UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC);
    DONE();

    TEST("mac_zone_table_boundaries");
    ASSERT(uft_as_flux_mac_zone_for_track(0)  == 0);
    ASSERT(uft_as_flux_mac_zone_for_track(79) == 4);
    ASSERT(uft_as_flux_mac_zone_for_track(80) == -1);
    ASSERT(uft_as_flux_mac_sectors_for_track(0)  == 12);
    ASSERT(uft_as_flux_mac_sectors_for_track(79) == 8);
    ASSERT(uft_as_flux_mac_rpm_for_zone(0) > 390.0);
    ASSERT(uft_as_flux_mac_rpm_for_zone(4) > 580.0);
    DONE();

    TEST("mac_35_track_generates_zone_sectors");
    uft_as_flux_params_t pm = { .seed = 7, .track = 0, .side = 0,
                                .volume = 0, .defects = 0, .weak_jitter_pct = 0 };
    uft_as_flux_capture_t m;
    ASSERT(uft_as_flux_gen_mac_35(&pm, &m) == UFT_AS_FLUX_GEN_OK);
    ASSERT(m.sector_count == 12);           /* zone 0 */
    ASSERT(uft_as_flux_gen_count_unsafe(&m) == 0);
    uft_as_flux_gen_free(&m);
    DONE();

    TEST("gcr62_table_roundtrips");
    int roundtrips = 0;
    for (uint8_t six = 0; six < 64; six++) {
        uint8_t nibv = uft_as_flux_gcr62_nibble(six);
        if (nibv >= 0x96 && uft_as_flux_gcr62_detrans(nibv) == (int)six)
            roundtrips++;
    }
    ASSERT(roundtrips == 64);
    ASSERT(uft_as_flux_gcr62_detrans(0x00) == -1);  /* not a GCR nibble */
    DONE();

    uft_as_flux_gen_free(&a);
    uft_as_flux_gen_free(&b);
}

int main(void) {
    printf("=== Applesauce firmware-realistic emulator tests ===\n");
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
