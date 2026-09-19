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
 *   1. JEDES abgeleitete Objekt weiss, woraus und wodurch es entstand.
 *      Ein Sektor aus einem Bitstrom traegt die Spur, den Sucher und den
 *      Bitbereich. Ein Bitstrom aus Fluss traegt die Umdrehung(en) und die
 *      PLL. Das ist "Herkunft je Wert", strukturell.
 *
 *   2. Zuversicht ist eine Zahl mit Bedeutung. 255 heisst: direkt vom
 *      Traeger, CRC stimmt. Alles darunter ist rekonstruiert, fusioniert
 *      oder geraten — und die Zahl sagt, wie sehr. Zuversicht steigt beim
 *      Aufsteigen der Schichten NIE ohne Beleg.
 *
 *   3. Keine stille Normalisierung. Die Spurlaenge ist, was gemessen wurde.
 *      Sektorgroessen sind je Sektor. Doppelte Sektornummern sind erlaubt.
 *      Ein Wert, der nicht passt, sieht nicht aus wie einer, der passt.
 *
 * ── WAS ES NICHT IST ────────────────────────────────────────────────────
 *
 * Kein Leser, kein Schreiber, keine PLL, kein Sektorsucher. Es ist die
 * Struktur, in die diese Dinge einspeisen und aus der sie lesen. Die
 * Algorithmen aus dieser Reihe — uft_revolution, uft_protection_scan,
 * uft_splice, uft_fat_robust, uft_a2_order, uft_amiga_media — sind die
 * ersten, die darauf arbeiten koennen. Bisher konnten sie es nicht, weil
 * es die Struktur nicht gab.
 *
 * ── BERICHTIGT GEGENUEBER DEM ENTWURF (MF-1272) ─────────────────────────
 *
 * Der Entwurf kam als eigenstaendiges Paket (Kopf, Umsetzung, Test — neun
 * Gruppen gruen unter ASan/UBSan). Beim Einbau in DIESEN Baum wurde
 * gemessen, nicht angenommen:
 *
 *   * FUENF Namen des Entwurfs gibt es hier schon, mit anderer Bedeutung:
 *     `uft_encoding_t` (21 Dateien), `uft_layer_t` (Bitmaske in
 *     `uft_unified_image.h` — und ein ZWEITES Mal in `uft_track.h`),
 *     `uft_diag_t` (8 Dateien, ein 256-Byte-Textpuffer), `UFT_CONF_CERTAIN`
 *     (`uft_protection.h`), `UFT_FS_FAT12` (`uft_integration.h`).
 *     Deshalb tragen ALLE oeffentlichen Namen dieses Moduls das Praefix
 *     `uft_d2_` / `UFT_D2_` — die Funktionen hatten es schon.
 *
 *   * Die Kodierung ist NICHT neu definiert: `uft_encoding_t` aus
 *     `uft/uft_types.h` traegt bereits UNKNOWN/FM/MFM/GCR-Varianten und
 *     ist der Typ von `uft_track_t.encoding`. Eine zweite Aufzaehlung
 *     daneben waere die Doppelhaltung, gegen die dieser Baum steht (D3).
 *
 *   * `UFT_D2_CONF_UNVERIFIED` (128) hat einen Namen: der Entwurf setzte
 *     die Zahl fuer unvollstaendige Umdrehungen als Literal. Sie heisst
 *     „vorhanden, aber ohne Beleg fuer Vollstaendigkeit oder Richtigkeit"
 *     und ist der Wert, den auch die Bruecke fuer Sektoren ohne
 *     Pruefsummenangabe vergibt (`uft_disk2_bridge.h`).
 *
 *   * `uft_d2_report()` gibt die BENOETIGTE Laenge zurueck, nicht die
 *     geschriebene (Bauform `snprintf`). Ein Rueckgabewert >= buflen heisst:
 *     der Bericht wurde gekuerzt. Vorher war eine Kuerzung nicht erkennbar.
 *
 *   * Die Sektorzeile des Berichts unterscheidet „CRC falsch" von „CRC nicht
 *     getragen". Der Entwurf schrieb „davon mit falscher CRC: 0" auch fuer
 *     ein Abbild, das gar keine Pruefsumme kennt — und genau dieses Urteil
 *     in EINE Richtung nennt `tests/test_disk_analyzer_no_fiction.cpp`
 *     seit MF-662 erfunden.
 *
 *   * Die Befundliste reserviert ihren letzten Platz von Anfang an: 511
 *     Befunde werden angenommen, der 512. wird abgewiesen UND zur
 *     Ueberlaufmeldung. Im Entwurf wurde der 512. angenommen (true) und
 *     danach still von der Ueberlaufmeldung ueberschrieben.
 *
 *   * `UFT_D2_FEAT_COUNT` ersetzt das Literal 13, das an zwei Stellen stand.
 *
 *   * Sektorgroessen werden nur unter Sektoren MIT Datenfeld verglichen;
 *     ein Adressfeld ohne Daten (SND) hat keine Groesse, die abweichen
 *     koennte.
 *
 *   * Fehlt der Speicher fuer eine der optionalen Bitstrom-Beilagen
 *     (Konfidenz, Phase, Flusszahl), scheitert `uft_d2_set_bitstream()`
 *     als Ganzes. Im Entwurf blieb die Beilage still NULL — und ein
 *     Bitstrom OHNE Konfidenz ist eine andere Aussage als einer mit.
 *
 *   * Name und Etikett des Dateisystems werden beim Einspeisen
 *     NUL-terminiert, damit ein 64 Zeichen langer Name den Bericht nicht
 *     ueber das Feldende hinaus lesen laesst.
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

