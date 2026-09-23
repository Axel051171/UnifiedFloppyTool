/**
 * @file uft_geos_protection.c
 * @brief GEOS Copy Protection Detection and Analysis
 * @version 4.1.1
 * 
 * ── BERICHTIGT MF-1333. Hier stand eine Liste von SECHS
 *    Schutzverfahren ("Track 1 Sector 0 Signature", "Sector Interleave
 *    Verification", "Hardware fingerprinting" …) und als Quelle die
 *    Buchtitel "GEOS Inside and Out, GEOS Programmer's Reference".
 *    Keines der sechs war umgesetzt, keines der Buecher liegt im Baum,
 *    und die drei Schutzarten, die der Code TATSAECHLICH zuwies,
 *    standen in der Liste nicht so, wie er sie zuwies. ────────────────
 *
 * WAS GEMESSEN IST
 *
 * 1. Wie eine GEOS-Diskette erkannt wird.
 *    `docs/format_specs/commodore/GEOS.TXT:247-249` — im Baum — sagt
 *    woertlich, wie GEOS SELBST prueft:
 *      "check the string in the BAM sector starting at $AD (offset 173)
 *       for the string "GEOS format". If it does not match, the disk is
 *       not in GEOS format. This is the way that GEOS itself verifies
 *       if a disk is GEOS formatted."
 *    Der BAM-Sektor ist Spur 18, Sektor 0. Die Fassung steht direkt
 *    dahinter: "AD-BC: GEOS ID string ("GEOS format V1.x", in ASCII)"
 *    (GEOS.TXT:232), die Ziffer also bei $AD+13 = $BA.
 *
 *    Vor MF-1333 suchte dieses Modul vier Byte "GEOS" an BELIEBIGEM
 *    Versatz in SPUR 1 SEKTOR 0 — falscher Sektor, falscher Versatz,
 *    zu schwache Kennung. Gemessen hiess das: eine echte GEOS-Diskette
 *    wurde NICHT erkannt, und eine beliebige Diskette mit den vier
 *    Buchstaben irgendwo in Spur 1 wurde angenommen. Klasse MF-961
 *    (`86f`, Kennung "86BX" steht in keiner Datei) und MF-1022 (`sap`).
 *
 * 2. Wo der Kopierschutz liegt.
 *    Die Beschreibung des URHEBERS des GeoCopy-Pakets (Christian
 *    Meilinger, `neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt`) sagt:
 *      "Dieser Track ist auf allen GEOS-Boot-Disketten die Nummer 21
 *       (dezimal). Der Inhalt der Luecken ist auf der Original-Diskette:
 *       ... $55 $55 $67 $55 $55 $67 SYNC ... statt
 *       ... $55 $55 $55 $55 $55 $55 SYNC ... (Standard-Formatierung)"
 *    und zur Verankerung in der BAM:
 *      "Der Track $15 ist in der BAM geschuetzt, aber nicht mit einem
 *       Directory-Eintrag verbunden."
 *
 *    KANAL nach MF-695: *Spec*, nicht *Port*. Die Lizenz im Paket
 *    ("Diese Programme sind somit Public Domain und duerfen nicht
 *    kommerziell ohne Einwilligung des Autors vertrieben werden!")
 *    gewaehrt die Weitergabe, verbietet aber den kommerziellen
 *    Vertrieb — damit ist sie mit diesem GPL-2-Baum unvereinbar. Aus
 *    dem Paket stammt KEINE Zeile Code, nur gelesene Beschreibung.
 *
 * 3. Was dieses Modul aus SEKTORDATEN nicht entscheiden kann.
 *    Der eigentliche Schutz liegt in den Luecken ZWISCHEN den Sektoren.
 *    Ein D64 speichert sie nicht, und der Baum hat (Stand MF-1333)
 *    keinen Zerleger, der eine rohe GCR-Spur in Sync/Header/Gap/Daten
 *    gliedert. Deshalb meldet dieses Modul `GEOS_UNMESSBAR_GAPS`,
 *    statt zu schweigen — eine 0 in `protection_count` waere sonst
 *    nicht von "nichts gefunden" zu unterscheiden (MF-1311).
 *
 *    `GEOS_PROT_V1_KEY_DISK` und `GEOS_PROT_V2_ENHANCED` werden
 *    deshalb NICHT mehr vergeben. Sie bleiben in der Aufzaehlung und
 *    in der Tafel stehen (MF-1077: Fehlklassifikation wird
 *    umgeschrieben, nicht entfernt) und warten auf die Gap-Ebene.
 */

