/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_cbm_track_segment.h
 * @brief Zerlegt eine rohe Commodore-GCR-Spur in ihre Bestandteile
 *        (MF-1333, Stufe 2).
 *
 * ── Warum es das braucht ─────────────────────────────────────────────
 *
 * Gemessen am 2026-09-21 hatte dieser Baum VIER GCR-Dekoder
 * (`uft_d64_g64.c:936` und `:1408`, `uft_g64.c:597`,
 * `uft_flux_decoder.c:1503`), und **alle vier springen ueber die Luecke
 * hinweg**, ohne sie zu zaehlen oder abzulegen:
 *
 *     while (pos < gcr_length && gcr_data[pos] != 0xFF) pos++;
 *
 * Die SCHREIBseite hat das Modell dagegen vollstaendig und benannt
 * (`D64_GAP1_LENGTH`, `D64_GAP2_LENGTH` in `include/uft/uft_d64_writer.h`
 * :96-99). UFT konnte Luecken also erzeugen und nicht zurueckmessen.
 *
 * Das ist kein stiller Verlust — der G64-Lesepfad haelt die ganze rohe
 * Spur (`uft_g64.c:541`, `track->raw_data`) —, sondern eine fehlende
 * Auswertung: Klasse P3-204 ("Arbeit, kein Fehler").
 *
 * ── Die benannte Referenz fuer die Anordnung ─────────────────────────
 *
 * `src/formats/uft_d64_writer.c:358-409` (`d64_write_track_gcr`) baut je
 * Sektor GENAU diese Folge, und sie ist die kanonische 1541-Anordnung:
 *
 *     Sync  (sync_len Byte 0xFF)
 *     Kopf  (10 GCR-Byte  = 8 Klarbyte)
 *     Gap1  (gap1 Byte, vorgabeweise 0x55)
 *     Sync  (sync_len Byte 0xFF)
 *     Daten (325 GCR-Byte = 260 Klarbyte)
 *     Gap2  (gap2 Byte)
 *
 * Blockkennungen: `D64_HEADER_MARK 0x08`, `D64_DATA_MARK 0x07`
 * (`include/uft/uft_d64_writer.h:46-47`).
 *
 * ── Wofuer die Luecken gebraucht werden ──────────────────────────────
 *
 * Der GEOS-Bootschutz liegt ausschliesslich dort. Die Beschreibung des
 * Urhebers (Christian Meilinger, `neue-ideen/geocopy.zip ->
 * geocopy/READ.ME.cvt`; Kanal *Spec* nach MF-695, NC-Klausel, kein Port)
 * nennt drei unterscheidbare Faelle auf Spur 21:
 *
 *     Original          ... $55 $55 $67 $55 $55 $67 SYNC ...
 *     Standardformat    ... $55 $55 $55 $55 $55 $55 SYNC ...
 *     GeoCopy-Nachbau   ... $67 $67 $67 $67 $67 $67 SYNC ...
 *
 * ── Die Zusage, die dieses Modul gibt ────────────────────────────────
 *
 * **Jedes Bit der Spur gehoert zu genau einem Segment.** Die Segmente
 * sind luecken- und ueberlappungsfrei, und ihre Laengen summieren sich
 * auf die Spurlaenge. Das ist mehr als eine Summenprobe: MF-1026 hat
 * gemessen, dass eine aufgehende Summe nichts ueber die Verteilung
 * darin sagt (zwei Zonengrenzen um +1 und -1 daneben, Summe stimmte,
 * 22 Spuren falsch). Deshalb pruefen die Tests BEIDES — die Summe und
 * die Lage jedes einzelnen Segments.
 *
 * ── Die Grenze zwischen Luecke und Sync, und warum sie wandert ───────
 *
 * Ein Sync ist ein Lauf aus 1-Bits. Endet das letzte Lueckenbyte
 * selbst auf 1-Bits, gehoeren die zum Lauf — der Controller sieht sie
 * nicht anders. Gemessen an einer Spur dieses Baums:
 *
 *     Fuellbyte 0x55 = 0101 0101 -> 1 abschliessendes 1-Bit
 *     Fuellbyte 0x67 = 0110 0111 -> 3 abschliessende 1-Bits
 *
 * Eine 9 Byte lange Luecke aus 0x55 meldet deshalb 71 Bit und 8 ganze
 * Bytes, eine aus 0x67 sogar nur 69 Bit. **Das ist kein Messfehler,
 * sondern die physikalische Lage**; ein byteweiser Zerleger verdeckt
 * sie bloss. Die angrenzenden Sync-Laeufe sind entsprechend 41 bzw.
 * 43 Bit lang statt 40.
 *
 * Fuer die Unterscheidung der drei Muster aendert das NICHTS — sie
 * haengt am Verhaeltnis der Bytes, nicht an ihrer Zahl (gemessen:
 * Standard 8x 0x55, GeoCopy 8x 0x67, Original 6x 0x55 + 2x 0x67).
 * Wer die GESCHRIEBENE Lueckenlaenge braucht, muss die verschmolzenen
 * Bits dazurechnen — und das waere eine Annahme ueber Byteausrichtung,
 * die dieses Modul bewusst nicht trifft.
 *
 * ── Was dieses Modul NICHT tut ───────────────────────────────────────
 *
 * Es deutet nichts. Es sagt, WO eine Luecke liegt, WIE LANG sie ist und
 * WELCHE Bytes darin stehen. Ob daraus ein GEOS-Schutz folgt, entscheidet
 * eine andere Schicht — mit einer eigenen, benannten Regel.
 */