/**
 * Wodurch ein Objekt entstand. JEDES abgeleitete Objekt traegt eines.
 *
 * `by` ist ein Name, kein Zeiger — "kalman_pll", "ibm_mfm_scanner",
 * "fat12_robust". Er steht im Bericht, damit man weiss, welcher Algorithmus
 * mit welchen Parametern am Werk war. Beide Zeichenketten muessen STATISCH
 * sein: das Modell kopiert sie nicht.
 */
typedef struct {
    uft_d2_layer_t  from_layer;
    uft_d2_origin_t origin;
    const char     *by;          /**< Algorithmus, statisch                  */
    const char     *params;      /**< dessen Parameter als Text, statisch    */
    uint32_t        source_a;    /**< z. B. Umdrehungsindex, Bitversatz      */
    uint32_t        source_b;    /**< z. B. Endversatz                       */
} uft_d2_derivation_t;

/* ═══════════════════════ Befunde ════════════════════════════════════════ */

typedef enum {
    UFT_D2_DIAG_INFO = 0,
    UFT_D2_DIAG_NOTE,      /**< bemerkenswert, nicht falsch                 */
    UFT_D2_DIAG_WARN,      /**< etwas ist unsicher oder unvollstaendig      */
    UFT_D2_DIAG_ERROR      /**< etwas ist falsch                            */
} uft_d2_diag_sev_t;

typedef struct {
    uft_d2_diag_sev_t sev;
    uft_d2_layer_t    layer;
    int16_t           cyl;       /**< -1 = ganze Diskette                    */
    int8_t            head;
    int16_t           sector;    /**< -1 = ganze Spur                        */
    const char       *code;      /**< kurz, maschinenlesbar, STATISCH:
                                      "TIE", "NO_ROOT"                        */
    char              text[192]; /**< Klartext                               */
} uft_d2_diag_t;

/** Die Befundliste ist endlich. Der letzte Platz ist von Anfang an fuer die
 *  Ueberlaufmeldung reserviert: UFT_D2_MAX_DIAG - 1 Befunde werden
 *  angenommen, jeder weitere wird abgewiesen und macht die Meldung. */
#define UFT_D2_MAX_DIAG  512u
#define UFT_D2_MAX_META   64u

/* ═══════════════════════ Schicht 1: Fluss ═══════════════════════════════ */

typedef struct {
    uint32_t *intervals;      /**< Flusszwischenzeiten in ns              */
    size_t    count;
    uint32_t  index_time_ns;  /**< 0 = nicht gemessen — ein BEFUND        */
    bool      complete;
    uft_d2_conf_t conf;
} uft_d2_rev_t;

typedef struct {
    uft_d2_rev_t *revs;       /**< GETRENNT. Nie verflacht.               */
    size_t        count;
    uft_d2_derivation_t deriv;   /**< Geraet, Transport, Parameter        */
} uft_d2_flux_t;

/* ═══════════════════════ Schicht 2: Bitstrom ════════════════════════════ */

