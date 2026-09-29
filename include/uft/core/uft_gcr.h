/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_gcr.h
 * @brief GCR-Codec-Register — die Wortmengen als PRAEDIKAT, nicht als Tafel
 *        (MF-1509).
 *
 * ── Warum es das gibt ─────────────────────────────────────────────────
 *
 * Dieselbe GCR-Tafel steht mehrfach im Baum: die CBM-Encode-Tafel an 12
 * Stellen, die Decode-Tafel an 10, die Apple-6&2-Tafel an 7 (gemessen
 * MF-1506, `scripts/konstantenfamilien_grundlinie.json`). Kopien driften,
 * und die Abweichung sieht dann aus wie ein Fehler in den DATEN — die
 * teuerste Verwechslung, die dieser Baum kennt (MF-1177).
 *
 * Zusammenfuehren heisst hier NICHT „eine Tafel fuer alle", sondern: die
 * Wortmenge entsteht aus einem Praedikat, das an EINER Stelle steht. Eine
 * Frage je Wort, die Menge durch Aufzaehlen. Damit gibt es nichts mehr,
 * was auseinanderlaufen koennte.
 *
 * ── Die drei Codecs, je mit gemessener Regel ──────────────────────────
 *
 * **Commodore 5/4** (1541/1571/1581) — 4 Datenbits, 5 Plattenbits:
 *
 *     keine zwei fuehrenden Nullen · keine zwei abschliessenden ·
 *     nie drei Nullen in Folge · **nicht 11111**
 *
 * Die vierte Bedingung wird gern vergessen und ist die wichtigste: ein
 * Datenwort aus lauter Einsen koennte mit dem naechsten zusammen eine
 * SYNC-Marke bilden (zehn Einsen in Folge), die der 1541-Kopf als
 * Spuranfang liest. Ohne sie liefert die Regel 17 statt 16 Woerter
 * (gemessen MF-1507).
 *
 * **Apple 6&2** (Disk II 16 Sektoren, 3,5 Zoll, Macintosh) — 6 Bit auf 8:
 *
 *     Bit 7 gesetzt · hoechstens EIN Paar benachbarter Nullen ·
 *     mindestens ein Einserpaar in Bit 6..0
 *
 * Zwei Feinheiten, ohne die die Regel 74 statt 64 Woerter liefert:
 * „hoechstens ein Nullenpaar" ist schaerfer als „keine drei Nullen"
 * (drei Nullen enthalten zwei Paare, umgekehrt gilt es nicht), und das
 * Einserpaar zaehlt OHNE Bit 7 — das ist ohnehin gesetzt und wuerde die
 * Bedingung sonst von selbst erfuellen.
 *
 * **Apple 5&3** (DOS 3.2, 13 Sektoren) — 5 Bit auf 8:
 *
 *     Bit 7 gesetzt · KEIN Paar benachbarter Nullen ·
 *     mindestens ein Einserpaar in Bit 6..0
 *
 * Unterscheidet sich von 6&2 in genau einer Bedingung und ist damit eine
 * echte Teilmenge davon (gemessen MF-1509).
 *
 * ── Die eine Tafel, die bleibt, und warum ─────────────────────────────
 *
 * Bei Apple ist die ZUORDNUNG Regel: Index k ist das k-te gueltige Byte
 * in aufsteigender Reihenfolge — fuer 6&2 und 5&3 gemessen, beide Male
 * byteweise identisch mit der Tafel im Baum.
 *
 * Bei Commodore ist sie es NICHT: die Menge folgt der Regel, die
 * Zuordnung 0..F ist Commodores Wahl (gemessen, Gegenprobe zur Apple-
 * Messung). Sie bleibt deshalb eine 16-Byte-Tafel — sie mit drei
 * Ausnahmen als Regel zu fassen waere Herleitung um der Herleitung
 * willen; 16 Byte sind ehrlicher und kuerzer.
 *
 * ── Dieselbe Frage beantwortet die Erkennung ──────────────────────────
 *
 * `uft_gcr_anteil_gueltig()` schickt einen Bitstrom durch das Praedikat
 * eines Codecs und liefert den Anteil gueltiger Kodewoerter. Commodore-
 * GCR unter dem Apple-Praedikat liefert massenhaft ungueltige Woerter und
 * umgekehrt — das ist eine ZAHL je Codec, kein Raten.
 *
 * **Und die Zahl beantwortet nicht jede Frage.** Ein Bitstrom, unter dem
 * JEDER Codec schlecht abschneidet, ist kein Codec-Problem, sondern
 * Rauschen oder ein Schutz mit absichtlich ungueltigem GCR (Fast Hack'em
 * `-f`, nibtools `-f`). Wer daraufhin den Codec wechselt, hat den Befund
 * gegen eine Einstellung getauscht. Aufrufer muessen den Fall
 * unterscheiden koennen; `uft_gcr_erkennung_t` traegt ihn deshalb
 * ausdruecklich.
 */
