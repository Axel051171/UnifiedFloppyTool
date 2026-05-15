/**
 * @file test_flux_amiga.c
 * @brief Hardware-independent unit test for flux_decode_amiga.
 *
 * MOTIVATION (audit/test_coverage/COVERAGE_AUDIT.md Lücke #1): Amiga
 * MFM has no dedicated C-unit test — only the differential conformance
 * harness (skips when `gw` is absent). This test mirrors
 * test_flux_mfm_sync.c / test_flux_gcr_c64.c / test_flux_gcr_apple.c
 * for the Amiga decode path (MF-225).
 *
 * WHAT IT TESTS: synthesises a minimal AmigaDOS trackdisk track holding
 * two 512-byte sectors, converts to a flux transition stream, and runs
 * flux_decode_amiga(). The decoder must:
 *   - locate two 0x4489 sync words per sector and skip them,
 *   - read the 4-byte info long (0xFF, track, sector, sectors-to-gap)
 *     in the Amiga odd/even-split scheme,
 *   - read the 16-byte label and the header/data checksums,
 *   - read the 512-byte data field,
 *   - return both sectors with byte-exact data and both AmigaDOS
 *     checksums (XOR of long-packed raw bytes, masked 0x55555555) OK.
 *
 * Encoding (inverse of src/flux/uft_flux_decoder.c amiga_read_field):
 *
 *   Each output byte is encoded as 16 cells = 8 odd-half cells + 8
 *   even-half cells. Within each 8-cell half, four data bits live at
 *   cell offsets 1, 3, 5, 7 (the `0x55` positions the decoder masks),
 *   and four clock bits at offsets 0, 2, 4, 6 (ignored by the
 *   decoder; populated with standard MFM clocking — 1 iff both
 *   adjacent data bits are 0 — to keep cell zero-runs ≤ 3 for the
 *   PLL). The odd half carries the data byte's bits 7,5,3,1 (in cell
 *   order); the even half carries 6,4,2,0.
 *
 *   Per field, all `nbytes` odd halves come first, then all `nbytes`
 *   even halves — matching amiga_read_field's two-pass reader.
 *
 *   Header checksum = XOR of (info-field-long, label-field-longs),
 *   where each long packs four consecutive raw bytes big-endian. The
 *   decoder masks with 0x55555555 before comparison, so the checksum
 *   reduces to an XOR over the DATA-bit portions of the raw bytes —
 *   independent of the clock-bit choice. Same for the data checksum
 *   over the 512-byte data field.
 *
 * Self-contained: synthesises its own bit-cell stream, no corpus
 * needed. Same CHECK-style harness as test_flux_mfm_sync.c.
 *
 * Scope: AmigaDOS DD sector decode only. Custom protections (Rob
 * Northen, Copylock), HD trackdisk, and AmigaDOS root-block file-
 * system semantics are out.
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

#define BITCELL_NS  2000u            /* Amiga DD = 2 µs/cell */
#define AMIGA_SECTOR_BYTES  512
#define AMIGA_CKSUM_MASK    0x55555555u

typedef struct { uint8_t *b; size_t n_bits, cap_bytes; } bitvec_t;

static void bv_push_bit(bitvec_t *v, int bit) {
    size_t byte_idx = v->n_bits / 8;
    if (byte_idx >= v->cap_bytes) {
        size_t new_cap = v->cap_bytes ? v->cap_bytes * 2 : 4096;
        v->b = realloc(v->b, new_cap);
        memset(v->b + v->cap_bytes, 0, new_cap - v->cap_bytes);
        v->cap_bytes = new_cap;
    }
    if (bit) v->b[byte_idx] |= (uint8_t)(1u << (7 - (v->n_bits % 8)));
    v->n_bits++;
}

/* Emit a 16-bit value MSB-first, taken as raw cells (no MFM clock
 * insertion). Used for the 0x4489 sync words, which ARE the recognised
 * cell pattern. */
static void bv_push_word16(bitvec_t *v, uint16_t w) {
    for (int i = 15; i >= 0; --i) bv_push_bit(v, (w >> i) & 1);
}

