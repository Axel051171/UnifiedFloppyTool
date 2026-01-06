/**
 * @file uft_tool_fluxengine.c
 * @brief FluxEngine Tool Adapter
 * 
 * Unterstützt:
 * - Flux lesen (verschiedene Formate)
 * - Flux schreiben
 * - Format-spezifische Profile
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
#include <sys/wait.h>
#endif

// ============================================================================
// Command Execution (shared)
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

static bool fe_is_available(void) {
    char buf[64];
    return run_cmd("which fluxengine 2>/dev/null", buf, sizeof(buf)) == UFT_OK && buf[0];
}

// ============================================================================
// Hardware Detection
// ============================================================================

static bool fe_detect_hardware(char* info, size_t size) {
    if (!info || size == 0) return false;
    
    char buf[1024];
    if (run_cmd("fluxengine --version 2>&1", buf, sizeof(buf)) == UFT_OK) {
        // Extract version
        const char* ver = strstr(buf, "fluxengine");
        if (ver) {
            snprintf(info, size, "FluxEngine %s", ver + 11);
            char* nl = strchr(info, '\n');
            if (nl) *nl = '\0';
            return true;
        }
    }
    
    snprintf(info, size, "FluxEngine");
    return true;
}

// ============================================================================
// Profile Mapping
// ============================================================================

static const char* get_fe_profile(uft_format_t format, uft_geometry_preset_t geo) {
    // FluxEngine uses profile names
    switch (format) {
        case UFT_FORMAT_ADF:
            return "amiga";
        case UFT_FORMAT_D64:
        case UFT_FORMAT_G64:
            return "commodore1541";
        case UFT_FORMAT_IMG:
            switch (geo) {
                case UFT_GEO_PC_360K:
                case UFT_GEO_PC_720K:
                    return "ibm";
                case UFT_GEO_PC_1200K:
                case UFT_GEO_PC_1440K:
                    return "ibm";
                default:
                    return "ibm";
            }
        case UFT_FORMAT_DSK:
            return "atarist";
        default:
            return "ibm";
    }
}

// ============================================================================
// Read Disk
// ============================================================================

static uft_error_t fe_read_disk(void* context,
                                 const uft_tool_read_params_t* params,
                                 uft_unified_image_t* output) {
    if (!params || !output) return UFT_ERROR_NULL_POINTER;
    
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "/tmp/uft_fe_read_%d.scp", getpid());
    
    // Build command
    char cmd[1024];
    const char* profile = get_fe_profile(params->format, params->geometry);
    
    int len = snprintf(cmd, sizeof(cmd), "fluxengine read %s", profile);
    
    // Track range
    if (params->start_track >= 0 && params->end_track > params->start_track) {
        len += snprintf(cmd + len, sizeof(cmd) - len,
                        " --cylinders=%d-%d", params->start_track, params->end_track);
    }
    
    // Output
    snprintf(cmd + len, sizeof(cmd) - len, " -o \"%s\" 2>&1", temp_file);
    
    // Execute
    char output_buf[4096];
    uft_error_t err = run_cmd(cmd, output_buf, sizeof(output_buf));
    
    if (err != UFT_OK) {
        unlink(temp_file);
        return err;
    }
    
    // Open result
    err = uft_image_open(output, temp_file);
    unlink(temp_file);
    
    return err;
}

// ============================================================================
// Write Disk
// ============================================================================

static uft_error_t fe_write_disk(void* context,
                                  const uft_tool_write_params_t* params,
                                  const uft_unified_image_t* input) {
    if (!params || !input || !input->path) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    const char* profile = get_fe_profile(input->source_format, UFT_GEO_UNKNOWN);
    
    snprintf(cmd, sizeof(cmd), "fluxengine write %s -i \"%s\" 2>&1",
             profile, input->path);
    
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Format-specific Reads
// ============================================================================

static uft_error_t fe_read_amiga(void* context, const char* output_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "fluxengine read amiga -o \"%s\" 2>&1", output_path);
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

static uft_error_t fe_read_c64(void* context, const char* output_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "fluxengine read commodore1541 -o \"%s\" 2>&1", output_path);
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

static uft_error_t fe_read_ibm(void* context, const char* output_path, bool hd) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "fluxengine read ibm%s -o \"%s\" 2>&1",
             hd ? "1440" : "720", output_path);
    char buf[4096];
    return run_cmd(cmd, buf, sizeof(buf));
}

// ============================================================================
// Init / Cleanup
// ============================================================================

static uft_error_t fe_init(void** context) {
    *context = NULL;
    return UFT_OK;
}

static void fe_cleanup(void* context) {
    (void)context;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_tool_adapter_t uft_tool_fluxengine = {
    .name = "fluxengine",
    .version = "1.0.0",
    .description = "FluxEngine Disk Tool",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE |
                    UFT_TOOL_CAP_FLUX | UFT_TOOL_CAP_HARDWARE,
    .supported_formats = (1u << UFT_FORMAT_SCP) | (1u << UFT_FORMAT_ADF) |
                         (1u << UFT_FORMAT_D64) | (1u << UFT_FORMAT_IMG),
    
    .init = fe_init,
    .cleanup = fe_cleanup,
    .is_available = fe_is_available,
    .detect_hardware = fe_detect_hardware,
    
    .read_disk = fe_read_disk,
    .write_disk = fe_write_disk,
    .convert = NULL,
    .get_disk_info = NULL,
    .seek = NULL,
    .reset = NULL,
};
