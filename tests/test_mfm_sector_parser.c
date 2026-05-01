/**
 * @file test_mfm_sector_parser.c
 * @brief Synthetic MFM bitstream tests for uft_mfm_decode_track().
 *
 * Builds a hand-encoded MFM track containing one or two sectors with
 * known data + valid IBM CRCs, then runs the parser and verifies:
 *   1. Sector count matches
 *   2. CHRN fields decoded correctly
 *   3. ID and Data CRCs validate
 *   4. Sector payload bytes match
 *   5. Bad-CRC sector is reported as such (not silently dropped)
 *
 * MF-141 / AUD-002.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/flux/uft_mfm_sector_parser.h"

/* Same minimal CRC-16/CCITT-FALSE as the parser TU — see comment in
 * src/flux/uft_mfm_sector_parser.c about the phantom unified-CRC
 * header. Duplicating ~10 lines is the right trade-off vs. dragging
 * an unresolved external dep into the test build. */
static uint16_t test_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
#define uft_crc16_ccitt(d, l) test_crc16_ccitt((d), (l))

/* ────────────────────────────────────────────────────────────────────
 * MFM bit-stream builder
 *
 * State: prev_data_bit (the last *data* bit emitted, controls next
 * clock bit per MFM rule: clock = NOT(prev_data OR cur_data)).
 * Bits are accumulated into a packed MSB-first buffer.
 * ──────────────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t *buf;
    size_t   cap_bits;
    size_t   pos_bits;
    int      prev_data_bit;
} mfm_writer_t;

static void mfm_init(mfm_writer_t *w, uint8_t *buf, size_t cap_bytes) {
    w->buf = buf;
    w->cap_bits = cap_bytes * 8u;
    w->pos_bits = 0;
    w->prev_data_bit = 0;
    memset(buf, 0, cap_bytes);
}

static void mfm_put_bit(mfm_writer_t *w, int bit) {
    if (w->pos_bits >= w->cap_bits) return;
    if (bit) {
        size_t byte_idx = w->pos_bits >> 3;
        size_t bit_idx  = 7u - (w->pos_bits & 7u);
        w->buf[byte_idx] |= (uint8_t)(1u << bit_idx);
    }
    w->pos_bits++;
}

/* Standard MFM byte encoding: per data bit D, emit (clock, D) where
 * clock = !(prev_data | D). */
static void mfm_put_byte(mfm_writer_t *w, uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        int d = (byte >> i) & 1;
        int c = (w->prev_data_bit | d) ? 0 : 1;
        mfm_put_bit(w, c);
        mfm_put_bit(w, d);
        w->prev_data_bit = d;
    }
}

/* The A1 sync mark with missing clock bit. Emits the literal 16-bit
 * pattern 0x4489 directly, bypassing the standard MFM rule. */
static void mfm_put_a1_sync(mfm_writer_t *w) {
    const uint16_t SYNC = 0x4489;
    for (int i = 15; i >= 0; i--) {
        mfm_put_bit(w, (SYNC >> i) & 1);
    }
    /* After A1 sync, the trailing data bit was 1 (LSB of 0xA1 = 1)
     * so the next byte's clock decision must use prev=1. */
    w->prev_data_bit = 1;
}

/* Convenience: write gap (zeros) for n_bytes worth of MFM time. */
static void mfm_put_gap(mfm_writer_t *w, size_t n_bytes) {
    for (size_t i = 0; i < n_bytes; i++) {
        mfm_put_byte(w, 0x4E);  /* IBM standard gap fill */
    }
}

/* ────────────────────────────────────────────────────────────────────
 * Test 1 — single 512-byte sector with valid CRCs round-trips
 * ──────────────────────────────────────────────────────────────────── */
