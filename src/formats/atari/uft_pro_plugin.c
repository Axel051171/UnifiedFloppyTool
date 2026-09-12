/**
 * @file uft_pro_plugin.c
 * @brief PRO (APE ProSystem, Atari 8-bit) - Leser
 *
 * PRO legt eine Atari-Diskette Sektor fuer Sektor ab und haengt jedem
 * Sektor einen 12-Byte-Kopf mit dem Controllerstatus und den
 * Phantomsektor-Angaben voran. Das Format dient dem Kopierschutz: ein
 * Sektor kann mehrfach auf der Spur liegen, und aufeinanderfolgende
 * Lesevorgaenge liefern verschiedene Ausfertigungen.
 *
 * -- Zwei benannte Haende (MF-1054) -------------------------------------
 *
 * 1. **Beschreibung:** `whizzosoftware.com/sio2arduino/prosys.html`
 *    (sio2arduino). Sie sagt ueber sich selbst: "Since the PRO format
 *    has never been officially documented, the information presented
 *    here is based on reverse engineering ... therefore, this
 *    information has a very good chance of being completely wrong and
 *    is at best an educated guess." Allein traegt sie nach der
 *    EINFRIER-REGEL nicht.
 * 2. **Umsetzung:** `atari800/src/sio.c` (GPL-2), Klon unter
 *    `tools/uft-scout/work/atari800/`, Quellstand b6bdf05c. **Nur
 *    gelesen**, keine Zeile uebernommen. Sie bestaetigt jedes Feld der
 *    Beschreibung:
 *
 *      Erkennung   `(len-16) % 140 == 0` und
 *                  `data[0]*256 + data[1] == (len-16)/140` und
 *                  `data[2] == 'P'`                     (sio.c:500-508)
 *      Versatz     `16 + 140*(sektor-1)`, Sektorgroesse immer 128
 *                                        (SIO_SizeOfSector, PRO-Zweig)
 *      Sektorkopf  12 Byte; Byte 1 = Status, `0xFF` heisst GUT (sio.c:711)
 *                  Byte 5 = Phantomzahl, Byte 6..10 = Indizes (sio.c:691)
 *                  Phantomsektor = nominale Sektorzahl + Index
 *      Nennweite   1040, wenn die Datei mindestens 1040 Saetze fasst,
 *                  sonst 720                            (sio.c:515-523)
 *
 * -- Was der Vorzustand tat (gemessen MF-1054) --------------------------
 *
 * Er suchte eine **erfundene Kennung** `"APRO"`/`"KPRO"` bei Versatz 0.
 * In einer echten PRO-Datei steht dort die Sektorzahl. Gemessen an einer
 * spezifikationsgerechten Datei (100 816 Byte, Kopf `02 d0 50 32`):
 * `probe` = **0**, `open` = **-25**. Das ist die Klasse von MF-961
 * (`86f`), MF-1022 (`sap`), MF-1029 (`myz80`), MF-1030 (`nanowasp`) und
 * MF-1032 (`logical`) - zum **sechsten** Mal.
 *
 * Dazu war der ganze Aufbau erfunden: der alte Kopfkommentar sagte
 * "16-byte header + sector entries (4 bytes each) + sector data". Es
 * gibt keine Sektortabelle; Kopf und Daten liegen **verschraenkt**, je
 * Sektor 12 + 128 Byte. Die Geometrie kam aus `raw[7]`/`raw[6]` -
 * Polsterbytes -, und die Statusbits (`flags & 0x02` weak,
 * `flags & 0x04` CRC) standen an einer Stelle, die es nicht gibt.
 * **Phantomsektoren, der Zweck des Formats, kamen gar nicht vor.**
 *
 * Abgenommen: `tests/test_pro_gegen_atari800.c`.
 */
#include "uft/uft_format_common.h"

#define PRO_HEADER_SIZE  16     /* Dateikopf                           */
#define PRO_SEC_HEADER   12     /* Kopf je Sektor                      */
#define PRO_SECTOR_SIZE 128     /* Nutzlast je Sektor, immer 128       */
#define PRO_RECORD      (PRO_SEC_HEADER + PRO_SECTOR_SIZE)   /* = 140  */
#define PRO_SPT          18     /* Atari-Konvention: 18 Sektoren/Spur  */
#define PRO_MAX_PHANTOM   5     /* fuenf Indizes, Byte 6..10           */

