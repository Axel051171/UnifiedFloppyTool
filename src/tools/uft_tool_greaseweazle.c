/**
 * @file uft_tool_greaseweazle.c
 * @brief Greaseweazle Tool Adapter - Vollständige Integration
 * 
 * Unterstützt:
 * - Flux lesen (SCP, HFE, raw)
 * - Flux schreiben
 * - Disk-Info abfragen
 * - Firmware-Version
 */

#include "uft/uft_tool_adapter.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

// ============================================================================
// Constants
// ============================================================================

#define GW_CMD              "gw"
#define GW_TIMEOUT_SEC      300
#define GW_MAX_OUTPUT       65536

// ============================================================================
// Helper: Command Execution
// ============================================================================

typedef struct {
    int     exit_code;
    char*   stdout_buf;
    char*   stderr_buf;
    size_t  stdout_len;
    size_t  stderr_len;
} cmd_result_t;

static void free_cmd_result(cmd_result_t* r) {
    if (r) {
        free(r->stdout_buf);
        free(r->stderr_buf);
        memset(r, 0, sizeof(*r));
    }
}

static uft_error_t run_command(const char* cmd, cmd_result_t* result) {
    if (!cmd || !result) return UFT_ERROR_NULL_POINTER;
    
    memset(result, 0, sizeof(*result));
    
    // Create pipes
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        return UFT_ERROR_IO;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return UFT_ERROR_IO;
    }
    
    if (pid == 0) {
        // Child
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
    
    // Parent
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    // Read output
    result->stdout_buf = malloc(GW_MAX_OUTPUT);
    result->stderr_buf = malloc(GW_MAX_OUTPUT);
    
    if (result->stdout_buf) {
        result->stdout_len = read(stdout_pipe[0], result->stdout_buf, GW_MAX_OUTPUT - 1);
        if (result->stdout_len > 0) {
            result->stdout_buf[result->stdout_len] = '\0';
        }
    }
    
    if (result->stderr_buf) {
        result->stderr_len = read(stderr_pipe[0], result->stderr_buf, GW_MAX_OUTPUT - 1);
        if (result->stderr_len > 0) {
            result->stderr_buf[result->stderr_len] = '\0';
        }
    }
    
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    
    int status;
    waitpid(pid, &status, 0);
    result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    
    return (result->exit_code == 0) ? UFT_OK : UFT_ERROR_TOOL_FAILED;
}

// ============================================================================
// Availability Check
// ============================================================================

static bool gw_is_available(void) {
    cmd_result_t r;
    uft_error_t err = run_command("which gw 2>/dev/null", &r);
    bool found = (err == UFT_OK && r.stdout_len > 0);
    free_cmd_result(&r);
    return found;
}

// ============================================================================
// Hardware Detection
// ============================================================================

static bool gw_detect_hardware(char* info, size_t size) {
    if (!info || size == 0) return false;
    
    cmd_result_t r;
    uft_error_t err = run_command("gw info 2>&1", &r);
    
    if (err == UFT_OK && r.stdout_buf) {
        // Parse output for device info
        // Example: "Host Controller: Greaseweazle F7 Plus"
        //          "Firmware: 1.4"
        
        const char* host = strstr(r.stdout_buf, "Host Controller:");
        const char* fw = strstr(r.stdout_buf, "Firmware:");
        
        if (host) {
            const char* name_start = host + 17;
            while (*name_start == ' ') name_start++;
            
            const char* name_end = strchr(name_start, '\n');
            size_t name_len = name_end ? (size_t)(name_end - name_start) : strlen(name_start);
            
            if (name_len >= size) name_len = size - 1;
            strncpy(info, name_start, name_len);
            info[name_len] = '\0';
            
            // Append firmware
            if (fw && strlen(info) + 20 < size) {
                const char* ver_start = fw + 10;
                while (*ver_start == ' ') ver_start++;
                
                const char* ver_end = strchr(ver_start, '\n');
                size_t ver_len = ver_end ? (size_t)(ver_end - ver_start) : strlen(ver_start);
                
                strcat(info, " (FW ");
                strncat(info, ver_start, ver_len);
                strcat(info, ")");
            }
            
            free_cmd_result(&r);
            return true;
        }
    }
    
    free_cmd_result(&r);
    snprintf(info, size, "Greaseweazle (detection failed)");
    return false;
}

// ============================================================================
// Read Disk
// ============================================================================

typedef struct {
    uft_progress_callback_t callback;
    void*                   user_data;
    volatile bool*          cancel_flag;
} gw_context_t;

static uft_error_t gw_read_disk(void* context,
                                 const uft_tool_read_params_t* params,
                                 uft_unified_image_t* output) {
    if (!params || !output) return UFT_ERROR_NULL_POINTER;
    
    gw_context_t* ctx = (gw_context_t*)context;
    
    // Build command
    char cmd[1024];
    char temp_file[256];
    
    snprintf(temp_file, sizeof(temp_file), "/tmp/uft_gw_read_%d.scp", getpid());
    
    // Base command
    int len = snprintf(cmd, sizeof(cmd), "gw read");
    
    // Track range
    if (params->start_track >= 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len, " --tracks=%d", params->start_track);
        if (params->end_track > params->start_track) {
            len += snprintf(cmd + len, sizeof(cmd) - len, ":%d", params->end_track);
        }
    }
    
    // Revolutions
    if (params->revolutions > 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len, " --revs=%d", params->revolutions);
    }
    
    // Output file
    snprintf(cmd + len, sizeof(cmd) - len, " \"%s\" 2>&1", temp_file);
    
    // Report progress
    if (ctx && ctx->callback) {
        ctx->callback(ctx->user_data, 0, "Starting Greaseweazle read...");
    }
    
    // Execute
    cmd_result_t r;
    uft_error_t err = run_command(cmd, &r);
    
    if (err != UFT_OK) {
        if (ctx && ctx->callback) {
            ctx->callback(ctx->user_data, -1, r.stderr_buf ? r.stderr_buf : "Read failed");
        }
        free_cmd_result(&r);
        unlink(temp_file);
        return err;
    }
    
    free_cmd_result(&r);
    
    // Report progress
    if (ctx && ctx->callback) {
        ctx->callback(ctx->user_data, 50, "Reading flux data...");
    }
    
    // Open the captured file
    err = uft_image_open(output, temp_file);
    
    // Cleanup temp file
    unlink(temp_file);
    
    if (err == UFT_OK && ctx && ctx->callback) {
        ctx->callback(ctx->user_data, 100, "Read complete");
    }
    
    return err;
}