#ifndef UFT_CORE_UFT_GCR_H
#define UFT_CORE_UFT_GCR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Die im Register gefuehrten GCR-Verfahren. */
typedef enum {
    UFT_GCR_CBM_5_4 = 0,   /**< Commodore 1541/1571/1581: 4 Bit -> 5 Bit */
    UFT_GCR_APPLE_6_2,     /**< Apple Disk II 16 Sektoren, 3,5", Mac */
    UFT_GCR_APPLE_5_3,     /**< Apple DOS 3.2, 13 Sektoren */
    UFT_GCR_ANZAHL
} uft_gcr_codec_t;

/** Was ein Codec ueber sich sagt — fuer Anzeige und Protokoll. */
typedef struct {
    const char *name;        /**< „Commodore 5/4“ */
    const char *familie;     /**< „Commodore“ / „Apple“ */
    uint8_t datenbits;       /**< Nutzbits je Kodewort (4, 6, 5) */
    uint8_t plattenbits;     /**< Bits auf der Platte (5, 8, 8) */
    uint16_t woerter;        /**< Groesse der Wortmenge (16, 64, 32) */
    const char *regel;       /**< die Bedingungen, in Worten */
    const char *beleg;       /**< woran die Regel gemessen wurde */
    bool zuordnung_ist_regel; /**< true: Index k = k-tes gueltiges Wort */
} uft_gcr_info_t;

/**
 * @brief Ist @p wort ein gueltiges Kodewort dieses Codecs?
 *
 * Das Praedikat. Die EINE Stelle, an der die Regel steht — alles andere
 * in diesem Register zaehlt nur auf.
 *
 * @param codec  Verfahren.
 * @param wort   Kodewort, in den unteren `plattenbits` Bits.
 * @return true, wenn es auf der Platte vorkommen darf.
 */
bool uft_gcr_wort_gueltig(uft_gcr_codec_t codec, uint32_t wort);

/** @brief Auskunft ueber einen Codec; NULL bei unbekanntem @p codec. */
const uft_gcr_info_t *uft_gcr_info(uft_gcr_codec_t codec);

/**
 * @brief Die Wortmenge, aufsteigend.
 *
 * Erzeugt durch Aufzaehlen ueber das Praedikat — keine gespeicherte
 * Tafel. Der Aufwand ist einmalig 32 bzw. 256 Praedikataufrufe.
 *
 * @param codec Verfahren.
 * @param out   Ziel, mindestens `woerter` Eintraege.
 * @param max   Groesse von @p out.
 * @return Zahl der geschriebenen Woerter, 0 bei zu kleinem Puffer.
 */
size_t uft_gcr_wortmenge(uft_gcr_codec_t codec, uint8_t *out, size_t max);

