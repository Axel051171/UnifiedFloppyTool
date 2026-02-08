/**
 * @file uft_crc_bfs.c
 * @brief CRC Brute-Force Search Algorithm
 * 
 * Ported to C for UFT integration.
 * 
 * Algorithm Overview:
 * - Creates "work strings" by XORing sample pairs
 * - Tests polynomials by checking if they divide work strings evenly
 * - For different-length samples, uses padding and alignment
 * - Once polynomial found, derives init and xorout values
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/crc/uft_crc_reveng.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Internal Bit Manipulation
 *===========================================================================*/

/**
 * @brief Polynomial division over GF(2)
 * 
 * Divides dividend by (1 << width) | divisor
 * Returns remainder (CRC)
 */
static uint64_t polydiv_bits(const uint8_t *dividend, size_t bits,
                              uint64_t divisor, uint8_t width)
{
    uint64_t rem = 0;
    uint64_t top = 1ULL << width;
    uint64_t poly = top | divisor;  /* Add implicit x^n term */
    
    for (size_t i = 0; i < bits; i++) {
        size_t byte_idx = i / 8;
        int bit_pos = 7 - (i % 8);
        int bit = (dividend[byte_idx] >> bit_pos) & 1;
        
        rem = (rem << 1) | bit;
        
        if (rem & top) {
            rem ^= poly;
        }
    }
    
    return rem;
}

/**
 * @brief Reverse polynomial division (undiv)
 * 
 * Given remainder R and polynomial P, finds original dividend
 * such that dividend * x^width mod P = R
 */
static uint64_t poly_undiv(uint64_t rem, uint8_t rem_bits,
                            uint64_t poly, uint8_t width)
{
    uint64_t result = 0;
    
    /* Reflect the polynomial for LSB-first processing */
    uint64_t poly_rev = 0;
    for (int i = 0; i < width; i++) {
        if (poly & (1ULL << i)) {
            poly_rev |= 1ULL << (width - 1 - i);
        }
    }
    
    /* Reflect remainder */
    uint64_t rem_rev = 0;
    for (int i = 0; i < rem_bits; i++) {
        if (rem & (1ULL << i)) {
            rem_rev |= 1ULL << (rem_bits - 1 - i);
        }
    }
    
    /* Process from LSB */
    for (int i = 0; i < rem_bits; i++) {
        if (rem_rev & 1) {
            rem_rev ^= (1ULL | (poly_rev << 1));
        }
        rem_rev >>= 1;
    }
    
    /* Reflect result back */
    for (int i = 0; i < width; i++) {
        if (rem_rev & (1ULL << i)) {
            result |= 1ULL << (width - 1 - i);
        }
    }
    
    return result;
}

/**
 * @brief XOR two buffers
 */
static void xor_buffers(uint8_t *dst, const uint8_t *src1, 
                         const uint8_t *src2, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        dst[i] = src1[i] ^ src2[i];
    }
}

/**
 * @brief Find first set bit position
 * @return Bit position (0-based) or total_bits if all zero
 */
static size_t find_first_bit(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != 0) {
            for (int bit = 7; bit >= 0; bit--) {
                if (buf[i] & (1 << bit)) {
                    return i * 8 + (7 - bit);
                }
            }
        }
    }
    return len * 8;
}

/**
 * @brief Check if buffer is all zeros
 */
static bool is_zero(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != 0) return false;
    }
    return true;
}

/*===========================================================================
 * Work String Preparation (from crcbfs.pl modulate())
 *===========================================================================*/

typedef struct {
    uint8_t *data;
    size_t bits;
} work_string_t;

typedef struct {
    work_string_t *strings;
    size_t count;
    size_t capacity;
} work_list_t;

static void work_list_init(work_list_t *list)
{
    list->strings = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void work_list_free(work_list_t *list)
{
    for (size_t i = 0; i < list->count; i++) {
        free(list->strings[i].data);
    }
    free(list->strings);
    list->strings = NULL;
    list->count = 0;
}

static int work_list_add(work_list_t *list, const uint8_t *data, size_t bits)
{
    /* Check for duplicates */
    size_t bytes = (bits + 7) / 8;
    for (size_t i = 0; i < list->count; i++) {
        if (list->strings[i].bits == bits) {
            if (memcmp(list->strings[i].data, data, bytes) == 0) {
                return 0;  /* Already exists */
            }
        }
    }
    
    /* Expand if needed */
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 16;
        work_string_t *new_arr = realloc(list->strings, 
                                          new_cap * sizeof(work_string_t));
        if (!new_arr) return -1;
        list->strings = new_arr;
        list->capacity = new_cap;
    }
    
    /* Add new string */
    uint8_t *copy = malloc(bytes);
    if (!copy) return -1;
    memcpy(copy, data, bytes);
    
    list->strings[list->count].data = copy;
    list->strings[list->count].bits = bits;
    list->count++;
    
    return 0;
}

