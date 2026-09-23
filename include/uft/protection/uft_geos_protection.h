/**
 * @file uft_geos_protection.h
 * @brief GEOS Copy Protection Detection Interface
 */

#ifndef UFT_GEOS_PROTECTION_H
#define UFT_GEOS_PROTECTION_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of protections that can be detected */
#define GEOS_MAX_PROTECTIONS  16

/* Protection types */
typedef enum {
    GEOS_PROT_NONE = 0,
    GEOS_PROT_V1_KEY_DISK,      /* Original GEOS key disk */
    GEOS_PROT_V2_ENHANCED,      /* GEOS 2.0+ enhanced protection */
    GEOS_PROT_SERIAL_CHECK,     /* Serial number verification */
    GEOS_PROT_HALF_TRACK,       /* Half-track data */
    GEOS_PROT_BAM_SIGNATURE,    /* Modified BAM entries */
    GEOS_PROT_INTERLEAVE,       /* Non-standard interleave */
    GEOS_PROT_SYNC_MARK,        /* Custom sync marks */
} geos_protection_type_t;

/* Protection severity */
typedef enum {
    GEOS_SEV_NONE = 0,
    GEOS_SEV_TRIVIAL,           /* Easily bypassed */
    GEOS_SEV_STANDARD,          /* Requires nibbler */
    GEOS_SEV_DIFFICULT,         /* May require flux capture */
} geos_severity_t;

/* Protection info */
typedef struct {
    geos_protection_type_t type;
    const char *name;
    const char *description;
    geos_severity_t severity;
    bool copyable_with_nibbler;
    bool requires_original;
} geos_protection_info_t;

/**
 * @brief Was der Detektor NICHT messen konnte — Bitmaske (MF-1333).
 *
 * Eine 0 in `protection_count` ist mehrdeutig: sie kann "gemessen, kein
 * Schutz" heissen oder "konnte gar nicht hinsehen". Dieselbe Dreiteilung
 * wie `caps`/`caps_bekannt` in `uft_copy_plan.h` (MF-1311).
 */
typedef enum {
    GEOS_UNMESSBAR_NICHTS      = 0,
    /** Der eigentliche Schutz liegt in den LUECKEN zwischen den Sektoren
     *  von Spur 21. Aus Sektordaten ist er prinzipiell unerreichbar; es
     *  braucht die rohe GCR-Spur. */
    GEOS_UNMESSBAR_GAPS        = 1u << 0,
    /** BAM-Sektor (Spur 18, Sektor 0) fehlt oder ist kuerzer als 256 Byte. */
    GEOS_UNMESSBAR_BAM         = 1u << 1,
    /** Die Blockkette des GEOS KERNAL wurde nicht verfolgt — ohne sie
     *  laesst sich "in der BAM belegt, aber von keinem Verzeichniseintrag
     *  referenziert" nicht entscheiden. */
    GEOS_UNMESSBAR_VERZEICHNIS = 1u << 2
} geos_unmessbar_t;

