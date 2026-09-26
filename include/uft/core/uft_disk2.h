/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2.h
 * @brief Das Zentrum — vier Schichten, jede mit Herkunft und Zuversicht.
 *
 * ── WARUM ES DAS GIBT ───────────────────────────────────────────────────
 *
 * Eine Analysereihe ueber den ganzen Baum hat nicht 88 einzelne Fehler
 * gefunden, sondern EIN fehlendes Zentrum, um das herum 88 Plugins jeweils
 * ihre eigene Kopie desselben gebaut haben:
 *
 *   uft_flux_revolution_t in 2 Headern, 0 .c     -> kein Umdrehungsmodell
 *   Metadaten in 3 unvereinbaren Strukturen       -> kein Herkunftsmodell
 *   FAT-Kettenlaeufer x2, "SINCLAIR" x4, 737280 x9 -> kein gemeinsamer Kern
 *   79 FloppyDevice-Module ohne Verteiler         -> zwei Architekturen
 *   88 Sonden, ADF gibt 95 nach 3 Bytes           -> keine Erkennungsschicht
 *   uft_recovery_fusion.c:74 "out of scope"       -> keine Struktur dafuer
 *
 * Dieses Modul ist das Zentrum. Ein Format-Plugin tut danach EINE Sache:
 * seinen Behaelter oeffnen und in die passende Schicht einspeisen. SCP
 * speist Fluss ein, HFE Bitstrom, IMG Sektoren, SCL Dateien. Alles darunter
 * — Erkennung, Geometrie, Fusion, Schutz, Herkunft, Verlustpruefung — ist
 * EINMAL da.
 *
 * ── DIE VIER SCHICHTEN ──────────────────────────────────────────────────
 *
 *   FLUSS        Umdrehungen GETRENNT, Indexzeit je Umdrehung
 *      ↓ PLL     Phasenlage und Flusszahl je Bit werden AUSGEGEBEN
 *   BITSTROM     je Spur; Laenge GEMESSEN, nicht geklemmt; Konfidenz je Bit
 *      ↓ Sucher  ganze Spur, ueber den Index, ueberlappend
 *   SEKTOREN     je Sektor: Groesse, Marke, CRC-Zustand, Herkunft, Konfidenz
 *      ↓ Sonden  Inhalt, nicht Groesse
 *   DATEISYSTEM  Eintraege samt geloeschten, mit Wiederherstellbarkeit
 *
 * Jede Schicht ist OPTIONAL. Ein IMG hat nur Sektoren; ein SCP hat Fluss und
 * alles, was daraus abgeleitet wurde. Welche Schichten da sind, ist eine
 * TATSACHE ueber den Traeger — und eine Wandlung in ein Format, das eine
 * Schicht braucht, die nicht da ist, muss mit benanntem Grund scheitern.
 *
 * ── DIE DREI GRUNDSAETZE ────────────────────────────────────────────────
 *
 *   1. JEDES abgeleitete Objekt weiss, woraus und wodurch es entstand —
 *      und ob diese Quelle seither ERSETZT wurde (Generationen, unten).
 *   2. Zuversicht steigt beim Aufsteigen der Schichten NIE ohne Beleg.
 *      Die Regel wird beim Einspeisen geprueft, an JEDER Schicht.
 *   3. Keine stille Normalisierung. Was gemessen wurde, bleibt; was
 *      fehlt, fehlt und wird gesagt.
 *
 * ── WAS ES NICHT IST ────────────────────────────────────────────────────
 *
 * Kein Leser, kein Schreiber, keine PLL, kein Sektorsucher. Es ist die
 * Struktur, in die diese Dinge einspeisen und aus der sie lesen.
 *
 * ── NEBENLAEUFIGKEIT ────────────────────────────────────────────────────
 *
 * Das Modell ist NICHT nebenlaeufig sicher. Ein Objekt gehoert einem
 * Faden. Wer es teilt, sperrt es aussen. Das steht hier, weil es sonst
 * niemand liest. Im Baum heisst das konkret: `uft_d2_from_disk()` und
 * `DiskAnalyzerWindow::traegerBericht()` (MF-1273) laufen auf dem
 * GUI-Faden, jeder mit seinem eigenen Modell.
 *
 * ── ZWEITE FASSUNG (MF-1274) ────────────────────────────────────────────
 *
 * Was gegenueber MF-1272 dazukam, und warum:
 *
 *   Generationen + validate()   abgeleitete Objekte wissen, ob ihre Quelle
 *                               noch die ist, aus der sie entstanden
 *   Ableitungsregister          Herkunft maschinenlesbar und KOPIERT statt
 *                               als statischer Zeiger — sonst ist sie nicht
 *                               serialisierbar
 *   Stimmen je Bit              „3 von 5 Umdrehungen" ist nachpruefbar,
 *                               „Konfidenz 153" nicht
 *   Mehrere Dateisysteme        Doppelformat, Partitionen, Trackloader UND
 *                               Dateisystem auf verschiedenen Spuren
 *   Kodierung je Sektor         Schutzverfahren mischen INNERHALB der Spur
 *   crosses_index GERECHNET     nicht mehr vom Aufrufer gesetzt
 *   Zuversichtsregel ueberall   auch Eintraege und Umdrehungen
 *   Spurindex                   [cyl][head] statt linearer Suche
 *   Merkmalcache                Schmutzflag statt Neuberechnung
 *   Metadaten dynamisch         mit Ueberlaufmeldung wie die Befunde
 *   Bericht mit Fehlerzahl      auch fuer die, die nicht gezeigt werden
 *
 * ── BERICHTIGT GEGENUEBER DEM ENTWURF (MF-1272, gilt weiter) ────────────
 *
 * Der Entwurf kam als eigenstaendiges Paket. Beim Einbau in DIESEN Baum
 * wurde gemessen, nicht angenommen — und die Messung gilt fuer die zweite
 * Fassung unveraendert, weil der Entwurf dieselben Namen wieder benutzt:
 *
 *   * SIEBEN Namen des Entwurfs gibt es hier schon, mit anderer Bedeutung
 *     (gemessen ueber `git grep` je Bezeichner, ohne die eigenen Dateien):
 *     `uft_encoding_t` (19 Dateien), `UFT_ENC_MFM` (19), `uft_diag_t` (6),
 *     `UFT_LAYER_FLUX` (2 — `uft_unified_image.h` UND `uft_track.h`),
 *     `uft_layer_t` (1), `UFT_CONF_CERTAIN` (1, `uft_protection.h`),
 *     `UFT_FS_FAT12` (1, `uft_integration.h`).
 *     Deshalb tragen ALLE oeffentlichen Namen dieses Moduls das Praefix
 *     `uft_d2_` / `UFT_D2_`.
 *
 *   * Die Kodierung ist NICHT neu definiert: `uft_encoding_t` aus
 *     `uft/uft_types.h` ist der Typ von `uft_track_t.encoding`. Eine zweite
 *     Aufzaehlung daneben waere die Doppelhaltung, gegen die dieser Baum
 *     steht (D3) — und sie waere AERMER: der Entwurf kennt vier Werte
 *     (UNKNOWN/FM/MFM/GCR), der Baum fuehrt gemessen 20, darunter
 *     `UFT_ENC_AMIGA_MFM`, `UFT_ENC_GCR_CBM`, `UFT_ENC_GCR_APPLE_525`,
 *     `UFT_ENC_GCR_APPLE_35`, `UFT_ENC_GCR_VICTOR` und `UFT_ENC_M2FM`.
 *     Ein `UFT_ENC_GCR` gibt es hier NICHT: welches GCR gemeint ist, ist
 *     bei Kopierschutz die entscheidende Frage, und ein Sammelwert haette
 *     sie verschluckt.
 *
 *   * `UFT_D2_CONF_UNVERIFIED` (128) hat einen Namen statt eines Literals.
 *
 *   * `uft_d2_report()` gibt die BENOETIGTE Laenge zurueck (Bauform
 *     `snprintf`). Ein Rueckgabewert >= buflen heisst: gekuerzt.
 *
 *   * Die Sektorzeile des Berichts unterscheidet „CRC falsch" von „CRC nicht
 *     getragen" — ein D64 traegt keine Pruefsumme, und ein Urteil in EINE
 *     Richtung nennt `tests/test_disk_analyzer_no_fiction.cpp` seit MF-662
 *     erfunden.
 *
 *   * Die Befundliste reserviert ihren letzten Platz von Anfang an.
 *
 *   * Sektorgroessen werden nur unter Sektoren MIT Datenfeld verglichen.
 *
 *   * Fehlt der Speicher fuer eine der optionalen Bitstrom-Beilagen,
 *     scheitert `uft_d2_set_bitstream()` als Ganzes.
 */

