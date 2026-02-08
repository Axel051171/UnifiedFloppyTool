#include "uft/compat/uft_platform.h"
/**
 * @file uft_dtc_wrapper.c
 * @brief DTC (Disk Tool Copy) Wrapper Implementation
 * 
 * EXT4-014: External tool integration wrapper
 * 
 * Features:
 * - External tool invocation
 * - Command-line building
 * - Output parsing
 * - Error handling
 * - Cross-platform support
 */

#include "uft/tools/uft_dtc_wrapper.h"
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <stdio.h>
#include "uft/core/uft_safe_parse.h"

#ifdef _WIN32
#include <windows.h>
#include "uft/core/uft_safe_parse.h"
#else
#include <unistd.h>
#include "uft/core/uft_safe_parse.h"
#include <sys/wait.h>
#include "uft/core/uft_safe_parse.h"
#include <spawn.h>
#include "uft/core/uft_safe_parse.h"
extern char **environ;
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_CMD_LEN     4096
#define MAX_OUTPUT_LEN  65536
#define TIMEOUT_MS      30000

/* Tool paths */
static const char *TOOL_PATHS[] = {
    "dtc",
    "/usr/local/bin/dtc",
    "/usr/bin/dtc",
    "./dtc",
    NULL
};

/*===========================================================================
 * Tool Discovery
 *===========================================================================*/

int uft_dtc_find_tool(char *path, size_t size)
{
    if (!path || size == 0) return -1;
    
    for (int i = 0; TOOL_PATHS[i] != NULL; i++) {
        const char *test_path = TOOL_PATHS[i];
        
#ifdef _WIN32
        /* Check if file exists on Windows */
        DWORD attrs = GetFileAttributesA(test_path);
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, test_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
#else
        /* Check if file is executable on Unix */
        if (access(test_path, X_OK) == 0) {
            strncpy(path, test_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
#endif
    }
    
    return -1;  /* Not found */
}

/*===========================================================================
 * Command Execution
 *===========================================================================*/

#ifdef _WIN32

static int execute_command(const char *cmd, char *output, size_t output_size, int *exit_code)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return -1;
    }
    
    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    
    char cmd_copy[MAX_CMD_LEN];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    
    if (!CreateProcessA(NULL, cmd_copy, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }
    
    CloseHandle(hWritePipe);
    
    /* Read output */
    DWORD bytes_read;
    size_t total_read = 0;
    
    while (total_read < output_size - 1) {
        if (!ReadFile(hReadPipe, output + total_read, 
                      (DWORD)(output_size - total_read - 1), &bytes_read, NULL)) {
            break;
        }
        if (bytes_read == 0) break;
        total_read += bytes_read;
    }
    output[total_read] = '\0';
    
    /* Wait for process */
    WaitForSingleObject(pi.hProcess, TIMEOUT_MS);
    
    DWORD code;
    GetExitCodeProcess(pi.hProcess, &code);
    *exit_code = (int)code;
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    return 0;
}

#else

static int execute_command(const char *cmd, char *output, size_t output_size, int *exit_code)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
    
    /* Parent process */
    close(pipefd[1]);
    
    size_t total_read = 0;
    ssize_t bytes;
    
    while (total_read < output_size - 1) {
        bytes = read(pipefd[0], output + total_read, output_size - total_read - 1);
        if (bytes <= 0) break;
        total_read += bytes;
    }
    output[total_read] = '\0';
    
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else {
        *exit_code = -1;
    }
    
    return 0;
}

#endif

/*===========================================================================
 * DTC Wrapper Functions
 *===========================================================================*/

int uft_dtc_init(uft_dtc_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    /* Find tool */
    if (uft_dtc_find_tool(ctx->tool_path, sizeof(ctx->tool_path)) != 0) {
        ctx->available = false;
        return -1;
    }
    
    ctx->available = true;
    ctx->timeout_ms = TIMEOUT_MS;
    
    return 0;
}

int uft_dtc_read_track(uft_dtc_ctx_t *ctx, int drive, int track, int side,
                       const char *output_file, uft_dtc_result_t *result)
{
    if (!ctx || !result || !ctx->available) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Build command */
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), 
             "%s -r -d%d -t%d -s%d -o \"%s\"",
             ctx->tool_path, drive, track, side, output_file);
    
    /* Execute */
    char output[MAX_OUTPUT_LEN];
    int exit_code;
    
    if (execute_command(cmd, output, sizeof(output), &exit_code) != 0) {
        result->error = true;
        snprintf(result->message, sizeof(result->message), "Execution failed");
        return -1;
    }
    
    result->exit_code = exit_code;
    result->error = (exit_code != 0);
    
    strncpy(result->output, output, sizeof(result->output) - 1);
    
    /* Parse output for flux count */
    char *p = strstr(output, "flux:");
    if (p) {
        { int32_t t; if(uft_parse_int32(p+5,&t,10) && t>=0) result->flux_count=(uint32_t)t; }
    }
    
    return result->error ? -1 : 0;
}

