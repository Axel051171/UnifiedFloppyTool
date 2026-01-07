/**
 * @file FORMAT.h
 * @brief FORMAT_NAME Disk Image Format (C11, no deps)
 * 
 * UFT - Unified Floppy Tooling
 *
 * BESCHREIBUNG DES FORMATS
 * 
 * Specs:
 *   - Link zur Dokumentation
 *   - Weitere Referenzen
 * 
 * Notes:
 *   - Besonderheiten des Formats
 *   - Bekannte Limitierungen
 *
 * @author DEIN NAME
 * @date DATUM
 */

#ifndef UFT_FORMAT_H
#define UFT_FORMAT_H

// ═══════════════════════════════════════════════════════════════════════════════
// PFLICHT-INCLUDES für alle Format-Module
// ═══════════════════════════════════════════════════════════════════════════════

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// WICHTIG: FloppyDevice ist hier zentral definiert!
#include "uft_floppy_device.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT-KONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════════

#define FORMAT_MAGIC            0x12345678  ///< Magic Bytes
#define FORMAT_HEADER_SIZE      256         ///< Header-Größe in Bytes
#define FORMAT_MAX_TRACKS       80          ///< Maximale Tracks
#define FORMAT_MAX_SECTORS      18          ///< Maximale Sektoren pro Track
#define FORMAT_SECTOR_SIZE      512         ///< Standard Sektorgröße

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT-SPEZIFISCHE STRUKTUREN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief FORMAT Header-Struktur (wie auf Disk gespeichert)
 * 
 * @note PACKED um exaktes Speicherlayout zu garantieren
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[4];          ///< Magic Bytes "FRMT"
    uint16_t version;           ///< Format-Version
    uint16_t flags;             ///< Feature-Flags
    uint8_t  tracks;            ///< Anzahl Tracks
    uint8_t  heads;             ///< Anzahl Köpfe (1 oder 2)
    uint8_t  sectors;           ///< Sektoren pro Track
    uint8_t  reserved[5];       ///< Reserved für Alignment
    // ... weitere Felder
} FormatHeader;
#pragma pack(pop)

/**
 * @brief Parsed Metadaten (nach Validierung)
 */
typedef struct {
    uint16_t version;
    uint32_t flags;
    uint8_t  tracks;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    uint32_t data_offset;       ///< Offset zum ersten Track
    // Berechnete Werte
    uint32_t total_sectors;
    uint64_t image_size;
} FormatMeta;

// ═══════════════════════════════════════════════════════════════════════════════
// UNIFIED API (PFLICHT für alle Format-Module)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Öffnet ein FORMAT Image
 * 
 * @param dev   FloppyDevice Handle
 * @param path  Pfad zur Datei
 * @return 0 bei Erfolg, Fehlercode sonst
 */
int uft_floppy_open(FloppyDevice *dev, const char *path);

/**
 * @brief Schließt ein FORMAT Image
 * 
 * @param dev  FloppyDevice Handle
 * @return 0 bei Erfolg, Fehlercode sonst
 */
int uft_floppy_close(FloppyDevice *dev);

/**
 * @brief Liest einen Sektor
 * 
 * @param dev  FloppyDevice Handle
 * @param t    Track (0-based)
 * @param h    Head (0 oder 1)
 * @param s    Sektor (1-based oder 0-based, formatabhängig)
 * @param buf  Ausgabepuffer (mind. sector_size Bytes)
 * @return 0 bei Erfolg, Fehlercode sonst
 */
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);

/**
 * @brief Schreibt einen Sektor
 * 
 * @param dev  FloppyDevice Handle
 * @param t    Track (0-based)
 * @param h    Head (0 oder 1)
 * @param s    Sektor (1-based oder 0-based, formatabhängig)
 * @param buf  Eingabepuffer
 * @return 0 bei Erfolg, Fehlercode sonst
 */
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);

/**
 * @brief Analysiert Kopierschutz
 * 
 * @param dev  FloppyDevice Handle
 * @return 0 bei Erfolg, Fehlercode sonst
 */
int uft_floppy_analyze_protection(FloppyDevice *dev);

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT-SPEZIFISCHE FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Gibt Metadaten des Images zurück
 * 
 * @param dev  FloppyDevice Handle
 * @return Pointer auf Metadaten oder NULL bei Fehler
 */
const FormatMeta* format_get_meta(const FloppyDevice *dev);

/**
 * @brief Prüft ob eine Datei ein gültiges FORMAT ist
 * 
 * @param path  Pfad zur Datei
 * @return true wenn gültiges FORMAT, false sonst
 */
bool format_is_valid(const char *path);

/**
 * @brief Erstellt ein neues leeres FORMAT Image
 * 
 * @param path    Ausgabepfad
 * @param tracks  Anzahl Tracks
 * @param heads   Anzahl Köpfe
 * @param sectors Sektoren pro Track
 * @return 0 bei Erfolg, Fehlercode sonst
 */
int format_create(const char *path, uint8_t tracks, uint8_t heads, uint8_t sectors);

// ═══════════════════════════════════════════════════════════════════════════════
// KONVERTIERUNGS-FUNKTIONEN (optional)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Exportiert als Raw-Image (CHS-geordnet)
 */
int format_export_raw(FloppyDevice *dev, const char *output_path);

/**
 * @brief Importiert von Raw-Image
 */
int format_import_raw(const char *input_path, const char *output_path,
                      uint8_t tracks, uint8_t heads, uint8_t sectors);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_H
