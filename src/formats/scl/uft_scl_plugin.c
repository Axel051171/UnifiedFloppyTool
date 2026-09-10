/**
 * @file uft_scl_plugin.c
 * @brief SCL (ZX Spectrum TR-DOS) — Plugin-B
 *
 * ── Was SCL ist, und warum das den Unterschied macht ───────────────────
 *
 * SCL ist **kein Abbild, sondern ein Archiv**: acht Byte `"SINCLAIR"`,
 * eine Dateizahl, je Datei ein **14-Byte-Katalogeintrag**, danach die
 * Dateidaten hintereinander, am Ende eine **32-Bit-Summe**.
 *
 * Wer eine SCL oeffnet, muss daraus eine TR-DOS-Diskette **ausbauen**:
 * Spur 0 traegt den Katalog (Sektoren 0..7) und den Systemsektor
 * (Sektor 8), die Dateidaten beginnen bei **LBA 16** — also auf der
 * zweiten logischen Spur, das ist Zylinder 0 / **Kopf 1**.
 *
 * ── Was hier vorher stand (MF-1014) ────────────────────────────────────
 *
 * `scl_open()` setzte `data_start = 9 + n*14` und legte die
 * **Dateidaten unmittelbar auf Zylinder 0, Sektor 0** — genau dorthin,
 * wo der Katalog gehoert. Dazu `heads = 1` und eine Zylinderzahl, die
 * aus der Datenlaenge gerechnet war statt aus der TR-DOS-Geometrie.
 *
 * Die Folge war nicht „etwas ungenau", sondern: **jeder Sektor lag eine
 * ganze Spur zu frueh, und einen Katalog gab es nirgends.** Ein
 * TR-DOS-Dateisystemleser findet auf so einer Diskette keine einzige
 * Datei — auch der in diesem Baum nicht, der die Dateizahl bei `0x8E4`
 * erwartet (`src/formats/trd/uft_trd.c`).
 *
 * ── Referenzen (EINFRIER-REGEL MF-363/498, Bedingung c) ────────────────
 *
 * **Umsetzungsreferenz:** `src/samdisk/scl.cpp` (SAMdisk, MIT, liegt in
 * diesem Baum). Von dort stammt das Ausbau-Verhalten: `uDataLba = 16`,
 * `pb[14] = uDataLba & 0x0f`, `pb[15] = uDataLba >> 4`, die Feldlagen im
 * Systemsektor und die Zylinderregel `SizeToCylsTRD()`
 * (`src/samdisk/trd.cpp:190`).
 *
 * **Unabhaengige Bestaetigung derselben Feldlagen** — nur gelesen, keine
 * Zeile uebernommen (Kanal *Spec* nach MF-695):
 * `tools/uft-scout/work/HxCFloppyEmulator/libhxcfe/sources/loaders/`
 * `scl_loader/scl_loader.c` (HxC, GPL-2). HxC baut dieselbe Diskette aus
 * einer voellig anderen Codebasis, und **neun Feldpositionen stimmen
 * byteweise**:
 *
 * | Byte | bedeutet | SAMdisk | HxC |
 * |---|---|---|---|
 * | 0x8E1 | erster freier Sektor | `pb[225]` | `trd_fsec` |
 * | 0x8E2 | erste freie Spur     | `pb[226]` | `trd_ftrk`, Vorlage `0x01` |
 * | 0x8E3 | Geometriebyte        | `pb[227] = 0x16` | Vorlage `0x16` |
 * | 0x8E4 | Dateizahl            | `pb[228]` | `trd_files` |
 * | 0x8E5 | freie Sektoren (16 Bit) | `pb[229..230]` | `tmp`, Vorlage `F0 09` |
 * | 0x8E7 | TR-DOS-Kennbyte      | `pb[231] = 0x10` | Vorlage `0x10` |
 * | 0x8EA | neun Leerzeichen     | `memset(pb+234,' ',9)` | Vorlage `0x20`×9 |
 * | 0x8F4 | geloeschte Dateien   | `pb[244] = 0` | Vorlage `0x00` |
 * | 0x8F5 | Diskettenname        | `pb[245]` | `"HxCFE"` |
 *
 * **Dritte Uebereinstimmung, im eigenen Baum:** `uft_trd.c` liest die
 * Dateizahl bei `0x8E4` und den Infosatz ab `0x800`. Drei unabhaengige
 * Umsetzungen, dieselben Stellen.
 *
 * **Und eine Stelle, an der dieses Werkzeug den Orakeln bewusst NICHT
 * folgt:** den Diskettennamen bei `0x8F5` traegt die SCL-Datei **nicht**.
 * SAMdisk setzt dort den Dateinamen der Quelldatei ein, HxC schreibt
 * `"HxCFE"`. Beides ist erfunden. Nach „Keine erfundenen Daten" bleibt
 * das Feld hier **leer** — wer den Namen sehen will, sieht, dass keiner
 * da war.
 *
 * ── Was diese Datei NICHT tut ──────────────────────────────────────────
 *
 * Sie stellt die **Sektorebene** her, nicht die physische Spur. Ob die
 * MFM-Sektorkennungen einer echten TR-DOS-Diskette ab 0 oder ab 1
 * zaehlen, ist hier **nicht gemessen**; die Kennungen folgen der
 * lineraren Ordnung, die beide Orakel festlegen, und dem Katalog (fuer
 * LBA 16 ergibt `& 0x0F` die 0). Fuer die Sektorebene ist das ohne
 * Belang, fuer einen Flusspfad waere es zu messen.
 */
