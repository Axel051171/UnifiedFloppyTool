/**
 * @file uft_apple_fs.h
 * @brief Apple DOS 3.x, ProDOS und Pascal Filesystem Support
 * @version 3.1.4.005
 *
 * Vollständige Unterstützung für:
 * - Apple DOS 3.2 (13-Sektor)
 * - Apple DOS 3.3 (16-Sektor)
 * - ProDOS / ProDOS 8 / ProDOS 16
 * - Apple Pascal (UCSD p-System)
 * - RDOS (SSI)
 * - 2MG Container Format
 * - Applesoft und Integer BASIC Tokenisierung
 *
 * Quellen:
 * - diskm8/dskalyzer (Go, GPL)
 * - Beneath Apple DOS / Beneath Apple ProDOS
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_APPLE_FS_H
#define UFT_APPLE_FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DISK GEOMETRIE
 *============================================================================*/

#define UFT_APPLE_BYTES_PER_SECTOR      256
#define UFT_APPLE_TRACKS_140K           35
#define UFT_APPLE_TRACKS_800K           80
#define UFT_APPLE_SECTORS_13            13   /**< DOS 3.2 */
#define UFT_APPLE_SECTORS_16            16   /**< DOS 3.3 / ProDOS */
#define UFT_APPLE_SECTORS_PER_BLOCK     2    /**< ProDOS Block = 2 Sektoren */

#define UFT_APPLE_DISK_140K             143360  /**< 35×16×256 = 140KB */
#define UFT_APPLE_DISK_140K_13          116480  /**< 35×13×256 */
#define UFT_APPLE_DISK_400K             409600  /**< 80×10×512 (Mac 400K) */
#define UFT_APPLE_DISK_800K             819200  /**< 80×20×512 = 800KB */

#define UFT_APPLE_PRODOS_BLOCKS_140K    280     /**< 140K / 512 */
#define UFT_APPLE_PRODOS_BLOCKS_800K    1600    /**< 800K / 512 */

/*============================================================================
 * SEKTOR-INTERLEAVING
 *============================================================================*/

/**
 * @brief DOS 3.3 physisches zu logischem Sektor Mapping
 */
extern const uint8_t UFT_DOS33_SECTOR_ORDER[16];

/**
 * @brief ProDOS physisches zu logischem Sektor Mapping
 */
extern const uint8_t UFT_PRODOS_SECTOR_ORDER[16];

/**
 * @brief DOS 3.2 (13-Sektor) Mapping
 */
extern const uint8_t UFT_DOS32_SECTOR_ORDER[13];

/**
 * @brief DiversiDOS Sektor-Mapping
 */
extern const uint8_t UFT_DIVERSI_SECTOR_ORDER[16];

/**
 * @brief Lineares Mapping (kein Interleaving)
 */
extern const uint8_t UFT_LINEAR_SECTOR_ORDER[16];

/*============================================================================
 * NIBBLE ENCODING
 *============================================================================*/

/**
 * @brief 6-and-2 Encoding Tabelle (64 gültige Disk-Bytes)
 */
extern const uint8_t UFT_NIBBLE_62[64];

/**
 * @brief 5-and-3 Encoding Tabelle (32 gültige Disk-Bytes, DOS 3.2)
 */
extern const uint8_t UFT_NIBBLE_53[32];

/**
 * @brief Dekodiert 6-and-2 Nibbles zu Bytes
 */
bool uft_decode_62(const uint8_t* nibbles, size_t nibble_count,
                   uint8_t* output, size_t* output_size);

/**
 * @brief Enkodiert Bytes zu 6-and-2 Nibbles
 */
bool uft_encode_62(const uint8_t* data, size_t data_size,
                   uint8_t* nibbles, size_t* nibble_size);

/*============================================================================
 * DOS 3.3 VTOC (Volume Table of Contents)
 *============================================================================*/

/**
 * @brief DOS 3.3 VTOC (Track 17, Sector 0)
 */
