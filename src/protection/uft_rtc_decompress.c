/**
 * @file uft_rtc_decompress.c
 * @brief Rob Northen Computing (RTC) Decompressor Implementation
 * 
 * LZAR-style decompression for RTC copy-protected software.
 * 
 * Algorithm:
 * 1. Read 32-bit little-endian output size from header
 * 2. Initialize arithmetic coder state
 * 3. Initialize probability models for lit/len and distance symbols
 * 4. Decode stream:
 *    - symbol < 256: literal byte
 *    - symbol >= 256: match (length = symbol - 253, decode distance)
 * 5. Copy matched bytes from sliding window
 * 
 * The arithmetic coder uses 17-bit precision with adaptive models.
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/protection/uft_rtc_decompress.h"
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Internal Constants
 *===========================================================================*/

#define WINDOW_SIZE     4096
#define WINDOW_MASK     0xFFF
#define WINDOW_INIT_POS 4036

#define LITLEN_SYMBOLS  315
#define DIST_SYMBOLS    4096

#define MODEL_MAX_FREQ  0x7FFF

/*===========================================================================
 * Context Structure
 *===========================================================================*/

struct uft_rtc_ctx {
    /* Input buffer */
    const uint8_t *in_buf;
    size_t in_size;
    size_t in_pos;
    
    /* Output buffer */
    uint8_t *out_buf;
    size_t out_size;
    size_t out_pos;
    size_t out_target;
    
    /* Sliding window */
    uint8_t window[WINDOW_SIZE];
    uint16_t window_pos;
    
    /* Arithmetic coder state */
    uint32_t ac_low;
    uint32_t ac_high;
    uint32_t ac_code;
    uint8_t ac_bit_buf;
    uint8_t ac_bit_pos;
    
    /* Literal/Length model */
    uint16_t litlen_freq[LITLEN_SYMBOLS + 1];   /* Cumulative frequencies */
    uint16_t litlen_symbol[LITLEN_SYMBOLS];     /* Symbol table */
    uint16_t litlen_pos[LITLEN_SYMBOLS];        /* Position in symbol table */
    
    /* Distance model */
    uint16_t dist_freq[DIST_SYMBOLS + 1];       /* Cumulative frequencies */
};

/*===========================================================================
 * Error Messages
 *===========================================================================*/

static const char *error_messages[] = {
    "Success",
    "Null pointer argument",
    "Memory allocation failed",
    "Invalid output size in header",
    "Input data truncated",
    "Output buffer overflow",
    "Compressed data corrupt"
};

const char *uft_rtc_error_string(uft_rtc_error_t err)
{
    if (err < 0 || err >= sizeof(error_messages) / sizeof(error_messages[0])) {
        return "Unknown error";
    }
    return error_messages[err];
}

/*===========================================================================
 * Context Management
 *===========================================================================*/

uft_rtc_ctx_t *uft_rtc_ctx_create(void)
{
    uft_rtc_ctx_t *ctx = calloc(1, sizeof(uft_rtc_ctx_t));
    return ctx;
}

void uft_rtc_ctx_destroy(uft_rtc_ctx_t *ctx)
{
    free(ctx);
}

void uft_rtc_ctx_reset(uft_rtc_ctx_t *ctx)
{
    if (!ctx) return;
    
    ctx->in_buf = NULL;
    ctx->in_size = 0;
    ctx->in_pos = 0;
    ctx->out_buf = NULL;
    ctx->out_size = 0;
    ctx->out_pos = 0;
    ctx->out_target = 0;
    
    ctx->ac_low = 0;
    ctx->ac_high = 0x20000;
    ctx->ac_code = 0;
    ctx->ac_bit_buf = 0;
    ctx->ac_bit_pos = 0;
}

/*===========================================================================
 * Bit Reading
 *===========================================================================*/

static inline int read_bit(uft_rtc_ctx_t *ctx)
{
    /* Shift bit buffer */
    ctx->ac_bit_pos >>= 1;
    
    if (ctx->ac_bit_pos == 0) {
        /* Need new byte */
        if (ctx->in_pos >= ctx->in_size) {
            ctx->ac_bit_buf = 0xFF;  /* Pad with 1s at end */
        } else {
            ctx->ac_bit_buf = ctx->in_buf[ctx->in_pos++];
        }
        ctx->ac_bit_pos = 0x80;
    }
    
    return (ctx->ac_bit_pos & ctx->ac_bit_buf) ? 1 : 0;
}

