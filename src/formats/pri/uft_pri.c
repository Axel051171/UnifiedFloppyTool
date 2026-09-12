/**
 * @file uft_pri.c
 * @brief PRI (PCE Raw Image) — Chunk-Behaelter fuer rohe Bitstroeme
 *
 * ── Der Aufbau, nach der Beschreibung des Urhebers ──────────────────
 *
 * Alle Zahlen big-endian. Die Datei ist eine Folge von Chunks:
 *
 *     Versatz  Groesse  Inhalt
 *     0        4        Chunk-Kennung
 *     4        4        Groesse der Daten (n)
 *     8        n        Daten
 *     8+n      4        CRC
 *
 * Die Groesse zaehlt **weder** die Kennung **noch** das Groessenfeld
 * **noch** die CRC. Die CRC deckt Kennung + Groessenfeld + Daten.
 *
 *     "PRI "  Dateikopf: Groesse 4, Daten = Version(2) + reserviert(2)
 *     "TEXT"  Kommentar (UTF-8, Zeilen mit LF); mehrere werden verkettet
 *     "TRAK"  Spurkopf, Groesse 16: Zylinder, Kopf, **Spurlaenge in
 *             Bits**, **Bittakt** — in dieser Reihenfolge
 *     "DATA"  Spurdaten, MSB zuerst; **darf kuerzer sein** als die
 *             Spurlaenge sagt, der Rest gilt dann als 0
 *     "WEAK"  Schwachbit-Maske, je Paar Bitversatz + Maske
 *     "BCLK"  abweichender Bittakt, je Paar Bitversatz + neuer Takt
 *     "END "  Ende; alles danach wird ignoriert
 *
 * Unbekannte Chunks werden uebersprungen — so sagt es die Beschreibung.
 *
 * CRC: big-endian CRC-32, Generatorpolynom **0x1EDC6F41**, Startwert 0,
 * ohne Spiegelung und ohne Schluss-XOR.
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * Der Spezifikationstext „PRI File Format (2020-03-26)" des **Urhebers**
 * (Hampa Hug, PCE). Er liegt im Baum als **Dokumentation** im Kopf von
 * `tools/uft-scout/work/HxCFloppyEmulator/libhxcfe/sources/loaders/
 * pri_loader/pri_format.h`. Kanal *Spec* nach MF-695: gelesen wird die
 * Beschreibung, **keine Zeile Code**. Die CRC ist aus der dort
 * angegebenen Parameterbeschreibung neu geschrieben — und an der Zahl
 * geeicht, die die Beschreibung selbst nennt: die CRC des leeren
 * `"END "`-Chunks muss **0x3D64AF78** sein. Sie trifft.
 *
 * Zweite Hand, nur **ausgefuehrt**: hxcfes `PRI`-Modul liest die
 * Pruefdateien (5 Spuren, 2 Seiten).
 *
 * ── MF-1036: acht Befunde, und der erste war toedlich ───────────────
 *
 * **S1 — die Chunk-Felder waren vertauscht.** Der alte Leser nahm
 * `Groesse = be32(pos)` und die Kennung bei `pos+4`; richtig ist
 * Kennung bei `pos`, Groesse bei `pos+4`. Damit las er die **Kennung
 * als Groesse**: bei einem `TEXT`-Chunk ergibt das 0x54455854, also
 * ueber 1,4 Milliarden — die Schranke griff, die Schleife brach beim
 * **ersten** Chunk ab, und `pri_scan()` lieferte null Spuren.
 *
 * **S2 — die CRC lag im falschen Feld.** Der alte Kopfkommentar sagte
 * „size(BE32) + id(4 bytes) + crc(BE32) + payload", also CRC **vor**
 * den Daten. Sie steht **dahinter**, und sie wurde nie geprueft.
 *
 * **S3 — die Groesse wurde falsch verrechnet.** Der alte Code rechnete
 * `payload_size = chunk_size - 12` und `pos += chunk_size`; die
 * Beschreibung sagt ausdruecklich, die Groesse zaehle die drei
 * Rahmenfelder **nicht**. Richtig ist `pos += 8 + n + 4`.
 *
 * **S4 — der Dateikopf ist 16 Byte, nicht 12.** `"PRI "` + Groesse(4) +
 * Daten(4) + CRC(4). Der alte Leser nahm 12 an und begann die
 * Chunk-Schleife damit **in der CRC des Dateikopfs**. Gemessen an einer
 * spezifikationsgerechten Datei: die ersten 16 Byte sind
 * `50 52 49 20 | 00 00 00 04 | 00 00 00 00 | 38 D2 C9 9D`, und
 * `be32(+12) = 0x38D2C99D` — der alte Leser hielt das fuer eine
 * Chunk-Groesse von 953 MB.
 *
 * **S5 — die Version wurde aus dem Groessenfeld gelesen.** `version =
 * be32(data+4)` ergab immer **4**. Die Version ist ein **16-Bit**-Wert
 * bei Versatz 8, gefolgt von zwei reservierten Byte.
 *
 * **S6 — die TRAK-Felder waren vertauscht.** Der alte Kommentar sagte
 * „cylinder, head, bit_rate, bit_count" und las die Bitzahl bei +12;
 * richtig ist **Spurlaenge bei +8** und **Bittakt bei +12**. Er nahm
 * also den Bittakt fuer die Bitzahl.
 *
 * **S7 — die Sonde verwarf die Dateigroesse** (`(void)file_size`) und
 * meldete **Konfidenz 95** auf vier Byte Kennung allein. Gemessen: sie
 * stimmte jeder spezifikationsgerechten Datei zu, und `open` antwortete
 * danach **-25**. Das ist die Gestalt von MF-961 und MF-1022 — eine
 * zuversichtliche Sonde vor einem Leser, der nichts kann. Und es ist die
 * `(void)file_size`-Falle aus MF-1029, zum **sechsten** Mal.
 *
 * **S8 — die Beschreibung der Schreibseite war ebenfalls erfunden.** Der
 * Kommentar an `pri_write_track()` nannte einen „FXMD chunk" mit
 * „per-bit flux-modulation flags", einen „DONE chunk" und einen
 * „4-byte header {cyl,head,size,_}". Keines davon gibt es: die Chunks
 * heissen TEXT/TRAK/DATA/WEAK/BCLK/END, und TRAK ist **16** Byte.
 *
 * Und am Rand: die Kopfzeile nannte PRI „MAME/MESS flux preservation".
 * PRI ist das Format von **PCE** (Hampa Hug); MAME hat damit nichts zu
 * tun. Berichtigt.
 *
 * ── Was NICHT geaendert wurde ───────────────────────────────────────
 *
 * `WEAK` und `BCLK` werden **erkannt und gezaehlt**, aber nicht
 * angewandt — ein Schwachbit-Muster auf einen Bitstrom zu legen, ohne
 * ein Abbild zu haben, an dem man es prueft, waere geraten. Die
 * Merkmalstafel sagt das; die Zahlen stehen in `pri_data_t` und werden
 * vom Test gelesen.
 *
 * Die Schreibseite bleibt abgesagt (die Begruendung steht dort), aber
 * ohne die erfundene Chunk-Liste.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Konstanten
 * ========================================================================== */

