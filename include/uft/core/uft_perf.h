/**
 * @file uft_perf.h
 * @brief Performance Profiling Utilities (P3-001)
 * 
 * Provides lightweight profiling hooks for identifying hotspots.
 * Enable with -DUFT_PERF_PROFILE=1
 * 
 * Usage:
 *   UFT_PERF_BEGIN("decode_track");
 *   // ... decode logic ...
 *   UFT_PERF_END("decode_track");
 *   
 *   uft_perf_report(stdout);
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_PERF_H
#define UFT_PERF_H

/* Enable POSIX features for clock_gettime */
#if !defined(_POSIX_C_SOURCE) && !defined(_WIN32)
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
    #include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_PERF_PROFILE
#define UFT_PERF_PROFILE 0  /* Disabled by default */
#endif

#define UFT_PERF_MAX_COUNTERS 64
#define UFT_PERF_NAME_LEN     32

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char        name[UFT_PERF_NAME_LEN];
    uint64_t    total_ns;       /**< Total time in nanoseconds */
    uint64_t    call_count;     /**< Number of calls */
    uint64_t    min_ns;         /**< Minimum call time */
    uint64_t    max_ns;         /**< Maximum call time */
    uint64_t    start_ns;       /**< Current measurement start */
} uft_perf_counter_t;

typedef struct {
    uft_perf_counter_t  counters[UFT_PERF_MAX_COUNTERS];
    int                 count;
    bool                enabled;
} uft_perf_context_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Time Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static inline uint64_t uft_perf_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Global Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if UFT_PERF_PROFILE

static uft_perf_context_t g_uft_perf_ctx = {0};

static inline int uft_perf_find_or_create(const char *name) {
    /* Find existing */
    for (int i = 0; i < g_uft_perf_ctx.count; i++) {
        if (strcmp(g_uft_perf_ctx.counters[i].name, name) == 0) {
            return i;
        }
    }
    /* Create new */
    if (g_uft_perf_ctx.count < UFT_PERF_MAX_COUNTERS) {
        int idx = g_uft_perf_ctx.count++;
        strncpy(g_uft_perf_ctx.counters[idx].name, name, UFT_PERF_NAME_LEN - 1);
        g_uft_perf_ctx.counters[idx].min_ns = UINT64_MAX;
        return idx;
    }
    return -1;
}

static inline void uft_perf_begin_impl(const char *name) {
    if (!g_uft_perf_ctx.enabled) return;
    int idx = uft_perf_find_or_create(name);
    if (idx >= 0) {
        g_uft_perf_ctx.counters[idx].start_ns = uft_perf_now_ns();
    }
}

static inline void uft_perf_end_impl(const char *name) {
    if (!g_uft_perf_ctx.enabled) return;
    uint64_t end = uft_perf_now_ns();
    int idx = uft_perf_find_or_create(name);
    if (idx >= 0) {
        uft_perf_counter_t *c = &g_uft_perf_ctx.counters[idx];
        uint64_t elapsed = end - c->start_ns;
        c->total_ns += elapsed;
        c->call_count++;
        if (elapsed < c->min_ns) c->min_ns = elapsed;
        if (elapsed > c->max_ns) c->max_ns = elapsed;
    }
}

#define UFT_PERF_BEGIN(name) uft_perf_begin_impl(name)
#define UFT_PERF_END(name)   uft_perf_end_impl(name)

#else /* UFT_PERF_PROFILE disabled */

#define UFT_PERF_BEGIN(name) ((void)0)
#define UFT_PERF_END(name)   ((void)0)

#endif /* UFT_PERF_PROFILE */

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Enable/disable profiling
 */
static inline void uft_perf_enable(bool enable) {
#if UFT_PERF_PROFILE
    g_uft_perf_ctx.enabled = enable;
#else
    (void)enable;
#endif
}

/**
 * @brief Reset all counters
 */
static inline void uft_perf_reset(void) {
#if UFT_PERF_PROFILE
    memset(&g_uft_perf_ctx, 0, sizeof(g_uft_perf_ctx));
#endif
}

/**
 * @brief Print performance report
 */
static inline void uft_perf_report(FILE *out) {
#if UFT_PERF_PROFILE
    if (!out) return;
    
    fprintf(out, "\n╔════════════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║                    UFT PERFORMANCE REPORT                      ║\n");
    fprintf(out, "╠════════════════════════════════════════════════════════════════╣\n");
    fprintf(out, "║ %-20s %10s %10s %10s %10s ║\n", 
            "Function", "Calls", "Total(ms)", "Avg(µs)", "Max(µs)");
    fprintf(out, "╠════════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < g_uft_perf_ctx.count; i++) {
        uft_perf_counter_t *c = &g_uft_perf_ctx.counters[i];
        if (c->call_count == 0) continue;
        
        double total_ms = (double)c->total_ns / 1000000.0;
        double avg_us = (double)c->total_ns / (double)c->call_count / 1000.0;
        double max_us = (double)c->max_ns / 1000.0;
        
        fprintf(out, "║ %-20s %10lu %10.2f %10.2f %10.2f ║\n",
                c->name, (unsigned long)c->call_count, total_ms, avg_us, max_us);
    }
    
    fprintf(out, "╚════════════════════════════════════════════════════════════════╝\n");
#else
    if (out) {
        fprintf(out, "Performance profiling disabled. Rebuild with -DUFT_PERF_PROFILE=1\n");
    }
#endif
}

/**
 * @brief RAII-style scoped timer (C++ only)
 */
#ifdef __cplusplus
class UftPerfScope {
    const char *m_name;
public:
    UftPerfScope(const char *name) : m_name(name) { UFT_PERF_BEGIN(m_name); }
    ~UftPerfScope() { UFT_PERF_END(m_name); }
};
#define UFT_PERF_SCOPE(name) UftPerfScope _perf_scope_##__LINE__(name)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_PERF_H */
