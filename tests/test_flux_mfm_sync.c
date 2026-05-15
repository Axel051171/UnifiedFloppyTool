/**
 * @file test_flux_mfm_sync.c
 * @brief Regression test for the IBM-MFM A1-sync-run bug (MF-218).
 *
 * THE BUG: decode_mfm_sector() in src/flux/uft_flux_decoder.c skipped
 * exactly ONE 0x4489 sync word after flux_find_sync() located a sync
 * group. IBM System-34 MFM precedes every address mark with THREE
 * 0xA1 sync bytes (A1 A1 A1). After skipping one, the decoder read
 * the SECOND A1 instead of the address mark, saw 0xA1 != 0xFE, and
 * bailed with FLUX_ERR_NO_SYNC — so a perfectly good gw-produced
 * IBM-DD flux stream decoded to ZERO sectors. The differential
 * conformance harness (task #109) surfaced it on its first real file.
 *
 * THE TEST: synthesise a minimal but protocol-correct IBM-MFM track
 * (gap + A1 A1 A1 + IDAM + C/H/S/N + CRC + gap + A1 A1 A1 + DAM +
 * data + CRC) for two sectors, convert it to a flux transition
 * stream, and run flux_decode_mfm(). Pre-fix this yields 0 sectors;
 * post-fix it yields exactly 2, with the right CHS and byte-exact
 * data. The "exactly 2, not more" assertion also guards the
 * companion fix: the scan must resume PAST a decoded sector, not
 * inside its own A1 run (which re-decoded each sector 3+ times).
 *
 * Self-contained: synthesises its own flux, no corpus file needed.
 * Real CHECK-style macros — the suite builds with -DNDEBUG.
 *
 * Scope (per the #109 sub-task brief): IBM-DD MFM sync only. AtariST
 * edge cases and HD encoding are explicitly out of scope here.
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

#define BITCELL_NS 2000u            /* DD MFM cell */
#define A1_WORD    0x4489u          /* the 0xA1 missing-clock sync word */

/* A growable 16-bit MFM-word track being synthesised. */
typedef struct { uint16_t *w; size_t n, cap; } track_t;

static void emit(track_t *t, uint16_t word) {
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 256;
        t->w = realloc(t->w, t->cap * sizeof(uint16_t));
    }
    t->w[t->n++] = word;
}

/* Encode one data byte to its 16-bit MFM word, MSB-first, tracking the
 * inter-byte clock rule via prev_bit. */
static void emit_byte(track_t *t, uint8_t b, int *prev_bit) {
    uint16_t w = 0;
    int prev = *prev_bit;
    for (int i = 7; i >= 0; --i) {
        int data = (b >> i) & 1;
        /* MFM clock bit = 1 only when both the previous and current
         * data bits are 0. */
        int clock = (!prev && !data) ? 1 : 0;
        w = (uint16_t)((w << 2) | (clock << 1) | data);
        prev = data;
    }
    *prev_bit = prev;
    emit(t, w);
}

/* CRC over a byte run, using the decoder's own flux_crc16_mfm() so the
 * synthesised CRC matches what decode_mfm_sector() will check. */
static void emit_crc(track_t *t, const uint8_t *data, size_t len, int *prev_bit) {
    uint16_t crc = flux_crc16_mfm(data, len);
    emit_byte(t, (uint8_t)(crc >> 8), prev_bit);
    emit_byte(t, (uint8_t)(crc & 0xFF), prev_bit);
}

/* Append one full IBM-MFM sector: gap + A1 A1 A1 + IDAM + CHSN + CRC
 * + gap + A1 A1 A1 + DAM + data + CRC. */
static void emit_sector(track_t *t, uint8_t cyl, uint8_t head, uint8_t sec,
                        const uint8_t *data, size_t data_len) {
    int prev = 0;
    for (int i = 0; i < 12; ++i) emit_byte(t, 0x00, &prev);   /* sync gap */

    /* --- ID field --- */
    prev = 0;
    emit(t, A1_WORD); emit(t, A1_WORD); emit(t, A1_WORD);
    uint8_t id[5] = { 0xFE, cyl, head, sec, 0x02 /*N: 512*/ };
    for (int i = 0; i < 5; ++i) emit_byte(t, id[i], &prev);
    emit_crc(t, id, 5, &prev);

    prev = 0;
    for (int i = 0; i < 22; ++i) emit_byte(t, 0x00, &prev);   /* gap2 */

    /* --- data field --- */
    prev = 0;
    emit(t, A1_WORD); emit(t, A1_WORD); emit(t, A1_WORD);
    uint8_t *dblk = malloc(1 + data_len);
    dblk[0] = 0xFB;                                           /* DAM */
    memcpy(dblk + 1, data, data_len);
    emit_byte(t, 0xFB, &prev);
    for (size_t i = 0; i < data_len; ++i) emit_byte(t, data[i], &prev);
    emit_crc(t, dblk, 1 + data_len, &prev);
    free(dblk);

    prev = 0;
    for (int i = 0; i < 12; ++i) emit_byte(t, 0x00, &prev);   /* trailing gap */
}

/* Turn the 16-bit-word track into a cumulative flux transition stream:
 * each `1` cell is a transition, cells are BITCELL_NS apart. */