static void test_single_sector_ok(void) {
    uint8_t track[16384] = {0};
    mfm_writer_t w;
    mfm_init(&w, track, sizeof(track));

    /* Pre-gap. */
    mfm_put_gap(&w, 80);

    /* IDAM. */
    mfm_put_a1_sync(&w);
    mfm_put_a1_sync(&w);
    mfm_put_a1_sync(&w);
    uint8_t idam[8] = { 0xA1, 0xA1, 0xA1, 0xFE, 0x05, 0x01, 0x07, 0x02 };  /* C=5,H=1,R=7,N=2(512) */
    mfm_put_byte(&w, idam[3]);
    mfm_put_byte(&w, idam[4]);
    mfm_put_byte(&w, idam[5]);
    mfm_put_byte(&w, idam[6]);
    mfm_put_byte(&w, idam[7]);
    uint16_t id_crc = uft_crc16_ccitt(idam, sizeof(idam));
    mfm_put_byte(&w, (uint8_t)(id_crc >> 8));
    mfm_put_byte(&w, (uint8_t)id_crc);

    /* Inter-mark gap (typical 22 bytes of 0x4E + 12 bytes of 0x00). */
    mfm_put_gap(&w, 22);
    for (int i = 0; i < 12; i++) mfm_put_byte(&w, 0x00);

    /* DAM. */
    mfm_put_a1_sync(&w);
    mfm_put_a1_sync(&w);
    mfm_put_a1_sync(&w);
    mfm_put_byte(&w, 0xFB);  /* DAM */

    /* Sector payload — 512 bytes of pattern. */
    uint8_t payload[512];
    for (int i = 0; i < 512; i++) payload[i] = (uint8_t)(i ^ 0xA5);
    for (int i = 0; i < 512; i++) mfm_put_byte(&w, payload[i]);

    /* DAM CRC = CRC(A1 A1 A1 FB || payload). */
    uint8_t crc_input[516];
    crc_input[0] = 0xA1; crc_input[1] = 0xA1; crc_input[2] = 0xA1; crc_input[3] = 0xFB;
    memcpy(crc_input + 4, payload, 512);
    uint16_t d_crc = uft_crc16_ccitt(crc_input, sizeof(crc_input));
    mfm_put_byte(&w, (uint8_t)(d_crc >> 8));
    mfm_put_byte(&w, (uint8_t)d_crc);

    /* Decode. */
    uint8_t pool[2048];
    uft_mfm_sector_t sectors[8];
    size_t n = uft_mfm_decode_track(track, w.pos_bits,
                                    pool, sizeof(pool),
                                    sectors, 8, NULL);
    assert(n == 1);
    assert(sectors[0].cylinder == 5);
    assert(sectors[0].head == 1);
    assert(sectors[0].sector == 7);
    assert(sectors[0].size_code == 2);
    assert(sectors[0].id_crc_ok);
    assert(sectors[0].dam_present);
    assert(!sectors[0].deleted);
    assert(sectors[0].data_crc_ok);
    assert(sectors[0].data_len == 512);
    assert(memcmp(pool + sectors[0].data_offset, payload, 512) == 0);
    printf("test_single_sector_ok: OK (CHRN=%d/%d/%d/%d, both CRCs valid)\n",
           sectors[0].cylinder, sectors[0].head, sectors[0].sector,
           sectors[0].size_code);
}

/* ────────────────────────────────────────────────────────────────────
 * Test 2 — corrupted DAM CRC is reported, not silently accepted
 * ──────────────────────────────────────────────────────────────────── */
static void test_bad_data_crc_reported(void) {
    uint8_t track[16384] = {0};
    mfm_writer_t w;
    mfm_init(&w, track, sizeof(track));

    mfm_put_gap(&w, 80);
    mfm_put_a1_sync(&w); mfm_put_a1_sync(&w); mfm_put_a1_sync(&w);
    uint8_t idam[8] = { 0xA1, 0xA1, 0xA1, 0xFE, 0x00, 0x00, 0x01, 0x02 };
    for (int i = 3; i < 8; i++) mfm_put_byte(&w, idam[i]);
    uint16_t id_crc = uft_crc16_ccitt(idam, sizeof(idam));
    mfm_put_byte(&w, (uint8_t)(id_crc >> 8));
    mfm_put_byte(&w, (uint8_t)id_crc);

    mfm_put_gap(&w, 22);
    for (int i = 0; i < 12; i++) mfm_put_byte(&w, 0x00);

    mfm_put_a1_sync(&w); mfm_put_a1_sync(&w); mfm_put_a1_sync(&w);
    mfm_put_byte(&w, 0xFB);
    uint8_t payload[512];
    for (int i = 0; i < 512; i++) payload[i] = (uint8_t)i;
    for (int i = 0; i < 512; i++) mfm_put_byte(&w, payload[i]);

    /* Write a deliberately wrong CRC. */
    mfm_put_byte(&w, 0xDE);
    mfm_put_byte(&w, 0xAD);

    uint8_t pool[2048];
    uft_mfm_sector_t sectors[8];
    size_t n = uft_mfm_decode_track(track, w.pos_bits,
                                    pool, sizeof(pool),
                                    sectors, 8, NULL);
    assert(n == 1);
    assert(sectors[0].id_crc_ok);
    assert(sectors[0].dam_present);
    assert(!sectors[0].data_crc_ok);   /* The whole point of this test */
    /* Data is still returned so caller can salvage if desired. */
    assert(sectors[0].data_len == 512);
    assert(memcmp(pool + sectors[0].data_offset, payload, 512) == 0);
    printf("test_bad_data_crc_reported: OK (sector returned with"
           " data_crc_ok=false, payload still copied for salvage)\n");
}