typedef struct  uft_dos33_vtoc {
    uint8_t  unused1;               /**< Nicht verwendet */
    uint8_t  catalog_track;         /**< Track des Katalogs */
    uint8_t  catalog_sector;        /**< Sektor des Katalogs */
    uint8_t  dos_release;           /**< DOS Release (3 = DOS 3.3) */
    uint8_t  unused2[2];            /**< Reserviert */
    uint8_t  volume_number;         /**< Volume-Nummer (1-254) */
    uint8_t  unused3[32];           /**< Reserviert */
    uint8_t  max_ts_pairs;          /**< Max T/S Paare pro Sektor (122) */
    uint8_t  unused4[8];            /**< Reserviert */
    uint8_t  last_alloc_track;      /**< Letzter allokierter Track */
    int8_t   alloc_direction;       /**< Allokationsrichtung (+1/-1) */
    uint8_t  unused5[2];            /**< Reserviert */
    uint8_t  tracks_per_disk;       /**< Tracks pro Disk (35) */
    uint8_t  sectors_per_track;     /**< Sektoren pro Track (16/13) */
    uint16_t bytes_per_sector;      /**< Bytes pro Sektor (256), LE */
    uint8_t  free_sector_map[200];  /**< Free Sector Bitmap */
} uft_dos33_vtoc_t;

/**
 * @brief DOS 3.3 Catalog Sektor Header
 */
typedef struct  uft_dos33_catalog_sector {
    uint8_t  unused;                /**< Nicht verwendet */
    uint8_t  next_track;            /**< Nächster Katalog-Track */
    uint8_t  next_sector;           /**< Nächster Katalog-Sektor */
    uint8_t  unused2[8];            /**< Reserviert */
    /* 7 File Descriptive Entries à 35 Bytes folgen */
} uft_dos33_catalog_sector_t;

/**
 * @brief DOS 3.3 File Descriptive Entry (35 Bytes)
 */
typedef struct  uft_dos33_file_entry {
    uint8_t  first_ts_track;        /**< Track der T/S Liste (0xFF = gelöscht) */
    uint8_t  first_ts_sector;       /**< Sektor der T/S Liste */
    uint8_t  file_type;             /**< Dateityp + Locked-Flag */
    char     filename[30];          /**< Dateiname (Space-padded) */
    uint16_t sector_count;          /**< Sektoranzahl, LE */
} uft_dos33_file_entry_t;

/** @brief DOS 3.3 Dateitypen */
typedef enum {
    UFT_DOS33_TYPE_TEXT         = 0x00,
    UFT_DOS33_TYPE_INTEGER      = 0x01,
    UFT_DOS33_TYPE_APPLESOFT    = 0x02,
    UFT_DOS33_TYPE_BINARY       = 0x04,
    UFT_DOS33_TYPE_S            = 0x08,
    UFT_DOS33_TYPE_RELOCATABLE  = 0x10,
    UFT_DOS33_TYPE_A            = 0x20,
    UFT_DOS33_TYPE_B            = 0x40,
    UFT_DOS33_TYPE_LOCKED       = 0x80,
} uft_dos33_file_type_t;

/*============================================================================
 * PRODOS STRUKTUREN
 *============================================================================*/

#define UFT_PRODOS_ENTRY_SIZE           39  /**< Bytes pro Directory Entry */
#define UFT_PRODOS_ENTRIES_PER_BLOCK    13  /**< Entries pro Block */

/**
 * @brief ProDOS Storage Types
 */
typedef enum {
    UFT_PRODOS_STORAGE_DELETED      = 0x0,  /**< Gelöschte Datei */
    UFT_PRODOS_STORAGE_SEEDLING     = 0x1,  /**< 1 Datenblock (≤512 Bytes) */
    UFT_PRODOS_STORAGE_SAPLING      = 0x2,  /**< Index + bis zu 256 Blöcke */
    UFT_PRODOS_STORAGE_TREE         = 0x3,  /**< Master Index + 256 Indices */
    UFT_PRODOS_STORAGE_PASCAL_AREA  = 0x4,  /**< Pascal-Area */
    UFT_PRODOS_STORAGE_GSOS_FORK    = 0x5,  /**< GS/OS Extended File */
    UFT_PRODOS_STORAGE_SUBDIR       = 0xD,  /**< Subdirectory */
    UFT_PRODOS_STORAGE_SUBDIR_HDR   = 0xE,  /**< Subdirectory Header */
    UFT_PRODOS_STORAGE_VOLUME_HDR   = 0xF,  /**< Volume Directory Header */
} uft_prodos_storage_type_t;

