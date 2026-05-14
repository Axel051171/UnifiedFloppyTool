/**
 * @file test_scp_read_memory.c
 * @brief Unit tests for uft_scp_read_track_memory() — the memory-buffer
 *        SCP track decoder added for P1.24 (MF-209).
 *
 * FluxEngineProviderV2 asks fluxengine for SCP output and decodes it from
 * the runner's in-memory bytes. uft_scp_read_track() is FILE*-only, so the
 * memory companion uft_scp_read_track_memory() does the work. This decoder
 * is forensic-critical — it is THE flux decode path for FluxEngine — so it
 * gets its own synthetic-vector unit test.
 *
 * SYNTHETIC vectors: hand-built SCP containers, not hardware captures.
 * They prove the decoder matches the SCP layout (header field offsets,
 * the 25ns-base resolution multiplier, BE16 flux cells, the 0x0000
 * overflow marker, per-revolution layout) and that it bounds-checks every
 * offset — a truncated/malformed container returns an error, never reads
 * out of bounds, never invents a flux value.
 *
 * NOTE: uses a real CHECK() macro, not assert() — the suite builds with
 * -DNDEBUG under which assert() is a no-op.
 */
#include "uft/flux/uft_scp_parser.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);     \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define SCP_HDR    16u
#define SCP_TABLE  (168u * 4u)
#define SCP_TRKOFF (SCP_HDR + SCP_TABLE)

static void put_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/*
 * Build a minimal SCP container into `buf`. One track at index `scp_track`
 * with `num_revs` revolutions, each carrying the SAME `flux` cell list.
 * Flags select malformations for the error-path tests.
 * Returns total byte count, or 0 on buffer overflow.
 */
static size_t build_scp(uint8_t* buf, size_t cap,
                        int scp_track, uint8_t resolution, uint8_t num_revs,
                        const uint16_t* flux, uint32_t flux_count,
                        uint32_t index_time_25ns,
                        int bad_sig, int bad_trk_sig, int omit_offset)
{
    const size_t rev_block = (size_t)flux_count * 2u;
    const size_t total = SCP_TRKOFF + 4u /*TRK hdr*/
                         + (size_t)num_revs * 12u
                         + (size_t)num_revs * rev_block;
    if (total > cap) return 0;
    memset(buf, 0, total);

    /* Header (uft_scp_header_t field offsets). */
    memcpy(buf, bad_sig ? "XXX" : "SCP", 3);
    buf[3]  = 0x22;                     /* version 2.2          */
    buf[5]  = num_revs;                 /* revolutions          */
    buf[7]  = (uint8_t)scp_track;       /* end_track            */
    buf[11] = resolution;               /* resolution (byte 11) */

    /* Offset table entry for scp_track. */
    if (!omit_offset)
        put_le32(&buf[SCP_HDR + (size_t)scp_track * 4u], (uint32_t)SCP_TRKOFF);

    /* Track header. */
    memcpy(&buf[SCP_TRKOFF], bad_trk_sig ? "YYY" : "TRK", 3);
    buf[SCP_TRKOFF + 3] = (uint8_t)scp_track;

    /* Revolution entries, then each revolution's flux block. The flux
     * blocks all sit after the last rev entry, laid out sequentially. */
    const size_t flux_base = SCP_TRKOFF + 4u + (size_t)num_revs * 12u;
    for (uint8_t r = 0; r < num_revs; ++r) {
        size_t rev_entry  = SCP_TRKOFF + 4u + (size_t)r * 12u;
        size_t flux_pos   = flux_base + (size_t)r * rev_block;
        put_le32(&buf[rev_entry + 0], index_time_25ns);
        put_le32(&buf[rev_entry + 4], flux_count);
        put_le32(&buf[rev_entry + 8], (uint32_t)(flux_pos - SCP_TRKOFF));
        for (uint32_t i = 0; i < flux_count; ++i) {
            buf[flux_pos + i * 2u + 0] = (uint8_t)((flux[i] >> 8) & 0xFF);
            buf[flux_pos + i * 2u + 1] = (uint8_t)(flux[i] & 0xFF);
        }
    }
    return total;
}

