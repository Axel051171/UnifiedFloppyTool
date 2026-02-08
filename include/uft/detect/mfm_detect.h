/**
 * =============================================================================
 * MFM Disk Format Detection Module
 * =============================================================================
 *
 * Mehrstufige Erkennung von MFM-kodierten Diskettenformaten:
 *
 *   Stufe 1: Physikalische Parameter (Burst-Query / Track-Analyse)
 *            → Sektorgröße, Sektoren/Spur, Kodierung (GCR/MFM/FM)
 *
 *   Stufe 2: Boot-Sektor-Analyse
 *            → FAT BPB, Amiga "DOS\0", Atari ST, BIOS-Signaturen
 *
 *   Stufe 3: Dateisystem-Heuristik
 *            → CP/M Directory-Pattern, FAT-Cluster-Ketten, Prüfsummen
 *
 * Unterstützte Formate:
 *   - MS-DOS / PC-DOS (FAT12: 160K..1.44M, FAT16)
 *   - Atari ST / TOS   (FAT12-Variante, 360K..1.44M)
 *   - Amiga OFS / FFS  (880K DD, 1.76M HD + Varianten)
 *   - CP/M 2.2 / 3.0   (Hunderte Varianten, Heuristik-basiert)
 *   - MSX-DOS           (FAT12-basiert)
 *   - Commodore 1581    (CP/M oder CBM-DOS)
 *   - Amstrad CPC/PCW   (CP/M 2.2 / CP/M Plus)
 *   - Sam Coupé          (SAMDOS / MasterDOS)
 *   - Spectrum +3        (CP/M Plus Variante)
 *   - Thomson MO/TO      (Proprietär)
 *
 * Integration: UFI Preservation Platform (STM32H723 + CM5)
 *
 * (C) 2026
 * =============================================================================
 */

#ifndef MFM_DETECT_H
#define MFM_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/* =============================================================================
 * Konstanten
 * ========================================================================== */

/** Maximale Sektorgröße (4096 Bytes für spezielle Formate) */
#define MFM_MAX_SECTOR_SIZE     4096

/** Standard-Sektorgrößen */
#define MFM_SECSIZE_128          128
#define MFM_SECSIZE_256          256
#define MFM_SECSIZE_512          512
#define MFM_SECSIZE_1024        1024

/** Amiga-Konstanten */
#define AMIGA_DD_BLOCKS         1760    /* 80 Zylinder × 2 Seiten × 11 Sektoren */
#define AMIGA_HD_BLOCKS         3520    /* 80 × 2 × 22 */
#define AMIGA_ROOTBLOCK_DD      880     /* Mitte der DD-Disk */
#define AMIGA_ROOTBLOCK_HD      1760    /* Mitte der HD-Disk */
#define AMIGA_BOOTBLOCK_SIZE    1024    /* 2 Sektoren */

/** FAT BPB Offsets im Boot-Sektor */
#define BPB_JMP                 0x00    /* Jump-Befehl (EB xx 90 oder E9 xx xx) */
#define BPB_OEM                 0x03    /* OEM-String (8 Bytes) */
#define BPB_BYTES_PER_SECTOR    0x0B    /* Bytes pro Sektor (LE16) */
#define BPB_SECTORS_PER_CLUSTER 0x0D    /* Sektoren pro Cluster */
#define BPB_RESERVED_SECTORS    0x0E    /* Reservierte Sektoren (LE16) */
#define BPB_NUM_FATS            0x10    /* Anzahl FATs */
#define BPB_ROOT_ENTRIES        0x11    /* Root-Directory-Einträge (LE16) */
#define BPB_TOTAL_SECTORS_16    0x13    /* Gesamtsektoren 16-Bit (LE16) */
#define BPB_MEDIA_DESCRIPTOR    0x15    /* Media-Descriptor-Byte */
#define BPB_SECTORS_PER_FAT     0x16    /* Sektoren pro FAT (LE16) */
#define BPB_SECTORS_PER_TRACK   0x18    /* Sektoren pro Spur (LE16) */
#define BPB_NUM_HEADS           0x1A    /* Anzahl Köpfe (LE16) */
#define BPB_HIDDEN_SECTORS      0x1C    /* Versteckte Sektoren (LE32) */
#define BPB_TOTAL_SECTORS_32    0x20    /* Gesamtsektoren 32-Bit (LE32) */