/**
 * @brief ProDOS Access Flags
 */
typedef enum {
    UFT_PRODOS_ACCESS_READ          = 0x01, /**< Lesbar */
    UFT_PRODOS_ACCESS_WRITE         = 0x02, /**< Beschreibbar */
    UFT_PRODOS_ACCESS_BACKUP        = 0x20, /**< Backup-Flag */
    UFT_PRODOS_ACCESS_RENAME        = 0x40, /**< Umbenennbar */
    UFT_PRODOS_ACCESS_DESTROY       = 0x80, /**< Löschbar */
    UFT_PRODOS_ACCESS_DEFAULT       = 0xC3, /**< Standard (RWD) */
} uft_prodos_access_t;

/**
 * @brief ProDOS File Types
 */
typedef enum {
    UFT_PRODOS_TYPE_UNK         = 0x00, /**< Unknown */
    UFT_PRODOS_TYPE_BAD         = 0x01, /**< Bad Blocks */
    UFT_PRODOS_TYPE_PCD         = 0x02, /**< Pascal Code */
    UFT_PRODOS_TYPE_PTX         = 0x03, /**< Pascal Text */
    UFT_PRODOS_TYPE_TXT         = 0x04, /**< ASCII Text */
    UFT_PRODOS_TYPE_PDA         = 0x05, /**< Pascal Data */
    UFT_PRODOS_TYPE_BIN         = 0x06, /**< Binary */
    UFT_PRODOS_TYPE_FNT         = 0x07, /**< Font */
    UFT_PRODOS_TYPE_FOT         = 0x08, /**< Graphics */
    UFT_PRODOS_TYPE_BA3         = 0x09, /**< Business BASIC */
    UFT_PRODOS_TYPE_DA3         = 0x0A, /**< Business BASIC Data */
    UFT_PRODOS_TYPE_WPF         = 0x0B, /**< Word Processor */
    UFT_PRODOS_TYPE_SOS         = 0x0C, /**< SOS System */
    UFT_PRODOS_TYPE_DIR         = 0x0F, /**< Directory */
    UFT_PRODOS_TYPE_RPD         = 0x10, /**< RPS Data */
    UFT_PRODOS_TYPE_RPI         = 0x11, /**< RPS Index */
    UFT_PRODOS_TYPE_AFD         = 0x12, /**< AppleFile Discard */
    UFT_PRODOS_TYPE_AFM         = 0x13, /**< AppleFile Model */
    UFT_PRODOS_TYPE_AFR         = 0x14, /**< AppleFile Report */
    UFT_PRODOS_TYPE_SCL         = 0x15, /**< Screen Library */
    UFT_PRODOS_TYPE_PFS         = 0x16, /**< PFS Document */
    UFT_PRODOS_TYPE_ADB         = 0x19, /**< AppleWorks Database */
    UFT_PRODOS_TYPE_AWP         = 0x1A, /**< AppleWorks Word Proc */
    UFT_PRODOS_TYPE_ASP         = 0x1B, /**< AppleWorks Spreadsheet */
    UFT_PRODOS_TYPE_CMD         = 0xF0, /**< ProDOS Command */
    UFT_PRODOS_TYPE_INT         = 0xFA, /**< Integer BASIC */
    UFT_PRODOS_TYPE_IVR         = 0xFB, /**< Integer BASIC Vars */
    UFT_PRODOS_TYPE_BAS         = 0xFC, /**< Applesoft BASIC */
    UFT_PRODOS_TYPE_VAR         = 0xFD, /**< Applesoft Variables */
    UFT_PRODOS_TYPE_REL         = 0xFE, /**< Relocatable Code */
    UFT_PRODOS_TYPE_SYS         = 0xFF, /**< ProDOS System */
} uft_prodos_file_type_t;

