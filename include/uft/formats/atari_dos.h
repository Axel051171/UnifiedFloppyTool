/**
 * =============================================================================
 * ATARI DOS - Vollständiges Disk-Image & Dateisystem Modul
 * =============================================================================
 *
 * Unterstützte Container-Formate:
 *   - ATR (Nick Kennedy / SIO2PC)
 *
 * Unterstützte Dateisysteme:
 *   - Atari DOS 2.0 Single Density (810)
 *   - Atari DOS 2.0 Double Density (XF551)
 *   - Atari DOS 2.5 Enhanced Density (1050)
 *   - MyDOS (erweiterte DOS 2.0 Variante)
 *   - SpartaDOS (hierarchisches Dateisystem)
 *
 * Referenzen:
 *   - "Inside Atari DOS" von Bill Wilkinson (1982)
 *   - atariarchives.org/iad/
 *   - ATR Format Spec (Nick Kennedy)
 *   - SpartaDOS X Dokumentation
 *   - jhallen/atari-tools (GitHub)
 *
 * Autor: Axel (UFI Preservation Platform)
 * =============================================================================
 */

#ifndef ATARI_DOS_H
#define ATARI_DOS_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Konstanten
 * ========================================================================== */

/* ATR Container */
#define ATR_MAGIC                   0x0296  /* Summe von "NICKATARI" */
#define ATR_HEADER_SIZE             16

/* Sektorgrößen */
#define SECTOR_SIZE_SD              128     /* Single Density */
#define SECTOR_SIZE_DD              256     /* Double/Enhanced Density */
#define SECTOR_SIZE_QD              512     /* Quad Density (SpartaDOS X) */

/* Disk-Geometrien */
#define TRACKS_STANDARD             40
#define SECTORS_PER_TRACK_SD        18      /* 810: Single Density */
#define SECTORS_PER_TRACK_ED        26      /* 1050: Enhanced Density */
#define SECTORS_PER_TRACK_DD        18      /* XF551: Double Density */

/* Gesamtsektoren */
#define TOTAL_SECTORS_SD            720     /* 40 * 18 */
#define TOTAL_SECTORS_ED            1040    /* 40 * 26 */
#define TOTAL_SECTORS_DD            720     /* 40 * 18 (256 Bytes/Sektor) */

/* Nutzbare Sektoren (nach Abzug von Boot, VTOC, Directory) */
#define USABLE_SECTORS_SD           707
#define USABLE_SECTORS_ED           1010
#define USABLE_SECTORS_DD           707

/* Image-Größen in Bytes (ohne ATR-Header) */
#define IMAGE_SIZE_SD               (TOTAL_SECTORS_SD * SECTOR_SIZE_SD)       /* 92160 */
#define IMAGE_SIZE_ED               (TOTAL_SECTORS_ED * SECTOR_SIZE_SD)       /* 133120 */
#define IMAGE_SIZE_DD               (3 * SECTOR_SIZE_SD + 717 * SECTOR_SIZE_DD)  /* 183936 */

/* ATR Image-Größen (mit Header) */
#define ATR_SIZE_SD                 (ATR_HEADER_SIZE + IMAGE_SIZE_SD)         /* 92176 */
#define ATR_SIZE_ED                 (ATR_HEADER_SIZE + IMAGE_SIZE_ED)         /* 133136 */
#define ATR_SIZE_DD                 (ATR_HEADER_SIZE + IMAGE_SIZE_DD)         /* 183952 */

/* DOS 2.0/2.5 Dateisystem-Layout */
#define BOOT_SECTOR_START           1
#define BOOT_SECTOR_COUNT           3
#define VTOC_SECTOR                 360     /* $168 */
#define VTOC2_SECTOR                1024    /* DOS 2.5 Extended VTOC */
#define DIR_SECTOR_START            361     /* $169 */
#define DIR_SECTOR_COUNT            8
#define DIR_SECTOR_END              368     /* $170 */
#define DIR_ENTRIES_PER_SECTOR      8
#define DIR_ENTRY_SIZE              16
#define MAX_FILES                   64      /* 8 Sektoren * 8 Einträge */

/* Datei-Namenslängen */
#define FILENAME_LEN                8
#define EXTENSION_LEN               3
#define FULL_FILENAME_LEN           11      /* 8 + 3 */

