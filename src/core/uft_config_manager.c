/**
 * @file uft_config_manager.c
 * @brief Configuration Manager Implementation
 * 
 * P3-003: Zentrale Config
 */

#include "uft/uft_config_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct config_entry {
    char section[UFT_CONFIG_MAX_SECTION_LEN];
    char key[UFT_CONFIG_MAX_KEY_LEN];
    uft_config_value_t value;
    const uft_config_def_t *definition;
    struct config_entry *next;
} config_entry_t;

typedef struct callback_entry {
    uft_config_changed_fn callback;
    void *ctx;
    struct callback_entry *next;
} callback_entry_t;

struct uft_config_manager {
    config_entry_t *entries;
    const uft_config_def_t *definitions;
    int def_count;
    callback_entry_t *callbacks;
    bool modified;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static config_entry_t* find_entry(const uft_config_manager_t *cfg,
                                   const char *section, const char *key)
{
    for (config_entry_t *e = cfg->entries; e; e = e->next) {
        if (strcmp(e->section, section) == 0 && strcmp(e->key, key) == 0) {
            return e;
        }
    }
    return NULL;
}

static const uft_config_def_t* find_definition(const uft_config_manager_t *cfg,
                                                const char *section, const char *key)
{
    for (int i = 0; i < cfg->def_count; i++) {
        if (strcmp(cfg->definitions[i].section, section) == 0 &&
            strcmp(cfg->definitions[i].key, key) == 0) {
            return &cfg->definitions[i];
        }
    }
    return NULL;
}

static void notify_change(uft_config_manager_t *cfg, const char *section,
                          const char *key, const uft_config_value_t *value)
{
    for (callback_entry_t *cb = cfg->callbacks; cb; cb = cb->next) {
        cb->callback(section, key, value, cb->ctx);
    }
    cfg->modified = true;
}

static void trim(char *str)
{
    if (!str) return;
    
    /* Trim leading */
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    
    /* Trim trailing */
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    if (start != str) memmove(str, start, strlen(start) + 1);
}

static bool parse_bool(const char *str)
{
    if (!str) return false;
    return (strcasecmp(str, "true") == 0 ||
            strcasecmp(str, "yes") == 0 ||
            strcasecmp(str, "1") == 0 ||
            strcasecmp(str, "on") == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_config_manager_t* uft_config_create(void)
{
    uft_config_manager_t *cfg = calloc(1, sizeof(uft_config_manager_t));
    return cfg;
}

void uft_config_destroy(uft_config_manager_t *cfg)
{
    if (!cfg) return;
    
    /* Free entries */
    config_entry_t *e = cfg->entries;
    while (e) {
        config_entry_t *next = e->next;
        free(e);
        e = next;
    }
    
    /* Free callbacks */
    callback_entry_t *cb = cfg->callbacks;
    while (cb) {
        callback_entry_t *next = cb->next;
        free(cb);
        cb = next;
    }
    
    free(cfg);
}

int uft_config_register(uft_config_manager_t *cfg,
                        const uft_config_def_t *defs, int count)
{
    if (!cfg || !defs || count <= 0) return -1;
    
    cfg->definitions = defs;
    cfg->def_count = count;
    
    /* Initialize with defaults */
    for (int i = 0; i < count; i++) {
        config_entry_t *entry = calloc(1, sizeof(config_entry_t));
        if (!entry) return -1;
        
        strncpy(entry->section, defs[i].section, UFT_CONFIG_MAX_SECTION_LEN - 1);
        strncpy(entry->key, defs[i].key, UFT_CONFIG_MAX_KEY_LEN - 1);
        entry->value = defs[i].default_value;
        entry->definition = &defs[i];
        entry->next = cfg->entries;
        cfg->entries = entry;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File I/O - INI
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_config_load_ini(uft_config_manager_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[UFT_CONFIG_MAX_VALUE_LEN + UFT_CONFIG_MAX_KEY_LEN + 16];
    char current_section[UFT_CONFIG_MAX_SECTION_LEN] = "";
    
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        
        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;
        
        /* Section header */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, line + 1, UFT_CONFIG_MAX_SECTION_LEN - 1);
            }
            continue;
        }
        
        /* Key = Value */
        char *eq = strchr(line, '=');
        if (eq && current_section[0]) {
            *eq = '\0';
            char key[UFT_CONFIG_MAX_KEY_LEN];
            char value[UFT_CONFIG_MAX_VALUE_LEN];
            
            strncpy(key, line, sizeof(key) - 1);
            strncpy(value, eq + 1, sizeof(value) - 1);
            trim(key);
            trim(value);
            
            /* Find definition and set value */
            const uft_config_def_t *def = find_definition(cfg, current_section, key);
            if (def) {
                switch (def->type) {
                    case UFT_CFG_TYPE_STRING:
                    case UFT_CFG_TYPE_PATH:
                        uft_config_set_string(cfg, current_section, key, value);
                        break;
                    case UFT_CFG_TYPE_INT:
                    case UFT_CFG_TYPE_ENUM:
                        uft_config_set_int(cfg, current_section, key, atoll(value));
                        break;
                    case UFT_CFG_TYPE_FLOAT:
                        uft_config_set_float(cfg, current_section, key, atof(value));
                        break;
                    case UFT_CFG_TYPE_BOOL:
                        uft_config_set_bool(cfg, current_section, key, parse_bool(value));
                        break;
                }
            }
        }
    }
    
    fclose(f);
    cfg->modified = false;
    return 0;
}

int uft_config_save_ini(const uft_config_manager_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "# UFT Configuration File\n");
    fprintf(f, "# Generated automatically\n\n");
    
