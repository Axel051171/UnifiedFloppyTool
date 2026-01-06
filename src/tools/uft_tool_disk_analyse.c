/**
 * @file uft_tool_disk_analyse.c
 * @brief disk-analyse Adapter (Keir Fraser)
 * 
 * Leistungsstarkes Analyse-Tool für:
 * - SCP/Kryoflux Stream Analyse
 * - Format-Erkennung
 * - Fehler-Diagnose
 */

#include "uft/uft_tool_adapter.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#endif

// ============================================================================
// Helper
// ============================================================================

static uft_error_t run_cmd(const char* cmd, char* output, size_t out_size) {
    FILE* p = popen(cmd, "r");
    if (!p) return UFT_ERROR_IO;
    
    if (output && out_size > 0) {
        size_t total = 0;
        while (total < out_size - 1) {
            size_t n = fread(output + total, 1, out_size - total - 1, p);
            if (n == 0) break;
            total += n;
        }
        output[total] = '\0';
    }
    
    int status = pclose(p);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? UFT_OK : UFT_ERROR_TOOL_FAILED;
}

// ============================================================================
// Availability
// ============================================================================

static bool da_is_available(void) {
    char buf[64];
    return run_cmd("which disk-analyse 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0];
}

// ============================================================================
// Hardware Detection (N/A - analysis only)
// ============================================================================

static bool da_detect_hardware(char* info, size_t size) {
    snprintf(info, size, "disk-analyse (Keir Fraser)");
    return true;
}

// ============================================================================
// Analyse Disk Image
// ============================================================================

static uft_error_t da_analyse(const char* input,
                               const char* format,
                               char* report,
                               size_t report_size) {
    if (!input || !report) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    
    if (format && format[0]) {
        snprintf(cmd, sizeof(cmd),
                 "disk-analyse -f %s \"%s\" 2>&1", format, input);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "disk-analyse \"%s\" 2>&1", input);
    }
    
    return run_cmd(cmd, report, report_size);
}

// ============================================================================
// Convert via disk-analyse
// ============================================================================

static uft_error_t da_convert(void* context,
                               const char* input,
                               const char* output,
                               uft_format_t format) {
    if (!input || !output) return UFT_ERROR_NULL_POINTER;
    
    const char* fmt_str = NULL;
    
    switch (format) {
        case UFT_FORMAT_ADF:  fmt_str = "adf"; break;
        case UFT_FORMAT_IMG:  fmt_str = "img"; break;
        case UFT_FORMAT_SCP:  fmt_str = "scp"; break;
        case UFT_FORMAT_HFE:  fmt_str = "hfe"; break;
        case UFT_FORMAT_IPF:  fmt_str = "ipf"; break;
        default:
            return UFT_ERROR_FORMAT_NOT_SUPPORTED;
    }
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "disk-analyse \"%s\" \"%s\" 2>&1", input, output);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Format Detection
// ============================================================================

static uft_error_t da_detect_format(const char* input,
                                     char* format_name,
                                     size_t size) {
    if (!input || !format_name) return UFT_ERROR_NULL_POINTER;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "disk-analyse -i \"%s\" 2>&1 | head -20", input);
    
    char buf[4096];
    uft_error_t err = run_cmd(cmd, buf, sizeof(buf));
    
    if (err == UFT_OK) {
        // Parse output for format
        const char* fmt = strstr(buf, "Format:");
        if (fmt) {
            fmt += 7;
            while (*fmt == ' ') fmt++;
            const char* end = strchr(fmt, '\n');
            size_t len = end ? (size_t)(end - fmt) : strlen(fmt);
            if (len >= size) len = size - 1;
            strncpy(format_name, fmt, len);
            format_name[len] = '\0';
        } else {
            strncpy(format_name, "Unknown", size - 1);
        }
    }
    
    return err;
}

// ============================================================================
// Track-by-Track Analysis
// ============================================================================

static uft_error_t da_analyse_track(const char* input,
                                     int track,
                                     int head,
                                     char* report,
                                     size_t size) {
    if (!input || !report) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "disk-analyse -t %d.%d \"%s\" 2>&1", track, head, input);
    
    return run_cmd(cmd, report, size);
}

// ============================================================================
// Init / Cleanup
// ============================================================================

static uft_error_t da_init(void** context) {
    *context = NULL;
    return UFT_OK;
}

static void da_cleanup(void* context) {
    (void)context;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_tool_adapter_t uft_tool_disk_analyse = {
    .name = "disk-analyse",
    .version = "1.0.0",
    .description = "Keir Fraser's Disk Analyzer",
    .capabilities = UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_INFO,
    .supported_formats = (1u << UFT_FORMAT_SCP) | (1u << UFT_FORMAT_ADF) |
                         (1u << UFT_FORMAT_IMG) | (1u << UFT_FORMAT_HFE) |
                         (1u << UFT_FORMAT_IPF) | (1u << UFT_FORMAT_KRYOFLUX),
    
    .init = da_init,
    .cleanup = da_cleanup,
    .is_available = da_is_available,
    .detect_hardware = da_detect_hardware,
    
    .read_disk = NULL,
    .write_disk = NULL,
    .convert = da_convert,
    .get_disk_info = NULL,
    .seek = NULL,
    .reset = NULL,
};
