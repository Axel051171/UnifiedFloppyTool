/**
 * @file uft_td0.h
 * @brief Teledisk (TD0) Format Support for UFT
 * 
 * Teledisk was a popular disk imaging program by Sydex. The format is
 * undocumented but has been reverse-engineered. This implementation is
 * based on the notes by various authors and others.
 * 
 * The format supports optional LZSS-Huffman compression ("Advanced Compression").
 * 
 * @copyright UFT Project - Based on reverse-engineering by various authors et al.
 */

#ifndef UFT_FORMATS_UFT_TD0_H
#define UFT_FORMATS_UFT_TD0_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TD0 Format Constants
 *============================================================================*/

/** Normal TD0 signature */
#define UFT_TD0_SIG_NORMAL      0x4454  /* "TD" little-endian */

/** Advanced compression TD0 signature */
#define UFT_TD0_SIG_ADVANCED    0x6474  /* "td" little-endian */

/** End of image marker */
#define UFT_TD0_END_OF_IMAGE    0xFF

/*============================================================================
 * LZSS Compression Constants
 *============================================================================*/

/** Size of LZSS ring buffer */
#define UFT_TD0_LZSS_SBSIZE     4096

/** Size of look-ahead buffer */
#define UFT_TD0_LZSS_LASIZE     60

/** Minimum match length for compression */
#define UFT_TD0_LZSS_THRESHOLD  2

/** Number of character codes */
#define UFT_TD0_LZSS_N_CHAR     (256 - UFT_TD0_LZSS_THRESHOLD + UFT_TD0_LZSS_LASIZE)

/** Tree size */
#define UFT_TD0_LZSS_TSIZE      (UFT_TD0_LZSS_N_CHAR * 2 - 1)

/** Root position in tree */
#define UFT_TD0_LZSS_ROOT       (UFT_TD0_LZSS_TSIZE - 1)

/** Maximum frequency before tree rebuild */
#define UFT_TD0_LZSS_MAX_FREQ   0x8000

/*============================================================================
 * TD0 Header Data Rate Values
 *============================================================================*/

/**
 * @brief TD0 data rate encoding
 */
typedef enum {
    UFT_TD0_RATE_250K = 0,  /**< 250 kbps (DD) */
    UFT_TD0_RATE_300K = 1,  /**< 300 kbps (DD on HD drive) */
    UFT_TD0_RATE_500K = 2   /**< 500 kbps (HD) */
} uft_td0_rate_t;

/*============================================================================
 * TD0 Drive Type Values
 *============================================================================*/

/**
 * @brief TD0 source drive type
 */
typedef enum {
    UFT_TD0_DRIVE_525_96   = 1,  /**< 5.25" 96 TPI (1.2MB) */
    UFT_TD0_DRIVE_525_48   = 2,  /**< 5.25" 48 TPI (360K) */
    UFT_TD0_DRIVE_35_HD    = 3,  /**< 3.5" HD */
    UFT_TD0_DRIVE_35_DD    = 4,  /**< 3.5" DD */
    UFT_TD0_DRIVE_8INCH    = 5,  /**< 8" drive */
    UFT_TD0_DRIVE_35_ED    = 6   /**< 3.5" ED */
} uft_td0_drive_t;

/*============================================================================
 * TD0 Sector Flags
 *============================================================================*/

/** Sector was duplicated */
#define UFT_TD0_SEC_DUP         0x01

/** Sector has CRC error */
#define UFT_TD0_SEC_CRC         0x02

/** Sector has Deleted Address Mark */
#define UFT_TD0_SEC_DAM         0x04

/** Sector not allocated (DOS mode) */
#define UFT_TD0_SEC_DOS         0x10

/** Sector has no data field */
#define UFT_TD0_SEC_NODAT       0x20

/** Sector has no ID field */
#define UFT_TD0_SEC_NOID        0x40

/*============================================================================
 * TD0 Data Encoding Methods
 *============================================================================*/

/**
 * @brief TD0 sector data encoding methods
 */