#include "uft/uft_format_common.h"
#include "uft/uft_log.h"

#define SCL_MAGIC        "SINCLAIR"
#define SCL_MAGIC_LEN    8
#define SCL_ENTRY_SIZE   14     /* im Archiv */
#define TRD_ENTRY_SIZE   16     /* auf der Diskette: + Sektor + Spur */
#define TRD_SECTOR_SIZE  256
#define TRD_SPT          16
#define TRD_HEADS        2
#define TRD_TRACK_SIZE   (TRD_SPT * TRD_SECTOR_SIZE)     /* 4096 */
#define TRD_MAXFILES     128    /* SAMdisk TRD_MAXFILES, HxC `> 127` */
#define TRD_NORM_CYLS    80
#define TRD_MAX_CYLS     128
#define SCL_DATA_LBA     16     /* SAMdisk: uDataLba = 16 */
#define SCL_SUM_SIZE     4
#define SCL_INFO_OFF     (8 * TRD_SECTOR_SIZE)           /* 0x800 */

typedef struct {
    uint8_t *image;          /* die ausgebaute TR-DOS-Diskette */
    size_t   image_size;
    int      cyls;
    uint8_t  file_count;
    uint32_t sum_stored;     /* die 4 Byte am Dateiende */
    uint32_t sum_computed;   /* selbst nachgerechnet */
    bool     sum_checked;    /* false, wenn die Dateilaenge nicht passt */
    bool     sum_ok;
} scl_pd_t;

/* ============================================================================
 * Die Zylinderregel — `SizeToCylsTRD()`, src/samdisk/trd.cpp:190
 * ============================================================================ */
static int scl_cyls_fuer(size_t inhalt_bytes)
{
    const size_t groesse_80_2 = (size_t)TRD_TRACK_SIZE * 80 * 2;    /* 655360 */
    const size_t groesse_128_2 = (size_t)TRD_TRACK_SIZE * 128 * 2;

    if (inhalt_bytes <= groesse_80_2) return TRD_NORM_CYLS;
    if (inhalt_bytes > groesse_128_2) return TRD_MAX_CYLS;

    /* auf einen vollen Zylinder aufrunden (zwei Spuren = 8192 Byte) */
    size_t block = (size_t)TRD_TRACK_SIZE * TRD_HEADS;
    size_t gerundet = ((inhalt_bytes + block - 1) / block) * block;
    return (int)(gerundet / block);
}

