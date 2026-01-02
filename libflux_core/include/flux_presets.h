#pragma once
/*
 * flux_presets.h — Named policy presets ("Preset-Registry")
 *
 * Presets are stable identifiers (strings) that map to policy structs.
 * They are meant to be discoverable by GUI/CLI.
 */

#include <stddef.h>
#include "flux_policy.h"
#include "ufm_media.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flux_preset {
    const char *name;        /* stable id, lowercase, no spaces */
    const char *title;       /* human label */
    const char *desc;        /* short description */
    ufm_media_profile_t media; /* media/profile hint */
    flux_policy_t policy;    /* full policy snapshot */
} flux_preset_t;

const flux_preset_t* flux_presets_all(size_t *count);
const flux_preset_t* flux_preset_find(const char *name);

/* Copy preset into out_policy; returns 0 on success. */
int flux_preset_get_policy(const char *name, flux_policy_t *out_policy);

#ifdef __cplusplus
}
#endif