#ifndef UFT_CBM_TRACK_SEGMENT_H
#define UFT_CBM_TRACK_SEGMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Ein Sync ist ein Lauf aus mindestens so vielen 1-Bits.
 *
 *  Der 1541-Controller synchronisiert auf 10 aufeinanderfolgende
 *  1-Bits; der Schreiber dieses Baums legt vorgabeweise 5 Byte = 40
 *  Bits (`d64_writer_config_t.sync_length`). 10 ist die physikalische
 *  Schranke und deshalb die richtige Untergrenze — eine hoehere wuerde
 *  echte kurze Syncs verschlucken. */
#define UFT_CBM_SYNC_MIN_BITS   10

/** Laenge eines Kopfblocks in GCR-Bytes (8 Klarbyte). */
#define UFT_CBM_HEADER_GCR      10
/** Laenge eines Datenblocks in GCR-Bytes (260 Klarbyte). */
#define UFT_CBM_DATA_GCR        325

/** Obergrenze der Segmente je Spur. Wird sie ueberschritten, SAGT das
 *  Modul ab, statt die Spur zu kappen (MF-1040/MF-1224: absagen statt
 *  stillschweigend kuerzen). 21 Sektoren erzeugen 127 Segmente. */
#define UFT_CBM_SEG_MAX         4096

typedef enum {
    UFT_CBM_SEG_SYNC = 0,   /**< Lauf aus >= UFT_CBM_SYNC_MIN_BITS 1-Bits */
    UFT_CBM_SEG_HEADER,     /**< Kopfblock, Kennung 0x08                  */
    UFT_CBM_SEG_DATA,       /**< Datenblock, Kennung 0x07                 */
    UFT_CBM_SEG_GAP,        /**< Luecke: alles zwischen Block und Sync    */
    UFT_CBM_SEG_UNKNOWN     /**< hinter einem Sync stand weder 0x08 noch 0x07 */
} uft_cbm_segment_art_t;

