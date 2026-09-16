/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_track_layout.h
 * @brief Spurmodell fuer ungleiche Sektorgroessen, FM/MFM je Spur und
 *        Fuellsektoren — und ein Wort dafuer, dass etwas Fuellung IST.
 *
 * Herkunft: Eigentuemer-Zulieferung `neue-ideen/uft-layout-code.zip`,
 * eigener Code des Eigentuemers, aus den Aussagen des OmniFlop User Guide
 * v3.2d §5.5.2-5.5.5 geschrieben — zitiert, nicht kopiert; das Dokument
 * traegt ein Reproduktionsverbot, und die verkaufte Formattafel ist
 * ausdruecklich NICHT Teil davon. Beim Einbau geaendert wurde genau eine
 * Sache, und sie steht unten unter „Der Nullwert".
 *
 * ── WARUM ───────────────────────────────────────────────────────────────
 *
 * `include/uft/core/uft_disk.h:117-118` hat heute:
 *
 *     bool variable_sectors;   // Sectors vary by track
 *     bool variable_density;   // Density varies
 *
 * Die FLAGS fuer Ungleichmaessigkeit sind da — die Datenstruktur, um sie
 * auszudruecken, nicht. Gemessen ueber `git grep`: beide haben im ganzen
 * Baum **null** Nutzer, nur ihre eigene Deklaration; das Tote-Felder-Tor
 * (MF-831/MF-1163) fuehrt sie bereits. Und ein Marker fuer Fuellung gibt es
 * nirgends — `is_padding|padding_sector|UFT_SECTOR_PAD|SEC_PADDING`
 * ergibt **0** Treffer.
 *
 * Was der Baum stattdessen hat, ist `uft_sector_status_t`
 * (`include/uft/protection/uft_protection.h:170-179`): OK, BAD_CRC,
 * DELETED, MISSING, EXTRA, DUPLICATE, WRONG_SIZE, WEAK. Er kann sagen
 * „erwartet, nicht gefunden" — aber NICHT „das habe ich selbst
 * angehaengt". Das sind zwei verschiedene Aussagen: MISSING heisst, das
 * Medium gab es nicht her; Fuellung heisst, WIR haben es erfunden.
 *
 * ── Drei belegte Faelle, an denen das heutige Modell bricht ─────────────
 *
 *  1. Ensoniq SQ80 — 1024-Byte-Sektoren fuer ALLE AUSSER DEM LETZTEN
 *     Sektor jeder Spur. Ungleiche Groessen INNERHALB einer Spur;
 *     `sector_size` als EIN Feld kann das nicht.
 *
 *  2. Slogger DDCPM und Computer Automation LSI-2 — auf den FM-Spuren ist
 *     die physische Sektorgroesse halbiert; der Treiber haengt 0xFF an, um
 *     eine gleiche Groesse zu halten. FM und MFM auf EINER Diskette, je
 *     Spur verschieden.
 *
 *  3. FLEX Double Density — FM-Spuren haben WENIGER Sektoren bei gleicher
 *     Groesse; angehaengt werden Fuellsektoren aus 0xFF, damit jedes
 *     Zylinder/Kopf-Paar gleich viel liefert.
 *
 * ── Und der Baum ist an genau dieser Unterscheidung DREIMAL gescheitert ─
 *
 *     MF-1022  `sap`s Fuellsektor galt als GUTER Sektor mit gueltiger CRC
 *     MF-1038  36 erfundene Nullbytes je FDS-Seite als UFT_SECTOR_OK
 *     MF-1135  DMS meldete elf gute Sektoren mit erfundenen 0xE5 — die
 *              Warnung erreichte den Bediener, die Datenstruktur nicht
 *
 * Jedes Mal dieselbe Ursache: es gab kein Feld, in das die Wahrheit
 * gepasst haette.
 *
 * ── DER NULLWERT — die einzige Aenderung gegenueber der Zulieferung ─────
 *
 * Die Zulieferung hatte `UFT_SEC_FROM_MEDIUM = 0`, mit der Begruendung,
 * `calloc` ergebe dann „vom Medium". Genau das ist hier geaendert, und der
 * Grund steht im Header der Zulieferung selbst: „ein Fuellsektor kann nie
 * versehentlich als gelesener Sektor gelten." Mit der Null auf
 * FROM_MEDIUM kann er es doch — jede vergessene Zuweisung, jedes
 * `memset(0)`, jeder neu angehaengte Eintrag BEHAUPTET dann, vom Medium zu
 * stammen.
 *
 * Das ist nicht hypothetisch, sondern die Bauform, an der dieser Baum
 * dreimal gescheitert ist: `uft_sector_status_t` hat `UFT_SECTOR_OK = 0`,
 * und `uft_format_add_sector()` setzt ihn unbedingt — MF-1022, MF-1038 und
 * MF-1135 sind alle drei Folgen davon. Ein Werkzeug, dessen Prinzip „Keine
 * erfundenen Daten" lautet, darf seine sicherste Aussage nicht an den
 * Zustand knuepfen, der beim Vergessen entsteht.
 *
 * Deshalb ist der Nullwert `UFT_SEC_UNBEKANNT`, und er zaehlt NICHT als
 * Nutzlast und verhindert das flache Ablegen. Eine Spur, deren Herkunft
 * niemand gesetzt hat, ist damit sichtbar unbrauchbar statt unsichtbar
 * falsch.
 *
 * **Das ist eine Entwurfsentscheidung und in einer Zeile umkehrbar** — wer
 * die Vorgabe der Zulieferung will, tauscht die ersten zwei Werte und
 * aendert die zwei Zusagen in `tests/test_track_layout.c`, die genau das
 * festnageln.
 *
 * ── Was dieses Modul NICHT ist ──────────────────────────────────────────
 *
 * Es hat **keinen Produktionsaufrufer**. Sein einziger Aufrufer ist
 * `tests/test_track_layout.c`, und das ist beabsichtigt: das Vokabular
 * muss da sein, bevor das naechste Format es braucht — sonst entsteht der
 * vierte Einzelfall neben MF-1022, MF-1038 und MF-1135.
 *
 * Es ersetzt `uft_track_t` (`uft_format_plugin.h`) NICHT und wird von
 * keinem Format-Plugin erzeugt. Der naechstliegende Verdrahtungsort ist
 * gemessen und benannt: `uftc_convert_sectors_to_hfe()` fuellt eine zu
 * kurze Eingabe mit `memset(pad, 0xE5, sector_size)` auf und gibt die so
 * entstandenen Sektoren als gewoehnliche aus — die Warnung erreicht den
 * Bediener, die Datenstruktur nicht. Das ist woertlich die MF-1135-Lage,
 * heute, auf dem Hauptpfad. Siehe P3-422.
 */

