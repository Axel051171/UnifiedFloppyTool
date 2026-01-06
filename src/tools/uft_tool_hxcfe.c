/**
 * @file uft_tool_hxcfe.c
 * @brief HxC Floppy Emulator Tool Adapter
 * 
 * Unterstützt:
 * - Universelle Format-Konvertierung
 * - Viele Input/Output-Formate
 * - GUI und CLI
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
// Format Mapping
// ============================================================================

static const char* hxc_format_name(uft_format_t format) {
    switch (format) {
        case UFT_FORMAT_HFE:      return "HFE";
        case UFT_FORMAT_IMG:      return "RAW_LOADER";
        case UFT_FORMAT_SCP:      return "SCP";
        case UFT_FORMAT_ADF:      return "ADF";
        case UFT_FORMAT_D64:      return "D64";
        case UFT_FORMAT_DSK:      return "DSK";
        case UFT_FORMAT_STX:      return "STX";
        case UFT_FORMAT_IPF:      return "IPF";
        case UFT_FORMAT_IMD:      return "IMD";
        case UFT_FORMAT_TD0:      return "TD0";
        case UFT_FORMAT_KRYOFLUX: return "KF_RAW";
        default:                  return "RAW_LOADER";
    }
}

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

static bool hxc_is_available(void) {
    char buf[64];
    return run_cmd("which hxcfe 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0];
}

// ============================================================================
// Hardware Detection (N/A - converter only)
// ============================================================================

static bool hxc_detect_hardware(char* info, size_t size) {
    if (!info || size == 0) return false;
    
    char buf[1024];
    if (run_cmd("hxcfe -help 2>&1 | head -3", buf, sizeof(buf)) == UFT_OK) {
        const char* ver = strstr(buf, "version");
        if (ver) {
            snprintf(info, size, "HxC Floppy Emulator Tool %.30s", ver);
            return true;
        }
    }
    
    snprintf(info, size, "HxC Floppy Emulator Tool");
    return true;
}

// ============================================================================
// Convert
// ============================================================================

static uft_error_t hxc_convert(void* context,
                                const char* input,
                                const char* output,
                                uft_format_t format) {
    if (!input || !output) return UFT_ERROR_NULL_POINTER;
    
    const char* fmt_name = hxc_format_name(format);
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "hxcfe -finput:\"%s\" -foutput:\"%s\" -conv:%s 2>&1",
             input, output, fmt_name);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Analyze (hxcfe can show disk structure)
// ============================================================================

static uft_error_t hxc_analyze(void* context,
                                const char* input,
                                char* report,
                                size_t report_size) {
    if (!input || !report) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "hxcfe -finput:\"%s\" -infos 2>&1", input);
    
    return run_cmd(cmd, report, report_size);
}

// ============================================================================
// Export to HFE (common operation)
// ============================================================================

static uft_error_t hxc_export_hfe(const char* input, const char* output) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "hxcfe -finput:\"%s\" -foutput:\"%s\" -conv:HFE 2>&1",
             input, output);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// List Supported Formats
// ============================================================================

static uft_error_t hxc_list_formats(char* formats, size_t size) {
    if (!formats || size == 0) return UFT_ERROR_NULL_POINTER;
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "hxcfe -modulelist 2>&1");
    
    return run_cmd(cmd, formats, size);
}

// ============================================================================
// Init / Cleanup
// ============================================================================

static uft_error_t hxc_init(void** context) {
    *context = NULL;
    return UFT_OK;
}

static void hxc_cleanup(void* context) {
    (void)context;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_tool_adapter_t uft_tool_hxcfe = {
    .name = "hxcfe",
    .version = "1.0.0",
    .description = "HxC Floppy Emulator Tool (Converter)",
    .capabilities = UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_INFO,
    .supported_formats = 0xFFFFFFFF,  // Supports almost everything
    
    .init = hxc_init,
    .cleanup = hxc_cleanup,
    .is_available = hxc_is_available,
    .detect_hardware = hxc_detect_hardware,
    
    .read_disk = NULL,  // No hardware support
    .write_disk = NULL,
    .convert = hxc_convert,
    .get_disk_info = NULL,
    .seek = NULL,
    .reset = NULL,
};
