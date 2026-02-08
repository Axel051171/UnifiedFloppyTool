/**
 * @file uft_format_variants.h
 * @brief UnifiedFloppyTool - Format-Varianten-Datenbank
 * 
 * Vollständige Übersicht aller unterstützten Disk-Format-Varianten.
 * Jedes "Format" kann mehrere Varianten haben, die sich durch:
 * - Dateigröße
 * - Track-Anzahl
 * - Sektor-Layout
 * - Spezielle Features (Error-Info, Half-Tracks, etc.)
 * unterscheiden.
 * 
 * AUTO-DETECTION:
 * - Primär: Dateigröße
 * - Sekundär: Magic Bytes
 * - Tertiär: Struktur-Analyse (BAM, Bootsector, etc.)
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_FORMAT_VARIANTS_H
#define UFT_FORMAT_VARIANTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Format-Varianten-Struktur
// ============================================================================

/**
 * @brief Beschreibt eine spezifische Format-Variante
 */
typedef struct {
    const char*     name;               ///< Varianten-Name
    const char*     description;        ///< Beschreibung
    uint32_t        file_size;          ///< Erwartete Dateigröße (0 = variabel)
    uint32_t        file_size_min;      ///< Minimale Größe (falls variabel)
    uint32_t        file_size_max;      ///< Maximale Größe (falls variabel)
    uint8_t         cylinders;          ///< Anzahl Zylinder/Tracks
    uint8_t         heads;              ///< Anzahl Seiten
    uint8_t         sectors;            ///< Sektoren pro Track (0 = variabel)
    uint16_t        sector_size;        ///< Bytes pro Sektor
    uint32_t        data_rate;          ///< Datenrate in bps
    uint32_t        flags;              ///< Feature-Flags
} uft_format_variant_t;

// Feature-Flags
#define UFT_VAR_ERROR_INFO      (1 << 0)    ///< Hat Error-Bytes
#define UFT_VAR_HALF_TRACKS     (1 << 1)    ///< Unterstützt Half-Tracks
#define UFT_VAR_EXTENDED        (1 << 2)    ///< Erweiterte Version
#define UFT_VAR_COMPRESSED      (1 << 3)    ///< Komprimiert
#define UFT_VAR_COPY_PROT       (1 << 4)    ///< Kann Kopierschutz speichern
#define UFT_VAR_RAW_GCR         (1 << 5)    ///< GCR-Rohdaten
#define UFT_VAR_RAW_MFM         (1 << 6)    ///< MFM-Rohdaten
#define UFT_VAR_FLUX            (1 << 7)    ///< Flux-Level
#define UFT_VAR_INTERLEAVE      (1 << 8)    ///< Sektor-Interleave
#define UFT_VAR_VARIABLE_SPT    (1 << 9)    ///< Variable Sektoren/Track
#define UFT_VAR_BOOTABLE        (1 << 10)   ///< Bootfähig
#define UFT_VAR_FILESYSTEM      (1 << 11)   ///< Hat Dateisystem-Info
#define UFT_VAR_HYBRID          (1 << 12)   ///< Hybrid-Encoding

// ============================================================================
// D64 - Commodore 64 Disk Image
// ============================================================================
// Sektor-Dump eines 1541/1571 Laufwerks
// Kein GCR, kein Kopierschutz - nur die reinen Sektor-Daten

