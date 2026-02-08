/**
 * @file uft_openmp.h
 * @brief OpenMP compatibility layer with fallback
 * 
 * Include this header instead of <omp.h> directly.
 * Provides stub implementations when OpenMP is not available.
 */
#ifndef UFT_OPENMP_H
#define UFT_OPENMP_H

#if defined(_OPENMP) && !defined(UFT_NO_OPENMP)

/* OpenMP available - use it */
#include <omp.h>
#define UFT_HAS_OPENMP 1
#define UFT_PARALLEL_FOR _Pragma("omp parallel for")
#define UFT_CRITICAL _Pragma("omp critical")

#else

/* OpenMP not available - provide stubs */
#define UFT_HAS_OPENMP 0
#define UFT_PARALLEL_FOR
#define UFT_CRITICAL

/* Stub functions */
static inline int omp_get_thread_num(void) { return 0; }
static inline int omp_get_num_threads(void) { return 1; }
static inline int omp_get_max_threads(void) { return 1; }
static inline void omp_set_num_threads(int n) { (void)n; }
static inline double omp_get_wtime(void) { return 0.0; }

#endif /* _OPENMP */

#endif /* UFT_OPENMP_H */
