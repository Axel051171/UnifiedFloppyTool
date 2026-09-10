/**
 * @file uft_apridisk.h
 * @brief ApriDisk format support
 * @version 3.9.0
 * 
 * ApriDisk format was created by the APRIDISK.EXE utility from Apricot computers.
 * Used for archiving Apricot MS-DOS and other disk formats.
 * 
 * File structure:
 * - 128-byte header (signature + info)
 * - Series of track/sector records
 * - Each record has: 16-byte descriptor + compressed/raw data
 * 
 * Reference: libdsk drvadisk.c (LGPL-2.0-or-later, Fassung 1.5.12 geprueft;
 *   hiess hier bis MF-651 faelschlich "drvapdsk.c")
 */

#ifndef UFT_APRIDISK_H
#define UFT_APRIDISK_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ApriDisk signature */
#define APRIDISK_SIGNATURE      "ACT Apricot disk image\032\004"
#define APRIDISK_SIGNATURE_LEN  24
#define APRIDISK_HEADER_SIZE    128

/* ── Satztypen ───────────────────────────────────────────────────────
 *
 * MF-1009: hier standen `0x00000000` / `0x00000002` / `0x00000001` /
 * `0x00000003` — die obere Haelfte `0xE31D` fehlte GANZ, und SEKTOR
 * und KOMMENTAR waren zusaetzlich VERTAUSCHT. Damit hat kein einziger
 * Typvergleich je auf einer echten Datei gegriffen.
 *
 * Referenz: MAME, `src/lib/formats/apridisk.h` — `enum : uint32_t
 * { APR_DELETED, APR_SECTOR, APR_COMMENT, APR_CREATOR }`
 * (BSD-3-Clause, Dirk Best). Kein MAME-Code uebernommen.
 */
#define APRIDISK_DELETED        0xE31D0000u  /* geloeschter Sektor  */
#define APRIDISK_SECTOR         0xE31D0001u  /* normaler Sektor     */
#define APRIDISK_COMMENT        0xE31D0002u  /* Kommentar           */
#define APRIDISK_CREATOR        0xE31D0003u  /* Erzeuger            */

/* ── Kompression ─────────────────────────────────────────────────────
 *
 * MF-1009: hier standen `0` und `1`. Auch das sind erfundene Werte.
 * Referenz: MAME `apridisk.h` — `APR_UNCOMPRESSED = 0x9e90`,
 * `APR_COMPRESSED = 0x3e5a`.
 *
 * Und „RLE" traf die Sache nicht: ein gepackter Satz ist **kein
 * Byte-Strom**, sondern genau drei Byte —
 *
 *     [0..1] u16le Laenge (muss APRIDISK_SECTOR_SIZE sein)
 *     [2]    Fuellbyte
 *
 * — also ein einzelner Lauf ueber den ganzen Sektor. MAME weist eine
 * andere Laenge ausdruecklich ab („Invalid compression length").
 */
#define APRIDISK_COMP_NONE      0x9E90u      /* unkomprimiert       */
#define APRIDISK_COMP_RLE       0x3E5Au      /* einzelner Fuelllauf */

/* Die Sektorgroesse ist im Format FEST (MAME: `SECTOR_SIZE = 512`).
 * MF-1009: UFT las sie aus einem `size_code`-Feld, das es im Satzkopf
 * nicht gibt — dort steht `data_size`. */
#define APRIDISK_SECTOR_SIZE    512

/**
 * @brief ApriDisk file header
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[24];      /* "ACT Apricot disk image\032\004" */
    uint8_t  reserved[104];      /* Padding to 128 bytes */
} apridisk_header_t;

/**
 * @brief ApriDisk-Satzkopf — 16 Byte, Little-Endian.
 *
 * MF-1009: hier standen VIER `uint32_t` und daneben ein separater
 * `apridisk_sector_desc_t` mit `cylinder, head, sector, size_code`,
 * den der Leser aus `pos - 8` holte. Beides war falsch:
 *
 *   - `compression` und `header_size` sind **16 Bit**
 *     (MAME: `get_u16le(&sector_header[4])` bzw. `[6]`). Als 32 Bit
 *     gelesen lag alles danach zwei Byte falsch.
 *   - Die Sektorfelder stehen **im Kopf selbst**, an 12..15, und in
 *     anderer Reihenfolge. Was UFT bei 8..11 als
 *     `cylinder/head/sector/size_code` las, ist in Wirklichkeit
 *     `data_size`.
 *
 * Der echte Aufbau, Byte fuer Byte (MAME `apridisk.cpp:load()`):
 *
 *     [0..3]   u32le  Typ            (APRIDISK_SECTOR usw.)
 *     [4..5]   u16le  Kompression    (APRIDISK_COMP_*)
 *     [6..7]   u16le  Kopfgroesse    -> Schrittweite bis zu den Daten
 *     [8..11]  u32le  Datengroesse   -> Schrittweite bis zum naechsten Satz
 *     [12]     u8     Kopf
 *     [13]     u8     Sektor         (**1-basiert**)
 *     [14..15] u16le  Spur
 *
 * Kein MAME-Code uebernommen; der Aufbau ist gelesen und hier
 * eigenstaendig beschrieben.
 */