typedef struct {
    uint8_t   *bits;          /**< gepackt, MSB zuerst                    */
    size_t     nbits;         /**< GEMESSEN. Nie auf 0x1900 geklemmt.     */
    uft_d2_conf_t *bit_conf;  /**< je Bit, NULL wenn nicht bekannt.
                                   Kommt aus dem Umdrehungsvergleich —
                                   ohne den ist es NULL, und das ist
                                   richtig, nicht ein Mangel.             */
    int16_t   *phase_q8;      /**< je Bit, NULL wenn die PLL es nicht
                                   liefert. Der Fuzzy-Nachweis.           */
    uint16_t  *flux_count;    /**< je Bit, NULL. Der Weak-Nachweis.       */
    size_t     index_bit;     /**< Bitlage des Indexpulses, oder SIZE_MAX */
    uft_encoding_t encoding;
    uint32_t   cell_ns;       /**< gemessene Zellbreite; 0 = nicht gemessen */
    uft_d2_derivation_t deriv;
} uft_d2_bitstream_t;

/* ═══════════════════════ Schicht 3: Sektoren ════════════════════════════ */

/**
 * Alles, was IRGENDEIN Format je Sektor traegt — die Vereinigung aus IMD,
 * TD0, PSI, JV3, DMK, STX, IPF. Was ein Format nicht traegt, bleibt auf
 * seinem "unbekannt"-Wert; es wird nicht erfunden.
 */
typedef struct {
    /* Adressfeld */
    uint8_t  id_cyl, id_head, id_sec, id_size_code;
    bool     id_crc_ok;
    bool     id_crc_known;    /**< false = das Format traegt es nicht     */

    /* Datenfeld */
    uint8_t *data;
    uint32_t data_len;        /**< TATSAECHLICH, kann von size_code
                                   abweichen (Ensoniq SQ80, Slogger)      */
    uint8_t  dam;             /**< 0xFB, 0xF8, 0xFA, 0xF9; 0 = unbekannt  */
    bool     data_crc_ok;
    bool     data_crc_known;
    bool     has_data;        /**< false = ID ohne Datenfeld (SND)        */

    /* Lage — nur wenn aus einem Bitstrom */
    size_t   idam_bit, dam_bit, data_end_bit;   /**< SIZE_MAX = unbekannt */
    bool     crosses_index;

    /* Herkunft */
    uft_d2_origin_t origin;
    uft_d2_conf_t   conf;
    uft_d2_derivation_t deriv;

    /* Flackern, nur mit Umdrehungen */
    uint32_t weak_bits;       /**< Zahl flackernder Bits im Datenfeld     */
    uint32_t fuzzy_bits;
} uft_d2_sector_t;

typedef struct {
    uft_d2_sector_t *items;   /**< DOPPELTE Nummern erlaubt. Ueberlappung
                                   erlaubt. Reihenfolge = Reihenfolge auf
                                   der Spur, wenn bekannt.                */
    size_t count;
    size_t capacity;
} uft_d2_sectors_t;

/* ═══════════════════════ Spur ═══════════════════════════════════════════ */

typedef struct {
    uint16_t cyl;
    uint8_t  head;
    bool     has_flux, has_bitstream, has_sectors;
    uft_d2_flux_t      flux;
    uft_d2_bitstream_t bitstream;
    uft_d2_sectors_t   sectors;
    uft_encoding_t     encoding;   /**< JE SPUR — Slogger, FLEX mischen  */
    bool               unformatted;
} uft_d2_track_t;

/* ═══════════════════════ Schicht 4: Dateisystem ═════════════════════════ */

typedef enum {
    UFT_D2_FS_UNKNOWN = 0, UFT_D2_FS_FAT12, UFT_D2_FS_AMIGADOS,
    UFT_D2_FS_CBMDOS, UFT_D2_FS_APPLEDOS33, UFT_D2_FS_PRODOS,
    UFT_D2_FS_PASCAL, UFT_D2_FS_TRDOS,
    UFT_D2_FS_NONE_TRACKLOADER   /**< Kopf sagt Dateisystem, Inhalt nicht  */
} uft_d2_fs_kind_t;

typedef struct {
    char     name[64];
    uint32_t size;
    uint8_t  type;
    bool     deleted;
    bool     recoverable;     /**< bei geloeschten: Daten noch erreichbar */
    bool     chain_broken;    /**< FAT: fortlaufend gelesen — ein VERSUCH */
    bool     cross_linked;
    uint32_t start_unit;      /**< Cluster / Block / Spur-Sektor          */
    uft_d2_conf_t conf;
    uft_d2_derivation_t deriv;
} uft_d2_entry_t;

