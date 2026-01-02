/**
 * @file uft_core.h
 * @brief UnifiedFloppyTool - Core API
 * 
 * Zentrale Disk/Track/Sector Abstraktion.
 * Dies ist die Haupt-API für alle UFT-Operationen.
 */

#ifndef UFT_CORE_H
#define UFT_CORE_H

#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Disk API
// ============================================================================

/**
 * @brief Disk-Image öffnen
 * 
 * @param path Pfad zur Datei
 * @param read_only Nur-Lesen Modus
 * @return Disk-Handle oder NULL bei Fehler
 */
uft_disk_t* uft_disk_open(const char* path, bool read_only);

/**
 * @brief Disk-Image mit Format-Hinweis öffnen
 * 
 * @param path Pfad zur Datei
 * @param format Format (UFT_FORMAT_UNKNOWN für Auto-Detect)
 * @param read_only Nur-Lesen Modus
 * @return Disk-Handle oder NULL bei Fehler
 */
uft_disk_t* uft_disk_open_format(const char* path, uft_format_t format, bool read_only);

/**
 * @brief Neues Disk-Image erstellen
 * 
 * @param path Pfad für neue Datei
 * @param format Ziel-Format
 * @param geometry Disk-Geometrie
 * @return Disk-Handle oder NULL bei Fehler
 */
uft_disk_t* uft_disk_create(const char* path, uft_format_t format,
                             const uft_geometry_t* geometry);

/**
 * @brief Disk-Image schließen
 * 
 * @param disk Disk-Handle
 */
void uft_disk_close(uft_disk_t* disk);

/**
 * @brief Disk-Format holen
 */
uft_format_t uft_disk_get_format(const uft_disk_t* disk);

/**
 * @brief Disk-Geometrie holen
 */
uft_error_t uft_disk_get_geometry(const uft_disk_t* disk, uft_geometry_t* geometry);

/**
 * @brief Disk-Pfad holen
 */
const char* uft_disk_get_path(const uft_disk_t* disk);

/**
 * @brief Ist Disk modifiziert?
 */
bool uft_disk_is_modified(const uft_disk_t* disk);

/**
 * @brief Disk speichern (wenn modifiziert)
 */
uft_error_t uft_disk_save(uft_disk_t* disk);

/**
 * @brief Disk unter neuem Namen/Format speichern
 */
uft_error_t uft_disk_save_as(uft_disk_t* disk, const char* path, uft_format_t format);

// ============================================================================
// Track API
// ============================================================================

/**
 * @brief Track lesen
 * 
 * @param disk Disk-Handle
 * @param cylinder Zylinder-Nummer
 * @param head Seite (0 oder 1)
 * @param options Lese-Optionen (NULL für Default)
 * @return Track-Handle oder NULL bei Fehler
 */
uft_track_t* uft_track_read(uft_disk_t* disk, int cylinder, int head,
                             const uft_read_options_t* options);

/**
 * @brief Track schreiben
 * 
 * @param disk Disk-Handle
 * @param track Track-Daten
 * @param options Schreib-Optionen (NULL für Default)
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_track_write(uft_disk_t* disk, const uft_track_t* track,
                             const uft_write_options_t* options);

/**
 * @brief Track freigeben
 */
void uft_track_free(uft_track_t* track);

/**
 * @brief Track-Position holen
 */
void uft_track_get_position(const uft_track_t* track, int* cylinder, int* head);

/**
 * @brief Anzahl Sektoren im Track
 */
size_t uft_track_get_sector_count(const uft_track_t* track);

/**
 * @brief Sektor aus Track holen
 * 
 * @param track Track-Handle
 * @param index Index (0 bis count-1)
 * @return Sektor-Pointer (gehört zum Track, nicht freigeben!)
 */
const uft_sector_t* uft_track_get_sector(const uft_track_t* track, size_t index);

/**
 * @brief Sektor nach ID finden
 * 
 * @param track Track-Handle
 * @param sector Sektor-Nummer
 * @return Sektor-Pointer oder NULL wenn nicht gefunden
 */
const uft_sector_t* uft_track_find_sector(const uft_track_t* track, int sector);

/**
 * @brief Track-Status holen
 */
uint32_t uft_track_get_status(const uft_track_t* track);

/**
 * @brief Track-Metriken holen
 */
uft_error_t uft_track_get_metrics(const uft_track_t* track, uft_track_metrics_t* metrics);

/**
 * @brief Hat Track Flux-Daten?
 */
bool uft_track_has_flux(const uft_track_t* track);

/**
 * @brief Flux-Daten holen
 * 
 * @param track Track-Handle
 * @param flux_out Pointer auf Flux-Array (wird allokiert)
 * @param count_out Anzahl Flux-Transitions
 * @return UFT_OK bei Erfolg
 * 
 * @note Caller muss flux_out mit free() freigeben!
 */
uft_error_t uft_track_get_flux(const uft_track_t* track, 
                                uint32_t** flux_out, size_t* count_out);

// ============================================================================
// Sektor API
// ============================================================================

/**
 * @brief Einzelnen Sektor lesen
 * 
 * @param disk Disk-Handle
 * @param cylinder Zylinder
 * @param head Seite
 * @param sector Sektor-Nummer
 * @param buffer Zielpuffer
 * @param buffer_size Puffergröße
 * @return Gelesene Bytes oder Fehler
 */
int uft_sector_read(uft_disk_t* disk, int cylinder, int head, int sector,
                    uint8_t* buffer, size_t buffer_size);

