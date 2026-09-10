/**
 * @file uft_sap_plugin.c
 * @brief Thomson MO/TO SAP Format Plugin-B
 *
 * SAP (Systeme d'Archivage Pukall) ist das Standard-Abbildformat fuer
 * Thomson MO5/MO6/TO7/TO8/TO9.
 *
 * ── Referenz (EINFRIER-REGEL MF-363/498, Bedingung c) ─────────────────
 *
 * **`libsap` aus `sap2`** (Alexandre Pukall / Eric Botcazou, GPL-2),
 * geklont unter `tools/uft-scout/work/sap2/`. Das Werkzeug wurde lokal
 * **gebaut und ausgefuehrt** (Kanal *Oracle* nach MF-695); aus seinem
 * Verhalten stammen alle Zahlen unten. Aus seinem Quelltext ist der
 * Dateikopf und die Formattafel abgelesen — Fakten ueber das Format,
 * keine Umsetzung.
 *
 * **Die Pruefsumme ist NICHT aus dem Quelltext uebernommen**, sondern
 * aus dem Verhalten **abgeleitet**: `libsap` rechnet mit einer eigenen
 * Tafel (`crc_pukall`), und eine Tafel abzuschreiben waere eine
 * GPL-2-Uebernahme. Stattdessen wurde der Parametersatz an sechs
 * Sektoren einer erzeugten Datei gesucht und eindeutig gefunden (siehe
 * Befund 4). Das ist der Nachbau-Weg aus
 * `docs/QUARANTINE_PROCESS.md` §5.
 *
 * **BERICHTIGUNG der alten Referenzzeile:** hier stand „Reference:
 * DCMOTO emulator, SAP format specification by Alexandre Pukall" — ohne
 * Fundstelle, und der darunter beschriebene Kopf war erfunden. Eine
 * Referenz, die man nicht nachlesen kann, ist keine (MF-636).
 *
 * ── Der Dateikopf, wirklich ───────────────────────────────────────────
 *
 *   Byte 0      Formatbyte: **1** oder **2**
 *   Byte 1..65  "SYSTEME D'ARCHIVAGE PUKALL S.A.P. (c) Alexandre
 *                PUKALL Avril 1998"
 *   -> Kopf = 66 Byte
 *
 * Formattafel (`sap_format_table` in libsap):
 *
 *   | Formatbyte | Spuren | Sektorgroesse | Sektoren/Spur |
 *   |---|---|---|---|
 *   | 1 | 80 | 256 | 16 |
 *   | 2 | 40 | 128 | 16 |
 *
 * Je Sektor: `format(1) protection(1) track(1) sector(1)` +
 * `daten[N] ^ 0xB3` + `crc(2, MSB zuerst)`.
 *
 * ── Befund 1 (MF-1022): die Kennung war erfunden ──────────────────────
 *
 * Hier stand „Bytes 0-2: \"SAP\" magic / Byte 3: Version (0x00 = FM,
 * 0x01 = MFM) / Bytes 4-65: zero padding", und Sonde wie `open`
 * verlangten genau das. In einer echten SAP-Datei steht bei Versatz 0
 * das **Formatbyte** und dahinter die Signatur — die Zeichenfolge
 * `"SAP"` kommt bei Versatz 0 **nicht** vor.
 *
 * Gemessen an einer mit `libsap` erzeugten Datei:
 *
 *     open : -25   (UFT_ERROR_FORMAT_INVALID)
 *
 * **UFT hat damit nie eine echte SAP-Datei gelesen.** Dieselbe Klasse
 * wie MF-787 (`sad` suchte `"SAD!"`, das in keiner SAD-Datei steht) und
 * MF-961 (`86f` probte auf `"86BX"`) — und wie dort meldete die
 * Merkmalstafel „Read: SUPPORTED".
 *
 * ── Befund 2 (MF-1022): die Formatwerte gab es nicht ──────────────────
 *
 * `SAP_VERSION_FM 0x00` existiert als Formatwert **nicht**; die Tafel
 * kennt 1 und 2. Die Zuordnung „0x00 -> 128 Byte, 0x01 -> 256 Byte"
 * traf fuer Format 1 zufaellig zu und wies Format 2 (40 Spuren,
 * 128 Byte) ab. Dazu war die Spurzahl mit `SAP_TRACKS 80` fest
 * verdrahtet, was ein Format-2-Abbild auch an der Groessenpruefung
 * scheitern liess.
 *
 * ── Befund 3 (MF-1022): die Daten wurden nicht entschluesselt ─────────
 *
 * SAP verschleiert jedes Datenbyte mit **XOR 0xB3**. Der Leser gab die
 * verschleierten Bytes als Sektorinhalt aus — **jedes Byte falsch**.
 * Gemessen: in der erzeugten Datei steht das Muster `"UFT-K"` genau
 * **1280 Mal** (einmal je Sektor), aber erst nach XOR 0xB3; roh sucht
 * man es vergeblich.
 *
 * ── Befund 4 (MF-1022): die Pruefsumme war dreifach falsch ────────────
 *
 * `sap_crc16(sec_data, sector_size)` rechnete
 *   (a) mit dem falschen **Algorithmus** — CRC-16-CCITT vorwaerts
 *       (0x1021), waehrend SAP eine gespiegelte Variante nutzt,
 *   (b) ueber die **verschleierten** Bytes,
 *   (c) ohne die vier **Kopfbytes**.
 *
 * Gemessen an drei Sektoren: gespeichert `A33C / 8AEB / 6F3F`, mit
 * UFTs Rechnung `AE47 / D9EC / 51CC` — keine Uebereinstimmung. Die
 * Pruefung konnte also nie zustimmen, und ihr Urteil landete
 * ungeprueft in `crc_ok`.
 *
 * **Der richtige Parametersatz, aus dem Verhalten abgeleitet** (sechs
 * Sektoren, eindeutiger Treffer bei einer Suche ueber 21 Polynome x
 * 8 Startwerte x Spiegelung x Abschluss-XOR x 5 Spannen):
 *
 *     Spanne   4 Kopfbytes + ENTSCHLUESSELTE Daten
 *     Polynom  0x8408   (gespiegeltes CCITT)
 *     Start    0xFFFF
 *     refin    ja       refout nein     xorout 0x0000
 *
 * ── Befund 5 (MF-1022): der Fuellsektor galt als guter Sektor ─────────
 *
 * Fehlte ein Sektorsatz, legte der Leser einen 0xE5-Sektor an — ohne
 * `uft_format_mark_last_missing()`. `uft_format_add_sector()` setzt
 * `UFT_SECTOR_OK` und beide CRC-Flags auf „gut" (siehe seinen eigenen
 * Kopf), also war die Fuellung von echten Daten nicht zu
 * unterscheiden. Das ist die Klasse von **MF-980**.
 *
 * **Und Tor 62 konnte das nicht sehen:** es sucht
 * `memset(X->data, 0xE5, ...)` ohne Kennzeichnung in der Naehe, aber
 * hier stand das `memset` **vor** der Schleife (ein
 * wiederverwendeter Puffer). Die Messgrenze steht im Kopf des Tores;
 * dieser Fall ist der erste belegte Durchfall (P3-328).
 */