/** Extended BPB (FAT12/16) */
#define EBPB_DRIVE_NUMBER       0x24    /* BIOS-Laufwerksnummer */
#define EBPB_BOOT_SIGNATURE     0x26    /* Extended Boot Signature (0x29) */
#define EBPB_VOLUME_SERIAL      0x27    /* Volume Serial Number (LE32) */
#define EBPB_VOLUME_LABEL       0x2B    /* Volume Label (11 Bytes) */
#define EBPB_FS_TYPE            0x36    /* FS-Typ String (8 Bytes) */

/** Boot-Sektor-Signatur */
#define BOOT_SIGNATURE_OFFSET   0x1FE
#define BOOT_SIGNATURE          0xAA55

/** CP/M Konstanten */
#define CPM_DIR_ENTRY_SIZE      32
#define CPM_DELETED_MARKER      0xE5
#define CPM_MAX_USER_NUM        31      /* 0-15 Standard, 0-31 erweitert */
#define CPM_FILENAME_LEN        8
#define CPM_EXTENSION_LEN       3

/** Amiga Bootblock DiskType Bytes */
#define AMIGA_DISK_OFS          0x00    /* Flags Byte: OFS */
#define AMIGA_DISK_FFS          0x01    /* Flags Byte: FFS */
#define AMIGA_DISK_INTL         0x02    /* International Mode */
#define AMIGA_DISK_DIRC         0x04    /* Directory Cache */

/** Konfidenz-Schwellwerte */
#define CONFIDENCE_NONE         0
#define CONFIDENCE_LOW          25
#define CONFIDENCE_MEDIUM       50
#define CONFIDENCE_HIGH         75
#define CONFIDENCE_CERTAIN      100


/* =============================================================================
 * Enumerationen
 * ========================================================================== */

/** Physikalische Aufzeichnungsmethode */
typedef enum {
    ENCODING_UNKNOWN = 0,
    ENCODING_FM,            /* Single Density (FM) */
    ENCODING_MFM,           /* Double/High Density (MFM) */
    ENCODING_GCR,           /* Commodore/Apple GCR */
    ENCODING_M2FM,          /* Modified MFM (Intel) */
} mfm_encoding_t;

/** Erkanntes Dateisystem / Format */
typedef enum {
    MFM_FS_UNKNOWN = 0,

    /* FAT-Familie */
    MFM_FS_FAT12_DOS,       /* MS-DOS / PC-DOS FAT12 */
    MFM_FS_FAT12_ATARI_ST,  /* Atari ST TOS (FAT12 Variante) */
    MFM_FS_FAT12_MSX,       /* MSX-DOS FAT12 */
    MFM_FS_FAT16,           /* FAT16 (unwahrscheinlich auf Floppy) */

    /* Amiga */
    MFM_FS_AMIGA_OFS,       /* Amiga Old File System */
    MFM_FS_AMIGA_FFS,       /* Amiga Fast File System */
    MFM_FS_AMIGA_OFS_INTL,  /* OFS + International Mode */
    MFM_FS_AMIGA_FFS_INTL,  /* FFS + International Mode */
    MFM_FS_AMIGA_OFS_DIRC,  /* OFS + Directory Cache */
    MFM_FS_AMIGA_FFS_DIRC,  /* FFS + Directory Cache */
    MFM_FS_AMIGA_PFS,       /* Professional File System */

    /* CP/M Familie */
    MFM_FS_CPM_22,          /* CP/M 2.2 (generisch) */
    MFM_FS_CPM_30,          /* CP/M 3.0 / CP/M Plus */
    MFM_FS_CPM_AMSTRAD,     /* Amstrad CPC/PCW CP/M */
    MFM_FS_CPM_SPECTRUM,    /* Spectrum +3 (CP/M Plus Variante) */
    MFM_FS_CPM_KAYPRO,      /* Kaypro CP/M */
    MFM_FS_CPM_OSBORNE,     /* Osborne CP/M */
    MFM_FS_CPM_C128,        /* Commodore 128 CP/M */
    MFM_FS_CPM_GENERIC,     /* CP/M (nicht näher bestimmt) */

    /* Commodore MFM */
    MFM_FS_CBM_1581,        /* Commodore 1581 DOS */

    /* Sonstige */
    MFM_FS_SAM_SAMDOS,      /* Sam Coupé SAMDOS */
    MFM_FS_SAM_MASTERDOS,   /* Sam Coupé MasterDOS */
    MFM_FS_THOMSON,         /* Thomson MO/TO */
    MFM_FS_BBC_DFS,         /* BBC Micro DFS */
    MFM_FS_BBC_ADFS,        /* BBC Micro ADFS */
    MFM_FS_SHARP_MZ,        /* Sharp MZ-Serie */
    MFM_FS_FLEX,            /* FLEX OS (6809) */
    MFM_FS_OS9,             /* OS-9/6809 */
    MFM_FS_UNIFLEX,         /* UniFLEX */
    MFM_FS_RT11,            /* DEC RT-11 */
    MFM_FS_P2DOS,           /* P2DOS (Z80DOS) */

    MFM_FS_MAX_TYPES
} mfm_fs_type_t;

