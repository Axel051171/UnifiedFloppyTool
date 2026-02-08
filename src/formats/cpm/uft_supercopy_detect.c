/**
 * @file uft_supercopy_detect.c
 * @brief SuperCopy v3.40 CP/M Format-Datenbank Integration
 *
 * Integriert die 301 CP/M-Diskettenformate aus Oliver Müllers SuperCopy v3.40
 * (1991) in UFTs Auto-Detect-Pipeline. SuperCopy liefert physikalische
 * Geometrie-Parameter; dieses Modul ergänzt DPB-Heuristiken für die
 * Dateisystem-Erkennung.
 *
 * Erkennungsfluss:
 *   1. MFM-Decoder → Sektorgröße, SPT, Encoding
 *   2. Greaseweazle/SCP → Zylinder, Heads
 *   3. supercopy_find_by_geometry() → Kandidatenliste
 *   4. DPB-Heuristik (Boot-Sektor, Directory) → Eingrenzung
 *   5. Bei Mehrdeutigkeit → GUI-Auswahl
 *
 * @author UFT Team
 * @date 2025
 * @version 4.0.0
 */

#include "uft/formats/supercopy_formats.h"
#include "uft/formats/uft_cpm_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum candidates returned by detection */
#define SC_MAX_CANDIDATES       32

/** CP/M directory entry magic: first byte of a valid entry */
#define CPM_DIR_USER_MAX        15
#define CPM_DIR_DELETED         0xE5
#define CPM_DIR_LABEL           0x20

/** Standard CP/M sector sizes for DPB heuristic */
#define CPM_LOGICAL_SECTOR      128

/* ============================================================================
 * DPB Heuristic Structures
 * ============================================================================ */

/**
 * @brief Detection confidence level
 */
typedef enum {
    SC_CONFIDENCE_NONE      = 0,   /**< No match */
    SC_CONFIDENCE_GEOMETRY  = 30,  /**< Geometry match only */
    SC_CONFIDENCE_DENSITY   = 40,  /**< Geometry + density match */
    SC_CONFIDENCE_CAPACITY  = 50,  /**< Geometry + capacity match */
    SC_CONFIDENCE_DPB_GUESS = 60,  /**< DPB heuristic match */
    SC_CONFIDENCE_DIRECTORY = 75,  /**< Valid CP/M directory found */
    SC_CONFIDENCE_BOOT      = 80,  /**< Boot sector signature match */
    SC_CONFIDENCE_FULL      = 90,  /**< Full DPB + directory match */
    SC_CONFIDENCE_UNIQUE    = 99,  /**< Unique geometry = certain */
} sc_confidence_t;

/**
 * @brief Detection candidate with confidence score
 */
typedef struct {
    const supercopy_format_t *sc_format;  /**< SuperCopy format entry */
    const cpm_format_def_t   *cpm_def;    /**< UFT CPM def (NULL if none) */
    sc_confidence_t           confidence;  /**< Detection confidence */
    uint16_t                  block_size;  /**< Guessed block size */
    uint8_t                   dir_entries; /**< Guessed directory entry count / 8 */
    uint8_t                   off_tracks;  /**< Guessed reserved tracks */
} sc_candidate_t;

/**
 * @brief Detection result
 */
typedef struct {
    sc_candidate_t candidates[SC_MAX_CANDIDATES];
    uint16_t       count;
    uint16_t       best_index;   /**< Index of highest-confidence match */
} sc_detect_result_t;

/* ============================================================================
 * DPB Heuristic Tables
 * ============================================================================ */

/**
 * @brief Standard block sizes by disk capacity
 *
 * CP/M convention: smaller disks use smaller blocks.
 * These are the most common defaults.
 */
typedef struct {
    uint32_t min_bytes;    /**< Minimum disk capacity */
    uint32_t max_bytes;    /**< Maximum disk capacity */
    uint16_t block_size;   /**< Typical block size */
    uint8_t  bsh;          /**< Block shift */
    uint8_t  blm;          /**< Block mask */
    uint16_t dir_entries;  /**< Typical directory entries */
    uint8_t  off;          /**< Typical reserved tracks */
} dpb_heuristic_t;