#include "uft/protection/uft_geos_protection.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * GEOS Format Constants
 * ============================================================================ */

/* GEOS file structure */
#define GEOS_HEADER_SIZE        256
#define GEOS_ICON_WIDTH         24
#define GEOS_ICON_HEIGHT        21

/* GEOS file types */
#define GEOS_TYPE_NON_GEOS      0
#define GEOS_TYPE_BASIC         1
#define GEOS_TYPE_ASSEMBLER     2
#define GEOS_TYPE_DATA          3
#define GEOS_TYPE_SYSTEM        4
#define GEOS_TYPE_DESK_ACC      5
#define GEOS_TYPE_APPLICATION   6
#define GEOS_TYPE_PRINTER       7
#define GEOS_TYPE_INPUT         8
#define GEOS_TYPE_DISK          9
#define GEOS_TYPE_BOOT          10
#define GEOS_TYPE_TEMP          11
#define GEOS_TYPE_AUTO_EXEC     12
#define GEOS_TYPE_DIRECTORY     13
#define GEOS_TYPE_FONT          14
#define GEOS_TYPE_DOCUMENT      15

/* GEOS structure types */
#define GEOS_STRUCT_SEQ         0
#define GEOS_STRUCT_VLIR        1

/* GEOS signature locations
 *
 * MF-1333: `GEOS_BOOT_TRACK 1` / `GEOS_BOOT_SECTOR 0` stehen hier noch,
 * weil sie in der Aufzaehlung der Konstanten niemandem schaden — die
 * KENNUNG steht dort aber nicht. Sie steht im BAM-Sektor, Spur 18
 * Sektor 0 (GEOS.TXT:247). Beide Konstanten haben seit MF-1333 keinen
 * Leser mehr; sie bleiben stehen statt zu verschwinden (MF-1077).
 */
#define GEOS_BOOT_TRACK         1
#define GEOS_BOOT_SECTOR        0
#define GEOS_DIR_TRACK          18
#define GEOS_DIR_SECTOR         1

/* ── Der BAM-Sektor, so wie GEOS.TXT ihn beschreibt ──────────────────
 *
 * GEOS.TXT:220  "04-8F: BAM entries for each track, in groups of four
 *                bytes per track, starting on track 1"
 * GEOS.TXT:232  "AD-BC: GEOS ID string ("GEOS format V1.x", in ASCII)"
 *
 * Der BAM-Eintrag einer Spur ist damit `4 + (spur-1)*4`, und sein
 * erstes Byte ist die Zahl der freien Sektoren. Dieselbe Formel rechnen
 * sechs weitere Stellen im Baum, u. a. `src/formats/c64/uft_d71_d81.c`
 * und `src/formats/d64/uft_d64_parser_v3.c:1211`.
 *
 * Vor MF-1333 stand hier `4 + 18*4` = 76 — das ist `4 + (19-1)*4`, also
 * der Eintrag von SPUR 19. Dass Spur 18 gemeint war, beweist die
 * Konstante daneben: `!= 0x11` (17 frei) trifft genau Spur 18 mit
 * belegtem BAM- und erstem Verzeichnissektor.
 */
#define GEOS_BAM_TRACK          18
#define GEOS_BAM_SECTOR         0
#define GEOS_BAM_ENTRY(spur)    (4 + ((spur) - 1) * 4)

/** Der BAM-Sektor ist ein CBM-Sektor: genau 256 Byte. Kuerzer heisst
 *  "nicht gemessen", nicht "kein GEOS". */
#define GEOS_BAM_SECTOR_SIZE    256

