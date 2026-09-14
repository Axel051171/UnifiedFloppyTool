/**
 * @file uft_crc_polys.h
 * @brief CRC Polynomial Database for Storage Devices
 * 
 * COVERAGE:
 * - Floppy disk CRCs (IBM, Amiga, Commodore, Apple)
 * - Hard disk CRCs (OMTI, Seagate, Western Digital, Adaptec)
 * - Tape drive CRCs (QIC, DAT)
 * - Optical media CRCs (CD, DVD)
 * - Network/Protocol CRCs (Ethernet, CAN, USB)
 * 
 * USAGE:
 * ```c
 * // Get CRC configuration
 * const uft_crc_config_t *cfg = uft_crc_get_config(UFT_CRC_IBM_MFM);
 * 
 * // Calculate CRC
 * uint32_t crc = uft_crc_calc(cfg, data, length);
 * 
 * // Or use convenience function
 * uint16_t crc = uft_crc_ibm_mfm(data, length);
 * ```
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Root-level API
 *
 * BERICHTIGT MF-1113 — hier standen zwei Zahlen und eine Aussage, und
 * alle drei tragen nicht mehr:
 *
 *   „declares 24 public functions; 22 are NOT implemented"
 *      -> gemessen ueber `git ls-files` je Bezeichner: **4 Prototypen,
 *         3 ohne Koerper**. Die 22 waren einmal richtig; die
 *         Deklarationen sind laengst entfernt, und zurueck blieben DREI
 *         LEERE Abschnittsueberschriften (je 0 Nicht-Kommentarzeilen),
 *         die MF-1113 zu einem Hinweis zusammengefasst hat.
 *
 *   „Callers exist for some of the unimplemented prototypes, so this
 *    file is a live hazard"
 *      -> gemessen: **keiner der drei hat einen Aufrufer**, im ganzen
 *         Baum kein Vorkommen ausser der Deklaration. Die Gefahr war
 *         also latent, nicht akut — und sie ist damit KLEINER als der
 *         Banner sagte, was genauso eine Falschaussage ist wie das
 *         Gegenteil.
 *
 * Was heute gilt: die drei Prototypen `uft_crc_get_config`,
 * `uft_crc_get_config_by_name` und `uft_crc_get_table` tragen seit
 * MF-1113 ein `error`-Attribut. Ein Aufruf bricht damit beim
 * UEBERSETZEN mit Begruendung statt beim Linken ohne; ein blosses
 * `#include` uebersetzt unveraendert (beides gemessen). Auf
 * Uebersetzern ohne das Attribut (MSVC) bleibt der heutige Zustand.
 *
 * `uft_crc16_ccitt` ist umgesetzt (`src/protection/uft_protection_ext.c`)
 * und nicht betroffen.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_CRC_POLYS_H
#define UFT_CRC_POLYS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Check if basic uft_crc_type_t is already defined in uft_crc.h */
#ifndef UFT_CRC_TYPE_DEFINED
#define UFT_CRC_TYPE_DEFINED