static const dpb_heuristic_t dpb_heuristics[] = {
    /*  min       max      BLS   BSH  BLM  DIR  OFF */
    {       0,  204800,  1024,   3,   7,   64,  2 },  /* ≤200K: 1K blocks */
    {  204801,  409600,  2048,   4,  15,   64,  2 },  /* 200-400K: 2K blocks */
    {  409601,  819200,  2048,   4,  15,  128,  2 },  /* 400-800K: 2K blocks */
    {  819201, 1228800,  4096,   5,  31,  256,  1 },  /* 800K-1.2M: 4K blocks */
    { 1228801, 1474560,  4096,   5,  31,  256,  0 },  /* 1.2-1.44M: 4K blocks */
};
#define DPB_HEURISTIC_COUNT (sizeof(dpb_heuristics) / sizeof(dpb_heuristics[0]))

/* ============================================================================
 * Known Boot Sector Signatures
 * ============================================================================ */

typedef struct {
    const uint8_t *signature;
    uint8_t        length;
    uint8_t        offset;     /**< Offset in boot sector */
    const char    *system;     /**< System name hint */
} boot_signature_t;

static const uint8_t sig_cpm22[]    = { 0xC3, 0x00, 0xF2 };  /* JP F200h */
static const uint8_t sig_cpm3[]     = { 0xC3, 0x00, 0x01 };  /* JP 0100h */
static const uint8_t sig_z80_nop[]  = { 0x00, 0x00, 0xC3 };  /* NOP NOP JP */
static const uint8_t sig_amstrad[]  = { 0x00, 0x01, 0x26 };  /* Amstrad PCW */
static const uint8_t sig_kaypro[]   = { 0xC3, 0x5C, 0xD4 };  /* Kaypro boot */

static const boot_signature_t boot_signatures[] = {
    { sig_cpm22,   3, 0, "CP/M 2.2" },
    { sig_cpm3,    3, 0, "CP/M 3.0" },
    { sig_z80_nop, 3, 0, "Z80 CP/M" },
    { sig_amstrad, 3, 0, "Amstrad"  },
    { sig_kaypro,  3, 0, "Kaypro"   },
};
#define BOOT_SIG_COUNT (sizeof(boot_signatures) / sizeof(boot_signatures[0]))

/* ============================================================================
 * Core Detection Functions
 * ============================================================================ */

/**
 * @brief Guess DPB parameters from physical geometry
 *
 * Uses capacity-based heuristics to estimate CP/M block size,
 * directory entries, and reserved tracks.
 */
static void sc_guess_dpb(sc_candidate_t *cand)
{
    if (!cand || !cand->sc_format) return;

    uint32_t cap = cand->sc_format->total_bytes;

    for (size_t i = 0; i < DPB_HEURISTIC_COUNT; i++) {
        if (cap >= dpb_heuristics[i].min_bytes &&
            cap <= dpb_heuristics[i].max_bytes) {
            cand->block_size  = dpb_heuristics[i].block_size;
            cand->dir_entries = dpb_heuristics[i].dir_entries / 8;
            cand->off_tracks  = dpb_heuristics[i].off;
            cand->confidence  = SC_CONFIDENCE_DPB_GUESS;
            return;
        }
    }

    /* Fallback: 2K blocks */
    cand->block_size  = 2048;
    cand->dir_entries = 128 / 8;
    cand->off_tracks  = 2;
    cand->confidence  = SC_CONFIDENCE_GEOMETRY;
}

/**
 * @brief Check if a sector contains a valid CP/M directory
 *
 * Scans for valid user numbers (0-15, 0xE5) in directory entry
 * positions. A high ratio of valid entries indicates a CP/M directory.
 *
 * @param data         Sector data
 * @param data_len     Length of data
 * @return             Percentage of valid directory entries (0-100)
 */
static int sc_check_directory(const uint8_t *data, size_t data_len)
{
    if (!data || data_len < 32) return 0;

    int valid = 0;
    int total = 0;
    size_t entries = data_len / 32;
    if (entries > 64) entries = 64;  /* Check max 64 entries */

    for (size_t i = 0; i < entries; i++) {
        uint8_t user = data[i * 32];
        total++;
        if (user <= CPM_DIR_USER_MAX || user == CPM_DIR_DELETED ||
            user == CPM_DIR_LABEL) {
            valid++;
        }
    }

    return total > 0 ? (valid * 100) / total : 0;
}