#ifndef UFT_DISK2_H
#define UFT_DISK2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_types.h"   /* uft_encoding_t — EINE Aufzaehlung, nicht zwei */

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════ Grundbegriffe ══════════════════════════════════ */

typedef enum {
    UFT_D2_LAYER_FLUX = 0,
    UFT_D2_LAYER_BITSTREAM,
    UFT_D2_LAYER_SECTORS,
    UFT_D2_LAYER_FILESYSTEM,
    UFT_D2_LAYER_COUNT
} uft_d2_layer_t;

/**
 * Woher ein Objekt stammt. Dieselbe Idee wie uft_prov_t im Parametermodell,
 * hier fuer Daten statt Einstellungen.
 */
typedef enum {
    UFT_D2_ORIGIN_MEDIUM = 0,     /**< direkt vom Traeger gelesen               */
    UFT_D2_ORIGIN_CONTAINER,      /**< aus einer Abbilddatei                    */
    UFT_D2_ORIGIN_DERIVED,        /**< aus einer tieferen Schicht abgeleitet    */
    UFT_D2_ORIGIN_FUSED,          /**< aus mehreren Lesungen zusammengefuehrt   */
    UFT_D2_ORIGIN_PADDING,        /**< Fuellmaterial, das das Format verlangt   */
    UFT_D2_ORIGIN_RECONSTRUCTED,  /**< erraten oder ergaenzt — ein VERSUCH      */
    UFT_D2_ORIGIN_UNKNOWN
} uft_d2_origin_t;