/** Versatz der GEOS-Kennung im BAM-Sektor (GEOS.TXT:232). */
#define GEOS_ID_OFFSET          0xAD
/** "GEOS format" — 11 Zeichen, ohne Abschluss-Null verglichen. */
#define GEOS_ID_LEN             11
/** "GEOS format V1.x": 'V' bei +12, die Fassungsziffer bei +13 ($BA). */
#define GEOS_ID_VERSION_AT      (GEOS_ID_OFFSET + 13)

/** Die Schutzspur, vom Urheber benannt: "die Nummer 21 (dezimal)". */
#define GEOS_SCHUTZSPUR         21

/* Die Kennung, an der GEOS selbst eine GEOS-Diskette erkennt. */
static const uint8_t GEOS_ID_MARKER[GEOS_ID_LEN] = {
    0x47, 0x45, 0x4F, 0x53, 0x20, 0x66, 0x6F, 0x72,
    0x6D, 0x61, 0x74                                  /* "GEOS format" */
};

/* ============================================================================
 * Protection Types
 * ============================================================================ */

static const geos_protection_info_t g_geos_protections[] = {
    {
        .type = GEOS_PROT_V1_KEY_DISK,
        .name = "GEOS V1 Key Disk",
        .description = "Original GEOS key disk protection",
        .severity = GEOS_SEV_STANDARD,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_V2_ENHANCED,
        .name = "GEOS V2 Enhanced",
        .description = "GEOS 2.0+ enhanced protection",
        .severity = GEOS_SEV_STANDARD,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_SERIAL_CHECK,
        .name = "Serial Number Check",
        .description = "Disk-specific serial number verification",
        .severity = GEOS_SEV_STANDARD,
        .copyable_with_nibbler = true,
        .requires_original = false
    },
    {
        .type = GEOS_PROT_HALF_TRACK,
        .name = "Half-Track Protection",
        .description = "Data written between standard tracks",
        .severity = GEOS_SEV_DIFFICULT,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_BAM_SIGNATURE,
        .name = "BAM Signature",
        .description = "Modified BAM entries for verification",
        .severity = GEOS_SEV_TRIVIAL,
        .copyable_with_nibbler = false,
        .requires_original = false
    },
    {
        .type = GEOS_PROT_INTERLEAVE,
        .name = "Non-Standard Interleave",
        .description = "Custom sector interleave pattern",
        .severity = GEOS_SEV_TRIVIAL,
        .copyable_with_nibbler = false,
        .requires_original = false
    },
    {
        .type = GEOS_PROT_SYNC_MARK,
        .name = "Custom Sync Marks",
        .description = "Modified GCR sync patterns",
        .severity = GEOS_SEV_DIFFICULT,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_NONE,
        .name = "No Protection",
        .description = "Standard GEOS disk without protection",
        .severity = GEOS_SEV_NONE,
        .copyable_with_nibbler = false,
        .requires_original = false
    }
};

/* ============================================================================
 * Detection Functions
 * ============================================================================ */

/* MF-1333: beide Funktionen behalten Namen und Signatur (0 Aufrufer im
 * Baum, aber MF-1077 gilt trotzdem) und pruefen jetzt an der Stelle,
 * die GEOS.TXT:247 nennt. Der Parameter ist der BAM-SEKTOR (Spur 18,
 * Sektor 0) — der historische Name "boot" traegt das nicht, und das
 * steht deshalb hier und im Header.
 *
 * Eine Suche "an beliebigem Versatz" ist bei einer 11 Byte langen
 * Zeichenkette in einem 256-Byte-Sektor keine Kennung, sondern eine
 * Wahrscheinlichkeitsaussage. Der Baum hat dieselbe Falle schon bei
 * `kfx` gehabt (MF-919: die Sonde ZAEHLTE 0x0D-Bytes und konnte in
 * 512 Zufallsbytes nie "nein" sagen).
 */
bool uft_geos_detect_boot_signature(const uint8_t *sector_data, size_t size) {
    if (!sector_data) return false;
    if (size < (size_t)(GEOS_ID_OFFSET + GEOS_ID_LEN)) return false;

    return memcmp(sector_data + GEOS_ID_OFFSET,
                  GEOS_ID_MARKER, GEOS_ID_LEN) == 0;
}

