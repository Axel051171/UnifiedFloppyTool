/**
 * @file uft_win98_fdb.h
 * @brief MSWIN4.1-Floppy-Bootrecord erkennen und benennen (MF-1181).
 *
 * @section QUELLE
 *
 * Daniel B. Sedory, „MSWIN4.1 (Windows 98) Floppy Disk Boot Record",
 * https://daniel.sedory.com/asm/mbr/WIN98FDB.htm
 * (Revised 29.04.2003, Update 27.04.2005, Updated 05.04.2009).
 * Text und Darstellung (c) 2001-2005 Daniel B. Sedory, ausdruecklich
 * „NOT to be reproduced in any form without Permission of the Author".
 *
 * Kanal *Spec* (MF-695): die Seite ist GELESEN. Uebernommen sind
 * BYTELAGEN — Tatsachen ueber ein Format, keine Textpassagen, keine
 * Disassemblierung und kein Bootcode. Jede Zahl unten steht mit ihrer
 * Stelle in der Quelle daneben, damit sie nachschlagbar ist.
 *
 * Die Seite sagt ausserdem etwas, das den Geltungsbereich festlegt:
 * derselbe Bootcode liegt auf Windows 98, 98 SE, ME UND den
 * Windows-XP-Startdisketten. „MSWIN4.1" ist also keine Aussage ueber
 * die Windows-Fassung.
 *
 * @section WAS_HIER_NICHT_STEHT
 *
 * **Kein zweiter BPB.** Der BIOS-Parameter-Block und die daraus
 * gerechnete Aufteilung stehen im Baum genau einmal, in
 * `fat_analysis_result_t` (`uft_fat_bootsector.h`), abgenommen von
 * `tests/test_fat_bootsector.c`. Diese Struktur HAELT einen, sie baut
 * keinen — eine zweite Kopie waere die Lage aus MF-1015, und die
 * Zulieferung, aus der dieses Modul stammt, hatte genau sie: 22 ihrer
 * 23 BPB-Felder gab es schon.
 *
 * **Keine Plugin-Registrierung.** Das Moratorium der EINFRIER-REGEL
 * gilt gemessen weiter (`nfd` steht auf T2). Dieses Modul erkennt und
 * benennt; es tritt nicht in die Formatregistratur ein. Dieselbe
 * Behandlung wie `uft_roland_identify()` seit MF-1176.
 *
 * SPDX-License-Identifier: MIT
 * Eigenstaendige Umsetzung nach der oben genannten Beschreibung; es ist
 * KEINE Ableitung fremden Codes (MF-636: eine Attribution ist eine
 * rechtliche Aussage, also steht hier, was wirklich zutrifft).
 */

#ifndef UFT_WIN98_FDB_H
#define UFT_WIN98_FDB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/formats/fat/uft_fat_bootsector.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fassung des Berichts-Aufbaus. Steigt, wenn Felder wegfallen. */
#define UFT_WIN98_FDB_SCHEMA_VERSION 2u

/** Ein Bootrecord ist genau ein Sektor. */
#define UFT_WIN98_FDB_SECTOR_SIZE 512u

/** Hoechstzahl gemeldeter Auffaelligkeiten; darueber wird GEKUERZT und
 *  das Kuerzen GEMELDET (`warnings_truncated`) — nie stillschweigend. */
#define UFT_WIN98_FDB_MAX_WARNINGS 16u