/* Daten pro Sektor */
#define DATA_BYTES_SD               125     /* 128 - 3 Link-Bytes */
#define DATA_BYTES_DD               253     /* 256 - 3 Link-Bytes */

/* VTOC Bitmap */
#define VTOC_BITMAP_OFFSET          10      /* $0A */
#define VTOC_BITMAP_SIZE_SD         90      /* 720 Bits = 90 Bytes */
#define VTOC_BITMAP_SIZE_ED         118     /* Für Sektoren 0-943 */

/* Boot-Sektor Offsets */
#define BOOT_FLAGS_OFFSET           0x00
#define BOOT_SECTOR_COUNT_OFFSET    0x01
#define BOOT_LOAD_ADDR_OFFSET       0x02    /* 2 Bytes, little-endian */
#define BOOT_INIT_ADDR_OFFSET       0x04    /* 2 Bytes, little-endian */
#define BOOT_LAUNCH_OFFSET          0x06
#define BOOT_DOS_SECTOR_COUNT       0x09    /* 3 Bytes */
#define BOOT_BLDISP_OFFSET          0x11    /* Link-Byte Displacement */

/* BLDISP Werte */
#define BLDISP_SD                   0x7D    /* 125: Single Density */
#define BLDISP_DD                   0xFD    /* 253: Double Density */

/* Sektor-Link-Byte Layout (letzte 3 Bytes eines Datensektors) */
/* Byte 125 (SD) / 253 (DD): Bits 7-2 = File-Nummer, Bits 1-0 = Next-Sektor High */
/* Byte 126 (SD) / 254 (DD): Next-Sektor Low */
/* Byte 127 (SD) / 255 (DD): Bit 7 = Short-Sector-Flag, Bits 6-0 = Byte-Count */

/* Directory Entry Status-Flags */
#define DIR_FLAG_OPEN_OUTPUT        0x01    /* Datei zum Schreiben geöffnet */
#define DIR_FLAG_DOS2_CREATED       0x02    /* Von DOS 2.x erstellt */
#define DIR_FLAG_LOCKED             0x20    /* Schreibgeschützt */
#define DIR_FLAG_IN_USE             0x40    /* Eintrag belegt */
#define DIR_FLAG_DELETED            0x80    /* Gelöscht (Eintrag wiederverwendbar) */
#define DIR_FLAG_NEVER_USED         0x00    /* Noch nie benutzt (Ende der Suche) */

/* Normale Statuswerte */
#define DIR_STATUS_NORMAL           0x42    /* IN_USE | DOS2_CREATED */
#define DIR_STATUS_LOCKED           0x62    /* IN_USE | LOCKED | DOS2_CREATED */
#define DIR_STATUS_DELETED          0x80    /* DELETED */

/* SpartaDOS Konstanten */
#define SPARTA_SUPERBLOCK_SECTOR    1
#define SPARTA_SIGNATURE_20         0x20    /* SpartaDOS 2.x */
#define SPARTA_SIGNATURE_21         0x21    /* SpartaDOS 2.1+ */
#define SPARTA_ROOT_DIR_SECTOR_OFF  0x09    /* Offset im Superblock */
#define SPARTA_TOTAL_SECTORS_OFF    0x0B    /* Offset im Superblock */
#define SPARTA_FREE_SECTORS_OFF     0x0D    /* Offset im Superblock */
#define SPARTA_BITMAP_SECTORS_OFF   0x0F    /* Offset im Superblock */
#define SPARTA_FIRST_BITMAP_OFF     0x10    /* Offset im Superblock */
#define SPARTA_DIR_ENTRY_SIZE       23      /* Bytes pro Directory-Eintrag */
#define SPARTA_FILENAME_LEN         8
#define SPARTA_EXT_LEN              3

/* SpartaDOS Directory-Flags */
#define SPARTA_FLAG_LOCKED          0x01
#define SPARTA_FLAG_HIDDEN          0x02
#define SPARTA_FLAG_ARCHIVED        0x04
#define SPARTA_FLAG_IN_USE          0x08
#define SPARTA_FLAG_DELETED         0x10
#define SPARTA_FLAG_SUBDIR          0x20
#define SPARTA_FLAG_OPEN_OUTPUT     0x80

/* MyDOS Erweiterungen */
#define MYDOS_VTOC_EXTENDED_BYTE    0x63    /* Erweitertes VTOC-Byte für Sektor 720 */