/* Die "erweiterte" Kennung gibt es nicht: GEOS.TXT kennt EINE Kennung,
 * und die Fassung steht als ASCII-Ziffer dahinter. Vor MF-1333 galt das
 * Vorhandensein von "GEOS format" als Beweis fuer Fassung 2 — gemessen
 * traegt JEDE GEOS-Diskette diese Zeichenkette, auch "GEOS format V1.3",
 * womit jede erkannte Diskette Fassung 2 bekam und bedingungslos
 * `GEOS_PROT_V2_ENHANCED` gemeldet wurde.
 *
 * Die Funktion antwortet deshalb jetzt auf die Frage, die sie beantworten
 * KANN: traegt die Kennung eine Fassungsziffer >= 2? */
bool uft_geos_detect_extended_signature(const uint8_t *sector_data, size_t size) {
    if (!uft_geos_detect_boot_signature(sector_data, size)) return false;
    if (size < (size_t)(GEOS_ID_VERSION_AT + 1)) return false;

    uint8_t ziffer = sector_data[GEOS_ID_VERSION_AT];
    return ziffer >= (uint8_t)'2' && ziffer <= (uint8_t)'9';
}

int uft_geos_detect_file_type(const uint8_t *info_sector) {
    if (!info_sector) return GEOS_TYPE_NON_GEOS;
    
    /* GEOS info sector structure:
     * Offset 0x00: Info block ID ($00)
     * Offset 0x01: Icon bitmap (63 bytes)
     * Offset 0x40: File type
     * Offset 0x41: GEOS file type
     * Offset 0x42: Structure type (SEQ/VLIR)
     */
    
    if (info_sector[0] != 0x00) {
        return GEOS_TYPE_NON_GEOS;
    }
    
    return info_sector[0x41];
}

/* ============================================================================
 * Main Detection Function
 * ============================================================================ */