/* ── Bytelagen, jede mit ihrer Stelle in der Quelle ─────────────────
 *
 * Alle Angaben sind Versaetze im 512-Byte-Sektor.
 *
 *   0x000  EB 3C 90         Sprung + NOP        (Quelle: Hexdump Z. 0000,
 *                                                und „the third byte has
 *                                                always been a 90h")
 *   0x003  "MSWIN4.1"       OEM System Name     (Quelle: Hexdump 0000,
 *                                                gelb hervorgehoben)
 *   0x00B  BPB              bis 0x023           (Quelle: „Beginning at
 *                                                offset 0Bh")
 *   0x026  0x29             Extended-BPB-Marke  (Quelle: BPB-Tafel)
 *   0x03E  Bootcode         Anfang              (Quelle: „Most of the
 *                                                code is between offsets
 *                                                3Eh through 17Eh")
 *   0x17E  Bootcode         Ende (einschl.)     (dieselbe Stelle)
 *   0x17F  Datenregister    vier Byte           (Quelle: „followed by
 *                                                some Data Registers")
 *   0x183  Fehlermeldungen  DREI, lokalisiert   (Quelle: „three error
 *                                                messages", Hexdump
 *                                                7D80..7DD5)
 *   0x1D8  "IO      SYS"    11 Byte             (Quelle: Hexdump 7DD8)
 *   0x1E3  "MSDOS   SYS"    11 Byte             (Quelle: Hexdump 7DE3)
 *   0x1F1  Unterprogramm    bis 0x1FB           (Quelle: „a subroutine
 *                                                at offsets 1F1h
 *                                                through 1FBh")
 *   0x1FE  55 AA            Signatur            (Quelle: „The sector
 *                                                ends as usual with ...
 *                                                AA55 hex")
 */
#define UFT_WIN98_FDB_OEM_OFFSET        0x003u
#define UFT_WIN98_FDB_OEM_NAME          "MSWIN4.1"
#define UFT_WIN98_FDB_CODE_BEGIN        0x03Eu
#define UFT_WIN98_FDB_CODE_END          0x17Fu   /* exklusiv */
#define UFT_WIN98_FDB_MSG_BEGIN         0x183u
#define UFT_WIN98_FDB_IO_SYS_OFFSET     0x1D8u
#define UFT_WIN98_FDB_MSDOS_SYS_OFFSET  0x1E3u
#define UFT_WIN98_FDB_SUBROUTINE_BEGIN  0x1F1u
#define UFT_WIN98_FDB_SUBROUTINE_END    0x1FCu   /* exklusiv */

typedef enum {
    UFT_WIN98_FDB_OK = 0,
    UFT_WIN98_FDB_INVALID_ARG,
    UFT_WIN98_FDB_TOO_SHORT,
    UFT_WIN98_FDB_UNRECOGNIZED,
    UFT_WIN98_FDB_IO_ERROR
} uft_win98_fdb_status_t;

typedef enum {
    UFT_WIN98_FDB_WARN_NONE = 0,
    UFT_WIN98_FDB_WARN_BPB,        /**< BPB unplausibel */
    UFT_WIN98_FDB_WARN_LAYOUT,     /**< Aufteilung geht nicht auf */
    UFT_WIN98_FDB_WARN_SIZE,       /**< Abbildgroesse passt nicht zum BPB */
    UFT_WIN98_FDB_WARN_BOOTCODE    /**< Bootcode weicht ab */
} uft_win98_fdb_warning_kind_t;

typedef struct {
    uft_win98_fdb_warning_kind_t kind;
    char message[128];
} uft_win98_fdb_warning_t;