/** Disk-Geometrie-Typ (physikalisch) */
typedef enum {
    MFM_GEOM_UNKNOWN = 0,

    /* 8 Zoll */
    MFM_GEOM_8_SSSD,        /* 8" SS/SD: 77×26×128 = 250K */
    MFM_GEOM_8_SSDD,        /* 8" SS/DD: 77×26×256 = 500K */
    MFM_GEOM_8_DSDD,        /* 8" DS/DD: 77×26×256 = 1M */

    /* 5.25 Zoll */
    MFM_GEOM_525_SSDD_40,   /* 5.25" SS/DD: 40×9×512 = 180K */
    MFM_GEOM_525_DSDD_40,   /* 5.25" DS/DD: 40×9×512 = 360K */
    MFM_GEOM_525_DSQD_80,   /* 5.25" DS/QD: 80×9×512 = 720K */
    MFM_GEOM_525_DSHD_80,   /* 5.25" DS/HD: 80×15×512 = 1.2M */

    /* 3.5 Zoll */
    MFM_GEOM_35_SSDD_80,    /* 3.5" SS/DD: 80×9×512 = 360K */
    MFM_GEOM_35_DSDD_80,    /* 3.5" DS/DD: 80×9×512 = 720K */
    MFM_GEOM_35_DSHD_80,    /* 3.5" DS/HD: 80×18×512 = 1.44M */
    MFM_GEOM_35_DSED_80,    /* 3.5" DS/ED: 80×36×512 = 2.88M */

    /* Amiga-spezifisch */
    MFM_GEOM_AMIGA_DD,      /* 80×2×11×512 = 880K */
    MFM_GEOM_AMIGA_HD,      /* 80×2×22×512 = 1.76M */

    /* Spezialformate */
    MFM_GEOM_CBM_1581,      /* Commodore 1581: 80×2×10×512 = 800K */
    MFM_GEOM_ATARI_ST_DD,   /* Atari ST DD: 80×2×9×512 = 720K */
    MFM_GEOM_ATARI_ST_HD,   /* Atari ST HD: 80×2×18×512 = 1.44M */

    MFM_GEOM_MAX_TYPES
} mfm_geometry_t;

/** Fehlercodes */
typedef enum {
    MFM_OK = 0,
    MFM_ERR_NULL_PARAM,
    MFM_ERR_NO_DATA,
    MFM_ERR_INVALID_SECTOR,
    MFM_ERR_READ_FAILED,
    MFM_ERR_NOT_MFM,
    MFM_ERR_UNKNOWN_FORMAT,
    MFM_ERR_ALLOC_FAILED,
    MFM_ERR_INVALID_BPB,
    MFM_ERR_CORRUPT_DIR,
} mfm_error_t;


/* =============================================================================
 * Strukturen
 * ========================================================================== */

