/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_edsk_parser.h
 * @brief Die EINE Definition der EDSK-Parser-Schnittstelle (MF-796).
 *
 * ── Warum es diese Datei gibt ────────────────────────────────────────────
 *
 * `uft_edsk.c` hat die Typen des Parsers **von Hand nachdeklariert**, unter
 * Namen, die in `uft_edsk_parser.c` etwas anderes bedeuten:
 *
 *   * dort ist `edsk_sector_info_t` der **8-Byte-Kopfeintrag der Datei**
 *     (C/H/R/N + zwei Statusbytes + Länge), hier war es der **fertig
 *     gelesene Sektor** mit Datenzeiger;
 *   * dort ist `edsk_track_info_t` der **24-Byte-Spurkopf**, hier war es
 *     die **fertig gelesene Spur** mit 64 Sektoren.
 *
 * C prüft Typen nicht über Übersetzungseinheiten hinweg. Der Aufruf
 * übersetzte und band, und beide Seiten lasen **denselben Speicher mit
 * verschiedenen Bauplänen**. Gemessen:
 *
 *     Sektor-Element  Parser 40 B      uft_edsk 32 B
 *       ->data        Parser +16       uft_edsk  +8
 *       ->Größe       Parser  +4       uft_edsk  +6
 *     Spur-Struktur   Parser 1200 B    uft_edsk 2088 B
 *
 * `sector_count` lag in beiden bei +8 — die **Zahl** stimmte also, der
 * **Inhalt** nicht. Deshalb meldete das Plugin „9 Sektoren" und lieferte
 * keinen einzigen: die Schleife las jeden Datenzeiger acht Byte zu früh
 * und verwarf jeden Sektor als leer.
 *
 * Das galt für **jede Spur jeder EDSK-Datei**, still, mit `UFT_OK`.
 * Aufgefallen ist es erst, als `edsk` zum ersten Mal ein Abbild bekam,
 * dessen Sektoren ihre eigene Nummer tragen (MF-796) — auf **T3** hatte
 * nie ein Test eine Spur gelesen.
 */
#ifndef UFT_EDSK_PARSER_H
#define UFT_EDSK_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Höchstzahl Sektoren je Spur: der Spurkopf ist 256 Byte, davon 24
 *  Kopf und je 8 je Sektoreintrag — (256-24)/8 = 29. */
#define UFT_EDSK_MAX_SECTORS 29

/** Ein fertig gelesener Sektor. */
typedef struct {
    uint8_t  id_track;      /**< ID-Feld C */
    uint8_t  id_side;       /**< ID-Feld H */
    uint8_t  id_sector;     /**< ID-Feld R */
    uint8_t  id_size;       /**< ID-Feld N (128 << N Byte) */
    uint16_t actual_size;   /**< tatsächliche Datenlänge */
    uint8_t  fdc_st1;
    uint8_t  fdc_st2;
    bool     crc_error;
    bool     deleted;
    bool     no_data;
    bool     weak;
    uint8_t *data;
    uint8_t *weak_data;     /**< weitere Fassungen bei schwachen Sektoren */
    int      weak_copies;
} edsk_sector_t;

/** Eine fertig gelesene Spur. */
typedef struct {
    int     track_number;
    int     side;
    int     sector_count;
    uint8_t sector_size_code;
    uint8_t gap3_length;
    uint8_t filler_byte;
    edsk_sector_t sectors[UFT_EDSK_MAX_SECTORS];
    int     good_sectors;
    int     bad_sectors;
    int     weak_sectors;
    int     deleted_sectors;
    float   quality_percent;
} edsk_track_t;

typedef struct edsk_parser_ctx_s edsk_parser_ctx_t;

/** Rückgabe von edsk_parser_read_track().
 *
 * Die drei Fälle waren vorher **einer**: jeder lieferte `-1`, und der
 * Aufrufer machte daraus ein `UFT_OK` mit leerer Spur. Damit war „diese
 * Spur ist unformatiert" von „das Lesen ist gescheitert" nicht zu
 * unterscheiden — dieselbe Verwechslung, gegen die
 * `uft_schutzbefund.h` geschrieben ist. */
typedef enum {
    EDSK_TRACK_OK = 0,      /**< gelesen */
    EDSK_TRACK_LEER = 1,    /**< unformatierte Spur (Größe 0 in der Tabelle) */
    EDSK_TRACK_FEHLER = -1  /**< Lesen oder Aufbau gescheitert */
} edsk_track_ergebnis_t;

edsk_parser_ctx_t *edsk_parser_open(const char *path);
void edsk_parser_close(edsk_parser_ctx_t **ctx);
int  edsk_parser_read_track(edsk_parser_ctx_t *ctx, int track_num, int side,
                            edsk_track_t **track_out);
void edsk_parser_free_track(edsk_track_t **track);
void edsk_parser_get_info(edsk_parser_ctx_t *ctx, int *num_tracks,
                          int *num_sides, bool *is_extended,
                          char *creator, size_t creator_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EDSK_PARSER_H */
