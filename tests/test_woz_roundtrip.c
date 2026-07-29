/**
 * @file test_woz_roundtrip.c
 * @brief WOZ2 writer — read -> write -> byte-identity round-trip test.
 *
 * Links the real module (src/formats/apple/uft_woz.c) and proves the new
 * woz_save_to_memory() serializer reproduces a standard WOZ2 image
 * bit-for-bit. This is the safety gate for the writer: a naive serializer
 * that got the block layout wrong would fail the byte-compare here rather
 * than silently ship a corrupt image.
 *
 * The synthesized image is the canonical WOZ2 layout:
 *   header(12) + INFO(8+60) + TMAP(8+160) + TRKS(8 + 1280 table + 512 BITS)
 * The 160x8 = 1280-byte TRK table ends at byte 1536 = block 3, exactly where
 * the TRK entry's absolute starting_block(=3) points — so the writer's
 * chunk-order serialization is layout-exact and must round-trip identically.
 */

#include "uft/formats/apple/uft_woz.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define TRKS_TABLE   1280u          /* 160 entries x 8 bytes */
#define BITS_BLOCK   512u
#define TRACK_DATA   (TRKS_TABLE + BITS_BLOCK)   /* 1792 */
#define WOZ_TOTAL    (12u + 8u + 60u + 8u + 160u + 8u + TRACK_DATA)  /* 2048 */

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* Build a canonical, CRC-valid WOZ2 image into buf (>= WOZ_TOTAL). */
static size_t build_woz2(uint8_t *buf) {
    memset(buf, 0, WOZ_TOTAL);
    /* header */
    buf[0] = 'W'; buf[1] = 'O'; buf[2] = 'Z'; buf[3] = '2';
    buf[4] = 0xFF; buf[5] = 0x0A; buf[6] = 0x0D; buf[7] = 0x0A;
    /* crc at [8..11] filled last */
    size_t p = 12;
    /* INFO */
    buf[p] = 'I'; buf[p+1] = 'N'; buf[p+2] = 'F'; buf[p+3] = 'O'; put_u32(buf + p + 4, 60); p += 8;
    buf[p + 0] = 2;   /* INFO version 2 */
    buf[p + 1] = 1;   /* disk_type 5.25" */
    buf[p + 2] = 0;   /* not write protected */
    for (int i = 5; i < 37; i++) buf[p + i] = (uint8_t)('A' + (i % 26)); /* creator */
    buf[p + 37] = 1;  /* disk_sides */
    buf[p + 39] = 32; /* optimal_bit_timing */
    p += 60;
    /* TMAP: track 0 -> index 0, everything else empty (0xFF) */
    buf[p] = 'T'; buf[p+1] = 'M'; buf[p+2] = 'A'; buf[p+3] = 'P'; put_u32(buf + p + 4, 160); p += 8;
    memset(buf + p, 0xFF, 160);
    buf[p + 0] = 0;   /* quarter-track 0 -> TRK 0 */
    p += 160;
    /* TRKS: table (1280) + one BITS block (512) */
    buf[p] = 'T'; buf[p+1] = 'R'; buf[p+2] = 'K'; buf[p+3] = 'S'; put_u32(buf + p + 4, TRACK_DATA); p += 8;
    /* TRK entry 0: starting_block=3, block_count=1, bit_count=4096 */
    put_u16(buf + p + 0, 3);
    put_u16(buf + p + 2, 1);
    put_u32(buf + p + 4, 4096);
    /* deterministic BITS block at table end (= file byte 1536 = block 3) */
    for (uint32_t i = 0; i < BITS_BLOCK; i++)
        buf[p + TRKS_TABLE + i] = (uint8_t)((i * 37u + 11u) & 0xFF);
    p += TRACK_DATA;
    /* CRC32 over everything after byte 12 */
    put_u32(buf + 8, woz_crc32(0, buf + 12, WOZ_TOTAL - 12));
    return WOZ_TOTAL;
}

/* WOZ2 with a trailing WRIT chunk (WOZ 2.1). Same canonical body as build_woz2,
 * then an 8-byte chunk header + WRIT_PAYLOAD bytes, then CRC over all of it. The
 * writer emits WRIT right after TRKS, so this must also round-trip byte-exact. */
#define WRIT_PAYLOAD  24u
#define WOZ_TOTAL_W   (WOZ_TOTAL + 8u + WRIT_PAYLOAD)

