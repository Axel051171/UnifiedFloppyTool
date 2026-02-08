/**
 * @file uft_global_mutex.c
 * @brief Global State Mutex Implementation
 */

#include "uft/core/uft_global_mutex.h"
#include <stdbool.h>

static uft_mutex_t g_global_mutex;
static bool g_mutex_initialized = false;

int uft_global_mutex_init(void) {
    if (g_mutex_initialized) return 0;
    
    int result = uft_mutex_init(&g_global_mutex);
    if (result == 0) {
        g_mutex_initialized = true;
    }
    return result;
}

void uft_global_mutex_destroy(void) {
    if (!g_mutex_initialized) return;
    
    uft_mutex_destroy(&g_global_mutex);
    g_mutex_initialized = false;
}

void uft_global_lock(void) {
    if (!g_mutex_initialized) {
        uft_global_mutex_init();
    }
    uft_mutex_lock(&g_global_mutex);
}

void uft_global_unlock(void) {
    if (g_mutex_initialized) {
        uft_mutex_unlock(&g_global_mutex);
    }
}
