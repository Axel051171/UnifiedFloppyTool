/**
 * @file uft_gui_bridge_v2.c
 * @brief GUI-Core Bridge - GOD MODE OPTIMIZED
 * @version 5.3.1-GOD
 *
 * This module provides the bridge between Qt GUI and UFT core,
 * handling parameter validation, synchronization, and callbacks.
 *
 * KEY FEATURES:
 * - Bidirectional parameter sync (GUI ↔ Core)
 * - Parameter change callbacks
 * - Preset management
 * - Thread-safe parameter access
 * - JSON serialization for profiles
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "uft/uft_atomics.h"
#include "uft/uft_threading.h"

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define UFT_MAX_PARAM_NAME      64
#define UFT_MAX_PARAM_VALUE     256
#define UFT_MAX_CALLBACKS       32
#define UFT_MAX_PARAMS          256
#define UFT_MAX_PRESETS         64
#define UFT_MAX_PRESET_NAME     64

/*============================================================================
 * TYPES
 *============================================================================*/

typedef enum {
    UFT_PARAM_BOOL = 0,
    UFT_PARAM_INT,
    UFT_PARAM_FLOAT,
    UFT_PARAM_STRING,
    UFT_PARAM_ENUM,
    UFT_PARAM_FLAGS
} uft_param_type_t;

typedef union {
    bool b;
    int i;
    float f;
    char s[UFT_MAX_PARAM_VALUE];
    int e;
    uint32_t flags;
} uft_param_value_t;

typedef struct {
    char name[UFT_MAX_PARAM_NAME];
    uft_param_type_t type;
    uft_param_value_t value;
    uft_param_value_t default_val;
    uft_param_value_t min_val;
    uft_param_value_t max_val;
    bool is_modified;
    bool is_gui_param;          // True if visible in GUI
    const char* category;       // Parameter category
    const char* description;    // Human-readable description
    const char* unit;           // Unit string (%, µs, etc.)
} uft_param_entry_t;

typedef void (*uft_param_callback_t)(const char* name,
                                      const uft_param_value_t* old_val,
                                      const uft_param_value_t* new_val,
                                      void* user_data);

typedef struct {
    uft_param_callback_t callback;
    void* user_data;
    char filter[UFT_MAX_PARAM_NAME];  // Empty = all params
} uft_callback_entry_t;

typedef struct {
    char name[UFT_MAX_PRESET_NAME];
    char description[UFT_MAX_PARAM_VALUE];
    int param_count;
    struct {
        char name[UFT_MAX_PARAM_NAME];
        uft_param_value_t value;
    } params[UFT_MAX_PARAMS];
} uft_preset_t;

typedef struct {
    // Parameters
    uft_param_entry_t params[UFT_MAX_PARAMS];
    int param_count;
    
    // Callbacks
    uft_callback_entry_t callbacks[UFT_MAX_CALLBACKS];
    int callback_count;
    
    // Presets
    uft_preset_t presets[UFT_MAX_PRESETS];
    int preset_count;
    
    // Thread safety
    pthread_mutex_t mutex;
    uft_atomic_int version;  // Incremented on each change
    
    // State
    bool is_initialized;
    bool changes_pending;
} uft_gui_bridge_t;

/*============================================================================
 * GLOBAL INSTANCE
 *============================================================================*/

static uft_gui_bridge_t g_bridge = {
    .is_initialized = false
};

/*============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * @brief Initialize the GUI bridge
 */
int uft_gui_bridge_init(void) {
    if (g_bridge.is_initialized) {
        return 0;  // Already initialized
    }
    
    memset(&g_bridge, 0, sizeof(g_bridge));
    
    if (pthread_mutex_init(&g_bridge.mutex, NULL) != 0) {
        return -1;
    }
    
    uft_atomic_store(&g_bridge.version, 1);
    g_bridge.is_initialized = true;
    
    return 0;
}

/**
 * @brief Shutdown the GUI bridge
 */
void uft_gui_bridge_shutdown(void) {
    if (!g_bridge.is_initialized) return;
    
    pthread_mutex_destroy(&g_bridge.mutex);
    g_bridge.is_initialized = false;
}