/**
 * @brief ProDOS Volume Directory Header (Block 2, Offset 4)
 */
typedef struct  uft_prodos_vdh {
    uint8_t  storage_name_len;      /**< Storage Type (high nibble) + Name Len */
    char     volume_name[15];       /**< Volume Name */
    uint8_t  reserved1[8];          /**< Reserviert */
    uint32_t creation_datetime;     /**< Erstellungsdatum, LE */
    uint8_t  version;               /**< ProDOS Version (0) */
    uint8_t  min_version;           /**< Minimale Version (0) */
    uint8_t  access;                /**< Access Flags */
    uint8_t  entry_length;          /**< Entry-Länge (39) */
    uint8_t  entries_per_block;     /**< Entries pro Block (13) */
    uint16_t file_count;            /**< Anzahl aktiver Einträge, LE */
    uint16_t bitmap_pointer;        /**< Start-Block der Bitmap, LE */
    uint16_t total_blocks;          /**< Gesamtzahl Blöcke, LE */
} uft_prodos_vdh_t;

/**
 * @brief ProDOS File Entry (39 Bytes)
 */
typedef struct  uft_prodos_file_entry {
    uint8_t  storage_name_len;      /**< Storage Type (high nibble) + Name Len */
    char     filename[15];          /**< Dateiname */
    uint8_t  file_type;             /**< Dateityp */
    uint16_t key_pointer;           /**< Key Block / Directory Block, LE */
    uint16_t blocks_used;           /**< Verwendete Blöcke, LE */
    uint8_t  eof[3];                /**< Dateigröße (24-bit), LE */
    uint32_t creation_datetime;     /**< Erstellungsdatum, LE */
    uint8_t  version;               /**< Version */
    uint8_t  min_version;           /**< Minimale Version */
    uint8_t  access;                /**< Access Flags */
    uint16_t aux_type;              /**< Aux Type (Load Address etc.), LE */
    uint32_t mod_datetime;          /**< Änderungsdatum, LE */
    uint16_t header_pointer;        /**< Pointer zum Directory Header, LE */
} uft_prodos_file_entry_t;

/**
 * @brief ProDOS Datum/Zeit Format dekodieren
 */
typedef struct {
    int year;       /**< Jahr (0-99 + 1900 oder 2000) */
    int month;      /**< Monat (1-12) */
    int day;        /**< Tag (1-31) */
    int hour;       /**< Stunde (0-23) */
    int minute;     /**< Minute (0-59) */
} uft_prodos_datetime_t;

/**
 * @brief ProDOS Datetime zu Struktur dekodieren
 */
void uft_prodos_decode_datetime(uint32_t raw, uft_prodos_datetime_t* dt);

/**
 * @brief Struktur zu ProDOS Datetime enkodieren
 */
uint32_t uft_prodos_encode_datetime(const uft_prodos_datetime_t* dt);

/*============================================================================
 * 2MG CONTAINER FORMAT
 *============================================================================*/

#define UFT_2MG_MAGIC               0x474D4932  /* "2IMG" */
#define UFT_2MG_CREATOR_PRODOS      0x21         /* '!' */
#define UFT_2MG_HEADER_SIZE         64

/**
 * @brief 2MG Image Format (für ProDOS-Order Images)
 */
typedef enum {
    UFT_2MG_FORMAT_DOS33    = 0,    /**< DOS 3.3 Sektor-Order */
    UFT_2MG_FORMAT_PRODOS   = 1,    /**< ProDOS Block-Order */
    UFT_2MG_FORMAT_NIBBLE   = 2,    /**< Nibble Format */
} uft_2mg_format_t;

/**
 * @brief 2MG Header (64 Bytes)
 */