#define PRI_MAGIC           "PRI "
#define PRI_CHUNK_RAHMEN    12          /* Kennung(4) + Groesse(4) + CRC(4) */
#define PRI_KOPF_DATEN      4           /* Version(2) + reserviert(2) */
#define PRI_CRC_POLY        0x1EDC6F41u
#define PRI_MAX_TRACKS      168
#define PRI_MAX_TRACK_BYTES (1024u * 1024u)  /* Schranke je Spur */
#define PRI_END_CRC         0x3D64AF78u /* CRC des leeren END-Chunks */

/* ============================================================================
 * Helfer
 * ========================================================================== */

static uint32_t pri_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t pri_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/**
 * @brief Die CRC der Beschreibung: CRC-32, Polynom 0x1EDC6F41, Start 0.
 *
 * Aus der Parameterangabe neu geschrieben, nicht uebernommen. Geeicht an
 * der Zahl, die die Beschreibung selbst nennt (`PRI_END_CRC`).
 */
uint32_t uft_pri_crc(const uint8_t *b, size_t n) {
    uint32_t crc = 0;
    size_t i;
    int j;
    for (i = 0; i < n; i++) {
        crc ^= (uint32_t)(b[i] & 0xFF) << 24;
        for (j = 0; j < 8; j++) {
            crc = (crc & 0x80000000u) ? ((crc << 1) ^ PRI_CRC_POLY)
                                      : (crc << 1);
        }
    }
    return crc;
}

/* ============================================================================
 * Plugin-Daten
 * ========================================================================== */

