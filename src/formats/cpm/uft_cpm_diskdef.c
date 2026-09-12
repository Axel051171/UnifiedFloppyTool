/**
 * @file uft_cpm_diskdef.c
 * @brief CP/M-Geometrietafel — und eine Sonde, die NIE zustimmen konnte
 *        (MF-1039)
 *
 * Referenz: libdsks Geometrietafel `lib/dsksgeom.c` (`stdg[]`, John
 * Elliott, **LGPL-2+**, im Baum unter `tools/uft-scout/work/libdsk`;
 * **nur gelesen**, Kanal *Spec* nach MF-695) und cpmtools von Michael
 * Haardt (**Lizenz NICHT gemessen** — LIZ-1; hier ist nichts daraus
 * uebernommen).
 *
 * ── Befund 1: die Sonde konnte nie zustimmen ───────────────────
 *
 * `uft_cpm_detect_diskdef()` vergleicht `size` mit
 * `Zylinder * Koepfe * Sektoren * Sektorgroesse`, also mit der
 * **Gesamtgroesse des Abbilds**. `cpm_probe_plugin()` verwarf aber
 * `file_size` (`(void)file_size`) und gab die **Puffergroesse** weiter —
 * und die ist 4096 Byte.
 *
 * Gemessen: von den 17 Definitionen hat **keine** die Gesamtgroesse
 * 4096. Die Bedingung traf also nie zu. An einem
 * spezifikationsgerechten Abbild von 256 256 Byte (`ibm-8ss`):
 *
 *     probe(4096-Puffer, Dateigroesse 256256) : 0
 *     probe(VOLLER Puffer)                    : 1, Konfidenz 60
 *     open                                    : 0, liest richtig
 *
 * Das Plugin war damit **ueber die Erkennung unerreichbar** — nur eine
 * ausdrueckliche Formatwahl kam hin. Das ist die Falle aus MF-1029 in
 * ihrer reinsten Gestalt: dort war ein Groessenrueckfall toter Code, hier
 * ist es der **ganze** Erkenner. Und es ist die Form von MF-635: eine
 * Tuer, hinter der viel Koennen liegt, die aber niemand oeffnet.
 *
 * ── Befund 2: die Groesse entscheidet nicht, aber sie entschied ────
 *
 * Gemessen ueber alle Paare der Tafel:
 *
 *     184 320 Byte : amstrad-pcw (erster Sektor   1, 1 Systemspur)
 *                    amstrad-cpc (erster Sektor 193, 2 Systemspuren)
 *                    spectrum-p3 (erster Sektor   1, 1 Systemspur)
 *     143 360 Byte : nec-pc8001  (3 Systemspuren)
 *                    sharp-mz80  (2 Systemspuren)
 *
 * Die alte Auswahl nahm die **erste** passende Definition. Fuer eine
 * CPC-Datendiskette waere das `amstrad-pcw`, und damit haetten **alle
 * 360 Sektoren** die Nummern 1..9 statt 0xC1..0xC9 — die Gestalt von
 * MF-1016 (`jv1`: IDs 1..10 statt 0..9) und MF-1026 (`tan`).
 *
 * **Die Zahlen selbst sind richtig, und das ist an libdsk geprueft:**
 * `stdg[]` fuehrt `pcw180` mit 40/1/9, erstem Sektor 1 und 512 Byte,
 * `cpcdata` mit erstem Sektor **0xC1**, `pcw720` mit 80/2/9 — alle drei
 * stimmen mit UFTs Eintraegen ueberein. Falsch war nicht die Tafel,
 * sondern die **Auswahl**.
 *
 * Seit MF-1039 sagen Sonde und `open` bei Mehrdeutigkeit **ab**, statt zu
 * raten — genau wie `logical` seit MF-1032, wo die Dateigroesse die
 * Anordnung ebenfalls nicht entscheiden kann. Mehrdeutig heisst: zwei
 * passende Definitionen unterscheiden sich in Geometrie **oder** im
 * ersten Sektor. Unterscheiden sie sich nur in der Zahl der
 * Systemspuren, aendert das keinen gelesenen Sektor; dann wird die erste
 * genommen.
 *
 * Die Konfidenz faellt dabei von 60 auf **40**: erkannt sind die
 * Dateigroesse und ein einzelnes Byte am Verzeichnisanfang, und das ist
 * nach MF-729 das Band „nur die Groesse" (30-49), nicht „Struktur
 * gelesen".
 *
 * Der fehlende Kanal, um eine Definition von aussen zu benennen, ist
 * derselbe wie bei `logical` und `posix`: **P3-337**.
 */