typedef struct  uft_2mg_header {
    uint32_t magic;                 /**< "2IMG" */
    uint32_t creator;               /**< Creator ID */
    uint16_t header_size;           /**< Header-Größe (64) */
    uint16_t version;               /**< Version (1) */
    uint32_t image_format;          /**< Format (0=DOS, 1=ProDOS, 2=NIB) */
    uint32_t flags;                 /**< Flags */
    uint32_t prodos_blocks;         /**< ProDOS Blöcke (wenn Format=1) */
    uint32_t data_offset;           /**< Offset zu Daten */
    uint32_t data_length;           /**< Länge der Daten */
    uint32_t comment_offset;        /**< Offset zu Kommentar */
    uint32_t comment_length;        /**< Länge des Kommentars */
    uint32_t creator_offset;        /**< Offset zu Creator-Daten */
    uint32_t creator_length;        /**< Länge der Creator-Daten */
    uint8_t  reserved[16];          /**< Reserviert */
} uft_2mg_header_t;

/** @brief 2MG Flags */
typedef enum {
    UFT_2MG_FLAG_LOCKED     = 0x80000000, /**< Write-protected */
    UFT_2MG_FLAG_VOLUME_SET = 0x00000100, /**< Volume-Nummer gesetzt */
    UFT_2MG_FLAG_VOLUME_MASK= 0x000000FF, /**< Volume-Nummer (0-254) */
} uft_2mg_flags_t;

/*============================================================================
 * APPLESOFT BASIC TOKENISIERUNG
 *============================================================================*/

/**
 * @brief Applesoft BASIC Token-Werte
 */
extern const char* UFT_APPLESOFT_TOKENS[107];  /* 0x80-0xEA */

/**
 * @brief Integer BASIC Token-Werte
 */
extern const char* UFT_INTEGER_TOKENS[128];    /* 0x00-0x7F */

/**
 * @brief Applesoft BASIC Programm zu Text dekodieren
 *
 * @param data Programmdaten (ab Offset 0x801)
 * @param size Datenlänge
 * @param output Output-Buffer
 * @param output_size Ein: Buffer-Größe, Aus: Geschriebene Bytes
 * @return true bei Erfolg
 */
bool uft_applesoft_detokenize(const uint8_t* data, size_t size,
                              char* output, size_t* output_size);

/**
 * @brief Text zu Applesoft BASIC tokenisieren
 *
 * @param text Quelltext
 * @param output Output-Buffer
 * @param output_size Ein: Buffer-Größe, Aus: Geschriebene Bytes
 * @return true bei Erfolg
 */
bool uft_applesoft_tokenize(const char* text,
                            uint8_t* output, size_t* output_size);

/**
 * @brief Integer BASIC Programm zu Text dekodieren
 */
bool uft_integer_basic_detokenize(const uint8_t* data, size_t size,
                                   char* output, size_t* output_size);

/*============================================================================
 * FORMAT-ERKENNUNG
 *============================================================================*/

/**
 * @brief Apple Disk Format IDs
 */
typedef enum {
    UFT_APPLE_FORMAT_UNKNOWN    = 0,
    UFT_APPLE_FORMAT_DOS32,         /**< DOS 3.2 (13-Sektor) */
    UFT_APPLE_FORMAT_DOS33,         /**< DOS 3.3 (16-Sektor) */
    UFT_APPLE_FORMAT_PRODOS,        /**< ProDOS 140K */
    UFT_APPLE_FORMAT_PRODOS_800K,   /**< ProDOS 800K */
    UFT_APPLE_FORMAT_PRODOS_CUSTOM, /**< ProDOS andere Größe */
    UFT_APPLE_FORMAT_PASCAL,        /**< Apple Pascal */
    UFT_APPLE_FORMAT_RDOS_3,        /**< SSI RDOS 3 */
    UFT_APPLE_FORMAT_RDOS_32,       /**< SSI RDOS 32 */
    UFT_APPLE_FORMAT_RDOS_33,       /**< SSI RDOS 33 */
    UFT_APPLE_FORMAT_CPM,           /**< Apple CP/M */
    UFT_APPLE_FORMAT_NIB,           /**< Nibble Format */
} uft_apple_format_t;