    /* Group by section */
    const char *sections[] = {
        UFT_SEC_GENERAL, UFT_SEC_HARDWARE, UFT_SEC_RECOVERY,
        UFT_SEC_FORMAT, UFT_SEC_GUI, UFT_SEC_LOGGING, UFT_SEC_PATHS,
        NULL
    };
    
    for (int s = 0; sections[s]; s++) {
        bool section_written = false;
        
        for (config_entry_t *e = cfg->entries; e; e = e->next) {
            if (strcmp(e->section, sections[s]) != 0) continue;
            
            if (!section_written) {
                fprintf(f, "[%s]\n", sections[s]);
                section_written = true;
            }
            
            switch (e->value.type) {
                case UFT_CFG_TYPE_STRING:
                case UFT_CFG_TYPE_PATH:
                    fprintf(f, "%s = %s\n", e->key, e->value.str_val);
                    break;
                case UFT_CFG_TYPE_INT:
                case UFT_CFG_TYPE_ENUM:
                    fprintf(f, "%s = %lld\n", e->key, (long long)e->value.int_val);
                    break;
                case UFT_CFG_TYPE_FLOAT:
                    fprintf(f, "%s = %.6f\n", e->key, e->value.float_val);
                    break;
                case UFT_CFG_TYPE_BOOL:
                    fprintf(f, "%s = %s\n", e->key, e->value.bool_val ? "true" : "false");
                    break;
            }
        }
        
        if (section_written) fprintf(f, "\n");
    }
    