typedef enum {
    UFT_TD0_ENC_RAW   = 0,  /**< Raw sector data (uncompressed) */
    UFT_TD0_ENC_REP2  = 1,  /**< 2-byte pattern repetition */
    UFT_TD0_ENC_RLE   = 2   /**< Run-length encoding */
} uft_td0_encoding_t;

/*============================================================================
 * TD0 File Structures (packed binary format)
 *============================================================================*/

/**
 * @brief TD0 main image header (12 bytes)
 */
#ifndef UFT_TD0_HEADER_T_DEFINED
#define UFT_TD0_HEADER_T_DEFINED
UFT_PACK_BEGIN
typedef struct {
    uint16_t signature;     /**< "TD" (0x4454) or "td" (0x6474) */
    uint8_t  sequence;      /**< Volume sequence number */
    uint8_t  check_seq;     /**< Check sequence for multi-volume */
    uint8_t  version;       /**< Teledisk version (BCD) */
    uint8_t  data_rate;     /**< Source data rate */
    uint8_t  drive_type;    /**< Source drive type */
    /* MF-460: Byte 7 ist die SPURDICHTE, nicht die Seitenzahl.
     *
     * Der Kommentar hier lautete "Stepping type (0=SS, 1=DS, 2=EDS)" — also
     * einseitig/zweiseitig. Das kann nicht stimmen: Byte 9 (`sides`) traegt
     * die Seitenzahl bereits. SAMdisk (MIT, src/samdisk/td0.cpp:22) liest es
     * als
     *
     *   0 = source matches media density
     *   1 = double density in quad drive
     *   2 = quad density in double drive
     *
     * und Bit 7 als "Kommentarblock vorhanden" (td0.cpp:28 und :208,
     * `th.bTrackDensity & 0x80`). Unser eigener Code tut genau das schon —
     * src/formats/td0/uft_td0_lzss.c:469 prueft `header.stepping & 0x80`.
     * Falsch war also nur die Beschreibung, nicht das Verhalten. Der Feldname
     * bleibt, weil er in mehreren Dateien steht; die Bedeutung steht jetzt
     * daneben. */
    uint8_t  stepping;      /**< Track density; Bit 7 = Kommentarblock folgt */
    uint8_t  dos_mode;      /**< DOS allocation mode */
    uint8_t  sides;         /**< Number of sides */
    uint16_t crc;           /**< Header CRC-16 */
} uft_td0_header_t;
UFT_PACK_END
#endif /* UFT_TD0_HEADER_T_DEFINED */

/**
 * @brief TD0 comment block header (10 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t crc;           /**< Comment CRC-16 */
    uint16_t length;        /**< Comment data length */
    uint8_t  year;          /**< Year - 1900 */
    /* BERICHTIGT MF-1285: hier stand „Month (1-12)". Gemessen ist das
     * Feld 0-BASIERT — libdsk schreibt `ptm->tm_mon` (`lib/drvtele.c:1031`,
     * 0..11), SAMdisk liest `tc.bMon + 1` (`src/samdisk/td0.cpp:216`), und
     * die Korpusdatei vom 2026-09-12 traegt eine 8. Ein Kommentar, der
     * eine Zahl um eins verschiebt, ist keine Kleinigkeit: an ihm haengt,
     * ob ein Datum stimmt. */
    uint8_t  month;         /**< Month, 0-basiert (0 = Januar) */
    uint8_t  day;           /**< Day (1-31) */
    uint8_t  hour;          /**< Hour (0-23) */
    uint8_t  minute;        /**< Minute (0-59) */
    uint8_t  second;        /**< Second (0-59) */
} uft_td0_comment_header_t;
UFT_PACK_END