/** 0..255. 255 = direkt vom Traeger, CRC stimmt. 0 = keine Aussage. */
typedef uint8_t uft_d2_conf_t;
#define UFT_D2_CONF_CERTAIN     255u
/** Vorhanden, aber ohne Beleg fuer Vollstaendigkeit oder Richtigkeit —
 *  eine unvollstaendige Umdrehung, ein Sektor ohne Pruefsummenangabe. */
#define UFT_D2_CONF_UNVERIFIED  128u
#define UFT_D2_CONF_NONE          0u

/* ═══════════════════════ Ableitungsregister ═════════════════════════════ */

/**
 * Eine Ableitung wird EINMAL registriert und bekommt eine Kennung. Objekte
 * verweisen auf die Kennung. Damit ist „welche Sektoren stammen aus
 * PLL-Lauf 2?" eine Abfrage, und ein Behaelter traegt die Herkunft einmal
 * statt in jedem Sektor.
 *
 * ZWEITE FASSUNG, und der Unterschied ist nicht kosmetisch: in MF-1272 war
 * `by`/`params` ein `const char *` mit der Auflage „muss STATISCH sein".
 * Ein Zeiger ist nicht serialisierbar, und die Auflage war eine Bitte an
 * den Aufrufer statt einer Eigenschaft des Modells. Jetzt wird kopiert, in
 * feste Feldbreite — was laenger ist, wird gekuerzt und NUL-terminiert.
 */
typedef uint16_t uft_d2_deriv_id_t;
#define UFT_D2_DERIV_NONE   ((uft_d2_deriv_id_t)0)
#define UFT_D2_DERIV_BY     32u
#define UFT_D2_DERIV_PARAMS 96u
/** Obergrenze des Registers. Eine Diskette mit mehr als 4096 verschiedenen
 *  Ableitungen ist kein Anwendungsfall, sondern ein Fehler im Aufrufer —
 *  und der bekommt einen Befund statt eines stillen Verlusts. */
#define UFT_D2_MAX_DERIV    4096u

typedef struct {
    uft_d2_layer_t  from_layer;
    uft_d2_origin_t origin;
    char            by[UFT_D2_DERIV_BY];          /**< „kalman_pll"        */
    char            params[UFT_D2_DERIV_PARAMS];  /**< „cell=2000ns"       */
    uint32_t        source_gen;   /**< Generation der Quelle beim Ableiten */
} uft_d2_derivation_t;

/* ═══════════════════════ Befunde ════════════════════════════════════════ */

