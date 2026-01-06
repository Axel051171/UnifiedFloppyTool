/**
 * @file uft_simd.c
 * @brief SIMD Infrastructure Implementation - CPU Detection & Dispatching
 */

#include "uft/uft_simd.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>  /* For sysconf() */
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define UFT_ARCH_X86
#endif

#ifdef UFT_ARCH_X86
    #if defined(_MSC_VER)
        #include <intrin.h>
    #elif defined(__GNUC__) || defined(__clang__)
        #include <cpuid.h>
    #endif
#endif

/* =============================================================================
 * GLOBAL CPU INFO (CACHED)
 * ============================================================================= */

static uft_cpu_info_t g_cpu_info = {0};
static bool g_cpu_detected = false;

/* =============================================================================
 * X86-64 CPUID WRAPPER
 * ============================================================================= */

#ifdef UFT_ARCH_X86

static void cpuid(uint32_t eax, uint32_t ecx, uint32_t *regs)
{
#if defined(_MSC_VER)
    __cpuidex((int *)regs, eax, ecx);
#elif defined(__GNUC__) || defined(__clang__)
    __cpuid_count(eax, ecx, regs[0], regs[1], regs[2], regs[3]);
#else
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
#endif
}

uft_cpu_info_t uft_cpu_detect(void)
{
    if (g_cpu_detected) {
        return g_cpu_info;
    }
    
    uft_cpu_info_t info = {0};
    uint32_t regs[4];
    
    /* ───────────────────────────────────────────────────────────────────────
     * VENDOR STRING
     * ─────────────────────────────────────────────────────────────────────── */
    
    cpuid(0, 0, regs);
    uint32_t max_basic_leaf = regs[0];
    
    memcpy(info.vendor + 0, &regs[1], 4);  /* EBX */
    memcpy(info.vendor + 4, &regs[3], 4);  /* EDX */
    memcpy(info.vendor + 8, &regs[2], 4);  /* ECX */
    info.vendor[12] = '\0';
    
    /* ───────────────────────────────────────────────────────────────────────
     * BRAND STRING (if supported)
     * ─────────────────────────────────────────────────────────────────────── */
    
    cpuid(0x80000000, 0, regs);
    uint32_t max_extended_leaf = regs[0];
    
    if (max_extended_leaf >= 0x80000004) {
        cpuid(0x80000002, 0, regs);
        memcpy(info.brand + 0, regs, 16);
        
        cpuid(0x80000003, 0, regs);
        memcpy(info.brand + 16, regs, 16);
        
        cpuid(0x80000004, 0, regs);
        memcpy(info.brand + 32, regs, 16);
        info.brand[48] = '\0';
    }
    
    /* ───────────────────────────────────────────────────────────────────────
     * FEATURE DETECTION (Leaf 1)
     * ─────────────────────────────────────────────────────────────────────── */
    
    if (max_basic_leaf >= 1) {
        cpuid(1, 0, regs);
        
        info.family   = ((regs[0] >> 8) & 0xF) + ((regs[0] >> 20) & 0xFF);
        info.model    = ((regs[0] >> 4) & 0xF) | ((regs[0] >> 12) & 0xF0);
        info.stepping = regs[0] & 0xF;
        
        /* ECX features */
        if (regs[2] & (1 << 0))  info.features |= UFT_CPU_SSE3;
        if (regs[2] & (1 << 9))  info.features |= UFT_CPU_SSSE3;
        if (regs[2] & (1 << 12)) info.features |= UFT_CPU_FMA;
        if (regs[2] & (1 << 19)) info.features |= UFT_CPU_SSE41;
        if (regs[2] & (1 << 20)) info.features |= UFT_CPU_SSE42;
        if (regs[2] & (1 << 23)) info.features |= UFT_CPU_POPCNT;
        if (regs[2] & (1 << 28)) info.features |= UFT_CPU_AVX;
        
        /* EDX features */
        if (regs[3] & (1 << 26)) info.features |= UFT_CPU_SSE2;
    }
    
    /* ───────────────────────────────────────────────────────────────────────
     * EXTENDED FEATURES (Leaf 7)
     * ─────────────────────────────────────────────────────────────────────── */
    
    if (max_basic_leaf >= 7) {
        cpuid(7, 0, regs);
        
        if (regs[1] & (1 << 3))  info.features |= UFT_CPU_BMI1;
        if (regs[1] & (1 << 5))  info.features |= UFT_CPU_AVX2;
        if (regs[1] & (1 << 8))  info.features |= UFT_CPU_BMI2;
        if (regs[1] & (1 << 16)) info.features |= UFT_CPU_AVX512F;
        if (regs[1] & (1 << 30)) info.features |= UFT_CPU_AVX512BW;
    }
    
    /* ───────────────────────────────────────────────────────────────────────
     * LZCNT (Extended Leaf 0x80000001)
     * ─────────────────────────────────────────────────────────────────────── */
    
    if (max_extended_leaf >= 0x80000001) {
        cpuid(0x80000001, 0, regs);
        if (regs[2] & (1 << 5)) info.features |= UFT_CPU_LZCNT;
    }
    
    /* ───────────────────────────────────────────────────────────────────────
     * CPU COUNT (OS-dependent, simplified)
     * ─────────────────────────────────────────────────────────────────────── */
    
#if defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    info.logical_cpus = sysinfo.dwNumberOfProcessors;
#elif defined(__linux__)
    info.logical_cpus = sysconf(_SC_NPROCESSORS_ONLN);
#else
    info.logical_cpus = 1;  /* Fallback */
#endif
    
    /* Cache info omitted for brevity - would use CPUID leaf 4 or 0x80000006 */
    
    g_cpu_info = info;
    g_cpu_detected = true;
    
    return info;
}

