/**
 * @file uft_settings.c
 * @brief Runtime Settings Implementation (W-P3-003)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_settings.h"
#include "uft/uft_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

#define MAX_SETTINGS 128
#define MAX_KEY_LEN  64
#define MAX_VAL_LEN  256

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VAL_LEN];
    uft_settings_group_t group;
} setting_entry_t;

static setting_entry_t g_settings[MAX_SETTINGS];
static int g_count = 0;
static bool g_initialized = false;

/*===========================================================================
 * DEFAULTS
 *===========================================================================*/

typedef struct {
    const char *key;
    const char *value;
    uft_settings_group_t group;
} default_entry_t;

static const default_entry_t DEFAULTS[] = {
    /* General */
    {UFT_SET_VERBOSE, "false", UFT_SET_GENERAL},
    {UFT_SET_QUIET, "false", UFT_SET_GENERAL},
    {UFT_SET_EXPERT_MODE, "false", UFT_SET_GENERAL},
    
    /* Format */
    {UFT_SET_DEFAULT_FORMAT, "auto", UFT_SET_FORMAT},
    {UFT_SET_DEFAULT_SIDES, "2", UFT_SET_FORMAT},
    {UFT_SET_DEFAULT_TRACKS, "80", UFT_SET_FORMAT},
    
    /* Hardware */
    {UFT_SET_HW_INTERFACE, "auto", UFT_SET_HARDWARE},
    {UFT_SET_HW_DEVICE, "", UFT_SET_HARDWARE},
    
    /* Recovery */
    {UFT_SET_RETRIES, "5", UFT_SET_RECOVERY},
    {UFT_SET_REVOLUTIONS, "3", UFT_SET_RECOVERY},
    {UFT_SET_MERGE_REVS, "true", UFT_SET_RECOVERY},
    
    /* PLL */
    {UFT_SET_PLL_PRESET, "default", UFT_SET_PLL},
    {UFT_SET_PLL_ADJUST, "15", UFT_SET_PLL},
    
    /* Logging */
    {UFT_SET_LOG_LEVEL, "info", UFT_SET_LOGGING},
    {UFT_SET_LOG_FILE, "", UFT_SET_LOGGING},
    
    /* Paths */
    {UFT_SET_PATH_OUTPUT, "", UFT_SET_PATHS},
    {UFT_SET_PATH_TEMP, "", UFT_SET_PATHS},
    
    {NULL, NULL, 0}
};

/*===========================================================================
 * HELPERS
 *===========================================================================*/