/* Analysis result */
typedef struct {
    bool is_geos_disk;
    int geos_version;
    int protection_count;
    geos_protection_type_t protections[GEOS_MAX_PROTECTIONS];

    /* ---- MF-1333: angehaengt, bestehende Feld-Offsets unveraendert ----
     *
     * Bis MF-1333 meldete dieses Modul drei Schutzarten, von denen KEINE
     * aus einer Messung am Datentraeger stammte (gemessen: Spur-36-
     * Daseinsabfrage, bedingungsloses V2 hinter einer Zeichenkettensuche,
     * und der Freisektor-Zaehler der FALSCHEN Spur). Die Felder unten
     * trennen, was gemessen wurde, von dem, was offen blieb.
     */

    /** Bitmaske aus @ref geos_unmessbar_t. 0 = nichts blieb offen. */
    uint32_t unmessbar;

    /** true, wenn der BAM-Eintrag fuer Spur 21 wirklich gelesen wurde.
     *  Ohne dieses Flag waere `spur21_frei == 0` nicht von "nicht
     *  gemessen" zu unterscheiden. */
    bool     spur21_bam_gelesen;

    /** Freie Sektoren auf Spur 21 laut BAM. Nur gueltig, wenn
     *  @ref spur21_bam_gelesen gesetzt ist. */
    uint8_t  spur21_frei;

    /** Sektoren auf Spur 21 laut Geometrie (19 bei 1541). Kommt aus
     *  `uft_cbm_sectors_per_track()` — EINE Rechnung, nicht nachgebaut
     *  (MF-1177). Nur gueltig mit @ref spur21_bam_gelesen. */
    uint8_t  spur21_sektoren;

    /** 0..100. Wie sicher ist "hier ist der GEOS-Bootschutz"? Ohne die
     *  Gaps ist die Obergrenze bewusst niedrig — siehe Dateikopf. */
    uint8_t  konfidenz;
} geos_analysis_result_t;

/* GEOS file info */
typedef struct {
    bool is_geos_file;
    uint8_t dos_file_type;
    uint8_t geos_file_type;
    uint8_t structure_type;
    uint16_t load_address;
    uint16_t end_address;
    uint16_t start_address;
    char class_name[21];
    char author[25];
    char parent_app[21];
    char description[33];
    uint8_t icon_data[63];
} geos_file_info_t;

/* Detection functions */

/**
 * @brief Traegt dieser Sektor die GEOS-Kennung?
 *
 * @param sector_data Der **BAM-SEKTOR** — Spur 18, Sektor 0. NICHT der
 *        Bootsektor; der historische Name sagt "boot" und traegt das
 *        nicht (MF-1333, Name beibehalten wegen MF-1077).
 * @param size Laenge des Sektors; unter $AD+11 Byte lautet die Antwort
 *        false, weil die Kennung dann gar nicht im Puffer liegt.
 *
 * Geprueft wird `"GEOS format"` an der FESTEN Stelle $AD — das ist die
 * Pruefung, die GEOS selbst anstellt
 * (`docs/format_specs/commodore/GEOS.TXT:247-249`). Bis MF-1333 suchte
 * diese Funktion vier Byte `"GEOS"` an beliebigem Versatz, was in einem
 * 256-Byte-Sektor keine Kennung ist, sondern eine
 * Wahrscheinlichkeitsaussage.
 */
bool uft_geos_detect_boot_signature(const uint8_t *sector_data, size_t size);

/**
 * @brief Traegt die Kennung eine Fassungsziffer >= 2?
 *
 * @param sector_data Der BAM-Sektor, wie oben.
 *
 * Eine eigene "erweiterte Kennung" gibt es nicht: GEOS.TXT kennt EINE
 * Kennung, und die Fassung steht als ASCII-Ziffer bei $BA dahinter
 * ("GEOS format V1.x"). Bis MF-1333 galt das blosse Vorhandensein von
 * `"GEOS format"` als Beweis fuer Fassung 2 — gemessen traegt aber JEDE
 * GEOS-Diskette diese Zeichenkette, auch eine V1.3.
 */
bool uft_geos_detect_extended_signature(const uint8_t *sector_data, size_t size);

int uft_geos_detect_file_type(const uint8_t *info_sector);

/* Main analysis function */
int uft_geos_analyze_disk(const uft_disk_image_t *disk, 
                          geos_analysis_result_t *result);

/* Information functions */
const geos_protection_info_t* uft_geos_get_protection_info(
    geos_protection_type_t type);
int uft_geos_get_report(const geos_analysis_result_t *result,
                        char *buffer, size_t buffer_size);

/* File analysis */
int uft_geos_analyze_file(const uint8_t *info_sector, size_t size,
                          geos_file_info_t *info);
const char* uft_geos_file_type_name(int type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GEOS_PROTECTION_H */