/* ── valid single-revolution decode, 25 ns base period ───────────── */
static void test_valid_single_rev(void)
{
    uint8_t buf[4096];
    const uint16_t flux[] = { 100, 150, 200, 300 };
    size_t n = build_scp(buf, sizeof(buf), /*track=*/0, /*resolution=*/0,
                         /*revs=*/1, flux, 4, /*index=*/8000000u, 0, 0, 0);
    CHECK(n > 0, "build_scp overflow");

    uft_scp_track_data_t td;
    int rc = uft_scp_read_track_memory(buf, n, 0, &td);
    CHECK(rc == UFT_SCP_OK, "valid SCP must decode OK");
    CHECK(td.revolution_count == 1, "revolution_count != 1");
    CHECK(td.revolutions[0].flux_count == 4, "flux_count != 4");
    /* resolution 0 -> 25 ns period; no overflow -> exact values. */
    CHECK(td.revolutions[0].flux_data[0] == 2500, "flux[0] != 2500ns");
    CHECK(td.revolutions[0].flux_data[1] == 3750, "flux[1] != 3750ns");
    CHECK(td.revolutions[0].flux_data[2] == 5000, "flux[2] != 5000ns");
    CHECK(td.revolutions[0].flux_data[3] == 7500, "flux[3] != 7500ns");
    uft_scp_free_track(&td);
    printf("  ok: valid_single_rev\n");
}

/* ── resolution != 0: period = 25 * (1 + resolution) ─────────────────
 * This vector pins the resolution byte at offset 11. A decoder that read
 * the wrong header byte (e.g. bit_cell_width at 9) would still pass with
 * resolution 0 — here resolution is 3, so the period MUST be 100 ns. */
static void test_resolution_multiplier(void)
{
    uint8_t buf[4096];
    const uint16_t flux[] = { 10, 40 };
    size_t n = build_scp(buf, sizeof(buf), 0, /*resolution=*/3,
                         1, flux, 2, 8000000u, 0, 0, 0);
    CHECK(n > 0, "build_scp overflow");

    uft_scp_track_data_t td;
    int rc = uft_scp_read_track_memory(buf, n, 0, &td);
    CHECK(rc == UFT_SCP_OK, "resolution vector must decode OK");
    /* period = 25 * (1 + 3) = 100 ns. */
    CHECK(td.revolutions[0].flux_data[0] == 1000,
          "resolution multiplier wrong (expected 10 * 100ns)");
    CHECK(td.revolutions[0].flux_data[1] == 4000,
          "resolution multiplier wrong (expected 40 * 100ns)");
    uft_scp_free_track(&td);
    printf("  ok: resolution_multiplier\n");
}

/* ── 0x0000 overflow marker: accumulate 65536 into the next cell ──── */
static void test_overflow_marker(void)
{
    uint8_t buf[4096];
    /* cell 0 = overflow marker, cell 1 = 0x0064 (100). The decoded value
     * for cell 1 must be (65536 + 100) * 25 ns; cell 0 stays a 0
     * placeholder (the caller skips it — not a real transition). */
    const uint16_t flux[] = { 0x0000, 0x0064 };
    size_t n = build_scp(buf, sizeof(buf), 0, 0, 1, flux, 2, 8000000u, 0, 0, 0);
    CHECK(n > 0, "build_scp overflow");

    uft_scp_track_data_t td;
    int rc = uft_scp_read_track_memory(buf, n, 0, &td);
    CHECK(rc == UFT_SCP_OK, "overflow vector must decode OK");
    CHECK(td.revolutions[0].flux_data[0] == 0,
          "overflow marker slot must be a 0 placeholder");
    CHECK(td.revolutions[0].flux_data[1] == (65536u + 100u) * 25u,
          "overflow not folded into the next cell");
    uft_scp_free_track(&td);
    printf("  ok: overflow_marker\n");
}

/* ── multi-revolution: every revolution decoded ──────────────────── */
static void test_multi_revolution(void)
{
    uint8_t buf[4096];
    const uint16_t flux[] = { 80, 120 };
    size_t n = build_scp(buf, sizeof(buf), 0, 0, /*revs=*/3,
                         flux, 2, 8000000u, 0, 0, 0);
    CHECK(n > 0, "build_scp overflow");

    uft_scp_track_data_t td;
    int rc = uft_scp_read_track_memory(buf, n, 0, &td);
    CHECK(rc == UFT_SCP_OK, "multi-rev vector must decode OK");
    CHECK(td.revolution_count == 3, "revolution_count != 3");
    for (int r = 0; r < 3; ++r) {
        CHECK(td.revolutions[r].flux_count == 2, "per-rev flux_count != 2");
        CHECK(td.revolutions[r].flux_data &&
              td.revolutions[r].flux_data[0] == 2000 &&
              td.revolutions[r].flux_data[1] == 3000,
              "per-rev flux values wrong");
    }
    uft_scp_free_track(&td);
    printf("  ok: multi_revolution\n");
}

