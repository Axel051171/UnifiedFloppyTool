/**
 * @file uft_preset.h
 * @brief Format preset system
 * 
 * Presets define common disk format configurations for various platforms
 * (Commodore, Atari, Apple, PC-98, etc.)
 */
#ifndef UFT_PRESET_H
#define UFT_PRESET_H

#include <stdint.h>

/** @brief Format preset configuration (opaque) */
typedef struct uft_preset uft_preset_t;

/**
 * @brief Find preset by name
 * @param name Preset name (e.g., "commodore_1541", "amiga_dd")
 * @return Preset pointer or NULL if not found
 */
const uft_preset_t *uft_preset_find(const char *name);

#endif /* UFT_PRESET_H */
