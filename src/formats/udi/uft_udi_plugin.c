/**
 * @file uft_udi_plugin.c
 * @brief UDI (Ultra Disk Image) Plugin-B — ZX Spectrum, MFM/FM je Byte
 *        mit Taktmarken
 *
 * ── Referenz (EINFRIER-REGEL MF-363/498, Bedingung c) ─────────────────
 *
 * **Alex Makeev, „UDI file format Version 1.0 final", 24.03.2002**,
 * wiedergegeben im Sinclair Wiki unter „UDI format"
 * (https://sinclair.wiki.zxnet.co.uk/wiki/UDI_format), abgerufen
 * 2026-09-10. Der Urheber gibt den Dateikopf, den Spursatz, die
 * **Typtafel** und den **Referenzcode der Pruefsumme** woertlich an.
 * Alles unten Gemessene steht gegen diese Quelle, nicht gegen einen
 * fremden Quelltext.
 *
 * **BERICHTIGUNG MF-1015 einer Attribution.** Hier stand: „Reference:
 * UDI specification by Simon Owen". Simon Owen ist der Autor von
 * **SAMdisk**, nicht von UDI. Eine Attribution ist eine rechtliche
 * Aussage (MF-636) — und diese war zugleich der Grund, warum der Kopf
 * darunter falsch beschrieben war. Der zweite Verweis („libdsk
 * 1.5.12 geprueft") bleibt stehen, ist aber **nicht nachgemessen**.
 *
 * ── Befund 1: der Kopf war um vier Byte verschoben ────────────────────
 *
 * Wirklich, nach der Spezifikation:
 *
 *     0..3   "UDI!"
 *     4..7   Dateigroesse minus 4        <-- dieses Feld fehlte hier
 *     8      Version (0x00 = 1.0)
 *     9      hoechste Zylindernummer (Zylinder = Wert + 1)
 *    10      hoechste Kopfnummer     (Koepfe   = Wert + 1)
 *    11      unbenutzt (0)
 *   12..15   Groesse des erweiterten Kopfes (EXTHDL, LE32)
 *
 * Das Kopfkommentar hier beschrieb einen Kopf **ohne** das
 * Groessenfeld und war ab Byte 4 um vier Byte verschoben (Version bei
 * 4, Zylinder bei 5, Kopf bei 6). `udi_open()` las genau so:
 * `data[5]` und `data[6]`.
 *
 * Das sind die Bytes 1 und 2 der little-endian **Dateigroesse**.
 * Gerechnet fuer realistische Dateien:
 *
 *   | Datei                                 | data[5] | data[6] | Folge |
 *   |---|---|---|---|
 *   | TR-DOS 80x2, 6250 B/Spur (1125620 B)  |  44 | 17 | **abgewiesen** |
 *   | TR-DOS 40x1, 6250 B/Spur ( 281420 B)  |  75 |  4 | **abgewiesen** |
 *   | TR-DOS 80x2, 6400 B/Spur (1152500 B)  | 149 | 17 | **abgewiesen** |
 *
 * Abgewiesen, weil `max_head > 1` die Datei verwarf. **Jede
 * realistisch grosse UDI wurde abgelehnt**; nur Dateien unter 128 KB
 * kamen durch, und die lieferten eine einzige Spur. Dieselbe Klasse
 * wie **MF-961**, wo `86f` auf ein Magic probte, das in keiner echten
 * Datei steht — und wie dort meldete die Merkmalstafel dabei
 * „Read: SUPPORTED".
 *
 * Bemerkenswert: `src/formats/udi/uft_udi.c` liest den Kopf ueber eine
 * **gepackte Struktur** und liegt damit richtig. Die falsche Fassung
 * stand in der Datei, die benutzt wird.
 *
 * ── Befund 2: die Taktmarken gibt es fuer JEDEN Spurtyp ───────────────
 *
 * Spursatz:  Typ(1) · TLEN(2, LE) · Daten[TLEN] · CLK[CLEN]
 * mit `CLEN = TLEN/8 + (TLEN%8 + 7)/8` (das ist ceil(TLEN/8)).
 *
 * Die Typtafel des Urhebers, und die letzte Spalte ist der Punkt:
 *
 *   | Typ  | bedeutet                        | CLK dahinter |
 *   |---|---|---|
 *   | 0x00 | MFM                             | ja |
 *   | 0x01 | FM                              | ja |
 *   | 0x02 | gemischt MFM/FM                 | ja |
 *   | 0x80 | schwache/schwebende MFM-Daten   | ja |
 *   | 0x81 | schwache/schwebende FM-Daten    | ja |
 *   | 0x82 | schwache/schwebende gemischte   | ja |
 *   | 0x83 | schwache Daten, mehrere Lesungen| ja |
 *   | 0xE0 | Microdrive-Kassette             | nein (Bad-Byte-Marken) |
 *   | 0xF0 | zlib-gepackter Spurbehaelter    | nein |
 *
 * `udi_find_track()` uebersprang die Marken nur bei `ttype == 0x00`.
 * Hinter einer **FM**- oder gemischten Spur lag damit jede weitere
 * Spur um CLEN Byte daneben — bei 6250 Byte je Spur sind das 782 Byte,
 * also mitten in den Daten. Klasse **MF-794** (`sad` las 158 von 160
 * Spuren an der falschen Stelle).
 *
 * Das Kopfkommentar sagte dazu „Optional: … for MFM type 0x00" — eine
 * **Spec-Aussage ohne Quelle**, die der Urheber widerlegt.
 *
 * ── Befund 3: drei Pruefsummen, und die richtige stand woanders ───────
 *
 * Der Urheber gibt den Code woertlich:
 *
 *     CRC ^= -1 ^ *(((unsigned char*)buf)+i);
 *     for( BYTE k = 8; k--; ) {
 *         temp = -(CRC & 1); CRC >>= 1; CRC ^= 0xedb88320 & temp; }
 *     CRC ^= -1;
 *
 * Das ist **nicht** das gewoehnliche CRC-32: invertiert wird je Byte,
 * nicht einmal am Anfang und Ende. Gemessen an drei Handproben:
 *
 *   | Fassung | „UDI!" | gegen die Spezifikation |
 *   |---|---|---|
 *   | Spezifikation             | `196DC161` | — |
 *   | `uft_udi.c`               | `196DC161` | **richtig** |
 *   | hier vorher („PKZIP")     | `C7D6E182` | falsch |
 *   | `src/samdisk/udi.cpp`     | `138C9487` | falsch (`int32_t`, also
 *                                              arithmetischer Shift) |
 *
 * Die falsche Fassung stand in `udi_write_track()` und schrieb den
 * Vier-Byte-Abschluss jeder von UFT geschriebenen UDI — kein anderes
 * Werkzeug haette sie akzeptiert. **Und das Orakel liegt hier selbst
 * daneben:** SAMdisk rechnet mit `int32_t`, sein `crc >> 1` ist ein
 * arithmetischer Shift. Ein Orakel ist eine Referenz, kein Beweis;
 * gegen den Urheber gehalten faellt es.
 *
 * ── Was diese Datei bewusst NICHT tut ─────────────────────────────────
 *
 * UDI speichert **dekodierte Bytes plus eine Taktmarke je Byte**, nicht
 * Flusszellen. Die Marken liegen vollstaendig im Speicher (die ganze
 * Datei tut das) und sind ueber `uft_udi_track_clk()` erreichbar — es
 * ist also **kein Bit verloren**. Was fehlt, ist die Umsetzung in einen
 * echten MFM-Bitstrom: dafuer braucht es einen Encoder, der ein Byte
 * MIT vorgegebenem Taktmuster ausgibt (`0xA1`/`0x0A`, `0xC2`/`0x14`),
 * und `src/core/uft_mfm_encoder.c` arbeitet sektorweise. Gefuehrt als
 * offener Punkt, nicht als Zusage.
 *
 * `UFT_FORMAT_CAP_FLUX` bleibt **gesetzt**, und das ist gemessen statt
 * geraten: `src/core/uft_disk_convert.c:206` nutzt das Bit fuer die
 * **Verlustmeldung** („Quelle hat Fluss, Ziel nicht"). UDI traegt
 * wirklich mehr als ein Sektorabbild — die Taktmarken sind
 * Zellenwissen. Das Bit zu entfernen wuerde eine **wahre**
 * Verlustwarnung unterdruecken.
 *
 * Die Typen 0x80..0x83, 0xE0 und 0xF0 werden **benannt abgewiesen**
 * statt geraten: ihr Satzaufbau ist hier nicht gemessen, und eine
 * geratene Satzlaenge liest jede folgende Spur an der falschen Stelle
 * (genau Befund 2). Absagen ist die ehrliche Antwort.
 */
