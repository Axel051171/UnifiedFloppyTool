/**
 * @file uft_preset.c
 * @brief Preset Implementation
 */

#include "uft/presets/uft_preset.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Built-in presets
static const uft_preset_t BUILTIN_PRESETS[] = {
    // Commodore
    {"c64_d64", "C64 D64 Standard", "Standard C64 D64 disk image", PRESET_CAT_COMMODORE, 0x0100, 0x0001, 0.25, 0.05, 3, true, 5, 2},
    {"c64_g64", "C64 G64 GCR", "C64 G64 GCR image with protection", PRESET_CAT_COMMODORE, 0x0110, 0x0001, 0.30, 0.08, 5, true, 5, 3},
    {"c64_tap", "C64 TAP Tape", "C64 tape image", PRESET_CAT_COMMODORE, 0x0130, 0x0001, 0.20, 0.03, 2, false, 5, 1},
    
    // Amiga
    {"amiga_ofs", "Amiga OFS", "Amiga OFS disk", PRESET_CAT_AMIGA, 0x0200, 0x0001, 0.25, 0.05, 3, true, 2, 2},
    {"amiga_ffs", "Amiga FFS", "Amiga FFS disk", PRESET_CAT_AMIGA, 0x0200, 0x0002, 0.25, 0.05, 3, true, 2, 2},
    {"amiga_hd", "Amiga HD", "Amiga HD floppy", PRESET_CAT_AMIGA, 0x0200, 0x0010, 0.20, 0.04, 3, true, 3, 2},
    
    // Apple
    {"apple_dos33", "Apple DOS 3.3", "Apple II DOS 3.3", PRESET_CAT_APPLE, 0x0300, 0x0001, 0.25, 0.05, 3, true, 6, 2},
    {"apple_prodos", "Apple ProDOS", "Apple ProDOS disk", PRESET_CAT_APPLE, 0x0301, 0x0001, 0.25, 0.05, 3, true, 6, 2},
    {"apple_woz", "Apple WOZ", "Apple WOZ flux image", PRESET_CAT_APPLE, 0x0320, 0x0004, 0.30, 0.08, 5, true, 6, 3},
    
    // IBM PC
    {"pc_360k", "PC 360K", "PC 5.25\" 360K", PRESET_CAT_IBM, 0x0400, 0x0008, 0.25, 0.05, 2, true, 0, 2},
    {"pc_720k", "PC 720K", "PC 3.5\" 720K", PRESET_CAT_IBM, 0x0400, 0x0010, 0.25, 0.05, 2, true, 2, 2},
    {"pc_1440k", "PC 1.44M", "PC 3.5\" 1.44M", PRESET_CAT_IBM, 0x0400, 0x0040, 0.20, 0.04, 2, true, 3, 2},
    
    // Atari
    {"atari_sd", "Atari SD", "Atari 8-bit SD", PRESET_CAT_ATARI, 0x0500, 0x0001, 0.25, 0.05, 3, true, 0, 2},
    {"atari_st", "Atari ST", "Atari ST disk", PRESET_CAT_ATARI, 0x0510, 0x0001, 0.25, 0.05, 3, true, 2, 2},
    
    // Flux
    {"flux_preserve", "Preservation", "High-quality preservation", PRESET_CAT_FLUX, 0x1000, 0x0000, 0.30, 0.10, 5, true, 0, 5},
    {"flux_analyze", "Analysis", "Detailed flux analysis", PRESET_CAT_FLUX, 0x1000, 0x0000, 0.35, 0.12, 7, true, 0, 3},
    
    // Recovery
    {"recovery_weak", "Weak Disk", "Recovery for weak/damaged", PRESET_CAT_RECOVERY, 0x0000, 0x0000, 0.40, 0.15, 10, true, 0, 10},
    {"recovery_copy", "Copy Protected", "Copy protection recovery", PRESET_CAT_RECOVERY, 0x0000, 0x0000, 0.35, 0.12, 7, true, 0, 5}
};

static const int BUILTIN_COUNT = sizeof(BUILTIN_PRESETS) / sizeof(BUILTIN_PRESETS[0]);

int uft_preset_get_builtin_count(void) {
    return BUILTIN_COUNT;
}

int uft_preset_load(const char* preset_id, uft_preset_t* preset) {
    if (!preset_id || !preset) return -1;
    
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        if (strcmp(BUILTIN_PRESETS[i].id, preset_id) == 0) {
            *preset = BUILTIN_PRESETS[i];
            return 0;
        }
    }
    
    return -1;  // Not found
}

int uft_preset_list(uft_preset_category_t category, uft_preset_t* presets, int max_count) {
    if (!presets || max_count <= 0) return 0;
    
    int count = 0;
    for (int i = 0; i < BUILTIN_COUNT && count < max_count; i++) {
        if (BUILTIN_PRESETS[i].category == category || category == PRESET_CAT_CUSTOM) {
            presets[count++] = BUILTIN_PRESETS[i];
        }
    }
    
    return count;
}

int uft_preset_save(const uft_preset_t* preset) {
    // Would save to user config directory
    return -1;  // Not implemented
}

int uft_preset_export(const uft_preset_t* preset, const char* path) {
    if (!preset || !path) return -1;
    
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"id\": \"%s\",\n", preset->id);
    fprintf(f, "  \"name\": \"%s\",\n", preset->name);
    fprintf(f, "  \"category\": %d,\n", preset->category);
    fprintf(f, "  \"format_id\": \"0x%04X\",\n", preset->format_id);
    fprintf(f, "  \"pll_phase_gain\": %.3f,\n", preset->pll_phase_gain);
    fprintf(f, "  \"pll_freq_gain\": %.3f,\n", preset->pll_freq_gain);
    fprintf(f, "  \"fusion_revolutions\": %d,\n", preset->fusion_revolutions);
    fprintf(f, "  \"crc_correction\": %s,\n", preset->crc_correction ? "true" : "false");
    fprintf(f, "  \"retry_count\": %d\n", preset->retry_count);
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

const char* uft_preset_category_name(uft_preset_category_t cat) {
    switch (cat) {
        case PRESET_CAT_COMMODORE: return "Commodore";
        case PRESET_CAT_AMIGA: return "Amiga";
        case PRESET_CAT_APPLE: return "Apple";
        case PRESET_CAT_IBM: return "IBM PC";
        case PRESET_CAT_ATARI: return "Atari";
        case PRESET_CAT_FLUX: return "Flux";
        case PRESET_CAT_RECOVERY: return "Recovery";
        case PRESET_CAT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}