    fclose(f);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File I/O - JSON
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_config_load_json(uft_config_manager_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    /* Simple JSON parser - handles flat structure only */
    char line[UFT_CONFIG_MAX_VALUE_LEN * 2];
    char current_section[UFT_CONFIG_MAX_SECTION_LEN] = "";
    
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        
        /* Section start "section": { */
        if (strstr(line, "\": {")) {
            char *quote = strchr(line, '"');
            if (quote) {
                char *end = strchr(quote + 1, '"');
                if (end) {
                    *end = '\0';
                    strncpy(current_section, quote + 1, UFT_CONFIG_MAX_SECTION_LEN - 1);
                }
            }
            continue;
        }
        
        /* Key-value pair */
        if (current_section[0] && strchr(line, ':')) {
            char *key_start = strchr(line, '"');
            if (!key_start) continue;
            
            char *key_end = strchr(key_start + 1, '"');
            if (!key_end) continue;
            
            *key_end = '\0';
            char key[UFT_CONFIG_MAX_KEY_LEN];
            strncpy(key, key_start + 1, sizeof(key) - 1);
            
            char *colon = strchr(key_end + 1, ':');
            if (!colon) continue;
            
            char *val_start = colon + 1;
            while (*val_start && isspace((unsigned char)*val_start)) val_start++;
            
            /* Remove trailing comma and whitespace */
            char *val_end = val_start + strlen(val_start) - 1;
            while (val_end > val_start && (*val_end == ',' || isspace((unsigned char)*val_end))) {
                *val_end = '\0';
                val_end--;
            }
            
            const uft_config_def_t *def = find_definition(cfg, current_section, key);
            if (def) {
                if (*val_start == '"') {
                    /* String value */
                    val_start++;
                    char *str_end = strrchr(val_start, '"');
                    if (str_end) *str_end = '\0';
                    uft_config_set_string(cfg, current_section, key, val_start);
                } else if (strcmp(val_start, "true") == 0) {
                    uft_config_set_bool(cfg, current_section, key, true);
                } else if (strcmp(val_start, "false") == 0) {
                    uft_config_set_bool(cfg, current_section, key, false);
                } else if (strchr(val_start, '.')) {
                    uft_config_set_float(cfg, current_section, key, atof(val_start));
                } else {
                    uft_config_set_int(cfg, current_section, key, atoll(val_start));
                }
            }
        }
        
        /* Section end } */
        if (strchr(line, '}') && !strchr(line, '{')) {
            current_section[0] = '\0';
        }
    }
    
    fclose(f);
    cfg->modified = false;
    return 0;
}

int uft_config_save_json(const uft_config_manager_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    
    const char *sections[] = {
        UFT_SEC_GENERAL, UFT_SEC_HARDWARE, UFT_SEC_RECOVERY,
        UFT_SEC_FORMAT, UFT_SEC_GUI, UFT_SEC_LOGGING, UFT_SEC_PATHS,
        NULL
    };
    
    bool first_section = true;
    for (int s = 0; sections[s]; s++) {
        bool has_entries = false;
        
        /* Check if section has entries */
        for (config_entry_t *e = cfg->entries; e; e = e->next) {
            if (strcmp(e->section, sections[s]) == 0) {
                has_entries = true;
                break;
            }
        }
        
        if (!has_entries) continue;
        
        if (!first_section) fprintf(f, ",\n");
        first_section = false;
        
        fprintf(f, "  \"%s\": {\n", sections[s]);
        
        bool first_entry = true;
        for (config_entry_t *e = cfg->entries; e; e = e->next) {
            if (strcmp(e->section, sections[s]) != 0) continue;
            
            if (!first_entry) fprintf(f, ",\n");
            first_entry = false;
            
            fprintf(f, "    \"%s\": ", e->key);
            
            switch (e->value.type) {
                case UFT_CFG_TYPE_STRING:
                case UFT_CFG_TYPE_PATH:
                    fprintf(f, "\"%s\"", e->value.str_val);
                    break;
                case UFT_CFG_TYPE_INT:
                case UFT_CFG_TYPE_ENUM:
                    fprintf(f, "%lld", (long long)e->value.int_val);
                    break;
                case UFT_CFG_TYPE_FLOAT:
                    fprintf(f, "%.6f", e->value.float_val);
                    break;
                case UFT_CFG_TYPE_BOOL:
                    fprintf(f, "%s", e->value.bool_val ? "true" : "false");
                    break;
            }
        }
        
        fprintf(f, "\n  }");
    }
    
    fprintf(f, "\n}\n");
    fclose(f);
    return 0;
}

