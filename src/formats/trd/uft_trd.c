/**
 * @file uft_trd.c
 * @brief ZX Spectrum TRD format (TR-DOS) — read + write
 */
#include "uft/uft_format_common.h"
#include "uft/uft_format_probe.h"    /* MF-1231: uft_format_variant_t */

#define TRD_SEC_SIZE 256
#define TRD_SPT 16

typedef struct { FILE* file; uint8_t tracks, sides; } trd_data_t;

bool trd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size != 655360 && file_size != 327680 && file_size != 163840)
        return false;
    *confidence = 45;  /* MF-729: nur die Groesse */

    /* TR-DOS disk info at track 0, sector 9 (offset 0x800) */
    if (size >= 0x228) {
        /* Byte 0x227 = disk type: 0x10 = TR-DOS */
        if (data[0x227] == 0x10) *confidence = 92;
        /* File count (0x8E4 = track 0 sector 8 + 0xE4) should be 0-128 */
        /* MF-729: hier stand `else if (data[0x8E4] <= 128) *confidence = 82;`
         * — eine Bereichspruefung, die auf die HAELFTE aller Bytewerte
         * zutrifft und auf Nullen immer. Sie hob 70 auf 82, ohne etwas
         * erkannt zu haben, und liess ein PC-160K-Abbild gegen TRD
         * verlieren. Eine Strukturpruefung, die Zufall zur Haelfte
         * durchlaesst, ist keine (Eichung 2). Entfernt statt gesenkt:
         * gesenkt waere sie von der reinen Groesse nicht mehr zu
         * unterscheiden. */
    }
    return true;
}

