/**
 * @file uft_param_bridge.c
 * @brief Parameter Bridge Implementation
 */

#include "uft/params/uft_param_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

#define MAX_PARAMS 256

typedef struct {
    char path[64];
    uft_param_type_t type;
    uft_param_value_t value;
} param_entry_t;

struct uft_param_bridge_s {
    param_entry_t params[MAX_PARAMS];
    int param_count;
    
    // Transaction
    param_entry_t transaction_backup[MAX_PARAMS];
    int transaction_count;
    bool in_transaction;
    
    // Callbacks
    uft_param_changed_cb on_change;
    void* on_change_user;
    uft_param_validate_cb on_validate;
    void* on_validate_user;
};

// ============================================================================
// LIFECYCLE
// ============================================================================

uft_param_bridge_t* uft_param_bridge_create(void) {
    uft_param_bridge_t* bridge = calloc(1, sizeof(uft_param_bridge_t));
    return bridge;
}

void uft_param_bridge_destroy(uft_param_bridge_t* bridge) {
    if (bridge) {
        // Free any allocated strings
        for (int i = 0; i < bridge->param_count; i++) {
            if (bridge->params[i].type == UFT_PARAM_TYPE_STRING && 
                bridge->params[i].value.s) {
                free(bridge->params[i].value.s);
            }
        }
        free(bridge);
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void uft_param_bridge_on_change(uft_param_bridge_t* bridge,
                                 uft_param_changed_cb cb, void* user) {
    if (bridge) {
        bridge->on_change = cb;
        bridge->on_change_user = user;
    }
}

void uft_param_bridge_on_validate(uft_param_bridge_t* bridge,
                                   uft_param_validate_cb cb, void* user) {
    if (bridge) {
        bridge->on_validate = cb;
        bridge->on_validate_user = user;
    }
}

// ============================================================================
// INTERNAL: FIND/CREATE PARAM
// ============================================================================

static param_entry_t* find_param(uft_param_bridge_t* bridge, const char* path) {
    for (int i = 0; i < bridge->param_count; i++) {
        if (strcmp(bridge->params[i].path, path) == 0) {
            return &bridge->params[i];
        }
    }
    return NULL;
}

static param_entry_t* find_or_create_param(uft_param_bridge_t* bridge, 
                                            const char* path, 
                                            uft_param_type_t type) {
    param_entry_t* p = find_param(bridge, path);
    if (p) return p;
    
    if (bridge->param_count >= MAX_PARAMS) return NULL;
    
    p = &bridge->params[bridge->param_count++];
    strncpy(p->path, path, sizeof(p->path) - 1);
    p->type = type;
    memset(&p->value, 0, sizeof(p->value));
    
    return p;
}

// ============================================================================
// SETTERS
// ============================================================================

int uft_param_set_int(uft_param_bridge_t* bridge, const char* path, int64_t value) {
    if (!bridge || !path) return -1;
    
    param_entry_t* p = find_or_create_param(bridge, path, UFT_PARAM_TYPE_INT);
    if (!p) return -1;
    
    p->value.i = value;
    return 0;
}

int uft_param_set_double(uft_param_bridge_t* bridge, const char* path, double value) {
    if (!bridge || !path) return -1;
    
    param_entry_t* p = find_or_create_param(bridge, path, UFT_PARAM_TYPE_DOUBLE);
    if (!p) return -1;
    
    p->value.d = value;
    return 0;
}

int uft_param_set_bool(uft_param_bridge_t* bridge, const char* path, bool value) {
    if (!bridge || !path) return -1;
    
    param_entry_t* p = find_or_create_param(bridge, path, UFT_PARAM_TYPE_BOOL);
    if (!p) return -1;
    
    p->value.b = value;
    return 0;
}

int uft_param_set_string(uft_param_bridge_t* bridge, const char* path, const char* value) {
    if (!bridge || !path) return -1;
    
    param_entry_t* p = find_or_create_param(bridge, path, UFT_PARAM_TYPE_STRING);
    if (!p) return -1;
    
    free(p->value.s);
    p->value.s = value ? strdup(value) : NULL;
    return 0;
}

// ============================================================================
// GETTERS
// ============================================================================

int64_t uft_param_get_int(const uft_param_bridge_t* bridge, const char* path) {
    if (!bridge || !path) return 0;
    param_entry_t* p = find_param((uft_param_bridge_t*)bridge, path);
    return p ? p->value.i : 0;
}

double uft_param_get_double(const uft_param_bridge_t* bridge, const char* path) {
    if (!bridge || !path) return 0.0;
    param_entry_t* p = find_param((uft_param_bridge_t*)bridge, path);
    return p ? p->value.d : 0.0;
}

bool uft_param_get_bool(const uft_param_bridge_t* bridge, const char* path) {
    if (!bridge || !path) return false;
    param_entry_t* p = find_param((uft_param_bridge_t*)bridge, path);
    return p ? p->value.b : false;
}

const char* uft_param_get_string(const uft_param_bridge_t* bridge, const char* path) {
    if (!bridge || !path) return NULL;
    param_entry_t* p = find_param((uft_param_bridge_t*)bridge, path);
    return p ? p->value.s : NULL;
}

// ============================================================================
// TRANSACTIONS
// ============================================================================

void uft_param_begin_transaction(uft_param_bridge_t* bridge) {
    if (!bridge || bridge->in_transaction) return;
    
    memcpy(bridge->transaction_backup, bridge->params, sizeof(bridge->params));
    bridge->transaction_count = bridge->param_count;
    bridge->in_transaction = true;
}

int uft_param_commit_transaction(uft_param_bridge_t* bridge) {
    if (!bridge || !bridge->in_transaction) return -1;
    bridge->in_transaction = false;
    return 0;
}

void uft_param_rollback_transaction(uft_param_bridge_t* bridge) {
    if (!bridge || !bridge->in_transaction) return;
    
    memcpy(bridge->params, bridge->transaction_backup, sizeof(bridge->params));
    bridge->param_count = bridge->transaction_count;
    bridge->in_transaction = false;
}

// ============================================================================
// JSON EXPORT
// ============================================================================

int uft_param_export_json(uft_param_bridge_t* bridge, char* buffer, size_t size) {
    if (!bridge || !buffer || size == 0) return -1;
    
    int pos = 0;
    pos += snprintf(buffer + pos, size - pos, "{\n");
    
    for (int i = 0; i < bridge->param_count; i++) {
        param_entry_t* p = &bridge->params[i];
        
        pos += snprintf(buffer + pos, size - pos, "  \"%s\": ", p->path);
        
        switch (p->type) {
            case UFT_PARAM_TYPE_INT:
                pos += snprintf(buffer + pos, size - pos, "%ld", (long)p->value.i);
                break;
            case UFT_PARAM_TYPE_DOUBLE:
                pos += snprintf(buffer + pos, size - pos, "%g", p->value.d);
                break;
            case UFT_PARAM_TYPE_BOOL:
                pos += snprintf(buffer + pos, size - pos, "%s", p->value.b ? "true" : "false");
                break;
            case UFT_PARAM_TYPE_STRING:
                pos += snprintf(buffer + pos, size - pos, "\"%s\"", p->value.s ? p->value.s : "");
                break;
            default:
                pos += snprintf(buffer + pos, size - pos, "null");
        }
        
        if (i < bridge->param_count - 1) {
            pos += snprintf(buffer + pos, size - pos, ",");
        }
        pos += snprintf(buffer + pos, size - pos, "\n");
    }
    
    pos += snprintf(buffer + pos, size - pos, "}\n");
    return 0;
}