int uft_config_load_env(uft_config_manager_t *cfg)
{
    if (!cfg) return -1;
    
    int loaded = 0;
    for (int i = 0; i < cfg->def_count; i++) {
        if (!cfg->definitions[i].env_override) continue;
        
        const char *env_val = getenv(cfg->definitions[i].env_override);
        if (!env_val) continue;
        
        const uft_config_def_t *def = &cfg->definitions[i];
        switch (def->type) {
            case UFT_CFG_TYPE_STRING:
            case UFT_CFG_TYPE_PATH:
                uft_config_set_string(cfg, def->section, def->key, env_val);
                break;
            case UFT_CFG_TYPE_INT:
            case UFT_CFG_TYPE_ENUM:
                uft_config_set_int(cfg, def->section, def->key, atoll(env_val));
                break;
            case UFT_CFG_TYPE_FLOAT:
                uft_config_set_float(cfg, def->section, def->key, atof(env_val));
                break;
            case UFT_CFG_TYPE_BOOL:
                uft_config_set_bool(cfg, def->section, def->key, parse_bool(env_val));
                break;
        }
        loaded++;
    }
    
    return loaded;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Getters
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char* uft_config_get_string(const uft_config_manager_t *cfg,
                                   const char *section, const char *key)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (e && (e->value.type == UFT_CFG_TYPE_STRING || 
              e->value.type == UFT_CFG_TYPE_PATH)) {
        return e->value.str_val;
    }
    return "";
}

int64_t uft_config_get_int(const uft_config_manager_t *cfg,
                           const char *section, const char *key)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (e && (e->value.type == UFT_CFG_TYPE_INT || 
              e->value.type == UFT_CFG_TYPE_ENUM)) {
        return e->value.int_val;
    }
    return 0;
}

double uft_config_get_float(const uft_config_manager_t *cfg,
                            const char *section, const char *key)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (e && e->value.type == UFT_CFG_TYPE_FLOAT) {
        return e->value.float_val;
    }
    return 0.0;
}

bool uft_config_get_bool(const uft_config_manager_t *cfg,
                         const char *section, const char *key)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (e && e->value.type == UFT_CFG_TYPE_BOOL) {
        return e->value.bool_val;
    }
    return false;
}

bool uft_config_get(const uft_config_manager_t *cfg,
                    const char *section, const char *key,
                    uft_config_value_t *out)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (e && out) {
        *out = e->value;
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Setters
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_config_set_string(uft_config_manager_t *cfg,
                          const char *section, const char *key,
                          const char *value)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (!e) return -1;
    
    e->value.type = UFT_CFG_TYPE_STRING;
    strncpy(e->value.str_val, value ? value : "", UFT_CONFIG_MAX_VALUE_LEN - 1);
    notify_change(cfg, section, key, &e->value);
    return 0;
}

int uft_config_set_int(uft_config_manager_t *cfg,
                       const char *section, const char *key,
                       int64_t value)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (!e) return -1;
    
    /* Validate range if defined */
    if (e->definition && e->definition->max_val > e->definition->min_val) {
        if (value < e->definition->min_val) value = e->definition->min_val;
        if (value > e->definition->max_val) value = e->definition->max_val;
    }
    
    e->value.type = UFT_CFG_TYPE_INT;
    e->value.int_val = value;
    notify_change(cfg, section, key, &e->value);
    return 0;
}

int uft_config_set_float(uft_config_manager_t *cfg,
                         const char *section, const char *key,
                         double value)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (!e) return -1;
    
    e->value.type = UFT_CFG_TYPE_FLOAT;
    e->value.float_val = value;
    notify_change(cfg, section, key, &e->value);
    return 0;
}

int uft_config_set_bool(uft_config_manager_t *cfg,
                        const char *section, const char *key,
                        bool value)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (!e) return -1;
    
    e->value.type = UFT_CFG_TYPE_BOOL;
    e->value.bool_val = value;
    notify_change(cfg, section, key, &e->value);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_config_reset(uft_config_manager_t *cfg)
{
    if (!cfg) return;
    
    for (config_entry_t *e = cfg->entries; e; e = e->next) {
        if (e->definition) {
            e->value = e->definition->default_value;
        }
    }
    cfg->modified = true;
}

void uft_config_reset_key(uft_config_manager_t *cfg,
                          const char *section, const char *key)
{
    config_entry_t *e = find_entry(cfg, section, key);
    if (e && e->definition) {
        e->value = e->definition->default_value;
        notify_change(cfg, section, key, &e->value);
    }
}

bool uft_config_exists(const uft_config_manager_t *cfg,
                       const char *section, const char *key)
{
    return find_entry(cfg, section, key) != NULL;
}

int uft_config_section_count(const uft_config_manager_t *cfg, const char *section)
{
    int count = 0;
    for (config_entry_t *e = cfg->entries; e; e = e->next) {
        if (strcmp(e->section, section) == 0) count++;
    }
    return count;
}