// ============================================================================
// Write Disk
// ============================================================================

static uft_error_t gw_write_disk(void* context,
                                  const uft_tool_write_params_t* params,
                                  const uft_unified_image_t* input) {
    if (!params || !input) return UFT_ERROR_NULL_POINTER;
    
    gw_context_t* ctx = (gw_context_t*)context;
    
    // Need a file path
    if (!input->path) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    // Build command
    char cmd[1024];
    int len = snprintf(cmd, sizeof(cmd), "gw write");
    
    // Track range
    if (params->start_track >= 0) {
        len += snprintf(cmd + len, sizeof(cmd) - len, " --tracks=%d", params->start_track);
        if (params->end_track > params->start_track) {
            len += snprintf(cmd + len, sizeof(cmd) - len, ":%d", params->end_track);
        }
    }
    
    // Erase first?
    if (params->erase_empty) {
        len += snprintf(cmd + len, sizeof(cmd) - len, " --erase-empty");
    }
    
    // Input file
    snprintf(cmd + len, sizeof(cmd) - len, " \"%s\" 2>&1", input->path);
    
    // Report progress
    if (ctx && ctx->callback) {
        ctx->callback(ctx->user_data, 0, "Starting Greaseweazle write...");
    }
    
    // Execute
    cmd_result_t r;
    uft_error_t err = run_command(cmd, &r);
    
    if (err != UFT_OK && ctx && ctx->callback) {
        ctx->callback(ctx->user_data, -1, r.stderr_buf ? r.stderr_buf : "Write failed");
    }
    
    if (err == UFT_OK && ctx && ctx->callback) {
        ctx->callback(ctx->user_data, 100, "Write complete");
    }
    
    free_cmd_result(&r);
    return err;
}

// ============================================================================
// Convert (via gw convert)
// ============================================================================

static uft_error_t gw_convert(void* context,
                               const char* input,
                               const char* output,
                               uft_format_t format) {
    if (!input || !output) return UFT_ERROR_NULL_POINTER;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gw convert \"%s\" \"%s\" 2>&1", input, output);
    
    cmd_result_t r;
    uft_error_t err = run_command(cmd, &r);
    free_cmd_result(&r);
    
    return err;
}

// ============================================================================
// Disk Info
// ============================================================================

static uft_error_t gw_get_disk_info(void* context, uft_tool_disk_info_t* info) {
    if (!info) return UFT_ERROR_NULL_POINTER;
    
    memset(info, 0, sizeof(*info));
    
    // Use 'gw rpm' to get disk info
    cmd_result_t r;
    uft_error_t err = run_command("gw rpm 2>&1", &r);
    
    if (err == UFT_OK && r.stdout_buf) {
        // Parse RPM
        const char* rpm_str = strstr(r.stdout_buf, "RPM:");
        if (rpm_str) {
            info->rpm = atof(rpm_str + 4);
        }
        info->disk_present = (info->rpm > 100);
    }
    
    free_cmd_result(&r);
    return err;
}

// ============================================================================
// Seek
// ============================================================================

static uft_error_t gw_seek(void* context, int track, int head) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "gw seek %d.%d 2>&1", track, head);
    
    cmd_result_t r;
    uft_error_t err = run_command(cmd, &r);
    free_cmd_result(&r);
    
    return err;
}

// ============================================================================
// Reset
// ============================================================================

static uft_error_t gw_reset(void* context) {
    cmd_result_t r;
    uft_error_t err = run_command("gw reset 2>&1", &r);
    free_cmd_result(&r);
    return err;
}

// ============================================================================
// Init / Cleanup
// ============================================================================

static uft_error_t gw_init(void** context) {
    gw_context_t* ctx = calloc(1, sizeof(gw_context_t));
    if (!ctx) return UFT_ERROR_NO_MEMORY;
    
    *context = ctx;
    return UFT_OK;
}

static void gw_cleanup(void* context) {
    free(context);
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_tool_adapter_t uft_tool_greaseweazle = {
    .name = "gw",
    .version = "1.0.0",
    .description = "Greaseweazle Command Line Tool",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE | 
                    UFT_TOOL_CAP_FLUX | UFT_TOOL_CAP_HARDWARE |
                    UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_INFO,
    .supported_formats = (1u << UFT_FORMAT_SCP) | (1u << UFT_FORMAT_HFE) |
                         (1u << UFT_FORMAT_IMG) | (1u << UFT_FORMAT_ADF),
    
    .init = gw_init,
    .cleanup = gw_cleanup,
    .is_available = gw_is_available,
    .detect_hardware = gw_detect_hardware,
    
    .read_disk = gw_read_disk,
    .write_disk = gw_write_disk,
    .convert = gw_convert,
    .get_disk_info = gw_get_disk_info,
    .seek = gw_seek,
    .reset = gw_reset,
};