#ifndef UFT_CRC_TYPE_T_DEFINED
#define UFT_CRC_TYPE_T_DEFINED
typedef enum {
    /* === Floppy Disk CRCs === */
    UFT_CRC_IBM_MFM,            /* IBM PC MFM: CRC-16-CCITT */
    UFT_CRC_IBM_FM,             /* IBM PC FM: CRC-16-CCITT */
    UFT_CRC_AMIGA_MFM,          /* Amiga MFM: Custom checksum */
    UFT_CRC_COMMODORE_GCR,      /* C64 GCR: XOR checksum */
    UFT_CRC_APPLE_GCR,          /* Apple II GCR: Custom */
    UFT_CRC_ATARI_FM,           /* Atari 810: CRC-16 */
    UFT_CRC_BBC_FM,             /* BBC Micro FM: CRC-16 */
    
    /* === Hard Disk CRCs === */
    UFT_CRC_OMTI,               /* OMTI controllers */
    UFT_CRC_OMTI_5100,          /* OMTI 5100 series */
    UFT_CRC_SEAGATE_ST506,      /* Seagate ST-506 */
    UFT_CRC_SEAGATE_ESDI,       /* Seagate ESDI */
    UFT_CRC_WD1003,             /* Western Digital WD1003 */
    UFT_CRC_WD1006,             /* Western Digital WD1006 */
    UFT_CRC_ADAPTEC,            /* Adaptec RLL */
    UFT_CRC_XEBEC,              /* Xebec controllers */
    UFT_CRC_DTC,                /* Data Technology Corp */
    UFT_CRC_SCSI,               /* SCSI disk */
    UFT_CRC_IDE_ATA,            /* IDE/ATA */
    
    /* === Tape Drive CRCs === */
    UFT_CRC_QIC40,              /* QIC-40 tape */
    UFT_CRC_QIC80,              /* QIC-80 tape */
    UFT_CRC_QIC3010,            /* QIC-3010 tape */
    UFT_CRC_DAT_DDS,            /* DAT DDS */
    UFT_CRC_LTO,                /* LTO tape */
    UFT_CRC_8MM,                /* 8mm Exabyte */
    
    /* === Optical Media === */
    UFT_CRC_CD_ROM,             /* CD-ROM EDC */
    UFT_CRC_CD_ECC,             /* CD-ROM ECC */
    UFT_CRC_DVD,                /* DVD */
    UFT_CRC_BD,                 /* Blu-ray */
    
    /* === Network/Protocol === */
    UFT_CRC_ETHERNET,           /* Ethernet CRC-32 */
    UFT_CRC_CAN,                /* CAN bus */
    UFT_CRC_USB,                /* USB */
    UFT_CRC_HDLC,               /* HDLC */
    UFT_CRC_MODBUS,             /* Modbus */
    
    /* === Standard Polynomials === */
    UFT_CRC_8,                  /* CRC-8 */
    UFT_CRC_8_DALLAS,           /* CRC-8 Dallas/Maxim */
    UFT_CRC_16,                 /* CRC-16 IBM */
    UFT_CRC_16_CCITT,           /* CRC-16 CCITT */
    UFT_CRC_16_XMODEM,          /* CRC-16 XMODEM */
    UFT_CRC_16_MODBUS,          /* CRC-16 Modbus */
    UFT_CRC_32,                 /* CRC-32 */
    UFT_CRC_32C,                /* CRC-32C (Castagnoli) */
    UFT_CRC_64_ECMA,            /* CRC-64 ECMA */
    UFT_CRC_64_ISO,             /* CRC-64 ISO */
    
    UFT_CRC_TYPE_COUNT
} uft_crc_type_t;
#endif /* UFT_CRC_TYPE_T_DEFINED */

#endif /* UFT_CRC_TYPE_DEFINED */

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;           /* Human-readable name */
    uft_crc_type_t type;        /* CRC type enum */
    
    uint8_t width;              /* CRC width in bits (8, 16, 32, 64) */
    uint64_t polynomial;        /* Generator polynomial */
    uint64_t init;              /* Initial value */
    uint64_t xor_out;           /* Final XOR value */
    
    bool reflect_in;            /* Reflect input bytes */
    bool reflect_out;           /* Reflect output */
    
    const char *description;    /* Usage description */
} uft_crc_config_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Predefined CRC Configurations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Floppy Disk CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_IBM_MFM = {
    .name = "IBM MFM",
    .type = UFT_CRC_IBM_MFM,
    .width = 16,
    .polynomial = 0x1021,       /* x^16 + x^12 + x^5 + 1 */
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "IBM PC MFM floppy sector CRC"
};

static const uft_crc_config_t UFT_CRC_CONFIG_IBM_FM = {
    .name = "IBM FM",
    .type = UFT_CRC_IBM_FM,
    .width = 16,
    .polynomial = 0x1021,
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "IBM PC FM floppy sector CRC"
};

/* Hard Disk CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_OMTI = {
    .name = "OMTI",
    .type = UFT_CRC_OMTI,
    .width = 32,
    .polynomial = 0x140A0445,   /* OMTI proprietary */
    .init = 0xFFFFFFFF,
    .xor_out = 0x00000000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "OMTI hard disk controller CRC"
};

static const uft_crc_config_t UFT_CRC_CONFIG_SEAGATE_ST506 = {
    .name = "Seagate ST-506",
    .type = UFT_CRC_SEAGATE_ST506,
    .width = 16,
    .polynomial = 0x8005,       /* CRC-16 */
    .init = 0x0000,
    .xor_out = 0x0000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "Seagate ST-506/412 MFM hard disk"
};

static const uft_crc_config_t UFT_CRC_CONFIG_WD1003 = {
    .name = "WD1003",
    .type = UFT_CRC_WD1003,
    .width = 16,
    .polynomial = 0x8005,
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "Western Digital WD1003 controller"
};

static const uft_crc_config_t UFT_CRC_CONFIG_ADAPTEC = {
    .name = "Adaptec RLL",
    .type = UFT_CRC_ADAPTEC,
    .width = 32,
    .polynomial = 0x04C11DB7,   /* CRC-32 */
    .init = 0xFFFFFFFF,
    .xor_out = 0xFFFFFFFF,
    .reflect_in = true,
    .reflect_out = true,
    .description = "Adaptec RLL hard disk controller"
};