int uft_geos_analyze_disk(const uft_disk_image_t *disk, 
                          geos_analysis_result_t *result) {
    if (!disk || !result) return UFT_ERR_INVALID_PARAM;
    
    memset(result, 0, sizeof(*result));

    /* ── 1. Den BAM-Sektor holen: Spur 18, Sektor 0 ─────────────────
     *
     * `track_data` ist 0-basiert, Spur 18 liegt also bei Index 17 — so
     * stand es auch vorher, mit einem Kommentar "Track 18" dabei.
     *
     * Der Sektor wird ueber seine ID gesucht und nicht als
     * `sectors[0]` genommen: "der erste im Feld" und "Sektor 0" sind
     * zwei Aussagen, und ein Leser, der Sektoren in Lesereihenfolge
     * ablegt, liefert sie in beliebiger Folge.
     */
    const uft_sector_t *bam = NULL;
    if (disk->track_count >= (size_t)GEOS_BAM_TRACK) {
        const uft_track_t *bam_track = disk->track_data[GEOS_BAM_TRACK - 1];
        if (bam_track && bam_track->sectors) {
            for (size_t i = 0; i < bam_track->sector_count; i++) {
                if (bam_track->sectors[i].id.sector == GEOS_BAM_SECTOR) {
                    bam = &bam_track->sectors[i];
                    break;
                }
            }
        }
    }

    if (!bam || !bam->data || bam->data_len < GEOS_BAM_SECTOR_SIZE) {
        /* Ohne BAM-Sektor ist die Frage nicht beantwortet, nicht
         * verneint. Genau dafuer gibt es das Feld. */
        result->unmessbar |= (uint32_t)GEOS_UNMESSBAR_BAM;
        return UFT_OK;
    }

    /* ── 2. Ist es eine GEOS-Diskette? ──────────────────────────────
     * Die Pruefung, die GEOS selbst anstellt (GEOS.TXT:247-249).
     *
     * Das Ergebnis steht bewusst in einer eigenen Variablen, statt den
     * Aufruf in die `if`-Bedingung zu setzen: das Stummel-Tor in
     * `scripts/check_consistency.py:476` sucht mit
     * `\b(uft_\w+)\s*\([^;{]*\)\s*\{([^{}]*)\}` nach Funktionen, deren
     * Rumpf nur `return UFT_OK;` ist — und kann einen AUFRUF in einem
     * `if` nicht von einer DEFINITION unterscheiden. Gemessen meldete
     * es diese Funktion als Lazy-Stub, obwohl ihr Rumpf zwei Schranken
     * und ein `memcmp` traegt. Der Torfehler steht als P3-541; hier
     * liest die Zeile ohnehin besser. */
    const bool ist_geos_diskette =
        uft_geos_detect_boot_signature(bam->data, bam->data_len);

    /* Beantwortet: keine GEOS-Diskette, und nichts blieb offen —
     * `unmessbar` bleibt 0. */
    if (!ist_geos_diskette) return UFT_OK;

    result->is_geos_disk = true;

    /* Die Fassung ist eine ASCII-Ziffer hinter "GEOS format V"
     * (GEOS.TXT:232). Keine Ziffer heisst 0 = unbekannt, nicht 1. */
    {
        uint8_t ziffer = bam->data[GEOS_ID_VERSION_AT];
        if (ziffer >= (uint8_t)'1' && ziffer <= (uint8_t)'9')
            result->geos_version = (int)(ziffer - (uint8_t)'0');
        else
            result->geos_version = 0;
    }

    /* Kennung an fester Stelle getroffen. */
    result->konfidenz = 20;

    /* ── 3. Was hier prinzipiell nicht entschieden werden kann ──────
     *
     * Der Schutz liegt in den Luecken zwischen den Sektoren von
     * Spur 21. Sektordaten tragen sie nicht — das ist keine Luecke im
     * Code, sondern eine Eigenschaft der Eingabe. Und ohne die
     * Blockkette des GEOS KERNAL laesst sich "in der BAM belegt, aber
     * von keinem Verzeichniseintrag referenziert" nicht entscheiden.
     */
    result->unmessbar |= (uint32_t)GEOS_UNMESSBAR_GAPS;
    result->unmessbar |= (uint32_t)GEOS_UNMESSBAR_VERZEICHNIS;

    /* ── 4. Was sich messen LAESST: die Belegung der Schutzspur ─────
     *
     * Der Urheber: "Der Track $15 ist in der BAM geschuetzt". Eine
     * vollstaendig belegte Spur 21 auf einer GEOS-Diskette ist damit
     * ein Indiz — kein Beweis, und der Bericht sagt beides.
     *
     * Die Sektorzahl kommt aus `uft_cbm_sectors_per_track()` und wird
     * NICHT hier nachgerechnet: eine Groesse, eine Rechnung (MF-1177).
     * Der Baum hatte davon bereits drei Kopien.
     */
    {
        int n = uft_cbm_sectors_per_track(UFT_CBM_1541, GEOS_SCHUTZSPUR);
        size_t eintrag = (size_t)GEOS_BAM_ENTRY(GEOS_SCHUTZSPUR);

        if (n > 0 && eintrag < bam->data_len) {
            result->spur21_bam_gelesen = true;
            result->spur21_sektoren    = (uint8_t)n;
            result->spur21_frei        = bam->data[eintrag];

            if (result->spur21_frei == 0 &&
                result->protection_count < GEOS_MAX_PROTECTIONS) {
                result->protections[result->protection_count++] =
                    GEOS_PROT_BAM_SIGNATURE;
                /* Kennung (20) + belegte Schutzspur (30). Der Deckel
                 * liegt bei 60, solange die Gaps fehlen. */
                result->konfidenz = 50;
            }
        }
    }

    /* ── 5. Was ausdruecklich NICHT mehr gemeldet wird ──────────────
     *
     * `GEOS_PROT_V1_KEY_DISK` kam aus `track_count > 35` — einer reinen
     * Daseinsabfrage, die kein Byte der Spur liest. `GEOS_PROT_V2_
     * ENHANCED` kam bedingungslos aus einer Fassungsnummer. Beide
     * bleiben in der Aufzaehlung und in `g_geos_protections[]` stehen
     * (MF-1077) und warten auf die Gap-Ebene; bis dahin sagt
     * `GEOS_UNMESSBAR_GAPS`, dass hier nicht hingesehen werden konnte.
     *
     * Ebenso `GEOS_PROT_INTERLEAVE` und `GEOS_PROT_HALF_TRACK`: beide
     * brauchen die rohe Spur bzw. den Fluss.
     */

    return UFT_OK;
}