#include "uft/uft_format_common.h"
#include "uft/uft_mfm_encoder.h"
#include "uft/uft_log.h"

#define UDI_MAGIC       "UDI!"
#define UDI_HDR_SIZE    16
#define UDI_MAX_TRACK   8192    /* SAMdisk MAX_UDI_TRACK_SIZE */

/* Spurtypen nach der Typtafel des Urhebers */
#define UDI_T_MFM       0x00
#define UDI_T_FM        0x01
#define UDI_T_MIXED     0x02

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   track_start;    /* UDI_HDR_SIZE + EXTHDL */
    uint8_t  version;
    uint8_t  max_cyl;
    uint8_t  max_head;
    uint32_t crc_stored;
    uint32_t crc_computed;
    bool     crc_ok;
} udi_pd_t;

/* ============================================================================
 * Die Pruefsumme, woertlich nach der Spezifikation des Urhebers.
 *
 * `temp = -(CRC & 1)` auf einem vorzeichenlosen Typ ergibt 0 oder
 * 0xFFFFFFFF — genau die Maske, die der Referenzcode meint. Mit einem
 * VORZEICHENBEHAFTETEN Typ waere `CRC >>= 1` ein arithmetischer Shift,
 * und genau daran scheitert SAMdisks Fassung (Befund 3).
 * ============================================================================ */
