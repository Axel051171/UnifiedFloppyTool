/**
 * @file uft_jv1.c
 * @brief JV1 (TRS-80) — kopfloses Sektorabbild, EINSEITIG
 *
 * ── Referenzen (EINFRIER-REGEL MF-363/498, Bedingung c) ───────────────
 *
 * **Tim Mann, „Common File Formats for Emulated TRS-80 Floppy Disks"**
 * (https://www.tim-mann.org/trs80/dskspec.html, abgerufen 2026-09-10) —
 * die Quelle, die dieser Kopf schon vorher nannte. Woertlich:
 *
 *   „There are 10 sectors per track (i.e., single density), numbered
 *    **0 through 9**, and **only one side**."
 *   „Byte 0 of the file is byte 0 of track 0, sector 0; byte 256 is
 *    byte 0 of track 0, sector 1."
 *   „All sectors on track 17 are formatted with the nonstandard data
 *    address mark 0xFA, indicating a TRSDOS 2.3 directory."
 *
 * **MAME `formats/trs80_dsk.cpp`** (BSD-3-Clause, Dirk Best), in
 * `neue-ideen/formats.zip`. Seine Tafel fuehrt drei Fassungen — 35, 40
 * und 80 Spuren — und **alle drei mit `head_count = 1`**:
 *
 *     { FF_525, SSSD, FM, 4000, 10, 35, 1, 256, {}, 0, {}, 14, 11, 12 }
 *     { FF_525, SSSD, FM, 4000, 10, 40, 1, 256, {}, 0, {}, 14, 11, 12 }
 *     { FF_525, SSQD, FM, 4000, 10, 80, 1, 256, {}, 0, {}, 14, 11, 12 }
 *
 * Das `0` an zehnter Stelle ist `sector_base_id`: **Sektoren ab 0.**
 *
 * ── Befund 1 (MF-1016): >40 Spuren wurden zweiseitig gelesen ──────────
 *
 * `jv1_detect_geometry()` nahm bei mehr als 40 Spuren eine zweite Seite
 * an:
 *
 *     if (total_tracks <= 80 && total_tracks % 2 == 0) {
 *         *cyl = total_tracks / 2;  *heads = 2;  return true; }
 *
 * Gemessen an einer Datei von **204 800 Byte** (80 x 10 x 256), deren
 * jede Spur mit ihrer eigenen Nummer gefuellt war:
 *
 *     Geometrie  : 40 Zyl, 2 Koepfe        (Orakel: 80 Zyl, 1 Kopf)
 *     Zyl 40/K 0 : nicht lesbar — bei 40 Zylindern gibt es ihn nicht
 *
 * Die Spuren 40..79 lagen damit auf **Kopf 1** statt auf den Zylindern
 * 40..79, und ein Aufrufer, der eine einseitige Diskette erwartet, fand
 * die halbe Diskette nicht. JV1 hat **keine zweite Seite** — beide
 * Referenzen sagen es, und die Formatdokumentation woertlich.
 *
 * ── Befund 2 (MF-1016): die Sektornummern waren 1..10 statt 0..9 ──────
 *
 * Der alte Rumpf rief `uft_format_add_sector(track, s, ...)` mit
 * `s = 0..9`. Diese Funktion nimmt laut ihrem eigenen Kopf einen
 * **0-basierten Laufindex und addiert 1** — richtig fuer IBM-PC
 * (1..N), falsch fuer alles, was ab 0 zaehlt. Der Kommentar dort nennt
 * Apple, Amiga und Commodore ausdruecklich; TRS-80 gehoert dazu.
 *
 * Jetzt `uft_format_add_sector_with_id()` mit der Nummer, wie sie auf
 * der Diskette steht.
 *
 * ── Eine Divergenz zwischen den beiden Referenzen, benannt ────────────
 *
 * Fuer Spur 17 nennt Tim Mann die Adressmarke **`0xFA`**, MAME setzt
 * `FM_DDAM` (das ist **`0xF8`**, „deleted"):
 *
 *     int jv1_format::get_track_dam_fm(...) const
 *     { return (track == 17 && head == 0) ? FM_DDAM : FM_DAM; }
 *
 * Beide sind sich einig, dass Spur 17 eine **nichtstandardmaessige**
 * Marke traegt — welche, sagen sie verschieden. **Entschieden wird das
 * hier nicht**, denn in einer JV1-Datei steht ueberhaupt keine
 * Adressmarke: sie ist ein reiner Sektorabzug. Wer JV1 je in einen
 * Fluss- oder Bitstrom schreibt, muss die Frage klaeren; das steht als
 * offener Punkt, nicht als Zusage.
 */

#include "uft/uft_format_common.h"

#define JV1_SECTOR_SIZE     256
#define JV1_SPT             10
#define JV1_TRACK_SIZE      (JV1_SPT * JV1_SECTOR_SIZE)     /* 2560 */
/* MF-1016: 80 ist die groesste Geometrie, die MAMEs Tafel WIRKLICH
 * umsetzt (SSQD 80/1). Sein Beschreibungstext sagt daneben „There's no
 * limit on the number of tracks. It's up to the emulation to decide how
 * to represent things" — zwei Aussagen aus derselben Quelle. Genommen
 * wird die umgesetzte, nicht die weitere: eine Grenze, die ein Orakel
 * belegt, ist besser als eine, die nur die Feldbreite vorgibt. */