/* ────────────────────────────────────────────────────────────────────
 * Test 3 — IDAM with no following DAM in window: dam_present=false
 * ──────────────────────────────────────────────────────────────────── */
static void test_orphan_idam(void) {
    uint8_t track[16384] = {0};
    mfm_writer_t w;
    mfm_init(&w, track, sizeof(track));

    mfm_put_gap(&w, 80);
    mfm_put_a1_sync(&w); mfm_put_a1_sync(&w); mfm_put_a1_sync(&w);
    uint8_t idam[8] = { 0xA1, 0xA1, 0xA1, 0xFE, 0x00, 0x00, 0x09, 0x01 };  /* N=1 (256) */
    for (int i = 3; i < 8; i++) mfm_put_byte(&w, idam[i]);
    uint16_t id_crc = uft_crc16_ccitt(idam, sizeof(idam));
    mfm_put_byte(&w, (uint8_t)(id_crc >> 8));
    mfm_put_byte(&w, (uint8_t)id_crc);

    /* Big gap, no DAM at all. */
    mfm_put_gap(&w, 200);

    uint8_t pool[1024];
    uft_mfm_sector_t sectors[4];
    size_t n = uft_mfm_decode_track(track, w.pos_bits,
                                    pool, sizeof(pool),
                                    sectors, 4, NULL);
    assert(n == 1);
    assert(sectors[0].id_crc_ok);
    assert(!sectors[0].dam_present);    /* No DAM in window */
    assert(sectors[0].data_len == 0);   /* No fabricated data */
    printf("test_orphan_idam: OK (IDAM seen, no DAM, no data fabricated)\n");
}

/* ────────────────────────────────────────────────────────────────────
 * Test 4 — empty / garbage track returns 0 sectors
 * ──────────────────────────────────────────────────────────────────── */
static void test_empty_track(void) {
    uint8_t track[1024];
    memset(track, 0xAA, sizeof(track));   /* Plausible-looking junk */
    uft_mfm_sector_t sectors[8];
    uint8_t pool[1024];
    size_t n = uft_mfm_decode_track(track, sizeof(track) * 8,
                                    pool, sizeof(pool),
                                    sectors, 8, NULL);
    /* 0xAA repeating contains no 0x4489 sync at any bit offset, so
     * the decoder must find nothing. */
    assert(n == 0);
    printf("test_empty_track: OK (no false IDAMs in 0xAA pattern)\n");
}

/* ────────────────────────────────────────────────────────────────────
 * Test 5 — input validation
 * ──────────────────────────────────────────────────────────────────── */
static void test_input_validation(void) {
    uft_mfm_sector_t sectors[1];
    uint8_t buf[64] = {0};
    uint8_t pool[64];

    /* NULL bitstream → 0. */
    assert(uft_mfm_decode_track(NULL, 1024, pool, sizeof(pool),
                                sectors, 1, NULL) == 0);
    /* Tiny bit_count → 0. */
    assert(uft_mfm_decode_track(buf, 32, pool, sizeof(pool),
                                sectors, 1, NULL) == 0);
    /* NULL sectors → 0. */
    assert(uft_mfm_decode_track(buf, 1024, pool, sizeof(pool),
                                NULL, 1, NULL) == 0);
    /* max_sectors=0 → 0. */
    assert(uft_mfm_decode_track(buf, 1024, pool, sizeof(pool),
                                sectors, 0, NULL) == 0);

    /* size_code helper. */
    assert(uft_mfm_sector_size_from_code(0) == 128);
    assert(uft_mfm_sector_size_from_code(1) == 256);
    assert(uft_mfm_sector_size_from_code(2) == 512);
    assert(uft_mfm_sector_size_from_code(3) == 1024);
    assert(uft_mfm_sector_size_from_code(8) == 0);   /* invalid */
    printf("test_input_validation: OK\n");
}

int main(void) {
    test_single_sector_ok();
    test_bad_data_crc_reported();
    test_orphan_idam();
    test_empty_track();
    test_input_validation();
    printf("\nAll MFM sector parser tests passed.\n");
    return 0;
}