static flux_raw_data_t track_to_flux(const track_t *t, uint32_t **out_trans) {
    size_t total_cells = t->n * 16;
    uint32_t *tr = malloc(total_cells * sizeof(uint32_t));
    size_t n = 0;
    uint64_t cum = 0;
    for (size_t wi = 0; wi < t->n; ++wi) {
        for (int b = 15; b >= 0; --b) {
            cum += BITCELL_NS;
            if ((t->w[wi] >> b) & 1) tr[n++] = (uint32_t)cum;
        }
    }
    *out_trans = tr;
    flux_raw_data_t f;
    memset(&f, 0, sizeof(f));
    f.transitions      = tr;
    f.transition_count = n;
    f.sample_rate      = 1000000000u;   /* 1 GHz -> 1 tick = 1 ns */
    return f;
}

/* ── the regression test ──────────────────────────────────────────── */
TEST(decodes_ibm_mfm_track_with_a1x3_sync) {
    uint8_t s0[512], s1[512];
    for (int i = 0; i < 512; ++i) { s0[i] = (uint8_t)(i & 0xFF);
                                    s1[i] = (uint8_t)((i * 7 + 3) & 0xFF); }

    track_t t = {0};
    emit_sector(&t, /*cyl*/3, /*head*/1, /*sec*/1, s0, 512);
    emit_sector(&t, /*cyl*/3, /*head*/1, /*sec*/2, s1, 512);

    uint32_t *tr = NULL;
    flux_raw_data_t flux = track_to_flux(&t, &tr);
    ASSERT(flux.transition_count > 0);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding   = FLUX_ENC_MFM;
    opts.bitcell_ns = BITCELL_NS;
    opts.use_pll    = true;

    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_status_t st = flux_decode_mfm(&flux, &dt, &opts);

    /* Pre-MF-218 this was FLUX_ERR_NO_SYNC with sector_count 0 — the
     * decoder skipped only one A1 of the A1 A1 A1 run. */
    ASSERT(st == FLUX_OK);
    /* Exactly two — not zero (the sync bug) and not six (the
     * re-decode-the-same-sector duplicate bug the companion fix
     * closed). */
    ASSERT(dt.sector_count == 2);

    ASSERT(dt.sectors[0].cylinder == 3 && dt.sectors[0].head == 1 &&
           dt.sectors[0].sector == 1);
    ASSERT(dt.sectors[1].cylinder == 3 && dt.sectors[1].head == 1 &&
           dt.sectors[1].sector == 2);

    ASSERT(dt.sectors[0].data && dt.sectors[0].data_size == 512);
    ASSERT(memcmp(dt.sectors[0].data, s0, 512) == 0);
    ASSERT(dt.sectors[1].data && dt.sectors[1].data_size == 512);
    ASSERT(memcmp(dt.sectors[1].data, s1, 512) == 0);

    /* CRCs were synthesised with the decoder's own flux_crc16_mfm(), so
     * the decoder must agree they verify. */
    ASSERT(dt.sectors[0].id_crc_ok && dt.sectors[0].data_crc_ok);
    ASSERT(dt.sectors[1].id_crc_ok && dt.sectors[1].data_crc_ok);

    flux_decoded_track_free(&dt);
    free(tr);
    free(t.w);
}

/* The sync-run skipper must also tolerate a single-A1 prefix (Amiga /
 * non-IBM) — it skips whatever run is there, never over-runs. */
TEST(single_a1_prefix_still_reaches_address_mark) {
    uint8_t s[512];
    for (int i = 0; i < 512; ++i) s[i] = (uint8_t)(0xA5 ^ i);

    /* Hand-build a sector with only ONE A1 before each mark. */
    track_t t = {0};
    int prev = 0;
    for (int i = 0; i < 12; ++i) emit_byte(&t, 0x00, &prev);
    prev = 0;
    emit(&t, A1_WORD);                                  /* one A1 only */
    uint8_t id[5] = { 0xFE, 0, 0, 1, 0x02 };
    for (int i = 0; i < 5; ++i) emit_byte(&t, id[i], &prev);
    emit_crc(&t, id, 5, &prev);
    prev = 0;
    for (int i = 0; i < 22; ++i) emit_byte(&t, 0x00, &prev);
    prev = 0;
    emit(&t, A1_WORD);                                  /* one A1 only */
    uint8_t *dblk = malloc(1 + 512);
    dblk[0] = 0xFB; memcpy(dblk + 1, s, 512);
    emit_byte(&t, 0xFB, &prev);
    for (int i = 0; i < 512; ++i) emit_byte(&t, s[i], &prev);
    emit_crc(&t, dblk, 1 + 512, &prev);
    free(dblk);
    prev = 0;
    for (int i = 0; i < 12; ++i) emit_byte(&t, 0x00, &prev);

    uint32_t *tr = NULL;
    flux_raw_data_t flux = track_to_flux(&t, &tr);
    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding = FLUX_ENC_MFM; opts.bitcell_ns = BITCELL_NS; opts.use_pll = true;
    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_status_t st = flux_decode_mfm(&flux, &dt, &opts);

    ASSERT(st == FLUX_OK);
    ASSERT(dt.sector_count == 1);
    ASSERT(dt.sectors[0].sector == 1);
    ASSERT(dt.sectors[0].data && memcmp(dt.sectors[0].data, s, 512) == 0);

    flux_decoded_track_free(&dt);
    free(tr);
    free(t.w);
}

int main(void) {
    printf("=== Regression: IBM-MFM A1-sync-run decode (MF-218) ===\n");
    RUN(decodes_ibm_mfm_track_with_a1x3_sync);
    RUN(single_a1_prefix_still_reaches_address_mark);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
