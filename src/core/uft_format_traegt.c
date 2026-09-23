/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_format_traegt.c
 * @brief Die Tafel: was traegt welches Format (MF-1283)
 *
 * Jede Zeile ist GEMESSEN, und jede nennt woran. Eine Zeile ohne Quelle
 * waere eine Behauptung ueber ein fremdes Format — genau die Klasse, an
 * der dieser Baum fuenf fabrizierte Parser hatte (FMT-2/3/10/11/12).
 *
 * Die Tafel ist ABSICHTLICH kurz: sie fuehrt, was gemessen ist, und
 * nicht, was plausibel waere. Wer eine Zeile ergaenzt, misst sie am
 * Format, nicht an der Erwartung — und schreibt die Fundstelle in
 * `quelle`.
 *
 * BERICHTIGT MF-1333: hier stand "Drei Formate, weil drei gemessen
 * sind". Gemessen sind es sieben (TD0, IMD, IMG, XFD, ADF seit
 * MF-1283, D64 und G64 seit MF-1333). Die Zahl war schon vor MF-1333
 * gedriftet — und eine gepflegte Zahl neben einer gemessenen driftet
 * immer (MF-541). Sie steht deshalb nicht mehr hier: `TAFEL_ANZAHL`
 * unten rechnet sie aus `sizeof`, und das ist die einzige Stelle, die
 * sie kennen muss.
 */
#include "uft/core/uft_format_traegt.h"
#include "uft/uft_types.h"   /* UFT_FORMAT_* */

#include <stddef.h>

#define L_SEKTOREN   (1u << UFT_D2_LAYER_SECTORS)
/* MF-1333: erst ab dieser Ebene sind die Luecken ZWISCHEN den Sektoren
 * ueberhaupt darstellbar — und genau dort liegt der GEOS-Bootschutz. */