/*============================================================================
 * PARAMETER REGISTRATION
 *============================================================================*/

/**
 * @brief Find parameter by name
 */
static uft_param_entry_t* find_param(const char* name) {
    for (int i = 0; i < g_bridge.param_count; i++) {
        if (strcmp(g_bridge.params[i].name, name) == 0) {
            return &g_bridge.params[i];
        }
    }
    return NULL;
}

/**
 * @brief Register a boolean parameter
 */
int uft_gui_register_bool(const char* name, bool default_val,
                          const char* category, const char* desc) {
    if (!name || g_bridge.param_count >= UFT_MAX_PARAMS) return -1;
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    if (!p) {
        p = &g_bridge.params[g_bridge.param_count++];
    }
    
    strncpy(p->name, name, UFT_MAX_PARAM_NAME - 1);
    p->type = UFT_PARAM_BOOL;
    p->value.b = default_val;
    p->default_val.b = default_val;
    p->is_gui_param = true;
    p->category = category;
    p->description = desc;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/**
 * @brief Register an integer parameter
 */
int uft_gui_register_int(const char* name, int default_val,
                         int min_val, int max_val,
                         const char* category, const char* desc,
                         const char* unit) {
    if (!name || g_bridge.param_count >= UFT_MAX_PARAMS) return -1;
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    if (!p) {
        p = &g_bridge.params[g_bridge.param_count++];
    }
    
    strncpy(p->name, name, UFT_MAX_PARAM_NAME - 1);
    p->type = UFT_PARAM_INT;
    p->value.i = default_val;
    p->default_val.i = default_val;
    p->min_val.i = min_val;
    p->max_val.i = max_val;
    p->is_gui_param = true;
    p->category = category;
    p->description = desc;
    p->unit = unit;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/**
 * @brief Register a float parameter
 */
int uft_gui_register_float(const char* name, float default_val,
                           float min_val, float max_val,
                           const char* category, const char* desc,
                           const char* unit) {
    if (!name || g_bridge.param_count >= UFT_MAX_PARAMS) return -1;
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    if (!p) {
        p = &g_bridge.params[g_bridge.param_count++];
    }
    
    strncpy(p->name, name, UFT_MAX_PARAM_NAME - 1);
    p->type = UFT_PARAM_FLOAT;
    p->value.f = default_val;
    p->default_val.f = default_val;
    p->min_val.f = min_val;
    p->max_val.f = max_val;
    p->is_gui_param = true;
    p->category = category;
    p->description = desc;
    p->unit = unit;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/*============================================================================
 * PARAMETER ACCESS (THREAD-SAFE)
 *============================================================================*/

/**
 * @brief Get boolean parameter value
 */
bool uft_gui_get_bool(const char* name, bool default_val) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    bool result = p ? p->value.b : default_val;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return result;
}

/**
 * @brief Get integer parameter value
 */
int uft_gui_get_int(const char* name, int default_val) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    int result = p ? p->value.i : default_val;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return result;
}

/**
 * @brief Get float parameter value
 */
float uft_gui_get_float(const char* name, float default_val) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    float result = p ? p->value.f : default_val;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return result;
}

/**
 * @brief Set boolean parameter value
 */
int uft_gui_set_bool(const char* name, bool value) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    if (!p) {
        pthread_mutex_unlock(&g_bridge.mutex);
        return -1;
    }
    
    uft_param_value_t old_val = p->value;
    p->value.b = value;
    p->is_modified = (value != p->default_val.b);
    uft_atomic_fetch_add(&g_bridge.version, 1);
    g_bridge.changes_pending = true;
    
    // Fire callbacks
    for (int i = 0; i < g_bridge.callback_count; i++) {
        if (g_bridge.callbacks[i].filter[0] == '\0' ||
            strcmp(g_bridge.callbacks[i].filter, name) == 0) {
            g_bridge.callbacks[i].callback(name, &old_val, &p->value,
                                           g_bridge.callbacks[i].user_data);
        }
    }
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/**
 * @brief Set integer parameter value
 */
