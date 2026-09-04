/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_encoding_caps.h
 * @brief Was kann dieses Werkzeug mit einer Kodierung wirklich? (MF-865)
 *
 * ── Warum es diese Datei gibt ────────────────────────────────────────
 *
 * Eine Kodierung zu BENENNEN ist etwas anderes, als sie lesen zu koennen.
 * Der Baum hat das jahrelang nicht unterschieden, und es hatte Folgen.
 *
 * `M2FM` steht in **elf** Aufzaehlungen ueber den ganzen Baum verteilt.
 * Gemessen (MF-865, ueber `git ls-files`):
 *
 *   - `grep M2FM` ueber `src/flux/`            → **null Treffer**
 *   - die Aufzaehlung des Flusspfads selbst
 *     (`flux_encoding_t`) kennt M2FM **gar nicht**
 *   - **niemand setzt** `ENCODING_M2FM` / `UFT_ENC_M2FM` je als
 *     ERGEBNIS — alle Fundstellen sind `case`-Zweige, Namenstabellen
 *     oder ein GUI-Eintrag
 *
 * M2FM ist damit weder dekodierbar **noch erkennbar**. (Der Bericht, der
 * den Fall aufbrachte, nannte es „Histogramm-Erkennung vorhanden, kein
 * Dekoder" — auch die erste Haelfte traegt nicht.)
 *
 * Trotzdem bot `src/uft_flux_histogram_widget.cpp:698` es zur Auswahl an,
 * und `setEncodingHint()` schreibt die Auswahl direkt nach
 * `m_detectedEncoding`. Der Benutzer las danach **„Encoding: M2FM"** —
 * seine eigene Vorgabe, zurueckgegeben als Erkennung.
 *
 * Dasselbe gilt fuer `UFT_ENC_GCR_VICTOR`: der Bezeichner kommt in
 * **keiner** `.c`/`.cpp`-Datei des Baums vor.
 *
 * ── Was diese Tabelle ist und was nicht ──────────────────────────────
 *
 * Sie ist eine **Erklaerung**, kein Schalter: sie aendert am Lesepfad
 * nichts. Sie erlaubt der Oberflaeche und der Erkennung, zu sagen, was
 * sie NICHT koennen, statt stillschweigend nichts zu liefern.
 *
 * ── Und sie wird gemessen, nicht gepflegt ────────────────────────────
 *
 * Eine von Hand gefuehrte Faehigkeitstabelle waere genau das Muster, das
 * dieser Baum siebzehnmal als Ursache gefunden hat: eine Aufzaehlung
 * bekannter Faelle, die still veraltet. Deshalb prueft
 * `scripts/audit_encoding_caps.py` die Spalte `can_decode` gegen den
 * **Verteiler selbst** — gegen die `switch`-Zweige in
 * `flux_decode_track()`. Wer einen Dekoder anschliesst und die Tabelle
 * vergisst, faellt dort auf; wer einen entfernt, ebenso.
 */
#ifndef UFT_CORE_ENCODING_CAPS_H
#define UFT_CORE_ENCODING_CAPS_H

#include <stdbool.h>

#include "uft/core/uft_track_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Was das Werkzeug mit einer Kodierung kann.
 *
 * `can_detect` heisst: irgendein Pfad im Baum SETZT diesen Wert als
 * Ergebnis. Nicht: „es gibt einen Aufzaehlungseintrag dafuer".
 *
 * `can_decode` heisst: der Verteiler `flux_decode_track()` routet ihn an
 * einen Dekoder, der Sektoren anlegt.
 */
typedef struct {
    uft_track_encoding_t enc;
    bool                 can_detect;
    bool                 can_decode;
    /** Kurze Begruendung, wenn etwas fehlt — sonst NULL. */
    const char          *grenze;
} uft_encoding_caps_t;

/**
 * @brief Faehigkeiten einer Kodierung.
 * @return Nie NULL. Fuer unbekannte Werte ein Eintrag mit beiden
 *         Faehigkeiten `false` und einer Begruendung.
 */
const uft_encoding_caps_t *uft_encoding_caps(uft_track_encoding_t enc);

/** @brief Kurzform: setzt irgendein Pfad diesen Wert als Ergebnis? */
bool uft_encoding_can_detect(uft_track_encoding_t enc);

/** @brief Kurzform: fuehrt der Verteiler ihn an einen echten Dekoder? */
bool uft_encoding_can_decode(uft_track_encoding_t enc);

/**
 * @brief Ein Satz fuer die Oberflaeche, oder NULL.
 *
 * Beispiel: „M2FM: im Baum benannt, aber weder erkannt noch dekodiert."
 * Gedacht fuer Anzeigen, die sonst nur den Namen zeigen wuerden.
 */
const char *uft_encoding_grenze(uft_track_encoding_t enc);

/** @brief Zahl der gefuehrten Eintraege (fuer Tests und Tore). */
size_t uft_encoding_caps_count(void);

/** @brief Eintrag @p i, oder NULL. Reihenfolge ist die Tabellenordnung. */
const uft_encoding_caps_t *uft_encoding_caps_at(size_t i);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CORE_ENCODING_CAPS_H */