/*===========================================================================
 * Arithmetic Coder
 *===========================================================================*/

static void ac_init(uft_rtc_ctx_t *ctx)
{
    ctx->ac_low = 0;
    ctx->ac_high = 0x20000;
    ctx->ac_code = 0;
    ctx->ac_bit_buf = 0;
    ctx->ac_bit_pos = 0;
    
    /* Prime coder with 17 bits */
    for (int i = 0; i < 17; i++) {
        ctx->ac_code = (ctx->ac_code << 1) | read_bit(ctx);
    }
}

static void ac_normalize(uft_rtc_ctx_t *ctx)
{
    while (1) {
        if (ctx->ac_low >= 0x10000) {
            /* Both in upper half - shift out MSB */
            ctx->ac_low -= 0x10000;
            ctx->ac_high -= 0x10000;
            ctx->ac_code -= 0x10000;
        } else if (ctx->ac_low >= 0x8000 && ctx->ac_high <= 0x18000) {
            /* Straddle case - shift out middle bit */
            ctx->ac_low -= 0x8000;
            ctx->ac_high -= 0x8000;
            ctx->ac_code -= 0x8000;
        } else if (ctx->ac_high <= 0x10000) {
            /* Both in lower half - shift */
        } else {
            break;
        }
        
        ctx->ac_low <<= 1;
        ctx->ac_high <<= 1;
        ctx->ac_code = (ctx->ac_code << 1) | read_bit(ctx);
    }
}

static int ac_decode_symbol(uft_rtc_ctx_t *ctx, 
                            const uint16_t *cum_freq, int num_symbols,
                            int (*find_symbol)(uft_rtc_ctx_t *, const uint16_t *, uint16_t))
{
    uint32_t range = ctx->ac_high - ctx->ac_low;
    uint16_t total = cum_freq[0];
    
    /* Calculate scaled code value */
    uint32_t scaled = ((uint32_t)total * (ctx->ac_code - ctx->ac_low + 1) - 1) / range;
    
    /* Find symbol using model-specific search */
    int symbol = find_symbol(ctx, cum_freq, (uint16_t)scaled);
    
    /* Update interval */
    ctx->ac_high = ctx->ac_low + range * cum_freq[symbol] / total;
    ctx->ac_low = ctx->ac_low + range * cum_freq[symbol + 1] / total;
    
    /* Normalize */
    ac_normalize(ctx);
    
    return symbol;
}

/*===========================================================================
 * Model Initialization
 *===========================================================================*/

static void init_litlen_model(uft_rtc_ctx_t *ctx)
{
    /* Initialize symbol table and positions */
    for (int i = 0; i < LITLEN_SYMBOLS; i++) {
        ctx->litlen_symbol[i] = LITLEN_SYMBOLS - 1 - i;
        ctx->litlen_pos[LITLEN_SYMBOLS - 1 - i] = i;
    }
    
    /* Initialize frequencies (uniform) */
    ctx->litlen_freq[0] = 0;
    for (int i = 1; i <= LITLEN_SYMBOLS; i++) {
        ctx->litlen_freq[i] = ctx->litlen_freq[i - 1] + 1;
    }
}

static void init_dist_model(uft_rtc_ctx_t *ctx)
{
    /* Initialize with decreasing frequencies based on distance */
    ctx->dist_freq[DIST_SYMBOLS] = 0;
    for (int i = DIST_SYMBOLS - 1; i >= 0; i--) {
        ctx->dist_freq[i] = ctx->dist_freq[i + 1] + 10000 / (DIST_SYMBOLS - i + 200);
    }
}

static void init_window(uft_rtc_ctx_t *ctx)
{
    /* Initialize window with spaces */
    memset(ctx->window, 0x20, WINDOW_SIZE);
    ctx->window_pos = WINDOW_INIT_POS;
}

/*===========================================================================
 * Symbol Search Functions
 *===========================================================================*/

static int find_litlen_symbol(uft_rtc_ctx_t *ctx, const uint16_t *freq, uint16_t target)
{
    /* Binary search for symbol where freq[sym] <= target < freq[sym+1] */
    int lo = 1, hi = LITLEN_SYMBOLS;
    
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (freq[mid] <= target) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    
    return lo;
}

