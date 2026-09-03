/**
 * @file uft_atr.c
 * @brief Atari 8-bit ATR Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"
#include "uft/uft_log.h"

#define ATR_MAGIC           0x0296
#define ATR_HEADER_SIZE     16
#define ATR_BOOT_SECTORS    3
#define ATR_BOOT_SECTOR_SIZE 128

/**
 * @brief Ablage der drei Bootsektoren bei 256 Byte Sektorgroesse (MF-833).
 *
 * Bei 256 B je Sektor belegen die ersten drei Sektoren physisch 256 Byte,
 * tragen aber nur 128 Byte Nutzdaten — der Bootlader liest sie im
 * Single-Density-Modus. Wie ein ATR das ablegt, ist NICHT eindeutig; drei
 * Varianten sind im Umlauf. Quelle: Joe Allen, `atari-tools`,
 * `readme.md` §Image formats, abgeleitet aus „Structure of an SIO2PC
 * Atari disk image".
 *
 * Bis MF-833 nahm dieses Plugin LOGICAL fest an. Ein PHYSICAL-Abbild
 * wurde damit ab Sektor 4 um 384 Byte VERSCHOBEN gelesen — ohne Fehler,
 * ohne Warnung, nur mit falschen Daten (Rotbeweis:
 * `tests/test_atr_boot_layout.c`).
 *
 * `atari-tools` selbst setzt das Verfahren nicht um: sein `atr.c`
 * akzeptiert allein `size-16 == 128*3 + 256*717` und weist PHYSICAL und
 * WEIRD als „Unknown disk size" ab. Beschrieben ist es dort, umgesetzt
 * nicht.
 */
typedef enum {
    ATR_BOOT_UNKNOWN  = 0,  /**< nicht entscheidbar                    */
    ATR_BOOT_LOGICAL  = 1,  /**< 3 x 128 B, danach Sektorgroesse       */
    ATR_BOOT_PHYSICAL = 2,  /**< 3 x Sektorgroesse, je 128 B genutzt   */
    ATR_BOOT_WEIRD    = 3,  /**< 3 x 128 B + 3 x 128 B Nullen          */
    /* MF-834: der Fall, den die Referenzimplementierung SELBST schreibt.
     * Nick Kennedys SIO2PC adressiert Sektor n>3 bei `0x190 + (n-4)*256`
     * (`8BUSCMND.S`: „Offset past 10h header & 180h 3 SD") — also
     * LOGICAL —, deklariert die 180-K-Diskette aber mit `(256/16)*720
     * = 11520` Absaetzen, also 184320 Byte. Adressiert werden davon nur
     * 183936; die letzten 384 Byte sind SCHLACKE.
     *
     * Damit ist die Nutzlast durch 256 teilbar und das Layout trotzdem
     * LOGICAL. Die Regel aus dem atari-tools-Readme („durch 256 teilbar
     * -> PHYSICAL oder WEIRD") deckt diesen Fall NICHT ab, und MF-833
     * hat ihn deshalb auf PHYSICAL abgebildet — 384 Byte verschoben,
     * genau bei den Dateien, die das Originalprogramm erzeugt hat.
     *
     * DIE TRICHOTOMIE IST DAMIT ERKLAERT: nicht drei Deutungen einer
     * klaren Spezifikation, sondern eine in sich WIDERSPRUECHLICHE
     * Referenzimplementierung — wer ihrer Groessenangabe folgte, baute
     * PHYSICAL, wer ihrer Adressrechnung folgte, LOGICAL. */
    ATR_BOOT_LOGICAL_SLACK = 4  /**< LOGICAL, Datei 384 B laenger */
} atr_boot_layout_t;

typedef struct {
    FILE*       file;
    uint16_t    sector_size;
    uint32_t    total_sectors;
    /* MF-833 */
    atr_boot_layout_t boot_layout;
    uint16_t    sector_size_raw;    /**< Kopfwert, auch wenn unbekannt  */
    bool        sector_size_assumed;/**< true: 128 B nur ANGENOMMEN     */
    /* MF-834: zwei Kopffelder, die bisher niemand gelesen hat. */
    uint8_t     flags_raw;          /**< H_FLAGS, unveraendert          */
    bool        copy_prot;          /**< b4: kopiergeschuetzte Diskette */
    bool        write_prot;         /**< b5: schreibgeschuetztes Abbild */
    uint16_t    first_bad_sec;      /**< BAD_1ST, erster VOLLSTAENDIGER Satz */
} atr_data_t;