#include "uft/uft_format_common.h"

#define SAP_HEADER_SIZE   66
#define SAP_SIG_OFF       1
#define SAP_SIG           "SYSTEME D'ARCHIVAGE PUKALL S.A.P."
#define SAP_SIG_LEN       32      /* der unterscheidende Anfang */
#define SAP_SIDES         1
#define SAP_SPT           16
#define SAP_SEC_HDR       4       /* format + protection + track + sector */
#define SAP_CRC_SIZE      2
#define SAP_XOR           0xB3    /* libsap SAP_MAGIC_NUM */
#define SAP_FORMAT1       1
#define SAP_FORMAT2       2
#define SAP_MAX_SECSIZE   256

/* Die Formattafel des Urhebers. Formatbyte 1 und 2 — 0 gibt es nicht. */
static bool sap_geometry(uint8_t format, uint8_t *tracks,
                         uint16_t *sector_size)
{
    switch (format) {
    case SAP_FORMAT1: *tracks = 80; *sector_size = 256; return true;
    case SAP_FORMAT2: *tracks = 40; *sector_size = 128; return true;
    default:          return false;
    }
}

/* Die Pruefsumme, aus dem Verhalten abgeleitet (Befund 4):
 * gespiegeltes CCITT-Polynom 0x8408, Start 0xFFFF, kein Abschluss-XOR.
 * Sie laeuft ueber die vier Kopfbytes UND die entschluesselten Daten. */