int uft_config_enumerate(const uft_config_manager_t *cfg, const char *section,
                         void (*callback)(const char *key, 
                                         const uft_config_value_t *value, 
                                         void *ctx),
                         void *ctx)
{
    if (!cfg || !callback) return -1;
    
    int count = 0;
    for (config_entry_t *e = cfg->entries; e; e = e->next) {
        if (section && strcmp(e->section, section) != 0) continue;
        callback(e->key, &e->value, ctx);
        count++;
    }
    return count;
}

int uft_config_on_change(uft_config_manager_t *cfg,
                         uft_config_changed_fn callback, void *ctx)
{
    if (!cfg || !callback) return -1;
    
    callback_entry_t *cb = calloc(1, sizeof(callback_entry_t));
    if (!cb) return -1;
    
    cb->callback = callback;
    cb->ctx = ctx;
    cb->next = cfg->callbacks;
    cfg->callbacks = cb;
    
    return 0;
}

int uft_config_validate(const uft_config_manager_t *cfg,
                        char *errors, size_t errors_size)
{
    if (!cfg) return -1;
    
    int error_count = 0;
    size_t written = 0;
    
    for (config_entry_t *e = cfg->entries; e; e = e->next) {
        if (!e->definition) continue;
        
        const uft_config_def_t *def = e->definition;
        
        /* Range validation for int */
        if (def->type == UFT_CFG_TYPE_INT && def->max_val > def->min_val) {
            if (e->value.int_val < def->min_val || e->value.int_val > def->max_val) {
                if (errors && written < errors_size) {
                    written += snprintf(errors + written, errors_size - written,
                        "[%s].%s: value %lld out of range [%lld..%lld]\n",
                        e->section, e->key, (long long)e->value.int_val,
                        (long long)def->min_val, (long long)def->max_val);
                }
                error_count++;
            }
        }
    }
    
    return error_count;
}

