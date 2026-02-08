/**
 * =============================================================================
 * Polyglotte Boot-Sektor Erkennung
 * =============================================================================
 *
 * Erkennt Multi-Platform Boot-Sektoren, wie sie auf Dual- und Triple-Format
 * Disketten verwendet wurden (PC/Atari ST/Amiga).
 *
 * Hintergrund:
 *   In den späten 1980er/frühen 1990er Jahren wurden kommerzielle Disketten
 *   (Spiele, Coverdisks von Magazinen wie "ST/Amiga Format") so formatiert,
 *   dass sie auf mehreren Plattformen bootbar waren. Die Technologie wurde
 *   hauptsächlich von Rob Northen Computing entwickelt.
 *
 *   - Dual-Format: Amiga + Atari ST (oder PC + ST)
 *   - Triple-Format: Amiga + Atari ST + PC auf einer Diskette
 *
 * Technische Grundlage:
 *   PC und Atari ST nutzen beide FAT12 mit 9×512 MFM-Sektoren.
 *   Der Amiga nutzt normalerweise 11×512 mit eigenem Sektor-Layout,
 *   kann aber über CrossDOS auch Standard-MFM-Sektoren lesen.
 *   Auf Dual/Triple-Disks ist Track 0 immer Standard-MFM (9×512),
 *   während andere Tracks im Amiga-Format (11×512) sein können.
 *
 * Boot-Sektor Signaturen:
 *   PC:       0xEB xx 0x90 (Short JMP+NOP) oder 0xE9 (Near JMP)
 *   Atari ST: 0x60 xx (68000 BRA.S), Checksum aller 256 BE-Words = 0x1234
 *   Amiga:    "DOS\0" (OFS) / "DOS\1" (FFS) Magic am Anfang des Bootblocks
 *
 * Integration: UFI Preservation Platform / MFM-Detect Modul
 * =============================================================================
 */

#ifndef POLYGLOT_BOOT_H
#define POLYGLOT_BOOT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Plattform-Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Erkannte Plattformen (Bit-Flags, kombinierbar) */
#define POLY_PLATFORM_NONE      0x00
#define POLY_PLATFORM_PC        0x01    /**< IBM PC / MS-DOS kompatibel */
#define POLY_PLATFORM_ATARI_ST  0x02    /**< Atari ST (GEMDOS/TOS) */
#define POLY_PLATFORM_AMIGA     0x04    /**< Commodore Amiga (OFS/FFS) */
#define POLY_PLATFORM_MSX       0x08    /**< MSX-DOS (FAT12-Variante) */
#define POLY_PLATFORM_CPM       0x10    /**< CP/M (kein FAT, eigenes FS) */

/** Disk-Layout Typen */
typedef enum {
    POLY_LAYOUT_SINGLE = 0,     /**< Nur ein Format auf der Disk */
    POLY_LAYOUT_DUAL,           /**< Zwei Formate (z.B. ST + Amiga) */
    POLY_LAYOUT_TRIPLE,         /**< Drei Formate (PC + ST + Amiga) */
} poly_layout_t;

/** Boot-Sektor Typ-Erkennung */
typedef enum {
    POLY_BOOT_UNKNOWN = 0,
    POLY_BOOT_PC_JMP_SHORT,     /**< 0xEB xx 0x90 - PC Short JMP + NOP */
    POLY_BOOT_PC_JMP_NEAR,      /**< 0xE9 xx xx - PC Near JMP */
    POLY_BOOT_ATARI_BRA,        /**< 0x60 xx - 68000 BRA.S */
    POLY_BOOT_AMIGA_OFS,        /**< "DOS\0" - Amiga Old Filesystem */
    POLY_BOOT_AMIGA_FFS,        /**< "DOS\1" - Amiga Fast Filesystem */
    POLY_BOOT_AMIGA_INTL_OFS,   /**< "DOS\2" - International OFS */
    POLY_BOOT_AMIGA_INTL_FFS,   /**< "DOS\3" - International FFS */
    POLY_BOOT_AMIGA_DC_OFS,     /**< "DOS\4" - Dir Cache OFS */
    POLY_BOOT_AMIGA_DC_FFS,     /**< "DOS\5" - Dir Cache FFS */
    POLY_BOOT_POLYGLOT,         /**< Mehrere gültige Interpretationen */
} poly_boot_type_t;

