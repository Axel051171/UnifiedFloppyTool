/**
 * @file tests/emulators/fc5025/test_fc5025_emulator.c
 * @brief Firmware-realistic tests for the FC5025 emulator (7/9).
 *
 * FC5025 is a sector-only, read-only USB device. The forensically
 * load-bearing property is provider Rule F-3: a CRC-error sector's
 * divergent re-reads are PRESERVED (>= 2 distinct copies), never
 * collapsed to one value. Group E asserts exactly that.
 *
 * Six groups:
 *   A. device lifecycle & detect
 *   B. CBW/CSW wire framing (round-trip)
 *   C. sector read status codes (OK / no-disk / no-sector / write-deny)
 *   D. geometry per format (vs the format table)
 *   E. per-sector CRC status + Rule F-3 divergent-read preservation
 *   F. defect classes (CRC / missing / weak) + medium-safety
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define ASSERT(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define TEST(name) printf("  [TEST] %-58s ... ", name)
#define DONE()     printf("OK\n")

/* ─── A. lifecycle ──────────────────────────────────────────────────── */

static void group_a(void) {
    printf("\n-- A. device lifecycle & detect --\n");
    fc_dev_t dev;

    TEST("reset_is_disconnected");
    fc_reset(&dev);
    ASSERT(dev.state == FC_STATE_DISCONNECTED);
    ASSERT(dev.max_retries == 3);
    DONE();

    TEST("detect_no_device_is_zero");
    ASSERT(fc_detect(&dev) == 0);
    DONE();

    TEST("power_on_defaults_detects");
    fc_power_on_defaults(&dev);
    ASSERT(fc_detect(&dev) == 1);
    ASSERT(dev.state == FC_STATE_READY);
    DONE();
}

/* ─── B. CBW/CSW framing ────────────────────────────────────────────── */

static void group_b(void) {
    printf("\n-- B. CBW/CSW wire framing --\n");

    TEST("read_cbw_has_signature_and_opcode");
    uint8_t cbw[64];
    ASSERT(fc_build_read_cbw(cbw, sizeof(cbw), 0x11223344u, 5, 0, 3, 256) == FC_CBW_LEN);
    /* signature "USBC" LE */
    ASSERT(cbw[0] == 0x55 && cbw[1] == 0x53 && cbw[2] == 0x42 && cbw[3] == 0x43);
    ASSERT(cbw[15] == FC_CMD_READ_FLEXIBLE);
    ASSERT(cbw[16] == 5 && cbw[18] == 3);   /* track, sector */
    DONE();

    TEST("csw_parse_roundtrip");
    uint8_t csw[FC_CSW_LEN];
    csw[0]=0x55; csw[1]=0x53; csw[2]=0x42; csw[3]=0x53;   /* "USBS" */
    csw[4]=0x44; csw[5]=0x33; csw[6]=0x22; csw[7]=0x11;   /* tag LE */
    csw[8]=0; csw[9]=0; csw[10]=0; csw[11]=0;             /* residue */
    csw[12]=FC_CSW_CRC_ERROR;
    uint32_t tag=0, res=99; fc_csw_status_t st;
    ASSERT(fc_parse_csw(csw, sizeof(csw), &tag, &res, &st) == true);
    ASSERT(tag == 0x11223344u && res == 0 && st == FC_CSW_CRC_ERROR);
    DONE();

    TEST("bad_csw_signature_rejected");
    csw[0] = 0x00;
    ASSERT(fc_parse_csw(csw, sizeof(csw), &tag, &res, &st) == false);
    DONE();
}

/* ─── C. read status codes ──────────────────────────────────────────── */

