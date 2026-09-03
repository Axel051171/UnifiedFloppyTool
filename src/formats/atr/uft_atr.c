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
    ATR_BOOT_WEIRD    = 3   /**< 3 x 128 B + 3 x 128 B Nullen          */
} atr_boot_layout_t;

typedef struct {
    FILE*       file;
    uint16_t    sector_size;
    uint32_t    total_sectors;
    /* MF-833 */
    atr_boot_layout_t boot_layout;
    uint16_t    sector_size_raw;    /**< Kopfwert, auch wenn unbekannt  */
    bool        sector_size_assumed;/**< true: 128 B nur ANGENOMMEN     */
} atr_data_t;

/** Nutzlastlaenge (ohne Kopf), aus der die drei Bootsektoren bestehen. */
static size_t atr_boot_span(atr_boot_layout_t layout, uint16_t sector_size) {
    switch (layout) {
    case ATR_BOOT_PHYSICAL: return (size_t)ATR_BOOT_SECTORS * sector_size;
    case ATR_BOOT_WEIRD:    return 2u * ATR_BOOT_SECTORS * ATR_BOOT_SECTOR_SIZE;
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
static atr_boot_layout_t atr_probe_boot_layout(FILE *f, size_t payload,
                                               uint16_t sector_size) {
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

    long merk = ftell(f);
    if (merk < 0) return ATR_BOOT_UNKNOWN;
    if (fseek(f, ATR_HEADER_SIZE + 384, SEEK_SET) != 0) return ATR_BOOT_UNKNOWN;

    uint8_t puf[384];
    size_t n = fread(puf, 1, sizeof puf, f);
    (void)fseek(f, merk, SEEK_SET);
    if (n != sizeof puf) return ATR_BOOT_UNKNOWN;

    for (size_t i = 0; i < sizeof puf; i++)
        if (puf[i] != 0) return ATR_BOOT_PHYSICAL;

    /* Alles Null. WEIRD ist wahrscheinlich — sicher ist es nicht: ein
     * PHYSICAL-Abbild, dessen Bootsektoren 4..6 leer sind, sieht genauso
     * aus. Die Quelle sagt ausdruecklich „probably". Also melden, nicht
     * behaupten. Die beiden Deutungen liegen hier ohnehin gleich (beide
     * Bootbereiche 768 Byte), der Unterschied betrifft nur die
     * BENENNUNG im Bericht. */
    UFT_WARN("ATR: Byte 384-767 sind Null — WEIRD-Layout angenommen; "
             "PHYSICAL mit leeren Bootsektoren 4-6 ist nicht auszuschliessen");
    return ATR_BOOT_WEIRD;
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
    uint32_t paragraphs = uft_read_le16(&header[2]) | ((uint32_t)header[6] << 16);
    uint32_t disk_bytes = paragraphs * 16;
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

    /* Sektorzahl aus derselben Nutzlast und demselben Layout ableiten.
     * Bis MF-833 rechnete diese Stelle mit `disk_bytes` aus dem Kopf UND
     * dem LOGICAL-Bootbereich; bei PHYSICAL/WEIRD lag sie damit daneben. */
    {
        size_t bspan = atr_boot_span(pdata->boot_layout, pdata->sector_size);
        size_t nutz  = payload ? payload : (size_t)disk_bytes;
        if (nutz <= bspan) {
            pdata->total_sectors = (uint32_t)(nutz / ATR_BOOT_SECTOR_SIZE);
        } else {
            pdata->total_sectors = ATR_BOOT_SECTORS +
                (uint32_t)((nutz - bspan) / pdata->sector_size);
        }
        if (payload && disk_bytes && payload != (size_t)disk_bytes) {
            UFT_WARN("ATR: Kopf nennt %u Byte Nutzlast, die Datei hat %zu — "
                     "mit der Datei weitergearbeitet",
                     (unsigned)disk_bytes, payload);
        }
    }
    disk->plugin_data = pdata;
    disk->geometry.cylinders = (pdata->total_sectors + 17) / 18;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
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
