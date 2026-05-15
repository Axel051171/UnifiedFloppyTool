/**
 * @file test_flux_gcr_c64.c
 * @brief Hardware-independent unit test for flux_decode_gcr_c64.
 *
 * MOTIVATION (audit/test_coverage/COVERAGE_AUDIT.md Lücke #1): UFT
 * decodes Commodore 1541 GCR exclusively via the differential
 * conformance harness, which skips when greaseweazle's `gw` binary
 * and its 196608-byte corpus are absent. A non-hardware-binding
 * regression test for the encoding has been missing — symmetrical to
 * the IBM-MFM coverage that test_flux_mfm_sync.c (MF-218) already
 * provides.
 *
 * WHAT IT TESTS: synthesises a minimal but protocol-correct C64 GCR
 * track holding two sectors of 256 bytes each (the 1541's native
 * sector size), converts the bitstream into a flux transition stream,
 * and runs flux_decode_gcr_c64(). The decoder must:
 *   - locate the 10-consecutive-1 sync runs that precede every block,
 *   - decode the 8-byte header (marker 0x08, checksum, sector, track,
 *     id2, id1, 0x0F, 0x0F),
 *   - decode the data block (marker 0x07, 256 data bytes, checksum),
 *   - return exactly two sectors with byte-exact data, both CRCs OK.
 *
 * Encoding: C64 GCR maps each 4-bit nibble to a 5-bit codeword via
 * the inverse of the decoder's c64_gcr_decode[32] table. Each output
 * byte is two GCR codewords back-to-back (10 bits). A 1 in the
 * resulting bitstream becomes a flux transition at that cell; a 0
 * becomes no transition. Sync gaps are runs of 1-bits (≥ 10 ones).
 *
 * Self-contained: synthesises its own bitstream, no corpus file
 * needed. Same CHECK-style framework as test_flux_mfm_sync.c so the
 * suite builds under -DNDEBUG.
 *
 * Scope: C64-GCR sector decode only. Speed-zone selection, half-track
 * tricks, and >35-track territory are out of scope.
 */
#include "uft/flux/uft_flux_decoder.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* C64 zone-0 cell time = 3.25 µs; the decoder defaults to 4000 ns
 * which works for synthetic input where every cell carries a clean
 * transition. Using the decoder's own default avoids any zone-table
 * coupling in the test. */
#define BITCELL_NS  4000u

/* Inverse of c64_gcr_decode[32] in src/flux/uft_flux_decoder.c.
 * For each 4-bit nibble (index 0..15) gives the 5-bit GCR codeword. */
static const uint8_t c64_gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* A growable bit-array of CELLS (each cell is 1 = flux transition,
 * 0 = none). The decoder's flux_to_bitstream() will reproduce these
 * 1-of-N cell positions verbatim from the synthesised flux stream. */
typedef struct { uint8_t *b; size_t n_bits, cap_bytes; } bitvec_t;

static void bv_push_bit(bitvec_t *v, int bit) {
    size_t byte_idx = v->n_bits / 8;
    if (byte_idx >= v->cap_bytes) {
        size_t new_cap = v->cap_bytes ? v->cap_bytes * 2 : 256;
        v->b = realloc(v->b, new_cap);
        memset(v->b + v->cap_bytes, 0, new_cap - v->cap_bytes);
        v->cap_bytes = new_cap;
    }
    if (bit) v->b[byte_idx] |= (uint8_t)(1u << (7 - (v->n_bits % 8)));
    v->n_bits++;
}

/* Emit one byte as two GCR codewords (10 cells), MSB-first. */
static void bv_push_gcr_byte(bitvec_t *v, uint8_t byte) {
    uint8_t hi = c64_gcr_encode[(byte >> 4) & 0x0F];
    uint8_t lo = c64_gcr_encode[byte & 0x0F];
    for (int i = 4; i >= 0; --i) bv_push_bit(v, (hi >> i) & 1);
    for (int i = 4; i >= 0; --i) bv_push_bit(v, (lo >> i) & 1);
}

/* Sync run = at least 10 consecutive 1-cells. The 1541 writes 40+ for
 * a strong run; 16 is plenty for the decoder. */
static void bv_push_sync(bitvec_t *v) {
    for (int i = 0; i < 16; ++i) bv_push_bit(v, 1);
}

/* Inter-block gap. Real 1541 gaps are zero cells, but flux_to_bitstream
 * caps inter-transition gaps at 8 cells (anti-glitch sanity limit),
 * which would crush long zero runs and shift downstream positions —
 * specifically, a synthesised flux stream with no transition after the
 * final data-checksum byte runs out of bits[] mid-cksum-byte. An
 * alternating 0/1 fill keeps a transition every two cells without
 * triggering the ≥10-consecutive-ones sync detector, so block positions
 * stay exact. */
static void bv_push_gap(bitvec_t *v, size_t cells) {
    for (size_t i = 0; i < cells; ++i) bv_push_bit(v, (int)(i & 1));
}

/* Convert cell bit-array into a cumulative flux transition stream.
 * Each cell is BITCELL_NS apart; a 1-cell produces a transition at
 * its cumulative timestamp. */
static flux_raw_data_t cells_to_flux(const bitvec_t *v, uint32_t **out_trans) {
    uint32_t *tr = malloc(v->n_bits * sizeof(uint32_t));
    size_t n = 0;
    uint64_t cum = 0;
    for (size_t i = 0; i < v->n_bits; ++i) {
        cum += BITCELL_NS;
        int bit = (v->b[i / 8] >> (7 - (i % 8))) & 1;
        if (bit) tr[n++] = (uint32_t)cum;
    }
    *out_trans = tr;
    flux_raw_data_t f;
    memset(&f, 0, sizeof(f));
    f.transitions      = tr;
    f.transition_count = n;
    f.sample_rate      = 1000000000u;        /* 1 GHz → 1 tick = 1 ns */
    return f;
}