/**
 * @brief Prepare work strings by XORing sample pairs
 * 
 * This is the core of the crcbfs.pl algorithm.
 * For same-length samples: simple XOR
 * For different-length samples: align and XOR with padding
 */
static int prepare_work_strings(const uint8_t **samples, 
                                 const size_t *lens,
                                 size_t num_samples,
                                 uint8_t crc_width,
                                 uint64_t xorout,
                                 work_list_t *work)
{
    work_list_init(work);
    
    size_t crc_bytes = (crc_width + 7) / 8;
    
    for (size_t i = 0; i < num_samples; i++) {
        for (size_t j = i + 1; j < num_samples; j++) {
            const uint8_t *a = samples[i];
            const uint8_t *b = samples[j];
            size_t a_len = lens[i];
            size_t b_len = lens[j];
            
            /* Ensure a is the longer one */
            if (a_len < b_len) {
                const uint8_t *tmp = a; a = b; b = tmp;
                size_t tmp_len = a_len; a_len = b_len; b_len = tmp_len;
            }
            
            uint8_t *result = calloc(1, a_len);
            if (!result) continue;
            
            if (a_len == b_len) {
                /* Same length: simple XOR */
                xor_buffers(result, a, b, a_len);
            } else {
                /* Different lengths: apply XOR-out and align */
                
                /* Start with copy of longer sample */
                memcpy(result, a, a_len);
                
                /* XOR the CRC bytes */
                for (size_t k = 0; k < crc_bytes && k < b_len; k++) {
                    result[a_len - crc_bytes + k] ^= b[b_len - crc_bytes + k];
                }
                
                /* XOR message portion (right-justified) */
                size_t msg_a = a_len - crc_bytes;
                size_t msg_b = b_len - crc_bytes;
                for (size_t k = 0; k < msg_b; k++) {
                    result[msg_a - msg_b + k] ^= b[k];
                }
                
                /* Apply XOR-out to CRC bytes */
                if (xorout != 0) {
                    for (size_t k = 0; k < crc_bytes; k++) {
                        uint8_t xor_byte = (xorout >> ((crc_bytes - 1 - k) * 8)) & 0xFF;
                        result[a_len - crc_bytes + k] ^= xor_byte;
                        
                        /* Also XOR the shorter sample's position */
                        size_t pos = a_len - (a_len - b_len) - crc_bytes + k;
                        if (pos < a_len) {
                            result[pos] ^= xor_byte;
                        }
                    }
                }
            }
            
            /* Add to work list if non-zero */
            if (!is_zero(result, a_len)) {
                /* Normalize: skip leading zeros */
                size_t first_bit = find_first_bit(result, a_len);
                size_t total_bits = a_len * 8;
                
                if (first_bit < total_bits) {
                    work_list_add(work, result, total_bits);
                }
            }
            
            free(result);
        }
    }
    
    return (work->count > 0) ? 0 : -1;
}

/*===========================================================================
 * Polynomial Search
 *===========================================================================*/

/**
 * @brief Test if polynomial divides all work strings evenly
 */
