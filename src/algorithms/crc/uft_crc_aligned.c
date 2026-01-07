/**
 * @file uft_crc_aligned.c
 * @brief CRC-16 calculation with bit-alignment support
 */

#include "uft_crc_aligned.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint8_t reflect8(uint8_t data) {
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        if (data & (1 << i)) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

static uint16_t reflect16(uint16_t data) {
    uint16_t result = 0;
    for (int i = 0; i < 16; i++) {
        if (data & (1 << i)) {
            result |= (1 << (15 - i));
        }
    }
    return result;
}

/* Pre-computed CRC table for CCITT polynomial */
static uint16_t crc_table_ccitt[256];
static bool table_initialized = false;

static void init_crc_table(void) {
    if (table_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ UFT_CRC16_CCITT;
            } else {
                crc <<= 1;
            }
        }
        crc_table_ccitt[i] = crc;
    }
    table_initialized = true;
}

/* ============================================================================
 * Standard CRC Functions
 * ============================================================================ */

void uft_crc16_init_ccitt(uft_crc16_ctx_t *ctx) {
    uft_crc16_init(ctx, UFT_CRC16_CCITT, UFT_CRC16_INIT_FFFF,
                   false, false, 0);
}

void uft_crc16_init(uft_crc16_ctx_t *ctx,
                    uint16_t polynomial,
                    uint16_t init_value,
                    bool reflect_in,
                    bool reflect_out,
                    uint16_t xor_out) {
    if (!ctx) return;
    
    ctx->polynomial = polynomial;
    ctx->init_value = init_value;
    ctx->reflect_in = reflect_in;
    ctx->reflect_out = reflect_out;
    ctx->xor_out = xor_out;
    ctx->crc = init_value;
    ctx->bit_buffer = 0;
    ctx->bit_count = 0;
    
    init_crc_table();
}

void uft_crc16_reset(uft_crc16_ctx_t *ctx) {
    if (!ctx) return;
    ctx->crc = ctx->init_value;
    ctx->bit_buffer = 0;
    ctx->bit_count = 0;
}

void uft_crc16_byte(uft_crc16_ctx_t *ctx, uint8_t byte) {
    if (!ctx) return;
    
    uint8_t data = ctx->reflect_in ? reflect8(byte) : byte;
    
    /* Use table for CCITT, bit-by-bit for others */
    if (ctx->polynomial == UFT_CRC16_CCITT && !ctx->reflect_in) {
        ctx->crc = (ctx->crc << 8) ^ crc_table_ccitt[(ctx->crc >> 8) ^ data];
    } else {
        /* Generic bit-by-bit */
        for (int i = 7; i >= 0; i--) {
            uint8_t bit = (data >> i) & 1;
            uft_crc16_bit(ctx, bit);
        }
    }
}

void uft_crc16_block(uft_crc16_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx || !data) return;
    
    for (size_t i = 0; i < len; i++) {
        uft_crc16_byte(ctx, data[i]);
    }
}

uint16_t uft_crc16_final(uft_crc16_ctx_t *ctx) {
    if (!ctx) return 0;
    
    uint16_t result = ctx->crc;
    
    if (ctx->reflect_out) {
        result = reflect16(result);
    }
    
    return result ^ ctx->xor_out;
}

uint16_t uft_crc16_calc(const uint8_t *data, size_t len) {
    uft_crc16_ctx_t ctx;
    uft_crc16_init_ccitt(&ctx);
    uft_crc16_block(&ctx, data, len);
    return uft_crc16_final(&ctx);
}

/* ============================================================================
 * Bit-Level CRC Functions
 * ============================================================================ */

void uft_crc16_bit(uft_crc16_ctx_t *ctx, uint8_t bit) {
    if (!ctx) return;
    
    bool xor_flag = (ctx->crc & 0x8000) != 0;
    ctx->crc <<= 1;
    
    if (bit) {
        ctx->crc |= 1;
    }
    
    if (xor_flag) {
        ctx->crc ^= ctx->polynomial;
    }
}

void uft_crc16_bits(uft_crc16_ctx_t *ctx, uint8_t bits, uint8_t count) {
    if (!ctx || count == 0 || count > 8) return;
    
    for (int i = count - 1; i >= 0; i--) {
        uft_crc16_bit(ctx, (bits >> i) & 1);
    }
}

uint16_t uft_crc16_with_offset(const uint8_t *data,
                                size_t len_bytes,
                                int bit_offset,
                                uint16_t polynomial,
                                uint16_t init) {
    if (!data || len_bytes == 0) return 0;
    
    uft_crc16_ctx_t ctx;
    uft_crc16_init(&ctx, polynomial, init, false, false, 0);
    
    size_t total_bits = len_bytes * 8;
    
    /* Handle negative offset (skip first bits) */
    size_t start_bit = 0;
    if (bit_offset < 0) {
        start_bit = (size_t)(-bit_offset);
    }
    
    /* Handle positive offset (read before data) */
    if (bit_offset > 0) {
        /* Assume leading zeros for positive offset */
        for (int i = 0; i < bit_offset; i++) {
            uft_crc16_bit(&ctx, 0);
        }
    }
    
    /* Process actual data bits */
    for (size_t i = start_bit; i < total_bits; i++) {
        size_t byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);  /* MSB first */
        uint8_t bit = (data[byte_idx] >> bit_idx) & 1;
        uft_crc16_bit(&ctx, bit);
    }
    
    return uft_crc16_final(&ctx);
}