typedef enum {
    UFT_D2_DIAG_INFO = 0,
    UFT_D2_DIAG_NOTE,      /**< bemerkenswert, nicht falsch                 */
    UFT_D2_DIAG_WARN,      /**< etwas ist unsicher oder unvollstaendig      */
    UFT_D2_DIAG_ERROR      /**< etwas ist falsch                            */
} uft_d2_diag_sev_t;

#define UFT_D2_DIAG_CODE 24u
#define UFT_D2_DIAG_TEXT 192u

typedef struct {
    uft_d2_diag_sev_t sev;
    uft_d2_layer_t    layer;
    int16_t           cyl;       /**< -1 = keine EINZELNE Spur: ganze
                                  *   Diskette, oder ein Spurbereich, den
                                  *   der Text vorn als „Ca..Cb Hx:" nennt
                                  *   (Querpruefung, Bruecke)            */
    int8_t            head;
    int16_t           sector;    /**< -1 = ganze Spur                       */
    char              code[UFT_D2_DIAG_CODE]; /**< kurz, maschinenlesbar   */
    char              text[UFT_D2_DIAG_TEXT]; /**< Klartext                */
} uft_d2_diag_t;

/** Der LETZTE Platz ist fuer die Ueberlaufmeldung reserviert: 511 Befunde
 *  werden angenommen, der 512. wird abgewiesen UND zur Meldung. */
#define UFT_D2_MAX_DIAG 512u

/* ═══════════════════════ Schicht 1: Fluss ═══════════════════════════════ */

typedef struct {
    uint32_t      *intervals;    /**< Zellzeiten in ns, GEMESSEN            */
    size_t         count;
    uint32_t       index_time_ns;/**< 0 = nicht gemessen — ein BEFUND       */
    bool           complete;     /**< Index bis Index                       */
    uft_d2_conf_t  conf;
} uft_d2_rev_t;

typedef struct {
    uft_d2_rev_t     *revs;      /**< GETRENNT. Nie verflacht.              */
    size_t            count;
    uft_d2_deriv_id_t deriv;
    uint32_t          gen;       /**< steigt bei jeder Aenderung            */
} uft_d2_flux_t;

/* ═══════════════════════ Schicht 2: Bitstrom ════════════════════════════ */

typedef struct {
    uint8_t       *bits;
    size_t         nbits;        /**< GEMESSEN, nie geklemmt                */
    uft_d2_conf_t *bit_conf;     /**< je Bit, NULL wenn nicht bekannt       */
    uint8_t       *agree;        /**< je Bit: wie viele Umdrehungen stimmten
                                      zu — NULL ohne Fusion. „3 von 5" ist
                                      nachpruefbar, „Konfidenz 153" nicht.  */
    uint8_t        nrevs_fused;  /**< 0 = keine Fusion                      */
    int16_t       *phase_q8;     /**< PLL-Phasenlage je Bit, NULL erlaubt   */
    uint16_t      *flux_count;   /**< Flusswechsel je Bit, NULL erlaubt     */
    size_t         index_bit;    /**< SIZE_MAX = unbekannt                  */
    uft_encoding_t encoding;
    uint32_t       cell_ns;
    uft_d2_deriv_id_t deriv;
    uint32_t          gen;
} uft_d2_bitstream_t;

/* ═══════════════════════ Schicht 3: Sektoren ════════════════════════════ */

typedef struct {
    uint8_t  id_cyl, id_head, id_sec, id_size_code;
    bool     id_crc_ok;
    bool     id_crc_known;     /**< false = das Format traegt keine Angabe  */
    uint8_t *data;             /**< NULL bei SND                            */
    uint32_t data_len;
    uint8_t  dam;              /**< 0xFB normal, 0xF8 geloescht             */
    bool     data_crc_ok;
    bool     data_crc_known;
    bool     has_data;
    size_t   idam_bit, dam_bit, data_end_bit;  /**< SIZE_MAX = unbekannt    */
    /** Kodierung DIESES Sektors, wenn sie von der Spur abweicht.
     *  UFT_ENC_UNKNOWN = wie die Spur. */
    uft_encoding_t    encoding;
    uft_d2_origin_t   origin;
    uft_d2_conf_t     conf;
    uft_d2_deriv_id_t deriv;
    uint32_t          source_gen; /**< Generation des Bitstroms beim Ableiten */
    uint32_t weak_bits, fuzzy_bits;
} uft_d2_sector_t;