/* Flags-Bits nach `2SIOTEXT.S:619-620` (Nick Kennedy). */
#define ATR_FLAG_COPY_PROTECTED  0x10u
#define ATR_FLAG_WRITE_PROTECTED 0x20u

/* Standardgroessen in 16-Byte-Absaetzen, `2SIOTEXT.S:1125`:
 *   SIZES: DW 01000h,(128/16)*720,(128/16)*1040,(256/16)*720 */
#define ATR_PARA_64K_RAMDISK  0x1000u  /*  4096 */
#define ATR_PARA_90K_SD        5760u
#define ATR_PARA_130K_ED       8320u
#define ATR_PARA_180K_DD      11520u

static bool atr_para_ist_standard(uint16_t lo) {
    return lo == ATR_PARA_64K_RAMDISK || lo == ATR_PARA_90K_SD ||
           lo == ATR_PARA_130K_ED     || lo == ATR_PARA_180K_DD;
}

/** Nutzlastlaenge (ohne Kopf), aus der die drei Bootsektoren bestehen. */
static size_t atr_boot_span(atr_boot_layout_t layout, uint16_t sector_size) {
    switch (layout) {
    case ATR_BOOT_PHYSICAL: return (size_t)ATR_BOOT_SECTORS * sector_size;
    case ATR_BOOT_WEIRD:    return 2u * ATR_BOOT_SECTORS * ATR_BOOT_SECTOR_SIZE;
    /* LOGICAL_SLACK liegt wie LOGICAL — der Unterschied steckt allein in
     * der Dateilaenge, nicht in den Versaetzen. */
    default:                return (size_t)ATR_BOOT_SECTORS * ATR_BOOT_SECTOR_SIZE;
    }
}

static size_t atr_sector_offset(int sector, uint16_t sector_size,
                                atr_boot_layout_t layout) {
    if (sector < 1) return 0;
    if (sector <= ATR_BOOT_SECTORS) {
        /* Die Nutzdaten der Bootsektoren liegen in allen drei Varianten
         * an derselben Stelle — ausser bei PHYSICAL, wo sie um die volle
         * Sektorgroesse auseinander stehen. */
        size_t schritt = (layout == ATR_BOOT_PHYSICAL)
                       ? (size_t)sector_size : (size_t)ATR_BOOT_SECTOR_SIZE;
        return ATR_HEADER_SIZE + (size_t)(sector - 1) * schritt;
    }
    return ATR_HEADER_SIZE + atr_boot_span(layout, sector_size) +
           (size_t)(sector - ATR_BOOT_SECTORS - 1) * sector_size;
}

/**
 * @brief Boot-Layout aus Nutzlastlaenge und Bootbereich bestimmen (MF-833).
 *
 * Verfahren nach der oben genannten Quelle:
 *   1. durch 128 teilbar, aber NICHT durch 256  ->  LOGICAL
 *   2. durch 256 teilbar  ->  PHYSICAL oder WEIRD; Byte 384..767
 *      pruefen: alles Null -> wahrscheinlich WEIRD, sonst PHYSICAL
 *
 * Nachgerechnet an der Standard-DD-Diskette (720 Sektoren):
 *   LOGICAL   384 + 717*256 = 183936, 183936 % 256 = 128  -> Regel 1
 *   PHYSICAL  768 + 717*256 = 184320, teilbar              -> Regel 2
 *   WEIRD     768 + 717*256 = 184320, GLEICHE Laenge       -> Regel 2
 * Die Laenge allein trennt PHYSICAL und WEIRD also nicht; deshalb der
 * Byte-Bereich.
 *
 * @param f          offene Datei, Position wird wiederhergestellt
 * @param payload    Dateilaenge ohne die 16 Kopfbytes
 */
static bool alles_null(FILE *f, long off, size_t len)
{
    long merk = ftell(f);
    if (merk < 0 || fseek(f, off, SEEK_SET) != 0) return false;
    bool null = true;
    uint8_t puf[256];
    size_t rest = len;
    while (rest) {
        size_t n = rest > sizeof puf ? sizeof puf : rest;
        if (fread(puf, 1, n, f) != n) { null = false; break; }
        for (size_t i = 0; i < n; i++) if (puf[i]) { null = false; break; }
        if (!null) break;
        rest -= n;
    }
    (void)fseek(f, merk, SEEK_SET);
    return null;
}