/**
 * Burst-Query Ergebnis (von 1571/1581/FD-Controller)
 *
 * Die Daten kommen über das CBM Burst Transfer Protokoll:
 *   Byte 0: Status     (< 0x02 = GCR, >= 0x02 = MFM)
 *   Byte 1: Status 2   (Bits 1-3 = Fehlerbits)
 *   Byte 2: Sektoren/Spur
 *   Byte 3: Logische Spur
 *   Byte 4: Minimale Sektornummer
 *   Byte 5: Maximale Sektornummer
 *   Byte 6: CP/M Hard-Interleave
 */
typedef struct {
    uint8_t  status;            /**< Status-Byte 1 */
    uint8_t  status2;           /**< Status-Byte 2 */
    uint8_t  sectors_per_track; /**< Physische Sektoren pro Spur */
    uint8_t  logical_track;     /**< Gefundene logische Spur */
    uint8_t  min_sector;        /**< Niedrigste Sektornummer */
    uint8_t  max_sector;        /**< Höchste Sektornummer */
    uint8_t  cpm_interleave;    /**< CP/M Hard-Interleave-Wert */

    bool     is_mfm;            /**< true wenn MFM erkannt */
    bool     has_errors;        /**< true wenn Fehlerbits gesetzt */
} burst_query_result_t;

/**
 * Physikalische Disk-Parameter (Stufe 1)
 */
typedef struct {
    mfm_encoding_t  encoding;       /**< Aufzeichnungsmethode */
    mfm_geometry_t  geometry;       /**< Erkannte Geometrie */

    uint16_t  sector_size;          /**< Bytes pro Sektor */
    uint8_t   sectors_per_track;    /**< Sektoren pro Spur */
    uint8_t   heads;                /**< Anzahl Köpfe (1 oder 2) */
    uint16_t  cylinders;            /**< Anzahl Zylinder */
    uint32_t  total_sectors;        /**< Gesamtanzahl Sektoren */
    uint32_t  disk_size;            /**< Gesamtgröße in Bytes */

    uint8_t   min_sector_id;        /**< Niedrigste Sektor-ID (0 oder 1) */
    uint8_t   max_sector_id;        /**< Höchste Sektor-ID */
    uint8_t   interleave;           /**< Erkannter Interleave */

    char      description[80];      /**< Lesbare Beschreibung */
} disk_physical_t;

/**
 * FAT BPB (BIOS Parameter Block) – geparst aus Boot-Sektor
 */
typedef struct {
    uint8_t   jmp[3];              /**< Jump-Befehl */
    char      oem_name[9];         /**< OEM-String (null-terminiert) */
    uint16_t  bytes_per_sector;    /**< Bytes pro Sektor */
    uint8_t   sectors_per_cluster; /**< Sektoren pro Cluster */
    uint16_t  reserved_sectors;    /**< Reservierte Sektoren */
    uint8_t   num_fats;            /**< Anzahl FATs */
    uint16_t  root_entries;        /**< Root-Directory-Einträge */
    uint16_t  total_sectors_16;    /**< Gesamtsektoren (16-Bit) */
    uint8_t   media_descriptor;    /**< Media-Descriptor */
    uint16_t  sectors_per_fat;     /**< Sektoren pro FAT */
    uint16_t  sectors_per_track;   /**< Sektoren pro Spur */
    uint16_t  num_heads;           /**< Köpfe */
    uint32_t  hidden_sectors;      /**< Versteckte Sektoren */
    uint32_t  total_sectors_32;    /**< Gesamtsektoren (32-Bit) */

    /* Extended BPB */
    uint8_t   drive_number;
    uint8_t   boot_signature;      /**< 0x29 = EBPB vorhanden */
    uint32_t  volume_serial;
    char      volume_label[12];    /**< null-terminiert */
    char      fs_type[9];          /**< "FAT12   " etc. */

    bool      has_valid_bpb;       /**< BPB-Validierung bestanden */
    bool      has_ebpb;            /**< Extended BPB vorhanden */
    bool      has_boot_sig;        /**< 0xAA55 Signatur vorhanden */
} fat_bpb_t;

/**
 * Amiga Bootblock-Info
 */