int uft_gui_set_int(const char* name, int value) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    if (!p) {
        pthread_mutex_unlock(&g_bridge.mutex);
        return -1;
    }
    
    // Clamp to range
    if (value < p->min_val.i) value = p->min_val.i;
    if (value > p->max_val.i) value = p->max_val.i;
    
    uft_param_value_t old_val = p->value;
    p->value.i = value;
    p->is_modified = (value != p->default_val.i);
    uft_atomic_fetch_add(&g_bridge.version, 1);
    g_bridge.changes_pending = true;
    
    // Fire callbacks
    for (int i = 0; i < g_bridge.callback_count; i++) {
        if (g_bridge.callbacks[i].filter[0] == '\0' ||
            strcmp(g_bridge.callbacks[i].filter, name) == 0) {
            g_bridge.callbacks[i].callback(name, &old_val, &p->value,
                                           g_bridge.callbacks[i].user_data);
        }
    }
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/**
 * @brief Set float parameter value
 */
int uft_gui_set_float(const char* name, float value) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_param_entry_t* p = find_param(name);
    if (!p) {
        pthread_mutex_unlock(&g_bridge.mutex);
        return -1;
    }
    
    // Clamp to range
    if (value < p->min_val.f) value = p->min_val.f;
    if (value > p->max_val.f) value = p->max_val.f;
    
    uft_param_value_t old_val = p->value;
    p->value.f = value;
    p->is_modified = (value != p->default_val.f);
    uft_atomic_fetch_add(&g_bridge.version, 1);
    g_bridge.changes_pending = true;
    
    // Fire callbacks
    for (int i = 0; i < g_bridge.callback_count; i++) {
        if (g_bridge.callbacks[i].filter[0] == '\0' ||
            strcmp(g_bridge.callbacks[i].filter, name) == 0) {
            g_bridge.callbacks[i].callback(name, &old_val, &p->value,
                                           g_bridge.callbacks[i].user_data);
        }
    }
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/*============================================================================
 * CALLBACK MANAGEMENT
 *============================================================================*/

/**
 * @brief Register a parameter change callback
 */
int uft_gui_register_callback(uft_param_callback_t callback,
                              void* user_data,
                              const char* filter) {
    if (!callback || g_bridge.callback_count >= UFT_MAX_CALLBACKS) {
        return -1;
    }
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    uft_callback_entry_t* cb = &g_bridge.callbacks[g_bridge.callback_count++];
    cb->callback = callback;
    cb->user_data = user_data;
    if (filter) {
        strncpy(cb->filter, filter, UFT_MAX_PARAM_NAME - 1);
    } else {
        cb->filter[0] = '\0';
    }
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return g_bridge.callback_count - 1;
}

/**
 * @brief Unregister a callback
 */
void uft_gui_unregister_callback(int callback_id) {
    if (callback_id < 0 || callback_id >= g_bridge.callback_count) {
        return;
    }
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    // Shift remaining callbacks
    for (int i = callback_id; i < g_bridge.callback_count - 1; i++) {
        g_bridge.callbacks[i] = g_bridge.callbacks[i + 1];
    }
    g_bridge.callback_count--;
    
    pthread_mutex_unlock(&g_bridge.mutex);
}

/*============================================================================
 * PRESET MANAGEMENT
 *============================================================================*/

/**
 * @brief Save current parameters as preset
 */