/* ============================================================================
 * Information Functions
 * ============================================================================ */

const geos_protection_info_t* uft_geos_get_protection_info(
    geos_protection_type_t type) {
    
    for (size_t i = 0; i < sizeof(g_geos_protections) / sizeof(g_geos_protections[0]); i++) {
        if (g_geos_protections[i].type == type) {
            return &g_geos_protections[i];
        }
    }
    
    return NULL;
}

int uft_geos_get_report(const geos_analysis_result_t *result,
                        char *buffer, size_t buffer_size) {
    if (!result || !buffer) return UFT_ERR_INVALID_PARAM;
    
    size_t offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "════════════════════════════════════════════════════════════════\n"
        "                    GEOS DISK ANALYSIS\n"
        "════════════════════════════════════════════════════════════════\n\n");
    
    if (!result->is_geos_disk) {
        /* MF-1333: der Grund gehoert dazu. "Keine GEOS-Diskette" und
         * "der BAM-Sektor fehlte" sind zwei verschiedene Aussagen. */
        if (result->unmessbar & (uint32_t)GEOS_UNMESSBAR_BAM) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "NICHT ENTSCHIEDEN: der BAM-Sektor (Spur 18, Sektor 0)\n"
                "lag nicht oder nicht vollstaendig vor. GEOS prueft genau\n"
                "dort; ohne ihn ist die Frage offen, nicht verneint.\n");
        } else {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "Keine GEOS-Diskette.\n"
                "Im BAM-Sektor steht bei $AD nicht \"GEOS format\" — das\n"
                "ist die Pruefung, die GEOS selbst anstellt\n"
                "(docs/format_specs/commodore/GEOS.TXT:247).\n");
        }
        return (int)offset;
    }

    if (result->geos_version > 0) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "GEOS-Diskette:      JA (Kennung bei $AD getroffen)\n"
            "GEOS-Fassung:       %d.x\n"
            "Schutzbefunde:      %d\n"
            "Konfidenz:          %u von 100\n\n",
            result->geos_version, result->protection_count,
            (unsigned)result->konfidenz);
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "GEOS-Diskette:      JA (Kennung bei $AD getroffen)\n"
            "GEOS-Fassung:       unbekannt (keine Ziffer bei $BA)\n"
            "Schutzbefunde:      %d\n"
            "Konfidenz:          %u von 100\n\n",
            result->protection_count, (unsigned)result->konfidenz);
    }

    /* Die gemessene Zahl gehoert in den Bericht, nicht nur das Urteil. */
    if (result->spur21_bam_gelesen) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "Schutzspur 21:      %u von %u Sektoren frei laut BAM\n\n",
            (unsigned)result->spur21_frei,
            (unsigned)result->spur21_sektoren);
    }

    if (result->protection_count == 0) {
        /* MF-1333: hier stand "No copy protection detected. This disk
         * can be copied with standard tools." Das war die gefaehrlichste
         * Zeile der Datei — sie sprach ein Urteil ueber etwas aus, das
         * auf dieser Ebene gar nicht gemessen werden kann. */
        offset += snprintf(buffer + offset, buffer_size - offset,
            "Kein Schutzmerkmal in den SEKTORDATEN gefunden.\n"
            "Das ist KEINE Entwarnung: siehe \"Nicht gemessen\" unten.\n\n");
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "Detected Protections:\n"
            "────────────────────────────────────────────────────────────────\n");
        
        for (size_t i = 0; i < result->protection_count && offset < buffer_size; i++) {
            const geos_protection_info_t *info = 
                uft_geos_get_protection_info(result->protections[i]);
            
            if (info) {
                const char *severity_str;
                switch (info->severity) {
                    case GEOS_SEV_NONE:     severity_str = "None"; break;
                    case GEOS_SEV_TRIVIAL:  severity_str = "Trivial"; break;
                    case GEOS_SEV_STANDARD: severity_str = "Standard"; break;
                    case GEOS_SEV_DIFFICULT: severity_str = "Difficult"; break;
                    default: severity_str = "Unknown";
                }
                
                offset += snprintf(buffer + offset, buffer_size - offset,
                    /* MF-509: `%zu`, nicht `%d`. `i` ist `size_t`; auf
                     * Win64 verbraucht `%d` vier statt acht Byte, und
                     * damit verschieben sich ALLE folgenden Argumente.
                     * `info->name` waere dann ein Zeiger von der falschen
                     * Stelle — Absturz oder Muell im Bericht, nicht bloss
                     * eine falsche Zahl. */
                    "\n  [%zu] %s\n"
                    "      Description: %s\n"
                    "      Severity:    %s\n"
                    "      Nibbler:     %s\n"
                    "      Original:    %s\n",
                    i + 1, info->name,
                    info->description,
                    severity_str,
                    info->copyable_with_nibbler ? "Can copy" : "Not needed",
                    info->requires_original ? "Required" : "Not required");
            }
        }
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "\n════════════════════════════════════════════════════════════════\n"
        "                    COPY RECOMMENDATIONS\n"
        "════════════════════════════════════════════════════════════════\n\n");
    
    if (result->protection_count == 0) {
        /* MF-1333: hier stand "Use: uft read --device xum1541 --format
         * d64". Ein solches CLI gibt es nicht — UFT ist GUI-only. Eine
         * Empfehlung, die auf ein nicht existierendes Werkzeug zeigt,
         * ist dieselbe Klasse wie eine erfundene Kennung. */
        offset += snprintf(buffer + offset, buffer_size - offset,
            "Auf Sektorebene spricht nichts gegen eine gewoehnliche\n"
            "D64-Kopie. Wer den Bootschutz ERHALTEN will, braucht eine\n"
            "Ebene, die Luecken speichert (G64/NIB/Fluss) — eine D64\n"
            "kann sie nicht darstellen.\n");
    } else {
        bool needs_nibbler = false;
        bool needs_original = false;
        
        for (int i = 0; i < result->protection_count; i++) {
            const geos_protection_info_t *info = 
                uft_geos_get_protection_info(result->protections[i]);
            if (info) {
                if (info->copyable_with_nibbler) needs_nibbler = true;
                if (info->requires_original) needs_original = true;
            }
        }
        
        if (needs_nibbler) {
            /* MF-1333: auch hier stand eine CLI-Zeile ("uft read
             * --device xum1541 --format g64 --nibtools"). Entfernt aus
             * demselben Grund. */
            offset += snprintf(buffer + offset, buffer_size - offset,
                "Empfohlen: eine Ebene, die Luecken traegt (G64, NIB oder\n"
                "Fluss). Eine D64 verliert sie ohne Warnung.\n\n");
        }

        if (needs_original) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "Hinweis: fuer volle Funktion kann die Originaldiskette\n"
                "noetig sein; auf Kopien koennen Schutzabfragen fehlschlagen.\n");
        }
    }

    /* ── Was NICHT gemessen wurde ────────────────────────────────────
     *
     * Der wichtigste Absatz des Berichts. Ohne ihn liest sich
     * "0 Schutzbefunde" als Entwarnung, und genau das waere eine
     * erfundene Aussage (MF-1311: eine Null ist mehrdeutig).
     */
    if (result->unmessbar != (uint32_t)GEOS_UNMESSBAR_NICHTS) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "\n────────────────────────────────────────────────────────────────\n"
            "                    NICHT GEMESSEN\n"
            "────────────────────────────────────────────────────────────────\n\n");

        if (result->unmessbar & (uint32_t)GEOS_UNMESSBAR_GAPS) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "* Der eigentliche GEOS-Bootschutz liegt in den LUECKEN\n"
                "  zwischen den Sektoren von Spur 21 (Original:\n"
                "  $55 $55 $67 ..., GeoCopy: durchgehend $67). Sektordaten\n"
                "  tragen sie nicht; es braucht die rohe GCR-Spur.\n");
        }
        if (result->unmessbar & (uint32_t)GEOS_UNMESSBAR_VERZEICHNIS) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "* Die Blockkette des GEOS KERNAL wurde nicht verfolgt.\n"
                "  Ohne sie laesst sich \"Spur 21 in der BAM belegt, aber\n"
                "  von keinem Verzeichniseintrag referenziert\" nicht\n"
                "  entscheiden — das Merkmal einer GeoCopy-Rekonstruktion.\n");
        }
        if (result->unmessbar & (uint32_t)GEOS_UNMESSBAR_BAM) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "* Der BAM-Sektor lag nicht vollstaendig vor.\n");
        }
    }

    return (int)offset;
}

