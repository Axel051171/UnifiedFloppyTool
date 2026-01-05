#include "uft/uft_error.h"
#include "mfm_ibm_encode.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static uint16_t crc16_ccitt_update(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)b << 8;
    for (unsigned i = 0; i < 8; ++i)
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    return crc;
}

typedef struct bitw {
    uint8_t *buf;
    size_t cap_bits;
    size_t bitpos;
} bitw_t;

static int bitw_init(bitw_t *w, size_t cap_bits)
{
    if (!w) return -EINVAL;
    memset(w, 0, sizeof(*w));
    w->cap_bits = cap_bits;
    size_t cap_bytes = (cap_bits + 7u) / 8u;
    w->buf = (uint8_t*)calloc(1, cap_bytes);
    if (!w->buf) return -ENOMEM;
    return 0;
}

static void bitw_free(bitw_t *w)
{
    if (!w) return;
    free(w->buf);
    w->buf = NULL;
    w->cap_bits = 0;
    w->bitpos = 0;
}

static int bitw_put_bit(bitw_t *w, uint8_t bit)
{
    if (!w || !w->buf) return -EINVAL;
    if (w->bitpos >= w->cap_bits) return -ENOSPC;
    const size_t byte = w->bitpos / 8u;
    const size_t b = w->bitpos % 8u; /* 0..7 => MSB..LSB */
    if (bit) w->buf[byte] |= (uint8_t)(0x80u >> b);
    w->bitpos++;
    return 0;
}

static int bitw_put_raw16_be(bitw_t *w, uint16_t raw16)
{
    /* MSB-first */
    for (int i = 15; i >= 0; --i) {
        int rc = bitw_put_bit(w, (uint8_t)((raw16 >> i) & 1u));
        if (rc < 0) return rc;
    }
    return 0;
}

static uint16_t mfm_encode_byte(uint8_t data, uint8_t *prev_data_bit)
{
    uint16_t raw = 0;
    uint8_t prev = prev_data_bit ? *prev_data_bit : 0;
    for (int i = 7; i >= 0; --i) {
        const uint8_t d = (uint8_t)((data >> i) & 1u);
        const uint8_t c = (uint8_t)((prev == 0u && d == 0u) ? 1u : 0u);
        raw = (uint16_t)((raw << 1) | c);
        raw = (uint16_t)((raw << 1) | d);
        prev = d;
    }
    if (prev_data_bit) *prev_data_bit = prev;
    return raw;
}

static int bitw_put_mfm_byte(bitw_t *w, uint8_t b, uint8_t *prev_data_bit)
{
    uint16_t raw = mfm_encode_byte(b, prev_data_bit);
    return bitw_put_raw16_be(w, raw);
}

static int bitw_put_mfm_a1_sync(bitw_t *w, uint8_t *prev_data_bit)
{
    /* Special A1 with missing clock bits -> raw word 0x4489 in MFM raw bitcells. */
    if (prev_data_bit) *prev_data_bit = 1u; /* last data bit of 0xA1 is 1 */
    return bitw_put_raw16_be(w, 0x4489u);
}

size_t mfm_ibm_nominal_track_bytes(uint16_t bit_rate_kbps, uint16_t rpm)
{
    if (bit_rate_kbps == 0 || rpm == 0) return 0;
    /* bit_rate_kbps * 1000 * (60/rpm) bits, convert to bytes */
    const double seconds = 60.0 / (double)rpm;
    const double bits = (double)bit_rate_kbps * 1000.0 * seconds;
    size_t bytes = (size_t)(bits / 8.0 + 0.5);
    /* Keep typical alignment (HFE stores in bytes; later we 256/512 interleave) */
    return bytes;
}

static uint16_t infer_n_from_size(uint32_t sec_size)
{
    /* N code where size = 128 << N */
    uint16_t n = 0;
    uint32_t s = 128;
    while (n < 8 && s < sec_size) { s <<= 1; n++; }
    return n;
}