/**
 * @brief Sektor-Order IDs
 */
typedef enum {
    UFT_APPLE_ORDER_DOS33,          /**< Standard DOS 3.3 Interleave */
    UFT_APPLE_ORDER_DOS33_ALT,      /**< Alternativer DOS Interleave */
    UFT_APPLE_ORDER_DOS32,          /**< DOS 3.2 (13-Sektor) */
    UFT_APPLE_ORDER_PRODOS,         /**< ProDOS Block Order */
    UFT_APPLE_ORDER_LINEAR,         /**< Keine Interleaving */
    UFT_APPLE_ORDER_DIVERSI,        /**< DiversiDOS */
} uft_apple_sector_order_t;

/**
 * @brief Ergebnis der Format-Erkennung
 */
typedef struct {
    uft_apple_format_t format;
    uft_apple_sector_order_t order;
    int tracks;
    int sectors_per_track;
    int bytes_per_sector;
    int total_blocks;               /**< ProDOS Blöcke */
    char volume_name[32];
    int volume_number;              /**< DOS Volume Number */
    float confidence;               /**< 0.0-1.0 */
} uft_apple_detection_t;

/**
 * @brief Erkennt Apple Disk Format automatisch
 */
bool uft_apple_detect_format(const uint8_t* data, size_t size,
                             uft_apple_detection_t* result);

/**
 * @brief Prüft ob DOS 3.3 Disk
 */
bool uft_apple_is_dos33(const uint8_t* data, size_t size,
                        uft_apple_sector_order_t order);

/**
 * @brief Prüft ob ProDOS Disk
 */
bool uft_apple_is_prodos(const uint8_t* data, size_t size,
                         uft_apple_sector_order_t order);

/**
 * @brief Prüft ob Apple Pascal Disk
 */
bool uft_apple_is_pascal(const uint8_t* data, size_t size,
                         char* volume_name);

/*============================================================================
 * DISK ACCESS FUNKTIONEN
 *============================================================================*/

/**
 * @brief Liest einen logischen Sektor (mit Interleaving)
 */
bool uft_apple_read_sector(const uint8_t* disk, size_t disk_size,
                           int track, int sector,
                           uft_apple_sector_order_t order,
                           uint8_t* output);

/**
 * @brief Liest einen ProDOS Block (2 Sektoren)
 */
bool uft_prodos_read_block(const uint8_t* disk, size_t disk_size,
                           int block, uint8_t* output);

/**
 * @brief Konvertiert Track/Sektor zu ProDOS Block
 */
int uft_prodos_ts_to_block(int track, int sector);

/**
 * @brief Konvertiert ProDOS Block zu Track/Sektor
 */
void uft_prodos_block_to_ts(int block, int* track, int* sector1, int* sector2);

/*============================================================================
 * FUZZY MATCHING (für Disk-Vergleich)
 *============================================================================*/

/**
 * @brief Sektor-Info für Fuzzy Matching
 */
typedef struct {
    int track;
    int sector;
    uint8_t sha256[32];             /**< SHA-256 des Sektors */
    bool is_empty;                  /**< Sektor enthält nur 0x00 */
} uft_apple_sector_info_t;

/**
 * @brief Berechnet SHA-256 aller Sektoren
 */
bool uft_apple_hash_sectors(const uint8_t* disk, size_t size,
                            uft_apple_sector_order_t order,
                            uft_apple_sector_info_t* sectors,
                            int* sector_count);

/**
 * @brief Vergleicht zwei Disks auf Sektor-Ebene
 *
 * @return Übereinstimmung in Prozent (0.0-1.0)
 */
float uft_apple_compare_disks(const uft_apple_sector_info_t* disk1, int count1,
                              const uft_apple_sector_info_t* disk2, int count2);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_FS_H */
