/**
 * @file uft_mfm.c
 * @brief MFM encoding/decoding implementation
 * 
 * Implementation of MFM encoding for IBM PC and Amiga formats.
 */

#include "encoding/uft_mfm.h"
#include <string.h>

/* ============================================================================
 * MFM Encoding Tables
 * ============================================================================ */

/**
 * MFM encoding lookup table
 * For each byte, given the last bit of the previous byte (0 or 1),
 * produces the 16-bit MFM encoded value.
 * 
 * MFM rules:
 * - Data '1' = transition in middle of bit cell (01)
 * - Data '0' after '0' = transition at start (10) 
 * - Data '0' after '1' = no transition (00)
 */
static const uint16_t mfm_encode_table[2][256] = {
    /* Last bit was 0 */
    {
        0xAAAA, 0xAAA9, 0xAAA4, 0xAAA5, 0xAA92, 0xAA91, 0xAA94, 0xAA95,
        0xAA4A, 0xAA49, 0xAA44, 0xAA45, 0xAA52, 0xAA51, 0xAA54, 0xAA55,
        0xA92A, 0xA929, 0xA924, 0xA925, 0xA912, 0xA911, 0xA914, 0xA915,
        0xA94A, 0xA949, 0xA944, 0xA945, 0xA952, 0xA951, 0xA954, 0xA955,
        0xA4AA, 0xA4A9, 0xA4A4, 0xA4A5, 0xA492, 0xA491, 0xA494, 0xA495,
        0xA44A, 0xA449, 0xA444, 0xA445, 0xA452, 0xA451, 0xA454, 0xA455,
        0xA52A, 0xA529, 0xA524, 0xA525, 0xA512, 0xA511, 0xA514, 0xA515,
        0xA54A, 0xA549, 0xA544, 0xA545, 0xA552, 0xA551, 0xA554, 0xA555,
        0x92AA, 0x92A9, 0x92A4, 0x92A5, 0x9292, 0x9291, 0x9294, 0x9295,
        0x924A, 0x9249, 0x9244, 0x9245, 0x9252, 0x9251, 0x9254, 0x9255,
        0x912A, 0x9129, 0x9124, 0x9125, 0x9112, 0x9111, 0x9114, 0x9115,
        0x914A, 0x9149, 0x9144, 0x9145, 0x9152, 0x9151, 0x9154, 0x9155,
        0x94AA, 0x94A9, 0x94A4, 0x94A5, 0x9492, 0x9491, 0x9494, 0x9495,
        0x944A, 0x9449, 0x9444, 0x9445, 0x9452, 0x9451, 0x9454, 0x9455,
        0x952A, 0x9529, 0x9524, 0x9525, 0x9512, 0x9511, 0x9514, 0x9515,
        0x954A, 0x9549, 0x9544, 0x9545, 0x9552, 0x9551, 0x9554, 0x9555,
        0x4AAA, 0x4AA9, 0x4AA4, 0x4AA5, 0x4A92, 0x4A91, 0x4A94, 0x4A95,
        0x4A4A, 0x4A49, 0x4A44, 0x4A45, 0x4A52, 0x4A51, 0x4A54, 0x4A55,
        0x492A, 0x4929, 0x4924, 0x4925, 0x4912, 0x4911, 0x4914, 0x4915,
        0x494A, 0x4949, 0x4944, 0x4945, 0x4952, 0x4951, 0x4954, 0x4955,
        0x44AA, 0x44A9, 0x44A4, 0x44A5, 0x4492, 0x4491, 0x4494, 0x4495,
        0x444A, 0x4449, 0x4444, 0x4445, 0x4452, 0x4451, 0x4454, 0x4455,
        0x452A, 0x4529, 0x4524, 0x4525, 0x4512, 0x4511, 0x4514, 0x4515,
        0x454A, 0x4549, 0x4544, 0x4545, 0x4552, 0x4551, 0x4554, 0x4555,
        0x52AA, 0x52A9, 0x52A4, 0x52A5, 0x5292, 0x5291, 0x5294, 0x5295,
        0x524A, 0x5249, 0x5244, 0x5245, 0x5252, 0x5251, 0x5254, 0x5255,
        0x512A, 0x5129, 0x5124, 0x5125, 0x5112, 0x5111, 0x5114, 0x5115,
        0x514A, 0x5149, 0x5144, 0x5145, 0x5152, 0x5151, 0x5154, 0x5155,
        0x54AA, 0x54A9, 0x54A4, 0x54A5, 0x5492, 0x5491, 0x5494, 0x5495,
        0x544A, 0x5449, 0x5444, 0x5445, 0x5452, 0x5451, 0x5454, 0x5455,
        0x552A, 0x5529, 0x5524, 0x5525, 0x5512, 0x5511, 0x5514, 0x5515,
        0x554A, 0x5549, 0x5544, 0x5545, 0x5552, 0x5551, 0x5554, 0x5555,
    },
    /* Last bit was 1 */
    {
        0x2AAA, 0x2AA9, 0x2AA4, 0x2AA5, 0x2A92, 0x2A91, 0x2A94, 0x2A95,
        0x2A4A, 0x2A49, 0x2A44, 0x2A45, 0x2A52, 0x2A51, 0x2A54, 0x2A55,
        0x292A, 0x2929, 0x2924, 0x2925, 0x2912, 0x2911, 0x2914, 0x2915,
        0x294A, 0x2949, 0x2944, 0x2945, 0x2952, 0x2951, 0x2954, 0x2955,
        0x24AA, 0x24A9, 0x24A4, 0x24A5, 0x2492, 0x2491, 0x2494, 0x2495,
        0x244A, 0x2449, 0x2444, 0x2445, 0x2452, 0x2451, 0x2454, 0x2455,
        0x252A, 0x2529, 0x2524, 0x2525, 0x2512, 0x2511, 0x2514, 0x2515,
        0x254A, 0x2549, 0x2544, 0x2545, 0x2552, 0x2551, 0x2554, 0x2555,
        0x12AA, 0x12A9, 0x12A4, 0x12A5, 0x1292, 0x1291, 0x1294, 0x1295,
        0x124A, 0x1249, 0x1244, 0x1245, 0x1252, 0x1251, 0x1254, 0x1255,
        0x112A, 0x1129, 0x1124, 0x1125, 0x1112, 0x1111, 0x1114, 0x1115,
        0x114A, 0x1149, 0x1144, 0x1145, 0x1152, 0x1151, 0x1154, 0x1155,
        0x14AA, 0x14A9, 0x14A4, 0x14A5, 0x1492, 0x1491, 0x1494, 0x1495,
        0x144A, 0x1449, 0x1444, 0x1445, 0x1452, 0x1451, 0x1454, 0x1455,
        0x152A, 0x1529, 0x1524, 0x1525, 0x1512, 0x1511, 0x1514, 0x1515,
        0x154A, 0x1549, 0x1544, 0x1545, 0x1552, 0x1551, 0x1554, 0x1555,
        0x0AAA, 0x0AA9, 0x0AA4, 0x0AA5, 0x0A92, 0x0A91, 0x0A94, 0x0A95,
        0x0A4A, 0x0A49, 0x0A44, 0x0A45, 0x0A52, 0x0A51, 0x0A54, 0x0A55,
        0x092A, 0x0929, 0x0924, 0x0925, 0x0912, 0x0911, 0x0914, 0x0915,
        0x094A, 0x0949, 0x0944, 0x0945, 0x0952, 0x0951, 0x0954, 0x0955,
        0x04AA, 0x04A9, 0x04A4, 0x04A5, 0x0492, 0x0491, 0x0494, 0x0495,
        0x044A, 0x0449, 0x0444, 0x0445, 0x0452, 0x0451, 0x0454, 0x0455,
        0x052A, 0x0529, 0x0524, 0x0525, 0x0512, 0x0511, 0x0514, 0x0515,
        0x054A, 0x0549, 0x0544, 0x0545, 0x0552, 0x0551, 0x0554, 0x0555,
        0x12AA, 0x12A9, 0x12A4, 0x12A5, 0x1292, 0x1291, 0x1294, 0x1295,
        0x124A, 0x1249, 0x1244, 0x1245, 0x1252, 0x1251, 0x1254, 0x1255,
        0x112A, 0x1129, 0x1124, 0x1125, 0x1112, 0x1111, 0x1114, 0x1115,
        0x114A, 0x1149, 0x1144, 0x1145, 0x1152, 0x1151, 0x1154, 0x1155,
        0x14AA, 0x14A9, 0x14A4, 0x14A5, 0x1492, 0x1491, 0x1494, 0x1495,
        0x144A, 0x1449, 0x1444, 0x1445, 0x1452, 0x1451, 0x1454, 0x1455,
        0x152A, 0x1529, 0x1524, 0x1525, 0x1512, 0x1511, 0x1514, 0x1515,
        0x154A, 0x1549, 0x1544, 0x1545, 0x1552, 0x1551, 0x1554, 0x1555,
    }
};

