/**
 * @file uft_process.c
 * @brief Cross-Platform Process Execution Implementation (W-P1-001)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_process.h"
#include "uft/uft_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UFT_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

/*===========================================================================
 * CONSTANTS
 *===========================================================================*/

#define READ_BUFFER_SIZE 4096
#define MAX_OUTPUT_SIZE (16 * 1024 * 1024)  /* 16 MB max */

/*===========================================================================
 * TOOL REGISTRY
 *===========================================================================*/

static uft_tool_info_t g_tools[] = {
    [UFT_TOOL_DTC] = {
        .name = "dtc",
        .description = "KryoFlux Disk Tool Console",
        .url = "https://kryoflux.com"
    },
    [UFT_TOOL_NIBREAD] = {
        .name = "nibread",
        .description = "nibtools disk reader",
        .url = "https://github.com/c64-tools/nibtools"
    },
    [UFT_TOOL_NIBWRITE] = {
        .name = "nibwrite",
        .description = "nibtools disk writer",
        .url = "https://github.com/c64-tools/nibtools"
    },
    [UFT_TOOL_D64COPY] = {
        .name = "d64copy",
        .description = "OpenCBM disk copy",
        .url = "https://github.com/OpenCBM/OpenCBM"
    },
    [UFT_TOOL_CBMCTRL] = {
        .name = "cbmctrl",
        .description = "OpenCBM control tool",
        .url = "https://github.com/OpenCBM/OpenCBM"
    },
    [UFT_TOOL_GW] = {
        .name = "gw",
        .description = "Greaseweazle command tool",
        .url = "https://github.com/keirf/greaseweazle"
    },
    [UFT_TOOL_DISK_ANALYSE] = {
        .name = "disk-analyse",
        .description = "FluxEngine disk analyzer",
        .url = "https://github.com/keirf/disk-utilities"
    }
};

/*===========================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static char* append_output(char *buf, size_t *size, size_t *capacity,
                           const char *data, size_t data_len) {
    if (*size + data_len + 1 > *capacity) {
        size_t new_cap = (*capacity == 0) ? 1024 : *capacity * 2;
        while (new_cap < *size + data_len + 1) {
            new_cap *= 2;
        }
        if (new_cap > MAX_OUTPUT_SIZE) {
            new_cap = MAX_OUTPUT_SIZE;
        }
        char *new_buf = (char *)realloc(buf, new_cap);
        if (!new_buf) return buf;
        buf = new_buf;
        *capacity = new_cap;
    }
    
    memcpy(buf + *size, data, data_len);
    *size += data_len;
    buf[*size] = '\0';
    return buf;
}

/*===========================================================================
 * WINDOWS IMPLEMENTATION
 *===========================================================================*/

#ifdef UFT_PLATFORM_WINDOWS