/** Atari ST Boot-Sektor Checksum-Status */
typedef enum {
    POLY_ST_CKSUM_NONE = 0,     /**< Keine gültige Checksumme */
    POLY_ST_CKSUM_BOOT,         /**< Bootbar: Summe = 0x1234 */
    POLY_ST_CKSUM_NONBOOT,      /**< Gültiger BPB, nicht bootbar */
} poly_st_cksum_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  FAT12 BPB (BIOS Parameter Block) - Gemeinsam für PC und Atari ST
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Geparstes BPB aus Boot-Sektor */
typedef struct {
    char     oem_name[9];       /**< OEM-String (Offset 0x03, 8 Bytes + NUL) */
    uint16_t bytes_per_sector;  /**< Offset 0x0B: normalerweise 512 */
    uint8_t  sectors_per_cluster;/**< Offset 0x0D: 1, 2, 4, 8, ... */
    uint16_t reserved_sectors;  /**< Offset 0x0E: normalerweise 1 */
    uint8_t  num_fats;          /**< Offset 0x10: normalerweise 2 */
    uint16_t root_dir_entries;  /**< Offset 0x11: z.B. 112, 224 */
    uint16_t total_sectors_16;  /**< Offset 0x13: Gesamtsektoren (16-Bit) */
    uint8_t  media_descriptor;  /**< Offset 0x15: 0xF8-0xFF */
    uint16_t sectors_per_fat;   /**< Offset 0x16: FAT-Größe in Sektoren */
    uint16_t sectors_per_track; /**< Offset 0x18: Sektoren pro Spur */
    uint16_t num_heads;         /**< Offset 0x1A: Anzahl Köpfe */
    uint32_t hidden_sectors;    /**< Offset 0x1C: Versteckte Sektoren */
    uint32_t total_sectors_32;  /**< Offset 0x20: Gesamtsektoren (32-Bit) */
    bool     valid;             /**< BPB-Werte sind plausibel */
} poly_bpb_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Amiga Bootblock-Info
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Amiga Bootblock Header (normalerweise 1024 Bytes = 2 Sektoren) */
typedef struct {
    char     type[5];           /**< "DOS\x" Magic (4 Bytes + NUL) */
    uint32_t checksum;          /**< Rootblock Checksum */
    uint32_t root_block;        /**< Rootblock-Position (normalerweise 880) */
    bool     valid;             /**< Gültiger Amiga-Bootblock erkannt */
    bool     is_ffs;            /**< Fast File System (statt Old FS) */
    bool     is_intl;           /**< International Mode */
    bool     is_dircache;       /**< Directory Cache Mode */
} poly_amiga_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Atari ST Boot-Sektor Info
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Atari ST spezifische Boot-Sektor Felder */
typedef struct {
    uint16_t branch;            /**< BRA.S Instruktion (0x60xx) */
    uint8_t  serial[3];        /**< Disk-Seriennummer (Offset 0x08) */
    uint16_t checksum;          /**< Berechnete Checksumme */
    poly_st_cksum_t cksum_status; /**< Checksum-Status */
    uint16_t exec_offset;       /**< Ziel des BRA.S (berechneter Offset) */
    bool     valid;             /**< Gültiger ST-Boot-Sektor */
} poly_atari_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Polyglot-Analyse Ergebnis
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Vollständiges Analyse-Ergebnis */
typedef struct {
    /* Boot-Sektor Rohdaten (Kopie) */
    uint8_t boot_sector[512];

    /* Allgemeine Analyse */
    poly_boot_type_t boot_type;         /**< Primärer Boot-Typ */
    uint8_t          platforms;         /**< Bit-Maske erkannter Plattformen */
    poly_layout_t    layout;            /**< Single/Dual/Triple Layout */
    uint8_t          platform_count;    /**< Anzahl erkannter Plattformen */
    uint8_t          confidence;        /**< Konfidenz 0-100 */

    /* PC-Analyse */
    struct {
        bool     has_jmp;               /**< Gültiger JMP am Anfang */
        bool     has_55aa;              /**< 0x55AA Signatur vorhanden */
        bool     valid;                 /**< Gültiger PC-Boot-Sektor */
        uint8_t  jmp_target;            /**< Ziel des JMP */
    } pc;

    /* Atari ST Analyse */
    poly_atari_info_t atari;

    /* Amiga Analyse */
    poly_amiga_info_t amiga;

    /* FAT12 BPB (gemeinsam für PC und ST) */
    poly_bpb_t bpb;

    /* Disk-Geometrie (aus BPB abgeleitet) */
    struct {
        uint16_t cylinders;     /**< Berechnete Zylinder */
        uint8_t  heads;         /**< Köpfe (aus BPB) */
        uint8_t  spt;           /**< Sektoren pro Spur (aus BPB) */
        uint16_t sector_size;   /**< Bytes pro Sektor (aus BPB) */
        uint32_t total_bytes;   /**< Gesamtkapazität */
    } geometry;

    /* Rob Northen Computing (RNC) Erkennung */
    struct {
        bool detected;          /**< RNC-Format erkannt */
        bool has_pdos;          /**< Protected DOS (RNC PDOS) Spuren */
        bool has_copylock;      /**< Copylock-Protection erkannt */
    } rnc;

    /* Track-Layout Hinweise für Dual/Triple */
    struct {
        bool fat_and_amiga;     /**< FAT12 + Amiga-Tracks gemischt */
        bool shared_track0;     /**< Track 0 wird von mehreren Systemen geteilt */
        uint16_t fat_tracks;    /**< Geschätzte Anzahl FAT-Tracks */
        uint16_t amiga_tracks;  /**< Geschätzte Anzahl Amiga-Tracks */
    } track_layout;

} poly_result_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  API-Funktionen
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Boot-Sektor analysieren (512 Bytes).
 * Erkennt PC, Atari ST und Amiga Signaturen sowie Polyglot-Kombinationen.
 *
 * @param sector   Zeiger auf 512-Byte Boot-Sektor
 * @param result   Ausgabe-Struktur
 */
