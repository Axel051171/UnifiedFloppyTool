/**
 * @file uft_tool_adftools.c
 * @brief Amiga ADF Tools Adapter
 * 
 * Unterstützt:
 * - ADF lesen/schreiben
 * - Filesystem-Operationen
 * - Boot-Block Analyse
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

static bool adf_is_available(void) {
    char buf[64];
    // Check for unadf (most common)
    if (run_cmd("which unadf 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0]) {
        return true;
    }
    // Also check for adftools
    return run_cmd("which adf-check 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0];
}

// ============================================================================
// Hardware Detection (N/A)
// ============================================================================

static bool adf_detect_hardware(char* info, size_t size) {
    snprintf(info, size, "ADF Tools (File operations only)");
    return true;
}

// ============================================================================
// Create ADF
// ============================================================================

static uft_error_t adf_create(const char* path, bool hd) {
    char cmd[512];
    
    // Use adf-create or dd
    if (hd) {
        // HD: 1760KB
        snprintf(cmd, sizeof(cmd),
                 "dd if=/dev/zero of=\"%s\" bs=512 count=3520 2>/dev/null", path);
    } else {
        // DD: 880KB
        snprintf(cmd, sizeof(cmd),
                 "dd if=/dev/zero of=\"%s\" bs=512 count=1760 2>/dev/null", path);
    }
    
    char buf[256];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// List Contents (unadf -l)
// ============================================================================

static uft_error_t adf_list(const char* path, char* listing, size_t size) {
    if (!path || !listing) return UFT_ERROR_NULL_POINTER;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "unadf -l \"%s\" 2>&1", path);
    
    return run_cmd(cmd, listing, size);
}

// ============================================================================
// Extract All (unadf -d)
// ============================================================================

static uft_error_t adf_extract(const char* path, const char* dest_dir) {
    if (!path || !dest_dir) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "unadf -d \"%s\" \"%s\" 2>&1", dest_dir, path);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Check Integrity
// ============================================================================

static uft_error_t adf_check(const char* path, char* report, size_t size) {
    if (!path) return UFT_ERROR_NULL_POINTER;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "unadf -c \"%s\" 2>&1", path);
    
    return run_cmd(cmd, report ? report : (char*)"", size);
}

// ============================================================================
// Get Boot Block Info
// ============================================================================

static uft_error_t adf_bootblock_info(const char* path, char* info, size_t size) {
    if (!path || !info) return UFT_ERROR_NULL_POINTER;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "unadf -b \"%s\" 2>&1", path);
    
    return run_cmd(cmd, info, size);
}

// ============================================================================
// Convert (limited - just copy for now)
// ============================================================================

static uft_error_t adf_convert(void* context,
                                const char* input,
                                const char* output,
                                uft_format_t format) {
    if (!input || !output) return UFT_ERROR_NULL_POINTER;
    
    // ADF tools can't really convert to other formats
    // Just copy if target is ADF
    if (format == UFT_FORMAT_ADF) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\" 2>&1", input, output);
        char buf[256];
        return run_cmd(cmd, buf, sizeof(buf));
    }
    
    return UFT_ERROR_FORMAT_NOT_SUPPORTED;
}

// ============================================================================
// Init / Cleanup
// ============================================================================

static uft_error_t adf_init(void** context) {
    *context = NULL;
    return UFT_OK;
}

static void adf_cleanup(void* context) {
    (void)context;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_tool_adapter_t uft_tool_adftools = {
    .name = "adftools",
    .version = "1.0.0",
    .description = "Amiga ADF Tools",
    .capabilities = UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_INFO | UFT_TOOL_CAP_SECTOR,
    .supported_formats = (1u << UFT_FORMAT_ADF),
    
    .init = adf_init,
    .cleanup = adf_cleanup,
    .is_available = adf_is_available,
    .detect_hardware = adf_detect_hardware,
    
    .read_disk = NULL,
    .write_disk = NULL,
    .convert = adf_convert,
    .get_disk_info = NULL,
    .seek = NULL,
    .reset = NULL,
};
