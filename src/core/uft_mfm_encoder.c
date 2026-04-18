/**
 * @file uft_mfm_encoder.c
 * @brief Implementation of the shared MFM encoder.
 *
 * Design summary (see also uft_mfm_encoder.h):
 *
 *   MFM encoding rule per data bit d_i (after previous data bit d_{i-1}):
 *     d_i == 1 : emit cells  "0 1"
 *     d_i == 0 and d_{i-1} == 0 : emit cells  "1 0"
 *     d_i == 0 and d_{i-1} == 1 : emit cells  "0 0"
 *
 *   Producing raw MFM bits (1 cell = 1 raw bit), packed into output bytes
 *   MSB-first. Each data byte thus becomes 2 MFM bytes.
 *
 *   A1 sync mark: the real byte value 0xA1 encodes to raw 0x44A9 under
 *   the normal rule. The MFM "missing clock" is achieved by clearing
 *   the clock bit between data bits 4 and 5 — raw bit position 4 from
 *   the left of the second byte — producing 0x4489 instead. Every FDC
 *   on every platform uses this pattern to address-mark-lock.
 *
 *   CRC-16-CCITT: polynomial 0x1021, MSB-first, initial 0xFFFF. The
 *   running CRC starts at the first 0xA1 of each address mark and
 *   continues through the ID fields (IDAM) or data bytes (DAM).
 */

#include "uft/uft_mfm_encoder.h"

#include <stdbool.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Bit-level output helpers
 * -------------------------------------------------------------------------- */

typedef struct {
    uint8_t *buf;
    size_t   capacity;
    size_t   bit_pos;      /* bit index in buf, MSB-first packing */
    bool     overflow;
    /* Encoder state: last data bit emitted (used by MFM rule). */
    uint8_t  last_data_bit;
} mfm_ctx_t;

static void put_cell(mfm_ctx_t *c, uint8_t cell)
{
    if (c->overflow) return;
    size_t byte = c->bit_pos >> 3;
    if (byte >= c->capacity) { c->overflow = true; return; }
    uint8_t mask = (uint8_t)(0x80u >> (c->bit_pos & 7));
    if (cell) c->buf[byte] |= mask;
    else      c->buf[byte] &= (uint8_t)~mask;
    c->bit_pos++;
}

/* Emit one data byte via the normal MFM rule (8 data bits → 16 cells). */
static void emit_byte(mfm_ctx_t *c, uint8_t byte)
{
    for (int i = 7; i >= 0; i--) {
        uint8_t d = (uint8_t)((byte >> i) & 1);
        if (d) {
            put_cell(c, 0);                         /* clock = 0 */
            put_cell(c, 1);                         /* data  = 1 */
        } else {
            put_cell(c, c->last_data_bit ? 0 : 1);  /* clock only if prev data = 0 */
            put_cell(c, 0);                         /* data  = 0 */
        }
        c->last_data_bit = d;
    }
}

/* Emit one A1 address-mark byte with the clock between data bits 4 and 5
 * suppressed. Raw pattern is 0x4489 (MSB-first: 01000100 10001001). */
static void emit_a1_sync(mfm_ctx_t *c)
{
    static const uint8_t pattern[2] = { 0x44, 0x89 };
    for (int B = 0; B < 2; B++) {
        uint8_t byte = pattern[B];
        for (int i = 7; i >= 0; i--) put_cell(c, (byte >> i) & 1);
    }
    /* After A1 sync the "last data bit" is the data of bit 8 (=1 in 0xA1). */
    c->last_data_bit = 1;
}

/* Emit a C2 sync byte (used in IAM). Raw pattern is 0x5224. */
static void emit_c2_sync(mfm_ctx_t *c)
{
    static const uint8_t pattern[2] = { 0x52, 0x24 };
    for (int B = 0; B < 2; B++) {
        uint8_t byte = pattern[B];
        for (int i = 7; i >= 0; i--) put_cell(c, (byte >> i) & 1);
    }
    c->last_data_bit = 0;
}

static void emit_fill(mfm_ctx_t *c, uint8_t byte, size_t count)
{
    for (size_t i = 0; i < count; i++) emit_byte(c, byte);
}

/* --------------------------------------------------------------------------
 * CRC-16-CCITT
 * -------------------------------------------------------------------------- */

static uint16_t crc16_ccitt_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
        else              crc = (uint16_t)(crc << 1);
    }
    return crc;
}

/* Emit a data byte and also fold it into a running CRC. */
static void emit_byte_crc(mfm_ctx_t *c, uint8_t byte, uint16_t *crc)
{
    emit_byte(c, byte);
    *crc = crc16_ccitt_update(*crc, byte);
}