/**
 * @brief Einzelnen Sektor schreiben
 * 
 * @param disk Disk-Handle
 * @param cylinder Zylinder
 * @param head Seite
 * @param sector Sektor-Nummer
 * @param data Quelldaten
 * @param data_size Datengröße
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_sector_write(uft_disk_t* disk, int cylinder, int head, int sector,
                              const uint8_t* data, size_t data_size);

/**
 * @brief Block lesen (LBA-Adressierung)
 * 
 * @param disk Disk-Handle
 * @param lba Logical Block Address
 * @param buffer Zielpuffer
 * @param buffer_size Puffergröße
 * @return Gelesene Bytes oder Fehler
 */
int uft_block_read(uft_disk_t* disk, uint32_t lba, uint8_t* buffer, size_t buffer_size);

/**
 * @brief Block schreiben (LBA-Adressierung)
 */
uft_error_t uft_block_write(uft_disk_t* disk, uint32_t lba, 
                             const uint8_t* data, size_t data_size);

/**
 * @brief CHS zu LBA konvertieren
 */
uint32_t uft_chs_to_lba(const uft_geometry_t* geo, int cyl, int head, int sector);

/**
 * @brief LBA zu CHS konvertieren
 */
void uft_lba_to_chs(const uft_geometry_t* geo, uint32_t lba,
                    int* cyl, int* head, int* sector);

// ============================================================================
// Konvertierung API
// ============================================================================

/**
 * @brief Disk konvertieren
 * 
 * @param src Quell-Disk
 * @param dst_path Ziel-Pfad
 * @param options Konvertierungs-Optionen
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_convert(uft_disk_t* src, const char* dst_path,
                         const uft_convert_options_t* options);

/**
 * @brief Kann Format A nach B konvertiert werden?
 */
bool uft_can_convert(uft_format_t from, uft_format_t to);

// ============================================================================
// Analyse API
// ============================================================================

/**
 * @brief Disk-Analyse-Ergebnis
 */
typedef struct uft_analysis {
    uft_format_t      detected_format;
    uft_encoding_t    detected_encoding;
    uft_geometry_t    detected_geometry;
    
    uint32_t          total_tracks;
    uint32_t          readable_tracks;
    uint32_t          total_sectors;
    uint32_t          readable_sectors;
    uint32_t          crc_errors;
    
    bool              has_copy_protection;
    bool              has_weak_bits;
    bool              is_uniform;       ///< Alle Tracks gleich?
    
    char              volume_name[64];
    char              filesystem[32];
} uft_analysis_t;

/**
 * @brief Disk analysieren
 * 
 * @param disk Disk-Handle
 * @param analysis Ergebnis-Struktur
 * @param progress Progress-Callback (optional)
 * @param user User-Daten für Callback
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_analyze(uft_disk_t* disk, uft_analysis_t* analysis,
                         uft_progress_fn progress, void* user);

/**
 * @brief Format auto-detecten
 * 
 * @param path Dateipfad
 * @param confidence Confidence (0-100), optional
 * @return Erkanntes Format oder UFT_FORMAT_UNKNOWN
 */
uft_format_t uft_detect_format(const char* path, int* confidence);

/**
 * @brief Encoding auto-detecten aus Flux-Daten
 * 
 * @param flux Flux-Transitions
 * @param count Anzahl Transitions
 * @return Erkannte Kodierung
 */
uft_encoding_t uft_detect_encoding(const uint32_t* flux, size_t count);

// ============================================================================
// Utility
// ============================================================================

/**
 * @brief UFT initialisieren
 * 
 * Muss vor allen anderen Aufrufen erfolgen.
 * Lädt Plugins, initialisiert Subsysteme.
 */
uft_error_t uft_init(void);

/**
 * @brief UFT beenden
 * 
 * Gibt alle Ressourcen frei.
 */
void uft_shutdown(void);

/**
 * @brief Version holen
 */
const char* uft_version(void);

/**
 * @brief Log-Handler setzen
 */
void uft_set_log_handler(uft_log_fn handler, void* user);

/**
 * @brief Standard-Lese-Optionen holen (modifizierbar)
 */
uft_read_options_t* uft_get_default_read_options(void);

/**
 * @brief Standard-Schreib-Optionen holen (modifizierbar)
 */
uft_write_options_t* uft_get_default_write_options(void);

// ============================================================================
// Format Registry
// ============================================================================

/**
 * @brief Alle registrierten Formate auflisten
 * 
 * @param formats Array für Format-Infos
 * @param max Maximale Anzahl
 * @return Anzahl der Formate
 */
size_t uft_list_formats(const uft_format_info_t** formats, size_t max);

/**
 * @brief Format für Extension finden
 */
uft_format_t uft_format_for_extension(const char* ext);

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Alle Tracks durchlaufen
 */
#define UFT_FOR_EACH_TRACK(disk, cyl, head) \
    for (int cyl = 0; cyl < (disk)->geometry.cylinders; cyl++) \
        for (int head = 0; head < (disk)->geometry.heads; head++)

/**
 * @brief Alle Sektoren eines Tracks durchlaufen
 */
#define UFT_FOR_EACH_SECTOR(track, idx, sector) \
    for (size_t idx = 0, _max = uft_track_get_sector_count(track); \
         idx < _max && ((sector) = uft_track_get_sector(track, idx)); \
         idx++)

#ifdef __cplusplus
}
#endif

#endif // UFT_CORE_H
