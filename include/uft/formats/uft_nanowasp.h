/**
 * @file uft_nanowasp.h
 * @brief NanoWasp — Microbee-Abbild des NanoWasp-Emulators
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/lib/drvnwasp.c` (John Elliott,
 * **LGPL-2+**). **Nur gelesen** — Kanal *Spec* nach MF-695; keine
 * Zeile Quelltext uebernommen. Fuenf Stellen tragen diese Datei:
 *
 *   Z.  23-30  Der Kopfkommentar, und er ist die wichtigste Quelle:
 *              die Datei speichert die Sektoren in **SIDES_OUTOUT**
 *              (kopf-dur: erst die ganze Seite 0, dann Seite 1) statt
 *              SIDES_ALT — „though the MicroBee actually writes them
 *              in SIDES_ALT order" —, und der Treiber **kehrt den
 *              Sektor-Skew um**, damit die Sektoren in logischer
 *              Reihenfolge herauskommen. libdsk nennt das selbst
 *              „an abuse of libdsk (skewing should be done at the
 *              cpmtools level). However, cpmtools doesn't support the
 *              type of skewing done by the microbee (**sector 1
 *              doesn't map to sector 1**)".
 *   Z.  71-94  `nwasp_open()` prueft **NICHTS**. Es gibt keine
 *              Kennung, keinen Kopf; die Datei beginnt mit
 *              Sektordaten.
 *   Z. 127     `static const int skew[10] = { 1,4,7,0,3,6,9,2,5,8 };`
 *   Z. 147     `offset = 204800L * head + 5120L * cylinder
 *                        + 512 * skew[sector-1];`
 *   Z. 286-301 `nwasp_getgeom()`: **40** Zylinder, **2** Koepfe,
 *              **10** Sektoren, `dg_secbase = 1` (**1-basierte**
 *              Sektornummern), **512** Byte.
 *
 * Die Datei ist damit genau 40 x 2 x 10 x 512 = **409600** Byte gross.
 *
 * ── BERICHTIGT MF-1030: hier stand ein Format, das es nicht gibt ────
 *
 * Bis MF-1030 verlangte diese Datei eine **24 Byte lange Kennung**
 * `"nanowasp floppy image\r\n\032"` und einen **80-Byte-Kopf** mit
 * Geometriefeldern, dazu **80** Zylinder als Vorgabe. **Nichts davon
 * existiert.** Eine echte NanoWasp-Datei beginnt mit dem ersten
 * Sektor.
 *
 * Die Folgen waren zwingend: `uft_nanowasp_probe()` verglich die
 * Kennung und schlug bei jeder echten Datei fehl; und haette sie
 * zugestimmt, waeren 80 Byte Sektordaten als Kopf verworfen worden und
 * jeder Sektor laege danach an der falschen Stelle — zweifach falsch,
 * weil die Anordnung ausserdem kopf-dur mit Skew ist und nicht linear.
 *
 * Das ist die Klasse von **MF-961** (`86f` probte auf `"86BX"`),
 * **MF-1022** (`sap` suchte `"SAP"` bei Versatz 0) und **MF-1029**
 * (`myz80` suchte `"MYZ80 "` in 256 Byte `0xE5`) — zum **vierten**
 * Mal, und dreimal davon in dieser Runde.
 */

#ifndef UFT_NANOWASP_H
#define UFT_NANOWASP_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Feste Geometrie, libdsk `drvnwasp.c:286-301`. */
#define NANOWASP_CYLINDERS      40
#define NANOWASP_HEADS          2
#define NANOWASP_SECTORS        10
#define NANOWASP_SECTOR_SIZE    512
#define NANOWASP_SECTOR_BASE    1       /**< `dg_secbase = 1` */
#define NANOWASP_TRACK_SIZE     5120    /**< 10 * 512 */
#define NANOWASP_SIDE_SIZE      204800  /**< 40 * 5120 */
#define NANOWASP_FILE_SIZE      409600  /**< 40 * 2 * 10 * 512 */

/**
 * @brief Der Sektor-Skew, libdsk `drvnwasp.c:127`.
 *
 * `skew[s-1]` ist der PHYSISCHE Platz des LOGISCHEN Sektors `s`.
 * Der physische Platz 0 traegt damit den logischen Sektor **4**
 * (weil `skew[3] == 0`) — genau das, was libdsk als „sector 1 doesn't
 * map to sector 1" beschreibt.
 */
extern const int uft_nanowasp_skew[NANOWASP_SECTORS];

/** @brief Leseoptionen. */
typedef struct {
    bool ignore_size;   /**< die Groessenpruefung uebergehen */
} nanowasp_read_options_t;

/** @brief Schreiboptionen. */
typedef struct {
    bool reserved;      /**< es gibt keine — NanoWasp hat keine Felder */
} nanowasp_write_options_t;

/** @brief Ergebnis eines Lesevorgangs. */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;

    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;
    uint64_t file_size;
} nanowasp_read_result_t;

/* ============================================================================
 * Datei-E/A
 * ==========================================================================*/

uft_error_t uft_nanowasp_read(const char *path,
                              uft_disk_image_t **out_disk,
                              const nanowasp_read_options_t *opts,
                              nanowasp_read_result_t *result);

uft_error_t uft_nanowasp_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  const nanowasp_read_options_t *opts,
                                  nanowasp_read_result_t *result);

uft_error_t uft_nanowasp_write(const uft_disk_image_t *disk,
                               const char *path,
                               const nanowasp_write_options_t *opts);

/**
 * @brief Byteversatz eines Sektors, libdsk `drvnwasp.c:147`.
 *
 * @param sector_1based logische Sektornummer 1..10
 * @return Versatz, oder -1 bei unzulaessigen Koordinaten
 */
long uft_nanowasp_offset(uint32_t cylinder, uint32_t head,
                         uint32_t sector_1based);

bool uft_nanowasp_probe(const uint8_t *data, size_t size,
                        size_t file_size, int *confidence);

void uft_nanowasp_read_options_init(nanowasp_read_options_t *opts);
void uft_nanowasp_write_options_init(nanowasp_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NANOWASP_H */