/* Emit 8 cells for one "raw half-byte": clock+data interleaved with
 * MFM clocking. data[0..3] sit at cell offsets 1, 3, 5, 7. Clocks at
 * 0, 2, 4, 6 are 1 iff both adjacent data bits are 0 (standard MFM
 * rule), which keeps the cell zero-run ≤ 3. `prev_data` is updated to
 * data[3] so the next half-byte's leading clock continues the chain. */
static void emit_half_byte(bitvec_t *v, int *prev_data,
                           int d0, int d1, int d2, int d3) {
    bv_push_bit(v, (*prev_data == 0 && d0 == 0) ? 1 : 0);  /* cell 0 */
    bv_push_bit(v, d0);                                     /* cell 1 */
    bv_push_bit(v, (d0 == 0 && d1 == 0) ? 1 : 0);          /* cell 2 */
    bv_push_bit(v, d1);                                     /* cell 3 */
    bv_push_bit(v, (d1 == 0 && d2 == 0) ? 1 : 0);          /* cell 4 */
    bv_push_bit(v, d2);                                     /* cell 5 */
    bv_push_bit(v, (d2 == 0 && d3 == 0) ? 1 : 0);          /* cell 6 */
    bv_push_bit(v, d3);                                     /* cell 7 */
    *prev_data = d3;
}

/* Emit one field of `nbytes` output bytes in Amiga odd/even-split
 * layout: all odd halves first, then all even halves. */
static void emit_field(bitvec_t *v, int *prev_data,
                       const uint8_t *out, size_t nbytes) {
    for (size_t j = 0; j < nbytes; ++j) {
        /* Odd half: data bits 7, 5, 3, 1 (in cell order). */
        emit_half_byte(v, prev_data,
                       (out[j] >> 7) & 1, (out[j] >> 5) & 1,
                       (out[j] >> 3) & 1, (out[j] >> 1) & 1);
    }
    for (size_t j = 0; j < nbytes; ++j) {
        /* Even half: data bits 6, 4, 2, 0. */
        emit_half_byte(v, prev_data,
                       (out[j] >> 6) & 1, (out[j] >> 4) & 1,
                       (out[j] >> 2) & 1,  out[j]       & 1);
    }
}

/* Compute the data-bit-only checksum contribution of a field as the
 * decoder would see it after masking with 0x55555555.
 *
 * The decoder XORs raw bytes packed big-endian into 32-bit longs; the
 * mask kills clock bits at the 0xAA positions, so any choice of clock
 * gives the same final csum. We can therefore compute the contribution
 * using rb_data only (rb's 0x55-bit slice), which is purely a function
 * of the output bytes. */
static uint32_t cksum_data(const uint8_t *out, size_t nbytes) {
    uint32_t csum = 0;
    for (int half = 0; half < 2; ++half) {
        uint32_t acc = 0;
        int acc_n = 0;
        for (size_t j = 0; j < nbytes; ++j) {
            uint8_t rb_data = (half == 0)
                ? (uint8_t)((out[j] >> 1) & 0x55u)
                : (uint8_t)( out[j]       & 0x55u);
            acc = (acc << 8) | rb_data;
            if (++acc_n == 4) { csum ^= acc; acc = 0; acc_n = 0; }
        }
        if (acc_n) {
            acc <<= 8u * (4 - acc_n);
            csum ^= acc;
        }
    }
    return csum & AMIGA_CKSUM_MASK;
}

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
    f.sample_rate      = 1000000000u;
    return f;
}

/* Inter-sector gap: alternating cells so flux_to_bitstream's
 * 8-cell-gap clamp never bites, and so no accidental 0x4489 forms in
 * the cell stream. Reset prev_data after the gap (we don't track the
 * gap through MFM clocking). */
static void emit_gap(bitvec_t *v, size_t cells) {
    for (size_t i = 0; i < cells; ++i) bv_push_bit(v, (int)(i & 1));
}

/* Emit one Amiga sector: two 0x4489 sync words + info+label+hchk+dchk
 * + data field. */
