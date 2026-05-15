/**
 * @file test_flux_gcr_apple_sync.c
 * @brief Hardware-independent unit test for flux_decode_gcr_apple.
 *
 * MOTIVATION (audit/test_coverage/COVERAGE_AUDIT.md Lücke #1): Apple ][
 * 6-and-2 GCR has no dedicated C-unit test — only the differential
 * conformance harness (which skips when greaseweazle's `gw` is absent).
 * This test mirrors test_flux_mfm_sync.c / test_flux_gcr_c64.c for the
 * Apple decode path.
 *
 * WHAT IT TESTS: synthesises a minimal 16-sector-style Apple DOS 3.3
 * track holding two 256-byte sectors, converts to a flux transition
 * stream, and runs flux_decode_gcr_apple(). The decoder must:
 *   - locate each D5 AA 96 address prologue,
 *   - decode the 4-and-4-encoded (volume, track, sector, cksum) header,
 *   - locate the D5 AA AD data prologue,
 *   - reverse the 343-byte 6-and-2 disk encoding (running XOR + low-2
 *     bit aux buffer + bit-reversed pair fix-up) into 256 bytes,
 *   - return both sectors with byte-exact data and both CRCs OK.
 *
 * Encoding rules used here (inverses of src/flux/uft_flux_decoder.c):
 *
 *   4-and-4   v → b1 = (v >> 1) | 0xAA, b2 = v | 0xAA
 *             decoder: ((b1 << 1) | 1) & b2  →  v
 *
 *   6-and-2   the 256 data bytes split as: 86 bytes of "low-2-bit
 *             aux" (each holding the bit-reversed 2-bit groups of
 *             output bytes i, i+86, i+172) + 256 bytes of high-6-bits.
 *             The 342-entry decoded array is then XOR-chained: the
 *             on-disk byte at offset i is `apple_gcr62_encode[
 *             decoded[i] ^ decoded[i-1] ]` with decoded[-1]=0. The
 *             343rd disk byte is `encode[ decoded[341] ]`.
 *
 * Self-contained: synthesises its own bit-cell stream, no corpus file
 * needed. Same CHECK-style harness as test_flux_mfm_sync.c.
 *
 * Scope: 16-sector Apple DOS 3.3 GCR sector decode. Apple 5-and-3
 * (13-sector), copy protection, and ProDOS-order remapping are out.
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

#define BITCELL_NS  4000u

/* 6-and-2 disk-byte alphabet: 64 entries mapping a 6-bit value
 * (0x00..0x3F) to its on-disk 8-bit encoding. The inverse of
 * apple_gcr62_decode[256] in src/flux/uft_flux_decoder.c. */
