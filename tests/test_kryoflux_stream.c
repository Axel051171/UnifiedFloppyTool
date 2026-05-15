/**
 * @file test_kryoflux_stream.c
 * @brief Unit tests for the KryoFlux stream-format decoder (uft_kf_decode).
 *
 * P1.24 (MF-208). These vectors are SYNTHETIC — hand-built from the
 * documented KryoFlux stream protocol, not captured from hardware. They
 * pin the decoder to the spec: every opcode class (Flux1/2/3, Nop1/2/3,
 * Ovl16, the OOB sub-types), the cell-stream position counter, the
 * overflow accumulator, index-cell resolution, and the truncation /
 * stream-end status reporting.
 *
 * What synthetic vectors prove: "the decoder matches our reading of the
 * spec." What they do NOT prove: "the decoder matches what real DTC
 * emits on real media" — that needs HIL capture vectors (a follow-up).
 * The forensic guarantee they DO lock down is the important one: the
 * decoder never invents a flux value, and a malformed container is
 * reported, not papered over.
 *
 * NOTE: this file uses a real CHECK() macro rather than assert(): the
 * test suite is built with -DNDEBUG, under which assert() is a no-op —
 * a test built on assert() would silently verify nothing. CHECK() works
 * regardless of NDEBUG and never carries side effects in its condition.
 */
#include "uft/flux/uft_kryoflux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);     \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/* ── Flux1: single-byte values 0x0E..0xFF ─────────────────────────── */
static void test_flux1_single_byte(void)
{
    const uint8_t data[] = { 0x40, 0x50, 0x60 };
    uft_kf_stream_t s;
    uft_kf_status_t init = uft_kf_init(&s);
    CHECK(init == UFT_UFT_KF_STATUS_OK, "uft_kf_init failed");
    if (init != UFT_UFT_KF_STATUS_OK) return;

    uft_kf_status_t st = uft_kf_decode(&s, data, sizeof(data));
    /* No StreamEnd block -> MISSING_END, but flux is still valid. */
    CHECK(st == UFT_UFT_KF_STATUS_MISSING_END, "expected MISSING_END");
    CHECK(s.flux_count == 3, "flux_count != 3");
    CHECK(s.flux_values[0] == 64, "flux[0] != 64");
    CHECK(s.flux_values[1] == 80, "flux[1] != 80");
    CHECK(s.flux_values[2] == 96, "flux[2] != 96");
    CHECK(s.flux_positions[0] == 0, "pos[0] != 0");
    CHECK(s.flux_positions[1] == 1, "pos[1] != 1");
    CHECK(s.flux_positions[2] == 2, "pos[2] != 2");

    uft_kf_free(&s);
    printf("  ok: flux1_single_byte\n");
}

/* ── Flux2: 2-byte values, header 0x00..0x07 ──────────────────────── */
static void test_flux2_two_byte(void)
{
    const uint8_t data[] = { 0x01, 0x02, 0x07, 0xFF };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_decode(&s, data, sizeof(data));
    CHECK(s.flux_count == 2, "flux_count != 2");
    CHECK(s.flux_values[0] == ((1u << 8) | 2u), "flux2[0] != 258");
    CHECK(s.flux_values[1] == ((7u << 8) | 0xFFu), "flux2[1] != 2047");
    CHECK(s.flux_positions[0] == 0, "pos[0] != 0");
    CHECK(s.flux_positions[1] == 2, "pos[1] != 2");

    uft_kf_free(&s);
    printf("  ok: flux2_two_byte\n");
}

/* ── Flux3: 0x0C + 2 value bytes ──────────────────────────────────── */
static void test_flux3_three_byte(void)
{
    const uint8_t data[] = { 0x0C, 0x12, 0x34 };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_decode(&s, data, sizeof(data));
    CHECK(s.flux_count == 1, "flux_count != 1");
    CHECK(s.flux_values[0] == 0x1234u, "flux3 != 0x1234");

    uft_kf_free(&s);
    printf("  ok: flux3_three_byte\n");
}