static void emit_amiga_sector(bitvec_t *v,
                              uint8_t track_0_159, uint8_t sec_0_10,
                              const uint8_t *data /*512 B*/) {
    int prev_data = 0;
    bv_push_word16(v, 0x4489);
    bv_push_word16(v, 0x4489);
    /* After the sync run, prev_data = LSB of 0x4489 = 1. */
    prev_data = 1;

    uint8_t info[4]   = { 0xFF, track_0_159, sec_0_10, 0 };
    uint8_t label[16] = {0};                                  /* OS-recovery */
    for (int i = 0; i < 16; ++i) label[i] = (uint8_t)(0x90 + i);

    uint32_t hdr_csum = cksum_data(info, 4) ^ cksum_data(label, 16);
    uint8_t hchk[4] = {
        (uint8_t)(hdr_csum >> 24), (uint8_t)(hdr_csum >> 16),
        (uint8_t)(hdr_csum >>  8), (uint8_t) hdr_csum
    };
    uint32_t data_cs = cksum_data(data, AMIGA_SECTOR_BYTES);
    uint8_t dchk[4] = {
        (uint8_t)(data_cs >> 24), (uint8_t)(data_cs >> 16),
        (uint8_t)(data_cs >>  8), (uint8_t) data_cs
    };

    emit_field(v, &prev_data, info,  4);
    emit_field(v, &prev_data, label, 16);
    emit_field(v, &prev_data, hchk,  4);
    emit_field(v, &prev_data, dchk,  4);
    emit_field(v, &prev_data, data,  AMIGA_SECTOR_BYTES);
}

/* ── the regression test ──────────────────────────────────────────── */
TEST(decodes_two_amiga_sectors) {
    uint8_t s3[AMIGA_SECTOR_BYTES], s7[AMIGA_SECTOR_BYTES];
    for (int i = 0; i < AMIGA_SECTOR_BYTES; ++i) {
        s3[i] = (uint8_t)((i * 23 + 11) ^ (i >> 4));
        s7[i] = (uint8_t)((i * 19 +  7) ^ (i >> 5));
    }

    bitvec_t v = {0};
    emit_gap(&v, 64);                                    /* leading gap */
    /* track 4 = cylinder 2, head 0. Sectors 3 and 7. */
    emit_amiga_sector(&v, /*track*/4, /*sec*/3, s3);
    emit_gap(&v, 64);
    emit_amiga_sector(&v, /*track*/4, /*sec*/7, s7);
    emit_gap(&v, 64);

    uint32_t *tr = NULL;
    flux_raw_data_t flux = cells_to_flux(&v, &tr);
    ASSERT(flux.transition_count > 0);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding   = FLUX_ENC_AMIGA;
    opts.bitcell_ns = BITCELL_NS;
    opts.use_pll    = false;        /* synthesis is exact; see C64 test */

    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_status_t st = flux_decode_amiga(&flux, &dt, &opts);

    ASSERT(st == FLUX_OK);
    ASSERT(dt.sector_count == 2);

    /* track 4 → cylinder 2, head 0. */
    ASSERT(dt.sectors[0].cylinder == 2 && dt.sectors[0].head == 0);
    ASSERT(dt.sectors[1].cylinder == 2 && dt.sectors[1].head == 0);
    ASSERT(dt.sectors[0].sector == 3);
    ASSERT(dt.sectors[1].sector == 7);

    ASSERT(dt.sectors[0].data && dt.sectors[0].data_size == AMIGA_SECTOR_BYTES);
    ASSERT(memcmp(dt.sectors[0].data, s3, AMIGA_SECTOR_BYTES) == 0);
    ASSERT(dt.sectors[1].data && dt.sectors[1].data_size == AMIGA_SECTOR_BYTES);
    ASSERT(memcmp(dt.sectors[1].data, s7, AMIGA_SECTOR_BYTES) == 0);

    ASSERT(dt.sectors[0].id_crc_ok && dt.sectors[0].data_crc_ok);
    ASSERT(dt.sectors[1].id_crc_ok && dt.sectors[1].data_crc_ok);

    flux_decoded_track_free(&dt);
    free(tr);
    free(v.b);
}

int main(void) {
    printf("=== Amiga MFM decoder unit test (audit Lücke #1) ===\n");
    RUN(decodes_two_amiga_sectors);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