/* Tape Drive CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_QIC80 = {
    .name = "QIC-80",
    .type = UFT_CRC_QIC80,
    .width = 16,
    .polynomial = 0x8005,
    .init = 0x0000,
    .xor_out = 0x0000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "QIC-80 tape block CRC"
};

/* Optical Media */
static const uft_crc_config_t UFT_CRC_CONFIG_CD_ROM = {
    .name = "CD-ROM EDC",
    .type = UFT_CRC_CD_ROM,
    .width = 32,
    .polynomial = 0x8001801B,   /* CD-ROM EDC polynomial */
    .init = 0x00000000,
    .xor_out = 0x00000000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "CD-ROM Error Detection Code"
};

/* Standard CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_16_CCITT = {
    .name = "CRC-16-CCITT",
    .type = UFT_CRC_16_CCITT,
    .width = 16,
    .polynomial = 0x1021,
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "CRC-16-CCITT (X.25, HDLC)"
};

static const uft_crc_config_t UFT_CRC_CONFIG_32 = {
    .name = "CRC-32",
    .type = UFT_CRC_32,
    .width = 32,
    .polynomial = 0x04C11DB7,
    .init = 0xFFFFFFFF,
    .xor_out = 0xFFFFFFFF,
    .reflect_in = true,
    .reflect_out = true,
    .description = "CRC-32 (Ethernet, ZIP, PNG)"
};

static const uft_crc_config_t UFT_CRC_CONFIG_32C = {
    .name = "CRC-32C",
    .type = UFT_CRC_32C,
    .width = 32,
    .polynomial = 0x1EDC6F41,   /* Castagnoli */
    .init = 0xFFFFFFFF,
    .xor_out = 0xFFFFFFFF,
    .reflect_in = true,
    .reflect_out = true,
    .description = "CRC-32C (iSCSI, SCTP, Btrfs)"
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ───────────────────────────────────────────────────────────────────────
 * MF-1113: drei Zusagen ohne Gegenstand — und sie brechen jetzt beim
 * UEBERSETZEN statt beim Linken.
 *
 * Gemessen ueber `git ls-files` je Bezeichner: `uft_crc_get_config`,
 * `uft_crc_get_config_by_name` und `uft_crc_get_table` haben im GANZEN
 * Baum **keine Definition und keinen Aufrufer** — kein Vorkommen ausser
 * dieser Deklaration. Wer sie benutzt, bekommt einen Linkerfehler; das
 * ist die „Link-Falle", und sie ist echt, nur kleiner als vermutet.
 *
 * **Die Zahl gehoert dazu, weil sie in der Ablage anders steht.**
 * `docs/skeleton_triage.csv:12` fuehrt diesen Header mit
 * `IMPLEMENT,24,22,2` — 24 Deklarationen, 22 ohne Koerper. Gemessen
 * sind es **4 und 3**. Die Zahl war einmal richtig: der Header traegt
 * unten **DREI LEERE** Abschnittsueberschriften — „Convenience
 * Functions - Floppy", „- Hard Disk" und „Verification", gemessen mit
 * **0** Nicht-Kommentarzeilen. Dort standen sie; jemand hat sie
 * entfernt und die Ueberschriften stehen lassen. Die CSV ist zwar
 * abgeleitet (`scripts/skeleton_consumers.py` schreibt sie), aber sie
 * ist eine eingecheckte Momentaufnahme und driftet zwischen den
 * Laeufen; das lebende Tor `scripts/audit_skeleton_headers.py` meldet
 * fuer diesen Header heute **0**.
 *
 * (Die Zahl DREI stand hier zuerst als „vier". Nachgezaehlt sind
 * „- Standard" und „Table Generation" NICHT leer: dort liegen
 * `uft_crc16_ccitt` bzw. `uft_crc_get_table`. Gemessen statt geschaetzt,
 * beim dritten Versuch — die ersten zwei Muster zaehlten Funktionen und
 * uebersahen Typen und Tafeln.)
 *
 * **Warum nicht einfach streichen.** Gemessen ist
 * `-Werror=implicit-function-declaration` in `CMakeLists.txt`,
 * `tests/CMakeLists.txt` und `UnifiedFloppyTool.pro` **nicht** gesetzt.
 * Ein bloss entfernter Prototyp gaebe also eine WARNUNG, und der Fehler
 * kaeme weiterhin erst vom Linker — ohne Begruendung. Das
 * `error`-Attribut macht den Aufruf zum Uebersetzungsfehler MIT Grund;
 * wo der Uebersetzer es nicht kennt (MSVC), bleibt genau der heutige
 * Zustand, also keine Verschlechterung.
 *
 * **Die Deklarationen bleiben stehen** (MF-1077: Fehlklassifikation
 * wird umgeschrieben, nicht entfernt) — sie sind der einzige Beleg
 * dafuer, WAS zugesagt war. Wer sie umsetzt, nimmt das Attribut weg.
 * ─────────────────────────────────────────────────────────────────── */