void uft_config_print(const uft_config_manager_t *cfg)
{
    if (!cfg) return;
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  UFT Configuration\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    const char *last_section = "";
    for (config_entry_t *e = cfg->entries; e; e = e->next) {
        if (strcmp(e->section, last_section) != 0) {
            printf("[%s]\n", e->section);
            last_section = e->section;
        }
        
        printf("  %-20s = ", e->key);
        switch (e->value.type) {
            case UFT_CFG_TYPE_STRING:
            case UFT_CFG_TYPE_PATH:
                printf("%s\n", e->value.str_val);
                break;
            case UFT_CFG_TYPE_INT:
            case UFT_CFG_TYPE_ENUM:
                printf("%lld\n", (long long)e->value.int_val);
                break;
            case UFT_CFG_TYPE_FLOAT:
                printf("%.4f\n", e->value.float_val);
                break;
            case UFT_CFG_TYPE_BOOL:
                printf("%s\n", e->value.bool_val ? "true" : "false");
                break;
        }
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT Default Definitions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_config_def_t uft_default_defs[] = {
    /* General */
    {UFT_SEC_GENERAL, UFT_KEY_VERSION, UFT_CFG_TYPE_STRING,
     {.type = UFT_CFG_TYPE_STRING, .str_val = "3.8.7"}, "UFT Version", NULL, 0, 0, NULL, 0},
    {UFT_SEC_GENERAL, UFT_KEY_LAST_DIR, UFT_CFG_TYPE_PATH,
     {.type = UFT_CFG_TYPE_PATH, .str_val = ""}, "Last used directory", NULL, 0, 0, NULL, 0},
    
    /* Hardware */
    {UFT_SEC_HARDWARE, UFT_KEY_DEVICE, UFT_CFG_TYPE_STRING,
     {.type = UFT_CFG_TYPE_STRING, .str_val = "auto"}, "Hardware device", "UFT_DEVICE", 0, 0, NULL, 0},
    {UFT_SEC_HARDWARE, UFT_KEY_DRIVE_NUM, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 0}, "Drive number", NULL, 0, 3, NULL, 0},
    {UFT_SEC_HARDWARE, UFT_KEY_AUTO_DETECT, UFT_CFG_TYPE_BOOL,
     {.type = UFT_CFG_TYPE_BOOL, .bool_val = true}, "Auto-detect hardware", NULL, 0, 0, NULL, 0},
    
    /* Recovery */
    {UFT_SEC_RECOVERY, UFT_KEY_MAX_RETRIES, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 5}, "Maximum read retries", "UFT_RETRIES", 1, 50, NULL, 0},
    {UFT_SEC_RECOVERY, UFT_KEY_REVOLUTIONS, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 3}, "Revolutions per track", NULL, 1, 20, NULL, 0},
    {UFT_SEC_RECOVERY, UFT_KEY_WEAK_BITS, UFT_CFG_TYPE_BOOL,
     {.type = UFT_CFG_TYPE_BOOL, .bool_val = true}, "Detect weak bits", NULL, 0, 0, NULL, 0},
    {UFT_SEC_RECOVERY, UFT_KEY_RECALIBRATE, UFT_CFG_TYPE_BOOL,
     {.type = UFT_CFG_TYPE_BOOL, .bool_val = true}, "Recalibrate on retry", NULL, 0, 0, NULL, 0},
    
    /* Format */
    {UFT_SEC_FORMAT, UFT_KEY_DEFAULT_FMT, UFT_CFG_TYPE_STRING,
     {.type = UFT_CFG_TYPE_STRING, .str_val = "auto"}, "Default format", NULL, 0, 0, NULL, 0},
    {UFT_SEC_FORMAT, UFT_KEY_CYLINDERS, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 80}, "Cylinders", NULL, 35, 85, NULL, 0},
    {UFT_SEC_FORMAT, UFT_KEY_HEADS, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 2}, "Heads", NULL, 1, 2, NULL, 0},
    {UFT_SEC_FORMAT, UFT_KEY_SECTORS, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 18}, "Sectors per track", NULL, 8, 36, NULL, 0},
    
    /* GUI */
    {UFT_SEC_GUI, UFT_KEY_DARK_MODE, UFT_CFG_TYPE_BOOL,
     {.type = UFT_CFG_TYPE_BOOL, .bool_val = false}, "Dark mode", NULL, 0, 0, NULL, 0},
    {UFT_SEC_GUI, UFT_KEY_WINDOW_W, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 1024}, "Window width", NULL, 640, 3840, NULL, 0},
    {UFT_SEC_GUI, UFT_KEY_WINDOW_H, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 768}, "Window height", NULL, 480, 2160, NULL, 0},
    
    /* Logging */
    {UFT_SEC_LOGGING, UFT_KEY_LOG_LEVEL, UFT_CFG_TYPE_INT,
     {.type = UFT_CFG_TYPE_INT, .int_val = 2}, "Log level (0-4)", "UFT_LOG_LEVEL", 0, 4, NULL, 0},
    {UFT_SEC_LOGGING, UFT_KEY_LOG_FILE, UFT_CFG_TYPE_PATH,
     {.type = UFT_CFG_TYPE_PATH, .str_val = "uft.log"}, "Log file", "UFT_LOG_FILE", 0, 0, NULL, 0},
    {UFT_SEC_LOGGING, UFT_KEY_LOG_CONSOLE, UFT_CFG_TYPE_BOOL,
     {.type = UFT_CFG_TYPE_BOOL, .bool_val = true}, "Log to console", NULL, 0, 0, NULL, 0},
    
    /* Paths */
    {UFT_SEC_PATHS, UFT_KEY_INPUT_DIR, UFT_CFG_TYPE_PATH,
     {.type = UFT_CFG_TYPE_PATH, .str_val = ""}, "Input directory", NULL, 0, 0, NULL, 0},
    {UFT_SEC_PATHS, UFT_KEY_OUTPUT_DIR, UFT_CFG_TYPE_PATH,
     {.type = UFT_CFG_TYPE_PATH, .str_val = ""}, "Output directory", NULL, 0, 0, NULL, 0},
};

const uft_config_def_t* uft_config_get_defaults(int *count)
{
    if (count) *count = sizeof(uft_default_defs) / sizeof(uft_default_defs[0]);
    return uft_default_defs;
}
