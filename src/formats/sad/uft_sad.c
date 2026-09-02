/**
 * @file uft_sad.c
 * @brief SAM Coupe SAD format — read + write
 * @version 3.8.1
 */
#include "uft/uft_format_common.h"

#define SAD_SIZE 819200

/* Die SAD-Kennung, gemessen — nicht geraten (MF-787).
 *
 * Hier stand `"SAD!"`. Die steht in KEINER SAD-Datei. Gemessen an einem
 * von SAMdisk 4.0 erzeugten Abbild (Oracle-Register: `samdisk`) lautet
 * der 22-Byte-Kopf:
 *
 *     41 6c 65 79 27 73 20 64 69 73 6b 20 62 61 63 6b 75 70
 *         -> "Aley's disk backup", 18 Byte
 *     02  -> Koepfe
 *     50  -> Zylinder (80)
 *     0a  -> Sektoren je Spur (10)
 *     08  -> Sektorgroesse / 64  (8 * 64 = 512)
 *
 * Der Kopf BELEGT SICH SELBST: 80 * 2 * 10 * 512 = 819 200, und das ist
 * genau die Nutzdatenmenge der Datei (819 222 - 22).
 *
 * Was die falsche Kennung kostete, war doppelt und still: `sad_probe()`
 * fiel auf die Groessenpruefung `file_size == SAD_SIZE` zurueck, die eine
 * echte SAD-Datei NIE erfuellt (ihr Kopf legt 22 Byte drauf) — eine echte
 * SAD-Datei wurde also gar nicht als SAD erkannt. Und `sad_open()`
 * schloss aus der fehlenden Kennung auf „kopflos" und las die 22
 * Kopfbytes als NUTZDATEN.
 *
 * Die Offsets waren aus demselben Grund falsch: `hdr[4..6]` unterstellt
 * eine 4-Byte-Kennung.
 *
 * Gefunden nicht durch Lesen, sondern durch einen Differenzlauf —
 * `tests/test_sad_magic.c`. */
#define SAD_MAGIC     "Aley's disk backup"
#define SAD_MAGIC_LEN 18
#define SAD_HDR_LEN   22
#define SAD_OFF_HEADS 18
#define SAD_OFF_CYLS  19
#define SAD_OFF_SPT   20
#define SAD_OFF_SSDIV 21    /* Sektorgroesse / 64 */

typedef struct { FILE* file; bool header; } sad_data_t;

bool sad_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= SAD_MAGIC_LEN &&
        memcmp(data, SAD_MAGIC, SAD_MAGIC_LEN) == 0) {
        *confidence = 95; return true;
    }
    /* MF-729: war 70 — reine Groesse, ohne Merkmal. Der Magic-Zweig
     * darueber bleibt bei 95; nur die Groessen-Vermutung sinkt ins
     * Band 30..49. Gemessen war SAD damit der Sieger auf einem
     * Macintosh-800K-Nullpuffer, sobald D81 gesenkt war. */
    if (file_size == SAD_SIZE) { *confidence = 45; return true; }
    return false;
}

static uft_error_t sad_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[SAD_HDR_LEN];
    if (fread(hdr, 1, SAD_HDR_LEN, f) != SAD_HDR_LEN) { fclose(f); return UFT_ERROR_IO; }
    bool has_hdr = (memcmp(hdr, SAD_MAGIC, SAD_MAGIC_LEN) == 0);
    if (!has_hdr) {
        if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }
    }

    sad_data_t* p = calloc(1, sizeof(sad_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->header = has_hdr;

    disk->plugin_data = p;
    /* Mit Kopf: die vier Zahlen dahinter. Ohne Kopf: der Normalfall
     * einer SAM-Coupe-Diskette, 80 x 2 x 10 x 512 = 819 200. */
    disk->geometry.cylinders   = has_hdr ? hdr[SAD_OFF_CYLS]  : 80;
    disk->geometry.heads       = has_hdr ? hdr[SAD_OFF_HEADS] : 2;
    disk->geometry.sectors     = has_hdr ? hdr[SAD_OFF_SPT]   : 10;
    disk->geometry.sector_size = has_hdr
                               ? (uint16_t)(hdr[SAD_OFF_SSDIV] * 64) : 512;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders *
                                   disk->geometry.heads * disk->geometry.sectors;
    return UFT_OK;
}

static void sad_close(uft_disk_t* disk) {
    sad_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t sad_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    sad_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    /* MF-787: Kopflaenge und Sektorgroesse kommen aus dem KOPF, nicht aus
     * einer Zahl im Code. Vorher stand hier zweimal `22` und viermal
     * `512` — dieselbe Annahme wie bei der erfundenen Kennung, nur eine
     * Ebene tiefer: ein SAD mit anderer Sektorgroesse waere still falsch
     * gelesen worden. */
    const uint16_t ss = disk->geometry.sector_size;
    if (ss == 0 || ss > 1024) return UFT_ERROR_INVALID_STATE;
    long off = (long)((p->header ? SAD_HDR_LEN : 0) +
               ((size_t)cyl * disk->geometry.heads + head) *
               disk->geometry.sectors * ss);
    uint8_t buf[1024];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        if (fseek(p->file, off + (long)s * ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, ss, p->file) != ss) { memset(buf, 0xE5, ss); }
        uft_format_add_sector(track, (uint8_t)s, buf, ss, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t sad_write_track(uft_disk_t* disk, int cyl, int head,
                                    const uft_track_t* track) {
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

    sad_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    const uint16_t ss = disk->geometry.sector_size;
    if (ss == 0 || ss > 1024) return UFT_ERROR_INVALID_STATE;
    long off = (long)((p->header ? SAD_HDR_LEN : 0) +
               ((size_t)cyl * disk->geometry.heads + head) *
               disk->geometry.sectors * ss);
    for (size_t s = 0; s < track->sector_count && (int)s < disk->geometry.sectors; s++) {
        if (fseek(p->file, off + (long)s * ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[1024];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, ss); data = pad;
        }
        if (fwrite(data, 1, ss, p->file) != ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_sad_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_sad = {
    .name = "SAD", .description = "Sam Coupe", .extensions = "sad;mgt",
    .format = UFT_FORMAT_SAD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = sad_probe, .open = sad_open, .close = sad_close,
    .read_track = sad_read_track, .write_track = sad_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_sad_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_sad_features) / sizeof(uft_format_plugin_sad_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(sad)