static void group_c(void) {
    printf("\n-- C. sector read status codes --\n");
    fc_dev_t dev;
    uft_fc_gen_params_t p = { .seed = 1, .format = UFT_FC_GEN_APPLE_DOS33,
                              .track = 0, .side = 0, .defects = 0,
                              .defect_sector = -1 };
    uft_fc_gen_track_t trk;
    uft_fc_gen_track(&p, &trk);
    fc_read_result_t r;

    TEST("clean_read_is_ok");
    fc_power_on_defaults(&dev);
    fc_mount_track(&dev, &trk);
    ASSERT(fc_read_sector(&dev, 0, 0, 3, &r) == FC_CSW_OK);
    ASSERT(r.divergent_count == 1);   /* clean = single copy */
    fc_free_read_result(&r);
    DONE();

    TEST("no_disk_read_fails");
    fc_set_disk_present(&dev, false);
    ASSERT(fc_read_sector(&dev, 0, 0, 3, &r) == FC_CSW_NO_DISK);
    fc_free_read_result(&r);
    DONE();

    TEST("out_of_range_sector_no_sector");
    fc_power_on_defaults(&dev);
    fc_mount_track(&dev, &trk);
    ASSERT(fc_read_sector(&dev, 0, 0, 99, &r) == FC_CSW_NO_SECTOR);
    fc_free_read_result(&r);
    DONE();

    TEST("write_always_refused");
    ASSERT(fc_write_sector(&dev, 0, 0, 3) == FC_CSW_WRITE_DENY);
    DONE();

    uft_fc_gen_free_track(&trk);
}

/* ─── D. geometry per format ────────────────────────────────────────── */

static void group_d(void) {
    printf("\n-- D. geometry per format --\n");
    uft_fc_geometry_t g;

    TEST("apple_dos33_geometry");
    ASSERT(uft_fc_gen_geometry(UFT_FC_GEN_APPLE_DOS33, &g));
    ASSERT(g.tracks == 35 && g.sectors_per_track == 16 && g.sector_size == 256);
    DONE();

    TEST("apple_dos32_is_13_sectors");
    ASSERT(uft_fc_gen_geometry(UFT_FC_GEN_APPLE_DOS32, &g));
    ASSERT(g.sectors_per_track == 13);
    DONE();

    TEST("ibm_fm_8inch_is_360rpm_128byte");
    ASSERT(uft_fc_gen_geometry(UFT_FC_GEN_IBM_FM_8, &g));
    ASSERT(g.sector_size == 128 && g.nominal_rpm > 355.0);
    DONE();

    TEST("track_sector_count_matches_geometry");
    uft_fc_gen_params_t p = { .seed = 2, .format = UFT_FC_GEN_IBM_FM_8,
                              .track = 0, .side = 0, .defects = 0,
                              .defect_sector = -1 };
    uft_fc_gen_track_t trk;
    ASSERT(uft_fc_gen_track(&p, &trk) == UFT_FC_GEN_OK);
    ASSERT(trk.sector_count == 26 && trk.sector_size == 128);
    uft_fc_gen_free_track(&trk);
    DONE();
}

/* ─── E. Rule F-3: divergent-read preservation ──────────────────────── */

static void group_e(void) {
    printf("\n-- E. per-sector status + Rule F-3 preservation --\n");
    fc_dev_t dev;

    /* A CRC-error on sector 7. */
    uft_fc_gen_params_t p = { .seed = 0xF3, .format = UFT_FC_GEN_APPLE_DOS33,
                              .track = 10, .side = 0,
                              .defects = UFT_FC_DEFECT_CRC_SECTOR,
                              .defect_sector = 7 };
    uft_fc_gen_track_t trk;
    uft_fc_gen_track(&p, &trk);
    fc_power_on_defaults(&dev);
    fc_set_max_retries(&dev, 3);
    fc_mount_track(&dev, &trk);
    fc_read_result_t r;

    TEST("crc_sector_reports_crc_error");
    ASSERT(fc_read_sector(&dev, 10, 0, 7, &r) == FC_CSW_CRC_ERROR);
    DONE();

    TEST("crc_sector_preserves_ge2_divergent_reads");
    /* Rule F-3: the marginal sector kept multiple DISTINCT copies. */
    ASSERT(r.divergent_count >= 2);
    ASSERT(r.attempts >= 2);
    DONE();

    TEST("divergent_copies_actually_differ");
    int all_differ = 1;
    for (int i = 1; i < r.divergent_count; i++)
        if (memcmp(r.copies[0], r.copies[i], r.copy_len) == 0) all_differ = 0;
    ASSERT(all_differ == 1);
    fc_free_read_result(&r);
    DONE();

    TEST("clean_sector_is_single_stable_copy");
    /* Sector 3 (not the defect) is clean → 1 copy, OK, stable re-read. */
    ASSERT(fc_read_sector(&dev, 10, 0, 3, &r) == FC_CSW_OK);
    ASSERT(r.divergent_count == 1);
    fc_free_read_result(&r);
    DONE();

    TEST("crc_status_never_silently_cleared");
    /* Re-reading the bad sector still reports CRC error — no silent
     * "repair" to OK. */
    ASSERT(fc_read_sector(&dev, 10, 0, 7, &r) == FC_CSW_CRC_ERROR);
    fc_free_read_result(&r);
    DONE();

    uft_fc_gen_free_track(&trk);
}

