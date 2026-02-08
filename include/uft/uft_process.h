/**
 * @file uft_process.h
 * @brief Cross-Platform Process Execution (W-P1-001)
 * 
 * Unified API for:
 * - Command execution (replaces popen/fork)
 * - Process output capture
 * - Tool detection
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_PROCESS_H
#define UFT_PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PROCESS RESULT
 *===========================================================================*/

/**
 * @brief Process execution result
 */
typedef struct {
    int exit_code;           /**< Process exit code */
    char *stdout_data;       /**< Captured stdout (caller must free) */
    size_t stdout_size;      /**< Size of stdout data */
    char *stderr_data;       /**< Captured stderr (caller must free) */
    size_t stderr_size;      /**< Size of stderr data */
    bool timed_out;          /**< True if process timed out */
    bool success;            /**< True if process completed successfully */
    char error[256];         /**< Error message if failed */
} uft_process_result_t;

/*===========================================================================
 * PROCESS OPTIONS
 *===========================================================================*/

/**
 * @brief Process execution options
 */
typedef struct {
    const char *working_dir;    /**< Working directory (NULL = current) */
    int timeout_ms;             /**< Timeout in ms (0 = infinite) */
    bool capture_stdout;        /**< Capture stdout */
    bool capture_stderr;        /**< Capture stderr */
    bool merge_stderr;          /**< Merge stderr into stdout */
    bool hide_window;           /**< Hide window (Windows) */
    const char **env;           /**< Environment (NULL-terminated, NULL = inherit) */
} uft_process_options_t;

/**
 * @brief Default process options
 */
static const uft_process_options_t UFT_PROCESS_OPTIONS_DEFAULT = {
    .working_dir = NULL,
    .timeout_ms = 30000,        /* 30 seconds */
    .capture_stdout = true,
    .capture_stderr = true,
    .merge_stderr = false,
    .hide_window = true,
    .env = NULL
};

/*===========================================================================
 * COMMAND EXECUTION
 *===========================================================================*/

/**
 * @brief Execute command and capture output
 * 
 * Cross-platform replacement for popen().
 * 
 * @param command Command to execute
 * @param options Execution options (NULL for defaults)
 * @param result Output result (caller must call uft_process_result_free)
 * @return 0 on success, <0 on error
 * 
 * Example:
 * @code
 *   uft_process_result_t result;
 *   if (uft_process_exec("dtc -f0 -i0", NULL, &result) == 0) {
 *       printf("Output: %s\n", result.stdout_data);
 *       uft_process_result_free(&result);
 *   }
 * @endcode
 */
int uft_process_exec(
    const char *command,
    const uft_process_options_t *options,
    uft_process_result_t *result);

/**
 * @brief Execute command with arguments array
 * 
 * @param program Program path
 * @param args Arguments (NULL-terminated array)
 * @param options Execution options
 * @param result Output result
 * @return 0 on success, <0 on error
 */
int uft_process_exec_args(
    const char *program,
    const char **args,
    const uft_process_options_t *options,
    uft_process_result_t *result);

/**
 * @brief Free process result resources
 */
void uft_process_result_free(uft_process_result_t *result);

/*===========================================================================
 * SIMPLE EXECUTION
 *===========================================================================*/

/**
 * @brief Execute command and get exit code only
 * 
 * @param command Command to execute
 * @return Exit code, or -1 on error
 */
int uft_process_run(const char *command);

/**
 * @brief Execute command and get first line of output
 * 
 * @param command Command to execute
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return 0 on success, <0 on error
 */
int uft_process_output_line(
    const char *command,
    char *output,
    size_t output_size);

/*===========================================================================
 * TOOL DETECTION
 *===========================================================================*/

/**
 * @brief Check if command/tool exists in PATH
 * 
 * @param tool Tool name (e.g., "dtc", "nibread")
 * @return true if tool is available
 */
bool uft_tool_exists(const char *tool);

/**
 * @brief Get full path to tool
 * 
 * @param tool Tool name
 * @param path Output buffer for full path
 * @param path_size Size of path buffer
 * @return 0 on success, <0 if not found
 */
int uft_tool_find(const char *tool, char *path, size_t path_size);

/**
 * @brief Get tool version string
 * 
 * Tries common version flags: --version, -v, -V
 * 
 * @param tool Tool name
 * @param version Output buffer
 * @param version_size Size of version buffer
 * @return 0 on success, <0 on error
 */
int uft_tool_version(const char *tool, char *version, size_t version_size);

/*===========================================================================
 * TOOL REGISTRY
 *===========================================================================*/

/**
 * @brief Known tool information
 */
typedef struct {
    const char *name;           /**< Tool name */
    const char *description;    /**< Description */
    const char *url;            /**< Download URL */
    bool available;             /**< Detected on system */
    char path[256];             /**< Path if found */
    char version[64];           /**< Version if found */
} uft_tool_info_t;

/**
 * @brief Detect all known floppy tools
 * 
 * @param tools Array to fill
 * @param max_tools Maximum tools to return
 * @return Number of tools detected
 */
int uft_tool_detect_all(uft_tool_info_t *tools, int max_tools);

/**
 * @brief Known floppy tools
 */
typedef enum {
    UFT_TOOL_DTC,           /**< KryoFlux DTC */
    UFT_TOOL_NIBREAD,       /**< nibtools nibread */
    UFT_TOOL_NIBWRITE,      /**< nibtools nibwrite */
    UFT_TOOL_D64COPY,       /**< OpenCBM d64copy */
    UFT_TOOL_CBMCTRL,       /**< OpenCBM cbmctrl */
    UFT_TOOL_GW,            /**< Greaseweazle gw */
    UFT_TOOL_DISK_ANALYSE,  /**< FluxEngine disk-analyse */
    UFT_TOOL_COUNT
} uft_tool_id_t;

/**
 * @brief Get info for specific tool
 */
const uft_tool_info_t* uft_tool_get_info(uft_tool_id_t tool);

/*===========================================================================
 * ASYNC EXECUTION (Future)
 *===========================================================================*/

/**
 * @brief Async process handle
 */
typedef struct uft_async_process uft_async_process_t;

/**
 * @brief Callback for async output
 */
typedef void (*uft_process_callback_t)(
    const char *data,
    size_t size,
    bool is_stderr,
    void *user_data);

/**
 * @brief Start async process
 */
uft_async_process_t* uft_process_start_async(
    const char *command,
    const uft_process_options_t *options,
    uft_process_callback_t callback,
    void *user_data);

/**
 * @brief Check if async process is running
 */
bool uft_process_is_running(uft_async_process_t *proc);

/**
 * @brief Wait for async process to complete
 */
int uft_process_wait(uft_async_process_t *proc, int timeout_ms);

/**
 * @brief Kill async process
 */
int uft_process_kill(uft_async_process_t *proc);

/**
 * @brief Free async process handle
 */
void uft_process_free(uft_async_process_t *proc);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROCESS_H */