static uint16_t sap_pukall_crc(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0x8408)
                            : (uint16_t)(crc >> 1);
    }
    return crc;
}

typedef struct {
    uint8_t *data;        /* Entire file in memory */
    size_t   data_size;
    uint8_t  format;      /* 1 oder 2 */
    uint8_t  tracks;      /* 80 oder 40 */
    uint16_t sector_size; /* 256 oder 128 */
} sap_pd_t;

/* Die EINE Stelle, an der ein Sektor entschluesselt und seine
 * Pruefsumme gerechnet wird.
 *
 * MF-1022: das stand zuerst zweimal da — einmal in `read_track`, einmal
 * im Zugang `uft_sap_pruefsumme()`. Die Mutationsprobe „CRC ohne die
 * Kopfbytes" fiel deshalb NICHT: sie traf nur die eine Kopie, und die
 * Zusicherung prueft die andere. Zwei Umsetzungen derselben Rechnung
 * sind eine Zusicherung, die nur die Haelfte bewacht — dieselbe
 * Gestalt wie die sechsfache GCR-Tafel aus dem Architektur-Pass.
 *
 * `klar` darf NULL sein, wenn nur die Pruefsummen gebraucht werden.
 * Rueckgabe: true, wenn der Satz vollstaendig in der Datei liegt. */
static bool sap_sektor_lesen(const sap_pd_t *p, size_t sec_offset,
                             uint8_t *klar, uint8_t *sec_num,
                             uint16_t *soll, uint16_t *ist)
{
    size_t rec = SAP_SEC_HDR + p->sector_size + SAP_CRC_SIZE;
    if (sec_offset + rec > p->data_size) return false;

    const uint8_t *hdr = p->data + sec_offset;
    const uint8_t *roh = hdr + SAP_SEC_HDR;

    /* Befund 3: SAP verschleiert jedes Datenbyte mit XOR 0xB3. */
    uint8_t eingabe[SAP_SEC_HDR + SAP_MAX_SECSIZE];
    memcpy(eingabe, hdr, SAP_SEC_HDR);
    for (uint16_t i = 0; i < p->sector_size; i++) {
        uint8_t b = (uint8_t)(roh[i] ^ SAP_XOR);
        eingabe[SAP_SEC_HDR + i] = b;
        if (klar) klar[i] = b;
    }

    /* Befund 4: die Pruefsumme laeuft ueber die vier Kopfbytes UND die
     * entschluesselten Daten. */
    if (ist) *ist = sap_pukall_crc(eingabe, SAP_SEC_HDR + p->sector_size);
    if (soll) {
        const uint8_t *cb = roh + p->sector_size;
        *soll = (uint16_t)(((uint16_t)cb[0] << 8) | cb[1]);
    }
    if (sec_num) *sec_num = hdr[3];
    return true;
}

/* Traegt die Datei die Signatur des Formats? */
static bool sap_has_signature(const uint8_t *d)
{
    return memcmp(d + SAP_SIG_OFF, SAP_SIG, SAP_SIG_LEN) == 0;
}