#define L_BITSTROM   (1u << UFT_D2_LAYER_BITSTREAM)

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

    /* MF-1313 — zwei weitere Zeilen, und beide sagen NICHTS zu.
     *
     * Warum gerade diese: gemessen ist ein Ausbau dieser Tafel additiv —
     * ausser fuer SCP. Von den naheliegenden Kandidaten aktivieren ATR,
     * XFD und G64 NULL zusaetzliche Eintraege der Rundlauf-Matrix, ADF
     * und D64 je genau ihr Selbstpaar, das wegen `X & ~X == 0` nicht
     * feuern kann. SCP dagegen aktiviert einen roten Eintrag und bleibt
     * deshalb draussen, bis seine Merkmale einzeln belegt sind.
     *
     * Warum "nichts" eine Aussage ist: eine FEHLENDE Zeile heisst laut
     * Kopf dieser Datei "unbekannt", und unbekannt sperrt. Eine Zeile
     * mit `0u` heisst dagegen "gemessen, traegt ausser Sektoren nichts"
     * — erst damit kann `uft_preflight_check()` eine Verlustdifferenz
     * ueberhaupt RECHNEN statt abzusagen. Der Unterschied ist derselbe
     * wie zwischen `caps_bekannt = false` und einer leeren Maske
     * (MF-1311).
     *
     * XFD — Atari, kopflos.
     *
     * Gemessen an `src/formats/xfd/uft_xfd.c`:
     *   Kopfkommentar Z. 5: "XFD is a headerless raw sector dump for
     *                        Atari 8-bit computers" — also kein
     *                        Behaelterfeld, das Metadaten tragen koennte
     *   Z. 209            : `disk->geometry.sector_size = p->ss` — EINE
     *                        Groesse fuer die ganze Diskette, also kein
     *                        VAR_SECTOR_SZ
     *   Z. 276            : `uft_format_add_sector(...)`, und der setzt
     *                        den Status UNBEDINGT (`uft_track_layout.h`
     *                        Z. 72 sagt es woertlich, unter Verweis auf
     *                        MF-1022 und MF-1038). Ein CRC-Fehler, ein
     *                        geloeschtes Datenfeld oder ein Sektor ohne
     *                        Datenfeld sind darueber nicht ausdrueckbar.
     *
     * ADF — Amiga, kopflos. Dieselbe Lage, dieselben drei Belege:
     *   `src/formats/adf/uft_adf_plugin.c` Z. 5 ("headerless raw sector
     *   dump of Amiga floppy disks"), Z. 214 (feste `ADF_SECTOR_SIZE`),
     *   Z. 290 (`uft_format_add_sector_with_id`).
     *
     * ADF_EXT ist ausdruecklich NICHT gemeint: das erweiterte Format
     * traegt eine Spurtafel mit Bitlaengen (MF-1222) und gehoert damit
     * in eine eigene Zeile mit eigenen Belegen. Eine gemeinsame Zeile
     * waere genau die Ungenauigkeit, die eine Differenzrechnung
     * wertlos macht. */
    { UFT_FORMAT_XFD, L_SEKTOREN, 0u,
      "uft_xfd.c:5 'headerless raw sector dump'; :209 eine Sektorgroesse "
      "fuer die ganze Diskette; :276 uft_format_add_sector, dessen Status "
      "unbedingt gesetzt wird (uft_track_layout.h:72)" },

    { UFT_FORMAT_ADF, L_SEKTOREN, 0u,
      "uft_adf_plugin.c:5 'headerless raw sector dump'; :214 feste "
      "ADF_SECTOR_SIZE; :290 uft_format_add_sector_with_id, dessen Status "
      "unbedingt gesetzt wird (uft_track_layout.h:72)" },

    /* ── MF-1333: die beiden Commodore-Zeilen ───────────────────────
     *
     * Gebraucht fuer den GEOS-Bootschutz, der AUSSCHLIESSLICH in den
     * Luecken zwischen den Sektoren liegt. Ob ein Ziel ihn tragen kann,
     * ist damit dieselbe Frage wie "traegt es die Bitstromebene" — und
     * die beantwortet diese Tafel. Ohne diese zwei Zeilen muesste die
     * Oberflaeche eine ZWEITE Tafel fuehren, und das ist die Bauform
     * aus MF-1177.
     *
     * Beide Zeilen sind an der Beschreibung des Formats gemessen, nicht
     * an der Erwartung. */

    { UFT_FORMAT_D64, L_SEKTOREN, 0u,
      "docs/format_specs/commodore/D64.TXT:18 'The standard D64 is a "
      "174848 byte file comprised of 256 byte sectors' — reine "
      "Sektorablage. Luecken zwischen den Sektoren sind darin "
      "prinzipiell nicht darstellbar; die Fehlerkarte (:321) traegt "
      "einen Kode je Sektor, keine Rohbytes" },

    /* BEIDE Ebenen, und das ist keine Grosszuegigkeit: ein Bitstrom ist
     * eine OBERMENGE der Sektorablage, keine Alternative dazu. G64.TXT
     * :399-401 fuehrt Kopfblock, Luecke und Datenblock einzeln auf —
     * die Sektoren stehen im Strom, sie sind nur nicht ausgepackt.
     *
     * BERICHTIGT NOCH IN MF-1333: hier stand zuerst `L_BITSTROM`
     * allein. Eine Messung hat es widerlegt, nicht eine Ueberlegung —
     * `test_convert_options_reach_encoder` wurde rot, weil
     * `uft_preflight.c:52` bei einem Ziel OHNE Sektorebene nur noch
     * NO-ROUNDTRIP fuer wahrhaftig haelt und den seit MF-532/533 als
     * verlustfrei GEMESSENEN Pfad D64->G64 damit abwies. Die Tafel
     * hatte eine richtige Aussage ueber Luecken mit einer falschen
     * ueber Sektoren erkauft. */
    { UFT_FORMAT_G64, L_SEKTOREN | L_BITSTROM, 0u,
      "docs/format_specs/commodore/G64.TXT:47 'Each track data area is "
      "simply the raw stream of GCR data' — und :399-401 fuehrt Kopf, "
      "Luecke und Daten einzeln auf: 'Header info … (10 GCR bytes)', "
      "'Header gap 55 55 55 55 55 55 55 55 55 (9 bytes, never read)'. "
      "Damit traegt G64 die Sektoren UND die Luecken dazwischen; "
      "letzteres kann eine D64 nicht" },
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