#ifndef UFT_TRACK_LAYOUT_H
#define UFT_TRACK_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_TL_MAX_SECTORS_PER_TRACK  64u

/* Eigene Namen statt `UFT_ENC_FM`/`UFT_ENC_MFM`: die gibt es im Baum
 * bereits als Aliase in `uft_types.h`, und zwei Aufzaehlungen mit gleichem
 * Namen waeren genau die Kollisionsklasse, die `audit_guard_kollision.py`
 * und `audit_typkollision.py` bewachen. */
typedef enum {
    UFT_ENC_TL_FM = 0,      /**< Single Density */
    UFT_ENC_TL_MFM,         /**< Double/High Density */
    UFT_ENC_TL_GCR
} uft_tl_encoding_t;

typedef enum {
    /** NIEMAND hat die Herkunft gesetzt. Der Nullwert, damit eine
     *  vergessene Zuweisung sichtbar unbrauchbar ist statt unsichtbar
     *  falsch. Zaehlt nicht als Nutzlast; verhindert das flache Ablegen. */
    UFT_SEC_UNBEKANNT = 0,
    /** Vom Medium gelesen. Die einzige Herkunft, die eine Aussage ueber
     *  die Diskette traegt — und deshalb ausdruecklich zu setzen. */
    UFT_SEC_FROM_MEDIUM,
    /** Vom Treiber/Konverter angehaengte Fuellung, damit Spuren gleich
     *  lang wirken (FLEX-DD-Fall). Enthaelt KEINE Mediendaten. */
    UFT_SEC_PADDING_SECTOR,
    /** Echter Sektor, aber hinten mit Fuellbytes aufgefuellt
     *  (Slogger/LSI-2-Fall): `size` ist die Ablagegroesse,
     *  `payload_size` die tatsaechlich gelesene. */
    UFT_SEC_PADDED_TAIL,
    /** Platz vorgesehen, aber nichts gelesen — unformatiert oder defekt.
     *  Unterschied zu UNBEKANNT: hier ist GEMESSEN, dass nichts kam. */
    UFT_SEC_ABSENT
} uft_sec_origin_t;