static uint32_t udi_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= 0xFFFFFFFFu ^ data[i];
        for (int k = 8; k--; ) {
            uint32_t temp = (uint32_t)-(int32_t)(crc & 1u);
            crc >>= 1;
            crc ^= 0xEDB88320u & temp;
        }
        crc ^= 0xFFFFFFFFu;
    }
    return crc;
}

static uint32_t udi_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* CLEN nach der Spezifikation: TLEN/8 + (TLEN%8 + 7)/8 */
static size_t udi_clen(size_t tlen)
{
    return tlen / 8 + (tlen % 8 + 7) / 8;
}

/* Traegt dieser Typ Daten + Taktmarken, die wir zerlegen koennen? */
static bool udi_typ_bekannt(uint8_t t)
{
    return t == UDI_T_MFM || t == UDI_T_FM || t == UDI_T_MIXED;
}

static bool udi_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    if (size < UDI_HDR_SIZE) return false;
    if (memcmp(data, UDI_MAGIC, 4) != 0) return false;

    /* Ueber die Kennung hinaus das Groessenfeld pruefen, das die alte
     * Fassung gar nicht kannte: es traegt die Dateilaenge minus 4.
     * Trifft es zu, ist das ein getroffenes Merkmal (MF-729, 80..100). */
    if (file_size > 4 && udi_le32(data + 4) == (uint32_t)(file_size - 4))
        *confidence = 95;
    else
        *confidence = 80;      /* Kennung sicher, Groessenfeld unpassend */
    return true;
}