static bool sap_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence)
{
    if (!data || size < SAP_HEADER_SIZE) return false;

    /* MF-1022, Befund 1: die Signatur steht bei Versatz 1, nicht 0. */
    if (!sap_has_signature(data)) return false;

    uint8_t tracks; uint16_t sec_size;
    if (!sap_geometry(data[0], &tracks, &sec_size)) return false;

    /* Die Groesse folgt aus der Formattafel und ist damit pruefbar. */
    size_t erwartet = SAP_HEADER_SIZE + (size_t)tracks * SAP_SPT
                    * (SAP_SEC_HDR + sec_size + SAP_CRC_SIZE);
    if (file_size && file_size < erwartet) return false;

    *confidence = 95;
    return true;
}

static uft_error_t sap_plugin_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *fdata = uft_read_file(path, &file_size);
    if (!fdata || file_size < SAP_HEADER_SIZE) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    if (!sap_has_signature(fdata)) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t tracks; uint16_t sec_size;
    if (!sap_geometry(fdata[0], &tracks, &sec_size)) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    size_t sector_record = SAP_SEC_HDR + sec_size + SAP_CRC_SIZE;
    size_t expected = SAP_HEADER_SIZE
                    + (size_t)tracks * SAP_SPT * sector_record;
    if (file_size < expected) {
        free(fdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    sap_pd_t *p = calloc(1, sizeof(sap_pd_t));
    if (!p) { free(fdata); return UFT_ERROR_NO_MEMORY; }
    p->data = fdata;
    p->data_size = file_size;
    p->format = fdata[0];
    p->tracks = tracks;
    p->sector_size = sec_size;

    disk->plugin_data = p;
    disk->geometry.cylinders = tracks;
    disk->geometry.heads = SAP_SIDES;
    disk->geometry.sectors = SAP_SPT;
    disk->geometry.sector_size = sec_size;
    disk->geometry.total_sectors = (uint32_t)tracks * SAP_SPT;
    return UFT_OK;
}

static void sap_plugin_close(uft_disk_t *disk)
{
    sap_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t sap_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track)
{
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    sap_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (head != 0) return UFT_ERROR_INVALID_PARAM;
    if (cyl >= p->tracks) return UFT_ERROR_INVALID_PARAM;

    uft_track_init(track, cyl, head);

    size_t sector_record = SAP_SEC_HDR + p->sector_size + SAP_CRC_SIZE;
    size_t track_offset = SAP_HEADER_SIZE
                        + (size_t)cyl * SAP_SPT * sector_record;

    for (int s = 0; s < SAP_SPT; s++) {
        size_t sec_offset = track_offset + (size_t)s * sector_record;

        if (sec_offset + sector_record > p->data_size) {
            /* MF-1022, Befund 5: der Fuellsektor wird JETZT als fehlend
             * gekennzeichnet. Vorher galt er als guter Sektor mit
             * gueltiger CRC — von echten Daten nicht zu unterscheiden
             * (Klasse MF-980). */
            uint8_t fill_buf[SAP_MAX_SECSIZE];
            memset(fill_buf, 0xE5, sizeof(fill_buf));
            uft_format_add_sector(track, (uint8_t)s, fill_buf,
                                  p->sector_size, (uint8_t)cyl, 0);
            uft_format_mark_last_missing(track);
            continue;
        }

        /* Eine Rechnung, ein Ort — siehe `sap_sektor_lesen()`. */
        uint8_t klar[SAP_MAX_SECSIZE];
        uint8_t sec_num = 0;
        uint16_t stored_crc = 0, calc_crc = 0;
        (void)sap_sektor_lesen(p, sec_offset, klar, &sec_num,
                               &stored_crc, &calc_crc);

        /* Die Sektornummer steht im Satzkopf und ist 1-basiert.
         * `uft_format_add_sector()` addiert 1, deshalb wird hier 1
         * abgezogen — netto also die Nummer aus der Datei. Das war
         * schon vorher richtig und bleibt. */
        uint8_t sec_idx = (sec_num > 0) ? (uint8_t)(sec_num - 1)
                                        : (uint8_t)s;

        uft_format_add_sector(track, sec_idx, klar, p->sector_size,
                              (uint8_t)cyl, 0);

        if (track->sector_count > 0) {
            uft_sector_t *sek = &track->sectors[track->sector_count - 1];
            bool gut = (stored_crc == calc_crc);
            sek->crc_ok = gut;
            if (!gut) uft_sector_set_crc(sek, false);
        }
    }
    return UFT_OK;
}

/* Gibt die Pruefsumme eines Sektors heraus — fuer den Beweis, dass die
 * Zahl aus der DATEI kommt und nicht aus unserem Code (MF-869/1013).
 * 0 = stimmt, 1 = stimmt nicht, -1 = nicht zu beantworten. */
int uft_sap_pruefsumme(const uft_disk_t *disk, int cyl, int sektor,
                       uint16_t *soll, uint16_t *ist)
{
    if (!disk) return -1;
    const sap_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return -1;
    if (cyl < 0 || cyl >= p->tracks) return -1;
    if (sektor < 0 || sektor >= SAP_SPT) return -1;

    size_t rec = SAP_SEC_HDR + p->sector_size + SAP_CRC_SIZE;
    size_t off = SAP_HEADER_SIZE + (size_t)cyl * SAP_SPT * rec
               + (size_t)sektor * rec;
    if (off + rec > p->data_size) return -1;

    uint16_t gespeichert = 0, berechnet = 0;
    if (!sap_sektor_lesen(p, off, NULL, NULL, &gespeichert, &berechnet))
        return -1;

    if (soll) *soll = gespeichert;
    if (ist)  *ist  = berechnet;
    return (gespeichert == berechnet) ? 0 : 1;
}

static uft_error_t sap_plugin_write_track(uft_disk_t *disk, int cyl, int head,
                                            const uft_track_t *track) {
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    sap_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-883: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Gemessen: in dieser Datei steht keine einzige Schreiboperation
     * (`fwrite`/`fputc`/`fprintf`/`ftruncate`/`WriteFile`), das Plugin hat
     * kein `.flush`, und `close()` gibt den Puffer frei. Kein Byte hat je
     * die Platte erreicht — der Aufrufer bekam Erfolg gemeldet.
     *
     * Und es gibt auch keinen allgemeinen Rueckweg: `plugin->flush` wird im
     * ganzen Baum von NIEMANDEM gerufen (gemessen ueber `git ls-files`,
     * kommentarfrei), `uft_disk_close()` ruft nur `close`. Selbst ein
     * Plugin MIT Flush kaeme nicht durch.
     *
     * MF-1022 macht die Absage inhaltlich klarer: ein SAP-Schreiber
     * muesste die Daten mit 0xB3 verschleiern UND die Pukall-Pruefsumme
     * ueber Kopf plus Klardaten neu bilden. Beides steht jetzt in dieser
     * Datei (`SAP_XOR`, `sap_pukall_crc`) und ist an einem Erzeugnis
     * fremder Hand abgenommen — der Schreiber ist damit vorbereitet,
     * aber nicht gebaut. Das ist Arbeit, kein Fehler (P3-204).
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_sap_thomson_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883/1022: schreibt nur in den Speicher — kein fwrite in der Datei, "
      "kein flush, close() gibt frei. Verschleierung und Pruefsumme liegen "
      "seit MF-1022 vor, der Schreiber ist vorbereitet (P3-204)" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_sap_thomson = {
    .name = "SAP",
    .description = "Thomson MO/TO SAP",
    .extensions = "sap",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = sap_plugin_probe,
    .open = sap_plugin_open,
    .close = sap_plugin_close,
    .read_track = sap_plugin_read_track,
    .write_track = sap_plugin_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* MF-1022: Kopf und Formattafel
                                             * aus libsap des Urhebers, die
                                             * Pruefsumme aus dem Verhalten
                                             * abgeleitet (vorher
                                             * REVERSE_ENGINEERED) */
    .features = uft_format_plugin_sap_thomson_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_sap_thomson_features) / sizeof(uft_format_plugin_sap_thomson_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(sap_thomson)