int uft_dtc_write_track(uft_dtc_ctx_t *ctx, int drive, int track, int side,
                        const char *input_file, uft_dtc_result_t *result)
{
    if (!ctx || !result || !ctx->available) return -1;
    
    memset(result, 0, sizeof(*result));
    
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "%s -w -d%d -t%d -s%d -i \"%s\"",
             ctx->tool_path, drive, track, side, input_file);
    
    char output[MAX_OUTPUT_LEN];
    int exit_code;
    
    if (execute_command(cmd, output, sizeof(output), &exit_code) != 0) {
        result->error = true;
        snprintf(result->message, sizeof(result->message), "Execution failed");
        return -1;
    }
    
    result->exit_code = exit_code;
    result->error = (exit_code != 0);
    strncpy(result->output, output, sizeof(result->output) - 1);
    
    return result->error ? -1 : 0;
}

int uft_dtc_info(uft_dtc_ctx_t *ctx, uft_dtc_result_t *result)
{
    if (!ctx || !result || !ctx->available) return -1;
    
    memset(result, 0, sizeof(*result));
    
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "%s --version", ctx->tool_path);
    
    char output[MAX_OUTPUT_LEN];
    int exit_code;
    
    if (execute_command(cmd, output, sizeof(output), &exit_code) != 0) {
        result->error = true;
        return -1;
    }
    
    result->exit_code = exit_code;
    strncpy(result->output, output, sizeof(result->output) - 1);
    
    /* Parse version */
    char *p = strstr(output, "version");
    if (p) {
        strncpy(result->version, p + 8, sizeof(result->version) - 1);
        /* Trim newline */
        char *nl = strchr(result->version, '\n');
        if (nl) *nl = '\0';
    }
    
    return 0;
}

/*===========================================================================
 *===========================================================================*/

int uft_gw_find_tool(char *path, size_t size)
{
    static const char *uft_gw_paths[] = {
        "gw",
        "/usr/local/bin/gw",
        "/usr/bin/gw",
        "./gw",
        NULL
    };
    
    if (!path || size == 0) return -1;
    
    for (int i = 0; uft_gw_paths[i] != NULL; i++) {
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(uft_gw_paths[i]);
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, uft_gw_paths[i], size - 1);
            path[size - 1] = '\0';
            return 0;
        }
#else
        if (access(uft_gw_paths[i], X_OK) == 0) {
            strncpy(path, uft_gw_paths[i], size - 1);
            path[size - 1] = '\0';
            return 0;
        }
#endif
    }
    
    return -1;
}

int uft_gw_read(const char *tool_path, const char *output_file,
                int tracks, int sides, uft_dtc_result_t *result)
{
    if (!tool_path || !output_file || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "%s read --tracks=c=0-%d --heads=0-%d \"%s\"",
             tool_path, tracks - 1, sides - 1, output_file);
    
    char output[MAX_OUTPUT_LEN];
    int exit_code;
    
    if (execute_command(cmd, output, sizeof(output), &exit_code) != 0) {
        result->error = true;
        return -1;
    }
    
    result->exit_code = exit_code;
    result->error = (exit_code != 0);
    strncpy(result->output, output, sizeof(result->output) - 1);
    
    return result->error ? -1 : 0;
}

/*===========================================================================
 *===========================================================================*/

int uft_fe_find_tool(char *path, size_t size)
{
    static const char *fe_paths[] = {
        "fluxengine",
        "/usr/local/bin/fluxengine",
        "/usr/bin/fluxengine",
        "./fluxengine",
        NULL
    };
    
    if (!path || size == 0) return -1;
    
    for (int i = 0; fe_paths[i] != NULL; i++) {
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(fe_paths[i]);
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, fe_paths[i], size - 1);
            path[size - 1] = '\0';
            return 0;
        }
#else
        if (access(fe_paths[i], X_OK) == 0) {
            strncpy(path, fe_paths[i], size - 1);
            path[size - 1] = '\0';
            return 0;
        }
#endif
    }
    
    return -1;
}

int uft_fe_read(const char *tool_path, const char *format,
                const char *output_file, uft_dtc_result_t *result)
{
    if (!tool_path || !format || !output_file || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "%s read %s -o \"%s\"",
             tool_path, format, output_file);
    
    char output[MAX_OUTPUT_LEN];
    int exit_code;
    
    if (execute_command(cmd, output, sizeof(output), &exit_code) != 0) {
        result->error = true;
        return -1;
    }
    
    result->exit_code = exit_code;
    result->error = (exit_code != 0);
    strncpy(result->output, output, sizeof(result->output) - 1);
    
    return result->error ? -1 : 0;
}