/**
 * @brief TD0 track header (4 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  nsectors;      /**< Number of sectors (0xFF = end) */
    uint8_t  cylinder;      /**< Physical cylinder */
    uint8_t  side;          /**< Physical side/head */
    /** Untere 8 Bit der CRC-16 ueber die DREI Byte davor.
     *
     * SAMdisk `src/samdisk/td0.cpp:240` (MIT, im Baum):
     *     uint8_t crc = CrcTd0Block(&tt, sizeof(tt) - sizeof(tt.crc)) & 0xff;
     *     if (tt.crc && crc != tt.crc) throw ...
     *
     * **Eine gespeicherte NULL heisst `keine Angabe`, nicht
     * `die CRC ist 0`** — das sagt SAMdisks `tt.crc &&`
     * ausdruecklich, und UFT haelt es genauso.
     */
    uint8_t  crc;
} uft_td0_track_header_t;
UFT_PACK_END

/**
 * @brief TD0 sector header (6 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  cylinder;      /**< Cylinder in ID field */
    uint8_t  side;          /**< Side in ID field */
    uint8_t  sector;        /**< Sector number in ID field */
    uint8_t  size;          /**< Sector size code (128 << size) */
    uint8_t  flags;         /**< Sector flags */
    /** Untere 8 Bit der CRC-16 ueber die ENTPACKTEN SEKTORDATEN.
     *
     * **BERICHTIGT MF-1296. Hier stand `Sector header CRC-8`.**
     * Das Feld deckt den Sektorkopf NICHT ab. Zwei unabhaengige
     * Haende sagen dasselbe:
     *
     *   SAMdisk `td0.cpp:53`   `uint8_t data_crc;`
     *                          `// Low 8-bits of sector data CRC`
     *   SAMdisk `td0.cpp:277`  CrcTd0Block(data.data(), data.size())
     *                          & 0xff  gegen  ts.data_crc
     *   libdsk  `drvtele.c:133` `buf[5] is the CRC, ignored on load`
     *   libdsk  `drvtele.c:138` Formatflag 0x02 -> ST2 0x20,
     *                          `Data Error in Data Field`
     *
     * **Und es ist am Objekt gemessen, nicht nur nachgelesen
     * (MF-1296):** an `tests/corpus_free/libdsk_uftk_pc720.td0`
     * gehen alle 1440 Sektorpruefsummen auf. Kippt man ein Byte
     * im SEKTORKOPF, faellt **keine** Pruefsumme der Datei —
     * kippt man eines in den DATEN, faellt genau diese.
     *
     * **Folge fuer das Zentrum: TD0 traegt KEINE ID-Feld-CRC.**
     * `uft_d2_sector_t.id_crc_known` bleibt deshalb false — das
     * Feld bedeutet laut eigener Beschreibung `false = das Format
     * traegt keine Angabe`, und etwas anderes einzutragen waere
     * eine erfundene Zusage. Zylinder, Kopf, Nummer und Groesse
     * eines TD0-Sektors sind durch nichts geschuetzt.
     */
    uint8_t  crc;
} uft_td0_sector_header_t;
UFT_PACK_END

/**
 * @brief TD0 data block header (3 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t offset;        /**< Offset to next data block */
    uint8_t  method;        /**< Encoding method */
} uft_td0_data_header_t;
UFT_PACK_END

/*============================================================================
 * LZSS Decompression State
 *============================================================================*/

/**
 * @brief LZSS-Huffman decompression state
 */
typedef struct {
    /* Huffman tree */
    uint16_t parent[UFT_TD0_LZSS_TSIZE + UFT_TD0_LZSS_N_CHAR];
    uint16_t son[UFT_TD0_LZSS_TSIZE];
    uint16_t freq[UFT_TD0_LZSS_TSIZE + 1];
    
    /* Ring buffer */
    uint8_t  ring_buff[UFT_TD0_LZSS_SBSIZE + UFT_TD0_LZSS_LASIZE - 1];
    
    /* Bit buffer */
    uint16_t bitbuff;
    uint8_t  bits;
    
    /* State machine */
    uint8_t  state;
    uint16_t r;             /**< Ring buffer position */
    uint16_t i, j, k;       /**< Decoder indices */
    
    /* I/O */
    const uint8_t* input;
    size_t   input_pos;
    size_t   input_size;
    bool     eof;
} uft_td0_lzss_state_t;

