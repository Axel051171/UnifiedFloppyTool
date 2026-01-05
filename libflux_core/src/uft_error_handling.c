/**
 * @file uft_error_handling.c
 * @brief Error Handling Framework Implementation
 * 
 * @version 3.0.0
 * @date 2024-12-27
 */

#include "uft_error_handling.h"
#include <string.h>
#include <stdio.h>

/* Thread-local error context */
__thread uft_error_context_t g_error_ctx = {0};

/**
 * Get last error context
 */
const uft_error_context_t* uft_get_last_error(void) {
    return &g_error_ctx;
}

/**
 * Get detailed error message
 */
const char* uft_get_error_message(void) {
    return g_error_ctx.message;
}

/**
 * Print error stack trace
 */
void uft_print_error_stack(FILE* fp) {
    if (!fp) fp = stderr;
    
    fprintf(fp, "ERROR: %s\n", uft_strerror(g_error_ctx.code));
    fprintf(fp, "  Location: %s:%d in %s()\n",
            g_error_ctx.file ? g_error_ctx.file : "unknown",
            g_error_ctx.line,
            g_error_ctx.function ? g_error_ctx.function : "unknown");
    fprintf(fp, "  Message: %s\n", g_error_ctx.message);
    
    if (g_error_ctx.cause_code != UFT_SUCCESS) {
        fprintf(fp, "  Caused by: %s\n", uft_strerror(g_error_ctx.cause_code));
        fprintf(fp, "  Cause message: %s\n", g_error_ctx.cause_message);
    }
}

/**
 * Clear error context
 */
void uft_clear_error(void) {
    memset(&g_error_ctx, 0, sizeof(g_error_ctx));
}