/* ============================================================================
 * IBM Track Format Parameters
 * ============================================================================ */

const uft_mfm_track_format_t uft_ibm_formats[] = {
    /* 360K 5.25" DD */
    { 9, 2, 50, 80, 1, true, 250 },
    /* 720K 3.5" DD */
    { 9, 2, 80, 180, 1, true, 250 },
    /* 1.2M 5.25" HD */
    { 15, 2, 54, 80, 1, true, 500 },
    /* 1.44M 3.5" HD */
    { 18, 2, 84, 180, 1, true, 500 },
    /* 2.88M 3.5" ED */
    { 36, 2, 84, 180, 1, true, 1000 },
    /* Terminator */
    { 0, 0, 0, 0, 0, false, 0 }
};

/* ============================================================================
 * MFM Encoding Functions
 * ============================================================================ */

uint16_t uft_mfm_encode_byte(uint8_t data, uint8_t last_bit) {
    return mfm_encode_table[last_bit & 1][data];
}

uint8_t uft_mfm_decode_byte(uint16_t mfm) {
    uint8_t result = 0;
    
    /* Extract data bits (odd positions) */
    for (int i = 0; i < 8; i++) {
        if (mfm & (1 << (14 - i * 2))) {
            result |= (1 << (7 - i));
        }
    }
    
    return result;
}