/*============================================================================
 * TD0 Expanded Structures
 *============================================================================*/

/* MF-1287: `uft_td0_sector_t` ist weg — er existierte nur fuer den
 * geloeschten zweiten Leser. Die PACKED-Strukturen des Dateiformats
 * bleiben: sie beschreiben die Datei, nicht den Leser. */

/* MF-1287: `uft_td0_track_t` ist weg — er existierte nur fuer den
 * geloeschten zweiten Leser. Die PACKED-Strukturen des Dateiformats
 * bleiben: sie beschreiben die Datei, nicht den Leser. */

/* MF-1287: `uft_td0_image_t` ist weg — er existierte nur fuer den
 * geloeschten zweiten Leser. Die PACKED-Strukturen des Dateiformats
 * bleiben: sie beschreiben die Datei, nicht den Leser. */

/*============================================================================
 * Huffman Decode Tables (from Teledisk reverse-engineering)
 *============================================================================*/

/**
 * @brief Huffman d_code table for position decoding
 */
extern const uint8_t uft_td0_d_code[256];

/**
 * @brief Huffman d_len table for bit lengths
 */
extern const uint8_t uft_td0_d_len[16];

/*============================================================================
 * TD0 API Functions
 *============================================================================*/

/* Vorwaertsdeklaration auf DATEI-Ebene (MF-507).
 *
 * Sie stand vorher nur in der Parameterliste unten — und ein Struktur-Tag,
 * das dort zum ersten Mal auftaucht, gilt nach C-Regeln nur fuer diesen
 * einen Prototyp. `struct uft_imd_image_t` war damit ein ANDERER Typ als
 * der aus `uft_imd.h`, und der Uebersetzer meldete am Aufrufer
 * "incompatible pointer type".
 *
 * Das Ergebnis war schlimmer als die Warnung: der Prototyp beschrieb
 * einen Parametertyp, den es sonst nirgends gibt, also konnte die
 * Typpruefung KEINEN Aufrufer pruefen. Wer hier etwas Falsches uebergibt,
 * faellt nicht auf.
 *
 * Auf Datei-Ebene meint der Tag denselben Typ wie in `uft_imd.h`, sobald
 * beide Header in derselben Uebersetzungseinheit liegen — und ohne
 * `uft_imd.h` bleibt er ein unvollstaendiger Typ, was fuer einen Zeiger
 * genuegt. Ein Include waere die andere Loesung; sie zoege den ganzen
 * IMD-Header in jede Datei, die nur TD0 lesen will. */
struct uft_imd_image_t;

/* MF-1287: `uft_td0_to_imd()` ist ans Ende dieses Headers gewandert —
 * sie nimmt jetzt `uft_td0_strom_t`, und der ist weiter unten definiert.
 * `uft_td0_to_raw()` ist ganz weg: sie tat nichts als `to_imd` +
 * `uft_imd_to_raw`, und ihr einziger Aufrufer macht beides selbst. */

/*============================================================================
 * Der Strom-Kern (MF-1285)
 *
 * `uft_td0_read_mem()` und der Plugin-Leser in `src/formats/td0/uft_td0.c`
 * waren ZWEI Leser fuer EIN Format, und jeder konnte etwas, das dem
 * anderen fehlte: `read_mem` entpackte und las den Kommentarblock,
 * erfand aber am Dateiende bis zu 255 Spuren; das Plugin las richtig und
 * warf den Kommentarblock weg. Zwei Leser sind der Fehler, nicht der
 * Muell — wer beide repariert, hat danach zwei, die nachgezogen werden
 * muessen, sobald der naechste TD0-Dialekt kommt.
 *
 * Hier steht EINMAL, was beide brauchen: der entpackte Strom, seine
 * gemessene Geometrie, sein Kommentarblock, und der Spurlauf darueber.
 *
 * **Warum das kein `uft_disk_t` nimmt:** der Speicher-Wandler
 * `uftc_td0_to_img_mem()` bekommt Bytes ohne Pfad, und
 * `uft_format_plugin_t` hat kein Oeffnen-aus-dem-Speicher — der
 * Verteiler sagt das in `uft_format_convert_dispatch.c` selbst und
 * fuehrt es als `KNOWN_ISSUES ARCH-6`. Ueber `uft_disk_open()` waere er
 * also nicht erreichbar. Der Kern haengt deshalb an Bytes, nicht an
 * einem Griff.
 *============================================================================*/