/* ── Nop1/2/3: consume bytes, emit no flux, advance position ──────── */
static void test_nops(void)
{
    const uint8_t data[] = {
        0x08, 0x40,              /* Nop1, then Flux1 64           */
        0x09, 0xAA, 0x50,        /* Nop2 (+1 skipped), Flux1 80   */
        0x0A, 0xBB, 0xCC, 0x60   /* Nop3 (+2 skipped), Flux1 96   */
    };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_decode(&s, data, sizeof(data));
    CHECK(s.flux_count == 3, "flux_count != 3");
    CHECK(s.flux_values[0] == 64, "flux[0] != 64");
    CHECK(s.flux_values[1] == 80, "flux[1] != 80");
    CHECK(s.flux_values[2] == 96, "flux[2] != 96");
    /* Position counter advances over the skipped Nop bytes. */
    CHECK(s.flux_positions[0] == 1, "pos[0] != 1");
    CHECK(s.flux_positions[1] == 4, "pos[1] != 4");
    CHECK(s.flux_positions[2] == 8, "pos[2] != 8");

    uft_kf_free(&s);
    printf("  ok: nops\n");
}

/* ── Ovl16: each 0x0B adds 0x10000 to the NEXT flux value ─────────── */
static void test_overflow(void)
{
    const uint8_t data[] = {
        0x0B, 0x40,              /* +0x10000, Flux1 64  -> 65600   */
        0x0B, 0x0B, 0x50         /* +0x20000, Flux1 80  -> 131152  */
    };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_decode(&s, data, sizeof(data));
    CHECK(s.flux_count == 2, "flux_count != 2");
    CHECK(s.flux_values[0] == 0x10000u + 64u, "ovl flux[0] wrong");
    CHECK(s.flux_values[1] == 0x20000u + 80u, "ovl flux[1] wrong");

    uft_kf_free(&s);
    printf("  ok: overflow\n");
}

/* ── OOB Index: parsed into index_internal, resolved to a flux cell ─ */
static void test_oob_index(void)
{
    const uint8_t data[] = {
        0x40, 0x50, 0x60,                       /* flux at pos 0,1,2 */
        0x0D, 0x02, 0x0C, 0x00,                 /* OOB Index, size 12 */
            0x01, 0x00, 0x00, 0x00,             /*   stream_pos = 1   */
            0x90, 0x00, 0x00, 0x00,             /*   sample_cnt = 144 */
            0x05, 0x00, 0x00, 0x00,             /*   index_cnt  = 5   */
        0x70                                    /* flux at pos 3      */
    };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_decode(&s, data, sizeof(data));
    CHECK(s.flux_count == 4, "flux_count != 4");
    /* OOB bytes do NOT advance the cell-stream position. */
    CHECK(s.flux_positions[3] == 3, "OOB advanced stream pos");

    CHECK(s.index_count == 1, "index_count != 1");
    CHECK(s.index_internal[0].stream_pos     == 1,   "index stream_pos");
    CHECK(s.index_internal[0].sample_counter == 144, "index sample_cnt");
    CHECK(s.index_internal[0].index_counter  == 5,   "index index_cnt");
    /* stream_pos 1 -> first flux cell with position >= 1 -> cell 1. */
    CHECK(s.indexes[0].flux_position == 1, "index flux_position");

    uft_kf_free(&s);
    printf("  ok: oob_index\n");
}

/* ── OOB KFInfo: sck= overrides the default sample clock ──────────── */
static void test_oob_kfinfo_sck(void)
{
    /* payload: "sck=12000000" + NUL  -> 13 bytes */
    const uint8_t data[] = {
        0x0D, 0x04, 0x0D, 0x00,
        's','c','k','=','1','2','0','0','0','0','0','0', 0x00,
        0x40
    };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_decode(&s, data, sizeof(data));
    CHECK(s.sample_clock == 12000000.0, "sck= override not applied");
    CHECK(s.flux_count == 1 && s.flux_values[0] == 64, "flux after KFInfo");

    uft_kf_free(&s);
    printf("  ok: oob_kfinfo_sck\n");
}

