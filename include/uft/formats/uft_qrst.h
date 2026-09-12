/**
 * @file uft_qrst.h
 * @brief QRST (Compaq Quick Release Sector Transfer) format support
 *
 * QRST wurde von Compaq benutzt, um Diagnose-Disketten zu verteilen.
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/doc/qrst.html` — die Formatbeschreibung
 * von John Elliott, und dieselbe, die libdsks `lib/drvqrst.c` umsetzt
 * (**LGPL-2+**, John Elliott). **Nur gelesen** — Kanal *Spec* nach
 * MF-695; keine Zeile Quelltext uebernommen.
 *
 * ── BERICHTIGT MF-1028: hier stand ein Format, das es nicht gibt ────
 *
 * Bis MF-1028 beschrieb diese Datei einen 22-Byte-Kopf mit
 * `version`/`cylinders`/`heads`/`sectors`/`sector_size` als
 * `uint16_t`-Feldern und einen 8-Byte-Spursatz. **Nichts davon steht
 * in einer QRST-Datei.** Gemessen an einer nach der Beschreibung
 * gebauten Datei, die libdsk byteweise zurueckliest: `uft_qrst_read()`
 * lieferte `-20`, und der Schreiber erzeugte einen Bytestrom, den kein
 * Werkzeug lesen kann. Das ist die Klasse der fuenf fabrizierten
 * Parser (FMT-2/3/10/11/12).
 *
 * Der wirkliche Kopf ist **796 Byte** lang:
 *
 *     0x000  'QRST',0          Kennung, FUENF Byte
 *     0x005  00 80 3F          unbenutzt; QRST.EXE liest sie nicht
 *     0x008  Pruefsumme LE32   Summe byte*(1+Versatz) ueber die Diskette
 *     0x00C  Kapazitaetskode   1=360k 2=1.2M 3=720k 4=1.4M
 *                              5=160k 6=180k 7=320k
 *     0x00D  Bandnummer        1-basiert
 *     0x00E  Bandzahl
 *     0x00F  Beschreibung      ASCII, 0-terminiert, bis 0x04A
 *     0x04B  Etikett           ASCII, 0-terminiert, bis 0x31B
 *
 * Die **Geometrie steht nicht im Kopf** — sie folgt aus dem
 * Kapazitaetskode ueber eine Tafel von Standardformaten.
 *
 * Danach folgen Spursaetze in **drei** Arten; die dritte fehlte ganz:
 *
 *     roh     : cyl, head, 0, dann `spt * secsize` Byte
 *     leer    : cyl, head, 1, dann EIN Fuellbyte
 *     gepackt : cyl, head, 2, LE16 Laenge, dann die gepackten Bytes
 *
 * Und die Packung ist **abwechselnd** ein Literal-Lauf
 * (`<len>` + `len` Bytes) und ein Wiederhol-Lauf (`<len>` + ein Byte),
 * beginnend mit dem Literal-Lauf. Bis MF-1028 stand hier ein
 * Byte-Strom, in dem `0x00` ein Wiederhol-Tripel einleitete — woertlich
 * die Gestalt aus MF-1009 (`apridisk`), und wie dort war der
 * Rundlauftest gruen, weil Packer und Entpacker Spiegelbilder waren.
 */

#ifndef UFT_QRST_H
#define UFT_QRST_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kennung: VIER Buchstaben plus ein Nullbyte. Das Nullbyte gehoert
 * dazu — `"QRSTX"` ist keine QRST-Datei, und bis MF-1028 wurde sie
 * angenommen (Konfidenz 95). */
#define QRST_SIGNATURE          "QRST\0"
#define QRST_SIGNATURE_LEN      5
#define QRST_HEADER_SIZE        796

/* Versatze im Kopf */
#define QRST_OFF_UNUSED         0x005
#define QRST_OFF_CHECKSUM       0x008
#define QRST_OFF_CAPACITY       0x00C
#define QRST_OFF_VOLUME         0x00D
#define QRST_OFF_VOLUMES        0x00E
#define QRST_OFF_DESCRIPTION    0x00F
#define QRST_OFF_LABEL          0x04B
#define QRST_DESCRIPTION_MAX    60      /* 0x00F .. 0x04A */
#define QRST_LABEL_MAX          721     /* 0x04B .. 0x31B */