static int find_dist_symbol(uft_rtc_ctx_t *ctx, const uint16_t *freq, uint16_t target)
{
    /* Binary search for distance */
    int lo = 1, hi = DIST_SYMBOLS;
    
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (freq[mid] <= target) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    
    return lo - 1;
}

/*===========================================================================
 * Model Update
 *===========================================================================*/

static void update_litlen_model(uft_rtc_ctx_t *ctx, int symbol)
{
    /* Check if rescaling needed */
    if (ctx->litlen_freq[LITLEN_SYMBOLS] >= MODEL_MAX_FREQ) {
        uint16_t sum = 0;
        for (int i = LITLEN_SYMBOLS; i >= 1; i--) {
            uint16_t freq = ctx->litlen_freq[i] - ctx->litlen_freq[i - 1];
            freq = (freq + 1) >> 1;  /* Halve, round up */
            ctx->litlen_freq[i] = sum;
            sum += freq;
        }
        ctx->litlen_freq[0] = sum;
    }
    
    /* Find actual position of symbol in table */
    int pos = ctx->litlen_pos[symbol];
    
    /* Move symbol toward front if it has same frequency as predecessor */
    while (pos > 0 && 
           ctx->litlen_freq[pos] == ctx->litlen_freq[pos - 1]) {
        /* Swap symbols */
        int sym1 = ctx->litlen_symbol[pos];
        int sym2 = ctx->litlen_symbol[pos - 1];
        ctx->litlen_symbol[pos] = sym2;
        ctx->litlen_symbol[pos - 1] = sym1;
        ctx->litlen_pos[sym1] = pos - 1;
        ctx->litlen_pos[sym2] = pos;
        pos--;
    }
    
    /* Increment frequency for this symbol and all predecessors */
    for (int i = pos; i >= 0; i--) {
        ctx->litlen_freq[i]++;
    }
}

/*===========================================================================
 * Output
 *===========================================================================*/

static inline uft_rtc_error_t emit_byte(uft_rtc_ctx_t *ctx, uint8_t byte)
{
    if (ctx->out_pos >= ctx->out_size) {
        return UFT_RTC_ERR_OVERFLOW;
    }
    
    ctx->out_buf[ctx->out_pos++] = byte;
    ctx->window[ctx->window_pos] = byte;
    ctx->window_pos = (ctx->window_pos + 1) & WINDOW_MASK;
    
    return UFT_RTC_OK;
}

/*===========================================================================
 * Main Decompression
 *===========================================================================*/