typedef struct {
    uft_d2_fs_kind_t kind;
    uft_d2_conf_t    kind_conf;  /**< wie sicher die Erkennung ist       */
    char             label[32];
    uft_d2_entry_t  *entries;
    size_t           count, capacity;
    size_t           deleted_count;
    bool             counters_consistent; /**< Kopf gegen Katalog        */
    bool             counters_checked;
} uft_d2_fs_t;

/* ═══════════════════════ Metadaten ══════════════════════════════════════ */

typedef enum {
    UFT_D2_META_FORMAT_FIELD = 0, UFT_D2_META_FREE_TEXT,
    UFT_D2_META_FILESYSTEM, UFT_D2_META_SELF
} uft_d2_meta_src_t;

typedef struct {
    char key[32];
    char value[192];
    uft_d2_meta_src_t src;
} uft_d2_meta_t;

/* ═══════════════════════ Die Diskette ═══════════════════════════════════ */

typedef struct uft_disk2 uft_disk2_t;

uft_disk2_t *uft_d2_create(void);
void         uft_d2_destroy(uft_disk2_t *d);

/* ── Spuren ──────────────────────────────────────────────────────────── */

/** Holt oder legt an. Spuren werden bei Bedarf angelegt, nie vorab. */
uft_d2_track_t *uft_d2_track(uft_disk2_t *d, uint16_t cyl, uint8_t head);
const uft_d2_track_t *uft_d2_track_get(const uft_disk2_t *d, uint16_t cyl,
                                       uint8_t head);
size_t uft_d2_track_count(const uft_disk2_t *d);
/** Hoechste vorhandene Lage — GEMESSEN, keine Annahme. */
bool uft_d2_extent(const uft_disk2_t *d, uint16_t *out_max_cyl,
                   uint8_t *out_max_head);

/* ── Schicht 1 einspeisen ────────────────────────────────────────────── */

/**
 * Eine Umdrehung anhaengen. Die Daten werden KOPIERT; der Aufrufer behaelt
 * seine.
 *
 * @param index_time_ns 0 heisst "nicht gemessen". Das wird als BEFUND
 *                      vermerkt, nicht stillschweigend durchgereicht —
 *                      ohne Indexzeit keine Drehzahl, kein Fuzzy-Nachweis.
 */
bool uft_d2_add_revolution(uft_disk2_t *d, uft_d2_track_t *t,
                           const uint32_t *intervals, size_t count,
                           uint32_t index_time_ns, bool complete,
                           const uft_d2_derivation_t *deriv);

/* ── Schicht 2 einspeisen ────────────────────────────────────────────── */

/**
 * Bitstrom setzen. `nbits` ist die gemessene Laenge. Konfidenz, Phase und
 * Flusszahl sind optional (NULL) — und wenn sie fehlen, fehlen sie. Sie
 * werden nicht mit 255 aufgefuellt. Wird eine Beilage uebergeben und der
 * Speicher dafuer fehlt, scheitert der ganze Aufruf (false, Bitstrom der
 * Spur geleert) — ein Bitstrom OHNE Konfidenz waere eine andere Aussage.
 */
bool uft_d2_set_bitstream(uft_disk2_t *d, uft_d2_track_t *t,
                          const uint8_t *bits, size_t nbits,
                          const uft_d2_conf_t *bit_conf,
                          const int16_t *phase_q8, const uint16_t *flux_count,
                          size_t index_bit, uft_encoding_t enc,
                          uint32_t cell_ns, const uft_d2_derivation_t *deriv);

/* ── Schicht 3 einspeisen ────────────────────────────────────────────── */

/**
 * Sektor anhaengen. Daten werden kopiert. Doppelte Nummern und
 * Ueberlappungen sind ERLAUBT — sie sind Tatsachen, keine Fehler.
 *
 * Die Zuversicht wird GEPRUEFT: ein Sektor mit `data_crc_ok == false`
 * darf nicht 255 tragen, und einer mit origin RECONSTRUCTED auch nicht.
 * Wer das versucht, bekommt false und einen Befund — Zuversicht steigt nie
 * ohne Beleg.
 */