static uft_error_t trd_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    trd_data_t* p = calloc(1, sizeof(trd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    if (sz == 655360)      { p->tracks = 80; p->sides = 2; }
    else if (sz == 327680) { p->tracks = 80; p->sides = 1; }
    else                   { p->tracks = 40; p->sides = 1; }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = TRD_SPT;
    disk->geometry.sector_size = TRD_SEC_SIZE;
    disk->geometry.total_sectors = (uint32_t)p->tracks * p->sides * TRD_SPT;
    return UFT_OK;
}

static void trd_close(uft_disk_t* disk) {
    trd_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t trd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    trd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    long off = (long)((cyl * p->sides + head) * TRD_SPT * TRD_SEC_SIZE);
    uint8_t buf[TRD_SEC_SIZE];
    for (int s = 0; s < TRD_SPT; s++) {
        if (fseek(p->file, off + s * TRD_SEC_SIZE, SEEK_SET) != 0) return UFT_ERROR_IO;
        /* MF-980: der kurze Lesevorgang wird GEMERKT, nicht nur gefuellt.
         *
         * Hier stand `if (fread(...) != N) memset(buf, 0xE5, N);` und
         * danach der unveraenderte `add_sector`. Der legt jeden Sektor
         * mit `status = UFT_SECTOR_OK` und „CRC gueltig" an — die
         * Fuellung war damit von echten 0xE5-Daten nicht zu
         * unterscheiden. `trd_open()` nimmt jede Dateigroesse an, der
         * Fall entsteht also bei jedem abgebrochenen Abzug.
         *
         * Die Bytes bleiben stehen („Kein Bit verloren"); sie gelten nur
         * nicht mehr als Messwert. */
        const bool kurz =
            (fread(buf, 1, TRD_SEC_SIZE, p->file) != TRD_SEC_SIZE);
        if (kurz) memset(buf, 0xE5, TRD_SEC_SIZE);
        uft_format_add_sector(track, (uint8_t)s, buf, TRD_SEC_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
        if (kurz) uft_format_mark_last_missing(track);
    }
    return UFT_OK;
}

static uft_error_t trd_write_track(uft_disk_t* disk, int cyl, int head,
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

    trd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)((cyl * p->sides + head) * TRD_SPT * TRD_SEC_SIZE);
    for (size_t s = 0; s < track->sector_count && s < TRD_SPT; s++) {
        if (fseek(p->file, off + (long)s * TRD_SEC_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        size_t len = track->sectors[s].data_len;
        uint8_t pad[TRD_SEC_SIZE];
        if (!data || len == 0) {
            memset(pad, 0xE5, TRD_SEC_SIZE);
            data = pad; len = TRD_SEC_SIZE;
        } else if (len < TRD_SEC_SIZE) {
            memset(pad, 0xE5, TRD_SEC_SIZE);
            memcpy(pad, data, len);
            data = pad; len = TRD_SEC_SIZE;
        }
        if (fwrite(data, 1, TRD_SEC_SIZE, p->file) != TRD_SEC_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_trd_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

/* ── Die drei Groessen, die dieses Plugin kennt (MF-1231) ────────────
 *
 * Sie stehen nicht neu hier, sondern an zwei Stellen in dieser Datei:
 * `trd_probe` weist alles ab, was nicht 655 360, 327 680 oder 163 840
 * Byte hat (Z. 14), und `trd_open` leitet daraus Spur- und Seitenzahl
 * ab (Z. 46-48). Diese Tafel faltet dieselben Zahlen in die Gestalt,
 * die die Oberflaeche beim Speichern braucht.
 *
 * Die Oberflaeche nannte bis MF-1231 nur ZWEI davon
 * (`m_formatInfo["TRD"]`: "DS/DD 640K", "SS/DD 320K") — die
 * 40-Spur-Diskette mit 163 840 Byte fehlte dort, obwohl das Plugin sie
 * seit jeher oeffnet.
 *
 * `can_write` ist fuer alle drei wahr: `trd_write_track` rechnet seinen
 * Versatz aus `p->sides`, das beim Oeffnen aus der Dateigroesse kommt,
 * und kennt keine Sonderbehandlung je Groesse.
 *
 * Schreibvorgabe ist die doppelseitige 640K-Diskette — die gewoehnliche
 * TR-DOS-Diskette und die einzige der drei, die `trd_open` mit zwei
 * Seiten fuehrt. */
static const uft_format_variant_t trd_variants[] = {
    { .name = "DS/DD 640K", .description = "TR-DOS 80 Spuren, 2 Seiten",
      .base_format = UFT_FORMAT_TRD,
      .min_size = 655360, .max_size = 655360, .exact_sizes = { 655360 },
      .cylinders = 80, .heads = 2,
      .sectors_min = TRD_SPT, .sectors_max = TRD_SPT,
      .sector_size = TRD_SEC_SIZE, .validate = NULL,
      .can_read = true, .can_write = true, .write_note = NULL,
      .is_write_default = true },
    { .name = "SS/DD 320K", .description = "TR-DOS 80 Spuren, 1 Seite",
      .base_format = UFT_FORMAT_TRD,
      .min_size = 327680, .max_size = 327680, .exact_sizes = { 327680 },
      .cylinders = 80, .heads = 1,
      .sectors_min = TRD_SPT, .sectors_max = TRD_SPT,
      .sector_size = TRD_SEC_SIZE, .validate = NULL,
      .can_read = true, .can_write = true, .write_note = NULL,
      .is_write_default = false },
    { .name = "SS/DD 160K", .description = "TR-DOS 40 Spuren, 1 Seite",
      .base_format = UFT_FORMAT_TRD,
      .min_size = 163840, .max_size = 163840, .exact_sizes = { 163840 },
      .cylinders = 40, .heads = 1,
      .sectors_min = TRD_SPT, .sectors_max = TRD_SPT,
      .sector_size = TRD_SEC_SIZE, .validate = NULL,
      .can_read = true, .can_write = true, .write_note = NULL,
      .is_write_default = false },
};

const uft_format_plugin_t uft_format_plugin_trd = {
    .name = "TRD", .description = "TR-DOS Spectrum", .extensions = "trd",
    .format = UFT_FORMAT_TRD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = trd_probe, .open = trd_open, .close = trd_close,
    .read_track = trd_read_track, .write_track = trd_write_track,
    .variants = trd_variants,
    .variant_count = sizeof(trd_variants) / sizeof(trd_variants[0]),
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_trd_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_trd_features) / sizeof(uft_format_plugin_trd_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(trd)
