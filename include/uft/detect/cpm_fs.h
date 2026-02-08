/**
 * =============================================================================
 * CP/M Dateisystem-Zugriff
 * =============================================================================
 *
 * Lesen, Schreiben und Extrahieren von Dateien auf CP/M-Disketten.
 * Nutzt die erkannten Parameter aus dem MFM-Detect-Modul.
 *
 * Unterstützt:
 *   - CP/M 2.2 (Standard, 8-Bit Block-Pointer)
 *   - CP/M 3.0 / Plus (16-Bit Block-Pointer, Timestamps)
 *   - P2DOS / Z80DOS Timestamps
 *   - User-Nummern 0-31
 *   - Blockgrößen 1K, 2K, 4K, 8K, 16K
 *   - 8-Bit und 16-Bit Block-Allokation
 *   - Sektor-Skew/Interleave
 *   - Multi-Extent-Dateien
 *
 * Design:
 *   cpm_disk_t ist das zentrale Handle. Es wird mit den physikalischen
 *   Parametern und einem Sektor-Lese/Schreib-Callback initialisiert.
 *   Die DPB-Parameter können automatisch (aus MFM-Detect) oder manuell
 *   gesetzt werden.
 *
 * Integration: UFI Preservation Platform
 * =============================================================================
 */

#ifndef CPM_FS_H
#define CPM_FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>


/* =============================================================================
 * Konstanten
 * ========================================================================== */

#define CPM_FILENAME_MAX    8
#define CPM_EXTENSION_MAX   3
#define CPM_FULLNAME_MAX    13   /* "FILENAME.EXT\0" */
#define CPM_DIR_ENTRY_SIZE  32
#define CPM_RECORD_SIZE     128  /* CP/M "Record" = 128 Bytes */
#define CPM_MAX_EXTENTS     512  /* Max Extents pro Datei */
#define CPM_MAX_FILES       512
#define CPM_MAX_SECTOR_SIZE 4096
#define CPM_DELETED         0xE5
#define CPM_UNUSED          0x00

/** Maximale Block-Pointer pro Extent (8-Bit oder 16-Bit) */
#define CPM_ALLOC_8BIT      16   /* 16 × 8-Bit Pointer */
#define CPM_ALLOC_16BIT     8    /* 8 × 16-Bit Pointer */


/* =============================================================================
 * Fehlercodes
 * ========================================================================== */

typedef enum {
    CPM_OK = 0,
    CPM_ERR_NULL,
    CPM_ERR_ALLOC,
    CPM_ERR_PARAMS,        /* Ungültige DPB/Geometrie */
    CPM_ERR_READ,          /* Sektor-Lesefehler */
    CPM_ERR_WRITE,         /* Sektor-Schreibfehler */
    CPM_ERR_NOT_FOUND,     /* Datei nicht gefunden */
    CPM_ERR_EXISTS,        /* Datei existiert bereits */
    CPM_ERR_DIR_FULL,      /* Directory voll */
    CPM_ERR_DISK_FULL,     /* Disk voll */
    CPM_ERR_CORRUPT,       /* Korruptes Dateisystem */
    CPM_ERR_NAME,          /* Ungültiger Dateiname */
    CPM_ERR_READ_ONLY,     /* Schreibgeschützt */
    CPM_ERR_IO,            /* Allgemeiner I/O-Fehler */
} cpm_error_t;


/* =============================================================================
 * Strukturen
 * ========================================================================== */

/**
 * CP/M Disk Parameter Block
 */