/* ATR Disk-Flags */
#define ATR_FLAG_COPY_PROTECTED     0x10
#define ATR_FLAG_WRITE_PROTECTED    0x20

/* Fehler-Codes */
typedef enum {
    ATARI_OK = 0,
    ATARI_ERR_NULL_PTR,
    ATARI_ERR_FILE_OPEN,
    ATARI_ERR_FILE_READ,
    ATARI_ERR_FILE_WRITE,
    ATARI_ERR_INVALID_MAGIC,
    ATARI_ERR_INVALID_SIZE,
    ATARI_ERR_INVALID_SECTOR,
    ATARI_ERR_INVALID_FILENAME,
    ATARI_ERR_FILE_NOT_FOUND,
    ATARI_ERR_DIR_FULL,
    ATARI_ERR_DISK_FULL,
    ATARI_ERR_SECTOR_CHAIN,
    ATARI_ERR_VTOC_MISMATCH,
    ATARI_ERR_ALLOC_FAILED,
    ATARI_ERR_UNSUPPORTED_FORMAT,
    ATARI_ERR_CORRUPT_FS,
    ATARI_ERR_LOCKED_FILE,
    ATARI_ERR_ALREADY_EXISTS,
} atari_error_t;

/* Disk-Dichte / Format-Typ */
typedef enum {
    DENSITY_UNKNOWN = 0,
    DENSITY_SINGLE,             /* 810: 720 Sektoren, 128 Bytes */
    DENSITY_ENHANCED,           /* 1050: 1040 Sektoren, 128 Bytes */
    DENSITY_DOUBLE,             /* XF551: 720 Sektoren, 256 Bytes */
    DENSITY_QUAD,               /* SpartaDOS X: 512 Bytes/Sektor */
} atari_density_t;

/* Dateisystem-Typ */
typedef enum {
    FS_UNKNOWN = 0,
    FS_DOS_20,                  /* Atari DOS 2.0 */
    FS_DOS_25,                  /* Atari DOS 2.5 */
    FS_MYDOS,                   /* MyDOS */
    FS_SPARTADOS,               /* SpartaDOS */
} atari_fs_type_t;

/* Checker-Severity */
typedef enum {
    CHECK_INFO = 0,
    CHECK_WARNING,
    CHECK_ERROR,
    CHECK_FIXED,
} check_severity_t;

/* =============================================================================
 * Strukturen
 * ========================================================================== */

/* ATR Header (16 Bytes) */
typedef struct {
    uint16_t magic;             /* $0296 = Summe "NICKATARI" */
    uint16_t size_paragraphs;   /* Image-Größe / 16 (Low Word) */
    uint16_t sector_size;       /* 128, 256 oder 512 */
    uint16_t size_high;         /* Image-Größe / 16 (High Word, REV 3.00) */
    uint8_t  flags;             /* Disk-Flags */
    uint16_t first_bad_sector;  /* Erster defekter Sektor */
    uint8_t  spare[5];          /* Unbenutzt (Nullen) */
} atr_header_t;

/* Boot-Sektor Informationen */
typedef struct {
    uint8_t  flags;             /* Boot-Flags */
    uint8_t  boot_sector_count; /* Anzahl Boot-Sektoren */
    uint16_t load_address;      /* Ladeadresse */
    uint16_t init_address;      /* Init-Adresse */
    uint8_t  launch;            /* JMP Opcode */
    uint16_t dos_file_sectors;  /* Sektoren für DOS.SYS */
    uint8_t  bldisp;            /* Link-Byte Displacement ($7D oder $FD) */
    uint8_t  raw[384];          /* Rohdaten (3 Sektoren à 128 Bytes) */
} atari_boot_info_t;

/* VTOC (Volume Table of Contents) */
typedef struct {
    uint8_t  dos_code;          /* DOS Version (2 = DOS 2.0/2.5) */
    uint16_t total_sectors;     /* Initiale Gesamtsektoren */
    uint16_t free_sectors;      /* Aktuelle freie Sektoren */
    uint8_t  bitmap[128];       /* Sektor-Bitmap (max 128 Bytes für DD) */
    uint16_t bitmap_sector_count; /* Anzahl Bytes in Bitmap */

    /* DOS 2.5 Extended VTOC (Sektor 1024) */
    bool     has_vtoc2;
    uint16_t free_sectors_above_719;  /* Freie Sektoren > 719 */
    uint8_t  bitmap2[128];     /* Erweiterte Bitmap */

    /* Rohdaten */
    uint8_t  raw[256];          /* Roher VTOC-Sektor */
    uint8_t  raw2[256];         /* Roher VTOC2-Sektor (DOS 2.5) */
} atari_vtoc_t;