typedef struct {
    uft_d2_sector_t *items;
    size_t           count, capacity;
    uint32_t         gen;
} uft_d2_sectors_t;

/* ═══════════════════════ Spur ═══════════════════════════════════════════ */

typedef struct {
    uint16_t cyl;
    uint8_t  head;
    bool     has_flux, has_bitstream, has_sectors;
    uft_d2_flux_t      flux;
    uft_d2_bitstream_t bitstream;
    uft_d2_sectors_t   sectors;
    uft_encoding_t     encoding;
    bool               unformatted;
} uft_d2_track_t;

/**
 * GERECHNET aus idam_bit, dam_bit, data_end_bit und index_bit — nicht
 * gespeichert, also nicht falsch zu setzen.
 *
 * Geprueft werden BEIDE Felder: ein Adressfeld ueber dem Index (IOI) und
 * ein Datenfeld ueber dem Index (DOI) sind verschiedene Schutzmuster.
 */
bool uft_d2_sector_crosses_index(const uft_d2_track_t *t,
                                 const uft_d2_sector_t *s);
/** Eigene Kodierung des Sektors, sonst die der Spur. */
uft_encoding_t uft_d2_sector_encoding(const uft_d2_track_t *t,
                                      const uft_d2_sector_t *s);

/* ═══════════════════════ Schicht 4: Dateisysteme ════════════════════════ */

typedef enum {
    UFT_D2_FS_UNKNOWN = 0, UFT_D2_FS_FAT12, UFT_D2_FS_AMIGADOS,
    UFT_D2_FS_CBMDOS, UFT_D2_FS_APPLEDOS33, UFT_D2_FS_PRODOS,
    UFT_D2_FS_PASCAL, UFT_D2_FS_TRDOS,
    UFT_D2_FS_NONE_TRACKLOADER    /**< kein Dateisystem — eine TATSACHE     */
} uft_d2_fs_kind_t;

#define UFT_D2_ENTRY_NAME 64u
#define UFT_D2_FS_LABEL   32u

typedef struct {
    char     name[UFT_D2_ENTRY_NAME];
    uint32_t size;
    uint8_t  type;
    bool     deleted, recoverable, chain_broken, cross_linked;
    uint32_t start_unit;
    uft_d2_conf_t     conf;
    uft_d2_deriv_id_t deriv;
} uft_d2_entry_t;

/**
 * EIN Dateisystem. Eine Diskette kann MEHRERE tragen — Doppelformat,
 * Partitionen, Trackloader UND Dateisystem auf verschiedenen Spuren.
 * In MF-1272 gab es genau eines; die Grenze war eine Annahme, keine
 * Eigenschaft von Disketten.
 */
typedef struct {
    uft_d2_fs_kind_t kind;
    uft_d2_conf_t    kind_conf;
    char             label[UFT_D2_FS_LABEL];
    uint16_t         cyl_from, cyl_to;  /**< 0..0xFFFF = ganze Diskette     */
    uint8_t          head_mask;         /**< 0 = alle Koepfe                */
    uft_d2_entry_t  *entries;
    size_t           count, capacity;
    size_t           deleted_count;
    bool             counters_consistent, counters_checked;
    uft_d2_deriv_id_t deriv;
    uint32_t          source_gen;
} uft_d2_fs_t;

/* ═══════════════════════ Metadaten ══════════════════════════════════════ */

typedef enum {
    UFT_D2_META_FORMAT_FIELD = 0, /**< stand als Feld im Behaelter          */
    UFT_D2_META_FREE_TEXT,        /**< Freitext aus dem Behaelter           */
    UFT_D2_META_FILESYSTEM,       /**< aus dem Dateisystem gelesen          */
    UFT_D2_META_SELF              /**< von UFT selbst erzeugt               */
} uft_d2_meta_src_t;

#define UFT_D2_META_KEY   32u
#define UFT_D2_META_VALUE 192u
/** Dynamisch bis hierher, dann ein Befund META_OVERFLOW — in MF-1272 waren
 *  es 64 feste Plaetze und der 65. fiel STILL heraus. */
#define UFT_D2_MAX_META   256u