uft_mfm_error_t uft_mfm_encode(
    const uint8_t *data,
    size_t data_len,
    uint8_t *mfm_out,
    uint8_t last_bit
) {
    if (!data || !mfm_out) return UFT_MFM_ERR_INVALID_PARAM;
    
    uint8_t prev_bit = last_bit & 1;
    
    for (size_t i = 0; i < data_len; i++) {
        uint16_t encoded = mfm_encode_table[prev_bit][data[i]];
        *mfm_out++ = (uint8_t)(encoded >> 8);
        *mfm_out++ = (uint8_t)(encoded & 0xFF);
        prev_bit = data[i] & 1;
    }
    
    return UFT_MFM_OK;
}

uft_mfm_error_t uft_mfm_decode(
    const uint8_t *mfm_in,
    size_t mfm_len,
    uint8_t *data_out
) {
    if (!mfm_in || !data_out) return UFT_MFM_ERR_INVALID_PARAM;
    if (mfm_len % 2 != 0) return UFT_MFM_ERR_INVALID_PARAM;
    
    for (size_t i = 0; i < mfm_len / 2; i++) {
        uint16_t mfm = (uint16_t)((mfm_in[i * 2] << 8) | mfm_in[i * 2 + 1]);
        data_out[i] = uft_mfm_decode_byte(mfm);
    }
    
    return UFT_MFM_OK;
}

/* ============================================================================
 * CRC Functions
 * ============================================================================ */

uint16_t uft_mfm_crc_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)((uint16_t)byte << 8);
    
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (uint16_t)((crc << 1) ^ UFT_CRC_CCITT_POLY);
        } else {
            crc <<= 1;
        }
    }
    
    return crc;
}

uint16_t uft_mfm_crc_ccitt(const uint8_t *data, size_t len, uint16_t init_crc) {
    uint16_t crc = init_crc;
    
    for (size_t i = 0; i < len; i++) {
        crc = uft_mfm_crc_update(crc, data[i]);
    }
    
    return crc;
}

/* ============================================================================
 * IBM Sector Operations
 * ============================================================================ */