static uft_error_t udi_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < UDI_HDR_SIZE + 4) {
        free(data);
        return UFT_ERR_FILE_OPEN;
    }

    if (memcmp(data, UDI_MAGIC, 4) != 0) {
        free(data);
        return UFT_ERR_FORMAT_INVALID;
    }

    /* Kopffelder an ihren wirklichen Stellen (Befund 1). */
    uint8_t version  = data[8];
    uint8_t max_cyl  = data[9];
    uint8_t max_head = data[10];
    uint32_t exthdl  = udi_le32(data + 12);

    /* Die Spezifikation kennt nur Version 0 (1.0 final). Eine spaetere
     * Fassung koennte den Satzaufbau aendern — dann waere jede
     * Spurlaenge geraten. */
    if (version != 0x00) {
        UFT_WARN("UDI: Version %u ist nicht die spezifizierte 1.0 (0x00)",
                 version);
        free(data);
        return UFT_ERR_FORMAT_INVALID;
    }
    /* `max_head` ist 0 oder 1; 0x02..0xFF sind laut Spezifikation
     * reserviert. SAMdisk maskiert (`& 1`) und raet damit; hier wird
     * abgesagt. */
    if (max_head > 1) {
        UFT_WARN("UDI: Kopfzahl-Feld ist %u — 0x02..0xFF sind reserviert",
                 max_head);
        free(data);
        return UFT_ERR_FORMAT_INVALID;
    }

    size_t track_start = (size_t)UDI_HDR_SIZE + exthdl;
    if (track_start + 4 > file_size) {
        free(data);
        return UFT_ERR_FORMAT_INVALID;
    }

    /* Alle Spursaetze einmal ablaufen, BEVOR die Datei als lesbar
     * gemeldet wird: ein unbekannter Typ macht jede folgende Spur
     * unauffindbar, und das soll der Aufrufer erfahren statt es zu
     * merken. */
    size_t pos = track_start;
    int spuren = (max_cyl + 1) * (max_head + 1);
    for (int i = 0; i < spuren; i++) {
        if (pos + 3 > file_size) {
            UFT_WARN("UDI: Datei endet vor Spursatz %d von %d", i, spuren);
            free(data);
            return UFT_ERR_FORMAT_INVALID;
        }
        uint8_t t = data[pos];
        size_t tlen = (size_t)data[pos + 1] | ((size_t)data[pos + 2] << 8);
        if (tlen == 0) { pos += 3; continue; }   /* Spur fehlt — erlaubt */
        if (!udi_typ_bekannt(t)) {
            UFT_WARN("UDI: Spurtyp 0x%02X bei Satz %d ist nicht zerlegbar "
                     "(0x80..0x83 schwach, 0xE0 Microdrive, 0xF0 zlib)",
                     t, i);
            free(data);
            return UFT_ERR_NOT_SUPPORTED;
        }
        if (tlen > UDI_MAX_TRACK) {
            UFT_WARN("UDI: Spurlaenge %zu bei Satz %d ueberschreitet %d",
                     tlen, i, UDI_MAX_TRACK);
            free(data);
            return UFT_ERR_FORMAT_INVALID;
        }
        pos += 3 + tlen + udi_clen(tlen);       /* Befund 2: immer CLK */
        if (pos > file_size) {
            UFT_WARN("UDI: Spursatz %d reicht ueber das Dateiende", i);
            free(data);
            return UFT_ERR_FORMAT_INVALID;
        }
    }

    udi_pd_t *p = calloc(1, sizeof(udi_pd_t));
    if (!p) { free(data); return UFT_ERR_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->track_start = track_start;
    p->version = version;
    p->max_cyl = max_cyl;
    p->max_head = max_head;

    /* Die Pruefsumme steht in der DATEI, nicht in unserem Code
     * (MF-869/MF-1013). Sie wird nachgerechnet und gemeldet, aber sie
     * verwirft die Datei nicht: „Kein Bit verloren". */
    p->crc_computed = udi_crc32(data, file_size - 4);
    p->crc_stored = udi_le32(data + file_size - 4);
    p->crc_ok = (p->crc_computed == p->crc_stored);
    if (!p->crc_ok)
        UFT_WARN("UDI: Pruefsumme stimmt nicht — Datei %08X, "
                 "nachgerechnet %08X", p->crc_stored, p->crc_computed);

    disk->plugin_data = p;
    disk->geometry.cylinders = (uint32_t)max_cyl + 1;
    disk->geometry.heads = (uint32_t)max_head + 1;
    disk->geometry.sectors = 0;     /* Bytestrom, keine feste SPT */
    disk->geometry.sector_size = 0;
    disk->geometry.total_sectors = 0;
    return UFT_OK;
}

static void udi_close(uft_disk_t *disk)
{
    udi_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->data);
        free(p);
        disk->plugin_data = NULL;
    }
}

/* Versatz des Spursatzes (cyl, head) in der Datei, oder 0.
 *
 * MF-1015, Befund 2: die Taktmarken folgen JEDEM zerlegbaren Typ, nicht
 * nur MFM. Die alte Fassung sprang nur bei `ttype == 0x00` darueber und
 * las hinter einer FM-Spur alles Weitere um CLEN Byte zu frueh.
 */
static size_t udi_find_track(const udi_pd_t *p, int cyl, int head,
                              uint8_t *out_type, uint16_t *out_len)
{
    if (cyl < 0 || head < 0) return 0;
    if (cyl > p->max_cyl || head > p->max_head) return 0;

    size_t pos = p->track_start;
    int num_heads = p->max_head + 1;

    for (int c = 0; c <= p->max_cyl; c++) {
        for (int h = 0; h < num_heads; h++) {
            if (pos + 3 > p->size) return 0;

            uint8_t ttype = p->data[pos];
            uint16_t tlen = (uint16_t)p->data[pos + 1] |
                            ((uint16_t)p->data[pos + 2] << 8);

            if (c == cyl && h == head) {
                *out_type = ttype;
                *out_len = tlen;
                return pos;
            }

            pos += 3;
            if (tlen > 0) {
                if (!udi_typ_bekannt(ttype)) return 0;  /* nicht raten */
                pos += (size_t)tlen + udi_clen(tlen);
            }
        }
    }
    return 0;
}