static size_t build_woz2_with_writ(uint8_t *buf) {
    /* Reuse the canonical builder for the body (it CRCs over WOZ_TOTAL-12; we
     * overwrite the CRC after appending WRIT). */
    build_woz2(buf);
    size_t p = WOZ_TOTAL;
    buf[p] = 'W'; buf[p+1] = 'R'; buf[p+2] = 'I'; buf[p+3] = 'T';
    put_u32(buf + p + 4, WRIT_PAYLOAD); p += 8;
    /* deterministic, opaque payload (verbatim passthrough — content unread) */
    for (uint32_t i = 0; i < WRIT_PAYLOAD; i++) buf[p + i] = (uint8_t)(0xA0 + i);
    p += WRIT_PAYLOAD;
    put_u32(buf + 8, woz_crc32(0, buf + 12, WOZ_TOTAL_W - 12));
    return WOZ_TOTAL_W;
}

/* ── A. round-trip is byte-identical ───────────────────────────────── */

TEST(read_write_byte_identical) {
    uint8_t original[WOZ_TOTAL];
    size_t n = build_woz2(original);
    ASSERT(n == WOZ_TOTAL);

    woz_image_t *img = NULL;
    ASSERT(woz_load_from_memory(original, n, &img) == WOZ_OK);
    ASSERT(img != NULL);

    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT(woz_save_to_memory(img, &out, &out_size) == WOZ_OK);
    ASSERT(out_size == WOZ_TOTAL);
    ASSERT(memcmp(out, original, WOZ_TOTAL) == 0);   /* the whole point */

    free(out);
    woz_free(img);
}

/* ── B. re-load of the written image matches ───────────────────────── */

TEST(written_image_reloads_valid) {
    uint8_t original[WOZ_TOTAL];
    build_woz2(original);

    woz_image_t *img1 = NULL;
    ASSERT(woz_load_from_memory(original, WOZ_TOTAL, &img1) == WOZ_OK);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT(woz_save_to_memory(img1, &out, &out_size) == WOZ_OK);

    woz_image_t *img2 = NULL;
    ASSERT(woz_load_from_memory(out, out_size, &img2) == WOZ_OK);
    ASSERT(img2->version == img1->version);
    ASSERT(img2->crc_valid == true);                     /* CRC re-validates */
    ASSERT(memcmp(img2->tmap, img1->tmap, 160) == 0);
    ASSERT(img2->track_data_size == img1->track_data_size);
    ASSERT(memcmp(img2->track_data, img1->track_data, img1->track_data_size) == 0);

    free(out);
    woz_free(img1);
    woz_free(img2);
}

/* ── C. error paths ────────────────────────────────────────────────── */

TEST(save_rejects_null_and_empty) {
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT(woz_save_to_memory(NULL, &out, &out_size) != WOZ_OK);

    woz_image_t empty;
    memset(&empty, 0, sizeof(empty));
    empty.version = 2;
    empty.track_data = NULL;                 /* no track data */
    ASSERT(woz_save_to_memory(&empty, &out, &out_size) != WOZ_OK);
}

/* ── D. WRIT chunk is preserved verbatim (MF-357) ──────────────────── */

TEST(writ_chunk_byte_identical) {
    uint8_t original[WOZ_TOTAL_W];
    size_t n = build_woz2_with_writ(original);
    ASSERT(n == WOZ_TOTAL_W);

    woz_image_t *img = NULL;
    ASSERT(woz_load_from_memory(original, n, &img) == WOZ_OK);
    ASSERT(img->has_write_hints == true);
    ASSERT(img->writ_raw_size == WRIT_PAYLOAD);
    ASSERT(img->writ_raw != NULL);

    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT(woz_save_to_memory(img, &out, &out_size) == WOZ_OK);
    ASSERT(out_size == WOZ_TOTAL_W);               /* WRIT re-emitted, not dropped */
    ASSERT(memcmp(out, original, WOZ_TOTAL_W) == 0);

    free(out);
    woz_free(img);
}

TEST(writ_survives_reload) {
    uint8_t original[WOZ_TOTAL_W];
    build_woz2_with_writ(original);

    woz_image_t *img1 = NULL;
    ASSERT(woz_load_from_memory(original, WOZ_TOTAL_W, &img1) == WOZ_OK);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT(woz_save_to_memory(img1, &out, &out_size) == WOZ_OK);

    woz_image_t *img2 = NULL;
    ASSERT(woz_load_from_memory(out, out_size, &img2) == WOZ_OK);
    ASSERT(img2->crc_valid == true);                    /* CRC covers WRIT */
    ASSERT(img2->writ_raw_size == img1->writ_raw_size);
    ASSERT(memcmp(img2->writ_raw, img1->writ_raw, img1->writ_raw_size) == 0);

    free(out);
    woz_free(img1);
    woz_free(img2);
}

int main(void) {
    printf("=== WOZ2 writer round-trip tests ===\n");
    RUN(read_write_byte_identical);
    RUN(written_image_reloads_valid);
    RUN(save_rejects_null_and_empty);
    RUN(writ_chunk_byte_identical);
    RUN(writ_survives_reload);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