typedef struct {
    char      disk_type[4];        /**< "DOS\0" + Flags */
    uint8_t   flags;               /**< Bit 0: FFS, Bit 1: INTL, Bit 2: DIRC */
    uint32_t  checksum;
    uint32_t  rootblock;           /**< Rootblock-Zeiger (normalerweise 880) */
    bool      checksum_valid;
    bool      is_bootable;         /**< Boot-Code vorhanden */

    /* Rootblock-Info (wenn gelesen) */
    bool      rootblock_read;
    char      volume_name[32];
    uint32_t  creation_days;
    uint32_t  creation_mins;
    uint32_t  creation_ticks;
    uint16_t  hash_table_size;
    bool      bitmap_valid;
} amiga_info_t;

/**
 * CP/M Disk Parameter Block (DPB) – rekonstruiert aus Analyse
 */
typedef struct {
    uint16_t  spt;               /**< 128-Byte Records pro Spur */
    uint8_t   bsh;               /**< Block Shift (3=1K, 4=2K, 5=4K) */
    uint8_t   blm;               /**< Block Mask */
    uint8_t   exm;               /**< Extent Mask */
    uint16_t  dsm;               /**< Disk Storage Maximum (höchster Block) */
    uint16_t  drm;               /**< Directory Maximum (höchster Eintrag) */
    uint8_t   al0;               /**< Alloc Bitmap Byte 0 */
    uint8_t   al1;               /**< Alloc Bitmap Byte 1 */
    uint16_t  cks;               /**< Checksum Vector Size */
    uint16_t  off;               /**< Reserved Tracks (Offset) */

    uint16_t  block_size;        /**< Berechnete Blockgröße */
    uint16_t  dir_entries;       /**< Berechnete Directory-Einträge */
    uint16_t  dir_blocks;        /**< Anzahl Directory-Blöcke */
    uint32_t  data_capacity;     /**< Datenkapazität in Bytes */

    bool      is_valid;          /**< DPB konsistent */
} mfm_cpm_dpb_t;

/**
 * CP/M Directory-Eintrag (32 Bytes)
 */
typedef struct {
    uint8_t   user_number;       /**< User 0-31 (0xE5 = gelöscht) */
    char      filename[9];       /**< Dateiname (null-terminiert) */
    char      extension[4];      /**< Erweiterung (null-terminiert) */
    bool      read_only;         /**< T1' gesetzt */
    bool      system_file;       /**< T2' gesetzt */
    bool      archived;          /**< T3' gesetzt */
    uint8_t   extent_lo;         /**< Extent Counter Low (EX) */
    uint8_t   s1;                /**< Reserved */
    uint8_t   s2;                /**< Extent Counter High */
    uint8_t   record_count;      /**< Records in diesem Extent (RC) */
    uint8_t   allocation[16];    /**< Block-Nummern */
    bool      is_deleted;        /**< User == 0xE5 */
    bool      is_valid;          /**< Gültiger Eintrag */
} mfm_cpm_dir_entry_t;

/**
 * CP/M Analyse-Ergebnis
 */
typedef struct {
    mfm_cpm_dpb_t      dpb;                /**< Rekonstruierter DPB */
    uint16_t       num_entries;         /**< Anzahl Directory-Einträge */
    uint16_t       num_files;           /**< Anzahl verschiedene Dateien */
    uint16_t       num_deleted;         /**< Gelöschte Einträge */
    uint16_t       boot_tracks;         /**< Erkannte System-Spuren */
    uint16_t       block_size;          /**< Erkannte Blockgröße */
    bool           uses_16bit_alloc;    /**< 16-Bit Block-Pointer */
    uint8_t        max_user;            /**< Höchste User-Nummer */
    uint8_t        confidence;          /**< Konfidenz 0-100 */

    char           machine_hint[40];    /**< Vermutetes System */
} mfm_cpm_analysis_t;

/**
 * Format-Erkennung: Einzelner Kandidat
 */
