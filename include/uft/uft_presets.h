/**
 * @file uft_presets.h
 * @brief Preset System API
 */

#ifndef UFT_PRESETS_H
#define UFT_PRESETS_H

#include "uft_params.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Preset Categories
// ============================================================================

typedef enum uft_preset_category {
    UFT_PRESET_CAT_GENERAL,
    UFT_PRESET_CAT_COMMODORE,
    UFT_PRESET_CAT_AMIGA,
    UFT_PRESET_CAT_APPLE,
    UFT_PRESET_CAT_IBM_PC,
    UFT_PRESET_CAT_ATARI,
    UFT_PRESET_CAT_PRESERVATION,
    UFT_PRESET_CAT_COPY_PROTECTION,
    UFT_PRESET_CAT_USER,
} uft_preset_category_t;

// ============================================================================
// Preset Structure
// ============================================================================

typedef struct uft_preset {
    char                    name[64];
    char                    description[256];
    uft_preset_category_t   category;
    bool                    is_builtin;
    bool                    is_modified;
    uft_params_t            params;
} uft_preset_t;

// ============================================================================
// API
// ============================================================================

uft_error_t uft_preset_init(void);
size_t uft_preset_count(void);
const uft_preset_t* uft_preset_get(size_t index);
const uft_preset_t* uft_preset_find(const char* name);

uft_error_t uft_preset_save(const char* name, const uft_params_t* params);
uft_error_t uft_preset_load(const char* name, uft_params_t* params);
uft_error_t uft_preset_delete(const char* name);

int uft_preset_list(const char** names, int max_count);
int uft_preset_list_by_category(uft_preset_category_t cat,
                                 const uft_preset_t** presets,
                                 int max_count);
const char* uft_preset_category_name(uft_preset_category_t cat);

#ifdef __cplusplus
}
#endif

#endif // UFT_PRESETS_H
