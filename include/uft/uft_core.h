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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
 * @brief Oeffnen mit GENANNTEM Format — die Sonde wird nicht gefragt.
 *
 * Seit MF-1251 sagt `uft_disk_open()` bei Mehrdeutigkeit AB, statt
 * einen Sieger durch Registrierungsreihenfolge zu bestimmen. Das ist
 * die Doktrin (`docs/SONDEN_DOKTRIN.md`: „bleibt es gleich, gewinnt
 * KEINER"), und dies ist der Ausweg: wer das Format kennt, nennt es.
 *
 * Gemessen betrifft das 10 von 20 Groessen der DSK-Geometrietafel und
 * 18 von 129 Korpus-Abbildern — darunter eine von VICE erzeugte
 * `.d81`, die sich 819 200 Byte mit `SAD` und `MGT` teilt.
 */
uft_disk_t* uft_disk_open_as(const char* path, bool read_only,
                             const struct uft_format_plugin* plugin);

/**
 * @brief Oeffnen MIT Rangliste — bei Mehrdeutigkeit NULL und die
 *        Kandidaten in @p ranking_out.
 *
 * Fuer jeden, der einem Menschen eine Auswahl zeigen muss. `tied`
 * sagt, wie viele gleichauf liegen; `tied_with[]` nennt bis zu vier
 * davon. Die Warnung darf nicht in einem Protokoll stehen, das
 * niemand liest — sie IST die Auswahl.
 */
uft_disk_t* uft_disk_open_ranked(const char* path, bool read_only,
                                 struct uft_probe_ranking* ranking_out);


/* uft_disk_create(path, format, geometry) — REMOVED (MF-294): this
 * 3-arg prototype never had a matching implementation. The only
 * definition (src/core/uft_core_stubs.c) is the 0-arg allocator whose
 * canonical declaration lives in include/uft/floppy/uft_floppy_v2.h.
 * A caller compiled against THIS prototype would have silently bound
 * to the 0-arg symbol with its arguments ignored — an ABI bomb
 * (signature mismatch, no compiler warning across TUs). Zero callers
 * existed; prototype deleted instead of faking an implementation. */

/**
 * @brief Disk-Image schließen
 * 
 * @param disk Disk-Handle
 */
void uft_disk_close(uft_disk_t* disk);


/**
 * @brief Disk-Geometrie holen
 */
uft_error_t uft_disk_get_geometry(const uft_disk_t* disk, uft_geometry_t* geometry);





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
 * @brief Track freigeben
 */
void uft_track_free(uft_track_t* track);


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
 * @brief Format auto-detecten
 * 
 * @param path Dateipfad
 * @param confidence Confidence (0-100), optional
 * @return Erkanntes Format oder UFT_FORMAT_UNKNOWN
 */
uft_format_t uft_detect_format(const char* path, int* confidence);


// ============================================================================
// Utility
// ============================================================================







// ============================================================================
// Format Registry
// ============================================================================



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