static int find_setting(const char *key) {
    if (!key) return -1;
    for (int i = 0; i < g_count; i++) {
        if (strcmp(g_settings[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static uft_settings_group_t group_from_key(const char *key) {
    if (!key) return UFT_SET_GENERAL;
    
    if (strncmp(key, "general.", 8) == 0) return UFT_SET_GENERAL;
    if (strncmp(key, "format.", 7) == 0) return UFT_SET_FORMAT;
    if (strncmp(key, "hardware.", 9) == 0) return UFT_SET_HARDWARE;
    if (strncmp(key, "recovery.", 9) == 0) return UFT_SET_RECOVERY;
    if (strncmp(key, "pll.", 4) == 0) return UFT_SET_PLL;
    if (strncmp(key, "gui.", 4) == 0) return UFT_SET_GUI;
    if (strncmp(key, "logging.", 8) == 0) return UFT_SET_LOGGING;
    if (strncmp(key, "paths.", 6) == 0) return UFT_SET_PATHS;
    
    return UFT_SET_GENERAL;
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

int uft_settings_init(void) {
    if (g_initialized) return 0;
    
    g_count = 0;
    
    /* Load defaults */
    for (int i = 0; DEFAULTS[i].key; i++) {
        if (g_count >= MAX_SETTINGS) break;
        
        strncpy(g_settings[g_count].key, DEFAULTS[i].key, MAX_KEY_LEN - 1);
        strncpy(g_settings[g_count].value, DEFAULTS[i].value, MAX_VAL_LEN - 1);
        g_settings[g_count].group = DEFAULTS[i].group;
        g_count++;
    }
    
    g_initialized = true;
    return 0;
}

void uft_settings_shutdown(void) {
    g_count = 0;
    g_initialized = false;
}

void uft_settings_reset(void) {
    uft_settings_shutdown();
    uft_settings_init();
}

/*===========================================================================
 * GETTERS
 *===========================================================================*/

const char* uft_settings_get_string(const char *key, const char *def) {
    int idx = find_setting(key);
    if (idx < 0) return def;
    return g_settings[idx].value;
}

int uft_settings_get_int(const char *key, int def) {
    const char *val = uft_settings_get_string(key, NULL);
    if (!val) return def;
    return atoi(val);
}

float uft_settings_get_float(const char *key, float def) {
    const char *val = uft_settings_get_string(key, NULL);
    if (!val) return def;
    return (float)atof(val);
}

bool uft_settings_get_bool(const char *key, bool def) {
    const char *val = uft_settings_get_string(key, NULL);
    if (!val) return def;
    
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        return true;
    }
    return false;
}

/*===========================================================================
 * SETTERS
 *===========================================================================*/

int uft_settings_set_string(const char *key, const char *value) {
    if (!key) return -1;
    
    int idx = find_setting(key);
    if (idx >= 0) {
        strncpy(g_settings[idx].value, value ? value : "", MAX_VAL_LEN - 1);
        return 0;
    }
    
    /* Add new */
    if (g_count >= MAX_SETTINGS) return -1;
    
    strncpy(g_settings[g_count].key, key, MAX_KEY_LEN - 1);
    strncpy(g_settings[g_count].value, value ? value : "", MAX_VAL_LEN - 1);
    g_settings[g_count].group = group_from_key(key);
    g_count++;
    
    return 0;
}

int uft_settings_set_int(const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return uft_settings_set_string(key, buf);
}

int uft_settings_set_float(const char *key, float value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", value);
    return uft_settings_set_string(key, buf);
}

int uft_settings_set_bool(const char *key, bool value) {
    return uft_settings_set_string(key, value ? "true" : "false");
}

/*===========================================================================
 * FILE I/O
 *===========================================================================*/

int uft_settings_load(const char *path) {
    if (!path) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;
        
        /* Find key */
        char *key_start = strchr(p, '"');
        if (!key_start) continue;
        key_start++;
        
        char *key_end = strchr(key_start, '"');
        if (!key_end) continue;
        
        char key[MAX_KEY_LEN];
        size_t key_len = key_end - key_start;
        if (key_len >= MAX_KEY_LEN) key_len = MAX_KEY_LEN - 1;
        strncpy(key, key_start, key_len);
        key[key_len] = '\0';
        
        /* Find value */
        char *colon = strchr(key_end, ':');
        if (!colon) continue;
        
        char *val_start = colon + 1;
        while (*val_start && isspace(*val_start)) val_start++;
        
        char value[MAX_VAL_LEN] = "";
        
        if (*val_start == '"') {
            val_start++;
            char *val_end = strchr(val_start, '"');
            if (val_end) {
                size_t val_len = val_end - val_start;
                if (val_len >= MAX_VAL_LEN) val_len = MAX_VAL_LEN - 1;
                strncpy(value, val_start, val_len);
                value[val_len] = '\0';
            }
        } else if (strncmp(val_start, "true", 4) == 0) {
            strcpy(value, "true");
        } else if (strncmp(val_start, "false", 5) == 0) {
            strcpy(value, "false");
        } else if (*val_start == '-' || isdigit(*val_start)) {
            char *end = val_start;
            while (*end && (isdigit(*end) || *end == '.' || *end == '-')) end++;
            size_t val_len = end - val_start;
            if (val_len >= MAX_VAL_LEN) val_len = MAX_VAL_LEN - 1;
            strncpy(value, val_start, val_len);
            value[val_len] = '\0';
        }
        
        uft_settings_set_string(key, value);
    }
    
    fclose(f);
    return 0;
}

int uft_settings_save(const char *path) {
    if (!path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    
    for (int i = 0; i < g_count; i++) {
        const char *val = g_settings[i].value;
        
        /* Check if numeric */
        bool is_num = true;
        bool is_bool = (strcmp(val, "true") == 0 || strcmp(val, "false") == 0);
        
        if (!is_bool) {
            for (const char *p = val; *p && is_num; p++) {
                if (!isdigit(*p) && *p != '.' && *p != '-') is_num = false;
            }
        }
        
        if (is_bool || (is_num && val[0])) {
            fprintf(f, "  \"%s\": %s", g_settings[i].key, val);
        } else {
            fprintf(f, "  \"%s\": \"%s\"", g_settings[i].key, val);
        }
        
        if (i < g_count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

bool uft_settings_has(const char *key) {
    return find_setting(key) >= 0;
}

const char* uft_settings_group_name(uft_settings_group_t group) {
    static const char *names[] = {
        "General", "Format", "Hardware", "Recovery",
        "PLL", "GUI", "Logging", "Paths"
    };
    if (group >= UFT_SET_COUNT) return "Unknown";
    return names[group];
}

char* uft_settings_to_json(bool pretty) {
    size_t size = 4096;
    char *json = (char *)malloc(size);
    if (!json) return NULL;
    
    char *p = json;
    char *end = json + size;
    const char *nl = pretty ? "\n" : "";
    const char *indent = pretty ? "  " : "";
    
    int n = snprintf(p, (size_t)(end - p), "{%s", nl);
    if (n > 0 && p + n < end) p += n;
    
    for (int i = 0; i < g_count; i++) {
        const char *val = g_settings[i].value;
        bool is_bool = (strcmp(val, "true") == 0 || strcmp(val, "false") == 0);
        bool is_num = !is_bool && val[0] && (val[0] == '-' || isdigit(val[0]));
        
        size_t remaining = (size_t)(end - p);
        if (remaining < 2) break;
        
        if (is_bool || is_num) {
            n = snprintf(p, remaining, "%s\"%s\": %s", indent, g_settings[i].key, val);
        } else {
            n = snprintf(p, remaining, "%s\"%s\": \"%s\"", indent, g_settings[i].key, val);
        }
        if (n > 0 && (size_t)n < remaining) p += n;
        
        remaining = (size_t)(end - p);
        if (i < g_count - 1) {
            n = snprintf(p, remaining, ",");
            if (n > 0 && (size_t)n < remaining) p += n;
        }
        remaining = (size_t)(end - p);
        n = snprintf(p, remaining, "%s", nl);
        if (n > 0 && (size_t)n < remaining) p += n;
    }
    
    {
        size_t remaining = (size_t)(end - p);
        n = snprintf(p, remaining, "}");
        if (n > 0 && (size_t)n < remaining) p += n;
    }
    return json;
}

int uft_settings_default_path(char *path, size_t size) {
    if (!path || size == 0) return -1;
    
#ifdef UFT_PLATFORM_WINDOWS
    const char *appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(path, size, "%s\\UFT\\settings.json", appdata);
        return 0;
    }
#else
    const char *home = getenv("HOME");
    if (home) {
        snprintf(path, size, "%s/.config/uft/settings.json", home);
        return 0;
    }
#endif
    
    snprintf(path, size, "uft_settings.json");
    return 0;
}