typedef struct {
    uint16_t spt;          /**< 128-Byte Records pro Spur */
    uint8_t  bsh;          /**< Block Shift Factor */
    uint8_t  blm;          /**< Block Mask */
    uint8_t  exm;          /**< Extent Mask */
    uint16_t dsm;          /**< Disk Storage Maximum (höchster Blocknr.) */
    uint16_t drm;          /**< Directory Maximum (höchster Eintrag-Nr.) */
    uint8_t  al0;          /**< Alloc Bitmap High */
    uint8_t  al1;          /**< Alloc Bitmap Low */
    uint16_t cks;          /**< Check Vector Size */
    uint16_t off;          /**< Reserved Tracks */

    /* Berechnete Felder */
    uint16_t block_size;   /**< Bytes pro Block */
    uint16_t dir_entries;  /**< drm + 1 */
    uint16_t dir_blocks;   /**< Anzahl Directory-Blöcke */
    uint32_t disk_capacity;/**< Gesamtkapazität (dsm+1)*block_size */
    bool     use_16bit;    /**< true wenn dsm > 255 → 16-Bit Pointer */
    uint8_t  al_per_ext;   /**< Alloc-Einträge pro Extent (8 oder 16) */
} cpm_dpb_t;

/**
 * Physikalische Disk-Geometrie
 */
typedef struct {
    uint16_t sector_size;       /**< Bytes pro physischem Sektor */
    uint8_t  sectors_per_track; /**< Physische Sektoren pro Spur */
    uint8_t  heads;             /**< 1 oder 2 */
    uint16_t cylinders;         /**< Zylinder */
    uint8_t  first_sector;      /**< Erste Sektor-ID (0 oder 1) */
    uint8_t  skew;              /**< Sektor-Skew */
    uint8_t  *skew_table;       /**< Skew-Tabelle (NULL = keine) */
    uint8_t  skew_table_len;    /**< Länge der Skew-Tabelle */
} cpm_geometry_t;

/**
 * CP/M Timestamp (P2DOS/CP/M 3)
 */
typedef struct {
    uint16_t days;         /**< Tage seit 1.1.1978 */
    uint8_t  hours;        /**< Stunden (BCD) */
    uint8_t  minutes;      /**< Minuten (BCD) */
    bool     valid;
} cpm_timestamp_t;

/**
 * CP/M Datei-Info
 */
typedef struct {
    char     name[CPM_FULLNAME_MAX];  /**< "FILENAME.EXT" */
    char     raw_name[9];             /**< CP/M-Rohname (8 Zeichen) */
    char     raw_ext[4];              /**< CP/M-Roherweiterung (3 Zeichen) */
    uint8_t  user;                    /**< User-Nummer */
    bool     read_only;               /**< Read-Only Flag (T1') */
    bool     system;                  /**< System Flag (T2') */
    bool     archived;                /**< Archive Flag (T3') */
    uint32_t size;                    /**< Dateigröße in Bytes (geschätzt) */
    uint16_t records;                 /**< Anzahl 128-Byte Records */
    uint16_t blocks;                  /**< Belegte Blöcke */
    uint8_t  extents;                 /**< Anzahl Extents */
    uint16_t first_extent_idx;        /**< Index des ersten Extents im Dir */

    /* Timestamps (wenn vorhanden) */
    cpm_timestamp_t created;
    cpm_timestamp_t modified;
    cpm_timestamp_t accessed;
} cpm_file_info_t;

/**
 * CP/M Directory-Eintrag (Roh, 32 Bytes)
 */
typedef struct {
    uint8_t  status;               /**< User-Nr oder 0xE5 (gelöscht) */
    uint8_t  name[8];              /**< Dateiname */
    uint8_t  ext[3];               /**< Erweiterung (mit Attribut-Bits) */
    uint8_t  ex;                   /**< Extent Low */
    uint8_t  s1;                   /**< Reserved */
    uint8_t  s2;                   /**< Extent High */
    uint8_t  rc;                   /**< Record Count */
    uint8_t  al[16];               /**< Block-Allokation */
} cpm_raw_dirent_t;

/**
 * Sektor I/O Callbacks
 */
typedef cpm_error_t (*cpm_read_fn)(void *ctx, uint16_t cyl, uint8_t head,
                                    uint8_t sector, uint8_t *buf,
                                    uint16_t *bytes_read);
typedef cpm_error_t (*cpm_write_fn)(void *ctx, uint16_t cyl, uint8_t head,
                                     uint8_t sector, const uint8_t *buf,
                                     uint16_t size);