static uft_error_t udi_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    udi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERR_INVALID_STATE;
    if (cyl > p->max_cyl || head > p->max_head)
        return UFT_ERROR_INVALID_PARAM;

    uft_track_init(track, cyl, head);

    uint8_t ttype = 0;
    uint16_t tlen = 0;
    size_t pos = udi_find_track(p, cyl, head, &ttype, &tlen);
    if (pos == 0) return UFT_OK; /* Spur nicht vorhanden */
    if (tlen == 0) return UFT_OK; /* Spur ausdruecklich leer */

    track->encoding = (ttype == UDI_T_FM) ? UFT_ENC_FM : UFT_ENC_MFM;

    size_t data_off = pos + 3;
    if (data_off + tlen + udi_clen(tlen) > p->size) return UFT_OK;

    track->raw_data = malloc(tlen);
    if (!track->raw_data) return UFT_ERR_MEMORY;
    memcpy(track->raw_data, p->data + data_off, tlen);
    track->raw_size = tlen;
    track->raw_len = tlen;
    /* UDI speichert dekodierte BYTES, nicht Flusszellen — `raw_bits`
     * zaehlt hier also Datenbits, nicht MFM-Zellen. Die Taktmarke je
     * Byte holt `uft_udi_track_clk()`. */
    track->raw_bits = (size_t)tlen * 8;
    track->raw_capacity = tlen;
    track->owns_data = true;

    return UFT_OK;
}

/* ============================================================================
 * Zugang zu den Taktmarken.
 *
 * Eine Marke je DATENBYTE: 0 = normaler Takt, 1 = Marken-Takt (das ist
 * die „missing clock" der Adressmarken). Bit n des Feldes gehoert zu
 * Datenbyte n, niedrigstes Bit zuerst — `clk[n >> 3] & (1 << (n & 7))`.
 *
 * MF-1015: ohne diesen Weg lagen die Marken im Speicher und waren von
 * aussen nicht zu sehen. Verloren waren sie nie — die ganze Datei liegt
 * im Puffer —, aber „nicht verloren" und „erreichbar" sind zwei
 * verschiedene Aussagen, und nur die zweite ist eine Faehigkeit.
 * ============================================================================ */
int uft_udi_track_clk(const uft_disk_t *disk, int cyl, int head,
                      const uint8_t **clk, size_t *clk_len)
{
    if (!disk || !clk || !clk_len) return -1;
    const udi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return -1;

    uint8_t ttype = 0;
    uint16_t tlen = 0;
    size_t pos = udi_find_track(p, cyl, head, &ttype, &tlen);
    if (pos == 0 || tlen == 0) return -1;

    size_t clen = udi_clen(tlen);
    size_t off = pos + 3 + tlen;
    if (off + clen > p->size) return -1;

    *clk = p->data + off;
    *clk_len = clen;
    return 0;
}

/* Sagt, ob die Pruefsumme der Datei aufgeht. 0 = ja, 1 = nein, -1 =
 * nicht zu beantworten. */
int uft_udi_pruefsumme(const uft_disk_t *disk, uint32_t *soll, uint32_t *ist)
{
    if (!disk) return -1;
    const udi_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return -1;
    if (soll) *soll = p->crc_stored;
    if (ist)  *ist  = p->crc_computed;
    return p->crc_ok ? 0 : 1;
}