typedef struct {
    uft_cbm_segment_art_t art;

    /** Bitversatz vom Spuranfang. */
    size_t bit_start;
    /** Laenge in Bits. Die Summe aller Laengen ist die Spurlaenge. */
    size_t bit_len;

    /* ── nur bei UFT_CBM_SEG_HEADER gueltig ───────────────────────── */
    uint8_t  kopf_spur;     /**< Spurnummer aus dem Kopfblock   */
    uint8_t  kopf_sektor;   /**< Sektornummer aus dem Kopfblock */
    bool     kopf_gelesen;  /**< false = Felder nicht gemessen  */

    /* ── nur bei UFT_CBM_SEG_GAP gueltig ──────────────────────────── */
    /** Haeufigstes Byte der Luecke. Bei Bitversatz ohne Byteausrichtung
     *  wird vom Lueckenanfang aus in 8-Bit-Schritten gelesen. */
    uint8_t  fuellbyte;
    /** true, wenn die Luecke NUR aus @ref fuellbyte besteht. Genau hier
     *  trennen sich Standardformat (einheitlich 0x55) und
     *  GeoCopy-Nachbau (einheitlich 0x67) vom Original (gemischt). */
    bool     einheitlich;
    /** Laenge der Luecke in ganzen Bytes (bit_len / 8). */
    uint16_t byte_zahl;
    /** Wie oft 0x55 bzw. 0x67 in der Luecke vorkommen — die beiden
     *  Werte, auf die GEOS laut seinem Urheber prueft. */
    uint16_t zahl_55;
    uint16_t zahl_67;
} uft_cbm_segment_t;

typedef struct {
    uft_cbm_segment_t *segmente;   /**< dynamisch, mit freigeben() loeschen */
    size_t             anzahl;
    size_t             kapazitaet;

    /** Spurlaenge in Bits, wie sie hineingegeben wurde. */
    size_t bits_gesamt;
    /** Summe aller Segmentlaengen. MUSS gleich @ref bits_gesamt sein —
     *  die Zusage dieses Moduls. */
    size_t bits_erfasst;

    unsigned sync_laeufe;
    unsigned koepfe;
    unsigned datenbloecke;
    unsigned luecken;
    unsigned unbekannt;
} uft_cbm_track_segmentierung_t;

/**
 * @brief Zerlegt eine rohe GCR-Spur.
 *
 * @param roh        Rohe Spurbytes (z. B. `uft_track_t.raw_data`).
 * @param roh_bytes  Laenge in Bytes.
 * @param roh_bits   Laenge in Bits. 0 bedeutet `roh_bytes * 8` — das ist
 *                   die uebliche Lage bei G64. Ein Format, das die
 *                   Bitlaenge KENNT (ADF-Ext, IPF), reicht sie durch;
 *                   MF-1222 hat gemessen, was es kostet, sie wegzuwerfen.
 * @param aus        Ergebnis. Wird vor dem Fuellen genullt.
 *
 * @return `UFT_OK`;
 *         `UFT_ERR_INVALID_ARG` bei Nullzeigern oder wenn `roh_bits`
 *         groesser ist als `roh_bytes * 8` — eine Spur, die mehr Bits
 *         beansprucht als sie speichert, wird ABGESAGT statt gekappt
 *         (dieselbe Entscheidung wie MF-1222 bei `adf_ext`);
 *         `UFT_ERR_MEMORY`, wenn das Segmentfeld nicht wachsen kann;
 *         `UFT_ERR_RESOURCE`, wenn mehr als @ref UFT_CBM_SEG_MAX
 *         Segmente entstuenden — dann wird abgesagt und nichts
 *         zurueckgegeben, nicht gekappt (MF-1040/MF-1224).
 */
uft_error_t uft_cbm_track_segmentieren(const uint8_t *roh,
                                       size_t roh_bytes,
                                       size_t roh_bits,
                                       uft_cbm_track_segmentierung_t *aus);

/** Gibt das Segmentfeld frei und nullt die Struktur. Mehrfachaufruf ist
 *  erlaubt, ebenso ein NULL-Zeiger. */
void uft_cbm_segmentierung_freigeben(uft_cbm_track_segmentierung_t *s);

/** Name einer Segmentart, fuer Berichte. Nie NULL. */
const char *uft_cbm_segment_art_name(uft_cbm_segment_art_t art);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_TRACK_SEGMENT_H */