/**
 * @brief Boot-Layout bestimmen (MF-833, Entscheidungstafel MF-834).
 *
 * Schritt 1 stammt aus dem atari-tools-Readme (Joe Allen): Nutzlast
 * durch 128 teilbar, aber NICHT durch 256 -> LOGICAL. Das ist eindeutig.
 *
 * Schritt 2 stand dort als „Byte 384..767 pruefen: alles Null ->
 * wahrscheinlich WEIRD, sonst PHYSICAL". Diese Regel ist UNVOLLSTAENDIG:
 * sie kennt den vierten Fall nicht, den die Referenzimplementierung
 * selbst erzeugt (LOGICAL_SLACK, siehe dort). MF-833 folgte ihr woertlich
 * und las SIO2PC-Originaldateien 384 Byte verschoben.
 *
 * Deshalb ZWEI Zeugen statt einem — der Anfang UND das Ende:
 *
 *   A = Byte 384..767 sind Null      (der Bootbereich ist aufgefuellt)
 *   B = die letzten 384 Byte sind Null (das Ende ist unadressiert)
 *
 *   A  B  Layout          Begruendung
 *   -  -  ------------    ------------------------------------------
 *   0  1  LOGICAL_SLACK   Sektor 4 steht bei 384, das Ende ist Schlacke
 *   0  0  PHYSICAL        Bootbereich belegt, Daten laufen bis zum Ende
 *   1  0  WEIRD           drei Nullsektoren, Daten laufen bis zum Ende
 *   1  1  unentscheidbar  -> LOGICAL, weil die Referenz so ADRESSIERT
 *
 * Die letzte Zeile ist eine bewusste Wahl, nicht Verlegenheit: bei einer
 * weitgehend leeren Diskette tragen beide Zeugen nichts, und dann ist
 * die Adressrechnung der Referenzimplementierung die bessere Annahme als
 * ihre Groessenangabe. Sie wird gemeldet.
 */
static atr_boot_layout_t atr_probe_boot_layout(FILE *f, size_t payload,
                                               uint16_t sector_size)
{
    /* Bei 128 B sind Boot- und Datensektoren gleich gross — es gibt
     * nichts zu unterscheiden. Bei 512 B ist keine Variante belegt;
     * das bisherige Verhalten (LOGICAL) bleibt, damit MF-340 haelt. */
    if (sector_size != 256) return ATR_BOOT_LOGICAL;

    if ((payload % 128u) == 0 && (payload % 256u) != 0)
        return ATR_BOOT_LOGICAL;

    if ((payload % 256u) != 0) {
        UFT_WARN("ATR: Nutzlast %zu Byte ist weder durch 128 noch durch 256 "
                 "teilbar — Boot-Layout unbestimmt, LOGICAL angenommen",
                 payload);
        return ATR_BOOT_UNKNOWN;
    }

    if (payload < 768u) return ATR_BOOT_UNKNOWN;

    bool a = alles_null(f, ATR_HEADER_SIZE + 384, 384);
    bool b = alles_null(f, (long)(ATR_HEADER_SIZE + payload - 384), 384);

    if (!a && b) {
        UFT_WARN("ATR: Nutzlast %zu ist 256-teilbar, aber die letzten 384 "
                 "Byte sind leer — LOGICAL mit Schlacke (SIO2PC-Original)",
                 payload);
        return ATR_BOOT_LOGICAL_SLACK;
    }
    if (!a && !b) return ATR_BOOT_PHYSICAL;
    if (a && !b)  return ATR_BOOT_WEIRD;

    /* a && b: beide Zeugen stumm. */
    UFT_WARN("ATR: Boot-Layout nicht entscheidbar (Byte 384-767 UND die "
             "letzten 384 Byte sind leer) — LOGICAL angenommen, weil die "
             "Referenzimplementierung so adressiert");
    return ATR_BOOT_LOGICAL;
}