static const uint8_t apple_gcr62_encode[64] = {
    0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xCB,0xCD,0xCE,0xCF,
    0xD3,0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
    0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
    0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

/* Cell-stream synthesis: each on-disk byte is 8 cells (the Apple
 * decoder reads 8 cells per disk byte directly). The 6-and-2 alphabet
 * guarantees each disk byte has the high bit set AND no two consecutive
 * zero cells, so the PLL has plenty of transitions throughout. */
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

/* Push one on-disk byte = 8 cells, MSB-first. */
static void bv_push_disk_byte(bitvec_t *v, uint8_t b) {
    for (int i = 7; i >= 0; --i) bv_push_bit(v, (b >> i) & 1);
}

/* Gap pattern. Apple gaps are normally 0xFF self-sync bytes, but for
 * unit-testing purposes ANY filler with regular transitions works as
 * long as it does not contain a D5 AA 96 / AD prologue. We use 0xFF
 * (decoded value 0x3F) — a real-Apple-style gap. */
static void bv_push_gap(bitvec_t *v, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) bv_push_disk_byte(v, 0xFF);
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

/* Encode one byte 4-and-4. Each pair of disk bytes carries 8 data
 * bits, the first holding odd-position bits, the second even. */
static void bv_push_4_and_4(bitvec_t *v, uint8_t value) {
    bv_push_disk_byte(v, (uint8_t)((value >> 1) | 0xAAu));
    bv_push_disk_byte(v, (uint8_t)( value       | 0xAAu));
}

/* Build one 16-sector Apple DOS 3.3 sector: gap, D5 AA 96 address
 * prologue, 4-and-4 (vol/track/sector/cksum), gap, D5 AA AD data
 * prologue, 343-byte 6-and-2 data field. Both epilogues (DE AA EB)
 * are emitted for realism but the decoder does not require them. */
static void emit_sector(bitvec_t *v,
                        uint8_t volume, uint8_t track,
                        uint8_t sec, const uint8_t *data /*256 B*/) {
    bv_push_gap(v, 10);                       /* leading self-sync */

    /* --- address field --- */
    bv_push_disk_byte(v, 0xD5);
    bv_push_disk_byte(v, 0xAA);
    bv_push_disk_byte(v, 0x96);
    bv_push_4_and_4(v, volume);
    bv_push_4_and_4(v, track);
    bv_push_4_and_4(v, sec);
    bv_push_4_and_4(v, (uint8_t)(volume ^ track ^ sec));
    bv_push_disk_byte(v, 0xDE); bv_push_disk_byte(v, 0xAA);  /* epilogue */
    bv_push_disk_byte(v, 0xEB);

    bv_push_gap(v, 5);                        /* inter-field gap */

    /* --- data field --- */
    bv_push_disk_byte(v, 0xD5);
    bv_push_disk_byte(v, 0xAA);
    bv_push_disk_byte(v, 0xAD);

    /* Pack 256 user bytes into 342 6-bit values:
     *   aux[0..85]  holds bit-reversed 2-bit pairs for bytes
     *               (i, i+86, i+172) at bit-offsets (0,2,4)
     *   hi [0..255] holds the high 6 bits of byte i  → decoded[86+i]
     *
     * The decoder reverses the LOW-bit pair (bit0 ↔ bit1) when
     * reassembling — so the synthesis must store the reversed pair to
     * survive the round-trip. */
    uint8_t decoded[342];
    memset(decoded, 0, sizeof(decoded));
    for (int i = 0; i < 256; ++i) {
        uint8_t lo2     = data[i] & 0x03;
        uint8_t lo2_rev = (uint8_t)(((lo2 & 1) << 1) | ((lo2 >> 1) & 1));
        int aux_idx    = i % 86;
        int shift      = (i / 86) * 2;        /* 0, 2, 4 */
        decoded[aux_idx] |= (uint8_t)(lo2_rev << shift);
        decoded[86 + i]   = (uint8_t)(data[i] >> 2);
    }
    /* XOR-chain encoding: disk[i] = encode[ decoded[i] ^ decoded[i-1] ],
     * with decoded[-1] = 0. */
    uint8_t prev = 0;
    for (int i = 0; i < 342; ++i) {
        uint8_t v6 = (uint8_t)(decoded[i] ^ prev);
        bv_push_disk_byte(v, apple_gcr62_encode[v6 & 0x3F]);
        prev = decoded[i];
    }
    /* Trailing checksum disk byte = encode[ decoded[341] ]. */
    bv_push_disk_byte(v, apple_gcr62_encode[decoded[341] & 0x3F]);

    bv_push_disk_byte(v, 0xDE); bv_push_disk_byte(v, 0xAA);
    bv_push_disk_byte(v, 0xEB);
}

/* ── the regression test ──────────────────────────────────────────── */
TEST(decodes_two_apple_gcr_sectors) {
    uint8_t s2[256], s9[256];
    for (int i = 0; i < 256; ++i) {
        /* Cover all low-2-bit values (0..3) so the bit-reversed pair
         * fix-up is exercised on every quadrant. */
        s2[i] = (uint8_t)((i * 13 + 5) ^ (i << 1));
        s9[i] = (uint8_t)((i * 11 + 19) ^ (i >> 1));
    }

    bitvec_t v = {0};
    bv_push_gap(&v, 20);                                /* track start */
    emit_sector(&v, /*vol*/254, /*track*/17, /*sec*/ 2, s2);
    emit_sector(&v, /*vol*/254, /*track*/17, /*sec*/ 9, s9);
    bv_push_gap(&v, 20);                                /* track end */

    uint32_t *tr = NULL;
    flux_raw_data_t flux = cells_to_flux(&v, &tr);
    ASSERT(flux.transition_count > 0);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding   = FLUX_ENC_GCR_APPLE;
    opts.bitcell_ns = BITCELL_NS;
    opts.use_pll    = false;        /* see test_flux_gcr_c64.c rationale */

    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_status_t st = flux_decode_gcr_apple(&flux, &dt, &opts);

    ASSERT(st == FLUX_OK);
    ASSERT(dt.sector_count == 2);

    ASSERT(dt.sectors[0].cylinder == 17 && dt.sectors[0].sector == 2);
    ASSERT(dt.sectors[1].cylinder == 17 && dt.sectors[1].sector == 9);

    ASSERT(dt.sectors[0].data && dt.sectors[0].data_size == 256);
    ASSERT(memcmp(dt.sectors[0].data, s2, 256) == 0);
    ASSERT(dt.sectors[1].data && dt.sectors[1].data_size == 256);
    ASSERT(memcmp(dt.sectors[1].data, s9, 256) == 0);

    ASSERT(dt.sectors[0].id_crc_ok && dt.sectors[0].data_crc_ok);
    ASSERT(dt.sectors[1].id_crc_ok && dt.sectors[1].data_crc_ok);

    flux_decoded_track_free(&dt);
    free(tr);
    free(v.b);
}

int main(void) {
    printf("=== Apple ][ 6-and-2 GCR decoder unit test (audit Lücke #1) ===\n");
    RUN(decodes_two_apple_gcr_sectors);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