/* ============================================================================
 * write_track — in-place overwrite via shared MFM encoder.
 *
 * Strategy (honest limits documented):
 *   1. Encode the sector array to raw MFM via uft_mfm_encode_from_track.
 *   2. Locate the target track's existing slot in the in-memory buffer.
 *   3. Overwrite ONLY if the new MFM length matches the existing slot
 *      exactly. UDI doesn't have per-track padding room, so any size
 *      change would shift every following track + the CRC32 footer —
 *      that's a full file rebuild which is out of scope here.
 *   4. Recompute the checksum over the full file minus the last 4
 *      bytes, write the updated buffer back to disk.
 *
 * MF-1015: Schritt 4 rechnete mit dem gewoehnlichen CRC-32 statt mit
 * der Fassung des Urhebers (Befund 3). Jede von UFT geschriebene UDI
 * trug damit einen Abschluss, den kein anderes Werkzeug akzeptiert —
 * eine **stille Veraenderung** an der Datei, die die Datei selbst als
 * beschaedigt ausweist. Jetzt derselbe `udi_crc32()` wie beim Lesen.
 *
 * **Und was hier weiterhin NICHT stimmt, steht als offener Punkt:** die
 * Taktmarken der ueberschriebenen Spur bleiben unangetastet, waehrend
 * die Daten neu kodiert werden. Fuer eine Spur, deren Adressmarken an
 * anderer Stelle liegen als vorher, passen Marken und Daten danach
 * nicht mehr zusammen. Der Schreiber ist deshalb bewusst eng: nur bei
 * **exakt gleicher Laenge**, und die Merkmalstafel sagt „Write:
 * UNSUPPORTED".
 *
 * Size-mismatch handling: returns UFT_ERR_INVALID_ARG with intent
 * clear in the caller's error path. This is honest: we don't silently
 * corrupt the file.
 * ============================================================================ */
static uft_error_t udi_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    if (!disk || !track) return UFT_ERR_NULL_POINTER;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    udi_pd_t *p = disk->plugin_data;
    if (!p || !p->data || p->size < UDI_HDR_SIZE + 4) return UFT_ERR_INVALID_STATE;

    uint8_t  ttype = 0;
    uint16_t tlen  = 0;
    size_t   pos   = udi_find_track(p, cyl, head, &ttype, &tlen);
    if (pos == 0 || ttype != UDI_T_MFM || tlen == 0) return UFT_ERR_NOT_FOUND;

    /* Encode sector payload to MFM. Use a scratch buffer sized to the
     * existing slot — the caller gets UFT_ERR_INVALID_ARG if the
     * encoder output doesn't fit exactly. */
    uint8_t *mfm = (uint8_t *)malloc(tlen);
    if (!mfm) return UFT_ERR_MEMORY;
    size_t n = uft_mfm_encode_from_track(track, mfm, tlen);
    if (n != tlen) {
        free(mfm);
        return UFT_ERR_INVALID_ARG;
    }

    /* In-place overwrite in the buffer. */
    memcpy(p->data + pos + 3, mfm, tlen);
    free(mfm);

    /* Abschluss neu bilden — mit der Fassung des Urhebers. */
    uint32_t crc = udi_crc32(p->data, p->size - 4);
    p->data[p->size - 4] = (uint8_t)(crc & 0xFF);
    p->data[p->size - 3] = (uint8_t)((crc >> 8) & 0xFF);
    p->data[p->size - 2] = (uint8_t)((crc >> 16) & 0xFF);
    p->data[p->size - 1] = (uint8_t)((crc >> 24) & 0xFF);
    p->crc_stored = crc;
    p->crc_computed = crc;
    p->crc_ok = true;

    /* Flush to disk. */
    if (!disk->path) return UFT_ERR_INVALID_STATE;
    if (!uft_write_file(disk->path, p->data, p->size)) return UFT_ERR_IO;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_udi_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-1015: write_track schreibt bis in die Datei, aber nur bei exakt "
      "gleicher Spurlaenge und ohne die Taktmarken nachzuziehen" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_SUPPORTED,
      "MF-1015: nicht Flusszeiten — UDI traegt eine Taktmarke je Datenbyte "
      "(uft_udi_track_clk); das ist Zellenwissen, das ein Sektorabbild "
      "nicht halten kann, und genau dafuer wertet uft_disk_convert.c das "
      "Bit aus" },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED,
      "MF-1015: die Typen 0x80..0x83 der Spezifikation tragen schwache "
      "Daten, ihr Satzaufbau ist hier NICHT gemessen — sie werden benannt "
      "abgewiesen statt geraten" },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_udi = {
    .name = "UDI", .description = "Ultra Disk Image (Spectrum)",
    .extensions = "udi", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe = udi_probe, .open = udi_open, .close = udi_close,
    .read_track = udi_read_track,
    .write_track = udi_write_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* MF-1015: die Spezifikation des
                                             * Urhebers liegt vor und ist
                                             * gegen sie umgesetzt (vorher
                                             * REVERSE_ENGINEERED) */
    .features = uft_format_plugin_udi_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_udi_features) / sizeof(uft_format_plugin_udi_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(udi)