static const uft_format_variant_t d64_variants[] = {
    {
        .name = "D64 Standard (35 Tracks)",
        .description = "Standard 1541 image, 683 sectors",
        .file_size = 174848,
        .cylinders = 35,
        .heads = 1,
        .sectors = 0,  // Variabel: 17-21 pro Track
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_FILESYSTEM
    },
    {
        .name = "D64 Standard + Error Info",
        .description = "35 tracks with 683 error bytes",
        .file_size = 175531,
        .cylinders = 35,
        .heads = 1,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_ERROR_INFO | UFT_VAR_FILESYSTEM
    },
    {
        .name = "D64 Extended (40 Tracks)",
        .description = "Extended 1541 image, 768 sectors",
        .file_size = 196608,
        .cylinders = 40,
        .heads = 1,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_EXTENDED | UFT_VAR_FILESYSTEM
    },
    {
        .name = "D64 Extended + Error Info",
        .description = "40 tracks with 768 error bytes",
        .file_size = 197376,
        .cylinders = 40,
        .heads = 1,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_EXTENDED | UFT_VAR_ERROR_INFO | UFT_VAR_FILESYSTEM
    },
    {
        .name = "D64 Extended (42 Tracks)",
        .description = "Unofficial 42-track extension",
        .file_size = 205312,
        .cylinders = 42,
        .heads = 1,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_EXTENDED | UFT_VAR_FILESYSTEM
    },
    {
        .name = "D64 Extended (42 Tracks) + Error",
        .description = "42 tracks with error bytes",
        .file_size = 206114,
        .cylinders = 42,
        .heads = 1,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_EXTENDED | UFT_VAR_ERROR_INFO | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// D64 Sektoren pro Track (Zone-basiert)
// Track  1-17: 21 Sektoren
// Track 18-24: 19 Sektoren  
// Track 25-30: 18 Sektoren
// Track 31-35: 17 Sektoren
// Track 36-40: 17 Sektoren (extended)
// Track 41-42: 17 Sektoren (unofficial)

// ============================================================================
// D71 - Commodore 1571 Disk Image
// ============================================================================
// Doppelseitiges D64-Format

static const uft_format_variant_t d71_variants[] = {
    {
        .name = "D71 Standard",
        .description = "Double-sided 1571 image, 1366 sectors",
        .file_size = 349696,
        .cylinders = 35,
        .heads = 2,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_FILESYSTEM
    },
    {
        .name = "D71 + Error Info",
        .description = "1571 image with 1366 error bytes",
        .file_size = 351062,
        .cylinders = 35,
        .heads = 2,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_ERROR_INFO | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// ============================================================================
// D81 - Commodore 1581 Disk Image
// ============================================================================
// 3.5" DD, Standard MFM

static const uft_format_variant_t d81_variants[] = {
    {
        .name = "D81 Standard",
        .description = "3.5\" DD, 80 tracks, 40 sectors/track",
        .file_size = 819200,
        .cylinders = 80,
        .heads = 2,
        .sectors = 40,
        .sector_size = 256,
        .flags = UFT_VAR_FILESYSTEM
    },
    {
        .name = "D81 + Error Info",
        .description = "D81 with 3200 error bytes",
        .file_size = 822400,
        .cylinders = 80,
        .heads = 2,
        .sectors = 40,
        .sector_size = 256,
        .flags = UFT_VAR_ERROR_INFO | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// ============================================================================
// D80/D82 - Commodore 8050/8250 Disk Image
// ============================================================================
// 8050: Single-Sided Dual Drive (77 Tracks)
// 8250: Double-Sided Dual Drive

static const uft_format_variant_t d80_variants[] = {
    {
        .name = "D80 Standard (8050)",
        .description = "Single-sided 8050, 77 tracks",
        .file_size = 533248,
        .cylinders = 77,
        .heads = 1,
        .sectors = 0,  // 23-29 variabel
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

static const uft_format_variant_t d82_variants[] = {
    {
        .name = "D82 Standard (8250)",
        .description = "Double-sided 8250, 77 tracks × 2",
        .file_size = 1066496,
        .cylinders = 77,
        .heads = 2,
        .sectors = 0,
        .sector_size = 256,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// ============================================================================
// G64 - GCR-Level Commodore Image
// ============================================================================
// Enthält echte GCR-Daten, unterstützt Kopierschutz

static const uft_format_variant_t g64_variants[] = {
    {
        .name = "G64 Standard",
        .description = "GCR image, variable size",
        .file_size = 0,  // Variabel
        .file_size_min = 8192,
        .file_size_max = 1000000,
        .cylinders = 42,  // Max
        .heads = 1,
        .sector_size = 0,  // GCR, nicht Sektor-basiert
        .flags = UFT_VAR_RAW_GCR | UFT_VAR_COPY_PROT | UFT_VAR_HALF_TRACKS
    },
    { NULL }
};

// ============================================================================
// G71 - GCR-Level 1571 Image
// ============================================================================

static const uft_format_variant_t g71_variants[] = {
    {
        .name = "G71 Standard",
        .description = "Double-sided GCR image",
        .file_size = 0,
        .file_size_min = 16384,
        .file_size_max = 2000000,
        .cylinders = 42,
        .heads = 2,
        .sector_size = 0,
        .flags = UFT_VAR_RAW_GCR | UFT_VAR_COPY_PROT | UFT_VAR_HALF_TRACKS
    },
    { NULL }
};

// ============================================================================
// ADF - Amiga Disk File
// ============================================================================

static const uft_format_variant_t adf_variants[] = {
    {
        .name = "ADF DD (880 KB)",
        .description = "Standard Amiga DD, OFS/FFS",
        .file_size = 901120,
        .cylinders = 80,
        .heads = 2,
        .sectors = 11,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_FILESYSTEM | UFT_VAR_BOOTABLE
    },
    {
        .name = "ADF HD (1.76 MB)",
        .description = "Amiga HD disk",
        .file_size = 1802240,
        .cylinders = 80,
        .heads = 2,
        .sectors = 22,
        .sector_size = 512,
        .data_rate = 500000,
        .flags = UFT_VAR_FILESYSTEM | UFT_VAR_BOOTABLE
    },
    {
        .name = "ADF Extended (81-84 Cyl)",
        .description = "Extended cylinders for more space",
        .file_size = 0,  // Variabel
        .file_size_min = 901120,
        .file_size_max = 950272,  // 84 × 2 × 11 × 512
        .cylinders = 84,
        .heads = 2,
        .sectors = 11,
        .sector_size = 512,
        .flags = UFT_VAR_FILESYSTEM | UFT_VAR_EXTENDED
    },
    { NULL }
};

// ============================================================================
// IMG/IMA - PC Disk Images
// ============================================================================

static const uft_format_variant_t img_variants[] = {
    // 5.25" Formate
    {
        .name = "160 KB 5.25\" SS/DD",
        .description = "8 sectors, 40 tracks, single-sided",
        .file_size = 163840,
        .cylinders = 40,
        .heads = 1,
        .sectors = 8,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = 0
    },
    {
        .name = "180 KB 5.25\" SS/DD",
        .description = "9 sectors, 40 tracks, single-sided",
        .file_size = 184320,
        .cylinders = 40,
        .heads = 1,
        .sectors = 9,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = 0
    },
    {
        .name = "320 KB 5.25\" DS/DD",
        .description = "8 sectors, 40 tracks, double-sided",
        .file_size = 327680,
        .cylinders = 40,
        .heads = 2,
        .sectors = 8,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = 0
    },
    {
        .name = "360 KB 5.25\" DS/DD",
        .description = "9 sectors, 40 tracks, double-sided",
        .file_size = 368640,
        .cylinders = 40,
        .heads = 2,
        .sectors = 9,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_BOOTABLE
    },
    {
        .name = "1.2 MB 5.25\" DS/HD",
        .description = "15 sectors, 80 tracks",
        .file_size = 1228800,
        .cylinders = 80,
        .heads = 2,
        .sectors = 15,
        .sector_size = 512,
        .data_rate = 500000,
        .flags = UFT_VAR_BOOTABLE
    },
    // 3.5" Formate
    {
        .name = "720 KB 3.5\" DS/DD",
        .description = "9 sectors, 80 tracks",
        .file_size = 737280,
        .cylinders = 80,
        .heads = 2,
        .sectors = 9,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_BOOTABLE
    },
    {
        .name = "1.44 MB 3.5\" DS/HD",
        .description = "18 sectors, 80 tracks",
        .file_size = 1474560,
        .cylinders = 80,
        .heads = 2,
        .sectors = 18,
        .sector_size = 512,
        .data_rate = 500000,
        .flags = UFT_VAR_BOOTABLE
    },
    {
        .name = "2.88 MB 3.5\" DS/ED",
        .description = "36 sectors, 80 tracks",
        .file_size = 2949120,
        .cylinders = 80,
        .heads = 2,
        .sectors = 36,
        .sector_size = 512,
        .data_rate = 1000000,
        .flags = UFT_VAR_BOOTABLE
    },
    // DMF und andere Spezialformate
    {
        .name = "1.68 MB DMF",
        .description = "Distribution Media Format (21 sectors)",
        .file_size = 1720320,
        .cylinders = 80,
        .heads = 2,
        .sectors = 21,
        .sector_size = 512,
        .data_rate = 500000,
        .flags = UFT_VAR_BOOTABLE | UFT_VAR_EXTENDED
    },
    {
        .name = "1.72 MB XDF",
        .description = "IBM XDF format",
        .file_size = 1763328,
        .cylinders = 80,
        .heads = 2,
        .sectors = 0,  // Variabel
        .sector_size = 512,
        .data_rate = 500000,
        .flags = UFT_VAR_VARIABLE_SPT | UFT_VAR_EXTENDED
    },
    { NULL }
};

// ============================================================================
// ST - Atari ST Disk Images
// ============================================================================

static const uft_format_variant_t st_variants[] = {
    {
        .name = "ST SS/DD (360 KB)",
        .description = "9 sectors, 80 tracks, single-sided",
        .file_size = 368640,
        .cylinders = 80,
        .heads = 1,
        .sectors = 9,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_BOOTABLE | UFT_VAR_FILESYSTEM
    },
    {
        .name = "ST DS/DD (720 KB)",
        .description = "9 sectors, 80 tracks, double-sided",
        .file_size = 737280,
        .cylinders = 80,
        .heads = 2,
        .sectors = 9,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_BOOTABLE | UFT_VAR_FILESYSTEM
    },
    {
        .name = "ST DS/DD Extended (800 KB)",
        .description = "10 sectors, 80 tracks",
        .file_size = 819200,
        .cylinders = 80,
        .heads = 2,
        .sectors = 10,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_BOOTABLE | UFT_VAR_FILESYSTEM | UFT_VAR_EXTENDED
    },
    {
        .name = "ST DS/DD Extended (880 KB)",
        .description = "11 sectors, 80 tracks",
        .file_size = 901120,
        .cylinders = 80,
        .heads = 2,
        .sectors = 11,
        .sector_size = 512,
        .data_rate = 250000,
        .flags = UFT_VAR_BOOTABLE | UFT_VAR_FILESYSTEM | UFT_VAR_EXTENDED
    },
    {
        .name = "ST HD (1.44 MB)",
        .description = "18 sectors, 80 tracks",
        .file_size = 1474560,
        .cylinders = 80,
        .heads = 2,
        .sectors = 18,
        .sector_size = 512,
        .data_rate = 500000,
        .flags = UFT_VAR_BOOTABLE | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// ============================================================================
// MSA - Magic Shadow Archiver (Atari ST, komprimiert)
// ============================================================================

static const uft_format_variant_t msa_variants[] = {
    {
        .name = "MSA Compressed",
        .description = "RLE-compressed Atari ST image",
        .file_size = 0,  // Variabel (komprimiert)
        .file_size_min = 100,
        .file_size_max = 1500000,
        .cylinders = 0,  // Im Header
        .heads = 0,
        .sectors = 0,
        .sector_size = 512,
        .flags = UFT_VAR_COMPRESSED | UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// ============================================================================
// SCP - SuperCard Pro Flux Image
// ============================================================================

static const uft_format_variant_t scp_variants[] = {
    {
        .name = "SCP Standard",
        .description = "Flux image, 1-5 revolutions",
        .file_size = 0,
        .file_size_min = 1024,
        .file_size_max = 50000000,
        .cylinders = 0,  // Im Header
        .heads = 0,
        .sector_size = 0,
        .flags = UFT_VAR_FLUX | UFT_VAR_COPY_PROT
    },
    { NULL }
};

// ============================================================================
// HFE - UFT HFE Format
// ============================================================================

static const uft_format_variant_t hfe_variants[] = {
    {
        .name = "HFE v1",
        .description = "UFT HFE Format format v1",
        .file_size = 0,
        .file_size_min = 512,
        .file_size_max = 5000000,
        .flags = UFT_VAR_RAW_MFM
    },
    {
        .name = "HFE v3",
        .description = "HxC format with extended features",
        .file_size = 0,
        .file_size_min = 512,
        .file_size_max = 5000000,
        .flags = UFT_VAR_RAW_MFM | UFT_VAR_EXTENDED
    },
    { NULL }
};

// ============================================================================
// IMD - ImageDisk (8" und 5.25" Formate)
// ============================================================================

static const uft_format_variant_t imd_variants[] = {
    {
        .name = "IMD Standard",
        .description = "ImageDisk with metadata",
        .file_size = 0,
        .file_size_min = 100,
        .file_size_max = 2000000,
        .flags = UFT_VAR_FILESYSTEM
    },
    { NULL }
};

// ============================================================================
// Apple II Formate
// ============================================================================

static const uft_format_variant_t dsk_apple_variants[] = {
    {
        .name = "Apple DOS 3.3 (140 KB)",
        .description = "16 sectors, 35 tracks",
        .file_size = 143360,
        .cylinders = 35,
        .heads = 1,
        .sectors = 16,
        .sector_size = 256,
        .flags = UFT_VAR_FILESYSTEM | UFT_VAR_INTERLEAVE
    },
    {
        .name = "Apple DOS 3.2 (113 KB)",
        .description = "13 sectors, 35 tracks",
        .file_size = 116480,
        .cylinders = 35,
        .heads = 1,
        .sectors = 13,
        .sector_size = 256,
        .flags = UFT_VAR_FILESYSTEM | UFT_VAR_INTERLEAVE
    },
    {
        .name = "Apple ProDOS (140 KB)",
        .description = "16 sectors, 35 tracks, ProDOS order",
        .file_size = 143360,
        .cylinders = 35,
        .heads = 1,
        .sectors = 16,
        .sector_size = 256,
        .flags = UFT_VAR_FILESYSTEM
    },
    { NULL }
};

static const uft_format_variant_t nib_variants[] = {
    {
        .name = "NIB Standard",
        .description = "6656 bytes/track, 35 tracks",
        .file_size = 232960,
        .cylinders = 35,
        .heads = 1,
        .sector_size = 0,  // Nibble-basiert
        .flags = UFT_VAR_COPY_PROT
    },
    { NULL }
};

// ============================================================================
// TRS-80 Formate
// ============================================================================

static const uft_format_variant_t trs80_variants[] = {
    {
        .name = "TRS-80 SS/SD (85 KB)",
        .description = "10 sectors, 35 tracks, FM",
        .file_size = 89600,
        .cylinders = 35,
        .heads = 1,
        .sectors = 10,
        .sector_size = 256,
        .data_rate = 125000,
        .flags = 0
    },
    {
        .name = "TRS-80 SS/SD (100 KB)",
        .description = "10 sectors, 40 tracks, FM",
        .file_size = 102400,
        .cylinders = 40,
        .heads = 1,
        .sectors = 10,
        .sector_size = 256,
        .data_rate = 125000,
        .flags = 0
    },
    {
        .name = "TRS-80 DS/DD (180 KB)",
        .description = "18 sectors, 40 tracks, MFM",
        .file_size = 184320,
        .cylinders = 40,
        .heads = 1,
        .sectors = 18,
        .sector_size = 256,
        .data_rate = 250000,
        .flags = 0
    },
    {
        .name = "TRS-80 DS/DD (360 KB)",
        .description = "18 sectors, 40 tracks, double-sided",
        .file_size = 368640,
        .cylinders = 40,
        .heads = 2,
        .sectors = 18,
        .sector_size = 256,
        .data_rate = 250000,
        .flags = 0
    },
    { NULL }
};

// ============================================================================
// 8" IBM-Format (FM)
// ============================================================================

static const uft_format_variant_t ibm8_variants[] = {
    {
        .name = "IBM 3740 SSSD (250 KB)",
        .description = "26 sectors × 128 bytes, 77 tracks",
        .file_size = 256256,
        .cylinders = 77,
        .heads = 1,
        .sectors = 26,
        .sector_size = 128,
        .data_rate = 250000,
        .flags = 0
    },
    {
        .name = "IBM SSDD (500 KB)",
        .description = "26 sectors × 256 bytes, 77 tracks",
        .file_size = 512512,
        .cylinders = 77,
        .heads = 1,
        .sectors = 26,
        .sector_size = 256,
        .data_rate = 500000,
        .flags = 0
    },
    {
        .name = "IBM DSDD (1 MB)",
        .description = "26 sectors × 256 bytes, 77 tracks × 2",
        .file_size = 1025024,
        .cylinders = 77,
        .heads = 2,
        .sectors = 26,
        .sector_size = 256,
        .data_rate = 500000,
        .flags = 0
    },
    { NULL }
};

// ============================================================================
// DEC RX Formate
// ============================================================================

static const uft_format_variant_t rx_variants[] = {
    {
        .name = "DEC RX01 (256 KB)",
        .description = "26 sectors × 128 bytes, 77 tracks, FM",
        .file_size = 256256,
        .cylinders = 77,
        .heads = 1,
        .sectors = 26,
        .sector_size = 128,
        .data_rate = 250000,
        .flags = 0
    },
    {
        .name = "DEC RX02 (512 KB)",
        .description = "26 sectors × 256 bytes, FM/MFM hybrid",
        .file_size = 512512,
        .cylinders = 77,
        .heads = 1,
        .sectors = 26,
        .sector_size = 256,
        .data_rate = 250000,
        .flags = UFT_VAR_HYBRID  // FM Header + MFM Data
    },
    { NULL }
};

// ============================================================================
// Format-Detection Funktionen
// ============================================================================

/**
 * @brief Findet Variante anhand der Dateigröße
 */
static inline const uft_format_variant_t* 
uft_find_variant_by_size(const uft_format_variant_t* variants, uint32_t file_size) {
    for (const uft_format_variant_t* v = variants; v->name; v++) {
        if (v->file_size == file_size) {
            return v;
        }
        if (v->file_size == 0 && 
            file_size >= v->file_size_min && 
            file_size <= v->file_size_max) {
            return v;
        }
    }
    return NULL;
}

/**
 * @brief Prüft ob Größe zu einem Format passt
 */
static inline bool 
uft_size_matches_format(const uft_format_variant_t* variants, uint32_t file_size) {
    return uft_find_variant_by_size(variants, file_size) != NULL;
}

/**
 * @brief Gibt alle Varianten für ein Format aus
 */
static inline void 
uft_print_format_variants(const char* format_name, const uft_format_variant_t* variants) {
    printf("%s Variants:\n", format_name);
    printf("%-35s %10s %4s×%1s×%2s×%4s  %s\n",
           "Name", "Size", "C", "H", "S", "Sz", "Flags");
    printf("---------------------------------------------------------------------\n");
    
    for (const uft_format_variant_t* v = variants; v->name; v++) {
        char size_str[16];
        if (v->file_size > 0) {
            snprintf(size_str, sizeof(size_str), "%u", v->file_size);
        } else {
            snprintf(size_str, sizeof(size_str), "variable");
        }
        
        printf("%-35s %10s %4d×%1d×%2d×%4d  ",
               v->name, size_str,
               v->cylinders, v->heads, v->sectors, v->sector_size);
        
        // Flags
        if (v->flags & UFT_VAR_ERROR_INFO) printf("E");
        if (v->flags & UFT_VAR_EXTENDED) printf("+");
        if (v->flags & UFT_VAR_COMPRESSED) printf("C");
        if (v->flags & UFT_VAR_COPY_PROT) printf("P");
        if (v->flags & UFT_VAR_RAW_GCR) printf("G");
        if (v->flags & UFT_VAR_RAW_MFM) printf("M");
        if (v->flags & UFT_VAR_FLUX) printf("F");
        if (v->flags & UFT_VAR_FILESYSTEM) printf("$");
        if (v->flags & UFT_VAR_HYBRID) printf("H");
        
        printf("\n");
    }
    printf("\n");
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_VARIANTS_H */