/* ── StreamEnd / EOF -> clean OK status ───────────────────────────── */
static void test_stream_end(void)
{
    const uint8_t end_block[] = {
        0x40,
        0x0D, 0x03, 0x08, 0x00,                 /* OOB StreamEnd, size 8 */
            0x00, 0x00, 0x00, 0x00,             /*   stream_pos          */
            0x00, 0x00, 0x00, 0x00              /*   result = OK         */
    };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }
    uft_kf_status_t st1 = uft_kf_decode(&s, end_block, sizeof(end_block));
    CHECK(st1 == UFT_UFT_KF_STATUS_OK, "StreamEnd -> OK status");
    CHECK(s.flux_count == 1, "StreamEnd flux_count");
    uft_kf_free(&s);

    /* EOF marker: OOB type 0x0D, size sentinel 0x0D0D, no payload. */
    const uint8_t eof_block[] = { 0x40, 0x0D, 0x0D, 0x0D, 0x0D };
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }
    uft_kf_status_t st2 = uft_kf_decode(&s, eof_block, sizeof(eof_block));
    CHECK(st2 == UFT_UFT_KF_STATUS_OK, "EOF -> OK status");
    CHECK(s.flux_count == 1 && s.flux_values[0] == 64, "EOF flux");
    uft_kf_free(&s);

    printf("  ok: stream_end\n");
}

/* ── Truncation: a fault is reported; flux decoded BEFORE it is kept ─ */
static void test_truncated_stream(void)
{
    /* Flux1 64, then a Flux2 opcode with no second byte. */
    const uint8_t data[] = { 0x40, 0x05 };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_status_t st = uft_kf_decode(&s, data, sizeof(data));
    CHECK(st == UFT_UFT_KF_STATUS_MISSING_DATA, "truncation -> MISSING_DATA");
    /* The valid flux value before the fault must survive — never
     * discarded, never padded out to hide the gap. */
    CHECK(s.flux_count == 1, "truncated: flux before fault must survive");
    CHECK(s.flux_values[0] == 64, "truncated: surviving flux value");

    uft_kf_free(&s);
    printf("  ok: truncated_stream\n");
}

/* ── Empty input: no flux, no crash ───────────────────────────────── */
static void test_empty_stream(void)
{
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_status_t st = uft_kf_decode(&s, NULL, 0);
    CHECK(st == UFT_UFT_KF_STATUS_MISSING_END, "empty -> MISSING_END");
    CHECK(s.flux_count == 0, "empty: flux_count != 0");
    CHECK(s.index_count == 0, "empty: index_count != 0");

    uft_kf_free(&s);
    printf("  ok: empty_stream\n");
}

/* ── Mixed realistic stream: clean decode with index + StreamEnd ──── */
static void test_mixed_stream(void)
{
    const uint8_t data[] = {
        0x80, 0x90, 0xA0, 0xB0,                 /* 4x Flux1            */
        0x0D, 0x02, 0x0C, 0x00,                 /* OOB Index, size 12  */
            0x02, 0x00, 0x00, 0x00,             /*   stream_pos = 2    */
            0x10, 0x01, 0x00, 0x00,             /*   sample_cnt = 272  */
            0x01, 0x00, 0x00, 0x00,             /*   index_cnt  = 1    */
        0xC0,                                   /* Flux1 (pos 4)       */
        0x0D, 0x03, 0x08, 0x00,                 /* OOB StreamEnd       */
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
    };
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) { CHECK(0, "init"); return; }

    uft_kf_status_t st = uft_kf_decode(&s, data, sizeof(data));
    CHECK(st == UFT_UFT_KF_STATUS_OK, "mixed -> OK status");
    CHECK(s.flux_count == 5, "mixed flux_count != 5");
    CHECK(s.flux_values[0] == 0x80 && s.flux_values[4] == 0xC0,
          "mixed flux endpoints");
    CHECK(s.flux_positions[4] == 4, "OOB Index bumped stream pos");
    CHECK(s.index_count == 1, "mixed index_count != 1");
    CHECK(s.indexes[0].flux_position == 2, "mixed index flux_position");

    uft_kf_free(&s);
    printf("  ok: mixed_stream\n");
}

int main(void)
{
    printf("test_kryoflux_stream: KryoFlux stream decoder (P1.24 / MF-208)\n");
    test_flux1_single_byte();
    test_flux2_two_byte();
    test_flux3_three_byte();
    test_nops();
    test_overflow();
    test_oob_index();
    test_oob_kfinfo_sck();
    test_stream_end();
    test_truncated_stream();
    test_empty_stream();
    test_mixed_stream();

    if (g_failures != 0) {
        printf("test_kryoflux_stream: %d CHECK(s) FAILED\n", g_failures);
        return 1;
    }
    printf("test_kryoflux_stream: all passed\n");
    return 0;
}