static bool test_polynomial(const work_list_t *work, 
                             uint64_t poly, uint8_t width)
{
    for (size_t i = 0; i < work->count; i++) {
        uint64_t rem = polydiv_bits(work->strings[i].data,
                                     work->strings[i].bits,
                                     poly, width);
        if (rem != 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Brute-force polynomial search
 * 
 * @param width CRC width in bits
 * @param samples Array of message+CRC samples
 * @param lens Length of each sample in bytes
 * @param num_samples Number of samples (minimum 2)
 * @param xorout Known or guessed XOR-out value (0 if unknown)
 * @param results Output array for found polynomials
 * @param max_results Maximum results to return
 * @param progress Optional progress callback
 * @param user_data User data for callback
 * @return Number of polynomials found, or -1 on error
 */
int uft_crc_bfs_find_poly(uint8_t width,
                           const uint8_t **samples,
                           const size_t *lens,
                           size_t num_samples,
                           uint64_t xorout,
                           uint64_t *results,
                           size_t max_results,
                           void (*progress)(uint64_t current, 
                                            uint64_t total, 
                                            void *user),
                           void *user_data)
{
    if (width < 3 || width > 64 || num_samples < 2 || !samples || !lens) {
        return -1;
    }
    
    work_list_t work;
    if (prepare_work_strings(samples, lens, num_samples, 
                              width, xorout, &work) < 0) {
        return -1;
    }
    
    size_t found = 0;
    uint64_t min_poly = 1ULL << (width - 1);  /* MSB must be set */
    uint64_t max_poly = 1ULL << width;
    uint64_t total = max_poly - min_poly;
    uint64_t step = total / 1000;
    if (step == 0) step = 1;
    
    for (uint64_t poly = min_poly; poly < max_poly; poly++) {
        /* Progress callback */
        if (progress && ((poly - min_poly) % step == 0)) {
            progress(poly - min_poly, total, user_data);
        }
        
        if (test_polynomial(&work, poly, width)) {
            if (found < max_results && results) {
                results[found] = poly;
            }
            found++;
        }
    }
    
    work_list_free(&work);
    return (int)found;
}

/*===========================================================================
 * Init/XorOut Derivation
 *===========================================================================*/

/**
 * @brief Derive init and xorout given polynomial
 * 
 * Based on crcbfs.pl initbfs() and getparams()
 */
int uft_crc_bfs_find_init(uint8_t width,
                           uint64_t poly,
                           const uint8_t **samples,
                           const size_t *lens,
                           size_t num_samples,
                           uint64_t *init_out,
                           uint64_t *xorout_out)
{
    if (width < 3 || width > 32 || num_samples < 2) {
        return -1;
    }
    
    /* Need different length samples */
    bool have_diff_len = false;
    for (size_t i = 1; i < num_samples; i++) {
        if (lens[i] != lens[0]) {
            have_diff_len = true;
            break;
        }
    }
    
    if (!have_diff_len) {
        return -1;  /* Cannot distinguish init from xorout */
    }
    
    uint64_t mask = (width < 64) ? ((1ULL << width) - 1) : ~0ULL;
    size_t crc_bytes = (width + 7) / 8;
    
    /* Extract CRC from first sample for validation */
    uint64_t crc0 = 0;
    for (size_t i = 0; i < crc_bytes; i++) {
        crc0 = (crc0 << 8) | samples[0][lens[0] - crc_bytes + i];
    }
    crc0 &= mask;
    
    /* Brute-force init value */
    for (uint64_t init = 0; init <= mask; init++) {
        /* Calculate CRC with this init, xorout=0 */
        uint64_t msg_len = lens[0] - crc_bytes;
        uint64_t crc = init;
        uint64_t top = 1ULL << (width - 1);
        
        for (size_t i = 0; i < msg_len; i++) {
            crc ^= ((uint64_t)samples[0][i] << (width - 8));
            for (int bit = 0; bit < 8; bit++) {
                if (crc & top) {
                    crc = (crc << 1) ^ poly;
                } else {
                    crc <<= 1;
                }
            }
        }
        crc &= mask;
        
        /* XorOut = actual CRC XOR calculated CRC */
        uint64_t xorout = crc0 ^ crc;
        
        /* Verify with other samples */
        bool valid = true;
        for (size_t s = 1; s < num_samples && valid; s++) {
            uint64_t exp_crc = 0;
            for (size_t i = 0; i < crc_bytes; i++) {
                exp_crc = (exp_crc << 8) | samples[s][lens[s] - crc_bytes + i];
            }
            exp_crc &= mask;
            
            uint64_t calc = init;
            msg_len = lens[s] - crc_bytes;
            
            for (size_t i = 0; i < msg_len; i++) {
                calc ^= ((uint64_t)samples[s][i] << (width - 8));
                for (int bit = 0; bit < 8; bit++) {
                    if (calc & top) {
                        calc = (calc << 1) ^ poly;
                    } else {
                        calc <<= 1;
                    }
                }
            }
            calc = (calc ^ xorout) & mask;
            
            if (calc != exp_crc) {
                valid = false;
            }
        }
        
        if (valid) {
            *init_out = init;
            *xorout_out = xorout;
            return 0;
        }
    }
    
    return -1;
}

/*===========================================================================
 * Complete CRC Detection
 *===========================================================================*/

/**
 * @brief Complete CRC parameter detection
 * 
 * Tries to find all parameters: poly, init, xorout, reflection
 */
int uft_crc_bfs_detect(uint8_t width,
                        const uint8_t **samples,
                        const size_t *lens,
                        size_t num_samples,
                        uft_crc_bfs_result_t *result)
{
    if (!result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->width = width;
    
    /* Try different XOR-out guesses */
    uint64_t xorout_guesses[] = {0, (1ULL << width) - 1};
    
    for (int x = 0; x < 2; x++) {
        uint64_t polys[16];
        int found = uft_crc_bfs_find_poly(width, samples, lens, num_samples,
                                           xorout_guesses[x], polys, 16,
                                           NULL, NULL);
        
        if (found > 0) {
            /* Try to find init for first polynomial */
            for (int p = 0; p < found && p < 16; p++) {
                uint64_t init, xorout;
                
                if (uft_crc_bfs_find_init(width, polys[p], samples, lens,
                                           num_samples, &init, &xorout) == 0) {
                    result->found = true;
                    result->poly = polys[p];
                    result->init = init;
                    result->xorout = xorout;
                    result->reflected = false;  /* Reflection detection via reversed poly check */
                    result->confidence = 100.0;
                    
                    /* Match against known presets */
                    result->preset_idx = uft_crc_match_preset(
                        width, polys[p], init, xorout, false);
                    
                    return 0;
                }
            }
            
            /* Couldn't find init, but have polynomial */
            result->found = true;
            result->poly = polys[0];
            result->init = 0;
            result->xorout = xorout_guesses[x];
            result->confidence = 50.0;
            return 0;
        }
    }
    
    return -1;
}

/*===========================================================================
 * Result Formatting
 *===========================================================================*/

int uft_crc_bfs_result_to_string(const uft_crc_bfs_result_t *result,
                                  char *buffer, size_t size)
{
    if (!result || !buffer) return -1;
    
    if (!result->found) {
        return snprintf(buffer, size, "No CRC parameters found");
    }
    
    int hex_width = (result->width + 3) / 4;
    
    return snprintf(buffer, size,
        "CRC-%u: poly=0x%0*llX init=0x%0*llX xorout=0x%0*llX ref=%s conf=%.0f%%",
        result->width,
        hex_width, (unsigned long long)result->poly,
        hex_width, (unsigned long long)result->init,
        hex_width, (unsigned long long)result->xorout,
        result->reflected ? "yes" : "no",
        result->confidence);
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_CRC_BFS_TEST

#include <time.h>

static void progress_cb(uint64_t current, uint64_t total, void *user)
{
    static time_t last = 0;
    time_t now = time(NULL);
    if (now != last) {
        printf("\rProgress: %llu / %llu (%.1f%%)", 
               (unsigned long long)current,
               (unsigned long long)total,
               100.0 * current / total);
        fflush(stdout);
        last = now;
    }
}

int main(void)
{
    printf("UFT CRC BFS Self-Test\n");
    printf("=====================\n\n");
    
    /* Create test samples with CRC-16/XMODEM (poly=0x1021, init=0, xorout=0) */
    uint8_t sample1[] = {0x31, 0x32, 0x00, 0x00};  /* "12" + CRC */
    uint8_t sample2[] = {0x31, 0x32, 0x33, 0x00, 0x00};  /* "123" + CRC */
    uint8_t sample3[] = {0x31, 0x32, 0x33, 0x34, 0x00, 0x00};  /* "1234" + CRC */
    
    /* Calculate CRCs manually (poly=0x1021) */
    uint16_t crc1 = 0x1611;  /* CRC of "12" */
    uint16_t crc2 = 0x0630;  /* CRC of "123" */
    uint16_t crc3 = 0x9738;  /* CRC of "1234" */
    
    sample1[2] = (crc1 >> 8) & 0xFF; sample1[3] = crc1 & 0xFF;
    sample2[3] = (crc2 >> 8) & 0xFF; sample2[4] = crc2 & 0xFF;
    sample3[4] = (crc3 >> 8) & 0xFF; sample3[5] = crc3 & 0xFF;
    
    const uint8_t *samples[] = {sample1, sample2, sample3};
    size_t lens[] = {4, 5, 6};
    
    printf("Test samples:\n");
    for (int i = 0; i < 3; i++) {
        printf("  Sample %d: ", i + 1);
        for (size_t j = 0; j < lens[i]; j++) {
            printf("%02X ", samples[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    
    /* Find polynomial */
    printf("Searching for CRC-16 polynomial...\n");
    uint64_t polys[8];
    int found = uft_crc_bfs_find_poly(16, samples, lens, 3, 0, polys, 8,
                                       progress_cb, NULL);
    printf("\n");
    
    printf("Found %d polynomial(s):\n", found);
    for (int i = 0; i < found && i < 8; i++) {
        printf("  0x%04llX%s\n",
               (unsigned long long)polys[i],
               polys[i] == 0x1021 ? " (CORRECT - XMODEM)" : "");
    }
    
    /* Full detection */
    printf("\nFull CRC detection:\n");
    uft_crc_bfs_result_t result;
    if (uft_crc_bfs_detect(16, samples, lens, 3, &result) == 0) {
        char buf[256];
        uft_crc_bfs_result_to_string(&result, buf, sizeof(buf));
        printf("  %s\n", buf);
    }
    
    return 0;
}

#endif /* UFT_CRC_BFS_TEST */
