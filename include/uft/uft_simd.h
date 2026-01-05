/**
 * @file uft_simd.h
 * @brief SIMD Optimization Framework - Runtime CPU Detection
 * 
 * FEATURES:
 * - Runtime CPU feature detection (SSE2, AVX2, AVX-512)
 * - Automatic dispatcher to fastest available implementation
 * - Fallback to scalar code if no SIMD available
 * - Cross-platform (x86-64, ARM NEON future support)
 * 
 * PERFORMANCE TARGETS:
 * - MFM Decode: 80 MB/s (scalar) → 400+ MB/s (AVX2)
 * - GCR Decode: 60 MB/s (scalar) → 350+ MB/s (AVX2)
 * 
 * USAGE:
 * ```c
 * // Automatic - uses best available
 * uft_mfm_decode_flux(flux_data, count, output);
 * 
 * // Manual selection (for benchmarking)
 * uft_mfm_decode_flux_scalar(flux_data, count, output);
 * uft_mfm_decode_flux_sse2(flux_data, count, output);
 * uft_mfm_decode_flux_avx2(flux_data, count, output);
 * ```
 */

#ifndef UFT_SIMD_H
#define UFT_SIMD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CPU FEATURE FLAGS
 * ============================================================================= */

typedef enum {
    UFT_CPU_SSE2      = (1 << 0),  /* SSE2 (Intel Pentium 4+, AMD K8+) */
    UFT_CPU_SSE3      = (1 << 1),  /* SSE3 */
    UFT_CPU_SSSE3     = (1 << 2),  /* SSSE3 */
    UFT_CPU_SSE41     = (1 << 3),  /* SSE4.1 */
    UFT_CPU_SSE42     = (1 << 4),  /* SSE4.2 */
    UFT_CPU_AVX       = (1 << 5),  /* AVX */
    UFT_CPU_AVX2      = (1 << 6),  /* AVX2 (Haswell+) */
    UFT_CPU_AVX512F   = (1 << 7),  /* AVX-512 Foundation */
    UFT_CPU_AVX512BW  = (1 << 8),  /* AVX-512 Byte/Word */
    UFT_CPU_FMA       = (1 << 9),  /* Fused Multiply-Add */
    UFT_CPU_POPCNT    = (1 << 10), /* Population Count */
    UFT_CPU_BMI1      = (1 << 11), /* Bit Manipulation 1 */
    UFT_CPU_BMI2      = (1 << 12), /* Bit Manipulation 2 */
    UFT_CPU_LZCNT     = (1 << 13), /* Leading Zero Count */
    
    /* ARM (future) */
    UFT_CPU_NEON      = (1 << 20), /* ARM NEON */
    UFT_CPU_SVE       = (1 << 21), /* ARM SVE */
} uft_cpu_features_t;

/* =============================================================================
 * CPU INFORMATION
 * ============================================================================= */

typedef struct {
    char vendor[13];         /* CPU vendor string (e.g., "GenuineIntel") */
    char brand[49];          /* CPU brand (e.g., "Intel Core i7-9700K") */
    
    uint32_t features;       /* Bitmask of uft_cpu_features_t */
    
    int family;              /* CPU family */
    int model;               /* CPU model */
    int stepping;            /* CPU stepping */
    
    int logical_cpus;        /* Number of logical CPUs (threads) */
    int physical_cpus;       /* Number of physical cores */
    int logical_cores;       /* Alias for logical_cpus */
    int physical_cores;      /* Alias for physical_cpus */
    
    int impl_level;          /* Best implementation level (uft_impl_level_t) */
    
    /* Cache information */
    size_t l1d_cache_size;   /* L1 data cache (bytes) */
    size_t l1i_cache_size;   /* L1 instruction cache (bytes) */
    size_t l2_cache_size;    /* L2 cache (bytes) */
    size_t l3_cache_size;    /* L3 cache (bytes) */
} uft_cpu_info_t;

/**
 * @brief Implementation level for dispatch
 */
typedef enum {
    UFT_IMPL_SCALAR = 0,     /* Pure scalar (fallback) */
    UFT_IMPL_SSE2   = 1,     /* SSE2 implementation */
    UFT_IMPL_AVX2   = 2,     /* AVX2 implementation */
    UFT_IMPL_AVX512 = 3,     /* AVX-512 implementation */
    UFT_IMPL_NEON   = 10     /* ARM NEON implementation */
} uft_impl_level_t;

/** @brief Alias for compatibility */
typedef uft_cpu_features_t uft_cpu_feature_t;

/**
 * @brief Detect CPU features (call once at startup)
 * @return CPU information structure
 */
uft_cpu_info_t uft_cpu_detect(void);

/**
 * @brief Check if specific feature is available
 */
bool uft_cpu_has_feature(uft_cpu_features_t feature);

/**
 * @brief Get detected CPU info (cached)
 */
const uft_cpu_info_t* uft_cpu_get_info(void);

/**
 * @brief Print CPU information to stdout
 */
void uft_cpu_print_info(void);