#define JV1_MAX_TRACKS      80

typedef struct {
    FILE*       file;
    uint8_t     cylinders;
} jv1_data_t;

/* MF-1016: nur noch einseitig. Angenommen wird jede volle Spurzahl bis
 * zur groessten Geometrie, die das Orakel umsetzt (80) — aber
 * niemals eine zweite Seite erfunden. */
static bool jv1_detect_geometry(size_t file_size, uint8_t *cyl)
{
    if (file_size == 0 || file_size % JV1_TRACK_SIZE != 0)
        return false;

    size_t total_tracks = file_size / JV1_TRACK_SIZE;
    if (total_tracks > JV1_MAX_TRACKS)
        return false;
    /* 41 Spuren waren vorher ABGEWIESEN — „only valid when even (two
     * heads)", so stand es sogar in `tests/test_plugin_probe_real.c`.
     * Diese Regel folgte aus der zweiten Seite, die es nicht gibt. Ein
     * gruener Test hat damit den Fehler BEWACHT; dieselbe Gestalt wie
     * die zehn gruenen Tests aus MF-992, die durch dieselbe falsche
     * Struktur zurueckgelesen haben. */

    *cyl = (uint8_t)total_tracks;
    return true;
}

bool jv1_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)data; (void)size;
    uint8_t cyl;
    if (!jv1_detect_geometry(file_size, &cyl)) return false;
    *confidence = 35;  /* Low — no magic, size-only detection */
    return true;
}

static uft_error_t jv1_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl;
    if (!jv1_detect_geometry((size_t)fs, &cyl)) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    jv1_data_t *pdata = calloc(1, sizeof(jv1_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->cylinders = cyl;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = 1;           /* MF-1016: JV1 ist einseitig */
    disk->geometry.sectors = JV1_SPT;
    disk->geometry.sector_size = JV1_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * JV1_SPT;
    return UFT_OK;
}

static void jv1_close(uft_disk_t *disk)
{
    jv1_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t jv1_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    jv1_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= p->cylinders || head != 0) return UFT_ERROR_INVALID_PARAM;

    uft_track_init(track, cyl, head);

    /* Tim Mann: „Byte 0 of the file is byte 0 of track 0, sector 0" —
     * die Spuren liegen einfach hintereinander, es gibt keine Seite. */
    long offset = (long)cyl * JV1_TRACK_SIZE;
    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[JV1_SECTOR_SIZE];
    for (int s = 0; s < JV1_SPT; s++) {
        if (fread(buf, 1, JV1_SECTOR_SIZE, p->file) != JV1_SECTOR_SIZE)
            return UFT_ERROR_IO;
        /* MF-1016: Sektoren 0..9, wie auf der Diskette. `add_sector`
         * ohne `_with_id` addiert 1 und ergab 1..10. */
        uft_format_add_sector_with_id(track, (uint8_t)s, buf,
                                      JV1_SECTOR_SIZE,
                                      (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t jv1_write_track(uft_disk_t *disk, int cyl, int head,
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

    jv1_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* MF-1016: dieselbe Schranke wie beim Lesen — vorher fehlte sie
     * ganz, und `head` ging in die Versatzrechnung ein. Das ist der
     * vierte Fall von „Leseseite geholt, Schreibseite uebersehen" nach
     * MF-519/529/931. */
    if (cyl >= p->cylinders || head != 0) return UFT_ERROR_INVALID_PARAM;

    long offset = (long)cyl * JV1_TRACK_SIZE;
    for (size_t s = 0; s < track->sector_count && (int)s < JV1_SPT; s++) {
        if (fseek(p->file, offset + (long)s * JV1_SECTOR_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[JV1_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            /* Ein kopfloser Sektorabzug hat keine Loecher: fuer einen
             * Sektor, den der Aufrufer nicht mitgibt, MUSS etwas in der
             * Datei stehen. `0xE5` ist die uebliche Formatierfuellung
             * und hier eine benannte Wahl, keine Messung — sie steht
             * nur auf dem SCHREIB-Weg, wo der Aufrufer die Luecke
             * gelassen hat. Auf dem Leseweg wird nichts erfunden
             * (Tor 62). */
            memset(pad, 0xE5, JV1_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, JV1_SECTOR_SIZE, p->file) != JV1_SECTOR_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_jv1_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_jv1 = {
    .name = "JV1", .description = "TRS-80 JV1 (Jeff Vavasour)",
    .extensions = "jv1;dsk", .format = UFT_FORMAT_JV1,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = jv1_probe, .open = jv1_open, .close = jv1_close,
    .read_track = jv1_read_track, .write_track = jv1_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* MF-1016: Tim Manns
                                             * Formatbeschreibung liegt vor
                                             * (vorher DERIVED) */
    .features = uft_format_plugin_jv1_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_jv1_features) / sizeof(uft_format_plugin_jv1_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(jv1)