/**
 * @brief Das Kodewort zum Datenwert @p index.
 *
 * Bei Apple die Regel (k-tes gueltiges Byte), bei Commodore die Tafel.
 *
 * @return Kodewort, oder 0xFF bei unzulaessigem @p index. 0xFF ist bei
 *         Commodore kein gueltiges Wort und bei Apple das letzte — wer
 *         Fehler unterscheiden muss, prueft @p index selbst.
 */
uint8_t uft_gcr_kodieren(uft_gcr_codec_t codec, uint8_t index);

/** Ungueltiges Kodewort — Rueckgabe von `uft_gcr_dekodieren()`. */
#define UFT_GCR_UNGUELTIG 0xFFu

/**
 * @brief Der Datenwert zu einem Kodewort — die Gegenrichtung.
 *
 * Die Rueckwaerts-Zuordnung wird beim ersten Aufruf je Codec AUS DEM
 * PRAEDIKAT erzeugt und dann gehalten. Sie ist damit keine Kopie im
 * Quelltext, sondern eine Ableitung zur Laufzeit — und sie ist noetig,
 * weil ein Dekoder je Byte laeuft: eine lineare Suche ueber 256 Werte
 * waere hier hundertfacher Aufwand gegenueber einem Tafelzugriff.
 *
 * Die Erzeugung ist idempotent (dasselbe Ergebnis aus demselben
 * Praedikat). Laufen zwei Faeden gleichzeitig hinein, schreiben beide
 * dieselben Werte; der Zustand bleibt gueltig.
 *
 * @param codec Verfahren.
 * @param wort  Kodewort von der Platte.
 * @return Datenwert, oder `UFT_GCR_UNGUELTIG`, wenn @p wort dort nicht
 *         vorkommen darf. Der Wert ist absichtlich derselbe, den die
 *         bestehenden Dekodiertafeln des Baums fuer „ungueltig" fuehren.
 */
uint8_t uft_gcr_dekodieren(uft_gcr_codec_t codec, uint32_t wort);

/** Was die Messung ueber einen Bitstrom sagt. */
typedef struct {
    uft_gcr_codec_t codec;   /**< geprueftes Verfahren */
    uint32_t woerter;        /**< gepruefte Kodewoerter */
    uint32_t gueltig;        /**< davon gueltig */
    float anteil;            /**< gueltig / woerter, 0..1 */
} uft_gcr_erkennung_t;

/**
 * @brief Anteil gueltiger Kodewoerter eines Bitstroms unter @p codec.
 *
 * Die Grundlage der Codec-Erkennung: eine Zahl je Verfahren statt einer
 * Vermutung. Gelesen wird MSB zuerst, ohne Ueberlappung.
 *
 * **Die Zahl sagt nicht, ob es dieser Codec IST** — sie sagt, wie gut er
 * passt. Ein niedriger Wert bei ALLEN Codecs heisst Rauschen oder
 * absichtlich ungueltiges GCR (Kopierschutz), nicht „falscher Codec“.
 *
 * @param codec  Verfahren.
 * @param bits   Bitstrom, MSB zuerst je Byte.
 * @param nbits  Zahl der Bits.
 * @param out    Ergebnis; darf NULL sein.
 * @return Anteil 0..1, oder -1.0f bei unbekanntem Codec oder zu kurzem
 *         Strom (weniger als ein Kodewort).
 */
float uft_gcr_anteil_gueltig(uft_gcr_codec_t codec, const uint8_t *bits,
                             size_t nbits, uft_gcr_erkennung_t *out);

/**
 * @brief Alle Codecs gegen einen Bitstrom messen, bester zuerst.
 *
 * @param bits  Bitstrom.
 * @param nbits Zahl der Bits.
 * @param out   Feld mit mindestens `UFT_GCR_ANZAHL` Eintraegen.
 * @return Zahl der gefuellten Eintraege, 0 bei zu kurzem Strom.
 */
size_t uft_gcr_erkennen(const uint8_t *bits, size_t nbits,
                        uft_gcr_erkennung_t out[UFT_GCR_ANZAHL]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CORE_UFT_GCR_H */
