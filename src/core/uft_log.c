/**
 * @file uft_log.c
 * @brief Unified Logging Implementation
 */

#include "uft_log.h"
#include <time.h>
#include <string.h>

/* Globals */
uft_log_level_t uft_log_level = UFT_LOG_INFO;
FILE* uft_log_file = NULL;
static void (*log_callback)(uft_log_level_t, const char*) = NULL;

static const char* level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char* level_colors[] = {
    "\x1b[90m",  /* TRACE: gray */
    "\x1b[36m",  /* DEBUG: cyan */
    "\x1b[32m",  /* INFO:  green */
    "\x1b[33m",  /* WARN:  yellow */
    "\x1b[31m",  /* ERROR: red */
    "\x1b[35m",  /* FATAL: magenta */
};

void uft_log(uft_log_level_t level, const char* file, int line,
             const char* func, const char* fmt, ...) {
    if (level < uft_log_level) return;
    
    FILE* out = uft_log_file ? uft_log_file : stderr;
    
    /* Timestamp */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);
    
    /* Extract filename without path */
    const char* fname = strrchr(file, '/');
    fname = fname ? fname + 1 : file;
    
    /* Format message */
    char msgbuf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
    va_end(args);
    
    /* Output with color (if terminal) */
    #ifdef _WIN32
    fprintf(out, "%s [%-5s] %s:%d (%s): %s\n",
            timebuf, level_names[level], fname, line, func, msgbuf);
    #else
    fprintf(out, "%s %s[%-5s]\x1b[0m %s:%d (%s): %s\n",
            timebuf, level_colors[level], level_names[level], 
            fname, line, func, msgbuf);
    #endif
    
    fflush(out);
    
    /* Callback */
    if (log_callback) {
        log_callback(level, msgbuf);
    }
}

void uft_log_set_level(uft_log_level_t level) {
    uft_log_level = level;
}

void uft_log_set_file(FILE* file) {
    uft_log_file = file;
}

void uft_log_set_callback(void (*cb)(uft_log_level_t, const char*)) {
    log_callback = cb;
}