/* --------------------------------------------------------------------------
 * Sector encoding
 * -------------------------------------------------------------------------- */

static uint16_t ssize_from_code(uint8_t code)
{
    if (code > 6) return 0;
    return (uint16_t)(128u << code);
}

static void encode_sector(mfm_ctx_t *c,
                           const uft_sector_t *s,
                           uint8_t track, uint8_t head,
                           const uft_mfm_encode_params_t *p)
{
    /* Sync before IDAM: 12× 0x00 */
    emit_fill(c, 0x00, 12);

    /* IDAM: 3×A1 sync + FE + {C, H, R, N} + CRC16 */
    uint16_t crc = 0xFFFF;
    emit_a1_sync(c); crc = crc16_ccitt_update(crc, 0xA1);
    emit_a1_sync(c); crc = crc16_ccitt_update(crc, 0xA1);
    emit_a1_sync(c); crc = crc16_ccitt_update(crc, 0xA1);
    emit_byte_crc(c, 0xFE, &crc);
    emit_byte_crc(c, track, &crc);
    emit_byte_crc(c, head,  &crc);
    emit_byte_crc(c, s->id.sector,    &crc);
    emit_byte_crc(c, s->id.size_code, &crc);
    emit_byte(c, (uint8_t)(crc >> 8));
    emit_byte(c, (uint8_t)(crc & 0xFF));

    /* Gap2 */
    emit_fill(c, p->gap_byte, p->gap2);
    emit_fill(c, 0x00, 12);

    /* DAM: 3×A1 sync + FB + data[size] + CRC16 */
    crc = 0xFFFF;
    emit_a1_sync(c); crc = crc16_ccitt_update(crc, 0xA1);
    emit_a1_sync(c); crc = crc16_ccitt_update(crc, 0xA1);
    emit_a1_sync(c); crc = crc16_ccitt_update(crc, 0xA1);
    uint8_t dam = s->deleted ? 0xF8 : 0xFB;
    emit_byte_crc(c, dam, &crc);

    uint16_t sz = ssize_from_code(s->id.size_code);
    if (sz == 0) sz = 512;   /* fallback for broken ID */
    for (uint16_t i = 0; i < sz; i++) {
        uint8_t b = (s->data && i < s->data_len) ? s->data[i] : 0x00;
        emit_byte_crc(c, b, &crc);
    }
    emit_byte(c, (uint8_t)(crc >> 8));
    emit_byte(c, (uint8_t)(crc & 0xFF));

    /* Gap3 */
    emit_fill(c, p->gap_byte, p->gap3);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

size_t uft_mfm_encode_track(const uft_sector_t *sectors, size_t sector_count,
                             uint8_t cylinder, uint8_t head,
                             const uft_mfm_encode_params_t *params,
                             uint8_t *out, size_t out_capacity)
{
    if (!sectors || !params || !out || out_capacity == 0) return 0;

    mfm_ctx_t c = { out, out_capacity, 0, false, 0 };
    memset(out, 0, out_capacity);

    /* Prolog: Gap4a + sync + IAM (optional) + Gap1 */
    emit_fill(&c, params->gap_byte, params->gap4a);
    if (params->iam) {
        emit_fill(&c, 0x00, 12);
        emit_c2_sync(&c);
        emit_c2_sync(&c);
        emit_c2_sync(&c);
        emit_byte(&c, 0xFC);
        emit_fill(&c, params->gap_byte, params->gap1);
    }

    for (size_t i = 0; i < sector_count; i++)
        encode_sector(&c, &sectors[i], cylinder, head, params);

    /* Epilog: fill remainder of track with gap byte. Caller typically
     * preallocates the exact track size so this pads to the boundary. */
    while (!c.overflow && (c.bit_pos >> 3) < c.capacity)
        emit_byte(&c, params->gap_byte);

    if (c.overflow) {
        /* Buffer was too small — return partial count so caller can grow. */
        return c.bit_pos >> 3;
    }
    return c.bit_pos >> 3;
}

size_t uft_mfm_encode_from_track(const uft_track_t *track,
                                  uint8_t *out, size_t out_capacity)
{
    if (!track || !out) return 0;

    uft_mfm_encode_params_t params = UFT_MFM_PARAMS_DEFAULT_DD;
    if (track->bitrate >= 400000) {
        uft_mfm_encode_params_t hd = UFT_MFM_PARAMS_DEFAULT_HD;
        params = hd;
    }

    return uft_mfm_encode_track(track->sectors, track->sector_count,
                                 (uint8_t)track->cylinder,
                                 (uint8_t)track->head,
                                 &params, out, out_capacity);
}
