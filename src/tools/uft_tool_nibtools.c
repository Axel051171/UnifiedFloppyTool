/**
 * @file uft_tool_nibtools.c
 * @brief nibtools Adapter - Commodore 1541/1571 Support
 * 
 * Unterstützt:
 * - D64/G64/NBZ lesen
 * - D64/G64 schreiben
 * - Track-Alignment
 * - Parallel Cable Support
 */

#include "uft/uft_tool_adapter.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

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

static bool nib_is_available(void) {
    char buf[64];
    // Check for nibread (main tool)
    if (run_cmd("which nibread 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0]) {
        return true;
    }
    // Also check for nibtools
    return run_cmd("which nibtools 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0];
}

// ============================================================================
// Hardware Detection
// ============================================================================

static bool nib_detect_hardware(char* info, size_t size) {
    if (!info || size == 0) return false;
    
    // nibtools requires XUM1541 or similar
    char buf[1024];
    if (run_cmd("nibread --help 2>&1 | head -5", buf, sizeof(buf)) == UFT_OK) {
        if (strstr(buf, "nibread") || strstr(buf, "nibtools")) {
            snprintf(info, size, "nibtools (XUM1541/ZoomFloppy)");
            return true;
        }
    }
    
    snprintf(info, size, "nibtools");
    return true;
}

// ============================================================================
// Read Disk (nibread)
// ============================================================================

static uft_error_t nib_read_disk(void* context,
                                  const uft_tool_read_params_t* params,
                                  uft_unified_image_t* output) {
    if (!params || !output) return UFT_ERROR_NULL_POINTER;
    
    char temp_file[256];
    const char* ext = ".g64";  // Default to G64 for raw capture
    
    switch (params->format) {
        case UFT_FORMAT_D64: ext = ".d64"; break;
        case UFT_FORMAT_G64: ext = ".g64"; break;
        case UFT_FORMAT_NBZ: ext = ".nbz"; break;
        default: break;
    }
    
    snprintf(temp_file, sizeof(temp_file), "/tmp/uft_nib_read_%d%s", getpid(), ext);
    
    // Build command
    char cmd[1024];
    int len = snprintf(cmd, sizeof(cmd), "nibread");
    
    // Track range
    if (params->start_track >= 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len,
                        " --start-track=%d", params->start_track);
    }
    if (params->end_track > 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len,
                        " --end-track=%d", params->end_track);
    }
    
    // Retries
    if (params->retries > 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len,
                        " --retries=%d", params->retries);
    }
    
    // Output
    snprintf(cmd + len, sizeof(cmd) - len, " \"%s\" 2>&1", temp_file);
    
    char buf[4096];
    uft_error_t err = run_cmd(cmd, buf, sizeof(buf));
    
    if (err != UFT_OK) {
        unlink(temp_file);
        return err;
    }
    
    err = uft_image_open(output, temp_file);
    unlink(temp_file);
    
    return err;
}

// ============================================================================
// Write Disk (nibwrite)
// ============================================================================

static uft_error_t nib_write_disk(void* context,
                                   const uft_tool_write_params_t* params,
                                   const uft_unified_image_t* input) {
    if (!params || !input || !input->path) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    int len = snprintf(cmd, sizeof(cmd), "nibwrite");
    
    // Track range
    if (params->start_track >= 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len,
                        " --start-track=%d", params->start_track);
    }
    if (params->end_track > 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len,
                        " --end-track=%d", params->end_track);
    }
    
    // Verify?
    if (params->verify) {
        len += snprintf(cmd + len, sizeof(cmd) - len, " --verify");
    }
    
    snprintf(cmd + len, sizeof(cmd) - len, " \"%s\" 2>&1", input->path);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Convert (d64copy for D64<->Disk)
// ============================================================================

static uft_error_t nib_convert(void* context,
                                const char* input,
                                const char* output,
                                uft_format_t format) {
    if (!input || !output) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    
    // Use nibconv for format conversion
    snprintf(cmd, sizeof(cmd), "nibconv \"%s\" \"%s\" 2>&1", input, output);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Format Disk
// ============================================================================

static uft_error_t nib_format_disk(void* context, int tracks) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "nibformat --tracks=%d 2>&1", tracks > 0 ? tracks : 35);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Init / Cleanup
// ============================================================================

static uft_error_t nib_init(void** context) {
    *context = NULL;
    return UFT_OK;
}

static void nib_cleanup(void* context) {
    (void)context;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_tool_adapter_t uft_tool_nibtools = {
    .name = "nibtools",
    .version = "1.0.0",
    .description = "Commodore 1541/1571 Disk Tools",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE |
                    UFT_TOOL_CAP_SECTOR | UFT_TOOL_CAP_HARDWARE |
                    UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_FORMAT,
    .supported_formats = (1u << UFT_FORMAT_D64) | (1u << UFT_FORMAT_G64) |
                         (1u << UFT_FORMAT_NBZ),
    
    .init = nib_init,
    .cleanup = nib_cleanup,
    .is_available = nib_is_available,
    .detect_hardware = nib_detect_hardware,
    
    .read_disk = nib_read_disk,
    .write_disk = nib_write_disk,
    .convert = nib_convert,
    .get_disk_info = NULL,
    .seek = NULL,
    .reset = NULL,
};