typedef struct {
    uint32_t schema_version;
    size_t   source_size;          /**< Groesse der Eingabe, wie uebergeben */

    /** Der BPB und die gerechnete Aufteilung — GEHALTEN, nicht
     *  nachgebaut. Gefuellt von `fat_analyze_boot_sector()`. */
    fat_analysis_result_t fat;
    bool fat_ok;                   /**< der Analysator hat zugestimmt */

    /* ── nur MSWIN4.1 ─────────────────────────────────────────────── */
    bool has_oem_mswin41;          /**< „MSWIN4.1" bei 0x003 */
    bool has_io_sys_marker;        /**< „IO      SYS" bei 0x1D8 */
    bool has_msdos_sys_marker;     /**< „MSDOS   SYS" bei 0x1E3 */

    /** FNV-1a-64 ueber den CODEBEREICH 0x03E..0x17E — und AUSDRUECKLICH
     *  nicht darueber hinaus. Der Bereich 0x183..0x1D5 traegt laut
     *  Quelle drei Fehlermeldungen, und die sind LOKALISIERT; ein
     *  Fingerabdruck, der sie mitnimmt, kann bei einer deutschen oder
     *  franzoesischen Startdiskette nie treffen. Die Zulieferung, aus
     *  der dieses Modul stammt, hashte 0x03E..0x1FD und hatte damit
     *  genau diesen stillen Fehlschlag (gemessen MF-1181). */
    uint64_t code_fingerprint_fnv1a64;
    /** Zweiter Abdruck, nur ueber das Unterprogramm 0x1F1..0x1FB. Er
     *  steht getrennt, weil die Quelle die beiden Codebereiche
     *  getrennt benennt und ein zusammengefasster Abdruck nicht sagen
     *  koennte, welcher der beiden abweicht. */
    uint64_t tail_fingerprint_fnv1a64;

    bool image_size_matches_bpb;   /**< Dateigroesse == BPB-Gesamtgroesse */

    /** Konfidenz NACH DER DOKTRIN (MF-1153), gebildet von
     *  `uft_probe_konfidenz()`. Keine hier vergebene Zahl. */
    int confidence;

    uft_win98_fdb_warning_t warnings[UFT_WIN98_FDB_MAX_WARNINGS];
    size_t warning_count;
    bool   warnings_truncated;
} uft_win98_fdb_report_t;

/**
 * Sonde: traegt dieser Puffer einen MSWIN4.1-Bootrecord?
 *
 * Die Konfidenz kommt aus `uft_probe_konfidenz()` und NICHT aus einer
 * hier gewaehlten Zahl. Belege in dieser Reihenfolge:
 *
 *   KENNUNG          „MSWIN4.1" bei 0x003 — formatspezifisch und an
 *                    fester Stelle. NUR das ist hier eine Kennung.
 *   SELBSTKONSISTENZ der BPB rechnet die uebergebene Groesse auf.
 *   STRUKTUR         beide Dateinamen an ihrer berechneten Stelle.
 *   GEOMETRIE        der BPB ist plausibel.
 *
 * Was ausdruecklich KEINE Kennung ist: die Signatur 0x55AA bei 0x1FE
 * und der Sprung `EB 3C 90` bei 0x000. Beide stehen auf JEDEM
 * PC-Bootsektor; sie sagen „das ist ein Bootsektor", nicht „das ist
 * dieser hier". Ohne die OEM-Kennung liegt die Obergrenze damit bei
 * 45 — und genau daran ist die Leiter der Zulieferung gescheitert, die
 * mit Sprung + Signatur + IO.SYS bereits 30 erreichte und zustimmte
 * (gemessen MF-1181, Rotbeweis im Test).
 *
 * @param data        Puffer, mindestens 512 Byte.
 * @param size        Groesse des Puffers ODER der ganzen Datei; die
 *                    Selbstkonsistenz-Pruefung benutzt sie, also darf
 *                    sie nicht verworfen werden (MF-1029: fuenfmal in
 *                    diesem Baum war genau das der Defekt).
 * @param confidence_out darf NULL sein.
 * @return true, wenn die OEM-Kennung sitzt. Ohne Kennung NIE true —
 *         eine Absage ist ein Ergebnis.
 */
bool uft_win98_fdb_probe(const uint8_t *data, size_t size,
                         int *confidence_out);

/** Vollstaendiger Bericht. `report` wird immer genullt, auch im
 *  Fehlerfall. */
uft_win98_fdb_status_t uft_win98_fdb_parse(const uint8_t *data, size_t size,
                                          uft_win98_fdb_report_t *report);

/** Wie `uft_win98_fdb_parse`, aber aus einer Datei. Liest GENAU den
 *  ersten Sektor und ermittelt die Dateigroesse fuer die
 *  Selbstkonsistenz. */
uft_win98_fdb_status_t uft_win98_fdb_parse_file(const char *path,
                                               uft_win98_fdb_report_t *report);

const char *uft_win98_fdb_status_name(uft_win98_fdb_status_t status);
const char *uft_win98_fdb_warning_name(uft_win98_fdb_warning_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WIN98_FDB_H */