typedef struct {
    char key[UFT_D2_META_KEY];
    char value[UFT_D2_META_VALUE];
    uft_d2_meta_src_t src;
} uft_d2_meta_t;

/* ═══════════════════════ Die Diskette ═══════════════════════════════════ */

typedef struct uft_disk2 uft_disk2_t;

uft_disk2_t *uft_d2_create(void);
void         uft_d2_destroy(uft_disk2_t *d);

/**
 * Registriert eine Ableitung und gibt ihre Kennung zurueck.
 * @return 0 (UFT_D2_DERIV_NONE) nur bei Speichermangel oder vollem Register.
 */
uft_d2_deriv_id_t uft_d2_register_deriv(uft_disk2_t *d, uft_d2_layer_t from,
                                        uft_d2_origin_t origin,
                                        const char *by, const char *params,
                                        uint32_t source_gen);
const uft_d2_derivation_t *uft_d2_deriv(const uft_disk2_t *d,
                                        uft_d2_deriv_id_t id);
size_t uft_d2_deriv_count(const uft_disk2_t *d);

/**
 * Die Spur, und wenn es sie nicht gibt, eine neue.
 *
 * ACHTUNG, und das steht hier, weil es sonst niemand merkt: der
 * Rueckgabewert gilt nur, bis die NAECHSTE Spur angelegt wird. Die Spuren
 * liegen in einem Feld, das beim Wachsen umzieht; ein ueber ein weiteres
 * `uft_d2_track()` hinweg gehaltener Zeiger sieht danach ins Leere. Wer
 * eine Spur laenger braucht, holt sie sich neu oder merkt sich cyl/head.
 * Der Spurindex im Inneren haelt deshalb NUMMERN, keine Zeiger.
 */
uft_d2_track_t *uft_d2_track(uft_disk2_t *d, uint16_t cyl, uint8_t head);
const uft_d2_track_t *uft_d2_track_get(const uft_disk2_t *d, uint16_t cyl,
                                       uint8_t head);
size_t uft_d2_track_count(const uft_disk2_t *d);
const uft_d2_track_t *uft_d2_track_at(const uft_disk2_t *d, size_t i);
/** Die groesste vorhandene Lage — GEMESSEN, nicht aus einer Geometrie. */
bool uft_d2_extent(const uft_disk2_t *d, uint16_t *out_max_cyl,
                   uint8_t *out_max_head);

bool uft_d2_add_revolution(uft_disk2_t *d, uft_d2_track_t *t,
                           const uint32_t *intervals, size_t count,
                           uint32_t index_time_ns, bool complete,
                           uft_d2_conf_t conf, uft_d2_deriv_id_t deriv);

bool uft_d2_set_bitstream(uft_disk2_t *d, uft_d2_track_t *t,
                          const uint8_t *bits, size_t nbits,
                          const uft_d2_conf_t *bit_conf,
                          const uint8_t *agree, uint8_t nrevs_fused,
                          const int16_t *phase_q8, const uint16_t *flux_count,
                          size_t index_bit, uft_encoding_t enc,
                          uint32_t cell_ns, uft_d2_deriv_id_t deriv);

bool uft_d2_add_sector(uft_disk2_t *d, uft_d2_track_t *t,
                       const uft_d2_sector_t *s);

uft_d2_fs_t *uft_d2_add_fs(uft_disk2_t *d, uft_d2_fs_kind_t kind,
                           uft_d2_conf_t kind_conf, uft_d2_deriv_id_t deriv);
size_t uft_d2_fs_count(const uft_disk2_t *d);
uft_d2_fs_t *uft_d2_fs_at(uft_disk2_t *d, size_t i);
bool uft_d2_add_entry(uft_disk2_t *d, uft_d2_fs_t *fs,
                      const uft_d2_entry_t *e);

bool uft_d2_add_meta(uft_disk2_t *d, const char *key, const char *value,
                     uft_d2_meta_src_t src);
const char *uft_d2_meta(const uft_disk2_t *d, const char *key);
size_t uft_d2_meta_count(const uft_disk2_t *d);
const uft_d2_meta_t *uft_d2_meta_at(const uft_disk2_t *d, size_t i);

bool uft_d2_diag(uft_disk2_t *d, uft_d2_diag_sev_t sev, uft_d2_layer_t layer,
                 int cyl, int head, int sector, const char *code,
                 const char *fmt, ...);
