/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_roland_s_metadata.h
 * @brief Was auf einer Roland-S-Diskette STEHT — nicht, wie sie liegt.
 *
 * ── Was dieses Modul ist, und was ausdruecklich nicht ──────────────────
 *
 * Roland S-50/S-330/S-550/W-30 liegen physisch als gewoehnliches MFM auf
 * 512-Byte-Sektoren (80x2x9, 737 280 Byte). Es gibt hier also NICHTS neu
 * zu dekodieren: kein Behaelterformat, kein Plugin, keine Geometrie.
 *
 * Was fehlt, ist die SEMANTISCHE Auswertung — Patches, Tones, Etikett.
 * Genau das und nichts anderes steht hier. Dieses Modul
 *
 *   - liest KEINE Geometrie, KEINEN Fluss, KEINE CRC, KEINE Weak Bits,
 *   - registriert KEIN Format-Plugin (das Moratorium der EINFRIER-REGEL
 *     bleibt damit unberuehrt — es gibt keinen neuen Behaelter),
 *   - beansprucht KEIN Dateisystem: es gibt kein Verzeichnis, keine
 *     Sampledaten-Zuordnung, keinen Export und keinen Schreibweg. Wer
 *     daraus `UFT_CAP_FILESYSTEM` oder `FileCopy` ableitet, behauptet
 *     etwas, das dieser Code nicht kann.
 *
 * ── Erkennung: EINE Quelle, nicht zwei ─────────────────────────────────
 *
 * Die Vorlage brachte eine eigene Sonde mit, die vier Zeichen ab Versatz
 * 4 vergleicht und danach fest 95 % Konfidenz vergibt. Die ist NICHT
 * uebernommen, und zwar aus einem Grund, der in diesem Baum schon
 * entschieden ist: nach `docs/SONDEN_DOKTRIN.md` (Eigentuemer-
 * Entscheidung 2026-09-15) wird eine Konfidenz ABGELEITET, nie vergeben.
 *
 * Gemessen ist das vorhandene `uft_roland_identify()` strenger:
 *   - die Dateigroesse muss exakt `UFT_ROLAND_IMAGE_SIZE` sein,
 *   - ZWEI 4-Byte-Schluessel muessen passen (`key_lo` UND `key_hi`),
 *   - die Konfidenz kommt aus `uft_probe_konfidenz(UFT_BELEG_KENNUNG)`.
 *
 * `uft_roland_s_analyze()` ruft deshalb `uft_roland_identify()` und hat
 * keine eigene Erkennung. Zwei Erkenner nebeneinander waeren die Bauform
 * aus MF-1015 (drei Pruefsummen) und MF-1026 (drei Victor-Geometrien):
 * sie driften, und dann sieht eine Abweichung wie ein Datenfehler aus.
 *
 * ── Ganzzahlen statt Fliesskomma ───────────────────────────────────────
 *
 * Die Vorlage speichert Sample-Dauern als `double` (`param[6] * 0.4`).
 * Hier sind es Millisekunden als `uint32_t` (`param[6] * 400`). Der Grund
 * ist nicht Geschmack: JSON-Ausgabe, Vergleiche und Testzusagen muessen
 * deterministisch sein, und ein `double` ist es ueber Plattformen hinweg
 * nicht zuverlaessig.
 *
 * ── WAS HIER NICHT BELEGT IST (MF-498 b) ───────────────────────────────
 *
 * Die Feldversaetze stammen aus der Vorlage und sind gegen KEINE aeussere
 * Quelle geprueft — kein Werkzeug, keine Spezifikation, kein fremd
 * erzeugtes Abbild. Die Tests dieses Moduls bauen ihre Pruefdatei selbst
 * und pruefen damit, dass der Leser tut, was er zu tun behauptet — NICHT,
 * dass die Deutung der Roland-Felder stimmt. Das ist die Bauform aus
 * MF-1009 (`apridisk`) und MF-1028 (`qrst`): ein geschlossener Kreis.
 *
 * Solange das so ist, gilt dieses Modul als UNGEPRUEFT im Sinne der
 * EINFRIER-REGEL, und es darf nichts ableiten, was ueber „hier steht ein
 * Text" hinausgeht. Der Weg heraus ist ein echtes Roland-Abbild mit
 * bekanntem Inhalt oder ein fremdes Werkzeug, das dieselben Felder
 * nennt; beides steht aus.
 *
 * ── Lizenzlage, offen benannt ──────────────────────────────────────────
 *
 * Die Vorlage (`neue-ideen/Roland_S_UFT_Paket.zip`) deklariert
 * `GPL-3.0-or-later`. Dieser Baum steht unter GPL Version 2 (`LICENSE`),
 * 72 Dateien fuehren `GPL-2.0-or-later`. GPL-3-Code hineinzunehmen hoebe
 * das Gesamtwerk auf GPL-3. Diese Fassung traegt deshalb
 * `GPL-2.0-or-later` — unter der ANNAHME, dass der Eigentuemer als
 * Urheber der Vorlage umlizenziert. Die Annahme ist hier benannt und
 * nicht gemessen; sie gehoert bestaetigt.
 */

#ifndef UFT_ROLAND_S_METADATA_H
#define UFT_ROLAND_S_METADATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_ROLAND_S_SCHEMA_VERSION 1u
#define UFT_ROLAND_S_PATCH_COUNT    16u
#define UFT_ROLAND_S_TONE_COUNT     32u
#define UFT_ROLAND_S_LABEL_ROWS      5u
#define UFT_ROLAND_S_LABEL_COLUMNS  12u
#define UFT_ROLAND_S_MAX_WARNINGS   64u

