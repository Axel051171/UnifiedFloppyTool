/**
 * @file uft_ipf_sektoren.h
 * @brief IPF: die Sektorebene aus dem Zellstrom (MF-1373, P3-360 Teil 1)
 *
 * IPF ist ein Bitstrom-Behaelter. Seine Sektoren stehen nicht in den
 * Saetzen, sie entstehen erst beim Dekodieren der Spur — so steht es seit
 * MF-1073 im Plugin. Bis MF-1373 hat das niemand getan: eine IPF lieferte
 * Zellen und **null** Sektoren.
 *
 * Diese Stelle dekodiert NICHTS selbst. Sie gibt den Zellstrom aus
 * `uft_ipf_zellstrom()` an die zwei Dekoder, die der Baum schon hat und die
 * an fremder Hand belegt sind:
 *
 *   - IBM-MFM:  `uft_mfm_decode_track()` (src/flux/uft_mfm_sector_parser.c)
 *   - Amiga:    `flux_decode_amiga_bits()` (src/flux/uft_flux_decoder.c,
 *               abgenommen in test_adf_ext_gegen_disk_analyse, 1760/1760)
 *
 * **Welcher gilt, entscheidet der Inhalt, nicht das Plattformfeld.** Keir
 * Frasers IPF-Schreiber traegt in JEDE Datei „Amiga" ein, auch in eine
 * PC-Diskette (libdisk/container/ipf.c: `info.platform[0] = 1`). Gezaehlt
 * werden Sektorkoepfe mit GUELTIGER Pruefsumme; findet nur ein Dekoder
 * welche, gilt er. Finden beide welche, ist die Spur mehrdeutig, und es
 * werden KEINE Sektoren angelegt — geraten wird nicht.
 *
 * Jeder angelegte Sektor traegt, was der Dekoder gemessen hat: eine
 * falsche Daten- oder Kopfpruefsumme, eine geloeschte Datenmarke. Ein
 * IBM-Sektorkopf ohne Datenfeld wird nicht angelegt (es gibt keine Bytes),
 * aber gezaehlt.
 *
 * Referenz fuer die Abnahme: Keir Fraser, disk-utilities (Public Domain /
 * Unlicense), `disk-analyse`, Quellstand 5e690f3a — sein IPF-Schreiber
 * ist unabhaengig von AIR, aus dem `uft_ipf_air.c` portiert ist.
 */
#ifndef UFT_IPF_SEKTOREN_H
#define UFT_IPF_SEKTOREN_H

#include <stdint.h>
#include "uft/uft_format_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_IPF_SEKTOR_KEINE      = 0,   /**< kein Dekoder fand einen gueltigen Kopf */
    UFT_IPF_SEKTOR_IBM        = 1,
    UFT_IPF_SEKTOR_AMIGA      = 2,
    UFT_IPF_SEKTOR_MEHRDEUTIG = 3    /**< beide fanden gueltige Koepfe */
} uft_ipf_sektor_art_t;

typedef struct {
    uft_ipf_sektor_art_t art;
    unsigned ibm_koepfe_ok;     /**< IBM-Sektorkoepfe mit gueltiger CRC   */
    unsigned amiga_koepfe_ok;   /**< Amiga-Sektorkoepfe mit gueltiger Summe */
    unsigned angelegt;          /**< in die Spur uebernommene Sektoren   */
    unsigned daten_crc_falsch;  /**< davon mit falscher Datenpruefsumme  */
    unsigned kopf_crc_falsch;   /**< davon mit falscher Kopfpruefsumme   */
    unsigned ohne_daten;        /**< IBM-Koepfe ohne Datenfeld           */
} uft_ipf_sektor_bericht_t;

/**
 * @brief Dekodiert die Sektoren eines Zellstroms in `track`.
 *
 * @param zellen  MFM-Zellen, MSB zuerst (wie `uft_ipf_zellstrom()`)
 * @param bits    Zahl der Zellen
 * @param track   Zielspur; Sektoren werden angehaengt, `encoding` wird
 *                nach dem gewaehlten Dekoder gesetzt
 * @param bericht darf NULL sein
 * @return 0 = gelaufen (auch ohne Sektoren), -1 = Argumente/Speicher
 */
int uft_ipf_sektoren(const uint8_t *zellen, uint32_t bits,
                     uft_track_t *track,
                     uft_ipf_sektor_bericht_t *bericht);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_SEKTOREN_H */