/* =============================================================================
 * SIMD-OPTIMIZED FUNCTIONS - MFM DECODING
 * ============================================================================= */

/**
 * @brief MFM Decode (automatic dispatcher)
 * Selects best available implementation at runtime
 */
size_t uft_mfm_decode_flux(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits
);

/**
 * @brief MFM Decode - Scalar implementation
 * Baseline, always available
 */
size_t uft_mfm_decode_flux_scalar(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits
);

/**
 * @brief MFM Decode - SSE2 implementation
 * ~3-5x faster than scalar
 * Requires: SSE2 (Pentium 4+)
 */
size_t uft_mfm_decode_flux_sse2(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits
);

/**
 * @brief MFM Decode - AVX2 implementation
 * ~8-10x faster than scalar
 * Requires: AVX2 (Haswell 2013+)
 */
size_t uft_mfm_decode_flux_avx2(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits
);

/**
 * @brief MFM Decode - AVX-512 implementation
 * ~15-20x faster than scalar
 * Requires: AVX-512F + AVX-512BW (Skylake-X 2017+)
 */
size_t uft_mfm_decode_flux_avx512(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits
);

/**
 * @brief MFM Encode - AVX-512 implementation
 * @param input_data Raw data bytes
 * @param input_bytes Number of input bytes
 * @param output_mfm Output MFM bits (2x input size)
 * @return Number of MFM bits written
 */
size_t uft_mfm_encode_avx512(
    const uint8_t *input_data,
    size_t input_bytes,
    uint8_t *output_mfm
);

/**
 * @brief Find MFM sync patterns using AVX-512
 */
size_t uft_mfm_find_sync_avx512(
    const uint8_t *mfm_data,
    size_t data_size,
    uint16_t pattern,
    size_t *positions,
    size_t max_positions
);

/**
 * @brief CRC-16-CCITT using AVX-512 (with prefetch)
 */
uint16_t uft_crc16_ccitt_avx512(const uint8_t *data, size_t length);

/* =============================================================================
 * SIMD-OPTIMIZED FUNCTIONS - GCR DECODING
 * ============================================================================= */

/**
 * @brief GCR 5-to-4 Decode (automatic dispatcher)
 */
size_t uft_gcr_decode_5to4(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bytes
);

size_t uft_gcr_decode_5to4_scalar(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bytes
);

size_t uft_gcr_decode_5to4_sse2(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bytes
);

size_t uft_gcr_decode_5to4_avx2(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bytes
);

/* =============================================================================
 * SIMD-OPTIMIZED FUNCTIONS - BIT MANIPULATION
 * ============================================================================= */

/**
 * @brief Count set bits in array (POPCNT-optimized)
 */
size_t uft_popcount_array(const uint8_t *data, size_t length);

/**
 * @brief Find first set bit (BSF/TZCNT)
 */
int uft_find_first_set_bit(uint64_t value);

/**
 * @brief Byte-swap array (BSWAP)
 */
void uft_byteswap_array(uint8_t *data, size_t length);

/* =============================================================================
 * BENCHMARKING
 * ============================================================================= */

/**
 * @brief Benchmark all MFM implementations
 * @param flux_data Test data
 * @param count Number of transitions
 * @param iterations Benchmark iterations
 */
void uft_benchmark_mfm(const uint64_t *flux_data, size_t count, int iterations);

/**
 * @brief Benchmark all GCR implementations
 */
void uft_benchmark_gcr(const uint64_t *flux_data, size_t count, int iterations);

/* =============================================================================
 * INTERNAL HELPERS (used by implementations)
 * ============================================================================= */

/**
 * @brief Aligned load (16-byte boundary for SSE2)
 */
#if defined(__SSE2__)
    #include <emmintrin.h>
    #define UFT_LOAD_ALIGNED_128(ptr) _mm_load_si128((__m128i const*)(ptr))
    #define UFT_STORE_ALIGNED_128(ptr, val) _mm_store_si128((__m128i*)(ptr), val)
#endif

/**
 * @brief Aligned load (32-byte boundary for AVX2)
 */
#if defined(__AVX2__)
    #include <immintrin.h>
    #define UFT_LOAD_ALIGNED_256(ptr) _mm256_load_si256((__m256i const*)(ptr))
    #define UFT_STORE_ALIGNED_256(ptr, val) _mm256_store_si256((__m256i*)(ptr), val)
#endif

/* =============================================================================
 * COMPILER HINTS (legacy - now provided by uft_compiler.h)
 * These are kept as guards only for backward compatibility
 * ============================================================================= */

#ifndef UFT_LIKELY
    #if defined(__GNUC__) || defined(__clang__)
        #define UFT_LIKELY(x)   __builtin_expect(!!(x), 1)
        #define UFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
        #define UFT_ALIGNED(n)  __attribute__((aligned(n)))
    #else
        #define UFT_LIKELY(x)   (x)
        #define UFT_UNLIKELY(x) (x)
        #define UFT_ALIGNED(n)
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_SIMD_H */