typedef struct {
    uint32_t    data_offset;    /* Versatz der DATA-Nutzlast in der Datei */
    uint32_t    data_size;      /* Groesse der DATA-Nutzlast */
    uint32_t    bit_count;      /* Spurlaenge in Bits (TRAK +8) */
    uint32_t    bit_clock;      /* Bittakt (TRAK +12) */
    uint32_t    cylinder;
    uint32_t    head;
    uint32_t    weak_paare;     /* erkannt, NICHT angewandt */
    uint32_t    bclk_paare;     /* erkannt, NICHT angewandt */
    bool        valid;
} pri_track_info_t;

typedef struct {
    uint8_t*            file_data;
    size_t              file_size;
    uint16_t            version;
    pri_track_info_t    tracks[PRI_MAX_TRACKS];
    uint16_t            track_count;
    uint32_t            max_cyl;
    uint32_t            max_head;
    uint32_t            crc_fehler;     /* Chunks mit falscher CRC */
    uint32_t            unbekannte;     /* uebersprungene Chunks */
} pri_data_t;

/* ============================================================================
 * Dateikopf
 * ========================================================================== */

/**
 * @brief Den Dateikopf-Chunk pruefen.
 *
 * `"PRI "` + Groesse **4** + Version(2) + reserviert(2) + CRC. Die CRC
 * deckt Kennung, Groessenfeld und Daten.
 *
 * @param version  darf NULL sein
 * @return 1 wenn der Kopf stimmt und seine CRC aufgeht
 */
int uft_pri_header_ok(const uint8_t *data, size_t size, uint16_t *version)
{
    uint32_t n, crc_soll;
    if (!data || size < PRI_CHUNK_RAHMEN + PRI_KOPF_DATEN) return 0;
    if (memcmp(data, PRI_MAGIC, 4) != 0) return 0;
    n = pri_be32(data + 4);
    if (n != PRI_KOPF_DATEN) return 0;
    crc_soll = pri_be32(data + 8 + n);
    if (uft_pri_crc(data, 8 + n) != crc_soll) return 0;
    if (version) *version = pri_be16(data + 8);
    return 1;
}

/* ============================================================================
 * Chunk-Durchlauf
 * ========================================================================== */

static bool pri_scan(const uint8_t *data, size_t size, pri_data_t *pdata)
{
    size_t pos;
    int offen = -1;     /* Index der zuletzt eroeffneten Spur, -1 = keine */

    if (!uft_pri_header_ok(data, size, &pdata->version)) return false;
    /* Die Beschreibung kennt Version 0. Eine andere koennte die
     * Bedeutung der Chunks aendern; geraten wird nicht. */
    if (pdata->version != 0) return false;

    pos = 8 + PRI_KOPF_DATEN + 4;   /* hinter den Dateikopf-Chunk */

    while (pos + 8 <= size) {
        const uint8_t *id = data + pos;
        uint32_t n = pri_be32(data + pos + 4);
        size_t nutz = pos + 8;
        uint32_t crc_soll;

        /* Rahmen muss vollstaendig in der Datei liegen. */
        if ((uint64_t)nutz + n + 4 > (uint64_t)size) break;
        crc_soll = pri_be32(data + nutz + n);
        if (uft_pri_crc(data + pos, 8 + n) != crc_soll) {
            pdata->crc_fehler++;
            return false;   /* eine falsche CRC ist kein Nebenumstand */
        }

        if (memcmp(id, "END ", 4) == 0) {
            /* Alles nach END wird ignoriert — so sagt es die Beschreibung. */
            break;
        } else if (memcmp(id, "TRAK", 4) == 0) {
            if (n != 16) return false;
            if (pdata->track_count >= PRI_MAX_TRACKS) return false;
            {
                pri_track_info_t *t = &pdata->tracks[pdata->track_count];
                memset(t, 0, sizeof(*t));
                t->cylinder  = pri_be32(data + nutz);
                t->head      = pri_be32(data + nutz + 4);
                t->bit_count = pri_be32(data + nutz + 8);
                t->bit_clock = pri_be32(data + nutz + 12);
                /* Schranken: eine Spur mit 1 MB Bitdaten ist bereits
                 * jenseits jeder Diskette (MF-543). */
                if (t->bit_count == 0
                    || t->bit_count > PRI_MAX_TRACK_BYTES * 8u) return false;
                if (t->cylinder > 255u || t->head > 3u) return false;
                t->valid = true;
                offen = (int)pdata->track_count;
                pdata->track_count++;
                if (t->cylinder > pdata->max_cyl) pdata->max_cyl = t->cylinder;
                if (t->head > pdata->max_head) pdata->max_head = t->head;
            }
        } else if (memcmp(id, "DATA", 4) == 0) {
            if (offen < 0) return false;    /* DATA ohne TRAK davor */
            {
                pri_track_info_t *t = &pdata->tracks[offen];
                /* Die Beschreibung erlaubt eine KUERZERE DATA als die
                 * Spurlaenge; der Rest gilt als 0. Laenger als die
                 * Spurlaenge darf sie nicht sein. */
                if ((uint64_t)n * 8u > (uint64_t)t->bit_count + 7u)
                    return false;
                t->data_offset = (uint32_t)nutz;
                t->data_size = n;
            }
        } else if (memcmp(id, "WEAK", 4) == 0) {
            if ((n % 8u) != 0) return false;
            if (offen >= 0) pdata->tracks[offen].weak_paare = n / 8u;
        } else if (memcmp(id, "BCLK", 4) == 0) {
            if ((n % 8u) != 0) return false;
            if (offen >= 0) pdata->tracks[offen].bclk_paare = n / 8u;
        } else if (memcmp(id, "TEXT", 4) == 0) {
            /* Kommentar; nichts zu tun. */
        } else {
            /* „Unknown chunks should be skipped." */
            pdata->unbekannte++;
        }

        pos = nutz + n + 4;
    }

    return pdata->track_count > 0;
}