static int emit_track(bitw_t *w, const ufm_logical_image_t *li, const mfm_ibm_track_params_t *p, size_t nominal_bytes)
{
    if (!w || !li || !p || nominal_bytes == 0) return -EINVAL;

    /* Conservative IBM layout (sufficient for sector images): */
    const uint16_t C = p->cyl;
    const uint16_t H = p->head;
    const uint16_t spt = p->spt;
    const uint32_t sec_size = p->sec_size;
    const uint8_t N = (uint8_t)infer_n_from_size(sec_size);

    uint8_t prev = 0;

    /* GAP 4a: 80x 0x4E */
    for (int i = 0; i < 80; ++i) {
        int rc = bitw_put_mfm_byte(w, 0x4Eu, &prev);
        if (rc < 0) return rc;
    }

    for (uint16_t r = 1; r <= spt; ++r) {
        const ufm_sector_t *sec = ufm_logical_find_const(li, C, H, r);

        /* --- IDAM --- */
        for (int i = 0; i < 12; ++i) { int rc = bitw_put_mfm_byte(w, 0x00u, &prev); if (rc < 0) return rc; }
        for (int i = 0; i < 3; ++i) { int rc = bitw_put_mfm_a1_sync(w, &prev); if (rc < 0) return rc; }

        uint16_t crc = 0xFFFFu;
        crc = crc16_ccitt_update(crc, 0xA1u);
        crc = crc16_ccitt_update(crc, 0xA1u);
        crc = crc16_ccitt_update(crc, 0xA1u);

        const uint8_t mark_id = 0xFEu;
        crc = crc16_ccitt_update(crc, mark_id);
        crc = crc16_ccitt_update(crc, (uint8_t)C);
        crc = crc16_ccitt_update(crc, (uint8_t)H);
        crc = crc16_ccitt_update(crc, (uint8_t)r);
        crc = crc16_ccitt_update(crc, N);

        { int rc = bitw_put_mfm_byte(w, mark_id, &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)C, &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)H, &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)r, &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)N, &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)(crc >> 8), &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)(crc & 0xFFu), &prev); if (rc < 0) return rc; }

        /* GAP 2 */
        for (int i = 0; i < 22; ++i) { int rc = bitw_put_mfm_byte(w, 0x4Eu, &prev); if (rc < 0) return rc; }

        /* --- DAM + DATA --- */
        for (int i = 0; i < 12; ++i) { int rc = bitw_put_mfm_byte(w, 0x00u, &prev); if (rc < 0) return rc; }
        for (int i = 0; i < 3; ++i) { int rc = bitw_put_mfm_a1_sync(w, &prev); if (rc < 0) return rc; }

        uint8_t dam = 0xFBu;
        if (sec && (sec->flags & UFM_SEC_DELETED_DAM)) dam = 0xF8u;

        crc = 0xFFFFu;
        crc = crc16_ccitt_update(crc, 0xA1u);
        crc = crc16_ccitt_update(crc, 0xA1u);
        crc = crc16_ccitt_update(crc, 0xA1u);
        crc = crc16_ccitt_update(crc, dam);

        { int rc = bitw_put_mfm_byte(w, dam, &prev); if (rc < 0) return rc; }

        /* Data bytes: if missing or wrong size, we emit zero-filled (archive-safe, deterministic). */
        for (uint32_t i = 0; i < sec_size; ++i) {
            uint8_t b = 0x00u;
            if (sec && sec->data && sec->size == sec_size) b = sec->data[i];
            crc = crc16_ccitt_update(crc, b);
            int rc = bitw_put_mfm_byte(w, b, &prev);
            if (rc < 0) return rc;
        }

        { int rc = bitw_put_mfm_byte(w, (uint8_t)(crc >> 8), &prev); if (rc < 0) return rc; }
        { int rc = bitw_put_mfm_byte(w, (uint8_t)(crc & 0xFFu), &prev); if (rc < 0) return rc; }

        /* GAP 3 */
        for (int i = 0; i < 54; ++i) { int rc = bitw_put_mfm_byte(w, 0x4Eu, &prev); if (rc < 0) return rc; }
    }

    /* Pad to nominal length with 0x4E */
    const size_t nominal_bits = nominal_bytes * 8u;
    while (w->bitpos + 16u <= nominal_bits) {
        int rc = bitw_put_mfm_byte(w, 0x4Eu, &prev);
        if (rc < 0) return rc;
    }
    while (w->bitpos < nominal_bits) {
        int rc = bitw_put_bit(w, 0);
        if (rc < 0) return rc;
    }

    return 0;
}

int mfm_ibm_build_track_bits(
    const ufm_logical_image_t *li,
    const mfm_ibm_track_params_t *p,
    uint8_t **out_bits,
    size_t *out_bit_count)
{
    if (!li || !p || !out_bits || !out_bit_count) return -EINVAL;
    *out_bits = NULL;
    *out_bit_count = 0;

    const size_t nominal_bytes = mfm_ibm_nominal_track_bytes(p->bit_rate_kbps, p->rpm);
    if (nominal_bytes == 0) return -EINVAL;

    /* Allocate a bitwriter buffer with slack. */
    bitw_t w;
    int rc = bitw_init(&w, (nominal_bytes * 8u) + (nominal_bytes * 8u / 2u));
    if (rc < 0) return rc;

    rc = emit_track(&w, li, p, nominal_bytes);
    if (rc < 0) {
        bitw_free(&w);
        return rc;
    }

    /* Truncate/normalize to nominal byte length. */
    const size_t want_bytes = nominal_bytes;
    uint8_t *out = (uint8_t*)malloc(want_bytes);
    if (!out) {
        bitw_free(&w);
        return -ENOMEM;
    }
    memcpy(out, w.buf, want_bytes);

    bitw_free(&w);

    *out_bits = out;
    *out_bit_count = want_bytes * 8u;
    return 0;
}