typedef struct {
    mfm_fs_type_t  fs_type;            /**< Erkanntes Dateisystem */
    uint8_t        confidence;          /**< Konfidenz 0-100 */
    char           description[120];    /**< Lesbare Beschreibung */
    char           system_name[40];     /**< Quellsystem */

    /* Format-spezifische Details */
    union {
        fat_bpb_t       fat;
        amiga_info_t    amiga;
        mfm_cpm_analysis_t  cpm;
    } detail;
} format_candidate_t;

/** Maximale Kandidaten pro Erkennung */
#define MFM_MAX_CANDIDATES  8

/**
 * Gesamtergebnis der Format-Erkennung
 */
typedef struct {
    /* Physikalische Ebene */
    disk_physical_t      physical;
    burst_query_result_t burst;
    bool                 has_burst_data;

    /* Boot-Sektor-Rohdaten */
    uint8_t              boot_sector[MFM_MAX_SECTOR_SIZE];
    uint16_t             boot_sector_size;
    bool                 has_boot_sector;

    /* Erkannte Kandidaten (sortiert nach Konfidenz) */
    format_candidate_t   candidates[MFM_MAX_CANDIDATES];
    uint8_t              num_candidates;

    /* Bestes Ergebnis (Shortcut) */
    mfm_fs_type_t        best_fs;
    uint8_t              best_confidence;
    const char          *best_description;

    /* Rohsektor-Lesefunktion (Callback) */
    mfm_error_t (*read_sector)(void *ctx, uint16_t cyl, uint8_t head,
                                uint8_t sector, uint8_t *buf, uint16_t *bytes_read);
    void                *read_ctx;
} mfm_detect_result_t;


/* =============================================================================
 * Callback-Typ für Sektor-Lesen
 * ========================================================================== */

/**
 * Sektor-Lese-Callback
 *
 * @param ctx         Benutzer-Kontext (z.B. Laufwerk-Handle)
 * @param cylinder    Zylinder (0-basiert)
 * @param head        Kopf (0 oder 1)
 * @param sector      Sektor-ID
 * @param buf         Zielpuffer (mindestens MFM_MAX_SECTOR_SIZE)
 * @param bytes_read  Gelesene Bytes
 * @return MFM_OK bei Erfolg
 */
typedef mfm_error_t (*mfm_read_sector_fn)(void *ctx, uint16_t cylinder,
                                           uint8_t head, uint8_t sector,
                                           uint8_t *buf, uint16_t *bytes_read);


/* =============================================================================
 * API: Haupt-Erkennung
 * ========================================================================== */

/**
 * Ergebnis-Struktur initialisieren
 */
mfm_detect_result_t *mfm_detect_create(void);

/**
 * Ergebnis freigeben
 */
void mfm_detect_free(mfm_detect_result_t *result);

/**
 * Sektor-Lese-Callback setzen
 *
 * Wird für Stufe 2 und 3 benötigt (Boot-Sektor lesen, Directory analysieren).
 */
void mfm_detect_set_reader(mfm_detect_result_t *result,
                            mfm_read_sector_fn reader, void *ctx);

/**
 * Stufe 1: Physikalische Parameter aus Burst-Query setzen
 *
 * @param result    Ergebnis-Struktur
 * @param data      7 Bytes Burst-Query-Antwort
 * @param len       Länge (mindestens 1, idealerweise 7)
 * @return MFM_OK bei Erfolg
 */
mfm_error_t mfm_detect_from_burst(mfm_detect_result_t *result,
                                    const uint8_t *data, uint8_t len);

/**
 * Stufe 1: Physikalische Parameter manuell setzen
 *
 * Für den Fall, dass kein Burst-Query verfügbar ist (z.B. bei
 * direkter Flux-Analyse oder Image-Dateien).
 */
mfm_error_t mfm_detect_set_physical(mfm_detect_result_t *result,
                                      uint16_t sector_size,
                                      uint8_t  sectors_per_track,
                                      uint8_t  heads,
                                      uint16_t cylinders,
                                      uint8_t  min_sector_id);

/**
 * Stufe 2: Boot-Sektor analysieren
 *
 * Liest den Boot-Sektor (Track 0, Head 0, Sector min_id) und
 * analysiert FAT BPB, Amiga-Signaturen, etc.
 *
 * @param result    Ergebnis-Struktur (physical muss gesetzt sein)
 * @return MFM_OK bei Erfolg
 */
