/**
 * @file uft_tool_adapter.c
 * @brief Tool Adapter Registry Implementation
 * 
 */

#include "uft/uft_tool_adapter.h"
#include "uft/uft_safe.h"
#include "uft/uft_format_hfe.h"
#include "uft/uft_format_scp.h"
#include "uft_security.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define unlink _unlink
#define getpid _getpid
typedef int pid_t;
#else
#include <unistd.h>     /* unlink, getpid */
#include <sys/types.h>  /* pid_t */
#endif

// ============================================================================
// Registry Storage
// ============================================================================

#define MAX_TOOLS 16

static struct {
    const uft_tool_adapter_t* tools[MAX_TOOLS];
    size_t count;
    const char* preferred_tool;
} g_tool_registry = {0};

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_tool_register(const uft_tool_adapter_t* adapter) {
    if (!adapter || !adapter->name) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (g_tool_registry.count >= MAX_TOOLS) {
        return UFT_ERROR_NO_SPACE;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, adapter->name) == 0) {
            return UFT_ERROR_ALREADY_EXISTS;
        }
    }
    
    g_tool_registry.tools[g_tool_registry.count++] = adapter;
    return UFT_OK;
}

uft_error_t uft_tool_unregister(const char* name) {
    if (!name) return UFT_ERROR_INVALID_ARG;
    
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, name) == 0) {
            for (size_t j = i; j + 1 < g_tool_registry.count; j++) {
                g_tool_registry.tools[j] = g_tool_registry.tools[j + 1];
            }
            g_tool_registry.count--;
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_NOT_FOUND;
}

// ============================================================================
// Lookup
// ============================================================================

const uft_tool_adapter_t* uft_tool_find(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, name) == 0) {
            return g_tool_registry.tools[i];
        }
    }
    
    return NULL;
}

const uft_tool_adapter_t* uft_tool_find_for_format(uft_format_t format) {
    // First try preferred tool
    if (g_tool_registry.preferred_tool) {
        const uft_tool_adapter_t* pref = uft_tool_find(g_tool_registry.preferred_tool);
        if (pref && (pref->supported_formats & (1u << format))) {
            if (pref->is_available && pref->is_available()) {
                return pref;
            }
        }
    }
    
    // Find first available tool supporting this format
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        if (tool->supported_formats & (1u << format)) {
            if (!tool->is_available || tool->is_available()) {
                return tool;
            }
        }
    }
    
    return NULL;
}

const uft_tool_adapter_t* uft_tool_find_for_operation(uft_tool_cap_t required_caps) {
    // First try preferred tool
    if (g_tool_registry.preferred_tool) {
        const uft_tool_adapter_t* pref = uft_tool_find(g_tool_registry.preferred_tool);
        if (pref && (pref->capabilities & required_caps) == required_caps) {
            if (pref->is_available && pref->is_available()) {
                return pref;
            }
        }
    }
    
    // Find first available tool with required capabilities
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        if ((tool->capabilities & required_caps) == required_caps) {
            if (!tool->is_available || tool->is_available()) {
                return tool;
            }
        }
    }
    
    return NULL;
}

size_t uft_tool_list(const uft_tool_adapter_t** tools, size_t max_count) {
    if (!tools || max_count == 0) {
        return g_tool_registry.count;
    }
    
    size_t count = (g_tool_registry.count < max_count) ? 
                    g_tool_registry.count : max_count;
    
    for (size_t i = 0; i < count; i++) {
        tools[i] = g_tool_registry.tools[i];
    }
    
    return count;
}

// ============================================================================
// Preference
// ============================================================================

uft_error_t uft_tool_set_preferred(const char* tool_name) {
    if (tool_name) {
        // Verify tool exists
        if (!uft_tool_find(tool_name)) {
            return UFT_ERROR_NOT_FOUND;
        }
    }
    
    g_tool_registry.preferred_tool = tool_name;
    return UFT_OK;
}

const char* uft_tool_get_preferred(void) {
    return g_tool_registry.preferred_tool;
}

// ============================================================================
// Built-in Tool Adapters (Stubs)
// ============================================================================


