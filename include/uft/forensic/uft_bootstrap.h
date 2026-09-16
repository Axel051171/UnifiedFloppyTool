/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_bootstrap.h
 * @brief Herkunft des DATENTRAEGERS aus seinem Bootstrap-Code (P3-454).
 *
 * WAS DAS IST UND WAS ES NICHT IST
 * --------------------------------
 * `src/forensic/uft_provenance.c` fuehrt die Herkunft von DATEIEN —
 * Werkzeug, Fassung, Weg, Lizenz einer Korpusdatei. Das hier ist die
 * Herkunft des **Datentraegers**: welches Werkzeug hat diese Diskette
 * formatiert? Dieselbe Frage, anderes Objekt.
 *
 * Der Schluessel dafuer steht auf der Diskette selbst. Ein PC-Bootsektor
 * traegt hinter BPB und Sprungbefehl den **Bootstrap-Code**, und der ist
 * je Formatierer verschieden. Eine CRC-32 darueber ist damit ein
 * Fingerabdruck des Werkzeugs; der OEM-Name im Kopf ist der Pruefwert
 * daneben.
 *
 * BENANNTE REFERENZ (EINFRIER-REGEL (c))
 * --------------------------------------
 * `Modules/BootstrapDB.vb` aus DiskImageTool (Digitoxin1, **GPL-3.0**),
 * im Baum unter `tools/uft-scout/work/DiskImageTool/`, 213 Zeilen —
 * **gelesen, nicht uebernommen** (MF-1187). Das Verfahren dort:
 *
 *   Private _OEMNameDictionary As Dictionary(Of UInteger, BootstrapLookup)
 *   Dim Checksum = CRC32.ComputeChecksum(BootstrapCode)
 *   If _OEMNameDictionary.ContainsKey(Checksum) Then …
 *
 * Also: CRC-32 ueber den Bootstrap-Code als **Schluessel** (32 Bit),
 * OEM-Name als **Pruefwert**. Die Win9x-Kennung — „IHC" an den
 * Positionen 5..7 des OEM-Namens — steht in `BootSector.vb:306`.
 *
 * DREI ZUSTAENDE, UND DER DRITTE IST DER PUNKT
 * --------------------------------------------
 * „Der Bestand kennt diesen Schluessel nicht" ist **nicht** dasselbe wie
 * „das Werkzeug ist unbekannt". Das erste ist eine Aussage ueber UNSEREN
 * Bestand, das zweite eine ueber die Diskette. MF-980 hat genau diese
 * Verwechslung an `0xE5` behoben („das Format sagt 0xE5" und „hier wurde
 * 0xE5 gelesen" sind zwei Aussagen), D6 verlangt dieselbe Unterscheidung
 * fuer leere Spalten.
 *
 * DER BESTAND IST HEUTE LEER, UND DER GRUND IST NICHT, DASS ER FEHLT
 * ------------------------------------------------------------------
 * **BERICHTIGT beim Schreiben dieser Datei (MF-1189).** Hier stand, die
 * Originaldatenbank „liegt **nicht** vor". Das ist falsch, und die
 * Messung war einen Befehl weit:
 * `tools/uft-scout/work/DiskImageTool/DiskImageTool/Resources/bootstrap.xml`
 * ist **60 251 Byte** gross und enthaelt **379** `<bootstrap>`-Elemente,
 * jedes mit `crc32=` und `jmp=`, dazu **490** `<oemname>`-Elemente, von
 * denen **223** `verified="true"` tragen. Ein Element sieht so aus:
 *
 *   <bootstrap crc32="FA8F7D48" jmp="EB3490">
 *     <oemname namehex="0000000000000000" verified="true"/>
 *   </bootstrap>
 *
 * **Die beiden Zahlen widersprechen sich nicht, sie zaehlen
 * Verschiedenes** — 379 Schluessel, 490 Namen, weil ein Schluessel
 * mehrere OEM-Namen tragen kann. Zwei Zahlen ohne Nenner in einem Satz
 * waeren MF-1015 in Reinform, deshalb stehen sie hier mit ihrem
 * Elementnamen.
 *
 * **Warum der Bestand trotzdem leer ist: S3, nicht Abwesenheit.** Die
 * Datei liegt im geklonten Fremdbaum und ist **gitignoriert**
 * (`tools/uft-scout/.gitignore:2: work/`, gemessen: `git ls-files`
 * kennt sie nicht). Sie zu uebernehmen ist eine Entscheidung mit ZWEI
 * Gruenden, und keiner davon liegt bei mir:
 *   1. DiskImageTool ist **GPL-3.0** — nach MF-698 bindet das die
 *      verteilbare Fassung des Gesamtwerks an GPL-3.
 *   2. Ein gepflegter Bestand von 379 Eintraegen ist eine **Datenbank**
 *      im Sinne des Leistungsschutzes (EU-Datenbankherstellerrecht,
 *      §§ 87a ff. UrhG) — unabhaengig von der Codelizenz. Dieselbe Lage
 *      wie die OmniFlop-Ernte der 239 Formate, die aus genau diesem
 *      Grund auf eine Eigentuemer-Entscheidung wartet.
 *
 * Bis diese Entscheidung vorliegt (P3-454) gibt `uft_bs_bestand_groesse()`
 * **0** zurueck, `UFT_BS_TRAEGER_ZUGEORDNET` ist unerreichbar, und genau
 * das sagt die Funktion einem Aufrufer, der es wissen will. **Es werden
 * keine Eintraege erfunden — und keine abgeschrieben.**
 *
 * EINE RECHNUNG, NICHT ZWEI (MF-1177)
 * -----------------------------------
 * Die CRC-32 kommt aus `air_crc32_buffer()`
 * (`include/uft/formats/uft_air_crc32.h`) und wird hier **nicht**
 * nachgebaut. Gemessen MF-1189: das Polynom `0xEDB88320` steht in **22**
 * Dateien des Baums mit **29** Vorkommen, und es gibt **vier**
 * CRC-Header. Eine weitere Fassung anzulegen waere MF-1177 in Reinform
 * („eine Groesse, eine Rechnung"). Der Test
 * `tests/test_traegerprovenienz.c` rechnet den Schluessel gegen
 * `air_crc32_buffer()` nach — waere es eine andere Rechnung, traefe er
 * die Datenbank nie.
 *
 * Hinweis zur Lizenz: `uft_air_crc32.h` ist ein erklaerter Port aus dem
 * AIR-Projekt und `GPL-3.0-only`. Diese Datei ist `GPL-2.0-or-later`; die
 * Kombination ist zulaessig und bindet die verteilbare Fassung des
 * Gesamtwerks an GPL-3 — die Eigentuemer-Entscheidung dazu ist MF-698.
 */

#ifndef UFT_BOOTSTRAP_H
#define UFT_BOOTSTRAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_BS_SEKTOR_LEN   512u
#define UFT_BS_OEM_LEN        8u

/**
 * Lage der Traegerherkunft. Drei Zustaende, und sie sind nicht
 * austauschbar — siehe Kopfkommentar.
 */
typedef enum uft_bs_traeger {
    /** Kein PC-Bootsektor, oder der Bootstrap-Bereich ist leer. Es gibt
     *  keinen Schluessel, also auch keine Aussage. */
    UFT_BS_TRAEGER_UNBEKANNT = 0,
    /** Ein Schluessel ist GEBILDET, der Bestand kennt ihn nicht. Das ist
     *  eine Aussage ueber unseren Bestand, nicht ueber die Diskette. */
    UFT_BS_TRAEGER_GEMESSEN,
    /** Ein Schluessel ist gebildet UND im Bestand gefunden. Heute
     *  unerreichbar, weil der Bestand leer ist — S3, siehe
     *  Kopfkommentar (P3-454). */
    UFT_BS_TRAEGER_ZUGEORDNET
} uft_bs_traeger_t;

/** Ergebnis der Traegerbestimmung. */
typedef struct uft_bs_traeger_id {
    uft_bs_traeger_t lage;
    uint32_t    crc32;          /**< 0 = nicht gebildet                 */
    size_t      code_offset;    /**< Beginn des Codes im Sektor         */
    size_t      code_len;       /**< Laenge OHNE nachlaufende Nullen    */
    char        oem[UFT_BS_OEM_LEN + 1]; /**< 8 Byte + Abschluss        */
    bool        oem_ist_win9x;  /**< „IHC" an 5..7 (BootSector.vb:306)  */
    /** Name des Werkzeugs, oder NULL. **NULL heisst „dieser Bestand
     *  kennt ihn nicht", nicht „unbekanntes Werkzeug".** */
    const char *werkzeug;
} uft_bs_traeger_id_t;

/**
 * Grenzt den Bootstrap-Code im Sektor ab.
 *
 * Der Sprungbefehl sagt, wo der Code beginnt — `EB xx 90` (kurz, Ziel
 * `2 + xx`) oder `E9 xx xx` (nah, Ziel `3 + Wort`); alles andere ist kein
 * PC-Bootsektor. Hinten wird die Kennung `0x55AA` abgeschnitten, wenn sie
 * da ist, und danach die **nachlaufenden Nullen** — sonst haengt der
 * Schluessel daran, wie viel Fuellung der Formatierer geschrieben hat.
 *
 * @return true, wenn ein nicht-leerer Bereich bestimmt wurde.
 */
bool uft_bs_bootstrap_bereich(const uint8_t *sektor, size_t sektor_len,
                              size_t *offset_aus, size_t *len_aus);

/** „IHC" an den Positionen 5..7 des OEM-Namens (BootSector.vb:306). */
bool uft_bs_oem_ist_win9x(const uint8_t *oem, size_t oem_len);

/**
 * Bestimmt die Traegerherkunft aus einem Bootsektor.
 *
 * `aus` wird in JEDEM Fall beschrieben, auch bei Rueckgabe false — dann
 * mit `UFT_BS_TRAEGER_UNBEKANNT` und Nullwerten. Ein Aufrufer, der den
 * Rueckgabewert ignoriert, liest also keine Altwerte.
 *
 * @return true, wenn ein Schluessel gebildet wurde (Lage GEMESSEN oder
 *         ZUGEORDNET); false, wenn nicht (Lage UNBEKANNT).
 */
bool uft_bs_identifiziere(const uint8_t *sektor, size_t sektor_len,
                          uft_bs_traeger_id_t *aus);

/**
 * Zahl der Eintraege im Bestand.
 *
 * **Heute 0**, und das ist kein Fehler, sondern der Stand: die
 * Originaldatenbank (379 Schluessel) liegt vor, darf aber ohne
 * Eigentuemer-Entscheidung nicht uebernommen werden (S3, P3-454; siehe
 * Kopfkommentar). Ein Aufrufer, der wissen will, ob eine
 * Nicht-Zuordnung etwas bedeutet, fragt hier.
 */
size_t uft_bs_bestand_groesse(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BOOTSTRAP_H */