mfm_error_t mfm_detect_analyze_boot(mfm_detect_result_t *result);

/**
 * Stufe 2: Boot-Sektor aus Buffer analysieren
 *
 * Wie mfm_detect_analyze_boot(), aber mit bereits gelesenem Sektor.
 */
mfm_error_t mfm_detect_analyze_boot_data(mfm_detect_result_t *result,
                                           const uint8_t *boot_data,
                                           uint16_t size);

/**
 * Stufe 3: Dateisystem-Heuristik (CP/M, etc.)
 *
 * Liest zusätzliche Sektoren und analysiert Directory-Strukturen.
 * Besonders wichtig für CP/M-Erkennung, da CP/M keinen
 * standardisierten Boot-Sektor hat.
 *
 * @param result    Ergebnis-Struktur
 * @return MFM_OK bei Erfolg
 */
mfm_error_t mfm_detect_analyze_filesystem(mfm_detect_result_t *result);

/**
 * Vollständige Erkennung (alle 3 Stufen)
 *
 * Führt nacheinander Stufe 1 (falls Burst-Daten vorhanden),
 * Stufe 2 und Stufe 3 aus.
 */
mfm_error_t mfm_detect_full(mfm_detect_result_t *result);


/* =============================================================================
 * API: Einzelne Analyse-Funktionen
 * ========================================================================== */

/**
 * FAT BPB aus Boot-Sektor parsen
 */
mfm_error_t mfm_parse_fat_bpb(const uint8_t *boot_sector, uint16_t size,
                                fat_bpb_t *bpb);

/**
 * FAT BPB validieren (Plausibilitätsprüfung)
 */
bool mfm_validate_fat_bpb(const fat_bpb_t *bpb);

/**
 * Amiga Bootblock analysieren
 *
 * Benötigt die ersten 2 Sektoren (1024 Bytes) der Disk.
 */
mfm_error_t mfm_parse_amiga_bootblock(const uint8_t *data, uint32_t size,
                                        amiga_info_t *info);

/**
 * Amiga Bootblock-Prüfsumme verifizieren
 */
bool mfm_verify_amiga_checksum(const uint8_t *data, uint32_t size);

/**
 * CP/M Directory analysieren
 *
 * Liest Sektoren nach den System-Spuren und sucht nach gültigen
 * CP/M Directory-Einträgen. Die Erkennung basiert auf:
 *   - User-Nummern im Bereich 0-31
 *   - Gültige ASCII-Dateinamen
 *   - Konsistente Block-Allokation
 *   - Extent-Muster
 *
 * @param data          Rohdaten ab erstem Directory-Sektor
 * @param size          Größe der Daten
 * @param sector_size   Sektorgröße
 * @param analysis      Ausgabe-Struktur
 * @return MFM_OK bei Erfolg
 */
mfm_error_t mfm_analyze_cpm_directory(const uint8_t *data, uint32_t size,
                                        uint16_t sector_size,
                                        mfm_cpm_analysis_t *analysis);

/**
 * CP/M DPB aus erkannten Parametern berechnen
 */
mfm_error_t mfm_calc_cpm_dpb(const disk_physical_t *phys,
                                uint16_t boot_tracks,
                                uint16_t block_size,
                                uint16_t dir_entries,
                                mfm_cpm_dpb_t *dpb);

/**
 * Atari ST Boot-Sektor erkennen
 *
 * Atari ST nutzt FAT12, aber mit Unterschieden:
 *   - OEM-String oft leer oder Atari-spezifisch
 *   - Executable Boot-Sektor (68000-Code statt x86)
 *   - Checksum über Boot-Sektor = 0x1234
 */
bool mfm_detect_atari_st(const uint8_t *boot_sector, uint16_t size);

/**
 * Atari ST Boot-Sektor Prüfsumme berechnen
 */
uint16_t mfm_atari_st_checksum(const uint8_t *boot_sector);

/**
 * MSX-DOS erkennen
 */
bool mfm_detect_msx(const uint8_t *boot_sector, uint16_t size);


