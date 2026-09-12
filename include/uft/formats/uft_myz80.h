/**
 * @file uft_myz80.h
 * @brief MYZ80 — Festplattenabbild des CP/M-Emulators MYZ80
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/lib/drvmyz80.c` (John Elliott,
 * **LGPL-2+**). **Nur gelesen** — Kanal *Spec* nach MF-695; keine Zeile
 * Quelltext uebernommen. Vier Aussagen tragen diese Datei:
 *
 *   Z.  82-91   `myz80_open()` liest 256 Byte und verlangt, dass
 *               **jedes einzelne `0xE5`** ist. Es gibt **keine**
 *               Kennung.
 *   Z. 288-297  `myz80_getgeom()`: **64** Zylinder, **1** Kopf,
 *               **128** Sektoren, `dg_secbase = 0` (**0-basierte**
 *               Sektornummern), **1024** Byte je Sektor.
 *   Z. 178      `offset = (131072L * cylinder) + (1024L * sector) + 256`
 *   Z. 182-190  **Kurze Dateien sind gueltig:** „MYZ80 disc files can
 *               be shorter than the full 8Mb. If so, the missing
 *               sectors are all assumed to be full of 0xE5s. Unlike in
 *               'raw' files, it is not an error to try to read a
 *               missing sector."
 *
 * Die Geometrie ist damit **fest** — sie steht nicht in der Datei und
 * ist auch nicht aus ihrer Groesse abzuleiten.
 *
 * ── BERICHTIGT MF-1029: hier stand ein Format, das es nicht gibt ────
 *
 * Bis MF-1029 beschrieb diese Datei eine `myz80_header_t` mit
 * `magic[6] = "MYZ80 "`, `version`, `flags`, `cylinders`, `heads`,
 * `sectors`, `sector_size`, `first_sector`, `label[32]`,
 * `comment[64]` und 142 Byte Polsterung. **Nichts davon existiert.**
 * Die ersten 256 Byte einer echten MYZ80-Datei sind durchgehend
 * `0xE5`.
 *
 * Die Folge war zwingend: `uft_myz80_validate_header()` verglich
 * `memcmp(header->magic, "MYZ80 ", 6)` gegen sechs Byte `0xE5` und
 * scheiterte immer; der Groessenrueckfall der Sonde kannte nur 256256
 * und 1025024 Byte (77 x 2 x 26 x 128, eine 8-Zoll-CP/M-Geometrie, die
 * mit MYZ80 nichts zu tun hat). Eine volle MYZ80-Datei ist **8388864**
 * Byte gross. **UFT konnte keine einzige lesen.**
 *
 * Das ist die Klasse von MF-961 (`86f` probte auf `"86BX"`, ein Magic,
 * das in keiner echten Datei steht) und MF-1022 (`sap` suchte `"SAP"`
 * bei Versatz 0, wo das Formatbyte steht) — **zum dritten Mal**. Und
 * wie bei `qrst` (MF-1028) nannte der alte Dateikopf `libdsk
 * drvmyz80.c ... Fassung 1.5.12 geprueft`: eine benannte Referenz,
 * deren Verhalten der Code nicht umsetzte.
 */

#ifndef UFT_MYZ80_H
#define UFT_MYZ80_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Der reservierte Bereich am Dateianfang — 256 Byte, alle `0xE5`. */
#define MYZ80_HEADER_SIZE       256
/** Das Fuellbyte. Es ist zugleich die einzige Erkennung. */
#define MYZ80_FILL              0xE5

/* Feste Geometrie, libdsk `drvmyz80.c:288-297`. */
#define MYZ80_CYLINDERS         64
#define MYZ80_HEADS             1
#define MYZ80_SECTORS           128
#define MYZ80_SECTOR_SIZE       1024
#define MYZ80_SECTOR_BASE       0       /**< `dg_secbase = 0` */
#define MYZ80_TRACK_SIZE        131072  /**< 128 * 1024 */
#define MYZ80_FULL_SIZE         8388864 /**< 256 + 64 * 131072 */

/**
 * @brief Was in einer MYZ80-Datei ueberhaupt steht.
 *
 * Es gibt kein Kopf-Layout. Diese Struktur haelt fest, was gemessen
 * wurde — nicht, was gelesen wird.
 */
typedef struct {
    bool     header_all_fill;   /**< alle 256 Byte sind `0xE5` */
    uint64_t file_size;         /**< die tatsaechliche Groesse */
    uint32_t cylinders_in_file; /**< wie viele Zylinder die Datei traegt */
    bool     short_file;        /**< kuerzer als 8388864 Byte */
} myz80_header_t;

/** @brief Leseoptionen. */
typedef struct {
    bool ignore_header;   /**< den 0xE5-Kopf nicht verlangen */
} myz80_read_options_t;

/** @brief Schreiboptionen. */
typedef struct {
    uint32_t cylinders;   /**< wie viele Zylinder geschrieben werden (1..64) */
} myz80_write_options_t;

/** @brief Ergebnis eines Lesevorgangs. */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;

    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;

    uint32_t cylinders_in_file;   /**< aus der Datei gelesen */
    uint32_t cylinders_filled;    /**< als `0xE5` ergaenzt (Kurzdatei) */
    uint64_t file_size;
} myz80_read_result_t;

/* ============================================================================
 * Datei-E/A
 * ==========================================================================*/

uft_error_t uft_myz80_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const myz80_read_options_t *opts,
                           myz80_read_result_t *result);

uft_error_t uft_myz80_read_mem(const uint8_t *data, size_t size,
                               uft_disk_image_t **out_disk,
                               const myz80_read_options_t *opts,
                               myz80_read_result_t *result);

uft_error_t uft_myz80_write(const uft_disk_image_t *disk,
                            const char *path,
                            const myz80_write_options_t *opts);

/** @brief Sind die ersten 256 Byte durchgehend `0xE5`? */
bool uft_myz80_validate_header(const uint8_t *data, size_t size);

/** @brief Byteversatz eines Sektors, libdsk `drvmyz80.c:178`. */
long uft_myz80_offset(uint32_t cylinder, uint32_t sector);

bool uft_myz80_probe(const uint8_t *data, size_t size, int *confidence);

void uft_myz80_read_options_init(myz80_read_options_t *opts);
void uft_myz80_write_options_init(myz80_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MYZ80_H */