/* Directory-Eintrag (DOS 2.0/2.5/MyDOS) */
typedef struct {
    uint8_t  status;            /* Status-Flags */
    uint16_t sector_count;      /* Anzahl Sektoren der Datei */
    uint16_t first_sector;      /* Erster Daten-Sektor */
    char     filename[FILENAME_LEN + 1];  /* Dateiname (null-terminiert) */
    char     extension[EXTENSION_LEN + 1]; /* Erweiterung (null-terminiert) */
    uint8_t  entry_index;       /* Position im Directory (0-63) */

    /* Berechnete Felder */
    bool     is_valid;
    bool     is_deleted;
    bool     is_locked;
    bool     is_dos2_compat;
    bool     is_open;
    uint32_t file_size;         /* Berechnete Dateigröße in Bytes */
} atari_dir_entry_t;

/* SpartaDOS Directory-Eintrag */
typedef struct {
    uint8_t  status;
    uint16_t first_sector;      /* Erster Sektor der Sector-Map */
    uint32_t file_size;         /* Dateigröße in Bytes */
    char     filename[SPARTA_FILENAME_LEN + 1];
    char     extension[SPARTA_EXT_LEN + 1];
    uint8_t  date_day;
    uint8_t  date_month;
    uint8_t  date_year;
    uint8_t  time_hour;
    uint8_t  time_minute;
    uint8_t  time_second;
    bool     is_subdir;
    bool     is_locked;
    bool     is_hidden;
    bool     is_deleted;
    uint8_t  entry_index;
} sparta_dir_entry_t;

/* Sektor-Link Informationen (aus den letzten 3 Bytes eines Datensektors) */
typedef struct {
    uint8_t  file_number;       /* Datei-Nummer (0-63), 6 Bits */
    uint16_t next_sector;       /* Nächster Sektor (0-1023), 10 Bits */
    uint8_t  byte_count;        /* Genutzte Bytes im Sektor (0-125/253) */
    bool     is_short_sector;   /* Letzter Sektor (EOF) */
    bool     is_last;           /* next_sector == 0 */
} sector_link_t;

/* Checker-Meldung */
typedef struct {
    check_severity_t severity;
    char             message[256];
    uint16_t         sector;        /* Betroffener Sektor (0 = N/A) */
    uint8_t          file_index;    /* Betroffene Datei (0xFF = N/A) */
} check_issue_t;

/* Checker-Ergebnis */
typedef struct {
    check_issue_t   *issues;
    uint32_t         issue_count;
    uint32_t         issue_capacity;
    uint32_t         errors;
    uint32_t         warnings;
    uint32_t         fixed;
    bool             is_valid;
} check_result_t;

/* Haupt-Disk-Image Struktur */
typedef struct {
    /* Container */
    atr_header_t     header;
    char             filepath[1024];

    /* Image-Daten */
    uint8_t         *data;          /* Rohe Sektordaten (ohne ATR-Header) */
    uint32_t         data_size;     /* Größe in Bytes */

    /* Disk-Eigenschaften */
    atari_density_t  density;
    atari_fs_type_t  fs_type;
    uint16_t         sector_size;   /* Effektive Sektorgröße */
    uint16_t         total_sectors; /* Gesamtanzahl Sektoren */
    uint16_t         data_bytes_per_sector; /* Nutzdaten pro Sektor (125/253) */

    /* Dateisystem */
    atari_boot_info_t boot;
    atari_vtoc_t      vtoc;
    atari_dir_entry_t directory[MAX_FILES];
    uint8_t           dir_entry_count; /* Tatsächliche Einträge */

    /* SpartaDOS-spezifisch */
    struct {
        uint8_t      version;
        uint16_t     root_dir_sector;
        uint16_t     total_sectors;
        uint16_t     free_sectors;
        uint8_t      bitmap_sector_count;
        uint16_t     first_bitmap_sector;
        uint16_t     first_data_sector;
        char         volume_name[8 + 1];
        uint8_t      volume_seq;
        uint8_t      volume_random;
    } sparta;

    /* Status */
    bool             is_loaded;
    bool             is_modified;
} atari_disk_t;