/* ============================================================================
 * Auto-Alignment Functions
 * ============================================================================ */

uft_crc_alignment_t uft_crc16_find_alignment(const uint8_t *data,
                                              size_t len_bytes,
                                              uint16_t expected_crc,
                                              int max_offset) {
    uft_crc_alignment_t result = {0};
    result.found = false;
    
    if (!data || len_bytes < 2) return result;
    
    /* Try each offset from -max to +max */
    for (int offset = -max_offset; offset <= max_offset; offset++) {
        uint16_t calc = uft_crc16_with_offset(data, len_bytes, offset,
                                               UFT_CRC16_CCITT, UFT_CRC16_INIT_FFFF);
        
        if (calc == expected_crc) {
            result.offset = offset;
            result.crc = calc;
            result.found = true;
            
            /* Confidence based on offset magnitude */
            if (offset == 0) {
                result.confidence = 100;
            } else {
                result.confidence = (uint8_t)(100 - (abs(offset) * 10));
            }
            
            return result;  /* First match wins (prefer smaller offsets) */
        }
    }
    
    return result;
}

bool uft_crc16_verify_auto(const uint8_t *data,
                           size_t len_bytes,
                           int *out_offset) {
    if (!data || len_bytes < 3) return false;
    
    /* Extract expected CRC from last 2 bytes */
    uint16_t expected = ((uint16_t)data[len_bytes - 2] << 8) | data[len_bytes - 1];
    
    /* Try to find alignment */
    uft_crc_alignment_t align = uft_crc16_find_alignment(
        data, len_bytes - 2, expected, 7);
    
    if (out_offset) {
        *out_offset = align.found ? align.offset : 0;
    }
    
    return align.found;
}

/* ============================================================================
 * IBM Floppy Specific
 * ============================================================================ */

uint16_t uft_crc16_sector_header(uint8_t c, uint8_t h, uint8_t r, uint8_t n) {
    uft_crc16_ctx_t ctx;
    uft_crc16_init_ccitt(&ctx);
    
    /* Include 3x A1 sync bytes in CRC (per IBM spec) */
    uft_crc16_byte(&ctx, 0xA1);
    uft_crc16_byte(&ctx, 0xA1);
    uft_crc16_byte(&ctx, 0xA1);
    
    /* IDAM byte */
    uft_crc16_byte(&ctx, 0xFE);
    
    /* Header data */
    uft_crc16_byte(&ctx, c);
    uft_crc16_byte(&ctx, h);
    uft_crc16_byte(&ctx, r);
    uft_crc16_byte(&ctx, n);
    
    return uft_crc16_final(&ctx);
}

uint16_t uft_crc16_sector_data(const uint8_t *data, size_t len, uint8_t dam) {
    if (!data) return 0;
    
    uft_crc16_ctx_t ctx;
    uft_crc16_init_ccitt(&ctx);
    
    /* Include 3x A1 sync bytes */
    uft_crc16_byte(&ctx, 0xA1);
    uft_crc16_byte(&ctx, 0xA1);
    uft_crc16_byte(&ctx, 0xA1);
    
    /* DAM byte */
    uft_crc16_byte(&ctx, dam);
    
    /* Sector data */
    uft_crc16_block(&ctx, data, len);
    
    return uft_crc16_final(&ctx);
}

bool uft_crc16_verify_header(const uint8_t *header_plus_crc) {
    if (!header_plus_crc) return false;
    
    uint16_t expected = ((uint16_t)header_plus_crc[4] << 8) | header_plus_crc[5];
    uint16_t calc = uft_crc16_sector_header(
        header_plus_crc[0], header_plus_crc[1],
        header_plus_crc[2], header_plus_crc[3]);
    
    return calc == expected;
}

bool uft_crc16_verify_data(const uint8_t *dam_plus_data_plus_crc, size_t total_len) {
    if (!dam_plus_data_plus_crc || total_len < 4) return false;
    
    size_t data_len = total_len - 3;  /* -1 DAM, -2 CRC */
    uint8_t dam = dam_plus_data_plus_crc[0];
    
    uint16_t expected = ((uint16_t)dam_plus_data_plus_crc[total_len - 2] << 8) | 
                        dam_plus_data_plus_crc[total_len - 1];
    uint16_t calc = uft_crc16_sector_data(dam_plus_data_plus_crc + 1, data_len, dam);
    
    return calc == expected;
}