bool atr_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < ATR_HEADER_SIZE) return false;
    if (uft_read_le16(data) == ATR_MAGIC) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t atr_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[ATR_HEADER_SIZE];
    if (fread(header, 1, ATR_HEADER_SIZE, f) != ATR_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    if (uft_read_le16(header) != ATR_MAGIC) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    atr_data_t* pdata = calloc(1, sizeof(atr_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    /* MF-834: HI_XSIZE ist ein WORT bei Offset 6-7, nicht ein Byte.
     * Layout nach Nick Kennedys `2SIOTEXT.S:1160-1167`:
     *   0-1 HEADER (Magic)  2-3 XSIZE  4-5 SEC_XSIZE
     *   6-7 HI_XSIZE        8 H_FLAGS  9-10 BAD_1ST  11-15 unbenutzt
     *
     * UND: SIO2PC VOR Revision 3.00 schrieb MUELL in HI_XSIZE. Der Autor
     * hat dafuer eine versteckte Reparaturoption eingebaut, die das Wort
     * schlicht nullt („This fixes it!!!", `A_DISKS.S:1256`), mit dem
     * Bildschirmtext „PRESS Y TO FORCE FILE SIZE TO STANDARD (FIX BUG)".
     * Er nennt es selbst einen Bug.
     *
     * Erkennung nach dem Verfahren des Autors (`A_DISKS.S:1265`): nur bei
     * HI_XSIZE == 0 wird gegen die Standardgroessentabelle geprueft.
     * Umgekehrt gilt: passt das Low-Wort EXAKT auf eine Standardgroesse
     * und ist das High-Wort ungleich 0, dann ist das High-Wort mit hoher
     * Wahrscheinlichkeit der bekannte Muell. Bedingungslos addiert ergaebe
     * es eine bis zu 255-fach zu grosse Diskette. */
    uint16_t para_lo = uft_read_le16(&header[2]);
    uint16_t para_hi = uft_read_le16(&header[6]);
    if (para_hi != 0 && atr_para_ist_standard(para_lo)) {
        UFT_WARN("ATR-Kopf: HI_XSIZE ist 0x%04X, XSIZE 0x%04X entspricht "
                 "aber genau einer Standardgroesse — bekannter Fehler in "
                 "SIO2PC vor Rev. 3.00; HI_XSIZE ignoriert",
                 (unsigned)para_hi, (unsigned)para_lo);
        para_hi = 0;
    }
    uint32_t paragraphs = (uint32_t)para_lo | ((uint32_t)para_hi << 16);
    uint32_t disk_bytes = paragraphs * 16;

    /* MF-834: zwei Kopffelder, die bisher niemand gelesen hat.
     * Flags-Byte bei 8 (`2SIOTEXT.S:619-620`): b4 = kopiergeschuetzte
     * Diskette, b5 = schreibgeschuetztes Abbild. Die uebrigen Bits
     * betrachtet SIO2PC ausdruecklich nicht („at present just look at b4
     * and b5") — also erhalten, nicht deuten.
     * Wort bei 9-10 ist NICHT „irgendein defekter Sektor", sondern der
     * erste Sektor, fuer den ein VOLLSTAENDIGER Satz aus Good- UND
     * Bad-Status vorliegt (`B_1050.S:748-753`). */
    pdata->flags_raw     = header[8];
    pdata->copy_prot     = (header[8] & ATR_FLAG_COPY_PROTECTED)  != 0;
    pdata->write_prot    = (header[8] & ATR_FLAG_WRITE_PROTECTED) != 0;
    pdata->first_bad_sec = uft_read_le16(&header[9]);
    if (header[8] & (uint8_t)~(ATR_FLAG_COPY_PROTECTED |
                               ATR_FLAG_WRITE_PROTECTED)) {
        UFT_WARN("ATR-Kopf: Flags 0x%02X traegt Bits ausserhalb b4/b5 — "
                 "unbekannte Erweiterung, unveraendert erhalten",
                 (unsigned)header[8]);
    }
    pdata->sector_size     = uft_read_le16(&header[4]);
    pdata->sector_size_raw = pdata->sector_size;
    /* 128 (SD/ED), 256 (DD), and 512 (SpartaDOS X / Altirra/AspeQt large ATR).
     * The first 3 boot sectors stay 128 B regardless (handled in read/offset). */
    if (pdata->sector_size != 128 && pdata->sector_size != 256 &&
        pdata->sector_size != 512) {
        /* MF-833: der Rueckfall auf 128 war STILL. Ein unbekannter Wert
         * ist ein Befund ueber die Datei, keine Gelegenheit zum Raten —
         * 128 als Arbeitsannahme ist vertretbar, aber der Kopfwert
         * gehoert in den Bericht, sonst ist die Herkunft der Zahl im
         * Nachhinein nicht rekonstruierbar. */
        UFT_WARN("ATR-Kopf: Sektorgroesse %u ist keine bekannte "
                 "(128/256/512); mit 128 weitergearbeitet",
                 (unsigned)pdata->sector_size_raw);
        pdata->sector_size         = 128;
        pdata->sector_size_assumed = true;
    }

    /* MF-833: das Boot-Layout aus der TATSAECHLICHEN Dateilaenge
     * bestimmen, nicht aus der Absatzzahl des Kopfes. `atr2imd.c`
     * derselben Quelle sagt dazu ausdruecklich „Get actual image size,
     * don't trust size from header" — und ein Kopf, der die Laenge
     * falsch angibt, waere gerade bei einer Variantenfrage der
     * schlechteste Zeuge. */
    size_t payload = 0;
    if (fseek(f, 0, SEEK_END) == 0) {
        long ende = ftell(f);
        if (ende > ATR_HEADER_SIZE) payload = (size_t)ende - ATR_HEADER_SIZE;
    }
    (void)fseek(f, ATR_HEADER_SIZE, SEEK_SET);

    pdata->boot_layout = atr_probe_boot_layout(f, payload, pdata->sector_size);

    /* MF-834: Sektorzahl nach EINER Regel, die alle belegten Faelle
     * trifft — und nicht mehr nach der LOGICAL-Formel allein.
     *
     * SIO2PC rechnet `Absaetze / (Sektorgroesse/16)` (`A_DISKS.S:174`),
     * also OHNE Sonderbehandlung der drei 128-Byte-Bootsektoren. Das
     * stimmt fuer jede Nutzlast, die ein Vielfaches der Sektorgroesse
     * ist. Ein echtes LOGICAL-Abbild ist das NICHT (183936 % 256 = 128),
     * dort gilt `3 + (Nutzlast - 384)/Sektorgroesse`.
     *
     * Nachgerechnet an allen fuenf belegten Faellen:
     *   SD  90K   92160 % 128 = 0  ->   92160/128 =  720
     *   ED 130K  133120 % 128 = 0  ->  133120/128 = 1040
     *   LOGICAL  183936 % 256 = 128 -> 3 + 717    =  720
     *   SIO2PC   184320 % 256 = 0  ->  184320/256 =  720
     *   SDX 512    2944 % 512 = 384 -> 3 +   5    =    8   (MF-340)
     *
     * Die alte Rechnung ergab beim SIO2PC-Fall 721 — ein Sektor, der nie
     * einer war, mit echtem Inhalt aus der Schlacke. */
    {
        size_t nutz  = payload ? payload : (size_t)disk_bytes;
        if (pdata->sector_size && (nutz % pdata->sector_size) == 0) {
            pdata->total_sectors = (uint32_t)(nutz / pdata->sector_size);
        } else {
            size_t bspan = atr_boot_span(pdata->boot_layout,
                                         pdata->sector_size);
            pdata->total_sectors = (nutz > bspan)
                ? ATR_BOOT_SECTORS +
                  (uint32_t)((nutz - bspan) / pdata->sector_size)
                : (uint32_t)(nutz / ATR_BOOT_SECTOR_SIZE);
        }
        if (payload && disk_bytes && payload != (size_t)disk_bytes) {
            UFT_WARN("ATR: Kopf nennt %u Byte Nutzlast, die Datei hat %zu — "
                     "mit der Datei weitergearbeitet",
                     (unsigned)disk_bytes, payload);
        }
    }

    disk->plugin_data = pdata;
    /* MF-834: Sektoren je Spur aus der Standardgroesse, nicht fest 18.
     * SIO2PCs Groessentabelle (`2SIOTEXT.S:1125`) nennt genau vier
     * Formate: 4096 Absaetze (64-K-RAMdisk), 5760 (90 K SD, 18 Sektoren),
     * 8320 (130 K ED, **26** Sektoren) und 11520 (180 K DD, 18).
     * Mit fest 18 meldete Enhanced Density 58 Zylinder statt 40 — die
     * Sektorzahl stimmte, die Spuraufteilung nicht. */
    uint16_t spt;
    if      (pdata->total_sectors == 1040u) spt = 26u;   /* DOS 2.5 ED */
    else if (pdata->total_sectors ==  720u) spt = 18u;   /* SD und DD  */
    else {
        spt = 18u;
        UFT_WARN("ATR: %u Sektoren entsprechen keinem der vier "
                 "Standardformate — 18 Sektoren/Spur angenommen",
                 (unsigned)pdata->total_sectors);
    }
    disk->geometry.cylinders = (pdata->total_sectors + spt - 1u) / spt;
    disk->geometry.heads = 1;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = pdata->sector_size;
    disk->geometry.total_sectors = pdata->total_sectors;
    
    return UFT_OK;
}

static void atr_close(uft_disk_t* disk) {
    atr_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t atr_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    atr_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file || head != 0) return UFT_ERROR_INVALID_STATE;
    
    uft_track_init(track, cyl, head);
    
    uint8_t* sec_buf = malloc(pdata->sector_size);
    if (!sec_buf) return UFT_ERROR_NO_MEMORY;
    for (int s = 0; s < 18; s++) {
        uint32_t sector_num = cyl * 18 + s + 1;
        if (sector_num > pdata->total_sectors) break;
        
        uint16_t this_size = (sector_num <= ATR_BOOT_SECTORS) ? 
                            ATR_BOOT_SECTOR_SIZE : pdata->sector_size;
        
        memset(sec_buf, 0, pdata->sector_size);
        if (fseek(pdata->file, atr_sector_offset(sector_num, pdata->sector_size,
                                          pdata->boot_layout), SEEK_SET) != 0) { free(sec_buf); return UFT_ERROR_IO; }
        if (fread(sec_buf, 1, this_size, pdata->file) != this_size) {
            memset(sec_buf, 0xE5, this_size); /* forensic fill on read error */
        }
        uft_format_add_sector(track, s, sec_buf, this_size, cyl, head);
    }
    free(sec_buf);
    
    return UFT_OK;
}

static uft_error_t atr_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. MF-519 hat das fuer
     * read_track getan und write_track uebersehen. Das ASan-Tor
     * der CI fand die Folge an d80_write_track: die Schranke
     * `cyl >= D80_TRACKS` laesst -1 durch, und d80_spt[-1] liest
     * vor der Tabelle.
     *
     * Beim SCHREIBEN wiegt das schwerer als beim Lesen: ein
     * falscher Index liefert nicht nur falsche Daten, er bestimmt,
     * WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    /* MF-522: gegen die Geometrie pruefen, die dieses Plugin bei `open`
     * SELBST gemeldet hat. Ohne diese Schranke rechnete die Zeile darunter
     * einen Offset aus beliebigen Koordinaten:
     *
     *   cyl=1000 -> Offset weit hinter dem Dateiende. `fseek` gelingt,
     *               `fwrite` verlaengert die Datei. Aus 880 KB wurden im
     *               Test 11 MB, und der Aufrufer bekam UFT_OK.
     *   cyl=-1   -> Offset konnte auf 0 zurueckfallen und damit SPUR 0
     *               ueberschreiben. Ein gueltiger Ort, erreicht ueber eine
     *               unsinnige Koordinate.
     *
     * Beides ist eine stille Veraenderung mit Erfolgsmeldung — genau das,
     * was DESIGN_PRINCIPLES verbietet. Gefunden von
     * tests/test_disk_write_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;
    if (cyl >= (int)disk->geometry.cylinders ||
        head >= (int)disk->geometry.heads) return UFT_ERROR_INVALID_PARAM;

    atr_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (size_t s = 0; s < track->sector_count && s < 18; s++) {
        uint32_t sec_num = (uint32_t)(cyl * 18 + s + 1);
        if (sec_num > pdata->total_sectors) break;
        uint16_t this_size = (sec_num <= ATR_BOOT_SECTORS) ?
                             ATR_BOOT_SECTOR_SIZE : pdata->sector_size;
        if (fseek(pdata->file, atr_sector_offset(sec_num, pdata->sector_size,
                                          pdata->boot_layout),
                  SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        if (!data) continue;
        size_t len = track->sectors[s].data_len;
        if (len > this_size) len = this_size;
        if (fwrite(data, 1, len, pdata->file) != len) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_atr_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_atr = {
    .name = "ATR",
    .description = "Atari 8-bit Disk Image",
    .extensions = "atr;xfd",
    .version = 0x00010000,
    .format = UFT_FORMAT_ATR,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = atr_probe,
    .open = atr_open,
    .close = atr_close,
    .read_track = atr_read_track,
    .write_track = atr_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* SIO2PC/APE docs, Atari 810 hw reference fully describe ATR */
    .features = uft_format_plugin_atr_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_atr_features) / sizeof(uft_format_plugin_atr_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(atr)