size_t uft_d2_diag_count(const uft_disk2_t *d);
const uft_d2_diag_t *uft_d2_diag_at(const uft_disk2_t *d, size_t i);
size_t uft_d2_diag_count_sev(const uft_disk2_t *d, uft_d2_diag_sev_t at_least);

/**
 * Prueft die innere Stimmigkeit und schreibt Befunde.
 *   STALE_DERIV     Objekt stammt aus einer Generation, die ersetzt wurde
 *   POS_BEYOND      Sektor nennt Bitpositionen jenseits des Bitstroms
 *   ENC_MISMATCH    Spur- und Bitstromkodierung widersprechen sich
 *   REV_EMPTY_INDEX Umdrehung mit Indexzeit, aber ohne Flusswechsel
 *   FS_RANGE        Dateisystem nennt Spuren, die es nicht gibt
 * @return Zahl der NEUEN Befunde. 0 heisst stimmig.
 */
size_t uft_d2_validate(uft_disk2_t *d);

/**
 * Vergleicht die Sektornummern-Mengen der Spuren EINES Kopfes miteinander
 * und schreibt Befunde. Liest nur; Spuren, Sektoren, Schichten, Merkmale
 * und Geometrie bleiben unberuehrt. Ein zweiter Aufruf fuegt keinen
 * Befund doppelt hinzu.
 *
 * Wozu: `uft_st_order_messen()` kennt die Sektorzahl EINER Spur (H-18,
 * MF-1339). Fehlt der LETZTE Sektor ganz, sieht man es der Spur nicht an —
 * wohl aber im Vergleich mit dem Rest. Das ist diese Ebene.
 *
 * Die KLAMMERREGEL (nicht die Nachbarregel des Entwurfs, die bei zwei
 * benachbarten beschaedigten Spuren schwieg):
 *
 *   Auf demselben Kopf liegt ein Lauf aufeinanderfolgender Spuren, deren
 *   Menge von S abweicht, zwischen zwei Klammerspuren, die BEIDE die Menge
 *   S tragen. Je GRUPPE aufeinanderfolgender Laufspuren mit derselben
 *   Menge EIN Befund (nicht je Spur — 60 Spuren mit derselben Luecke
 *   ergaben sonst 60 WARN):
 *     echte Teilmenge von S   WARN  SEC_GAP_VS_BRACKET   („fehlt: …")
 *     echte Obermenge von S   NOTE  SEC_EXTRA_VS_BRACKET (moeglicher
 *                                   Schutz, kein Fehler)
 *     weder noch              kein Befund
 *   Eine Gruppe aus EINER Spur steht in cyl/head („traegt 9, Klammer 10
 *   (C39/C41); fehlt: 10"); eine groessere hat cyl = head = -1 und nennt
 *   den Bereich im Text („C10..C69 H0: tragen 9, Klammer 10 (C9/C70);
 *   fehlt: 10"), weil das Befundfeld EINEN Zylinder fasst.
 *
 *   Am RAND (Zylinder 0 oder hoechster Zylinder) gibt es nur eine
 *   Klammer; zweiter Zeuge ist dann der andere Kopf auf demselben
 *   Zylinder, und der Lauf ist genau diese eine Randspur. Ohne zweiten
 *   Zeugen: kein Befund. Eine echte Teilmenge am Rand ist ein NOTE, kein
 *   WARN, und ihr Text nennt die zweite Lesart: eine BOOTSPUR, die nur
 *   auf einer Seite eine andere Sektorzahl traegt, sieht genauso aus.
 *   Der gemessene Fall ist TRS-80 Model I DD zweiseitig mit einer
 *   Spur 0 in einfacher Dichte auf Seite 0 (0..9, alles andere 0..17):
 *   als WARN war das „fehlt: 10, …, 17" an einer richtigen Diskette.
 *
 * Zonenformate (C64, Apple 3,5", Victor 9000) bleiben stumm, weil ihre
 * Mengen auf einem Kopf nie zu einer frueheren zurueckkehren — es gibt
 * keine zweite Klammer. Geprueft in `tests/test_d2_querpruefung.c`.
 *
 * GRENZEN, ausdruecklich:
 *   - Eine FEHLENDE Spur ist kein Beleg und unterbricht jede Klammer. Das
 *     Modell unterscheidet nicht zwischen „nicht gelesen", „read_track
 *     lieferte einen Fehler" (die Bruecke meldet das je Lauf als NOTE
 *     TRACK_UNREADABLE — ob unlesbar oder im Behaelter nicht vorhanden
 *     bzw. unformatiert, sagt der Rueckgabewert nicht: D88/D77 melden
 *     eine unformatierte Spur als Fehler) und „gelesen und leer" (die
 *     Bruecke legt solche Spuren nicht an). Deshalb meldet diese Pruefung
 *     NIE eine Spurluecke.
 *   - Eine Bootspur mit anderer Sektorzahl auf BEIDEN Seiten ist vom Rand
 *     aus nicht zu sehen (kein zweiter Zeuge) — und nur auf EINER Seite
 *     nicht von einer beschaedigten Randspur zu unterscheiden. Daher NOTE,
 *     siehe oben.
 *   - Eine Spur ohne Sektoren (nur Bitstrom/Fluss) nimmt nicht teil.
 *   - Zwei beschaedigte Randspuren nebeneinander bleiben stumm: fuer die
 *     innere gibt es keine Klammer, fuer die aeussere keinen zweiten
 *     Zeugen in Klammerlage.
 *   - Sektorabbilder ohne eigene Sektorliste (IMG, D64, ADF, ST) tragen
 *     immer die volle Menge; dort sagt die Pruefung konstruktionsbedingt
 *     nichts.
 *
 * @return Zahl der NEUEN Befunde.
 */