/* =============================================================================
 * API: ATR Container
 * ========================================================================== */

/**
 * ATR-Image aus Datei laden
 */
atari_error_t ados_atr_load(atari_disk_t *disk, const char *filepath);

/**
 * ATR-Image in Datei speichern
 */
atari_error_t ados_atr_save(const atari_disk_t *disk, const char *filepath);

/**
 * Neues leeres ATR-Image erstellen
 */
atari_error_t ados_atr_create(atari_disk_t *disk, atari_density_t density,
                         atari_fs_type_t fs_type);

/**
 * Disk-Image Ressourcen freigeben
 */
void ados_atr_free(atari_disk_t *disk);

/**
 * Einzelnen Sektor lesen (1-basiert)
 */
atari_error_t ados_atr_read_sector(const atari_disk_t *disk, uint16_t sector,
                              uint8_t *buffer, uint16_t *bytes_read);

/**
 * Einzelnen Sektor schreiben (1-basiert)
 */
atari_error_t ados_atr_write_sector(atari_disk_t *disk, uint16_t sector,
                               const uint8_t *buffer, uint16_t size);


/* =============================================================================
 * API: Dateisystem-Erkennung
 * ========================================================================== */

/**
 * Dichte und Dateisystem-Typ erkennen
 */
atari_error_t ados_detect_format(atari_disk_t *disk);

/**
 * Density als String
 */
const char *ados_density_str(atari_density_t density);

/**
 * Dateisystem-Typ als String
 */
const char *ados_fs_type_str(atari_fs_type_t fs_type);


/* =============================================================================
 * API: DOS 2.0/2.5/MyDOS Dateisystem
 * ========================================================================== */

/**
 * VTOC lesen und parsen
 */
atari_error_t dos2_read_vtoc(atari_disk_t *disk);

/**
 * VTOC auf Disk schreiben
 */
atari_error_t dos2_write_vtoc(atari_disk_t *disk);

/**
 * Directory lesen und parsen
 */
atari_error_t dos2_read_directory(atari_disk_t *disk);

/**
 * Directory auf Disk schreiben
 */
atari_error_t dos2_write_directory(atari_disk_t *disk);

/**
 * Prüfen ob Sektor frei ist
 */
bool dos2_is_sector_free(const atari_disk_t *disk, uint16_t sector);

/**
 * Sektor in VTOC als belegt markieren
 */
atari_error_t dos2_alloc_sector(atari_disk_t *disk, uint16_t sector);

/**
 * Sektor in VTOC als frei markieren
 */
atari_error_t dos2_free_sector(atari_disk_t *disk, uint16_t sector);

/**
 * Nächsten freien Sektor finden
 */
uint16_t dos2_find_free_sector(const atari_disk_t *disk, uint16_t start);

/**
 * Sektor-Link Bytes eines Datensektors parsen
 */
sector_link_t dos2_parse_sector_link(const uint8_t *sector_data,
                                     uint16_t sector_size);

/**
 * Sektor-Link Bytes in Datensektor schreiben
 */
void dos2_write_sector_link(uint8_t *sector_data, uint16_t sector_size,
                            const sector_link_t *link);

/**
 * Datei nach Name suchen
 */
atari_error_t dos2_find_file(const atari_disk_t *disk, const char *filename,
                             atari_dir_entry_t *entry);

/**
 * Datei aus Image extrahieren
 */
atari_error_t dos2_extract_file(const atari_disk_t *disk,
                                const atari_dir_entry_t *entry,
                                uint8_t **data, uint32_t *size);

/**
 * Datei in Image schreiben
 */
atari_error_t dos2_write_file(atari_disk_t *disk, const char *filename,
                              const uint8_t *data, uint32_t size);

/**
 * Datei löschen
 */
atari_error_t dos2_delete_file(atari_disk_t *disk, const char *filename);

/**
 * Datei umbenennen
 */
atari_error_t dos2_rename_file(atari_disk_t *disk, const char *old_name,
                               const char *new_name);

/**
 * Datei sperren/entsperren
 */
atari_error_t dos2_lock_file(atari_disk_t *disk, const char *filename,
                             bool locked);