static uft_rtc_error_t decompress_stream(uft_rtc_ctx_t *ctx)
{
    /* Initialize */
    ac_init(ctx);
    init_litlen_model(ctx);
    init_dist_model(ctx);
    init_window(ctx);
    
    size_t decoded = 0;
    
    while (decoded < ctx->out_target) {
        /* Decode literal/length symbol */
        int symbol = ac_decode_symbol(ctx, ctx->litlen_freq, LITLEN_SYMBOLS,
                                       find_litlen_symbol);
        
        /* Get actual symbol value from table */
        int value = ctx->litlen_symbol[symbol];
        
        if (value < 256) {
            /* Literal byte */
            uft_rtc_error_t err = emit_byte(ctx, (uint8_t)value);
            if (err != UFT_RTC_OK) return err;
            decoded++;
        } else {
            /* Match: length = value - 253 (minimum 3) */
            int length = value - 253;
            
            /* Decode distance */
            int distance = ac_decode_symbol(ctx, ctx->dist_freq, DIST_SYMBOLS,
                                            find_dist_symbol);
            
            /* Calculate source position in window */
            uint16_t src_pos = (ctx->window_pos - distance - 1) & WINDOW_MASK;
            
            /* Copy matched bytes */
            for (size_t i = 0; i < length; i++) {
                uint8_t byte = ctx->window[(src_pos + i) & WINDOW_MASK];
                uft_rtc_error_t err = emit_byte(ctx, byte);
                if (err != UFT_RTC_OK) return err;
            }
            decoded += length;
        }
        
        /* Update model */
        update_litlen_model(ctx, symbol);
    }
    
    return UFT_RTC_OK;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

uint32_t uft_rtc_get_output_size(const uint8_t *src, size_t src_len)
{
    if (!src || src_len < 4) return 0;
    
    /* Read 32-bit little-endian size */
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

bool uft_rtc_is_compressed(const uint8_t *src, size_t src_len)
{
    if (!src || src_len < 8) return false;
    
    uint32_t size = uft_rtc_get_output_size(src, src_len);
    
    /* Sanity checks */
    if (size == 0 || size > 16 * 1024 * 1024) return false;  /* Max 16MB */
    if (size < src_len) return false;  /* Must expand */
    
    /* Check compression ratio is reasonable (1.1x to 10x) */
    if ((size_t)size < src_len + src_len / 10) return false;
    if ((size_t)size > src_len * 10) return false;
    
    return true;
}

uft_rtc_error_t uft_rtc_decompress(uft_rtc_ctx_t *ctx,
                                    const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t *dst_len)
{
    if (!ctx || !src || !dst_len) {
        return UFT_RTC_ERR_NULLPTR;
    }
    
    if (src_len < 4) {
        return UFT_RTC_ERR_TRUNCATED;
    }
    
    /* Read output size from header */
    uint32_t out_size = uft_rtc_get_output_size(src, src_len);
    if (out_size == 0) {
        return UFT_RTC_ERR_INVALID_SIZE;
    }
    
    /* Query mode: just return size */
    if (dst == NULL) {
        *dst_len = out_size;
        return UFT_RTC_OK;
    }
    
    /* Check buffer size */
    if (*dst_len < out_size) {
        *dst_len = out_size;
        return UFT_RTC_ERR_OVERFLOW;
    }
    
    /* Set up context */
    uft_rtc_ctx_reset(ctx);
    ctx->in_buf = src;
    ctx->in_size = src_len;
    ctx->in_pos = 4;  /* Skip size header */
    ctx->out_buf = dst;
    ctx->out_size = *dst_len;
    ctx->out_pos = 0;
    ctx->out_target = out_size;
    
    /* Decompress */
    uft_rtc_error_t err = decompress_stream(ctx);
    
    *dst_len = ctx->out_pos;
    return err;
}

uft_rtc_error_t uft_rtc_decompress_alloc(const uint8_t *src, size_t src_len,
                                          uint8_t **dst_out, size_t *dst_len_out)
{
    if (!src || !dst_out || !dst_len_out) {
        return UFT_RTC_ERR_NULLPTR;
    }
    
    *dst_out = NULL;
    *dst_len_out = 0;
    
    /* Get output size */
    uint32_t out_size = uft_rtc_get_output_size(src, src_len);
    if (out_size == 0) {
        return UFT_RTC_ERR_INVALID_SIZE;
    }
    
    /* Allocate output buffer */
    uint8_t *dst = malloc(out_size);
    if (!dst) {
        return UFT_RTC_ERR_ALLOC;
    }
    
    /* Create temporary context */
    uft_rtc_ctx_t *ctx = uft_rtc_ctx_create();
    if (!ctx) {
        free(dst);
        return UFT_RTC_ERR_ALLOC;
    }
    
    /* Decompress */
    size_t dst_len = out_size;
    uft_rtc_error_t err = uft_rtc_decompress(ctx, src, src_len, dst, &dst_len);
    
    uft_rtc_ctx_destroy(ctx);
    
    if (err != UFT_RTC_OK) {
        free(dst);
        return err;
    }
    
    *dst_out = dst;
    *dst_len_out = dst_len;
    return UFT_RTC_OK;
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_RTC_TEST

#include <stdio.h>

int main(void)
{
    printf("UFT RTC Decompressor Self-Test\n");
    printf("==============================\n\n");
    
    /* Test data would need actual RTC-compressed samples */
    printf("Note: Real RTC-compressed test data required for full test.\n");
    printf("Context create/destroy test: ");
    
    uft_rtc_ctx_t *ctx = uft_rtc_ctx_create();
    if (ctx) {
        uft_rtc_ctx_destroy(ctx);
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        return 1;
    }
    
    /* Test size detection with fake header */
    uint8_t fake_header[] = {0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint32_t size = uft_rtc_get_output_size(fake_header, sizeof(fake_header));
    printf("Size detection test (0x1000 = %u): %s\n", 
           size, size == 0x1000 ? "PASS" : "FAIL");
    
    return 0;
}

#endif /* UFT_RTC_TEST */