/**
 * CP/M Disk Handle
 */
typedef struct {
    cpm_dpb_t       dpb;
    cpm_geometry_t  geom;

    /* I/O */
    cpm_read_fn     read_sector;
    cpm_write_fn    write_sector;
    void           *io_ctx;
    bool            read_only;

    /* Directory Cache */
    uint8_t        *dir_buffer;     /**< Rohe Directory-Daten */
    uint32_t        dir_buf_size;   /**< Größe des Directory-Buffers */
    bool            dir_loaded;     /**< Directory geladen */
    bool            dir_dirty;      /**< Änderungen vorhanden */

    /* Allokations-Bitmap */
    uint8_t        *alloc_map;      /**< 1 Bit pro Block */
    uint16_t        alloc_map_size; /**< Bytes im alloc_map */

    /* Datei-Index */
    cpm_file_info_t files[CPM_MAX_FILES];
    uint16_t        num_files;

    /* Status */
    bool            mounted;
    uint32_t        free_blocks;
    uint32_t        used_blocks;
} cpm_disk_t;


/* =============================================================================
 * API: Disk öffnen / schließen
 * ========================================================================== */

/**
 * CP/M Disk erstellen und öffnen
 *
 * @param geom      Physikalische Geometrie
 * @param dpb       Disk Parameter Block (kann NULL sein → Auto-Detect)
 * @param read_fn   Sektor-Lese-Callback
 * @param write_fn  Sektor-Schreib-Callback (NULL = read-only)
 * @param io_ctx    Benutzer-Kontext für Callbacks
 * @return Disk-Handle oder NULL bei Fehler
 */
cpm_disk_t *cpm_open(const cpm_geometry_t *geom,
                       const cpm_dpb_t *dpb,
                       cpm_read_fn read_fn,
                       cpm_write_fn write_fn,
                       void *io_ctx);

/**
 * CP/M Disk schließen
 *
 * Schreibt ggf. ausstehende Änderungen und gibt Speicher frei.
 */
cpm_error_t cpm_close(cpm_disk_t *disk);

/**
 * DPB manuell setzen (nach cpm_open mit dpb=NULL)
 */
cpm_error_t cpm_set_dpb(cpm_disk_t *disk, const cpm_dpb_t *dpb);

/**
 * DPB aus Standardwerten berechnen
 */
cpm_error_t cpm_calc_dpb(cpm_dpb_t *dpb, uint16_t block_size,
                           uint16_t dir_entries, uint16_t off,
                           const cpm_geometry_t *geom);


/* =============================================================================
 * API: Directory lesen
 * ========================================================================== */

/**
 * Directory von Disk laden
 */
cpm_error_t cpm_read_directory(cpm_disk_t *disk);

/**
 * Anzahl Dateien (alle User)
 */
uint16_t cpm_file_count(const cpm_disk_t *disk);

/**
 * Datei-Info per Index abrufen
 */
const cpm_file_info_t *cpm_get_file(const cpm_disk_t *disk, uint16_t index);

/**
 * Datei per Name und User suchen
 *
 * @param name  "FILENAME.EXT" (case-insensitive)
 * @param user  User-Nummer (0-31, oder 0xFF für alle User)
 */
const cpm_file_info_t *cpm_find_file(const cpm_disk_t *disk,
                                       const char *name, uint8_t user);

/**
 * Datei-Liste formatiert ausgeben
 */
void cpm_list_files(const cpm_disk_t *disk, FILE *out,
                     uint8_t user_filter, bool show_system);


/* =============================================================================
 * API: Dateien lesen
 * ========================================================================== */

/**
 * Datei in Speicher lesen
 *
 * @param disk    Disk-Handle
 * @param info    Datei-Info (von cpm_find_file/cpm_get_file)
 * @param buf     Zielpuffer (muss groß genug sein)
 * @param bufsize Puffergröße
 * @param bytes_read  Tatsächlich gelesene Bytes
 * @return CPM_OK bei Erfolg
 */
