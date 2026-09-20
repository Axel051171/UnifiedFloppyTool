/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_format_traegt.c
 * @brief Die Tafel: was traegt welches Format (MF-1283)
 *
 * Jede Zeile ist GEMESSEN, und jede nennt woran. Eine Zeile ohne Quelle
 * waere eine Behauptung ueber ein fremdes Format — genau die Klasse, an
 * der dieser Baum fuenf fabrizierte Parser hatte (FMT-2/3/10/11/12).
 *
 * Die Tafel ist ABSICHTLICH kurz. Drei Formate, weil drei gemessen sind.
 * Wer eine Zeile ergaenzt, misst sie am Format, nicht an der Erwartung.
 */
#include "uft/core/uft_format_traegt.h"
#include "uft/uft_types.h"   /* UFT_FORMAT_* */

#include <stddef.h>

#define L_SEKTOREN   (1u << UFT_D2_LAYER_SECTORS)

/* Die fuenf Merkmale, die ein Sektorformat ueber die blossen Nutzbytes
 * hinaus tragen KANN — mehr behauptet hier niemand. */
#define F_TD0   (UFT_D2_FEAT_BAD_CRC       | \
                 UFT_D2_FEAT_DELETED_DAM   | \
                 UFT_D2_FEAT_VAR_SECTOR_SZ | \
                 UFT_D2_FEAT_NO_DATA_SEC   | \
                 UFT_D2_FEAT_METADATA)

#define F_IMD   (UFT_D2_FEAT_BAD_CRC       | \
                 UFT_D2_FEAT_DELETED_DAM   | \
                 UFT_D2_FEAT_VAR_SECTOR_SZ | \
                 UFT_D2_FEAT_NO_DATA_SEC   | \
                 UFT_D2_FEAT_METADATA)

typedef struct {
    uft_format_id_t     id;
    uint32_t            layers;
    uint32_t            features;
    const char         *quelle;
} zeile_t;

static const zeile_t g_tafel[] = {

    /* TD0 — Teledisk.
     *
     * Gemessen an `include/uft/formats/uft_td0.h`:
     *   BAD_CRC       `UFT_TD0_SEC_CRC   0x02` (Sektorflagge)
     *   DELETED_DAM   `UFT_TD0_SEC_DAM   0x04`
     *   NO_DATA_SEC   `UFT_TD0_SEC_NODAT 0x20`
     *   VAR_SECTOR_SZ Groessencode im `uft_td0_sector_header_t`
     *   METADATA      `uft_td0_comment_header_t` — und der traegt MEHR
     *                 als ein Kommentar: eine CRC-16 ueber den Text und
     *                 einen Zeitstempel aus sechs Feldern (Jahr, Monat,
     *                 Tag, Stunde, Minute, Sekunde).
     *
     * Die Flagge `UFT_TD0_SEC_NOID 0x40` (kein Adressfeld) ist BEWUSST
     * nicht abgebildet: es gibt dafuer kein Merkmal, dessen Bedeutung
     * gemessen dieselbe waere. Eine ungefaehre Zuordnung waere schlimmer
     * als keine, weil sie in einer Differenzrechnung wie eine Aussage
     * aussaehe. */
    { UFT_FORMAT_TD0, L_SEKTOREN, F_TD0,
      "uft_td0.h: SEC_CRC/SEC_DAM/SEC_NODAT, Groessencode im Sektorkopf, "
      "Kommentarkopf mit CRC-16 und Zeitstempel" },

    /* IMD — ImageDisk.
     *
     * Gemessen an `include/uft/formats/uft_imd.h`:
     *   BAD_CRC       `stype` 0x05..0x08 (die ERROR-Spielarten)
     *   DELETED_DAM   `stype` 0x03/0x04/0x07/0x08 (die DELETED-Spielarten)
     *   NO_DATA_SEC   `stype` 0x00 `UFT_IMD_SEC_UNAVAIL`
     *   VAR_SECTOR_SZ Groessenangabe im Spurkopf
     *   METADATA      Kommentarblock, beendet durch `0x1A`
     *
     * Dieselben fuenf wie TD0 — deshalb ist `TD0 -> IMD` eine Wandlung
     * OHNE Merkmalsverlust. Was die Maske NICHT ausdruecken kann, gehoert
     * in die Notiz des Matrixeintrags: IMDs Kommentar ist reiner Text und
     * hat weder Platz fuer TD0s Zeitstempel noch fuer dessen CRC-16, und
     * er darf kein `0x1A` enthalten. Eine Maske ist grobkoerniger als die
     * Wirklichkeit, und das ist ihr Zweck — sie soll pruefbar sein, nicht
     * vollstaendig. */
    { UFT_FORMAT_IMD, L_SEKTOREN, F_IMD,
      "uft_imd.h: stype 0x00..0x08 deckt Lesefehler, geloeschte Marken und "
      "fehlende Daten; Groesse im Spurkopf; Kommentarblock bis 0x1A" },

    /* IMG — flaches Sektorabbild.
     *
     * Gemessen an `src/formats/img/uft_img.c`: die Datei ist die blosse
     * Aneinanderreihung der Sektorbytes. Kein Kopf, keine Spurangabe,
     * keine Sektorkennzeichnung.
     *
     * UND ES IST SCHAERFER ALS „TRAEGT NICHT": der Leser setzt beim
     * Aufbauen jedes Sektors `sector.id.crc_ok = true` — BEDINGUNGSLOS.
     * Aus einem Sektor mit falscher CRC wird auf dem Weg durch IMG also
     * nicht ein Sektor ohne Angabe, sondern ein GUTER. Das ist die
     * Fehlerklasse MF-1001/MF-1022/MF-1038 in der Wandlungsschicht.
     *
     * `img_read_metadata()` liest zwar einen Namen — aber aus dem
     * FAT-Bootsektor, also Inhalt des Dateisystems und nicht des
     * Behaelters. Fuer diese Tafel traegt IMG keine Metadaten. */
    { UFT_FORMAT_IMG, L_SEKTOREN, 0u,
      "uft_img.c: flache Sektorfolge ohne Kopf; der Leser setzt "
      "crc_ok unbedingt auf true; read_metadata liest den FAT-Namen, "
      "nicht den Behaelter" },
};

#define TAFEL_ANZAHL (sizeof(g_tafel) / sizeof(g_tafel[0]))

uft_format_traegt_t uft_format_traegt(uft_format_id_t id)
{
    uft_format_traegt_t r;
    r.bekannt  = false;
    r.layers   = 0u;
    r.features = 0u;
    r.quelle   = "keine gemessene Zeile";

    for (size_t i = 0; i < TAFEL_ANZAHL; ++i) {
        if (g_tafel[i].id == id) {
            r.bekannt  = true;
            r.layers   = g_tafel[i].layers;
            r.features = g_tafel[i].features;
            r.quelle   = g_tafel[i].quelle;
            break;
        }
    }
    return r;
}

bool uft_format_verlust(uft_format_id_t von, uft_format_id_t nach,
                        uint32_t *out_verloren)
{
    if (out_verloren) *out_verloren = 0u;

    const uft_format_traegt_t q = uft_format_traegt(von);
    const uft_format_traegt_t z = uft_format_traegt(nach);

    /* Nur wenn BEIDE gemessen sind, ist die Differenz eine Aussage.
     * Sonst waere sie eine Rechnung mit einer erfundenen Null. */
    if (!q.bekannt || !z.bekannt) return false;

    if (out_verloren) *out_verloren = q.features & ~z.features;
    return true;
}