/* ============================================================================
 * Die Summe, die in der Datei selbst steht
 *
 * SAMdisk summiert BYTEWEISE: die 9 Kopfbytes, je Datei die 14 gelesenen
 * Eintragsbytes (ohne die zwei, die es selbst ergaenzt) und alle
 * Datenbytes. 32 Bit, Ueberlauf umlaufend, am Dateiende little-endian.
 * ============================================================================ */
static uint32_t scl_summe(const uint8_t *raw, size_t bis)
{
    uint32_t s = 0;
    for (size_t i = 0; i < bis; i++) s += raw[i];
    return s;
}

static uint32_t scl_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool scl_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < SCL_MAGIC_LEN + 1) return false;
    if (memcmp(data, SCL_MAGIC, SCL_MAGIC_LEN) != 0) return false;
    /* Ueber die Kennung hinaus pruefen, was die Orakel pruefen: die
     * Dateizahl gegen die TR-DOS-Grenze. Eine Kennung plus ein
     * gepruefter Strukturwert ist nach MF-729 ein getroffenes Merkmal. */
    if (data[SCL_MAGIC_LEN] > TRD_MAXFILES) return false;
    *confidence = 96;
    return true;
}

static uft_error_t scl_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);

    /* SAMdisks Mindestgroesse: Kopf + ein Eintrag + 1 */
    if (!raw || raw_size < (size_t)(SCL_MAGIC_LEN + 1 + SCL_ENTRY_SIZE + 1)) {
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }
    if (memcmp(raw, SCL_MAGIC, SCL_MAGIC_LEN) != 0) {
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t n = raw[SCL_MAGIC_LEN];
    /* Beide Orakel weisen mehr als die TR-DOS-Grenze ab: SAMdisk
     * `sh.bFiles > 128`, HxC `*trd_files > 127`. Der Katalog hat
     * 128 * 16 = 2048 Byte, also genau acht Sektoren — mehr passt
     * nicht, und ein neunter Eintrag ueberschriebe den Infosatz. */
    if (n > TRD_MAXFILES) { free(raw); return UFT_ERROR_FORMAT_INVALID; }

    size_t kopf = (size_t)SCL_MAGIC_LEN + 1;
    size_t katalog_bytes = (size_t)n * SCL_ENTRY_SIZE;
    if (raw_size < kopf + katalog_bytes + SCL_SUM_SIZE) {
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* Sektorzahl aus den Eintraegen aufsummieren */
    size_t daten_sektoren = 0;
    for (uint8_t i = 0; i < n; i++)
        daten_sektoren += raw[kopf + (size_t)i * SCL_ENTRY_SIZE + 13];

    size_t daten_off = kopf + katalog_bytes;
    size_t daten_bytes = daten_sektoren * TRD_SECTOR_SIZE;
    size_t soll_groesse = daten_off + daten_bytes + SCL_SUM_SIZE;

    /* Fehlende Daten sind ein Abbruch — wir wuerden sonst Nullen als
     * Dateiinhalt ausgeben. Ueberzaehlige Bytes werden geduldet, die
     * Summe dann aber als nicht pruefbar gefuehrt. */
    if (raw_size < soll_groesse) { free(raw); return UFT_ERROR_FORMAT_INVALID; }

    scl_pd_t *p = calloc(1, sizeof(scl_pd_t));
    if (!p) { free(raw); return UFT_ERROR_NO_MEMORY; }

    size_t inhalt = ((size_t)SCL_DATA_LBA + daten_sektoren) * TRD_SECTOR_SIZE;
    p->cyls = scl_cyls_fuer(inhalt);
    p->file_count = n;
    p->image_size = (size_t)p->cyls * TRD_HEADS * TRD_TRACK_SIZE;

    /* Passt der Inhalt ueberhaupt? Bei > 1 MB sagt SizeToCylsTRD 128 und
     * SAMdisk warnt; hier wird abgesagt, statt still abzuschneiden. */
    if (inhalt > p->image_size) { free(p); free(raw); return UFT_ERROR_FORMAT_INVALID; }

    p->image = calloc(1, p->image_size);
    if (!p->image) { free(p); free(raw); return UFT_ERROR_NO_MEMORY; }

    /* ── Katalog: Spur 0, Sektoren 0..7 ──────────────────────────────── */
    size_t lba = SCL_DATA_LBA;
    for (uint8_t i = 0; i < n; i++) {
        const uint8_t *e = raw + kopf + (size_t)i * SCL_ENTRY_SIZE;
        uint8_t *ziel = p->image + (size_t)i * TRD_ENTRY_SIZE;
        memcpy(ziel, e, SCL_ENTRY_SIZE);
        ziel[14] = (uint8_t)(lba & 0x0F);           /* Startsektor */
        ziel[15] = (uint8_t)(lba >> 4);             /* Startspur   */
        lba += e[13];
    }

    /* ── Infosatz: Spur 0, Sektor 8 ──────────────────────────────────── */
    uint8_t *info = p->image + SCL_INFO_OFF;
    size_t gesamt_sektoren = (size_t)p->cyls * TRD_HEADS * TRD_SPT;
    size_t frei = (gesamt_sektoren > lba) ? gesamt_sektoren - lba : 0;
    info[225] = (uint8_t)(lba & 0x0F);              /* erster freier Sektor */
    info[226] = (uint8_t)(lba >> 4);                /* erste freie Spur     */
    info[227] = 0x16;                               /* Geometriebyte        */
    info[228] = n;                                  /* Dateizahl            */
    info[229] = (uint8_t)(frei & 0xFF);
    info[230] = (uint8_t)((frei >> 8) & 0xFF);
    info[231] = 0x10;                               /* TR-DOS-Kennbyte      */
    memset(info + 234, ' ', 9);
    info[244] = 0;                                  /* geloeschte Dateien   */
    /* 0x8F5 (Diskettenname) bleibt leer — siehe Dateikopf. */

    /* ── Dateidaten ab LBA 16 ────────────────────────────────────────── */
    memcpy(p->image + (size_t)SCL_DATA_LBA * TRD_SECTOR_SIZE,
           raw + daten_off, daten_bytes);

    /* ── Die Summe, die in der Datei steht ───────────────────────────── */
    p->sum_computed = scl_summe(raw, daten_off + daten_bytes);
    p->sum_stored = scl_le32(raw + daten_off + daten_bytes);
    p->sum_checked = (raw_size == soll_groesse);
    p->sum_ok = p->sum_checked && (p->sum_computed == p->sum_stored);
    if (p->sum_checked && !p->sum_ok) {
        UFT_WARN("SCL: Summe stimmt nicht — Datei %08X, nachgerechnet %08X",
                 p->sum_stored, p->sum_computed);
    } else if (!p->sum_checked) {
        UFT_WARN("SCL: %zu Byte statt %zu — Summe nicht pruefbar",
                 raw_size, soll_groesse);
    }

    free(raw);

    disk->plugin_data = p;
    disk->geometry.cylinders = (uint32_t)p->cyls;
    disk->geometry.heads = TRD_HEADS;
    disk->geometry.sectors = TRD_SPT;
    disk->geometry.sector_size = TRD_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)gesamt_sektoren;
    return UFT_OK;
}