#include "uft/formats/uft_cpm_diskdef.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "uft/uft_compat.h"

/* ============================================================================
 * Predefined CP/M Disk Definitions
 * ============================================================================ */

/* IBM 8" Single-Sided Single-Density (CP/M 1.4/2.2 standard) */
const cpm_diskdef_t cpm_diskdef_ibm_8ss = {
    .name = "ibm-8ss",
    .description = "IBM 8\" SS SD (250K)",
    .cylinders = 77,
    .heads = 1,
    .sectors = 26,
    .sector_size = 128,
    .first_sector = 1,
    .skew = 6,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 26,      /* 26 sectors * 128 bytes = 3328 bytes/track */
        .bsh = 3,       /* 1K blocks */
        .blm = 7,
        .exm = 0,
        .dsm = 242,     /* 243 blocks = 243K data */
        .drm = 63,      /* 64 directory entries */
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* IBM 8" Double-Sided Single-Density */
const cpm_diskdef_t cpm_diskdef_ibm_8ds = {
    .name = "ibm-8ds",
    .description = "IBM 8\" DS SD (500K)",
    .cylinders = 77,
    .heads = 2,
    .sectors = 26,
    .sector_size = 128,
    .first_sector = 1,
    .skew = 6,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 52,
        .bsh = 4,       /* 2K blocks */
        .blm = 15,
        .exm = 1,
        .dsm = 242,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Kaypro II (single-sided) */
const cpm_diskdef_t cpm_diskdef_kaypro2 = {
    .name = "kaypro2",
    .description = "Kaypro II 5.25\" SS DD (191K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 1,
    .dpb = {
        .spt = 40,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 194,
        .drm = 63,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Kaypro 4/10 (double-sided) */
const cpm_diskdef_t cpm_diskdef_kaypro4 = {
    .name = "kaypro4",
    .description = "Kaypro 4 5.25\" DS DD (390K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 1,
    .dpb = {
        .spt = 40,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 196,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Osborne 1 */
const cpm_diskdef_t cpm_diskdef_osborne1 = {
    .name = "osborne1",
    .description = "Osborne 1 5.25\" SS SD (92K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 2,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 3,
    .dpb = {
        .spt = 20,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 45,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Morrow MD2 */
const cpm_diskdef_t cpm_diskdef_morrow_md2 = {
    .name = "morrow-md2",
    .description = "Morrow MD2 5.25\" SS DD (384K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 17,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 68,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 149,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Morrow MD3 */
const cpm_diskdef_t cpm_diskdef_morrow_md3 = {
    .name = "morrow-md3",
    .description = "Morrow MD3 5.25\" DS DD (768K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 17,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 68,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 314,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Epson QX-10 */
const cpm_diskdef_t cpm_diskdef_epson_qx10 = {
    .name = "epson-qx10",
    .description = "Epson QX-10 5.25\" DS DD",
    .cylinders = 40,
    .heads = 2,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 64,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 299,
        .drm = 127,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Cromemco */
const cpm_diskdef_t cpm_diskdef_cromemco = {
    .name = "cromemco",
    .description = "Cromemco 5.25\" DS DD",
    .cylinders = 40,
    .heads = 2,
    .sectors = 18,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 5,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 72,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 176,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Amstrad PCW 3" CF2 */
const cpm_diskdef_t cpm_diskdef_amstrad_pcw = {
    .name = "amstrad-pcw",
    .description = "Amstrad PCW 3\" CF2 (173K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM3,
    .system_tracks = 1,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 174,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = false,
    .extent_bytes = 16,
};

/* Amstrad CPC */
const cpm_diskdef_t cpm_diskdef_amstrad_cpc = {
    .name = "amstrad-cpc",
    .description = "Amstrad CPC 3\" (178K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 0xC1,  /* CPC uses 0xC1-0xC9 */
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_NONE,
    .system_tracks = 2,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 170,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 16,
};

/* Spectrum +3 */
const cpm_diskdef_t cpm_diskdef_spectrum_p3 = {
    .name = "spectrum-p3",
    .description = "Spectrum +3 3\" (173K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM3,
    .system_tracks = 1,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 174,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = false,
    .extent_bytes = 16,
};

/* Amstrad PCW 720K */
const cpm_diskdef_t cpm_diskdef_pcw_720 = {
    .name = "pcw-720",
    .description = "Amstrad PCW 3.5\" 720K",
    .cylinders = 80,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM3,
    .system_tracks = 1,
    .dpb = {
        .spt = 36,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 357,
        .drm = 255,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 64,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = false,
    .extent_bytes = 16,
};

/* RC2014 CF format */
const cpm_diskdef_t cpm_diskdef_rc2014 = {
    .name = "rc2014",
    .description = "RC2014 CF Card (8MB)",
    .cylinders = 512,
    .heads = 2,
    .sectors = 32,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_NONE,
    .system_tracks = 6,
    .dpb = {
        .spt = 128,
        .bsh = 5,
        .blm = 31,
        .exm = 1,
        .dsm = 2039,
        .drm = 511,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 0,
        .off = 6,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 16,
};

/* RCBus */
const cpm_diskdef_t cpm_diskdef_rcbus = {
    .name = "rcbus",
    .description = "RCBus CF (4MB)",
    .cylinders = 256,
    .heads = 2,
    .sectors = 32,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_NONE,
    .system_tracks = 2,
    .dpb = {
        .spt = 128,
        .bsh = 5,
        .blm = 31,
        .exm = 1,
        .dsm = 1019,
        .drm = 511,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 0,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 16,
};

/* NEC PC-8001 */
const cpm_diskdef_t cpm_diskdef_nec_pc8001 = {
    .name = "nec-pc8001",
    .description = "NEC PC-8001 5.25\" (143K)",
    .cylinders = 35,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 3,
    .dpb = {
        .spt = 32,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 127,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Sharp MZ-80 */
const cpm_diskdef_t cpm_diskdef_sharp_mz80 = {
    .name = "sharp-mz80",
    .description = "Sharp MZ-80 5.25\" (140K)",
    .cylinders = 35,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 32,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 131,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Array of all definitions */
const cpm_diskdef_t *cpm_diskdefs[] = {
    &cpm_diskdef_ibm_8ss,
    &cpm_diskdef_ibm_8ds,
    &cpm_diskdef_kaypro2,
    &cpm_diskdef_kaypro4,
    &cpm_diskdef_osborne1,
    &cpm_diskdef_morrow_md2,
    &cpm_diskdef_morrow_md3,
    &cpm_diskdef_epson_qx10,
    &cpm_diskdef_cromemco,
    &cpm_diskdef_amstrad_pcw,
    &cpm_diskdef_amstrad_cpc,
    &cpm_diskdef_spectrum_p3,
    &cpm_diskdef_pcw_720,
    &cpm_diskdef_rc2014,
    &cpm_diskdef_rcbus,
    &cpm_diskdef_nec_pc8001,
    &cpm_diskdef_sharp_mz80,
    NULL
};

const size_t cpm_diskdef_count = sizeof(cpm_diskdefs) / sizeof(cpm_diskdefs[0]) - 1;

/* ============================================================================
 * Disk Definition Functions
 * ============================================================================ */

const cpm_diskdef_t* uft_cpm_find_diskdef(const char *name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < cpm_diskdef_count; i++) {
        if (strcasecmp(cpm_diskdefs[i]->name, name) == 0) {
            return cpm_diskdefs[i];
        }
    }
    
    return NULL;
}

const cpm_diskdef_t* uft_cpm_find_diskdef_by_geometry(
    uint16_t cylinders, uint8_t heads, uint8_t sectors, uint16_t sector_size) {
    
    for (size_t i = 0; i < cpm_diskdef_count; i++) {
        const cpm_diskdef_t *def = cpm_diskdefs[i];
        if (def->cylinders == cylinders &&
            def->heads == heads &&
            def->sectors == sectors &&
            def->sector_size == sector_size) {
            return def;
        }
    }
    
    return NULL;
}

/**
 * @brief Waehlt die Definition — und sagt ab, wenn die Datei die Wahl
 *        nicht traegt.
 *
 * @param data       Anfang der Datei (mindestens `size` Byte lesbar)
 * @param size       wie viel von `data` lesbar ist
 * @param file_size  die GESAMTE Dateigroesse. **Das ist der Unterschied
 *                   zu vorher:** die Groessengleichheit muss gegen die
 *                   Datei gehalten werden, nicht gegen den Puffer
 *                   (MF-1039, Befund 1).
 * @param mehrdeutig wird auf 1 gesetzt, wenn mehrere Definitionen passen
 *                   und sich in Geometrie oder erstem Sektor
 *                   unterscheiden — dann ist die Rueckgabe NULL.
 */
static const cpm_diskdef_t *cpm_waehle(const uint8_t *data, size_t size,
                                       size_t file_size, int *mehrdeutig)
{
    const cpm_diskdef_t *erste = NULL;
    size_t i;

    if (mehrdeutig) *mehrdeutig = 0;
    if (!data || size == 0 || file_size == 0) return NULL;

    for (i = 0; i < cpm_diskdef_count; i++) {
        const cpm_diskdef_t *def = cpm_diskdefs[i];
        size_t expected = (size_t)def->cylinders * def->heads *
                          def->sectors * def->sector_size;
        size_t dir_offset;

        if (file_size != expected) continue;

        /* Das Verzeichnis beginnt hinter den Systemspuren und traegt dort
         * 0xE5 (leer) oder einen Benutzerbereich 0..15. Liegt die Stelle
         * nicht im gelesenen Puffer, wird sie NICHT geraten — die
         * Definition gilt dann allein ueber die Groesse. */
        dir_offset = (size_t)def->system_tracks * def->heads *
                     def->sectors * def->sector_size;
        if (dir_offset < size
            && !(data[dir_offset] == 0xE5 || data[dir_offset] <= 15))
            continue;

        if (!erste) {
            erste = def;
            continue;
        }
        /* MF-1039, Befund 2: eine zweite passende Definition. Sie ist nur
         * dann harmlos, wenn sie dieselbe Geometrie UND denselben ersten
         * Sektor hat — sonst waere jede Wahl geraten. */
        if (def->cylinders       != erste->cylinders
            || def->heads        != erste->heads
            || def->sectors      != erste->sectors
            || def->sector_size  != erste->sector_size
            || def->first_sector != erste->first_sector) {
            if (mehrdeutig) *mehrdeutig = 1;
            return NULL;
        }
    }
    return erste;
}

/**
 * @brief Oeffentliche Fassung. **Achtung:** `size` muss die GESAMTE
 *        Dateigroesse sein, nicht die eines Sondenpuffers — genau diese
 *        Verwechslung war MF-1039, Befund 1.
 */
const cpm_diskdef_t* uft_cpm_detect_diskdef(const uint8_t *data, size_t size)
{
    return cpm_waehle(data, size, size, NULL);
}


size_t uft_cpm_list_diskdefs(const cpm_diskdef_t **defs, size_t max) {
    if (!defs || max == 0) return 0;
    
    size_t count = (cpm_diskdef_count < max) ? cpm_diskdef_count : max;
    
    for (size_t i = 0; i < count; i++) {
        defs[i] = cpm_diskdefs[i];
    }
    
    return count;
}

/* ============================================================================
 * CP/M Directory Operations
 * ============================================================================ */

/* Calculate logical sector number */
static size_t cpm_logical_sector(const cpm_diskdef_t *def,
                                  uint16_t track, uint16_t sector) {
    return (size_t)track * def->dpb.spt + sector;
}

/* Read sector from disk image */
static uft_error_t cpm_read_sector(const uft_disk_image_t *disk,
                                    const cpm_diskdef_t *def,
                                    uint16_t log_sector,
                                    uint8_t *buffer) {
    /* Convert logical sector to physical */
    uint16_t phys_track = log_sector / (def->sectors * (def->heads > 1 ? 2 : 1));
    uint16_t rem = log_sector % (def->sectors * (def->heads > 1 ? 2 : 1));
    uint8_t head = (def->heads > 1) ? (rem / def->sectors) : 0;
    uint8_t sector = rem % def->sectors;
    
    /* Apply skew if present */
    if (def->has_skew_table && sector < CPM_MAX_SKEW_TABLE) {
        sector = def->skew_table[sector];
    } else if (def->skew > 0) {
        sector = (sector * def->skew) % def->sectors;
    }
    
    /* Get track from disk */
    if (phys_track >= disk->tracks || head >= disk->heads) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t idx = phys_track * disk->heads + head;
    uft_track_t *track = disk->track_data[idx];
    
    if (!track || sector >= track->sector_count) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Find sector by ID */
    for (uint8_t s = 0; s < track->sector_count; s++) {
        if (track->sectors[s].id.sector == sector + def->first_sector) {
            if (track->sectors[s].data) {
                memcpy(buffer, track->sectors[s].data, def->sector_size);
                return UFT_OK;
            }
        }
    }
    
    /* Fallback: use index */
    if (track->sectors[sector].data) {
        memcpy(buffer, track->sectors[sector].data, def->sector_size);
        return UFT_OK;
    }
    
    return UFT_ERR_NOT_FOUND;
}

/* Parse filename from directory entry */
static void cpm_parse_filename(const cpm_dirent_t *entry, char *filename) {
    int pos = 0;
    
    /* Copy name, trimming spaces */
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        filename[pos++] = entry->name[i] & 0x7F;  /* Strip high bit (attributes) */
    }
    
    filename[pos++] = '.';
    
    /* Copy extension */
    for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) {
        filename[pos++] = entry->ext[i] & 0x7F;
    }
    
    /* Remove trailing dot if no extension */
    if (filename[pos - 1] == '.') {
        pos--;
    }
    
    filename[pos] = '\0';
}

uft_error_t uft_cpm_read_directory(const uft_disk_image_t *disk,
                                   const cpm_diskdef_t *def,
                                   cpm_file_t *files,
                                   size_t max_files,
                                   size_t *file_count) {
    if (!disk || !def || !files || max_files == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate directory location */
    uint16_t dir_track = def->system_tracks;
    size_t entries_per_sector = def->sector_size / 32;
    size_t dir_entries = def->dpb.drm + 1;
    size_t dir_sectors = (dir_entries + entries_per_sector - 1) / entries_per_sector;
    
    /* Allocate sector buffer */
    uint8_t *sector_buf = malloc(def->sector_size);
    if (!sector_buf) {
        return UFT_ERR_MEMORY;
    }
    
    /* Clear file list */
    memset(files, 0, max_files * sizeof(cpm_file_t));
    size_t file_idx = 0;
    
    /* Read directory */
    size_t log_sector = cpm_logical_sector(def, dir_track, 0);
    
    for (size_t ds = 0; ds < dir_sectors && file_idx < max_files; ds++) {
        if (cpm_read_sector(disk, def, log_sector + ds, sector_buf) != UFT_OK) {
            continue;
        }
        
        for (size_t e = 0; e < entries_per_sector && file_idx < max_files; e++) {
            cpm_dirent_t *entry = (cpm_dirent_t *)(sector_buf + e * 32);
            
            /* Skip deleted entries */
            if (entry->user == 0xE5) continue;
            
            /* Skip invalid user numbers */
            if (entry->user > 15) continue;
            
            /* Parse filename */
            char filename[13];
            cpm_parse_filename(entry, filename);
            
            /* Check if file already in list (additional extent) */
            bool found = false;
            for (size_t f = 0; f < file_idx; f++) {
                if (files[f].user == entry->user &&
                    strcmp(files[f].filename, filename) == 0) {
                    /* Update existing entry */
                    files[f].extents++;
                    files[f].size += entry->record_count * 128;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                /* New file */
                files[file_idx].user = entry->user;
                strncpy(files[file_idx].filename, filename, sizeof(files[file_idx].filename) - 1);
                files[file_idx].filename[sizeof(files[file_idx].filename) - 1] = '\0';
                files[file_idx].read_only = (entry->ext[0] & 0x80) != 0;
                files[file_idx].system = (entry->ext[1] & 0x80) != 0;
                files[file_idx].archived = (entry->ext[2] & 0x80) != 0;
                files[file_idx].size = entry->record_count * 128;
                files[file_idx].extents = 1;
                
                /* First block */
                if (def->dpb.dsm > 255) {
                    files[file_idx].first_block = entry->alloc[0] | (entry->alloc[1] << 8);
                } else {
                    files[file_idx].first_block = entry->alloc[0];
                }
                
                file_idx++;
            }
        }
    }
    
    free(sector_buf);
    
    if (file_count) {
        *file_count = file_idx;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool cpm_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    int mehrdeutig = 0;
    /* MF-1039, Befund 1: hier stand `(void)file_size;`, und die
     * Groessengleichheit wurde gegen die PUFFERgroesse geprueft. Keine
     * der Definitionen ist 4096 Byte gross, also sagte die Sonde
     * **immer** nein — gemessen an einer gueltigen 256 256-Byte-Datei. */
    const cpm_diskdef_t *def = cpm_waehle(data, size, file_size,
                                          &mehrdeutig);
    if (!def) return false;      /* auch bei mehrdeutig: kein Anspruch */

    /* MF-729: erkannt sind die Dateigroesse und ein Byte am
     * Verzeichnisanfang. Das ist das Band „nur die Groesse" (30-49).
     * Vorher standen hier 60 — eine Zahl aus dem Band „Struktur
     * gelesen", fuer die es keine Deckung gab. */
    if (confidence) *confidence = 40;
    return true;
}


static uft_error_t cpm_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    
    /* Read file */
    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    /* Detect format.
     *
     * MF-1039, Befund 2: hier entschied die Tabellenreihenfolge. Bei
     * 184 320 Byte passen drei Definitionen, und `amstrad-cpc` hat den
     * ersten Sektor 0xC1 statt 1 — eine CPC-Datendiskette bekam damit
     * alle 360 Sektornummern falsch. Jetzt wird abgesagt, statt zu
     * raten; der fehlende Kanal fuer eine benannte Definition ist
     * P3-337, dieselbe Lage wie bei `logical` (MF-1032) und `posix`
     * (MF-1034). */
    int mehrdeutig = 0;
    const cpm_diskdef_t *def = cpm_waehle(data, size, size, &mehrdeutig);
    if (!def) {
        free(data);
        return mehrdeutig ? UFT_ERROR_NOT_SUPPORTED : UFT_ERR_FORMAT;
    }
    
    /* Create disk image */
    uft_disk_image_t *image = uft_disk_alloc(def->cylinders, def->heads);
    if (!image) {
        free(data);
        return UFT_ERR_MEMORY;
    }
    
    image->format = UFT_FORMAT_RAW;
    snprintf(image->format_name, sizeof(image->format_name), "CP/M (%s)", def->name);
    image->sectors_per_track = def->sectors;
    image->bytes_per_sector = def->sector_size;
    
    /* Read sectors */
    size_t data_pos = 0;
    uint8_t size_code = 2;  /* Assume 512 bytes, adjust if needed */
    if (def->sector_size == 128) size_code = 0;
    else if (def->sector_size == 256) size_code = 1;
    else if (def->sector_size == 1024) size_code = 3;
    
    for (uint16_t c = 0; c < def->cylinders; c++) {
        for (uint8_t h = 0; h < def->heads; h++) {
            size_t idx = c * def->heads + h;
            
            uft_track_t *track = uft_track_alloc(def->sectors, 0);
            if (!track) {
                uft_disk_free(image);
                free(data);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = def->encoding;
            
            for (uint8_t s = 0; s < def->sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + def->first_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(def->sector_size);
                sect->data_size = def->sector_size;
                
                if (sect->data && data_pos + def->sector_size <= size) {
                    memcpy(sect->data, data + data_pos, def->sector_size);
                }
                data_pos += def->sector_size;
                track->sector_count++;
            }
            
            image->track_data[idx] = track;
        }
    }
    
    free(data);
    
    disk->plugin_data = image;
    disk->geometry.cylinders = image->tracks;
    disk->geometry.heads = image->heads;
    disk->geometry.sectors = image->sectors_per_track;
    disk->geometry.sector_size = image->bytes_per_sector;
    disk->geometry.total_sectors = (uint32_t)image->tracks * image->heads *
                                   image->sectors_per_track;

    return UFT_OK;
}

static void cpm_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t cpm_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;

    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }

    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = head;
    track->encoding = src->encoding;

    /* MF-516: hier stand `track->sectors[s] = src->sectors[s];`.
     *
     * `uft_track_t.sectors` ist ein DYNAMISCHER Zeiger, kein Feld:
     *
     *     uft_sector_t*  sectors;
     *     size_t         sector_count, sector_capacity;
     *
     * `uft_track_init()` legt ihn NICHT an — es nullt die Struktur und
     * setzt Zylinder und Kopf. Der Zielpuffer kommt vom Aufrufer und ist
     * genullt. `track->sectors` war hier also bei JEDEM erfolgreichen
     * Lesen NULL, und die Schleife schrieb hindurch. Dieses read_track
     * kann nie funktioniert haben.
     *
     * `uft_track_add_sector()` legt den Puffer an, laesst ihn wachsen und
     * kopiert die Sektordaten tief — genau das, was die Schleife von Hand
     * versuchte, nur ohne den Nullzeiger.
     *
     * Derselbe Rumpf stand woertlich in 12 Plugins. Alle 12 sind
     * geaendert; `scripts/audit_read_track_contract.py` meldet den 13ten.
     * Gefunden hat es tests/test_disk_open_fuzz.c, indem es eine gueltige
     * D81-Datei an MGT weiterreichte, dessen Sonde zugestimmt hatte. */
    for (size_t s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }

    return UFT_OK;
}

/* In-memory write: updates cached disk image. Persist via uft_cpm_write(). */
static uft_error_t cpm_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. MF-519 hat das fuer
     * read_track getan und write_track uebersehen. Das ASan-Tor
     * der CI fand die Folge an d80_write_track: die Schranke
     * `cyl >= D80_TRACKS` laesst -1 durch, und d80_spt[-1] liest
     * vor der Tabelle.
     *
     * Beim SCHREIBEN wiegt das schwerer als beim Lesen: ein
     * falscher Index liefert nicht nur falsche Daten, er bestimmt,
     * WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;

    /* MF-883: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Gemessen: in dieser Datei steht keine einzige Schreiboperation
     * (`fwrite`/`fputc`/`fprintf`/`ftruncate`/`WriteFile`), das Plugin hat
     * kein `.flush`, und `close()` gibt den Puffer frei. Kein Byte hat je
     * die Platte erreicht — der Aufrufer bekam Erfolg gemeldet.
     *
     * Und es gibt auch keinen allgemeinen Rueckweg: `plugin->flush` wird im
     * ganzen Baum von NIEMANDEM gerufen (gemessen ueber `git ls-files`,
     * kommentarfrei), `uft_disk_close()` ruft nur `close`. Selbst ein
     * Plugin MIT Flush kaeme nicht durch.
     *
     * Betroffen war auch der Wandlungspfad: `uft_disk_convert.c:41` zaehlt
     * `tracks_converted++` bei `UFT_OK` und schreibt danach nichts hinaus.
     *
     * Warum kein echter Schreiber gebaut wurde: die EINFRIER-REGEL
     * (MF-363/498) verlangt benannte Referenz, gemessene Zahlen und die
     * Referenz im Header. Neun Container-Schreiber gegen diese Lage waeren
     * neun Wetten. Die Zusage wahr zu machen ist der kleinere und richtige
     * Schritt — dieselbe Entscheidung wie MF-880 (PRO).
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    (void)track;
    return UFT_ERR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_cpm_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_cpm = {
    .name = "CP/M",
    .description = "CP/M Disk Image",
    .extensions = "cpm,dsk",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = cpm_probe_plugin,
    .open = cpm_open,
    .close = cpm_close,
    .read_track = cpm_read_track,
    .write_track = cpm_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_cpm_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_cpm_features) / sizeof(uft_format_plugin_cpm_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(cpm)