/**
 * @brief Check boot sector for known signatures
 *
 * @param boot_data    First sector data
 * @param boot_len     Length of boot data
 * @return             System name hint, or NULL
 */
static const char *sc_check_boot_signature(const uint8_t *boot_data,
                                           size_t boot_len)
{
    if (!boot_data || boot_len < 3) return NULL;

    for (size_t i = 0; i < BOOT_SIG_COUNT; i++) {
        const boot_signature_t *sig = &boot_signatures[i];
        if (sig->offset + sig->length <= boot_len) {
            if (memcmp(boot_data + sig->offset, sig->signature,
                       sig->length) == 0) {
                return sig->system;
            }
        }
    }
    return NULL;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Detect CP/M format from physical geometry
 *
 * Primary detection entry point. Matches geometry against SuperCopy's
 * 301 format database and assigns confidence scores.
 *
 * @param sector_size       Sector size in bytes
 * @param sectors_per_track Sectors per track
 * @param heads             Number of heads (1 or 2)
 * @param cylinders         Number of cylinders (40 or 80)
 * @param result            Output: detection result with candidates
 * @return                  Number of candidates found
 */
uint16_t sc_detect_by_geometry(
    uint16_t sector_size,
    uint8_t  sectors_per_track,
    uint8_t  heads,
    uint16_t cylinders,
    sc_detect_result_t *result)
{
    if (!result) return 0;
    memset(result, 0, sizeof(*result));

    const supercopy_format_t *matches[SC_MAX_CANDIDATES];
    uint16_t n = supercopy_find_by_geometry(
        sector_size, sectors_per_track, heads, cylinders,
        matches, SC_MAX_CANDIDATES);

    for (uint16_t i = 0; i < n && result->count < SC_MAX_CANDIDATES; i++) {
        sc_candidate_t *c = &result->candidates[result->count];
        c->sc_format  = matches[i];
        c->cpm_def    = NULL;
        c->confidence = (n == 1) ? SC_CONFIDENCE_UNIQUE
                                 : SC_CONFIDENCE_GEOMETRY;
        sc_guess_dpb(c);
        result->count++;
    }

    /* Find best candidate */
    sc_confidence_t best = SC_CONFIDENCE_NONE;
    for (uint16_t i = 0; i < result->count; i++) {
        if (result->candidates[i].confidence > best) {
            best = result->candidates[i].confidence;
            result->best_index = i;
        }
    }

    return result->count;
}

/**
 * @brief Refine detection with sector data
 *
 * Called after initial geometry detection. Analyzes boot sector and
 * directory to refine confidence scores.
 *
 * @param result         Detection result from sc_detect_by_geometry
 * @param boot_sector    Boot sector data (Track 0, Sector 0/1)
 * @param boot_len       Length of boot sector data
 * @param dir_sector     First directory sector data
 * @param dir_len        Length of directory data
 */
void sc_detect_refine(
    sc_detect_result_t *result,
    const uint8_t *boot_sector, size_t boot_len,
    const uint8_t *dir_sector,  size_t dir_len)
{
    if (!result || result->count == 0) return;

    /* Check boot signature */
    const char *boot_system = sc_check_boot_signature(boot_sector, boot_len);

    /* Check directory validity */
    int dir_score = sc_check_directory(dir_sector, dir_len);

    for (uint16_t i = 0; i < result->count; i++) {
        sc_candidate_t *c = &result->candidates[i];

        /* Boot signature match boosts confidence */
        if (boot_system && c->sc_format->description) {
            /* Check if boot signature matches system description */
            if (strstr(c->sc_format->description, boot_system) ||
                strstr(c->sc_format->description, "CP/M")) {
                if (c->confidence < SC_CONFIDENCE_BOOT)
                    c->confidence = SC_CONFIDENCE_BOOT;
            }
        }

        /* Valid directory boosts confidence */
        if (dir_score > 80) {
            if (c->confidence < SC_CONFIDENCE_DIRECTORY)
                c->confidence = SC_CONFIDENCE_DIRECTORY;
        }
    }

    /* Re-find best candidate */
    sc_confidence_t best = SC_CONFIDENCE_NONE;
    for (uint16_t i = 0; i < result->count; i++) {
        if (result->candidates[i].confidence > best) {
            best = result->candidates[i].confidence;
            result->best_index = i;
        }
    }
}

/**
 * @brief Get detection statistics
 *
 * Returns overall statistics about the SuperCopy format database.
 */
void sc_get_stats(uint16_t *total_formats,
                  uint16_t *unique_geometries,
                  uint16_t *sd_formats,
                  uint16_t *dd_formats,
                  uint16_t *hd_formats)
{
    if (total_formats) *total_formats = SUPERCOPY_FORMAT_COUNT;

    /* Count unique geometries */
    if (unique_geometries) {
        typedef struct { uint16_t ss; uint8_t spt, h; uint16_t c; } geom_t;
        geom_t seen[64];
        uint16_t nseen = 0;

        for (uint16_t i = 0; i < SUPERCOPY_FORMAT_COUNT; i++) {
            const supercopy_format_t *f = &supercopy_formats[i];
            int found = 0;
            for (uint16_t j = 0; j < nseen; j++) {
                if (seen[j].ss == f->sector_size &&
                    seen[j].spt == f->sectors_per_track &&
                    seen[j].h == f->heads &&
                    seen[j].c == f->cylinders) {
                    found = 1;
                    break;
                }
            }
            if (!found && nseen < 64) {
                seen[nseen].ss  = f->sector_size;
                seen[nseen].spt = f->sectors_per_track;
                seen[nseen].h   = f->heads;
                seen[nseen].c   = f->cylinders;
                nseen++;
            }
        }
        *unique_geometries = nseen;
    }

    /* Count by density */
    uint16_t sd = 0, dd = 0, hd = 0;
    for (uint16_t i = 0; i < SUPERCOPY_FORMAT_COUNT; i++) {
        if (supercopy_formats[i].density & SC_DENS_SD) sd++;
        if (supercopy_formats[i].density & SC_DENS_DD) dd++;
        if (supercopy_formats[i].density & SC_DENS_HD) hd++;
    }
    if (sd_formats) *sd_formats = sd;
    if (dd_formats) *dd_formats = dd;
    if (hd_formats) *hd_formats = hd;
}

/**
 * @brief Print detection result (debug/log)
 */
void sc_detect_print(const sc_detect_result_t *result, FILE *out)
{
    if (!result || !out) return;

    fprintf(out, "SuperCopy Detection: %u candidates\n", result->count);
    for (uint16_t i = 0; i < result->count; i++) {
        const sc_candidate_t *c = &result->candidates[i];
        fprintf(out, "  %s[%u] %-12s %-32s %uB/%uspt/%uH/%uC %6uB  conf=%u  BLS=%u\n",
            (i == result->best_index) ? "→ " : "  ",
            i,
            c->sc_format->name,
            c->sc_format->description,
            c->sc_format->sector_size,
            c->sc_format->sectors_per_track,
            c->sc_format->heads,
            c->sc_format->cylinders,
            c->sc_format->total_bytes,
            c->confidence,
            c->block_size);
    }
}

/**
 * @brief Iterate all SuperCopy formats matching a density flag
 *
 * Useful for GUI format selection dialogs.
 *
 * @param density    SC_DENS_SD, SC_DENS_DD, or SC_DENS_HD
 * @param callback   Called for each matching format
 * @param user_data  Passed to callback
 * @return           Number of formats iterated
 */
uint16_t sc_iterate_by_density(
    uint8_t density,
    void (*callback)(const supercopy_format_t *fmt, void *user),
    void *user_data)
{
    if (!callback) return 0;
    uint16_t count = 0;
    for (uint16_t i = 0; i < SUPERCOPY_FORMAT_COUNT; i++) {
        if (supercopy_formats[i].density & density) {
            callback(&supercopy_formats[i], user_data);
            count++;
        }
    }
    return count;
}
