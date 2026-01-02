#include "uft/uft_error.h"
#include "cpc_mfm.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

/* --- bit helpers: MSB-first within bytes --- */
static inline int bit_get(const uint8_t *buf, size_t bitpos)
{
    const size_t byte = bitpos >> 3;
    const size_t sh   = 7u - (bitpos & 7u);
    return (buf[byte] >> sh) & 1u;
}

static inline uint16_t read_u16_be_bits(const uint8_t *bits, size_t bitpos)
{
    /* read 16 bits MSB-first */
    uint16_t v = 0;
    for (int i = 0; i < 16; ++i) {
        v = (uint16_t)((v << 1) | (uint16_t)bit_get(bits, bitpos + (size_t)i));
    }
    return v;
}

static inline uint8_t mfm_decode_byte_from_raw16(uint16_t raw16)
{
    /* raw16 layout: [c7 d7 c6 d6 ... c0 d0]
       We extract data bits d7..d0 = bits 14,12,...,0.
    */
    uint8_t out = 0;
    for (int i = 0; i < 8; ++i) {
        const int bit = (raw16 >> (14 - 2*i)) & 1;
        out = (uint8_t)((out << 1) | (uint8_t)bit);
    }
    return out;
}

/* CRC-16/CCITT (poly 0x1021, init 0xFFFF), byte-wise. */
static uint16_t crc16_ccitt_update(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; ++i)
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    return crc;
}

static uint32_t secsize_from_n(uint8_t n)
{
    if (n > 7) return 0;
    return (uint32_t)128u << n;
}

/*
 * Track parser state: last seen IDAM fields.
 * CPC uses IBM MFM layout (IDAM=0xFE, DAM=0xFB, DDAM=0xF8).
 */
typedef struct last_idam {
    bool     valid;
    uint8_t  C, H, R, N;

    /* Forensic metadata about the last IDAM seen */
    uint32_t idam_bitpos;           /* bit position of the IDAM mark byte (0xFE) */
    uint16_t idam_crc_read;
    uint16_t idam_crc_calc;
    uint16_t pre_idam_sync_zeros;   /* best-effort (0 if unknown) */
} last_idam_t;

int cpc_mfm_decode_track_bits(
    const uint8_t *mfm_bits, size_t mfm_bit_count,
    uint16_t cyl, uint16_t head,
    bool want_head_match,
    ufm_logical_image_t *li,
    cpc_mfm_decode_stats_t *stats_out)
{
    if (!mfm_bits || mfm_bit_count < 128u || !li) return -EINVAL;

    cpc_mfm_decode_stats_t st;
    memset(&st, 0, sizeof(st));

    last_idam_t last = {0};

    /* We scan in 16-bit steps, but need resync tolerance.
       So we advance by 1 bit when hunting sync, and by 16 bits when consuming fields.
    */
    size_t bit = 0;
    const size_t end = mfm_bit_count;

    while (bit + 16u <= end) {
        /* hunt 0x4489 */
        if (read_u16_be_bits(mfm_bits, bit) != 0x4489u) {
            bit += 1u;
            continue;
        }

        /* potential sync run: need 3x 0x4489 */
        if (bit + 16u*3u + 16u > end) { st.truncated_fields++; break; }
        const uint16_t w0 = read_u16_be_bits(mfm_bits, bit + 0u);
        const uint16_t w1 = read_u16_be_bits(mfm_bits, bit + 16u);
        const uint16_t w2 = read_u16_be_bits(mfm_bits, bit + 32u);
        if (w0 != 0x4489u || w1 != 0x4489u || w2 != 0x4489u) {
            bit += 1u;
            continue;
        }
        st.sync_hits++;
        bit += 48u;

        /* Address Mark byte */
        if (bit + 16u > end) { st.truncated_fields++; break; }
        const uint8_t mark = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit));
        bit += 16u;

        if (mark == 0xFEu) {
            /* ID Address Mark: C H R N + CRC16 */
            st.idams++;
            uint16_t crc = 0xFFFFu;
            /* include A1 A1 A1 FE */
            crc = crc16_ccitt_update(crc, 0xA1u);
            crc = crc16_ccitt_update(crc, 0xA1u);
            crc = crc16_ccitt_update(crc, 0xA1u);
            crc = crc16_ccitt_update(crc, mark);

            if (bit + 16u* (4u + 2u) > end) { st.truncated_fields++; break; }
            const uint8_t C = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 0u));
            const uint8_t H = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 16u));
            const uint8_t R = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 32u));
            const uint8_t N = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 48u));
            crc = crc16_ccitt_update(crc, C);
            crc = crc16_ccitt_update(crc, H);
            crc = crc16_ccitt_update(crc, R);
            crc = crc16_ccitt_update(crc, N);
            bit += 64u;

            const uint8_t crc_hi = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 0u));
            const uint8_t crc_lo = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 16u));
            bit += 32u;
            const uint16_t on_disk = (uint16_t)((uint16_t)crc_hi << 8) | crc_lo;
            if (crc != on_disk) st.bad_crc_idam++;