/* Spursatz-Arten */
#define QRST_TRACK_RAW          0
#define QRST_TRACK_BLANK        1
#define QRST_TRACK_PACKED       2

/* Kapazitaetskodes */
#define QRST_CAP_360K           1
#define QRST_CAP_1200K          2
#define QRST_CAP_720K           3
#define QRST_CAP_1440K          4
#define QRST_CAP_160K           5
#define QRST_CAP_180K           6
#define QRST_CAP_320K           7

/**
 * @brief QRST-Dateikopf, in der Gestalt, die er wirklich hat.
 *
 * Wird nicht als Speicherabbild ueber die Datei gelegt — die Felder
 * werden byteweise gelesen, damit Ausrichtung und Packung keine Rolle
 * spielen. Die Struktur beschreibt, was gelesen wurde.
 */
typedef struct {
    uint8_t  capacity;      /**< Kapazitaetskode 1..7 */
    uint8_t  volume;        /**< Bandnummer, 1-basiert */
    uint8_t  volumes;       /**< Baender im Satz */
    uint32_t checksum;      /**< Pruefsumme aus dem Kopf */
    char     description[QRST_DESCRIPTION_MAX + 1];
    char     label[QRST_LABEL_MAX + 1];
    /* aus dem Kapazitaetskode abgeleitet */
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
} qrst_header_t;

/**
 * @brief Ergebnis eines Lesevorgangs
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;

    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;

    uint32_t total_tracks;
    uint32_t raw_tracks;         /**< Saetze der Art 0 */
    uint32_t blank_tracks;       /**< Saetze der Art 1 */
    uint32_t packed_tracks;      /**< Saetze der Art 2 */
    uint32_t checksum_header;    /**< wie im Kopf */
    uint32_t checksum_computed;  /**< nachgerechnet */
    bool     checksum_ok;
    size_t   original_size;
    size_t   compressed_size;
} qrst_read_result_t;

/**
 * @brief Schreiboptionen
 */
typedef struct {
    bool use_compression;   /**< gepackte Spursaetze erzeugen */
} qrst_write_options_t;

/* ============================================================================
 * Packung
 * ==========================================================================*/

/**
 * @brief Entpackt einen QRST-Spurblock (abwechselnde Laeufe).
 *
 * Die Namen `qrst_rle_*` sind historisch; die Regel ist keine
 * gewoehnliche RLE, sondern der Wechsel aus Literal- und
 * Wiederhol-Lauf nach `doc/qrst.html`.
 *
 * @return entpackte Groesse, oder -1
 */
int qrst_rle_decompress(const uint8_t *input, size_t input_size,
                        uint8_t *output, size_t output_size);

/**
 * @brief Packt einen Spurblock nach derselben Regel.
 * @return gepackte Groesse, oder -1
 */
int qrst_rle_compress(const uint8_t *input, size_t input_size,
                      uint8_t *output, size_t output_capacity);

/* ============================================================================
 * Datei-E/A
 * ==========================================================================*/

uft_error_t uft_qrst_read(const char *path,
                          uft_disk_image_t **out_disk,
                          qrst_read_result_t *result);

uft_error_t uft_qrst_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              qrst_read_result_t *result);

uft_error_t uft_qrst_write(const uft_disk_image_t *disk,
                           const char *path,
                           const qrst_write_options_t *opts);

/** @brief Traegt der Kopf einen bekannten Kapazitaetskode? */
bool uft_qrst_validate_header(const qrst_header_t *header);

/** @brief Geometrie zu einem Kapazitaetskode; false bei unbekanntem. */
bool uft_qrst_geometry(uint8_t capacity, uint8_t *cyl, uint8_t *heads,
                       uint8_t *spt, uint16_t *sector_size);

/**
 * @brief Pruefsumme nach `doc/qrst.html`.
 *
 * „The checksum is the sum of all bytes on the disc, each byte
 * multiplied by (1 + its offset on the disc)." Sie steht **in der
 * Datei selbst** und ist damit ein Beleg am Objekt, nicht an einem
 * Werkzeug (wie MF-869 und MF-1013).
 */
uint32_t uft_qrst_checksum(const uint8_t *disc, size_t size);

bool uft_qrst_probe(const uint8_t *data, size_t size, int *confidence);

void uft_qrst_write_options_init(qrst_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_QRST_H */