#else /* Non-x86 */

uft_cpu_info_t uft_cpu_detect(void)
{
    uft_cpu_info_t info = {0};
    strcpy(info.vendor, "Unknown");  /* REVIEW: Consider bounds check */
    strcpy(info.brand, "Non-x86 CPU");  /* REVIEW: Consider bounds check */
    info.logical_cpus = 1;
    
    g_cpu_info = info;
    g_cpu_detected = true;
    
    return info;
}

#endif /* UFT_ARCH_X86 */

/* =============================================================================
 * PUBLIC API
 * ============================================================================= */

bool uft_cpu_has_feature(uft_cpu_features_t feature)
{
    if (!g_cpu_detected) {
        uft_cpu_detect();
    }
    
    return (g_cpu_info.features & feature) != 0;
}

const uft_cpu_info_t* uft_cpu_get_info(void)
{
    if (!g_cpu_detected) {
        uft_cpu_detect();
    }
    
    return &g_cpu_info;
}

void uft_cpu_print_info(void)
{
    if (!g_cpu_detected) {
        uft_cpu_detect();
    }
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  CPU INFORMATION                                          ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║  Vendor:        %-40s  ║\n", g_cpu_info.vendor);
    printf("║  Brand:         %-40s  ║\n", g_cpu_info.brand);
    printf("║  Family/Model:  %d / %d (Stepping %d)                     ║\n",
           g_cpu_info.family, g_cpu_info.model, g_cpu_info.stepping);
    printf("║  Logical CPUs:  %-40d  ║\n", g_cpu_info.logical_cpus);
    printf("╟───────────────────────────────────────────────────────────╢\n");
    printf("║  SIMD FEATURES:                                           ║\n");
    printf("║    SSE2:        %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_SSE2) ? "✓ Yes" : "✗ No");
    printf("║    SSE3:        %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_SSE3) ? "✓ Yes" : "✗ No");
    printf("║    SSSE3:       %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_SSSE3) ? "✓ Yes" : "✗ No");
    printf("║    SSE4.1:      %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_SSE41) ? "✓ Yes" : "✗ No");
    printf("║    SSE4.2:      %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_SSE42) ? "✓ Yes" : "✗ No");
    printf("║    AVX:         %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_AVX) ? "✓ Yes" : "✗ No");
    printf("║    AVX2:        %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_AVX2) ? "✓ Yes" : "✗ No");
    printf("║    AVX-512:     %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_AVX512F) ? "✓ Yes" : "✗ No");
    printf("║    POPCNT:      %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_POPCNT) ? "✓ Yes" : "✗ No");
    printf("║    BMI1/BMI2:   %s                                        ║\n",
           (g_cpu_info.features & UFT_CPU_BMI1) ? "✓ Yes" : "✗ No");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
}

/* =============================================================================
 * DISPATCHER - MFM DECODE
 * ============================================================================= */

size_t uft_mfm_decode_flux(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits)
{
    if (!g_cpu_detected) {
        uft_cpu_detect();
    }
    
    /* Select best available implementation */
    if (g_cpu_info.features & UFT_CPU_AVX2) {
        return uft_mfm_decode_flux_avx2(flux_transitions, transition_count, output_bits);
    } else if (g_cpu_info.features & UFT_CPU_SSE2) {
        return uft_mfm_decode_flux_sse2(flux_transitions, transition_count, output_bits);
    } else {
        return uft_mfm_decode_flux_scalar(flux_transitions, transition_count, output_bits);
    }
}

/* =============================================================================
 * DISPATCHER - GCR DECODE
 * ============================================================================= */

size_t uft_gcr_decode_5to4(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bytes)
{
    if (!g_cpu_detected) {
        uft_cpu_detect();
    }
    
    if (g_cpu_info.features & UFT_CPU_AVX2) {
        return uft_gcr_decode_5to4_avx2(flux_transitions, transition_count, output_bytes);
    } else if (g_cpu_info.features & UFT_CPU_SSE2) {
        return uft_gcr_decode_5to4_sse2(flux_transitions, transition_count, output_bytes);
    } else {
        return uft_gcr_decode_5to4_scalar(flux_transitions, transition_count, output_bytes);
    }
}