/**
 * Freien Speicherplatz berechnen (in Bytes)
 */
uint32_t dos2_free_space(const atari_disk_t *disk);

/**
 * Dateinamen in 8.3 Format konvertieren
 */
atari_error_t dos2_parse_filename(const char *input,
                                  char *name, char *ext);

/**
 * 8.3 Dateinamen als String formatieren
 */
void dos2_format_filename(const atari_dir_entry_t *entry,
                          char *output, size_t output_size);

/**
 * Boot-Sektoren lesen und parsen
 */
atari_error_t dos2_read_boot(atari_disk_t *disk);

/**
 * Neues DOS 2.0/2.5 Dateisystem formatieren
 */
atari_error_t dos2_format(atari_disk_t *disk, atari_density_t density);

/**
 * Neues MyDOS Dateisystem formatieren
 */
atari_error_t mydos_format(atari_disk_t *disk, atari_density_t density);


/* =============================================================================
 * API: SpartaDOS Dateisystem
 * ========================================================================== */

/**
 * SpartaDOS Superblock lesen
 */
atari_error_t sparta_read_superblock(atari_disk_t *disk);

/**
 * SpartaDOS Root-Directory lesen
 */
atari_error_t sparta_read_directory(atari_disk_t *disk, uint16_t dir_sector,
                                    sparta_dir_entry_t *entries,
                                    uint32_t *entry_count, uint32_t max_entries);

/**
 * SpartaDOS Datei extrahieren
 */
atari_error_t sparta_extract_file(const atari_disk_t *disk,
                                  const sparta_dir_entry_t *entry,
                                  uint8_t **data, uint32_t *size);

/**
 * SpartaDOS Sector-Map lesen (verkettete Sektorliste)
 */
atari_error_t sparta_read_sector_map(const atari_disk_t *disk,
                                     uint16_t map_sector,
                                     uint16_t **sectors, uint32_t *count);

/**
 * SpartaDOS Dateisystem erkennen
 */
bool sparta_detect(const atari_disk_t *disk);

/**
 * SpartaDOS Freien Speicher berechnen
 */
uint32_t sparta_free_space(const atari_disk_t *disk);


/* =============================================================================
 * API: Filesystem Checker
 * ========================================================================== */

/**
 * Checker-Ergebnis initialisieren
 */
check_result_t *check_create(void);

/**
 * Checker-Ergebnis freigeben
 */
void check_free(check_result_t *result);

/**
 * Vollständige Dateisystem-Prüfung durchführen
 */
atari_error_t check_filesystem(atari_disk_t *disk, check_result_t *result,
                               bool fix);

/**
 * VTOC-Konsistenz prüfen
 */
atari_error_t check_vtoc(atari_disk_t *disk, check_result_t *result,
                         bool fix);

/**
 * Directory-Konsistenz prüfen
 */
atari_error_t check_directory(atari_disk_t *disk, check_result_t *result,
                              bool fix);

/**
 * Sektor-Ketten aller Dateien prüfen
 */
atari_error_t check_sector_chains(atari_disk_t *disk, check_result_t *result,
                                  bool fix);

/**
 * Cross-linked Sektoren erkennen
 */
atari_error_t check_cross_links(atari_disk_t *disk, check_result_t *result);

/**
 * Verlorene Sektoren finden (belegt in VTOC, aber keiner Datei zugeordnet)
 */
atari_error_t check_lost_sectors(atari_disk_t *disk, check_result_t *result,
                                 bool fix);

/**
 * Checker-Ergebnis als Text ausgeben
 */
void check_print_report(const check_result_t *result, FILE *out);


/* =============================================================================
 * API: Utilities
 * ========================================================================== */

/**
 * Disk-Info ausgeben
 */
void ados_print_info(const atari_disk_t *disk, FILE *out);

/**
 * Directory-Listing ausgeben (Atari DOS Format)
 */
void ados_print_directory(const atari_disk_t *disk, FILE *out);

/**
 * VTOC-Bitmap visualisieren
 */
void ados_print_vtoc_map(const atari_disk_t *disk, FILE *out);

/**
 * Hex-Dump eines Sektors
 */
void ados_hex_dump_sector(const atari_disk_t *disk, uint16_t sector,
                           FILE *out);

/**
 * Fehlermeldung als String
 */
const char *ados_error_str(atari_error_t err);


#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* ATARI_DOS_H */