/* Build one C64 1541 sector: sync + header block + sync + data block.
 *
 * The 1541 protocol writes a header block (marker 0x08, hdr checksum,
 * sector, track-1-based, id2, id1, 0x0F, 0x0F) followed by a short
 * gap and the data block (marker 0x07, 256 bytes, XOR checksum). The
 * decoder's c64_find_sync() locates each block via its leading
 * ≥10-bit run of 1s. */
static void emit_sector(bitvec_t *v,
                        uint8_t cyl_1based, uint8_t sec,
                        const uint8_t *data /*256 B*/) {
    /* --- header block --- */
    bv_push_sync(v);
    uint8_t hdr_payload[4] = { sec, cyl_1based, 'X', 'Y' }; /* id2='X', id1='Y' */
    uint8_t hdr_chk = (uint8_t)(hdr_payload[0] ^ hdr_payload[1] ^
                                hdr_payload[2] ^ hdr_payload[3]);
    bv_push_gcr_byte(v, 0x08);              /* header marker */
    bv_push_gcr_byte(v, hdr_chk);
    bv_push_gcr_byte(v, hdr_payload[0]);    /* sector */
    bv_push_gcr_byte(v, hdr_payload[1]);    /* track (1-based) */
    bv_push_gcr_byte(v, hdr_payload[2]);    /* id2 */
    bv_push_gcr_byte(v, hdr_payload[3]);    /* id1 */
    bv_push_gcr_byte(v, 0x0F);              /* trailing pad bytes */
    bv_push_gcr_byte(v, 0x0F);

    bv_push_gap(v, 80);                     /* inter-block gap */

    /* --- data block --- */
    bv_push_sync(v);
    bv_push_gcr_byte(v, 0x07);              /* data marker */
    uint8_t chk = 0;
    for (int i = 0; i < 256; ++i) { chk ^= data[i]; bv_push_gcr_byte(v, data[i]); }
    bv_push_gcr_byte(v, chk);               /* data checksum */

    bv_push_gap(v, 80);
}

/* ── the regression test ──────────────────────────────────────────── */
TEST(decodes_two_c64_gcr_sectors) {
    uint8_t s1[256], s5[256];
    /* Avalanche-style fill (not periodic) — a wrong sector-to-byte
     * mapping would otherwise survive byte-equality. */
    for (int i = 0; i < 256; ++i) {
        s1[i] = (uint8_t)((i * 17 + 3) ^ (i >> 3));
        s5[i] = (uint8_t)((i *  7 + 41) ^ (i >> 2));
    }

    bitvec_t v = {0};
    bv_push_gap(&v, 200);                                  /* leading gap */
    emit_sector(&v, /*cyl_1based*/ 18, /*sec*/ 1, s1);     /* track 17 */
    emit_sector(&v, /*cyl_1based*/ 18, /*sec*/ 5, s5);     /* track 17 */
    bv_push_gap(&v, 100);                                  /* trailing gap */

    uint32_t *tr = NULL;
    flux_raw_data_t flux = cells_to_flux(&v, &tr);
    ASSERT(flux.transition_count > 0);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding   = FLUX_ENC_GCR_C64;
    opts.bitcell_ns = BITCELL_NS;
    /* Disable adaptive PLL for the synthesis-driven test: a deterministic
     * fixed-period sampler matches the cell timing we emit (every cell is
     * exactly BITCELL_NS ns), so the bit_count matches the cell count
     * exactly. With the PLL active, phase drift across the 5000+ cells
     * per sector accumulates enough to shift the trailing data-block
     * checksum read by a fraction of a cell, decoding it as 0xFF.
     * The PLL is exercised by the differential corpus (real flux); this
     * unit test isolates the symbol-level decode contract. */
    opts.use_pll    = false;

    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_status_t st = flux_decode_gcr_c64(&flux, &dt, &opts);

    ASSERT(st == FLUX_OK);
    ASSERT(dt.sector_count == 2);

    /* The decoder converts 1-based 1541 tracks to 0-based cylinders:
     * track 18 on the wire → cylinder 17 in the decoded struct. */
    ASSERT(dt.sectors[0].cylinder == 17);
    ASSERT(dt.sectors[0].sector   == 1);
    ASSERT(dt.sectors[1].cylinder == 17);
    ASSERT(dt.sectors[1].sector   == 5);

    ASSERT(dt.sectors[0].data && dt.sectors[0].data_size == 256);
    ASSERT(memcmp(dt.sectors[0].data, s1, 256) == 0);
    ASSERT(dt.sectors[1].data && dt.sectors[1].data_size == 256);
    ASSERT(memcmp(dt.sectors[1].data, s5, 256) == 0);

    /* Both checksums were synthesised with the decoder's own algebra
     * (XOR of payload bytes for the data block, XOR of CHS+IDs for the
     * header), so the decoder must report both OK. */
    ASSERT(dt.sectors[0].id_crc_ok && dt.sectors[0].data_crc_ok);
    ASSERT(dt.sectors[1].id_crc_ok && dt.sectors[1].data_crc_ok);

    flux_decoded_track_free(&dt);
    free(tr);
    free(v.b);
}

int main(void) {
    printf("=== C64 GCR decoder unit test (audit Lücke #1) ===\n");
    RUN(decodes_two_c64_gcr_sectors);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
