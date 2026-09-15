/**
 * @file uft_xfd.c
 * @brief XFD (Atari 8-bit Raw Disk) Plugin
 *
 * XFD is a headerless raw sector dump for Atari 8-bit computers.
 * Geometry from file size: 720 sectors × 128 bytes = 92160 (SD)
 * or 720 × 256 = 184320 (DD), or 1040 × 128/256 (ED).
 */
#include "uft/uft_format_common.h"

#define XFD_SS  128
#define XFD_DS  256

typedef struct { FILE* file; uint16_t ss; uint32_t total; uint16_t spt; } xfd_data_t;

/*
 * MF-1164: Sektorgroesse und Spurbreite kommen aus einer TAFEL, nicht aus
 * der Teilbarkeit der Dateigroesse.
 *
 * Vorher stand hier:
 *
 *     p->ss = ((size_t)fs % 256 == 0 && fs > 92160) ? 256 : 128;
 *
 * Eine Enhanced-Density-Diskette hat 1040 Sektoren von 128 Byte, also
 * 133 120 Byte — und diese Zahl ist durch 256 teilbar und groesser als
 * 92 160. Der Leser meldete deshalb 520 Sektoren von 256 Byte, dazu mit
 * fest 18 Sektoren je Spur 29 Zylinder statt 40. Keine Absage, keine
 * Warnung: die Datei ging auf, und jeder Sektor trug die Bytes zweier
 * anderer (Klasse MF-1016/MF-1026/MF-1038). Der SD-Fall ging nur durch,
 * weil die Schranke `> 92160` und nicht `>= 92160` lautet — 92 160 ist
 * selbst durch 256 teilbar.
 *
 * XFD ist ATR ohne den 16-Byte-Kopf, und `atr[16:] == xfd` ist ein
 * REGISTRIERTER verlustfreier Wandlungspfad (src/core/uft_roundtrip.c:393,
 * MF-655). Die Spurbreite folgt hier deshalb WOERTLICH derselben Regel wie
 * in `src/formats/atr/uft_atr.c` (MF-834): 1040 Sektoren -> 26, 720 -> 18.
 * Nicht weil es huebscher ist, sondern damit die beiden Haelften eines
 * zugesagten Paares nicht wieder auseinanderlaufen koennen.
 *
 * Belegt durch drei unabhaengige Quellen, alle nur GELESEN:
 *   - SIO2PCs Groessentabelle `2SIOTEXT.S:1125`, ueber uft_atr.c (MF-834):
 *     5760 Absaetze = 90 K SD = 18, 8320 = 130 K ED = 26, 11520 = 180 K DD = 18
 *   - Firmware der Atari 1050 Turbo, `FORMAT.M65` FORTAB Z. 830-930, erstes
 *     Feld `SECCNT` (Feldfolge `EQUATES.M65` Z. 495-530): SD 18, ED 26,
 *     DD 18. Diese Quelle ist gegen ihr eigenes Erzeugnis geprueft — das
 *     aus dem Quelltext gebaute `turbo1050-35.rom` und das ausgelieferte
 *     `T1050_2B.8KB` (v3.5, 1988) sind md5-identisch
 *     (35be2c58f1e0b04ab5a1f2459e5515bd, 0 abweichende Byte).
 *     (c) 1986-88 Bernhard Engl, kein Grant — nicht portiert.
 *   - `jhallen/atari-tools`, readme.md: „133,136 bytes (16 byte .atr header
 *     + 40 tracks * 26 sectors per track * 128 bytes per sector)".
 *
 * Was die Tafel NICHT leistet und was hier ausdruecklich offen bleibt:
 * fuer eine Groesse ausserhalb der vier gibt es in einem KOPFLOSEN Abbild
 * keine Angabe, aus der die Sektorgroesse folgen koennte. Der alte
 * Rueckfall bleibt deshalb unveraendert stehen und ist als ANGENOMMEN
 * benannt — die Sonde vergibt fuer solche Dateien ohnehin nur 25
 * (Band „kein Anspruch", MF-729).
 */
typedef struct {
    long     bytes;
    uint16_t ss;
    uint32_t total;
    uint16_t spt;
} xfd_standardformat_t;