int uft_process_exec(
    const char *command,
    const uft_process_options_t *options,
    uft_process_result_t *result)
{
    if (!command || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    if (!options) {
        options = &UFT_PROCESS_OPTIONS_DEFAULT;
    }
    
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL
    };
    
    HANDLE stdout_rd = NULL, stdout_wr = NULL;
    HANDLE stderr_rd = NULL, stderr_wr = NULL;
    
    /* Create pipes for stdout */
    if (options->capture_stdout) {
        if (!CreatePipe(&stdout_rd, &stdout_wr, &sa, 0)) {
            snprintf(result->error, sizeof(result->error), "CreatePipe failed");
            return -1;
        }
        SetHandleInformation(stdout_rd, HANDLE_FLAG_INHERIT, 0);
    }
    
    /* Create pipes for stderr */
    if (options->capture_stderr && !options->merge_stderr) {
        if (!CreatePipe(&stderr_rd, &stderr_wr, &sa, 0)) {
            if (stdout_rd) CloseHandle(stdout_rd);
            if (stdout_wr) CloseHandle(stdout_wr);
            snprintf(result->error, sizeof(result->error), "CreatePipe failed");
            return -1;
        }
        SetHandleInformation(stderr_rd, HANDLE_FLAG_INHERIT, 0);
    }
    
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = options->hide_window ? SW_HIDE : SW_SHOW;
    
    if (options->capture_stdout || options->capture_stderr) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = stdout_wr ? stdout_wr : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = options->merge_stderr ? stdout_wr : 
                       (stderr_wr ? stderr_wr : GetStdHandle(STD_ERROR_HANDLE));
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    
    PROCESS_INFORMATION pi = {0};
    
    char *cmd_copy = _strdup(command);
    BOOL success = CreateProcessA(
        NULL,
        cmd_copy,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        options->working_dir,
        &si,
        &pi
    );
    free(cmd_copy);
    
    /* Close write ends in parent */
    if (stdout_wr) CloseHandle(stdout_wr);
    if (stderr_wr) CloseHandle(stderr_wr);
    
    if (!success) {
        if (stdout_rd) CloseHandle(stdout_rd);
        if (stderr_rd) CloseHandle(stderr_rd);
        snprintf(result->error, sizeof(result->error), 
                 "CreateProcess failed: %lu", GetLastError());
        return -1;
    }
    
    /* Wait for process */
    DWORD wait_result = WaitForSingleObject(pi.hProcess, 
        options->timeout_ms > 0 ? options->timeout_ms : INFINITE);
    
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result->timed_out = true;
    }
    
    /* Get exit code */
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result->exit_code = (int)exit_code;
    
    /* Read stdout */
    if (stdout_rd) {
        size_t size = 0, capacity = 0;
        char buffer[READ_BUFFER_SIZE];
        DWORD bytes_read;
        
        while (ReadFile(stdout_rd, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
            result->stdout_data = append_output(result->stdout_data, &size, &capacity,
                                                buffer, bytes_read);
        }
        result->stdout_size = size;
        CloseHandle(stdout_rd);
    }
    
    /* Read stderr */
    if (stderr_rd) {
        size_t size = 0, capacity = 0;
        char buffer[READ_BUFFER_SIZE];
        DWORD bytes_read;
        
        while (ReadFile(stderr_rd, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
            result->stderr_data = append_output(result->stderr_data, &size, &capacity,
                                                buffer, bytes_read);
        }
        result->stderr_size = size;
        CloseHandle(stderr_rd);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    result->success = !result->timed_out && result->exit_code == 0;
    return 0;
}

bool uft_tool_exists(const char *tool) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "where %s >nul 2>&1", tool);
    return system(cmd) == 0;
}

int uft_tool_find(const char *tool, char *path, size_t path_size) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "where %s", tool);
    return uft_process_output_line(cmd, path, path_size);
}

#else

/*===========================================================================
 * POSIX IMPLEMENTATION
 *===========================================================================*/