/* stash for DAM correlation */
last.idam_bitpos = (uint32_t)(bit - 32u - 64u - 16u); /* approximate: mark byte start */
last.idam_crc_read = on_disk;
last.idam_crc_calc = crc;
last.pre_idam_sync_zeros = 0;

            if (want_head_match && H != (uint8_t)head) {
                /* ignore mismatching head if requested */
                last.valid = false;
                continue;
            }

            last.valid = true;
            last.C = C; last.H = H; last.R = R; last.N = N;
            continue;
        }

        if (mark == 0xFBu || mark == 0xF8u) {
            /* Data Address Mark (normal or deleted). Needs a preceding valid IDAM. */
            st.dams++;
            if (!last.valid) {
                /* no ID context; skip conservatively */
                continue;
            }

            const uint32_t sec_sz = secsize_from_n(last.N);
            if (sec_sz == 0) {
                last.valid = false;
                continue;
            }

            /* CRC includes A1 A1 A1 + mark + data */
            uint16_t crc = 0xFFFFu;
            crc = crc16_ccitt_update(crc, 0xA1u);
            crc = crc16_ccitt_update(crc, 0xA1u);
            crc = crc16_ccitt_update(crc, 0xA1u);
            crc = crc16_ccitt_update(crc, mark);

            const size_t need_bits = 16u * (size_t)sec_sz + 32u;
            if (bit + need_bits > end) { st.truncated_fields++; break; }

            /* decode payload */
            uint8_t tmp[8192];
            if (sec_sz > sizeof(tmp)) {
                /* Phase-1: guard; CPC is typically <= 2048 */
                return -ENOSPC;
            }
            for (uint32_t i = 0; i < sec_sz; ++i) {
                const uint8_t b = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + (size_t)i * 16u));
                tmp[i] = b;
                crc = crc16_ccitt_update(crc, b);
            }
            bit += 16u * (size_t)sec_sz;

            const uint8_t crc_hi = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 0u));
            const uint8_t crc_lo = mfm_decode_byte_from_raw16(read_u16_be_bits(mfm_bits, bit + 16u));
            bit += 32u;
            const uint16_t on_disk = (uint16_t)((uint16_t)crc_hi << 8) | crc_lo;

            ufm_sec_flags_t flags = UFM_SEC_OK;
            if (mark == 0xF8u) flags = (ufm_sec_flags_t)(flags | UFM_SEC_DELETED_DAM);
            if (crc != on_disk) { flags = (ufm_sec_flags_t)(flags | UFM_SEC_BAD_CRC); st.bad_crc_dam++; }

            /* Prefer CHRN from IDAM; fall back to physical cyl/head. */
            const uint16_t out_c = last.C ? (uint16_t)last.C : cyl;
            const uint16_t out_h = (uint16_t)last.H;
            const uint16_t out_r = (uint16_t)last.R;

            ufm_sector_t *out = ufm_logical_add_sector_ref(li, out_c, out_h, out_r, tmp, sec_sz, flags);
            if (out) {
                ufm_sector_meta_mfm_ibm_t *m = (ufm_sector_meta_mfm_ibm_t*)calloc(1u, sizeof(*m));
                if (m) {
                    m->id_c = last.C; m->id_h = last.H; m->id_r = last.R; m->id_n = last.N;
                    m->dam_mark = mark;
                    m->idam_crc_read = last.idam_crc_read;
                    m->idam_crc_calc = last.idam_crc_calc;
                    m->dam_crc_read = on_disk;
                    m->dam_crc_calc = crc;
                    m->idam_bitpos = last.idam_bitpos;
                    m->dam_bitpos = (uint32_t)(bit - 32u - (sec_sz * 16u) - 16u);
                    m->pre_idam_sync_zeros = last.pre_idam_sync_zeros;
                    m->pre_dam_sync_zeros = 0;
                    m->weak_score = 0;
                    out->meta_type = UFM_SECTOR_META_MFM_IBM;
                    out->meta = m;
                }
            }
            st.sectors_emitted++;

            /* One DAM consumes the current ID context. */
            last.valid = false;
            continue;
        }

        /* Any other mark: ignore. */
    }

    if (stats_out) *stats_out = st;
    return 0;
}