int uft_gui_save_preset(const char* name, const char* description) {
    if (!name || g_bridge.preset_count >= UFT_MAX_PRESETS) {
        return -1;
    }
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    // Check if preset exists
    uft_preset_t* preset = NULL;
    for (int i = 0; i < g_bridge.preset_count; i++) {
        if (strcmp(g_bridge.presets[i].name, name) == 0) {
            preset = &g_bridge.presets[i];
            break;
        }
    }
    
    if (!preset) {
        preset = &g_bridge.presets[g_bridge.preset_count++];
    }
    
    strncpy(preset->name, name, UFT_MAX_PRESET_NAME - 1);
    if (description) {
        strncpy(preset->description, description, UFT_MAX_PARAM_VALUE - 1);
    }
    
    // Save all modified parameters
    preset->param_count = 0;
    for (int i = 0; i < g_bridge.param_count; i++) {
        if (g_bridge.params[i].is_modified) {
            strncpy(preset->params[preset->param_count].name,
                    g_bridge.params[i].name, UFT_MAX_PARAM_NAME - 1);
            preset->params[preset->param_count].value = g_bridge.params[i].value;
            preset->param_count++;
        }
    }
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/**
 * @brief Load preset
 */
int uft_gui_load_preset(const char* name) {
    if (!name) return -1;
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    // Find preset
    uft_preset_t* preset = NULL;
    for (int i = 0; i < g_bridge.preset_count; i++) {
        if (strcmp(g_bridge.presets[i].name, name) == 0) {
            preset = &g_bridge.presets[i];
            break;
        }
    }
    
    if (!preset) {
        pthread_mutex_unlock(&g_bridge.mutex);
        return -1;
    }
    
    // Apply parameters
    for (int i = 0; i < preset->param_count; i++) {
        uft_param_entry_t* p = find_param(preset->params[i].name);
        if (p) {
            p->value = preset->params[i].value;
            p->is_modified = true;
        }
    }
    
    uft_atomic_fetch_add(&g_bridge.version, 1);
    g_bridge.changes_pending = true;
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return 0;
}

/**
 * @brief Reset all parameters to defaults
 */
void uft_gui_reset_to_defaults(void) {
    pthread_mutex_lock(&g_bridge.mutex);
    
    for (int i = 0; i < g_bridge.param_count; i++) {
        g_bridge.params[i].value = g_bridge.params[i].default_val;
        g_bridge.params[i].is_modified = false;
    }
    
    uft_atomic_fetch_add(&g_bridge.version, 1);
    g_bridge.changes_pending = true;
    
    pthread_mutex_unlock(&g_bridge.mutex);
}

/*============================================================================
 * JSON SERIALIZATION
 *============================================================================*/

/**
 * @brief Export parameters to JSON string
 */
int uft_gui_to_json(char* buf, size_t buf_size) {
    if (!buf || buf_size < 100) return -1;
    
    pthread_mutex_lock(&g_bridge.mutex);
    
    int pos = snprintf(buf, buf_size, "{\n  \"parameters\": [\n");
    
    for (size_t i = 0; i < g_bridge.param_count && (size_t)pos < buf_size - 100; i++) {
        uft_param_entry_t* p = &g_bridge.params[i];
        
        pos += snprintf(buf + pos, buf_size - pos, "    {\n");
        pos += snprintf(buf + pos, buf_size - pos, 
                       "      \"name\": \"%s\",\n", p->name);
        pos += snprintf(buf + pos, buf_size - pos,
                       "      \"type\": %d,\n", p->type);
        
        switch (p->type) {
            case UFT_PARAM_BOOL:
                pos += snprintf(buf + pos, buf_size - pos,
                               "      \"value\": %s\n", p->value.b ? "true" : "false");
                break;
            case UFT_PARAM_INT:
                pos += snprintf(buf + pos, buf_size - pos,
                               "      \"value\": %d\n", p->value.i);
                break;
            case UFT_PARAM_FLOAT:
                pos += snprintf(buf + pos, buf_size - pos,
                               "      \"value\": %.6f\n", p->value.f);
                break;
            default:
                pos += snprintf(buf + pos, buf_size - pos,
                               "      \"value\": null\n");
        }
        
        pos += snprintf(buf + pos, buf_size - pos, "    }%s\n",
                       (i < g_bridge.param_count - 1) ? "," : "");
    }
    
    pos += snprintf(buf + pos, buf_size - pos, "  ]\n}\n");
    
    pthread_mutex_unlock(&g_bridge.mutex);
    return pos;
}

/*============================================================================
 * STATUS QUERIES
 *============================================================================*/

/**
 * @brief Get current version number
 */
int uft_gui_get_version(void) {
    return uft_atomic_load(&g_bridge.version);
}

/**
 * @brief Check if changes are pending
 */
bool uft_gui_has_changes(void) {
    return g_bridge.changes_pending;
}

/**
 * @brief Clear pending changes flag
 */
void uft_gui_clear_changes(void) {
    g_bridge.changes_pending = false;
}

/**
 * @brief Get parameter count
 */
int uft_gui_get_param_count(void) {
    return g_bridge.param_count;
}

/*============================================================================
 * UNIT TEST
 *============================================================================*/

#ifdef UFT_GUI_BRIDGE_V2_TEST

#include <assert.h>

static int g_callback_count = 0;
static void test_callback(const char* name, const uft_param_value_t* old_val,
                         const uft_param_value_t* new_val, void* user_data) {
    (void)name; (void)old_val; (void)new_val; (void)user_data;
    g_callback_count++;
}

int main(void) {
    printf("=== uft_gui_bridge_v2 Unit Tests ===\n");
    
    // Test 1: Initialization
    {
        int result = uft_gui_bridge_init();
        assert(result == 0);
        assert(g_bridge.is_initialized);
        printf("✓ Initialization\n");
    }
    
    // Test 2: Register parameters
    {
        uft_gui_register_bool("pll.enable_adaptive", true, "PLL", "Enable adaptive bandwidth");
        uft_gui_register_int("pll.lock_threshold", 8, 4, 32, "PLL", "Lock threshold", "bits");
        uft_gui_register_float("pll.bandwidth", 5.0f, 1.0f, 15.0f, "PLL", "PLL bandwidth", "%");
        
        assert(g_bridge.param_count == 3);
        printf("✓ Parameter registration: %d params\n", g_bridge.param_count);
    }
    
    // Test 3: Get/Set parameters
    {
        bool b = uft_gui_get_bool("pll.enable_adaptive", false);
        assert(b == true);
        
        int i = uft_gui_get_int("pll.lock_threshold", 0);
        assert(i == 8);
        
        float f = uft_gui_get_float("pll.bandwidth", 0.0f);
        assert(f > 4.9f && f < 5.1f);
        
        uft_gui_set_int("pll.lock_threshold", 16);
        i = uft_gui_get_int("pll.lock_threshold", 0);
        assert(i == 16);
        
        printf("✓ Get/Set parameters\n");
    }
    
    // Test 4: Parameter clamping
    {
        uft_gui_set_int("pll.lock_threshold", 100);  // Max is 32
        int i = uft_gui_get_int("pll.lock_threshold", 0);
        assert(i == 32);  // Should be clamped
        
        uft_gui_set_float("pll.bandwidth", 0.1f);  // Min is 1.0
        float f = uft_gui_get_float("pll.bandwidth", 0.0f);
        assert(f >= 1.0f);  // Should be clamped
        
        printf("✓ Parameter clamping\n");
    }
    
    // Test 5: Callbacks
    {
        g_callback_count = 0;
        int cb_id = uft_gui_register_callback(test_callback, NULL, NULL);
        assert(cb_id >= 0);
        
        uft_gui_set_int("pll.lock_threshold", 20);
        assert(g_callback_count == 1);
        
        uft_gui_set_float("pll.bandwidth", 7.5f);
        assert(g_callback_count == 2);
        
        uft_gui_unregister_callback(cb_id);
        printf("✓ Callbacks: %d fired\n", g_callback_count);
    }
    
    // Test 6: Presets
    {
        uft_gui_set_int("pll.lock_threshold", 12);
        uft_gui_set_float("pll.bandwidth", 8.0f);
        
        int result = uft_gui_save_preset("My Preset", "Test preset");
        assert(result == 0);
        
        uft_gui_reset_to_defaults();
        
        result = uft_gui_load_preset("My Preset");
        assert(result == 0);
        
        int i = uft_gui_get_int("pll.lock_threshold", 0);
        assert(i == 12);
        
        printf("✓ Preset save/load\n");
    }
    
    // Test 7: JSON export
    {
        char json[4096];
        int len = uft_gui_to_json(json, sizeof(json));
        assert(len > 0);
        assert(strstr(json, "pll.bandwidth") != NULL);
        printf("✓ JSON export: %d bytes\n", len);
    }
    
    // Test 8: Version tracking
    {
        int v1 = uft_gui_get_version();
        uft_gui_set_int("pll.lock_threshold", 15);
        int v2 = uft_gui_get_version();
        assert(v2 > v1);
        printf("✓ Version tracking: %d -> %d\n", v1, v2);
    }
    
    // Cleanup
    uft_gui_bridge_shutdown();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
#endif