/* Status im Sektorkopf: 0xFF heisst "ohne Befund". Jeder andere Wert ist
 * ein Befund des 1771-Controllers; atari800 antwortet darauf 'E'. */
#define PRO_STATUS_OK 0xFFu

typedef struct {
    uint8_t *file_data;
    size_t   file_size;
    uint32_t records;      /* Saetze in der Datei = Sektorzahl im Kopf */
    uint32_t nominal;      /* 720 oder 1040 - die Diskette selbst      */
} pro_pd_t;

/* Die Erkennung ist genau die von atari800: drei Bedingungen, keine
 * Kennung. Die Sektorzahl im Kopf muss die Dateigroesse RESTLOS
 * erklaeren - das ist eine Selbstpruefung und deshalb mehr wert als ein
 * einzelnes Byte. */
static bool pro_kopf_lesen(const uint8_t *d, size_t file_size,
                           uint32_t *records, uint32_t *nominal)
{
    uint32_t n;

    if (!d || file_size < PRO_HEADER_SIZE + PRO_RECORD) return false;
    if ((file_size - PRO_HEADER_SIZE) % PRO_RECORD != 0) return false;

    n = (uint32_t)d[0] * 256u + (uint32_t)d[1];
    if (n == 0) return false;
    if (n != (uint32_t)((file_size - PRO_HEADER_SIZE) / PRO_RECORD))
        return false;
    if (d[2] != 'P') return false;

    *records = n;
    /* Die Nennweite steht NICHT in der Datei; atari800 leitet sie aus der
     * Groesse ab. Die Grenze ist damit geerbt und benannt: eine
     * 720-Sektoren-Diskette mit sehr vielen Phantomsaetzen wuerde hier
     * faelschlich als 1040er gelten. Erfunden wird sie trotzdem nicht -
     * sie ist die Aussage der einzigen ausgefuehrten Umsetzung. */
    *nominal = (file_size >= (size_t)1040 * PRO_RECORD + PRO_HEADER_SIZE)
                   ? 1040u : 720u;
    if (*nominal > n) *nominal = n;
    return true;
}

/* Konfidenz 75, Band "Struktur gelesen" (50..79) nach MF-729: geprueft
 * sind drei Dinge - ein Kennbyte, die Restlosigkeit der Groesse und die
 * Uebereinstimmung der Sektorzahl mit ihr. Keine Signatur, also nicht
 * das Band "Merkmal getroffen"; der Vorzustand stand auf 96 und beruhte
 * auf einer Kennung, die es nicht gibt. */
static bool pro_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    uint32_t records, nominal;
    (void)size;
    if (!pro_kopf_lesen(data, file_size, &records, &nominal)) return false;
    if (confidence) *confidence = 75;
    return true;
}

static uft_error_t pro_open(uft_disk_t *disk, const char *path, bool ro) {
    size_t raw_size = 0;
    uint8_t *raw;
    pro_pd_t *p;
    uint32_t records, nominal;

    (void)ro;
    raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FORMAT_INVALID;

    if (!pro_kopf_lesen(raw, raw_size, &records, &nominal)) {
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }

    p = calloc(1, sizeof(pro_pd_t));
    if (!p) { free(raw); return UFT_ERROR_NO_MEMORY; }
    p->file_data = raw;
    p->file_size = raw_size;
    p->records = records;
    p->nominal = nominal;

    disk->plugin_data = p;
    disk->geometry.cylinders = (int)((nominal + PRO_SPT - 1u) / PRO_SPT);
    disk->geometry.heads = 1;
    disk->geometry.sectors = PRO_SPT;
    disk->geometry.sector_size = PRO_SECTOR_SIZE;
    disk->geometry.total_sectors = nominal;
    return UFT_OK;
}