size_t uft_mfm_encode_sector_id(
    const uft_mfm_sector_id_t *id,
    uint8_t *mfm_out
) {
    if (!id || !mfm_out) return 0;
    
    uint8_t *out = mfm_out;
    uint8_t last_bit = 0;
    
    /* 12 bytes of 0x00 (sync preamble) */
    for (int i = 0; i < 12; i++) {
        uint16_t enc = uft_mfm_encode_byte(0x00, last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = 0;
    }
    
    /* 3 bytes of 0xA1 with missing clock (sync) */
    for (int i = 0; i < 3; i++) {
        *out++ = 0x44;
        *out++ = 0x89;
    }
    last_bit = 1;
    
    /* ID address mark 0xFE */
    uint16_t enc = uft_mfm_encode_byte(UFT_MFM_AM_ID, last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = UFT_MFM_AM_ID & 1;
    
    /* Calculate CRC over A1 A1 A1 FE C H S N */
    uint16_t crc = UFT_CRC_CCITT_INIT;
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, UFT_MFM_AM_ID);
    crc = uft_mfm_crc_update(crc, id->cylinder);
    crc = uft_mfm_crc_update(crc, id->head);
    crc = uft_mfm_crc_update(crc, id->sector);
    crc = uft_mfm_crc_update(crc, id->size_code);
    
    /* Cylinder */
    enc = uft_mfm_encode_byte(id->cylinder, last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = id->cylinder & 1;
    
    /* Head */
    enc = uft_mfm_encode_byte(id->head, last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = id->head & 1;
    
    /* Sector */
    enc = uft_mfm_encode_byte(id->sector, last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = id->sector & 1;
    
    /* Size code */
    enc = uft_mfm_encode_byte(id->size_code, last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = id->size_code & 1;
    
    /* CRC high byte */
    enc = uft_mfm_encode_byte((uint8_t)(crc >> 8), last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = (crc >> 8) & 1;
    
    /* CRC low byte */
    enc = uft_mfm_encode_byte((uint8_t)(crc & 0xFF), last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    
    return (size_t)(out - mfm_out);
}

uft_mfm_error_t uft_mfm_decode_sector_id(
    const uint8_t *mfm_in,
    uft_mfm_sector_id_t *id
) {
    if (!mfm_in || !id) return UFT_MFM_ERR_INVALID_PARAM;
    
    /* Skip sync bytes, find address mark */
    const uint8_t *in = mfm_in;
    
    /* Decode address mark */
    uint16_t mfm = (uint16_t)((in[0] << 8) | in[1]);
    uint8_t am = uft_mfm_decode_byte(mfm);
    if (am != UFT_MFM_AM_ID) return UFT_MFM_ERR_ID_NOT_FOUND;
    in += 2;
    
    /* Decode fields */
    mfm = (uint16_t)((in[0] << 8) | in[1]); id->cylinder = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); id->head = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); id->sector = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); id->size_code = uft_mfm_decode_byte(mfm); in += 2;
    
    /* Decode CRC */
    mfm = (uint16_t)((in[0] << 8) | in[1]); uint8_t crc_hi = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); uint8_t crc_lo = uft_mfm_decode_byte(mfm);
    id->crc = (uint16_t)((crc_hi << 8) | crc_lo);
    
    /* Verify CRC */
    uint16_t calc_crc = UFT_CRC_CCITT_INIT;
    calc_crc = uft_mfm_crc_update(calc_crc, 0xA1);
    calc_crc = uft_mfm_crc_update(calc_crc, 0xA1);
    calc_crc = uft_mfm_crc_update(calc_crc, 0xA1);
    calc_crc = uft_mfm_crc_update(calc_crc, UFT_MFM_AM_ID);
    calc_crc = uft_mfm_crc_update(calc_crc, id->cylinder);
    calc_crc = uft_mfm_crc_update(calc_crc, id->head);
    calc_crc = uft_mfm_crc_update(calc_crc, id->sector);
    calc_crc = uft_mfm_crc_update(calc_crc, id->size_code);
    
    if (calc_crc != id->crc) {
        return UFT_MFM_ERR_CRC;
    }
    
    return UFT_MFM_OK;
}

size_t uft_mfm_encode_sector_data(
    const uint8_t *data,
    size_t data_len,
    bool deleted,
    uint8_t *mfm_out
) {
    if (!data || !mfm_out) return 0;
    
    uint8_t *out = mfm_out;
    uint8_t last_bit = 0;
    
    /* 12 bytes of 0x00 (sync preamble) */
    for (int i = 0; i < 12; i++) {
        uint16_t enc = uft_mfm_encode_byte(0x00, last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = 0;
    }
    
    /* 3 bytes of 0xA1 with missing clock (sync) */
    for (int i = 0; i < 3; i++) {
        *out++ = 0x44;
        *out++ = 0x89;
    }
    last_bit = 1;
    
    /* Data address mark */
    uint8_t am = deleted ? UFT_MFM_AM_DELETED : UFT_MFM_AM_DATA;
    uint16_t enc = uft_mfm_encode_byte(am, last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = am & 1;
    
    /* Calculate CRC */
    uint16_t crc = UFT_CRC_CCITT_INIT;
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, am);
    
    /* Encode data */
    for (size_t i = 0; i < data_len; i++) {
        crc = uft_mfm_crc_update(crc, data[i]);
        enc = uft_mfm_encode_byte(data[i], last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = data[i] & 1;
    }
    
    /* CRC */
    enc = uft_mfm_encode_byte((uint8_t)(crc >> 8), last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    last_bit = (crc >> 8) & 1;
    
    enc = uft_mfm_encode_byte((uint8_t)(crc & 0xFF), last_bit);
    *out++ = (uint8_t)(enc >> 8);
    *out++ = (uint8_t)(enc & 0xFF);
    
    return (size_t)(out - mfm_out);
}

uft_mfm_error_t uft_mfm_decode_sector_data(
    const uint8_t *mfm_in,
    uint8_t *data_out,
    size_t expected_len,
    bool *deleted
) {
    if (!mfm_in || !data_out) return UFT_MFM_ERR_INVALID_PARAM;
    
    const uint8_t *in = mfm_in;
    
    /* Decode address mark */
    uint16_t mfm = (uint16_t)((in[0] << 8) | in[1]);
    uint8_t am = uft_mfm_decode_byte(mfm);
    
    if (am == UFT_MFM_AM_DELETED) {
        if (deleted) *deleted = true;
    } else if (am == UFT_MFM_AM_DATA) {
        if (deleted) *deleted = false;
    } else {
        return UFT_MFM_ERR_DATA_NOT_FOUND;
    }
    in += 2;
    
    /* Calculate CRC and decode data */
    uint16_t crc = UFT_CRC_CCITT_INIT;
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, 0xA1);
    crc = uft_mfm_crc_update(crc, am);
    
    for (size_t i = 0; i < expected_len; i++) {
        mfm = (uint16_t)((in[0] << 8) | in[1]);
        data_out[i] = uft_mfm_decode_byte(mfm);
        crc = uft_mfm_crc_update(crc, data_out[i]);
        in += 2;
    }
    
    /* Verify CRC */
    mfm = (uint16_t)((in[0] << 8) | in[1]); uint8_t crc_hi = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); uint8_t crc_lo = uft_mfm_decode_byte(mfm);
    uint16_t read_crc = (uint16_t)((crc_hi << 8) | crc_lo);
    
    if (crc != read_crc) {
        return UFT_MFM_ERR_CRC;
    }
    
    return UFT_MFM_OK;
}

/* ============================================================================
 * Amiga Sector Operations
 * ============================================================================ */

uint32_t uft_amiga_checksum(const uint8_t *data, size_t len) {
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = ((uint32_t)data[i] << 24) |
                        ((uint32_t)data[i+1] << 16) |
                        ((uint32_t)data[i+2] << 8) |
                        (uint32_t)data[i+3];
        checksum ^= word;
    }
    
    return checksum;
}

size_t uft_amiga_encode_sector(
    const uft_amiga_sector_header_t *header,
    const uint8_t *data,
    uint8_t *mfm_out
) {
    if (!header || !data || !mfm_out) return 0;
    
    uint8_t *out = mfm_out;
    
    /* Two sync words: 0x4489 0x4489 */
    *out++ = 0x44; *out++ = 0x89;
    *out++ = 0x44; *out++ = 0x89;
    
    /* Format info in Amiga odd/even encoding */
    uint8_t info[4] = {
        header->format,
        header->track,
        header->sector,
        header->sectors_to_gap
    };
    
    /* Encode header info (odd bits, then even bits) */
    uint8_t last_bit = 1;  /* After sync */
    
    /* ... Amiga encoding is complex, simplified here ... */
    /* In real implementation, need proper odd/even split MFM */
    
    for (int i = 0; i < 4; i++) {
        uint16_t enc = uft_mfm_encode_byte(info[i], last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = info[i] & 1;
    }
    
    /* Label (16 bytes) */
    for (int i = 0; i < 16; i++) {
        uint16_t enc = uft_mfm_encode_byte(header->label[i], last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = header->label[i] & 1;
    }
    
    /* Header checksum */
    uint8_t hdr_chk[4] = {
        (uint8_t)(header->header_checksum >> 24),
        (uint8_t)(header->header_checksum >> 16),
        (uint8_t)(header->header_checksum >> 8),
        (uint8_t)(header->header_checksum)
    };
    for (int i = 0; i < 4; i++) {
        uint16_t enc = uft_mfm_encode_byte(hdr_chk[i], last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = hdr_chk[i] & 1;
    }
    
    /* Data checksum */
    uint8_t data_chk[4] = {
        (uint8_t)(header->data_checksum >> 24),
        (uint8_t)(header->data_checksum >> 16),
        (uint8_t)(header->data_checksum >> 8),
        (uint8_t)(header->data_checksum)
    };
    for (int i = 0; i < 4; i++) {
        uint16_t enc = uft_mfm_encode_byte(data_chk[i], last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = data_chk[i] & 1;
    }
    
    /* Data (512 bytes) */
    for (int i = 0; i < 512; i++) {
        uint16_t enc = uft_mfm_encode_byte(data[i], last_bit);
        *out++ = (uint8_t)(enc >> 8);
        *out++ = (uint8_t)(enc & 0xFF);
        last_bit = data[i] & 1;
    }
    
    return (size_t)(out - mfm_out);
}

uft_mfm_error_t uft_amiga_decode_sector(
    const uint8_t *mfm_in,
    uft_amiga_sector_header_t *header,
    uint8_t *data_out
) {
    if (!mfm_in || !header || !data_out) return UFT_MFM_ERR_INVALID_PARAM;
    
    /* Simplified decode - real implementation needs odd/even handling */
    const uint8_t *in = mfm_in + 4;  /* Skip sync */
    
    /* Decode header info */
    uint16_t mfm;
    mfm = (uint16_t)((in[0] << 8) | in[1]); header->format = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); header->track = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); header->sector = uft_mfm_decode_byte(mfm); in += 2;
    mfm = (uint16_t)((in[0] << 8) | in[1]); header->sectors_to_gap = uft_mfm_decode_byte(mfm); in += 2;
    
    /* Decode label */
    for (int i = 0; i < 16; i++) {
        mfm = (uint16_t)((in[0] << 8) | in[1]);
        header->label[i] = uft_mfm_decode_byte(mfm);
        in += 2;
    }
    
    /* Skip checksums */
    in += 8;
    
    /* Decode data */
    for (int i = 0; i < 512; i++) {
        mfm = (uint16_t)((in[0] << 8) | in[1]);
        data_out[i] = uft_mfm_decode_byte(mfm);
        in += 2;
    }
    
    return UFT_MFM_OK;
}

/* ============================================================================
 * Track-Level Operations
 * ============================================================================ */

ssize_t uft_mfm_find_sync(
    const uint8_t *track_data,
    size_t track_len,
    size_t start_offset
) {
    if (!track_data || start_offset >= track_len) return -1;
    
    /* Look for 0x44 0x89 0x44 0x89 0x44 0x89 (3x sync A1) */
    for (size_t i = start_offset; i <= track_len - 6; i++) {
        if (track_data[i] == 0x44 && track_data[i+1] == 0x89 &&
            track_data[i+2] == 0x44 && track_data[i+3] == 0x89 &&
            track_data[i+4] == 0x44 && track_data[i+5] == 0x89) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

ssize_t uft_amiga_find_sync(
    const uint8_t *track_data,
    size_t track_len,
    size_t start_offset
) {
    if (!track_data || start_offset >= track_len) return -1;
    
    /* Look for 0x44 0x89 0x44 0x89 (2x sync) */
    for (size_t i = start_offset; i <= track_len - 4; i++) {
        if (track_data[i] == 0x44 && track_data[i+1] == 0x89 &&
            track_data[i+2] == 0x44 && track_data[i+3] == 0x89) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

const uft_mfm_track_format_t* uft_mfm_get_format(const char *type) {
    if (!type) return NULL;
    
    if (strcmp(type, "360K") == 0) return &uft_ibm_formats[0];
    if (strcmp(type, "720K") == 0) return &uft_ibm_formats[1];
    if (strcmp(type, "1.2M") == 0) return &uft_ibm_formats[2];
    if (strcmp(type, "1.44M") == 0) return &uft_ibm_formats[3];
    if (strcmp(type, "2.88M") == 0) return &uft_ibm_formats[4];
    
    return NULL;
}