/** Eine Laengeneinheit im Tone-Satz, in Millisekunden. Vorlage: 0,4 s. */
#define UFT_ROLAND_S_LENGTH_UNIT_MS 400u

typedef enum {
    UFT_ROLAND_S_OK = 0,
    UFT_ROLAND_S_INVALID_ARGUMENT,
    UFT_ROLAND_S_UNRECOGNIZED,   /**< `uft_roland_identify()` sagt nein   */
    UFT_ROLAND_S_TRUNCATED,      /**< Groesse passt nicht                 */
    UFT_ROLAND_S_IO_ERROR,
    UFT_ROLAND_S_NO_MEMORY
} uft_roland_s_status_t;

typedef enum {
    UFT_ROLAND_S_BANK_NONE = 0,
    UFT_ROLAND_S_BANK_A,
    UFT_ROLAND_S_BANK_B,
    UFT_ROLAND_S_BANK_UNKNOWN
} uft_roland_s_wave_bank_t;

typedef enum {
    UFT_ROLAND_S_WARNING_INVALID_SAMPLE_RATE = 1,
    UFT_ROLAND_S_WARNING_INVALID_WAVE_BANK,
    UFT_ROLAND_S_WARNING_NONPRINTABLE_TEXT
} uft_roland_s_warning_kind_t;

typedef struct {
    unsigned number;                 /**< angezeigte Patchnummer          */
    bool     populated;
    char     name[13];               /**< 12 Zeichen + NUL                */
    bool     output_jack_available;  /**< false bei S-50                  */
    uint8_t  output_jack_raw;
    char     output_jack[3];         /**< "1".."8", "T" oder "?"          */
} uft_roland_s_patch_t;

typedef struct {
    unsigned number;                 /**< I11..I48, numerisch             */
    bool     populated;
    char     name[9];                /**< 8 Zeichen + NUL                 */
    uint8_t  original_tone_raw;
    unsigned original_tone_number;
    bool     is_subtone;
    uint8_t  sample_rate_raw;
    unsigned sample_rate_hz;         /**< 0 = unbekannt, NICHT „keine"    */
    uint8_t  wave_bank_raw;
    uft_roland_s_wave_bank_t wave_bank;
    uint8_t  sample_length_raw;
    bool     sample_length_available; /**< false bei 0xFF                 */
    uint32_t sample_duration_ms;      /**< 0, wenn nicht verfuegbar       */
} uft_roland_s_tone_t;

typedef struct {
    uft_roland_s_warning_kind_t kind;
    int  tone_number;                /**< 0 = diskettenweit               */
    char message[128];
} uft_roland_s_warning_t;

typedef struct {
    uint32_t schema_version;
    size_t   source_size;

    /* Aus `uft_roland_identify()` — die EINZIGE Erkennung. */
    char     model[16];              /**< z. B. "S-550"                   */
    char     content[24];            /**< z. B. "Sound"                   */
    bool     erkannt;

    char disk_label[UFT_ROLAND_S_LABEL_ROWS][UFT_ROLAND_S_LABEL_COLUMNS + 1u];
    uft_roland_s_patch_t patches[UFT_ROLAND_S_PATCH_COUNT];
    size_t   patch_count;
    uft_roland_s_tone_t  tones[UFT_ROLAND_S_TONE_COUNT];
    size_t   tone_count;

    uint32_t used_ms_a;              /**< Summe der Bank-A-Dauern         */
    uint32_t used_ms_b;

    uft_roland_s_warning_t warnings[UFT_ROLAND_S_MAX_WARNINGS];
    size_t   warning_count;
    bool     warnings_truncated;     /**< mehr als MAX_WARNINGS angefallen */
} uft_roland_s_report_t;

/**
 * @brief Die semantische Auswertung eines Roland-S-Sektorabbilds.
 *
 * Beginnt mit `uft_roland_identify()`. Schlaegt die Erkennung fehl, wird
 * NICHTS geparst — ein Bericht ueber eine Datei, die gar keine
 * Roland-Diskette ist, waere erfundene Auskunft.
 *
 * @param image      das vollstaendige Abbild
 * @param image_size seine Groesse; muss `UFT_ROLAND_IMAGE_SIZE` sein
 * @param report     Ziel; wird vollstaendig ueberschrieben, enthaelt
 *                   keine eigenen Zeiger
 */
uft_roland_s_status_t uft_roland_s_analyze(const uint8_t *image,
                                           size_t image_size,
                                           uft_roland_s_report_t *report);

/**
 * @brief Stabiles JSON zum Bericht. `*json_out` mit `free()` freigeben.
 *
 * Alle Zahlen sind Ganzzahlen; es gibt kein Fliesskomma in der Ausgabe.
 */
uft_roland_s_status_t uft_roland_s_report_to_json_alloc(
    const uft_roland_s_report_t *report, char **json_out,
    size_t *json_size_out);

const char *uft_roland_s_status_name(uft_roland_s_status_t status);
const char *uft_roland_s_wave_bank_name(uft_roland_s_wave_bank_t bank);
const char *uft_roland_s_warning_name(uft_roland_s_warning_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ROLAND_S_METADATA_H */