static bool uft_gw_is_available(void) {
    // Check if 'gw' command exists
    FILE* p = popen("which gw 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    
    return found;
}

static bool uft_gw_detect_hardware(char* info, size_t size) {
    /* BUGFIX: Better error handling and device detection
     * - Check for actual device presence, not just command output
     * - Handle cases where gw is installed but no device connected
     * - Provide meaningful feedback
     */
    FILE* p = popen("gw info 2>&1", "r");  /* Include stderr for error messages */
    if (!p) {
        if (info && size > 0) {
            snprintf(info, size, "Failed to execute 'gw info'");
        }
        return false;
    }
    
    char buf[1024] = {0};
    size_t total = 0;
    while (fgets(buf + total, (int)(sizeof(buf) - total), p) && total < sizeof(buf) - 1) {
        total = strlen(buf);
    }
    int status = pclose(p);
    
    /* Check for common error messages indicating no device */
    if (strstr(buf, "No Greaseweazle") != NULL ||
        strstr(buf, "not found") != NULL ||
        strstr(buf, "Cannot find") != NULL ||
        strstr(buf, "error") != NULL ||
        status != 0) {
        if (info && size > 0) {
            if (total > 0) {
                snprintf(info, size, "No device: %s", buf);
            } else {
                snprintf(info, size, "No Greaseweazle device detected");
            }
        }
        return false;
    }
    
    /* Check for valid device info (should contain version/model) */
    if (total > 0 && (strstr(buf, "Greaseweazle") != NULL || 
                      strstr(buf, "Model") != NULL ||
                      strstr(buf, "version") != NULL)) {
        if (info && size > 0) {
            snprintf(info, size, "%s", buf);
        }
        return true;
    }
    
    /* Unknown response */
    if (info && size > 0) {
        snprintf(info, size, "Unknown response from 'gw info'");
    }
    return false;
}

static uft_error_t uft_gw_read_disk(void* context,
                                 const uft_tool_read_params_t* params,
                                 uft_unified_image_t* output) {
    (void)context;
    
    if (!params || !output) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    /* Build temp filename based on requested format */
    char tmpfile[256];
    const char* ext = "scp";  /* Default to SCP for flux */
    const char* fmt_arg = "scp";
    
    if (params->format == UFT_FORMAT_HFE) {
        ext = "hfe";
        fmt_arg = "hfe";
    } else if (params->format == UFT_FORMAT_RAW) {
        ext = "raw";
        fmt_arg = "raw";
    }
    
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/uft_gw_read_%u.%s", 
             (unsigned)getpid(), ext);
    
    /* Build gw read command */
    char cmd[512];
    int ret = snprintf(cmd, sizeof(cmd),
        "gw read --format=%s", fmt_arg);
    
    /* Add track range if specified */
    if (params->start_track > 0 || params->end_track > 0) {
        int end = (params->end_track > 0) ? params->end_track : 83;
        ret += snprintf(cmd + ret, sizeof(cmd) - (size_t)ret,
            " --tracks=%d-%d", params->start_track, end);
    }
    
    /* Add heads/sides */
    if (params->heads == 1) {
        ret += snprintf(cmd + ret, sizeof(cmd) - (size_t)ret, " --heads=0");
    }
    
    /* Add revolutions for flux capture */
    if (params->revolutions > 0) {
        ret += snprintf(cmd + ret, sizeof(cmd) - (size_t)ret,
            " --revs=%d", params->revolutions);
    }
    
    /* Add drive select */
    if (params->drive > 0) {
        ret += snprintf(cmd + ret, sizeof(cmd) - (size_t)ret,
            " --drive=%c", 'A' + (char)(params->drive - 1));
    }
    
    /* SECURITY FIX: Validate filename before shell use */
    if (!uft_is_safe_filename(tmpfile)) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    /* Add output file - tmpfile validated above */
    snprintf(cmd + ret, sizeof(cmd) - (size_t)ret, " \"%s\" 2>&1", tmpfile);
    
    /* Execute gw read */
    FILE* p = popen(cmd, "r");
    if (!p) {
        return UFT_ERROR_IO;
    }
    
    /* Read command output for progress/errors */
    char line[256];
    bool success = false;
    while (fgets(line, sizeof(line), p)) {
        /* Check for success indicators */
        if (strstr(line, "Written") || strstr(line, "tracks")) {
            success = true;
        }
        /* Check for errors */
        if (strstr(line, "error") || strstr(line, "failed") || 
            strstr(line, "Error") || strstr(line, "Failed")) {
            pclose(p);
            unlink(tmpfile);
            return UFT_ERROR_HAL;
        }
    }
    
    int exit_code = pclose(p);
    if (exit_code != 0 && !success) {
        unlink(tmpfile);
        return UFT_ERROR_HAL;
    }
    
    /* Check if file was created */
    FILE* f = fopen(tmpfile, "rb");
    if (!f) {
        return UFT_ERROR_IO;
    }
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long fsize = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (fsize <= 0) {
        fclose(f);
        unlink(tmpfile);
        return UFT_ERROR_EMPTY;
    }
    
    /* Allocate buffer and read file */
    uint8_t* data = (uint8_t*)malloc((size_t)fsize);
    if (!data) {
        fclose(f);
        unlink(tmpfile);
        return UFT_ERROR_MEMORY;
    }
    
    size_t read_bytes = fread(data, 1, (size_t)fsize, f);
    fclose(f);
    unlink(tmpfile);
    
    if (read_bytes != (size_t)fsize) {
        free(data);
        return UFT_ERROR_IO;
    }
    
    /* Parse captured data into unified image using format-specific loader */
    uft_error_t err;
    if (params->format == UFT_FORMAT_HFE) {
        err = uft_hfe_load(data, (size_t)fsize, output);
    } else if (params->format == UFT_FORMAT_SCP) {
        err = uft_scp_load(data, (size_t)fsize, output);
    } else {
        /* Raw flux - store directly */
        output->raw_data = data;
        output->raw_size = (size_t)fsize;
        output->format = UFT_FORMAT_RAW;
        data = NULL;  /* Transfer ownership */
        err = UFT_SUCCESS;
    }
    
    free(data);
    return err;
}

