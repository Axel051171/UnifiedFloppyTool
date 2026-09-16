/**
 * @file uft_mfm_sector_parser.h
 * @brief Standalone MFM IDAM/DAM sector extractor (MF-141 / AUD-002).
 *
 * The audit identified this as the largest functional gap in UFT v4.1.3:
 * the SCP/HFE/KryoFlux -> sector-image conversion pipeline existed in
 * skeleton form but had no clean MFM sector parser to call. The
 * convert_flux pipeline open-coded a partial parser inline that:
 *   - did not pair IDAM (0xFE) with the matching DAM/DDAM in the gap,
 *   - did not validate either CRC,
 *   - placed sectors in scan order, not by IBM sector ID R.
 *
 * This API provides a forensically honest, single-call sector decoder
 * over an arbitrary MFM-encoded bitstream. Callers feed in a packed
 * bitstream (MSB-first, the format produced by the existing
 * uft_pll_process_flux_mfm() PLL output) plus a caller-owned buffer
 * pool, and receive back an array of decoded sectors with explicit
 * CRC-validity flags.
 *
 * Forensic contract:
 *   - Sectors with bad CRC are still returned (with the corresponding
 *     flag set false), never silently dropped or rewritten. The caller
 *     decides whether to use, retry, or report them.
 *   - Sectors are NOT placed by R-position into a backing image — the
 *     caller does that. This API stays at "what was on the track"
 *     level; image-layout decisions are the conversion-pipeline's
 *     concern.
 *   - No data is fabricated to fill gaps. If a DAM is missing after a
 *     successful IDAM, the entry is returned with `data_len == 0`.
 *
 * Encoding contract:
 *   - The input bitstream is the raw MFM bitstream (i.e. the
 *     interleaved clock+data stream the PLL produces). The decoder
 *     is responsible for skipping the clock bits and re-assembling
 *     bytes from the data bits.
 *   - The 0xA1 sync mark with the missing-clock pattern is detected
 *     by its bit-level signature 0x4489 (clock+data interleaved).
 *
 * Not in scope (future work):
 *   - FM (single-density) decoding — separate API needed.
 *   - GCR (CBM/Apple) — separate API needed.
 *   - Track-write back-encoding — this is decoder-only.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef UFT_MFM_SECTOR_PARSER_H
#define UFT_MFM_SECTOR_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** „keine solche Stelle" — `first/last_deviating_bit`, wenn nichts
 *  abweicht. 0 waere als Sentinel falsch, weil Bit 0 eine gueltige
 *  Stelle ist. */
#define UFT_MFM_GAP_NO_BIT ((size_t)-1)