static void scl_close(uft_disk_t *disk) {
    scl_pd_t *p = disk->plugin_data;
    if (p) { free(p->image); free(p); disk->plugin_data = NULL; }
}

static uft_error_t scl_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    scl_pd_t *p = disk->plugin_data;
    if (!p || !p->image) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->cyls || head >= TRD_HEADS) return UFT_ERROR_INVALID_PARAM;

    uft_track_init(track, cyl, head);

    /* Logische Spur = Zylinder * 2 + Kopf. Beide Orakel legen die Daten
     * so ab: HxC rechnet `trd_ftrk * 4096`, SAMdisk fuellt die Spuren in
     * derselben Reihenfolge (Zylinder aussen, Kopf innen). */
    size_t log_spur = (size_t)cyl * TRD_HEADS + (size_t)head;
    for (int s = 0; s < TRD_SPT; s++) {
        size_t off = log_spur * TRD_TRACK_SIZE + (size_t)s * TRD_SECTOR_SIZE;
        if (off + TRD_SECTOR_SIZE > p->image_size) break;
        uft_format_add_sector(track, (uint8_t)s, p->image + off,
                              TRD_SECTOR_SIZE, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* Write: MF-883/MF-1014 — siehe Begruendung unten. */
static uft_error_t scl_write_track(uft_disk_t *disk, int cyl, int head,
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

    scl_pd_t *p = disk->plugin_data;
    if (!p || !p->image) return UFT_ERROR_INVALID_STATE;
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
     * MF-1014 macht die Absage inhaltlich noch klarer: SCL ist ein
     * ARCHIV. Wer eine Spur schreibt, muesste den Katalog neu lesen, die
     * betroffenen Dateien neu zuschneiden, die Eintraege umnummerieren
     * und die Summe neu bilden. Das ist kein Sektorschreiber, das ist ein
     * Packer — und ohne ein SCL von fremder Hand im Korpus waere er
     * unpruefbar (EINFRIER-REGEL MF-363/498).
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_scl_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883/1014: SCL ist ein Archiv — ein Spurschreiber muesste packen; "
      "kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_scl = {
    .name = "SCL", .description = "ZX Spectrum SCL Container",
    .extensions = "scl", .format = UFT_FORMAT_SCL,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = scl_plugin_probe, .open = scl_open,
    .close = scl_close, .read_track = scl_read_track,
    .write_track = scl_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_scl_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_scl_features) / sizeof(uft_format_plugin_scl_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(scl)

/* ============================================================================
 * Fuer den Beweis: die Summe aus der Datei nach draussen geben.
 *
 * `uft_scl_pruefsumme()` liest die Datei selbst, rechnet nach und sagt,
 * ob die 32-Bit-Summe am Dateiende aufgeht. Das ist der einzige Wert an
 * einer SCL, der NICHT aus unserem Code kommt — er steht in der Datei.
 * Genau der Weg von MF-869 (die CRCs stehen auf der Diskette) und
 * MF-1013 (die Pruefsummen stehen auf der Diskette).
 * ============================================================================ */
int uft_scl_pruefsumme(const char *path, uint32_t *soll, uint32_t *ist)
{
    if (!path) return -1;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);
    if (!raw || raw_size < (size_t)(SCL_MAGIC_LEN + 1 + SCL_SUM_SIZE)) {
        free(raw);
        return -1;
    }
    if (memcmp(raw, SCL_MAGIC, SCL_MAGIC_LEN) != 0) { free(raw); return -1; }

    uint8_t n = raw[SCL_MAGIC_LEN];
    size_t kopf = (size_t)SCL_MAGIC_LEN + 1;
    if (raw_size < kopf + (size_t)n * SCL_ENTRY_SIZE + SCL_SUM_SIZE) {
        free(raw);
        return -1;
    }
    size_t daten_sektoren = 0;
    for (uint8_t i = 0; i < n; i++)
        daten_sektoren += raw[kopf + (size_t)i * SCL_ENTRY_SIZE + 13];

    size_t ende = kopf + (size_t)n * SCL_ENTRY_SIZE
                + daten_sektoren * TRD_SECTOR_SIZE;
    if (raw_size < ende + SCL_SUM_SIZE) { free(raw); return -1; }

    if (ist)  *ist  = scl_summe(raw, ende);
    if (soll) *soll = scl_le32(raw + ende);
    int gleich = (scl_summe(raw, ende) == scl_le32(raw + ende));
    free(raw);
    return gleich ? 0 : 1;
}