int uft_process_exec(
    const char *command,
    const uft_process_options_t *options,
    uft_process_result_t *result)
{
    if (!command || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    if (!options) {
        options = &UFT_PROCESS_OPTIONS_DEFAULT;
    }
    
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    
    /* Create pipes */
    if (options->capture_stdout) {
        if (pipe(stdout_pipe) < 0) {
            snprintf(result->error, sizeof(result->error), "pipe() failed: %s", strerror(errno));
            return -1;
        }
    }
    
    if (options->capture_stderr && !options->merge_stderr) {
        if (pipe(stderr_pipe) < 0) {
            if (stdout_pipe[0] >= 0) {
                close(stdout_pipe[0]);
                close(stdout_pipe[1]);
            }
            snprintf(result->error, sizeof(result->error), "pipe() failed: %s", strerror(errno));
            return -1;
        }
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        snprintf(result->error, sizeof(result->error), "fork() failed: %s", strerror(errno));
        if (stdout_pipe[0] >= 0) {
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }
        if (stderr_pipe[0] >= 0) {
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        }
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        
        /* Change working directory */
        if (options->working_dir) {
            if (chdir(options->working_dir) < 0) {
                _exit(127);
            }
        }
        
        /* Redirect stdout */
        if (stdout_pipe[1] >= 0) {
            close(stdout_pipe[0]);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[1]);
        }
        
        /* Redirect stderr */
        if (options->merge_stderr && stdout_pipe[1] >= 0) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        } else if (stderr_pipe[1] >= 0) {
            close(stderr_pipe[0]);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]);
        }
        
        /* Execute */
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    
    /* Parent process */
    
    /* Close write ends */
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
    if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
    
    /* Set non-blocking */
    if (stdout_pipe[0] >= 0) {
        fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    }
    if (stderr_pipe[0] >= 0) {
        fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);
    }
    
    /* Read output with timeout */
    int timeout_remaining = options->timeout_ms > 0 ? options->timeout_ms : -1;
    size_t stdout_size = 0, stdout_cap = 0;
    size_t stderr_size = 0, stderr_cap = 0;
    char buffer[READ_BUFFER_SIZE];
    
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        int max_fd = -1;
        if (stdout_pipe[0] >= 0) {
            FD_SET(stdout_pipe[0], &read_fds);
            if (stdout_pipe[0] > max_fd) max_fd = stdout_pipe[0];
        }
        if (stderr_pipe[0] >= 0) {
            FD_SET(stderr_pipe[0], &read_fds);
            if (stderr_pipe[0] > max_fd) max_fd = stderr_pipe[0];
        }
        
        if (max_fd < 0) break;
        
        struct timeval tv;
        struct timeval *tv_ptr = NULL;
        if (timeout_remaining > 0) {
            tv.tv_sec = timeout_remaining / 1000;
            tv.tv_usec = (timeout_remaining % 1000) * 1000;
            tv_ptr = &tv;
        }
        
        int sel = select(max_fd + 1, &read_fds, NULL, NULL, tv_ptr);
        
        if (sel < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (sel == 0) {
            /* Timeout */
            result->timed_out = true;
            kill(pid, SIGKILL);
            break;
        }
        
        bool any_read = false;
        
        if (stdout_pipe[0] >= 0 && FD_ISSET(stdout_pipe[0], &read_fds)) {
            ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                result->stdout_data = append_output(result->stdout_data, 
                    &stdout_size, &stdout_cap, buffer, n);
                any_read = true;
            } else if (n == 0) {
                close(stdout_pipe[0]);
                stdout_pipe[0] = -1;
            }
        }
        
        if (stderr_pipe[0] >= 0 && FD_ISSET(stderr_pipe[0], &read_fds)) {
            ssize_t n = read(stderr_pipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                result->stderr_data = append_output(result->stderr_data,
                    &stderr_size, &stderr_cap, buffer, n);
                any_read = true;
            } else if (n == 0) {
                close(stderr_pipe[0]);
                stderr_pipe[0] = -1;
            }
        }
        
        if (!any_read && stdout_pipe[0] < 0 && stderr_pipe[0] < 0) {
            break;
        }
    }
    
    result->stdout_size = stdout_size;
    result->stderr_size = stderr_size;
    
    /* Close remaining pipes */
    if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
    if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
    
    /* Wait for child */
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result->exit_code = 128 + WTERMSIG(status);
    }
    
    result->success = !result->timed_out && result->exit_code == 0;
    return 0;
}

bool uft_tool_exists(const char *tool) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", tool);
    return system(cmd) == 0;
}

int uft_tool_find(const char *tool, char *path, size_t path_size) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "which %s 2>/dev/null", tool);
    return uft_process_output_line(cmd, path, path_size);
}

#endif

/*===========================================================================
 * COMMON IMPLEMENTATION
 *===========================================================================*/