size_t uft_d2_querpruefung(uft_disk2_t *d);

/** Bitmaske 1<<layer ueber alle Spuren. */
uint32_t uft_d2_layers(const uft_disk2_t *d);

typedef enum {
    UFT_D2_FEAT_REVOLUTIONS   = 1u << 0,
    UFT_D2_FEAT_INDEX_TIME    = 1u << 1,
    UFT_D2_FEAT_WEAK_BITS     = 1u << 2,
    UFT_D2_FEAT_GAPS          = 1u << 3,
    UFT_D2_FEAT_VAR_SECTOR_SZ = 1u << 4,
    UFT_D2_FEAT_DUP_SECTORS   = 1u << 5,
    UFT_D2_FEAT_BAD_CRC       = 1u << 6,
    UFT_D2_FEAT_DELETED_DAM   = 1u << 7,
    UFT_D2_FEAT_MIXED_ENC     = 1u << 8,
    UFT_D2_FEAT_NO_DATA_SEC   = 1u << 9,
    UFT_D2_FEAT_UNFORMATTED   = 1u << 10,
    UFT_D2_FEAT_DELETED_FILES = 1u << 11,
    UFT_D2_FEAT_METADATA      = 1u << 12,
    UFT_D2_FEAT_MULTI_FS      = 1u << 13,
    UFT_D2_FEAT_VOTES         = 1u << 14
} uft_d2_feature_t;
/** Zahl der Merkmale — ersetzt das Literal, das in MF-1272 an zwei
 *  Stellen stand und beim Erweitern an einer vergessen wuerde. */
#define UFT_D2_FEAT_COUNT 15u

/** Aus dem Cache; beim Einspeisen als schmutzig markiert. */
uint32_t uft_d2_features(const uft_disk2_t *d);

/**
 * Was bei einer Wandlung verloren ginge. Die Frage, die jedes Zielformat
 * beantwortet bekommen muss, BEVOR geschrieben wird.
 * @return true, wenn nichts verloren geht.
 */
bool uft_d2_check_loss(const uft_disk2_t *d, uint32_t target_layers,
                       uint32_t target_features,
                       uint32_t *out_lost_layers, uint32_t *out_lost_features);

const char *uft_d2_layer_name(uft_d2_layer_t l);
const char *uft_d2_feature_name(uft_d2_feature_t f);
const char *uft_d2_origin_name(uft_d2_origin_t o);

/**
 * Bericht in einen Puffer.
 * @return die BENOETIGTE Laenge ohne die abschliessende Null. Ein Wert
 *         >= @p buflen heisst: gekuerzt (Bauform `snprintf`).
 */
size_t uft_d2_report(const uft_disk2_t *d, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
#endif /* UFT_DISK2_H */
