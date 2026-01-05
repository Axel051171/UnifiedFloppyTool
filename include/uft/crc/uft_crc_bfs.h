/**
 * @file uft_crc_bfs.h
 * @brief CRC Brute-Force Search API
 * 
 * Based on crcbfs.pl v1.03 by Gregory Cook
 * 
 * Usage Example:
 * @code
 * // Samples are message+CRC (CRC appended at end)
 * const uint8_t *samples[] = {sample1, sample2, sample3};
 * size_t lens[] = {sample1_len, sample2_len, sample3_len};
 * 
 * // Find polynomial
 * uint64_t polys[8];
 * int found = uft_crc_bfs_find_poly(16, samples, lens, 3, 0, polys, 8, NULL, NULL);
 * 
 * // Or complete detection
 * uft_crc_bfs_result_t result;
 * uft_crc_bfs_detect(16, samples, lens, 3, &result);
 * @endcode
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#ifndef UFT_CRC_BFS_H
#define UFT_CRC_BFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Result Structure
 *===========================================================================*/

/**
 * @brief CRC brute-force search result
 */
typedef struct {
    bool     found;         /**< Parameters successfully found */
    uint8_t  width;         /**< CRC width in bits */
    uint64_t poly;          /**< Polynomial (without implicit x^n) */
    uint64_t init;          /**< Initial value */
    uint64_t xorout;        /**< Final XOR value */
    bool     reflected;     /**< Input/output reflection */
    float    confidence;    /**< Detection confidence (0-100%) */
    int      preset_idx;    /**< Index in preset table (-1 if custom) */
} uft_crc_bfs_result_t;

/*===========================================================================
 * Polynomial Search
 *===========================================================================*/

/**
 * @brief Find CRC polynomial by brute-force search
 * 
 * Given two or more message+CRC samples, finds polynomials that
 * produce consistent CRC values.
 * 
 * Algorithm:
 * 1. XOR pairs of samples to create "work strings"
 * 2. For each candidate polynomial, check if it divides
 *    all work strings evenly (remainder = 0)
 * 
 * @param width CRC width in bits (3-64)
 * @param samples Array of pointers to message+CRC data
 * @param lens Length of each sample in bytes
 * @param num_samples Number of samples (minimum 2)
 * @param xorout Known/guessed XOR-out value (use 0 if unknown)
 * @param results Output array for found polynomials
 * @param max_results Maximum results to store
 * @param progress Optional progress callback
 * @param user_data User data for callback
 * @return Number of polynomials found, or -1 on error
 * 
 * @note For best results, provide samples of different lengths.
 *       Same-length samples can only determine polynomial, not init/xorout.
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
                           void *user_data);

/*===========================================================================
 * Init/XorOut Detection
 *===========================================================================*/

/**
 * @brief Find init and xorout given polynomial
 * 
 * Once the polynomial is known, this function finds the initial
 * register value (init) and final XOR value (xorout).
 * 
 * @param width CRC width in bits
 * @param poly Known polynomial
 * @param samples Array of pointers to message+CRC data
 * @param lens Length of each sample in bytes
 * @param num_samples Number of samples (minimum 2, must have different lengths)
 * @param init_out Output: initial value
 * @param xorout_out Output: XOR-out value
 * @return 0 on success, -1 on error
 * 
 * @note Requires samples of different lengths to distinguish init from xorout.
 */
int uft_crc_bfs_find_init(uint8_t width,
                           uint64_t poly,
                           const uint8_t **samples,
                           const size_t *lens,
                           size_t num_samples,
                           uint64_t *init_out,
                           uint64_t *xorout_out);

/*===========================================================================
 * Complete Detection
 *===========================================================================*/

/**
 * @brief Complete CRC parameter detection
 * 
 * Attempts to find all CRC parameters: polynomial, init, xorout, and reflection.
 * 
 * @param width CRC width in bits
 * @param samples Array of pointers to message+CRC data
 * @param lens Length of each sample in bytes
 * @param num_samples Number of samples
 * @param result Output result structure
 * @return 0 on success, -1 if no parameters found
 */
int uft_crc_bfs_detect(uint8_t width,
                        const uint8_t **samples,
                        const size_t *lens,
                        size_t num_samples,
                        uft_crc_bfs_result_t *result);

/*===========================================================================
 * Utilities
 *===========================================================================*/

/**
 * @brief Format result as string
 * 
 * @param result Result to format
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null), or -1 on error
 */
int uft_crc_bfs_result_to_string(const uft_crc_bfs_result_t *result,
                                  char *buffer, size_t size);

/**
 * @brief Match detected parameters against preset database
 * 
 * @param width CRC width
 * @param poly Polynomial
 * @param init Initial value
 * @param xorout XOR-out value
 * @param reflected Reflection flag
 * @return Index in preset table, or -1 if not found
 */
int uft_crc_match_preset(uint8_t width, uint64_t poly,
                          uint64_t init, uint64_t xorout,
                          bool reflected);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_BFS_H */