static const uft_tool_adapter_t g_tool_gw = {
    .name = "gw",
    .version = "1.0",
    .description = "Greaseweazle Command Line Tool",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE | UFT_TOOL_CAP_FLUX | UFT_TOOL_CAP_HARDWARE,
    .supported_formats = (1 << UFT_FORMAT_SCP) | (1 << UFT_FORMAT_HFE),
    .init = NULL,
    .cleanup = NULL,
    .is_available = uft_gw_is_available,
    .detect_hardware = uft_gw_detect_hardware,
    .read_disk = uft_gw_read_disk,
    .write_disk = NULL,
    .convert = NULL,
};

// --- GCR tools Adapter ---

static bool GCR tools_is_available(void) {
    FILE* p = popen("which nibread 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    
    return found;
}

static const uft_tool_adapter_t g_tool_GCR tools = {
    .name = "GCR tools",
    .version = "1.0",
    .description = "Commodore Disk Tools",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE | UFT_TOOL_CAP_SECTOR | UFT_TOOL_CAP_HARDWARE,
    .supported_formats = (1 << UFT_FORMAT_D64) | (1 << UFT_FORMAT_G64) | (1 << UFT_FORMAT_NBZ),
    .init = NULL,
    .cleanup = NULL,
    .is_available = GCR tools_is_available,
    .detect_hardware = NULL,
    .read_disk = NULL,
    .write_disk = NULL,
    .convert = NULL,
};

// --- libflux_ctx Adapter ---

static bool libflux_is_available(void) {
    FILE* p = popen("which libflux_ctx 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    
    return found;
}

static uft_error_t libflux_convert(void* context,
                                  const char* input,
                                  const char* output,
                                  uft_format_t format) {
    (void)context;
    
    if (!input || !output) return UFT_ERROR_INVALID_ARG;
    
    // Build command
    char cmd[1024];
    const char* fmt_str = "raw";
    
    switch (format) {
        case UFT_FORMAT_HFE: fmt_str = "hfe"; break;
        case UFT_FORMAT_IMG: fmt_str = "raw"; break;
        case UFT_FORMAT_SCP: fmt_str = "scp"; break;
        default: break;
    }
    
    snprintf(cmd, sizeof(cmd), "libflux_ctx -finput:\"%s\" -foutput:\"%s\" -conv:%s 2>/dev/null",
             input, output, fmt_str);
    
    int ret = system(cmd);
    return (ret == 0) ? UFT_OK : UFT_ERROR_TOOL_FAILED;
}

static const uft_tool_adapter_t g_tool_libflux_ctx = {
    .name = "libflux_ctx",
    .version = "1.0",
    .description = "UFT HFE Format Tool",
    .capabilities = UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_FLUX | UFT_TOOL_CAP_SECTOR,
    .supported_formats = 0xFFFFFFFF,  // Supports many formats
    .init = NULL,
    .cleanup = NULL,
    .is_available = libflux_is_available,
    .detect_hardware = NULL,
    .read_disk = NULL,
    .write_disk = NULL,
    .convert = libflux_convert,
};

// ============================================================================
// Initialization
// ============================================================================

void uft_register_builtin_tools(void) {
    uft_tool_register(&g_tool_gw);
    uft_tool_register(&g_tool_GCR tools);
    uft_tool_register(&g_tool_libflux_ctx);
}