cpm_error_t cpm_read_file(cpm_disk_t *disk,
                            const cpm_file_info_t *info,
                            uint8_t *buf, uint32_t bufsize,
                            uint32_t *bytes_read);

/**
 * Datei in Datei extrahieren
 */
cpm_error_t cpm_extract_file(cpm_disk_t *disk,
                               const cpm_file_info_t *info,
                               const char *dest_path);

/**
 * Alle Dateien in Verzeichnis extrahieren
 */
cpm_error_t cpm_extract_all(cpm_disk_t *disk, const char *dest_dir,
                              uint8_t user_filter);


/* =============================================================================
 * API: Dateien schreiben
 * ========================================================================== */

/**
 * Datei auf Disk schreiben
 *
 * @param disk    Disk-Handle
 * @param name    "FILENAME.EXT"
 * @param user    User-Nummer
 * @param data    Quelldaten
 * @param size    Größe in Bytes
 * @return CPM_OK bei Erfolg
 */
cpm_error_t cpm_write_file(cpm_disk_t *disk,
                             const char *name, uint8_t user,
                             const uint8_t *data, uint32_t size);

/**
 * Datei von Datei importieren
 */
cpm_error_t cpm_import_file(cpm_disk_t *disk,
                              const char *src_path,
                              const char *cpm_name, uint8_t user);

/**
 * Datei löschen
 */
cpm_error_t cpm_delete_file(cpm_disk_t *disk,
                              const char *name, uint8_t user);

/**
 * Datei umbenennen
 */
cpm_error_t cpm_rename_file(cpm_disk_t *disk,
                              const char *old_name, const char *new_name,
                              uint8_t user);

/**
 * Datei-Attribute setzen
 */
cpm_error_t cpm_set_attributes(cpm_disk_t *disk,
                                 const char *name, uint8_t user,
                                 bool read_only, bool system, bool archived);


/* =============================================================================
 * API: Disk-Verwaltung
 * ========================================================================== */

/**
 * Disk formatieren (Directory leeren, Allokation zurücksetzen)
 */
cpm_error_t cpm_format(cpm_disk_t *disk);

/**
 * Freien Speicher berechnen
 *
 * @param free_bytes    Freie Bytes (Ausgabe)
 * @param total_bytes   Gesamte Bytes (Ausgabe)
 */
cpm_error_t cpm_free_space(const cpm_disk_t *disk,
                             uint32_t *free_bytes, uint32_t *total_bytes);

/**
 * Änderungen auf Disk schreiben (Directory flush)
 */
cpm_error_t cpm_sync(cpm_disk_t *disk);

/**
 * Disk-Info formatiert ausgeben
 */
void cpm_print_info(const cpm_disk_t *disk, FILE *out);

/**
 * DPB formatiert ausgeben
 */
void cpm_print_dpb(const cpm_dpb_t *dpb, FILE *out);

/**
 * Allokations-Map ausgeben
 */
void cpm_print_allocation(const cpm_disk_t *disk, FILE *out);


/* =============================================================================
 * API: Hilfsfunktionen
 * ========================================================================== */

/**
 * CP/M Dateiname parsen ("FILENAME.EXT" → name[8] + ext[3])
 */
cpm_error_t cpm_parse_name(const char *input, char name[9], char ext[4]);

/**
 * CP/M Dateiname zusammenbauen (name[8] + ext[3] → "FILENAME.EXT")
 */
void cpm_format_name(const uint8_t *raw_name, const uint8_t *raw_ext,
                      char *output);

/**
 * Fehlerbeschreibung
 */
const char *cpm_error_str(cpm_error_t err);

/**
 * Timestamp in lesbare Form konvertieren
 * Ausgabe: "YYYY-MM-DD HH:MM"
 */
void cpm_format_timestamp(const cpm_timestamp_t *ts, char *buf, size_t bufsize);

/**
 * Datum/Uhrzeit in CP/M Timestamp konvertieren
 */
void cpm_make_timestamp(cpm_timestamp_t *ts,
                          int year, int month, int day,
                          int hours, int minutes);


#endif /* CPM_FS_H */