/* ── track index resolution: cylinder*2 + head ───────────────────── */
static void test_track_index(void)
{
    uint8_t buf[4096];
    const uint16_t flux[] = { 100 };
    /* Place the track at index 5 (cylinder 2, head 1). */
    size_t n = build_scp(buf, sizeof(buf), /*track=*/5, 0, 1, flux, 1,
                         8000000u, 0, 0, 0);
    CHECK(n > 0, "build_scp overflow");

    uft_scp_track_data_t td;
    /* Asking for the wrong track index must fail cleanly (offset 0). */
    int rc_wrong = uft_scp_read_track_memory(buf, n, 4, &td);
    CHECK(rc_wrong == UFT_SCP_ERR_TRACK,
          "absent track must return ERR_TRACK, not garbage");
    /* The correct index decodes. */
    int rc_ok = uft_scp_read_track_memory(buf, n, 5, &td);
    CHECK(rc_ok == UFT_SCP_OK, "track at its real index must decode");
    CHECK(td.track_number == 5, "track_number mismatch");
    uft_scp_free_track(&td);
    printf("  ok: track_index\n");
}

/* ── malformed inputs: reported, never read out of bounds ────────── */
static void test_malformed(void)
{
    uint8_t buf[4096];
    const uint16_t flux[] = { 100, 200 };
    uft_scp_track_data_t td;

    /* Bad file signature. */
    size_t n1 = build_scp(buf, sizeof(buf), 0, 0, 1, flux, 2, 8000000u,
                          /*bad_sig=*/1, 0, 0);
    CHECK(uft_scp_read_track_memory(buf, n1, 0, &td) == UFT_SCP_ERR_SIGNATURE,
          "bad SCP signature must return ERR_SIGNATURE");

    /* Bad track header signature. */
    size_t n2 = build_scp(buf, sizeof(buf), 0, 0, 1, flux, 2, 8000000u,
                          0, /*bad_trk_sig=*/1, 0);
    CHECK(uft_scp_read_track_memory(buf, n2, 0, &td) == UFT_SCP_ERR_SIGNATURE,
          "bad track signature must return ERR_SIGNATURE");

    /* Track offset omitted (entry is 0 -> track not present). */
    size_t n3 = build_scp(buf, sizeof(buf), 0, 0, 1, flux, 2, 8000000u,
                          0, 0, /*omit_offset=*/1);
    CHECK(uft_scp_read_track_memory(buf, n3, 0, &td) == UFT_SCP_ERR_TRACK,
          "absent track offset must return ERR_TRACK");

    /* Truncated buffer: a valid file cut short before the flux span.
     * Must report a read error, not walk off the end. */
    size_t n4 = build_scp(buf, sizeof(buf), 0, 0, 1, flux, 2, 8000000u, 0, 0, 0);
    CHECK(uft_scp_read_track_memory(buf, n4 - 2, 0, &td) == UFT_SCP_ERR_READ,
          "truncated flux span must return ERR_READ");

    /* Too small even for header + offset table. */
    CHECK(uft_scp_read_track_memory(buf, 100, 0, &td) == UFT_SCP_ERR_READ,
          "buffer smaller than header+table must return ERR_READ");

    /* NULL guards. */
    CHECK(uft_scp_read_track_memory(NULL, 1000, 0, &td) == UFT_SCP_ERR_NULLPTR,
          "NULL data must return ERR_NULLPTR");
    CHECK(uft_scp_read_track_memory(buf, n4, 0, NULL) == UFT_SCP_ERR_NULLPTR,
          "NULL out must return ERR_NULLPTR");

    printf("  ok: malformed\n");
}

int main(void)
{
    printf("test_scp_read_memory: uft_scp_read_track_memory (P1.24 / MF-209)\n");
    test_valid_single_rev();
    test_resolution_multiplier();
    test_overflow_marker();
    test_multi_revolution();
    test_track_index();
    test_malformed();

    if (g_failures != 0) {
        printf("test_scp_read_memory: %d CHECK(s) FAILED\n", g_failures);
        return 1;
    }
    printf("test_scp_read_memory: all passed\n");
    return 0;
}