bool uft_d2_add_sector(uft_disk2_t *d, uft_d2_track_t *t,
                       const uft_d2_sector_t *s);

/* ── Schicht 4 einspeisen ────────────────────────────────────────────── */

uft_d2_fs_t *uft_d2_fs(uft_disk2_t *d);
bool uft_d2_add_entry(uft_disk2_t *d, const uft_d2_entry_t *e);

/* ── Metadaten und Befunde ───────────────────────────────────────────── */

/** Gleiche Schluessel werden NICHT zusammengelegt — zwei Kommentarfelder
 *  sind zwei Werte. `uft_d2_meta()` liefert den ersten. */
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

/* ═══════════════════════ Was der Traeger TRAEGT ═════════════════════════ */

/**
 * Welche Schichten vorhanden sind — auf mindestens einer Spur.
 *
 * Das ist die Grundlage der Verlustpruefung: ein Zielformat braucht
 * bestimmte Schichten, und was die Diskette nicht hat, kann es nicht
 * bekommen.
 */
uint32_t uft_d2_layers(const uft_disk2_t *d);   /**< Bitmaske 1<<layer   */

/** Merkmale jenseits der Schichten, die ein Zielformat tragen muss. */
typedef enum {
    UFT_D2_FEAT_REVOLUTIONS   = 1u << 0,  /**< mehrere, getrennt              */
    UFT_D2_FEAT_INDEX_TIME    = 1u << 1,
    UFT_D2_FEAT_WEAK_BITS     = 1u << 2,  /**< Konfidenz je Bit vorhanden     */
    UFT_D2_FEAT_GAPS          = 1u << 3,  /**< Bitstrom, also Luecken         */
    UFT_D2_FEAT_VAR_SECTOR_SZ = 1u << 4,  /**< ungleiche Groessen auf 1 Spur  */
    UFT_D2_FEAT_DUP_SECTORS   = 1u << 5,
    UFT_D2_FEAT_BAD_CRC       = 1u << 6,  /**< Sektoren mit falscher CRC      */
    UFT_D2_FEAT_DELETED_DAM   = 1u << 7,
    UFT_D2_FEAT_MIXED_ENC     = 1u << 8,  /**< FM und MFM gemischt            */
    UFT_D2_FEAT_NO_DATA_SEC   = 1u << 9,  /**< ID ohne Datenfeld              */
    UFT_D2_FEAT_UNFORMATTED   = 1u << 10,
    UFT_D2_FEAT_DELETED_FILES = 1u << 11,
    UFT_D2_FEAT_METADATA      = 1u << 12
} uft_d2_feature_t;
/** Zahl der Merkmalsbits — EINE Stelle, nicht ein Literal an zweien. */
#define UFT_D2_FEAT_COUNT 13u

/** GEMESSEN ueber alle Spuren. Nicht behauptet. */
uint32_t uft_d2_features(const uft_disk2_t *d);

/**
 * Verlustpruefung: was ginge verloren, wenn dieser Traeger in ein Format
 * geschrieben wuerde, das nur `target_layers` und `target_features`
 * tragen kann?
 *
 * @param out_lost_layers   Bitmaske der Schichten, die verloren gingen
 * @param out_lost_features Bitmaske der Merkmale
 * @return true, wenn NICHTS verloren geht. Bei false muss der Aufrufer
 *         entweder abbrechen oder `allow_loss` verlangen — und das
 *         Verlorene BENENNEN.
 */
bool uft_d2_check_loss(const uft_disk2_t *d, uint32_t target_layers,
                       uint32_t target_features,
                       uint32_t *out_lost_layers, uint32_t *out_lost_features);

const char *uft_d2_layer_name(uft_d2_layer_t l);
const char *uft_d2_feature_name(uft_d2_feature_t f);
const char *uft_d2_origin_name(uft_d2_origin_t o);

/* ═══════════════════════ Bericht ════════════════════════════════════════ */

/**
 * Schreibt den Bericht nach `buf` und gibt die Laenge zurueck, die der
 * VOLLE Bericht braucht (ohne NUL) — Bauform `snprintf`. Ist der
 * Rueckgabewert >= buflen, wurde gekuerzt; die Warnungen stehen deshalb
 * ZUERST, damit eine Kuerzung nie sie trifft.
 */
size_t uft_d2_report(const uft_disk2_t *d, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK2_H */