typedef struct {
    uint32_t type;
    uint16_t compression;
    uint16_t header_size;
    uint32_t data_size;
    uint8_t  head;
    uint8_t  sector;             /* 1-basiert */
    uint16_t cylinder;
} apridisk_record_desc_t;
#pragma pack(pop)

/**
 * @brief ApriDisk read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info */
    uint16_t max_cylinder;
    uint8_t  max_head;
    uint8_t  max_sector;
    uint16_t sector_size;
    
    /* Comment */
    char *comment;
    size_t comment_len;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t deleted_sectors;
    uint32_t rle_sectors;
    
} apridisk_read_result_t;

/**
 * @brief ApriDisk write options
 */
typedef struct {
    bool use_rle;                /* Use RLE compression */
    const char *comment;         /* Optional comment */
    const char *creator;         /* Creator string */
} apridisk_write_options_t;

/* ============================================================================
 * ApriDisk-Kompression
 *
 * MF-1009: hier standen `apridisk_rle_decompress()` und
 * `apridisk_rle_compress()` — ein Byte-Strom mit Zaehl-, Literal- und
 * Laufbloecken. **Diesen Strom gibt es im Format nicht.** Ein gepackter
 * APRIDISK-Satz ist genau drei Byte (Laenge + Fuellbyte), also ein
 * einzelner Lauf ueber den ganzen Sektor.
 *
 * Beide Funktionen hatten ausserhalb dieser Uebersetzungseinheit
 * **keinen Aufrufer** (ueber `git grep` gemessen), sind also ersetzt
 * statt angepasst — MF-699: erst der Ersatz, dann die Loeschung.
 * ============================================================================ */

/**
 * @brief Entpackt einen gepackten APRIDISK-Sektor.
 *
 * Eingabe sind die drei Datenbytes eines Satzes mit
 * `APRIDISK_COMP_RLE`: `[0..1]` u16le Laenge, `[2]` Fuellbyte.
 *
 * @param input        Zeiger auf die drei Bytes
 * @param input_size   verfuegbare Bytes (muss >= 3 sein)
 * @param output       Ziel, mindestens `output_size` Byte
 * @param output_size  erwartete Sektorgroesse (APRIDISK_SECTOR_SIZE)
 * @return `output_size` bei Erfolg, sonst -1.
 *
 * Die Laenge MUSS `output_size` sein. MAME weist alles andere
 * ausdruecklich ab („Invalid compression length %04x"), und ein
 * stillschweigend anderes Ergebnis waere eine erfundene Angabe.
 */
int apridisk_expand_fill(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size);

/**
 * @brief Packt einen Sektor, wenn er aus einem einzigen Byte besteht.
 *
 * @return 3 bei Erfolg (Laenge + Fuellbyte in `output`), sonst -1 —
 *         letzteres auch dann, wenn der Sektor nicht gleichfoermig ist.
 *         Das Format kennt keine andere Kompression.
 */
int apridisk_make_fill(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_capacity);

/* ============================================================================
 * ApriDisk File I/O
 * ============================================================================ */

/**
 * @brief Read ApriDisk file
 */
uft_error_t uft_apridisk_read(const char *path,
                              uft_disk_image_t **out_disk,
                              apridisk_read_result_t *result);

/**
 * @brief Read ApriDisk from memory
 */
uft_error_t uft_apridisk_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  apridisk_read_result_t *result);

/**
 * @brief Write ApriDisk file
 */
uft_error_t uft_apridisk_write(const uft_disk_image_t *disk,
                               const char *path,
                               const apridisk_write_options_t *opts);

/**
 * @brief Validate ApriDisk header
 */
bool uft_apridisk_validate_header(const apridisk_header_t *header);

/**
 * @brief Initialize write options with defaults
 */
void uft_apridisk_write_options_init(apridisk_write_options_t *opts);

/**
 * @brief Probe if data is ApriDisk format
 */
bool uft_apridisk_probe(const uint8_t *data, size_t size, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APRIDISK_H */