/* ============================================================================
 * GEOS File Analysis
 * ============================================================================ */

int uft_geos_analyze_file(const uint8_t *info_sector, size_t size,
                          geos_file_info_t *info) {
    if (!info_sector || !info || size < 256) return UFT_ERR_INVALID_PARAM;
    
    memset(info, 0, sizeof(*info));
    
    /* Check if this is a GEOS file */
    if (info_sector[0] != 0x00) {
        info->is_geos_file = false;
        return UFT_OK;
    }
    
    info->is_geos_file = true;
    
    /* Parse info sector */
    /* Offset 0x01-0x3F: Icon bitmap (63 bytes) */
    memcpy(info->icon_data, &info_sector[0x01], 63);
    
    /* Offset 0x40: DOS file type */
    info->dos_file_type = info_sector[0x40];
    
    /* Offset 0x41: GEOS file type */
    info->geos_file_type = info_sector[0x41];
    
    /* Offset 0x42: Structure type */
    info->structure_type = info_sector[0x42];
    
    /* Offset 0x43-0x44: Load address */
    info->load_address = info_sector[0x43] | (info_sector[0x44] << 8);
    
    /* Offset 0x45-0x46: End address */
    info->end_address = info_sector[0x45] | (info_sector[0x46] << 8);
    
    /* Offset 0x47-0x48: Start address */
    info->start_address = info_sector[0x47] | (info_sector[0x48] << 8);
    
    /* Offset 0x49-0x5C: Class name (20 bytes) */
    memcpy(info->class_name, &info_sector[0x49], 20);
    info->class_name[20] = '\0';
    
    /* Offset 0x5D-0x74: Author (24 bytes) */
    memcpy(info->author, &info_sector[0x5D], 24);
    info->author[24] = '\0';
    
    /* Offset 0x75-0x88: Parent application (20 bytes) - for documents */
    memcpy(info->parent_app, &info_sector[0x75], 20);
    info->parent_app[20] = '\0';
    
    /* Offset 0x89-0xA8: Description (32 bytes) */
    memcpy(info->description, &info_sector[0x89], 32);
    info->description[32] = '\0';
    
    return UFT_OK;
}

const char* uft_geos_file_type_name(int type) {
    switch (type) {
        case GEOS_TYPE_NON_GEOS:    return "Non-GEOS";
        case GEOS_TYPE_BASIC:       return "BASIC";
        case GEOS_TYPE_ASSEMBLER:   return "Assembler";
        case GEOS_TYPE_DATA:        return "Data";
        case GEOS_TYPE_SYSTEM:      return "System";
        case GEOS_TYPE_DESK_ACC:    return "Desk Accessory";
        case GEOS_TYPE_APPLICATION: return "Application";
        case GEOS_TYPE_PRINTER:     return "Printer Driver";
        case GEOS_TYPE_INPUT:       return "Input Driver";
        case GEOS_TYPE_DISK:        return "Disk Driver";
        case GEOS_TYPE_BOOT:        return "Boot";
        case GEOS_TYPE_TEMP:        return "Temporary";
        case GEOS_TYPE_AUTO_EXEC:   return "Auto-Exec";
        case GEOS_TYPE_DIRECTORY:   return "Directory";
        case GEOS_TYPE_FONT:        return "Font";
        case GEOS_TYPE_DOCUMENT:    return "Document";
        default:                    return "Unknown";
    }
}