/**
 * @brief Was in einer Luecke zwischen zwei Marken wirklich steht
 *        (P3-453, MF-1190).
 *
 * WOZU
 * ----
 * Eine Schreibnaht ist die Stelle, an der ein Laufwerk beim Nachschreiben
 * EINES Sektors das Schreibtor ein- oder ausgeschaltet hat. Im Fluss
 * sieht man sie als Zeitluecke (`uff_detect_splices()`), in den
 * OTDR-Spuren als Sprung (`uft_deepread_detect_splice()`) — beide
 * Erkenner haben gemessen **0** Produktivaufrufer, und fuer einen
 * BITSTROM tragen sie ohnehin nicht. Im Bitstrom sieht man die Naht
 * daran, dass die gleichmaessige Fuellung der Luecke an einer Stelle
 * unterbrochen ist.
 *
 * WAS HIER STEHT, IST EINE MESSUNG — KEIN URTEIL
 * ----------------------------------------------
 * Es gibt in dieser Struktur **kein** Feld `splice` und **keine**
 * Schwelle, und das ist die Entscheidung, nicht ihr Fehlen. Die
 * Zulieferung `DiskImageTool-extrakt.zip` (GPL-2.0-or-later) liefert
 * `UFT_SPLICE_WORDS 2`, `UFT_SPLICE_MIN_BAD 2`, `UFT_GAP_MAX_VALUES 8`
 * und die Quoten 0 %/75 % — und sagt ueber die letzten beiden woertlich,
 * sie seien „nicht an einem echten Traeger kalibriert". Fuenf Zahlen ohne
 * Quelle sind fuenf Aussagen ohne Quelle (S1, `P3-451`), also kommt keine
 * davon mit. Gemeldet wird, was dasteht; die Deutung braucht einen
 * Beleg, den es noch nicht gibt.
 *
 * Der Aufbau ist deshalb schwellenfrei: `deviating` zaehlt, wie viele
 * Woerter vom haeufigsten abweichen, und `first/last_deviating_bit`
 * sagen WO. Ob zwei abweichende Woerter am Lueckenende eine Naht sind,
 * entscheidet nicht diese Struktur.
 *
 * DIE GRENZEN UND DAS RASTER
 * --------------------------
 * Die Woerter werden **vom Ende her** auf einem 16-Bit-Raster gelesen,
 * weil die Marke der physikalisch bedeutsame Anker ist: der Vorlauf aus
 * Nullwoertern und die Sync-Marke sitzen am Lueckenende, nicht an ihrem
 * Anfang.
 *
 * `sync_nulls` sind die Nullwoerter (MFM `0xAAAA`) unmittelbar vor
 * `end_bit` — bei IBM System 34 sind das 12 (Encoder-Kopf). Sie gehoeren
 * zum Vorlauf der Marke und **nicht** zum Fuellteil; wer sie mitzaehlt,
 * sieht in jeder gesunden Luecke zwei verschiedene Woerter.
 *
 * `distinct == 0` heisst **nicht beurteilbar** (der Fuellteil traegt kein
 * volles Wort) und ist nicht dasselbe wie `deviating == 0` („nichts
 * weicht ab"). Dieselbe Unterscheidung wie MF-980 bei `0xE5` und die
 * Spaltenregel D6.
 *
 * DAS ERSTE WORT EINER LUECKE IST EIN SONDERFALL, UND ZWAR EIN ERKLAERTER
 * -----------------------------------------------------------------------
 * MFM schreibt eine Taktzelle nur, wenn das vorige UND das aktuelle
 * Datenbit 0 sind. Das erste Fuellwort einer Luecke haengt damit am
 * letzten Datenbit DAVOR — dem letzten Bit der CRC — und kann sich vom
 * Rest der Luecke unterscheiden, obwohl dasselbe Byte geschrieben wurde.
 *
 * Gemessen an einer vom hauseigenen Encoder geschriebenen Spur (MF-1190,
 * 9 Sektoren): das Fuellbyte 0x4E ergibt `0x9254`, nach einem Datenbit 1
 * dagegen `0x1254` — die Differenz ist `0x8000`, also GENAU die fuehrende
 * Taktzelle. Es trat in **5 von 9** Luecken auf, und zwar in genau den
 * fuenf, deren letztes Bit davor eine 1 war; in den anderen vier nicht.
 *
 * `leading_clock_only` sagt, dass dieser Fall vorliegt, und ein so
 * erklaertes Wort zaehlt NICHT in `deviating` — sonst meldete jede zweite
 * gesunde Luecke eine Naht. Es wird aber auch nicht verschwiegen:
 * `distinct` zaehlt es weiter mit, denn es STEHT dort. Eine echte Naht am
 * Lueckenanfang bleibt sichtbar, weil sie sich in mehr als dieser einen
 * Taktzelle unterscheidet.
 */
typedef struct {
    size_t   start_bit;      /**< erste Bitstelle der Luecke            */
    size_t   end_bit;        /**< erste Bitstelle DAHINTER (die Marke)  */
    uint32_t words;          /**< volle 16-Bit-Woerter im FUELLTEIL     */
    uint32_t sync_nulls;     /**< Nullwoerter am Ende (Marken-Vorlauf)  */
    uint32_t distinct;       /**< verschiedene Woerter im Fuellteil;
                              *   **0 = nicht beurteilbar**. Zaehlt ein
                              *   erklaertes erstes Wort MIT.           */
    uint32_t deviating;      /**< Woerter != `dominant_word`, OHNE das
                              *   durch die MFM-Taktregel erklaerte
                              *   erste Wort (siehe oben)               */
    uint16_t dominant_word;  /**< das haeufigste MFM-Wort im Fuellteil  */
    uint8_t  leading_clock_only; /**< 1 = das erste Fuellwort weicht NUR
                              *   in der fuehrenden Taktzelle ab (0x8000)
                              *   und ist damit erklaert, nicht auffaellig */
    size_t   first_deviating_bit; /**< `UFT_MFM_GAP_NO_BIT`, wenn keines */
    size_t   last_deviating_bit;  /**< `UFT_MFM_GAP_NO_BIT`, wenn keines */
} uft_mfm_gap_t;

/**
 * @brief Decoded MFM sector record.
 *
 * One entry per IDAM seen on the track, regardless of CRC validity.
 * The caller inspects the `*_crc_ok` flags to decide what to trust.
 */