void poly_analyze_boot_sector(const uint8_t *sector, poly_result_t *result);

/**
 * Erweiterte Analyse mit zweitem Sektor (für Amiga Bootblock).
 * Der Amiga-Bootblock ist 1024 Bytes (2 Sektoren), daher wird für
 * vollständige Amiga-Erkennung auch Sektor 2 benötigt.
 *
 * @param sector0  Zeiger auf ersten 512-Byte Sektor (Boot)
 * @param sector1  Zeiger auf zweiten 512-Byte Sektor (oder NULL)
 * @param result   Ausgabe-Struktur
 */
void poly_analyze_boot_extended(const uint8_t *sector0,
                                const uint8_t *sector1,
                                poly_result_t *result);

/**
 * Prüft ob ein Track Amiga-Format Merkmale hat.
 * Nützlich für Dual/Triple-Format Erkennung: Track 0 ist Standard-MFM,
 * aber spätere Tracks können im Amiga-Format sein.
 *
 * @param track_data   Rohdaten eines Tracks
 * @param track_len    Länge in Bytes
 * @return true wenn Amiga-Sync-Words (0x4489) gefunden wurden
 */
bool poly_check_amiga_track(const uint8_t *track_data, size_t track_len);

/**
 * FAT12 BPB aus Boot-Sektor extrahieren und validieren.
 *
 * @param sector  Zeiger auf 512-Byte Boot-Sektor
 * @param bpb     Ausgabe-Struktur
 * @return true wenn BPB plausibel ist
 */
bool poly_parse_bpb(const uint8_t *sector, poly_bpb_t *bpb);

/**
 * Atari ST Boot-Sektor Checksumme berechnen.
 * Die Summe aller 256 Big-Endian Words muss 0x1234 ergeben für bootbar.
 *
 * @param sector  Zeiger auf 512-Byte Boot-Sektor
 * @return Berechnete Checksumme
 */
uint16_t poly_atari_checksum(const uint8_t *sector);

/**
 * Plattform-Namen als String.
 *
 * @param platforms  Bit-Maske (POLY_PLATFORM_*)
 * @param buf        Ausgabepuffer
 * @param buf_len    Puffergröße
 * @return buf
 */
const char *poly_platforms_str(uint8_t platforms, char *buf, size_t buf_len);

/**
 * Boot-Typ als String.
 */
const char *poly_boot_type_str(poly_boot_type_t type);

/**
 * Layout-Typ als String.
 */
const char *poly_layout_str(poly_layout_t layout);

/**
 * Analyse-Bericht auf Stream ausgeben.
 */
void poly_print_report(const poly_result_t *result, FILE *stream);

#endif /* POLYGLOT_BOOT_H */