#if defined(__clang__)
#  if defined(__has_attribute)
#    if __has_attribute(diagnose_if)
#      define UFT_CRC_OHNE_KOERPER(msg) \
           __attribute__((diagnose_if(1, msg, "error")))
#    endif
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define UFT_CRC_OHNE_KOERPER(msg) __attribute__((error(msg)))
#endif
#ifndef UFT_CRC_OHNE_KOERPER
/* MSVC u. a.: Linkerfehler wie bisher — keine Verschlechterung. */
#  define UFT_CRC_OHNE_KOERPER(msg)
#endif

/**
 * @brief Get CRC configuration by type
 * @param type CRC type
 * @return Configuration pointer or NULL if not found
 *
 * NICHT UMGESETZT (MF-1113) — keine Definition im Baum.
 */
const uft_crc_config_t *uft_crc_get_config(uft_crc_type_t type)
    UFT_CRC_OHNE_KOERPER("uft_crc_get_config() ist nicht umgesetzt "
                         "(MF-1113): der Header sagt sie zu, im Baum "
                         "gibt es keine Definition. Entweder umsetzen "
                         "oder die Tafel uft_crc_configs[] direkt lesen.");

/**
 * @brief Get CRC configuration by name
 * @param name CRC name (case-insensitive)
 * @return Configuration pointer or NULL if not found
 *
 * NICHT UMGESETZT (MF-1113) — keine Definition im Baum.
 */
const uft_crc_config_t *uft_crc_get_config_by_name(const char *name)
    UFT_CRC_OHNE_KOERPER("uft_crc_get_config_by_name() ist nicht "
                         "umgesetzt (MF-1113): der Header sagt sie zu, "
                         "im Baum gibt es keine Definition.");





/* ═════════════════════════════════════════════════════════════════════════════
 * Convenience Functions - Floppy / Hard Disk  +  Verification
 *
 * MF-1113: hier standen drei Ueberschriften mit NICHTS darunter —
 * gemessen je 0 Nicht-Kommentarzeilen. Sie sind der Fingerabdruck einer
 * frueheren Aufraeumung: die Deklarationen wurden entfernt, die
 * Ueberschriften blieben. Eine Ueberschrift ohne Inhalt verspricht
 * optisch eine Gruppe von Funktionen, die es nicht gibt, und genau
 * daraus stammt die Zahl 22 in `docs/skeleton_triage.csv`.
 *
 * Zusammengefasst statt dreifach leer stehen gelassen — die Information
 * waechst dabei (MF-1077: umschreiben, nicht entfernen). Wer hier
 * wieder etwas eintraegt, legt seinen eigenen Abschnitt an.
 * ═════════════════════════════════════════════════════════════════════════════ */












/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Functions - Standard
 * ═══════════════════════════════════════════════════════════════════════════════ */



/** CRC-16 CCITT */
uint16_t uft_crc16_ccitt(const uint8_t *data, size_t length);





/* ═══════════════════════════════════════════════════════════════════════════════
 * Table Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */


/**
 * @brief Get precomputed CRC table
 * @param type CRC type
 * @return Pointer to table or NULL
 *
 * NICHT UMGESETZT (MF-1113) — keine Definition im Baum. Die Begruendung
 * und die Messung stehen oben beim ersten der drei.
 */
const void *uft_crc_get_table(uft_crc_type_t type)
    UFT_CRC_OHNE_KOERPER("uft_crc_get_table() ist nicht umgesetzt "
                         "(MF-1113): es gibt im Baum keine vorberechnete "
                         "Tafel und keine Definition dieser Funktion.");





/* ═══════════════════════════════════════════════════════════════════════════════
 * Reverse Engineering
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Try to determine CRC polynomial from samples
 * @param samples Array of data/CRC pairs
 * @param sample_count Number of samples
 * @param width Expected CRC width
 * @param polynomial Output detected polynomial
 * @return 0 on success, negative on failure
 */
typedef struct {
    const uint8_t *data;
    size_t length;
    uint64_t crc;
} uft_crc_sample_t;


#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_POLYS_H */