typedef struct {
    uint8_t  cylinder;     /**< CHRN.C — track number from IDAM */
    uint8_t  head;         /**< CHRN.H — head number from IDAM */
    uint8_t  sector;       /**< CHRN.R — sector ID (1-based, IBM) */
    uint8_t  size_code;    /**< CHRN.N — 0=128, 1=256, 2=512, 3=1024 */
    bool     id_crc_ok;    /**< true if IDAM CRC matched */
    bool     data_crc_ok;  /**< true if DAM/DDAM CRC matched (false if no DAM) */
    bool     deleted;      /**< true if DAM was 0xF8 (Deleted DAM) */
    bool     dam_present;  /**< false if no DAM was found within the gap window */
    size_t   data_offset;  /**< byte offset into caller's data pool */
    size_t   data_len;     /**< actual data length: 1 << (7 + size_code), or 0 */

    /* ────────────────────────────────────────────────────────────────
     * Bitpositionen und Lueckenmessung (P3-453, MF-1190).
     *
     * ANGEHAENGT, nicht eingefuegt: `uft_mfm_sector_t` ist oeffentlich,
     * und ein Feld in der Mitte waere ein ABI-Bruch ohne Compiler-
     * Warnung.
     *
     * Vorher trug dieser Satz KEINE einzige Bitposition (gemessen
     * MF-1190) — ein Aufrufer wusste, WAS gelesen wurde, aber nicht WO.
     * Damit war jede Aussage ueber die Luecken zwischen den Sektoren
     * ausserhalb dieser Datei unmoeglich.
     * ──────────────────────────────────────────────────────────────── */
    size_t   id_sync_bit;    /**< Beginn der ersten A1-Marke des IDAM */
    size_t   data_start_bit; /**< erstes Datenbit hinter der DAM-Marke */
    /** Die Luecke zwischen IDAM und DAM — sie liegt VOLLSTAENDIG in
     *  diesem Sektor. Encoder-Lage: 22x0x4E + 12x0x00. */
    uft_mfm_gap_t gap2;
    /** Die Luecke VOR der eigenen IDAM-Marke. Fuer die Sektoren ab dem
     *  zweiten ist das Gap 3 des Vorgaengers plus dessen 12 Sync-Nullen.
     *  **Fuer den ERSTEN Sektor ist es der Spurvorlauf** (Gap 4a, IAM,
     *  Gap 1) — kein Sektorzwischenraum, und deshalb dort mit mehreren
     *  verschiedenen Woertern. Das ist gemessen und benannt, nicht
     *  weggerundet. */
    uft_mfm_gap_t lead_gap;
} uft_mfm_sector_t;

/**
 * @brief Decoder configuration. Pass NULL to use defaults.
 */
typedef struct {
    /** Maximum bytes between IDAM and matching DAM. IBM/PC default = 43,
     *  Amiga formats can be tighter, weird-protected disks may have
     *  larger. 0 means "use default 43". */
    uint16_t dam_search_window_bytes;
    /** When true, accept N=0..6 (128..8192 byte sectors). When false,
     *  reject anything not in {0,1,2,3} (i.e. 128..1024). Defaults to
     *  strict (false) since N>3 is almost always a CRC-corrupted IDAM
     *  pretending to be a giant sector — accepting it would make the
     *  decoder OOM on bad data. */
    bool     accept_extended_size;
} uft_mfm_decoder_opts_t;

/**
 * @brief Decode all MFM sectors from a track bitstream.
 *
 * @param bitstream     Packed MFM bits, MSB-first. Output of the PLL.
 * @param bit_count     Total valid bits in @p bitstream (not bytes).
 * @param data_pool     Caller-owned buffer of size @p data_pool_size,
 *                      receives sector payloads back-to-back.
 * @param data_pool_size Size of @p data_pool in bytes. The decoder
 *                      stops adding sectors when the pool is full;
 *                      already-found IDAMs whose data wouldn't fit
 *                      are returned with `dam_present = false` and
 *                      `data_len = 0`.
 * @param sectors       Caller-owned array of size @p max_sectors,
 *                      receives the decoded sector records.
 * @param max_sectors   Capacity of @p sectors.
 * @param opts          Decoder options (NULL = defaults).
 * @return Number of sectors written to @p sectors (i.e. number of
 *         IDAMs successfully parsed). Note: this is NOT the number
 *         of CRC-good sectors — caller must inspect flags.
 */
size_t uft_mfm_decode_track(
    const uint8_t *bitstream,
    size_t bit_count,
    uint8_t *data_pool,
    size_t data_pool_size,
    uft_mfm_sector_t *sectors,
    size_t max_sectors,
    const uft_mfm_decoder_opts_t *opts);

/**
 * @brief Convert IBM size code N (0..7) to byte length.
 *
 * size_code N -> 128 << N bytes. N=3 = 512 (most common).
 * Returns 0 for N > 7 (defensive).
 */
size_t uft_mfm_sector_size_from_code(uint8_t size_code);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_SECTOR_PARSER_H */