typedef struct {
    uint8_t          id;            /**< Sektornummer auf dem Medium (1-basiert) */
    uint16_t         size;          /**< Ablagegroesse in Byte */
    uint16_t         payload_size;  /**< tatsaechlich vom Medium, <= size */
    uft_sec_origin_t origin;
} uft_tl_sector_t;

typedef struct {
    uint16_t          cyl;
    uint8_t           head;
    uft_tl_encoding_t encoding;     /**< je Spur, nicht je Diskette */
    uint8_t           count;        /**< belegte Eintraege in `sectors` */
    uft_tl_sector_t   sectors[UFT_TL_MAX_SECTORS_PER_TRACK];
} uft_tl_track_t;

/* ───────────────────────────── Abfragen ────────────────────────────────── */

/** Summe aller Ablagegroessen der Spur (was in der Datei stuende). */
uint32_t uft_tl_track_stored_bytes(const uft_tl_track_t *t);

/**
 * Summe der tatsaechlich vom Medium gelesenen Bytes.
 *
 * Fuellsektoren, abwesende UND unbekannte Sektoren zaehlen NICHT mit.
 * Genau hier entscheidet sich, ob das Werkzeug ueber die Diskette oder
 * ueber seinen eigenen Puffer Auskunft gibt.
 */
uint32_t uft_tl_track_payload_bytes(const uft_tl_track_t *t);

/** true, wenn alle Sektoren der Spur dieselbe Ablagegroesse haben. */
bool uft_tl_track_is_uniform(const uft_tl_track_t *t);

/** Zahl der Sektoren mit der angegebenen Herkunft. */
uint8_t uft_tl_count_origin(const uft_tl_track_t *t, uft_sec_origin_t o);

/**
 * Prueft, ob eine Spur als Abbild mit fester Sektorgroesse abgelegt werden
 * KANN, ohne dass Fuellung als Daten erscheint.
 * @param out_reason optional: Klartextgrund bei false.
 */
bool uft_tl_can_flatten(const uft_tl_track_t *t, const char **out_reason);

/* ───────────────────────── Vorlagen der belegten Faelle ────────────────── */

/**
 * Ensoniq SQ80: @p count Sektoren, alle 1024 Byte ausser dem letzten,
 * der @p last_size traegt.
 */
void uft_tl_make_sq80(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                      uint8_t count, uint16_t last_size);

/**
 * Slogger DDCPM / Computer Automation LSI-2: FM-Spur, physische Groesse
 * halbiert, auf @p stored_size mit Fuellbytes aufgefuellt.
 */
void uft_tl_make_fm_padded_tail(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                                uint8_t count, uint16_t stored_size);

/**
 * FLEX Double Density: FM-Spur mit @p real Sektoren, aufgefuellt auf
 * @p target durch Fuellsektoren.
 */
void uft_tl_make_flex_dd_fm(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                            uint8_t real, uint8_t target, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_LAYOUT_H */
