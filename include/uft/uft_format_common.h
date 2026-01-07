/**
 * @file uft_format_common.h
 * @brief UnifiedFloppyTool - Gemeinsame Definitionen für Format-Plugins
 * 
 * Dieses Header vereinheitlicht die Plugin-Entwicklung und stellt
 * Hilfsfunktionen für alle Format-Plugins bereit.
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_FORMAT_COMMON_H
#define UFT_FORMAT_COMMON_H

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "uft/uft_endian.h"  // For endian functions

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Byte-Order Helpers
// ============================================================================

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

/* Note: uft_write_be16/be32/le16/le32 are now in uft/uft_endian.h */

// ============================================================================
// Sektor-Hilsfunktionen
// ============================================================================

/**
 * @brief Erstellt einen Sektor und fügt ihn zum Track hinzu
 * 
 * @param track Ziel-Track
 * @param sector_num Sektor-Nummer (0-basiert intern)
 * @param data Sektor-Daten
 * @param size Größe der Daten
 * @param cylinder Logischer Zylinder (für ID)
 * @param head Logischer Kopf (für ID)
 * @return UFT_OK bei Erfolg
 */
static inline uft_error_t uft_format_add_sector(
    uft_track_t* track,
    uint8_t sector_num,
    const uint8_t* data,
    uint16_t size,
    uint8_t cylinder,
    uint8_t head)
{
    if (!track || !data) return UFT_ERROR_NULL_POINTER;
    
    uft_sector_t sector;
    memset(&sector, 0, sizeof(sector));
    
    // ID setzen
    sector.id.cylinder = cylinder;
    sector.id.head = head;
    sector.id.sector = sector_num + 1;  // 1-basiert in ID
    
    // Size code berechnen (N = log2(size/128))
    uint8_t size_code = 0;
    uint16_t calc_size = 128;
    while (calc_size < size && size_code < 7) {
        calc_size *= 2;
        size_code++;
    }
    sector.id.size_code = size_code;
    sector.id.crc_ok = true;
    
    // Daten kopieren
    sector.data = malloc(size);
    if (!sector.data) return UFT_ERROR_NO_MEMORY;
    
    memcpy(sector.data, data, size);
    sector.data_size = size;
    sector.status = UFT_SECTOR_OK;
    
    uft_error_t err = uft_track_add_sector(track, &sector);
    if (err != UFT_OK) {
        free(sector.data);
    }
    
    return err;
}

/**
 * @brief Erstellt einen leeren Sektor
 */
static inline uft_error_t uft_format_add_empty_sector(
    uft_track_t* track,
    uint8_t sector_num,
    uint16_t size,
    uint8_t fill_byte,
    uint8_t cylinder,
    uint8_t head)
{
    uint8_t* data = malloc(size);
    if (!data) return UFT_ERROR_NO_MEMORY;
    
    memset(data, fill_byte, size);
    uft_error_t err = uft_format_add_sector(track, sector_num, data, size, cylinder, head);
    free(data);
    
    return err;
}

// ============================================================================
// File I/O Helpers
// ============================================================================

/**
 * @brief Liest komplette Datei in Speicher
 */
static inline uint8_t* uft_read_file(const char* path, size_t* size_out) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    if (size_out) *size_out = size;
    return data;
}

/**
 * @brief Schreibt Daten in Datei
 */
static inline bool uft_write_file(const char* path, const uint8_t* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    
    bool ok = (fwrite(data, 1, size, f) == size);
    fclose(f);
    return ok;
}

// ============================================================================
// Standard Sektor-Größen
// ============================================================================

static const uint16_t uft_sector_sizes[8] = {
    128, 256, 512, 1024, 2048, 4096, 8192, 16384
};

static inline uint16_t uft_size_code_to_bytes(uint8_t code) {
    return (code < 8) ? uft_sector_sizes[code] : 512;
}

static inline uint8_t uft_bytes_to_size_code(uint16_t bytes) {
    for (uint8_t i = 0; i < 8; i++) {
        if (uft_sector_sizes[i] == bytes) return i;
    }
    return 2;  // Default: 512
}

// ============================================================================
// Plugin Registration Macro
// ============================================================================

#define UFT_REGISTER_FORMAT_PLUGIN(name) \
    uft_error_t uft_register_##name(void) { \
        return uft_register_format_plugin(&uft_format_plugin_##name); \
    }

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_COMMON_H