static const xfd_standardformat_t XFD_STANDARD[] = {
    {  92160, 128,  720, 18 },   /* 90 K  SD  40 x 18 x 128 */
    { 133120, 128, 1040, 26 },   /* 130 K ED  40 x 26 x 128 */
    { 184320, 256,  720, 18 },   /* 180 K DD  40 x 18 x 256 */
    { 266240, 256, 1040, 26 },   /* 260 K     40 x 26 x 256 */
};

/* Sektoren je Spur nach derselben Regel wie uft_atr.c (MF-834). */
static uint16_t xfd_spt_aus_sektorzahl(uint32_t total) {
    if (total == 1040u) return 26u;   /* DOS 2.5 ED */
    if (total ==  720u) return 18u;   /* SD und DD  */
    return 18u;                       /* angenommen, siehe Kopf oben */
}

static bool uft_xfd_plugin_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    if (fs != 92160 && fs != 184320 && fs != 133120 && fs != 266240) {
        if (fs == 0 || (fs % 128 != 0 && fs % 256 != 0) || fs > 266240) return false;
        *c = 25; return true;
    }
    *c = 40;

    /* Atari DOS boot sector: byte 0 = boot flag (0x00 or 0x01),
     * bytes 1-2 = sector count (LE16), byte 3 = load address high */
    if (s >= 4) {
        if ((d[0] == 0x00 || d[0] == 0x01) && d[3] >= 0x07 && d[3] <= 0xBF)
            *c = 82;
    }
    return true;
}

static uft_error_t xfd_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    xfd_data_t *p = calloc(1, sizeof(xfd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    /* MF-1164: erst die Tafel, dann der benannte Rueckfall. */
    p->ss = 0;
    for (size_t i = 0; i < sizeof(XFD_STANDARD) / sizeof(XFD_STANDARD[0]); i++) {
        if (XFD_STANDARD[i].bytes == fs) {
            p->ss    = XFD_STANDARD[i].ss;
            p->total = XFD_STANDARD[i].total;
            p->spt   = XFD_STANDARD[i].spt;
            break;
        }
    }
    if (p->ss == 0) {
        /* Unbekannte Groesse: die alte Ableitung, unveraendert, und die
         * Sektorgroesse ist damit ANGENOMMEN (siehe Kopf). */
        p->ss    = ((size_t)fs % 256 == 0 && fs > 92160) ? XFD_DS : XFD_SS;
        p->total = (uint32_t)fs / p->ss;
        p->spt   = xfd_spt_aus_sektorzahl(p->total);
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = (p->total + p->spt - 1u) / p->spt;
    disk->geometry.heads = 1;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = p->total;
    return UFT_OK;
}

static void xfd_close(uft_disk_t *d) {
    xfd_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t xfd_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    xfd_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    /* MF-1164: die Spurbreite aus `open`, nicht fest 18 — bei Enhanced
     * Density sind es 26, und mit 18 lag ab Spur 1 jeder Sektor falsch. */
    const uint32_t spt = p->spt ? p->spt : 18u;
    long off = (long)cyl * (long)spt * p->ss;
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (uint32_t s = 0; s < spt; s++) {
        uint32_t sec = (uint32_t)cyl * spt + s;
        if (sec >= p->total) break;
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->ss, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t xfd_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
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
    if (cyl >= (int)d->geometry.cylinders ||
        head >= (int)d->geometry.heads) return UFT_ERROR_INVALID_PARAM;

    xfd_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* MF-1164: auch hier die Spurbreite aus `open`. Beim SCHREIBEN wiegt
     * der falsche Teiler schwerer — er bestimmt, WOHIN geschrieben wird
     * (dieselbe Begruendung wie MF-529). */
    const size_t spt = p->spt ? p->spt : 18u;
    long off = (long)cyl * (long)spt * p->ss;
    for (size_t s = 0; s < t->sector_count && s < spt; s++) {
        uint32_t sec = (uint32_t)((size_t)cyl * spt + s);
        if (sec >= p->total) break;
        if (fseek(p->file, off + (long)s * p->ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, p->ss); data = pad; }
        if (fwrite(data, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_xfd_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_xfd = {
    .name = "XFD", .description = "Atari 8-bit Raw Disk",
    .extensions = "xfd", .format = UFT_FORMAT_XFD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = uft_xfd_plugin_probe, .open = xfd_open, .close = xfd_close,
    .read_track = xfd_read_track, .write_track = xfd_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_xfd_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_xfd_features) / sizeof(uft_format_plugin_xfd_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(xfd)