/* =============================================================================
 * API: Geometrie-Erkennung
 * ========================================================================== */

/**
 * Geometrie aus physikalischen Parametern bestimmen
 */
mfm_geometry_t mfm_identify_geometry(uint16_t sector_size,
                                       uint8_t  sectors_per_track,
                                       uint8_t  heads,
                                       uint16_t cylinders);

/**
 * Geometrie-Beschreibung als String
 */
const char *mfm_geometry_str(mfm_geometry_t geom);

/**
 * Dateisystem-Typ als String
 */
const char *mfm_fs_type_str(mfm_fs_type_t fs);

/**
 * Encoding als String
 */
const char *mfm_encoding_str(mfm_encoding_t enc);

/**
 * Fehlerbeschreibung
 */
const char *mfm_error_str(mfm_error_t err);


/* =============================================================================
 * API: Ausgabe / Reporting
 * ========================================================================== */

/**
 * Erkennungsergebnis formatiert ausgeben
 */
void mfm_detect_print_report(const mfm_detect_result_t *result, FILE *out);

/**
 * Physikalische Parameter ausgeben
 */
void mfm_print_physical(const disk_physical_t *phys, FILE *out);

/**
 * FAT BPB formatiert ausgeben
 */
void mfm_print_fat_bpb(const fat_bpb_t *bpb, FILE *out);

/**
 * Amiga-Info ausgeben
 */
void mfm_print_amiga_info(const amiga_info_t *info, FILE *out);

/**
 * CP/M-Analyse ausgeben
 */
void mfm_print_cpm_analysis(const mfm_cpm_analysis_t *analysis, FILE *out);


/* =============================================================================
 * API: Bekannte CP/M-Formate (Datenbank)
 * ========================================================================== */

/**
 * Bekanntes CP/M-Format aus Geometrie und DPB-Parametern suchen
 */
typedef struct {
    const char    *name;            /**< Format-Name */
    const char    *machine;         /**< Quellsystem */
    mfm_fs_type_t  fs_type;        /**< Spezifischer Typ */

    /* Physikalische Parameter */
    uint16_t  sector_size;
    uint8_t   sectors_per_track;
    uint8_t   heads;
    uint16_t  cylinders;
    uint8_t   min_sector_id;        /**< 0 oder 1 */

    /* CP/M DPB Parameter */
    uint16_t  block_size;
    uint16_t  dir_entries;
    uint16_t  boot_tracks;
    uint8_t   skew;
} mfm_cpm_known_format_t;

/**
 * In der Datenbank bekannter Formate suchen
 *
 * @param phys      Physikalische Parameter
 * @param matches   Array für Treffer (min. 8 Einträge)
 * @param max       Max. Treffer
 * @return Anzahl Treffer
 */
uint8_t mfm_find_known_cpm_formats(const disk_physical_t *phys,
                                     const mfm_cpm_known_format_t **matches,
                                     uint8_t max);

/**
 * Gesamtanzahl bekannter CP/M-Formate
 */
uint16_t mfm_get_known_cpm_format_count(void);

/**
 * Bekanntes Format per Index abrufen (für Iteration)
 * @return Zeiger auf Format oder NULL wenn Index außerhalb
 */
const mfm_cpm_known_format_t *mfm_get_known_cpm_format(uint16_t index);

/**
 * Kandidaten nach Konfidenz sortieren (absteigend)
 * und best_fs/best_confidence setzen.
 */
void mfm_sort_candidates(mfm_detect_result_t *result);


/* =============================================================================
 * API: Image-Datei Unterstützung
 * ========================================================================== */

/**
 * Format aus Raw-Image-Datei erkennen
 *
 * Erstellt ein Ergebnis basierend auf Dateigröße und Inhalt.
 * Unterstützt rohe Sektor-Dumps und .IMG/.DSK-Dateien.
 *
 * @param filename  Pfad zur Image-Datei
 * @param result    Ergebnis-Struktur (wird initialisiert)
 * @return MFM_OK bei Erfolg
 */
mfm_error_t mfm_detect_from_image(const char *filename,
                                    mfm_detect_result_t *result);


#endif /* MFM_DETECT_H */