/* ============================================================================
 * Sonde
 * ========================================================================== */

bool pri_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    uint16_t v = 0;
    if (!data || size < PRI_CHUNK_RAHMEN + PRI_KOPF_DATEN) return false;
    if (memcmp(data, PRI_MAGIC, 4) != 0) return false;

    /* MF-1036/S7: die Kennung allein ist kein Merkmal — vier Byte
     * treffen zu leicht. Der Dateikopf traegt seine eigene CRC, und die
     * ist der Beleg AM OBJEKT. */
    if (!uft_pri_header_ok(data, size, &v) || v != 0) {
        if (confidence) *confidence = 40;   /* nur die Kennung (MF-729) */
        return false;
    }

    /* Und die Datei muss gross genug sein, um ueberhaupt einen Chunk
     * hinter dem Kopf zu tragen. `file_size` wird dafuer gebraucht —
     * vorher stand hier `(void)file_size` (MF-1029). */
    if (file_size < (size_t)(8 + PRI_KOPF_DATEN + 4) + PRI_CHUNK_RAHMEN) {
        if (confidence) *confidence = 40;
        return false;
    }

    if (confidence) *confidence = 95;
    return true;
}

/* ============================================================================
 * open / close
 * ========================================================================== */

static uft_error_t pri_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    size_t file_size = 0;
    uint8_t *file_data;
    pri_data_t *pdata;

    (void)read_only;

    file_data = uft_read_file(path, &file_size);
    if (!file_data) return UFT_ERROR_FILE_OPEN;

    pdata = calloc(1, sizeof(pri_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    pdata->file_data = file_data;
    pdata->file_size = file_size;

    if (!pri_scan(file_data, file_size, pdata)) {
        free(file_data);
        free(pdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->max_cyl + 1;
    disk->geometry.heads = pdata->max_head + 1;
    disk->geometry.sectors = 1;     /* Fluss: eine Spur ist eine Einheit */
    disk->geometry.sector_size = 0;
    disk->geometry.total_sectors = pdata->track_count;

    return UFT_OK;
}

static void pri_close(uft_disk_t *disk)
{
    pri_data_t *pdata = disk->plugin_data;
    if (pdata) {
        free(pdata->file_data);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ========================================================================== */

/**
 * Eine PRI-Spur ist ein **Bitstrom**, kein Sektorsatz. Sie wird als eine
 * Einheit uebergeben, und die Laenge ist die **Spurlaenge aus dem
 * TRAK-Chunk**, nicht die DATA-Groesse: die Beschreibung erlaubt eine
 * kuerzere DATA, deren Rest als 0 gilt. Vorher wurde die DATA-Groesse
 * ausgegeben und still auf 65535 gedeckelt.
 */
static uft_error_t pri_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    pri_data_t *pdata;
    int i;

    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    pdata = disk->plugin_data;
    if (!pdata || !track) return UFT_ERROR_INVALID_STATE;

    for (i = 0; i < (int)pdata->track_count; i++) {
        pri_track_info_t *t = &pdata->tracks[i];
        size_t bytes;
        uint8_t *puffer;

        if (!t->valid || (int)t->cylinder != cyl || (int)t->head != head)
            continue;

        bytes = ((size_t)t->bit_count + 7u) / 8u;
        if (bytes == 0 || bytes > PRI_MAX_TRACK_BYTES)
            return UFT_ERROR_FORMAT_INVALID;
        if ((size_t)t->data_offset + t->data_size > pdata->file_size)
            return UFT_ERROR_IO;

        puffer = calloc(1, bytes);
        if (!puffer) return UFT_ERROR_NO_MEMORY;
        /* Die kuerzere DATA wird mit Nullen aufgefuellt — so sagt es die
         * Beschreibung, und `calloc` hat das schon getan. */
        memcpy(puffer, pdata->file_data + t->data_offset,
               (t->data_size < bytes) ? t->data_size : bytes);

        uft_track_init(track, cyl, head);
        uft_format_add_sector(track, 0, puffer, bytes,
                              (uint8_t)cyl, (uint8_t)head);
        free(puffer);
        return UFT_OK;
    }

    return UFT_ERROR_INVALID_STATE;
}

/* ============================================================================
 * write_track
 * ========================================================================== */

/**
 * Abgesagt, und die Begruendung ist jetzt richtig.
 *
 * **BERICHTIGT MF-1036.** Hier stand eine Anleitung mit einem „FXMD
 * chunk" fuer „per-bit flux-modulation flags", einem „DONE chunk" und
 * einem „4-byte header {cyl,head,size,_}". **Keines davon gibt es.** Die
 * Chunks heissen TEXT/TRAK/DATA/WEAK/BCLK/END, und TRAK ist 16 Byte
 * gross. Eine Anleitung, die erfundene Strukturen nennt, ist schlimmer
 * als keine: sie sieht aus wie Wissen.
 *
 * Was wirklich fehlt, um eine PRI zu schreiben:
 *
 *   1. ein Bitstrom je Spur. Fuer IBM-MFM gibt es ihn
 *      (`src/core/uft_mfm_encoder.c`, MF-938); fuer AmigaDOS und GCR
 *      nicht.
 *   2. TRAK mit Zylinder, Kopf, **Spurlaenge in Bits** und Bittakt,
 *      dann DATA mit dem Strom (MSB zuerst),
 *   3. je Chunk die CRC ueber Kennung + Groessenfeld + Daten
 *      (`uft_pri_crc()`, in diesem Baum vorhanden und geeicht),
 *   4. ein `"END "`-Chunk; seine CRC ist **0x3D64AF78**.
 *
 * Der Grund fuer die Absage ist damit **nicht** mehr „unbekannter
 * Aufbau", sondern: es gibt kein von fremder Hand erzeugtes PRI im
 * Korpus, gegen das ein Rundlauf zu pruefen waere, und die
 * EINFRIER-REGEL (MF-363/498) verlangt genau das. Verzeichnet als
 * P3-204-Nachbar; siehe den Commit zu MF-1036.
 */
static uft_error_t pri_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    (void)disk; (void)cyl; (void)head; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Plugin
 * ========================================================================== */

static const uft_plugin_feature_t uft_format_plugin_pri_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-1036: der Aufbau ist jetzt belegt, aber im Korpus liegt kein "
      "fremd erzeugtes PRI, gegen das ein Rundlauf zu pruefen waere "
      "(EINFRIER-REGEL MF-363/498)" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_SUPPORTED, NULL },
    { "Timing", UFT_FEATURE_PARTIAL,
      "MF-1036: der Bittakt je Spur wird gelesen (TRAK +12); ein "
      "abweichender Takt aus einem BCLK-Chunk wird ERKANNT und gezaehlt, "
      "aber nicht angewandt — ohne ein Abbild, an dem man es prueft, "
      "waere das geraten" },
    { "Weak Bits", UFT_FEATURE_PARTIAL,
      "MF-1036: WEAK-Chunks werden ERKANNT und gezaehlt, aber nicht auf "
      "den Bitstrom angewandt — siehe Timing" },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_pri = {
    .name         = "PRI",
    .description  = "PCE Raw Image (Hampa Hug) — Bitstrom-Behaelter",
    .extensions   = "pri",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe        = pri_probe,
    .open         = pri_open,
    .close        = pri_close,
    .read_track   = pri_read_track,
    .write_track  = pri_write_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* MF-1036: die Beschreibung des Urhebers, vollstaendig */
    .features = uft_format_plugin_pri_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_pri_features) / sizeof(uft_format_plugin_pri_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(pri)