void uft_process_result_free(uft_process_result_t *result) {
    if (!result) return;
    free(result->stdout_data);
    free(result->stderr_data);
    memset(result, 0, sizeof(*result));
}

int uft_process_exec_args(
    const char *program,
    const char **args,
    const uft_process_options_t *options,
    uft_process_result_t *result)
{
    if (!program) return -1;
    
    /* Build command string */
    char cmd[4096];
    size_t pos = 0;
    
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, "%s", program);
    
    if (args) {
        for (int i = 0; args[i] && pos < sizeof(cmd) - 1; i++) {
            pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", args[i]);
        }
    }
    
    return uft_process_exec(cmd, options, result);
}

int uft_process_run(const char *command) {
    uft_process_result_t result;
    uft_process_options_t opts = UFT_PROCESS_OPTIONS_DEFAULT;
    opts.capture_stdout = false;
    opts.capture_stderr = false;
    
    if (uft_process_exec(command, &opts, &result) < 0) {
        return -1;
    }
    
    int code = result.exit_code;
    uft_process_result_free(&result);
    return code;
}

int uft_process_output_line(
    const char *command,
    char *output,
    size_t output_size)
{
    if (!command || !output || output_size == 0) return -1;
    
    output[0] = '\0';
    
    uft_process_result_t result;
    if (uft_process_exec(command, NULL, &result) < 0) {
        return -1;
    }
    
    if (result.stdout_data && result.stdout_size > 0) {
        /* Copy first line */
        size_t len = 0;
        while (len < result.stdout_size && len < output_size - 1) {
            if (result.stdout_data[len] == '\n' || result.stdout_data[len] == '\r') {
                break;
            }
            output[len] = result.stdout_data[len];
            len++;
        }
        output[len] = '\0';
    }
    
    int rc = result.success ? 0 : -1;
    uft_process_result_free(&result);
    return rc;
}

int uft_tool_version(const char *tool, char *version, size_t version_size) {
    if (!tool || !version || version_size == 0) return -1;
    
    version[0] = '\0';
    
    /* Try common version flags */
    const char *flags[] = {"--version", "-v", "-V", "version", NULL};
    
    for (int i = 0; flags[i]; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s %s 2>&1", tool, flags[i]);
        
        if (uft_process_output_line(cmd, version, version_size) == 0) {
            if (version[0] != '\0') {
                return 0;
            }
        }
    }
    
    return -1;
}

int uft_tool_detect_all(uft_tool_info_t *tools, int max_tools) {
    int count = 0;
    
    for (int i = 0; i < UFT_TOOL_COUNT && count < max_tools; i++) {
        uft_tool_info_t *t = &tools[count];
        *t = g_tools[i];
        
        t->available = uft_tool_exists(t->name);
        if (t->available) {
            uft_tool_find(t->name, t->path, sizeof(t->path));
            uft_tool_version(t->name, t->version, sizeof(t->version));
        }
        count++;
    }
    
    return count;
}

const uft_tool_info_t* uft_tool_get_info(uft_tool_id_t tool) {
    if (tool >= UFT_TOOL_COUNT) return NULL;
    return &g_tools[tool];
}

/*===========================================================================
 * ASYNC PROCESS (Stub)
 *===========================================================================*/

uft_async_process_t* uft_process_start_async(
    const char *command,
    const uft_process_options_t *options,
    uft_process_callback_t callback,
    void *user_data)
{
    (void)command;
    (void)options;
    (void)callback;
    (void)user_data;
    /* TODO: Implement async process support */
    return NULL;
}

bool uft_process_is_running(uft_async_process_t *proc) {
    (void)proc;
    return false;
}

int uft_process_wait(uft_async_process_t *proc, int timeout_ms) {
    (void)proc;
    (void)timeout_ms;
    return -1;
}

int uft_process_kill(uft_async_process_t *proc) {
    (void)proc;
    return -1;
}

void uft_process_free(uft_async_process_t *proc) {
    (void)proc;
}