#ifndef UFT_TRACK_T_DEFINED
struct uft_track;
typedef struct uft_track uft_track_t;
#endif

/**
 * @brief Kommentarblock einer TD0 — vorhanden, wenn Kopfbyte 7 Bit 7 traegt.
 *
 * `text` zeigt IN den Strom und wird mit ihm frei; nicht selbst freigeben,
 * und nicht als NUL-terminiert behandeln — `text_len` gilt.
 */
typedef struct {
    bool        vorhanden;
    uint16_t    crc;      /**< CRC-16 wie in der Datei. NICHT nachgerechnet. */
    /* Beide Felder stehen ROH da, wie die Datei sie traegt. Die Deutung
     * macht der Verbraucher — und sie ist GEMESSEN, an zwei
     * voneinander unabhaengigen Haenden:
     *
     *   libdsk `lib/drvtele.c:1030-1031` (LGPL-2+, John Elliott; nur
     *   gelesen, Kanal *Spec*) SCHREIBT
     *       stamp[0] = ptm->tm_year;   // Jahre seit 1900
     *       stamp[1] = ptm->tm_mon;    // 0..11, NICHT 1..12
     *   SAMdisk `src/samdisk/td0.cpp:216` (MIT, im Baum) LIEST
     *       tc.bYear + ((tc.bYear < 70) ? 2000 : 1900), tc.bMon + 1
     *
     * Und das Objekt bestaetigt es: `libdsk_uftk_pc720.td0` kam am
     * 2026-09-12 in den Baum und traegt Monat **8** — 0-basiert ist das
     * September.
     *
     * Damit ist `uft_td0_to_imd()`s `month + 1` RICHTIG, und der
     * Kommentar „Month (1-12)" an `uft_td0_comment_header_t` war es
     * nicht; er ist unten berichtigt.
     *
     * **Beim Jahr weichen die beiden ab, und das ist nicht entschieden:**
     * SAMdisk kippt bei 70 auf 2000, UFT rechnet unbedingt +1900. Siehe
     * `docs/OPEN_ITEMS.md` P3-522. */
    uint8_t     jahr;     /**< roh; Jahre seit 1900 (libdsk schreibt tm_year) */
    uint8_t     monat;    /**< roh; 0-basiert (libdsk schreibt tm_mon) */
    uint8_t     tag;
    uint8_t     stunde;
    uint8_t     minute;
    uint8_t     sekunde;
    const char *text;
    size_t      text_len;
} uft_td0_anmerkung_t;

/**
 * @brief Eine TD0 als Strom: entpackt, vermessen, mit Kommentarblock.
 */
typedef struct {
    uint8_t    *strom;       /**< hinter dem 12-Byte-Kopf, bei `td` ENTPACKT */
    size_t      strom_len;
    size_t      daten_start; /**< Versatz der ersten Spur IM Strom */
    uint8_t     version;
    uint8_t     data_rate;
    uint8_t     sides;       /**< Kopfangabe, NICHT gemessen */
    bool        gepackt;
    uft_td0_anmerkung_t anmerkung;
    /* Aus dem Satzlauf GEMESSEN, nicht aus dem Kopf uebernommen. */
    unsigned    zylinder;
    unsigned    sektoren;    /**< groesste Sektorzahl einer Spur */
} uft_td0_strom_t;