static void pro_close(uft_disk_t *disk) {
    pro_pd_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t pro_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    pro_pd_t *p = disk->plugin_data;
    if (!p || !p->file_data || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    for (int s = 0; s < PRO_SPT; s++) {
        uint32_t nr = (uint32_t)cyl * PRO_SPT + (uint32_t)s + 1u;
        size_t off;
        const uint8_t *h;
        int ph;

        if (nr > p->nominal) break;

        off = (size_t)PRO_HEADER_SIZE + (size_t)(nr - 1u) * PRO_RECORD;
        if (off + PRO_RECORD > p->file_size) break;
        h = p->file_data + off;

        uft_format_add_sector(track, (uint8_t)s,
                              h + PRO_SEC_HEADER, PRO_SECTOR_SIZE,
                              (uint8_t)cyl, 0);

        /* Der Controllerstatus steht in Byte 1. `uft_format_add_sector()`
         * setzt jeden Sektor unbedingt auf OK (siehe seinen Kopf), also
         * muss der Befund hier NACHGETRAGEN werden - sonst ist ein
         * Sektor, den der Controller beanstandet hat, von einem guten
         * nicht zu unterscheiden (Klasse MF-980/Tor 62). */
        if (track->sector_count > 0 && h[1] != PRO_STATUS_OK) {
            uft_sector_t *sec = &track->sectors[track->sector_count - 1];
            sec->status = UFT_SECTOR_CRC_ERROR;
            sec->crc_ok = false;
        }

        /* Phantomsektoren: Byte 5 zaehlt sie, Byte 6..10 tragen die
         * Indizes, und der Satz liegt bei `nominal + index`.
         *
         * atari800 gibt bei aufeinanderfolgenden Lesevorgaengen REIHUM
         * eine andere Ausfertigung aus - das ist die Sicht eines
         * Emulators. Ein forensisches Werkzeug will alle sehen, also
         * liegen sie hier als zusaetzliche Sektoren auf derselben Spur,
         * mit derselben logischen Nummer und `UFT_SECTOR_DUPLICATE`.
         * Genau so liegen sie auch auf der Diskette. */
        ph = (int)h[5];
        if (ph > PRO_MAX_PHANTOM) ph = PRO_MAX_PHANTOM;
        for (int i = 1; i <= ph; i++) {
            uint32_t pnr = p->nominal + (uint32_t)h[5 + i];
            size_t poff;
            const uint8_t *phdr;

            if (h[5 + i] == 0 || pnr > p->records) continue;
            poff = (size_t)PRO_HEADER_SIZE + (size_t)(pnr - 1u) * PRO_RECORD;
            if (poff + PRO_RECORD > p->file_size) continue;
            phdr = p->file_data + poff;

            uft_format_add_sector(track, (uint8_t)s,
                                  phdr + PRO_SEC_HEADER, PRO_SECTOR_SIZE,
                                  (uint8_t)cyl, 0);
            if (track->sector_count > 0) {
                uft_sector_t *sec = &track->sectors[track->sector_count - 1];
                sec->status = (phdr[1] != PRO_STATUS_OK)
                                  ? UFT_SECTOR_CRC_ERROR
                                  : UFT_SECTOR_DUPLICATE;
                if (phdr[1] != PRO_STATUS_OK) sec->crc_ok = false;
            }
        }
    }
    return UFT_OK;
}

/**
 * MF-880: PRO wird NICHT geschrieben — und sagt das jetzt.
 *
 * Hier stand eine Schleife, die Sektoren nach `p->file_data` kopierte
 * und `UFT_OK` meldete. `p->file_data` ist die SPEICHERKOPIE, die
 * `pro_open()` mit `uft_read_file()` angelegt hat; ein Pfad oder ein
 * `FILE*` wird nirgends aufbewahrt, `pro_close()` macht `free()`, und
 * im ganzen File steht kein `fwrite`, kein `fopen`, kein `fseek`
 * (gezaehlt: 0). Die Plugin-Struktur hat kein `.flush`.
 *
 * Jeder Schreibvorgang auf ein PRO-Abbild wurde also still verworfen,
 * und der Aufrufer bekam Erfolg gemeldet. Das ist woertlich die Klasse,
 * die MF-522 an D64/D81 behoben hat: „Eine Erfolgsmeldung ohne Tat ist
 * in einem forensischen Werkzeug schlimmer als ein Fehler."
 *
 * WARUM HIER KEIN ECHTER SCHREIBER STEHT
 *
 * Die EINFRIER-REGEL (MF-363/498) verlangt eine benannte Referenz, jede
 * Zahl gemessen, die Referenz im Header. Fuer PRO ist im Baum nichts
 * davon vorhanden:
 *
 *   - diese Datei nennt keine Quelle
 *   - im Korpus liegt kein PRO-Abbild
 *   - `PRO_MAX_TRACKS` ist hier 77 und in `uft_pro_parser_v2.c` 80 —
 *     ein unaufgeloester Widerspruch
 *   - der Kopfkommentar sagt „Tracks and SPT from header bytes 4-5",
 *     `pro_open()` liest `raw[7]` und `raw[6]`
 *
 * Einen Schreiber gegen diese Lage zu bauen waere die Wette der fuenf
 * fabrizierten Parser (ARCH-25/MF-509). Die Zusage wahr zu machen ist
 * der kleinere und richtige Schritt; die Merkmalstabelle unten sagt
 * seither dasselbe wie dieser Rumpf.
 *
 * Die Koordinatenpruefung aus MF-529 bleibt vor der Ablehnung stehen:
 * ein Aufrufer, der mit -1 kommt, hat einen anderen Fehler als einer,
 * der ein nicht schreibbares Format beschreiben will, und die beiden
 * duerfen nicht dieselbe Antwort bekommen.
 */
static uft_error_t pro_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    (void)track;
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    pro_pd_t *p = disk->plugin_data;
    if (!p || !p->file_data || head != 0) return UFT_ERROR_INVALID_STATE;

    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_pro_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    /* MF-880: stand auf SUPPORTED, waehrend write_track in eine
     * Speicherkopie schrieb, die close() verwirft. Siehe den Kommentar
     * bei pro_write_track(). */
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "PRO wird nur gelesen. Fuer einen Schreiber fehlen im Baum die "
      "benannte Referenz und ein Pruefabbild (EINFRIER-REGEL MF-498)." },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    /* MF-1054: stand auf SUPPORTED und stuetzte sich auf Flagbits
     * (`flags & 0x02`) an einer Stelle, die es im Format nicht gibt.
     * PRO traegt seinen Kopierschutz als PHANTOMSEKTOREN, nicht als
     * schwache Bits - die liest der Leser jetzt, und sie stehen als
     * zusaetzliche Sektoren derselben Nummer auf der Spur. */
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED,
      "PRO speichert keine schwachen Bits. Sein Kopierschutz sind "
      "Phantomsektoren; die werden gelesen und als Duplikate auf "
      "derselben Spur ausgegeben (MF-1054)." },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_pro = {
    .name = "PRO", .description = "Atari 8-bit Protected (APE Pro)",
    /* MF-1054: hier stand "pro;atx". ATX (VAPI) ist ein ANDERES
     * Format mit eigenem Aufbau - atari800 fuehrt es als
     * IMAGE_TYPE_VAPI getrennt neben IMAGE_TYPE_PRO, und dieser
     * Leser kann es nicht. */
    .extensions = "pro", .format = UFT_FORMAT_DSK,
    /* MF-880: UFT_FORMAT_CAP_WRITE entfernt. Es war die DRITTE Stelle,
     * die Schreiben behauptete — neben der Merkmalstabelle und dem
     * `UFT_OK` aus `pro_write_track()`. `write_track` bleibt gesetzt und
     * antwortet UFT_ERROR_NOT_SUPPORTED: ein NULL-Zeiger gaebe dem
     * Aufrufer keine Begruendung, diese Antwort schon. */
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WEAK_BITS | UFT_FORMAT_CAP_VERIFY,
    .probe = pro_plugin_probe, .open = pro_open,
    .close = pro_close, .read_track = pro_read_track,
    .write_track = pro_write_track,
    .verify_track = uft_weak_bit_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_pro_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_pro_features) / sizeof(uft_format_plugin_pro_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(pro)