/* ─── F. defects + medium safety ────────────────────────────────────── */

static void group_f(void) {
    printf("\n-- F. defect classes + read-only invariant --\n");
    fc_dev_t dev;

    TEST("missing_sector_maps_to_no_sector");
    uft_fc_gen_params_t p = { .seed = 5, .format = UFT_FC_GEN_APPLE_DOS33,
                              .track = 0, .side = 0,
                              .defects = UFT_FC_DEFECT_MISSING_SEC,
                              .defect_sector = 4 };
    uft_fc_gen_track_t trk;
    uft_fc_gen_track(&p, &trk);
    fc_power_on_defaults(&dev);
    fc_mount_track(&dev, &trk);
    fc_read_result_t r;
    ASSERT(fc_read_sector(&dev, 0, 0, 4, &r) == FC_CSW_NO_SECTOR);
    fc_free_read_result(&r);
    uft_fc_gen_free_track(&trk);
    DONE();

    TEST("weak_sector_ok_but_diverges");
    uft_fc_gen_params_t pw = { .seed = 6, .format = UFT_FC_GEN_APPLE_DOS33,
                               .track = 0, .side = 0,
                               .defects = UFT_FC_DEFECT_WEAK_SECTOR,
                               .defect_sector = 2 };
    uft_fc_gen_track_t tw;
    uft_fc_gen_track(&pw, &tw);
    fc_power_on_defaults(&dev);
    fc_mount_track(&dev, &tw);
    /* CSW OK (no CRC flag) but the re-reads diverge → preserved copies. */
    ASSERT(fc_read_sector(&dev, 0, 0, 2, &r) == FC_CSW_OK);
    ASSERT(r.divergent_count >= 2);
    fc_free_read_result(&r);
    uft_fc_gen_free_track(&tw);
    DONE();

    TEST("read_is_deterministic");
    uft_fc_gen_track_t t1, t2;
    uft_fc_gen_params_t pc = { .seed = 0xABC, .format = UFT_FC_GEN_TRS80_SSSD,
                               .track = 4, .side = 0, .defects = 0,
                               .defect_sector = -1 };
    uft_fc_gen_track(&pc, &t1);
    uft_fc_gen_track(&pc, &t2);
    ASSERT(t1.sector_count == t2.sector_count);
    ASSERT(memcmp(t1.data, t2.data,
                  (size_t)t1.sector_count * t1.sector_size) == 0);
    uft_fc_gen_free_track(&t1);
    uft_fc_gen_free_track(&t2);
    DONE();

    TEST("unknown_format_refused");
    uft_fc_geometry_t g;
    ASSERT(uft_fc_gen_geometry((uft_fc_gen_format_t)999, &g) == false);
    DONE();

    TEST("out_of_range_track_refused");
    uft_fc_gen_params_t pb = { .seed = 1, .format = UFT_FC_GEN_APPLE_DOS33,
                               .track = 999, .side = 0, .defects = 0,
                               .defect_sector = -1 };
    uft_fc_gen_track_t tb;
    ASSERT(uft_fc_gen_track(&pb, &tb) == UFT_FC_GEN_ERR_OUT_OF_SPEC);
    DONE();
}

int main(void) {
    printf("=== FC5025 firmware-realistic emulator tests ===\n");
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