/**
 * @brief Baut den Strom aus den rohen Dateibytes.
 *
 * Sagt AB statt zu kappen: eine Kommentarlaenge oder ein Datensatz, der
 * ueber das Stromende reicht, ist ein Formatfehler, kein Anlass zum
 * Kuerzen (Dauerregel D5).
 *
 * @return `UFT_OK` oder ein `uft_error_t`-Kode. Der Rueckgabetyp ist
 *         `int`, weil dieser Header `uft_error.h` nicht einbindet — die
 *         WERTE sind dieselben, nicht eine eigene Zaehlung.
 */
/**
 * @brief Teledisks CRC-16 ueber einen Block.
 *
 * Polynom **0xA097**, Anfangswert 0, MSB zuerst, NICHT gespiegelt.
 * Zwei unabhaengige Umsetzungen im Baum stimmen ueberein:
 *
 *   SAMdisk `src/samdisk/td0.cpp:71-85` — bitweise, nennt das
 *   Polynom ausdruecklich (`the CCITT polynomial 0xa097`).
 *   libdsk  `lib/comptlzh.c:711` — tabellengetrieben, ebenfalls
 *   Anfangswert 0.
 *
 * Beide sind **gelesen, nicht uebernommen** (Kanal *Spec* nach
 * MF-695); die Umsetzung hier ist aus der Parameterangabe neu
 * geschrieben und an 1602 Pruefsummen einer libdsk-Datei geeicht
 * (Dateikopf, Kommentar, 160 Spurkoepfe, 1440 Sektoren).
 *
 * @param daten Block.
 * @param len   Laenge in Byte.
 * @param start Anfangswert; 0 fuer einen eigenstaendigen Block.
 * @return Die volle CRC-16. Spur- und Sektorpruefsumme benutzen
 *         davon nur die unteren 8 Bit.
 */
uint16_t uft_td0_crc(const uint8_t *daten, size_t len, uint16_t start);

int uft_td0_strom_aus_bytes(const uint8_t *daten, size_t len,
                            uft_td0_strom_t *aus);

/** @brief Gibt den Strom frei und nullt die Struktur. */
void uft_td0_strom_frei(uft_td0_strom_t *s);

/**
 * @brief Laeuft den Strom ab und fuellt die angeforderte Spur.
 *
 * `track` wird mit `uft_track_init()` vorbereitet; der Aufrufer raeumt
 * mit `uft_track_cleanup()` ab.
 */
int uft_td0_strom_spur(const uft_td0_strom_t *s, int cyl, int head,
                       uft_track_t *track);

/**
 * @brief Baut aus dem Strom ein IMD-Abbild.
 *
 * Liest die KANONISCHEN Sektorfelder, die `uft_td0_strom_spur()` setzt —
 * `status`, `crc_ok`, `deleted` —, nicht noch einmal die TD0-Flaggen.
 * Eine Regel, eine Stelle (§MF-1177).
 */
int uft_td0_to_imd(const uft_td0_strom_t* s, struct uft_imd_image_t* imd);

/*============================================================================
 * LZSS Decompression Functions
 *============================================================================*/

/**
 * @brief Initialize LZSS decompression state
 * @param state Decompression state to initialize
 * @param data Compressed data
 * @param size Data size
 */
void uft_td0_lzss_init(uft_td0_lzss_state_t* state,
                       const uint8_t* data, size_t size);

/**
 * @brief Get next decompressed byte
 * @param state Decompression state
 * @return Decompressed byte or -1 on EOF
 */
int uft_td0_lzss_getbyte(uft_td0_lzss_state_t* state);

/**
 * @brief Read block of decompressed data
 * @param state Decompression state
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return Bytes actually read
 */
size_t uft_td0_lzss_read(uft_td0_lzss_state_t* state,
                         uint8_t* buffer, size_t size);

/*============================================================================
 * Sector Data Decoding
 *============================================================================*/

/**
 * @brief Get drive type name
 */
const char* uft_td0_drive_name(uft_td0_drive_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_UFT_TD0_H */
