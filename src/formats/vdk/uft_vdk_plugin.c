/**
 * @file uft_vdk_plugin.c
 * @brief VDK (Dragon 32/64 und Tandy CoCo) Plugin-B
 *
 * VDK has a 12-byte header followed by raw sector data.
 * Header: magic(2) + header_size(2) + version(1) + compat(1) +
 *         source_id(1) + source_ver(1) + tracks(1) + sides(1) +
 *         flags(1) + compression(1)
 *
 * ── Referenz (EINFRIER-REGEL MF-363/498, Bedingung c) ─────────────────
 *
 * **MAME `formats/vdk_dsk.cpp`** (BSD-3-Clause, Dirk Best), in
 * `neue-ideen/formats.zip`. Woertlich:
 *
 *     uint8_t header[0x100];
 *     read(io, header, 0x100);
 *     int const header_size = header[3] * 0x100 + header[2];
 *     int const track_count = header[8];
 *     int const head_count  = header[9];
 *     io.seek(header_size, SEEK_SET);
 *     ... SECTOR_COUNT = 18, SECTOR_SIZE = 256, FIRST_SECTOR_ID = 1
 *
 * und aus `save()`: `header[2] = sizeof(header) % 0x100;`
 * `header[3] = sizeof(header) / 0x100;` bei `uint8_t header[12]`.
 *
 * **MAME liest 0x100 Byte Kopf, BEVOR es die Kopfgroesse benutzt** —
 * ein Kopf bis 256 Byte ist im Format also vorgesehen (VDK traegt dort
 * unter anderem den Diskettennamen).
 *
 * ── Befund 1 (MF-1018): die Kopfgroesse lag in einem `uint8_t` ────────
 *
 *     typedef struct { FILE *file; uint8_t hdr_size, tracks, sides; }
 *     ...
 *     p->hdr_size = uft_read_le16(hdr + 2);
 *
 * Ein 16-Bit-Wert in einem 8-Bit-Feld. Bei einem Kopf von **exakt
 * 0x0100 Byte** wird daraus **0**, und der Leser beginnt bei Versatz 0.
 *
 * Gemessen an einer Datei mit 256-Byte-Kopf, deren Spuren und Sektoren
 * sich selbst benennen:
 *
 *     Sektor-ID 1, Byte 0 = 64, Byte 1 = 6B
 *     Orakel   : ID 1, Byte 0 = 40, Byte 1 = 01
 *
 * `64 6B` ist `'d' 'k'` — **die eigene Kennung der Datei wird als
 * Sektordaten ausgegeben.** Klasse MF-796/MF-1017: ein Versatz, der
 * still falsche Bytes liefert.
 *
 * ── Befund 2 (MF-1018): eine Geometrie, die die Datei verneint ────────
 *
 *     if (p->tracks == 0) p->tracks = 35;
 *     if (p->sides == 0)  p->sides = 1;
 *
 * Steht im Kopf eine 0, sagt die Datei „keine Spuren". Gemessen wurden
 * daraus **35 Zylinder** und 18 Sektoren je Spur; die Sektoren selbst
 * waren dank MF-980 ehrlich als fehlend gekennzeichnet, aber die
 * ZAHL war erfunden. MAME nimmt `header[8]`/`header[9]` woertlich.
 *
 * ── Befund 3 (MF-1018): das Kompressionsbyte wurde ignoriert ──────────
 *
 * `hdr[11]` sagt, ob die Sektordaten gepackt sind. Ist es gesetzt, sind
 * die Bytes hinter dem Kopf **kein** Sektorabzug — gemessen las UFT sie
 * trotzdem als solchen und meldete Erfolg.
 *
 * **Hier ist das Orakel ebenfalls stumm:** MAMEs `load()` prueft das
 * Byte nicht. Abgesagt wird es hier trotzdem, denn „Keine erfundenen
 * Daten" wiegt schwerer als Gleichlauf mit dem Orakel — und ein
 * gepackter Datenbereich als Sektoren gelesen ist genau das.
 *
 * ── Was gemessen wurde und STIMMT ─────────────────────────────────────
 *
 * Die Sektor-IDs. `uft_format_add_sector(track, s, ...)` mit `s = 0..17`
 * ergibt nach der `+1` dieser Funktion **1..18** — und MAME setzt
 * `FIRST_SECTOR_ID = 1`. Anders als bei `jv1` (MF-1016) ist das hier
 * also richtig, und es bleibt deshalb unangetastet.
 */
#include "uft/uft_format_common.h"

#define VDK_MAGIC   0x6B64  /* "dk" LE */
#define VDK_HDR     12
#define VDK_SS      256
#define VDK_SPT     18

/* MF-1018, Befund 1: `hdr_size` war ein `uint8_t` und nahm einen
 * LE16-Wert auf. Ein Kopf von 0x0100 Byte wurde damit zu 0. */
typedef struct { FILE *file; uint16_t hdr_size; uint8_t tracks, sides; } vdk_pd_t;

static bool vdk_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < VDK_HDR) return false;
    if (uft_read_le16(d) == VDK_MAGIC) { *c = 92; return true; }
    return false;
}

static uft_error_t vdk_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    uint8_t hdr[VDK_HDR];
    if (fread(hdr, 1, VDK_HDR, f) != VDK_HDR) { fclose(f); return UFT_ERROR_IO; }
    if (uft_read_le16(hdr) != VDK_MAGIC) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    uint16_t hdr_size = uft_read_le16(hdr + 2);
    uint8_t tracks = hdr[8];
    uint8_t sides = hdr[9];
    uint8_t kompression = hdr[11];

    /* MF-1018, Befund 1: die Kopfgroesse ist 16 Bit. Kleiner als die
     * 12 Byte, die MAMEs `save()` selbst schreibt, kann sie nicht
     * sein — dann waere der Kopf kuerzer als seine eigenen Felder. */
    if (hdr_size < VDK_HDR) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    /* MF-1018, Befund 2: eine 0 im Kopf heisst „keine Spuren", nicht
     * „nimm 35 an". MAME nimmt `header[8]`/`header[9]` woertlich. */
    if (tracks == 0 || sides == 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* MF-1018, Befund 3: ist der Datenbereich gepackt, sind die Bytes
     * hinter dem Kopf kein Sektorabzug. MAME prueft das Byte nicht —
     * hier wird trotzdem abgesagt, weil gepackte Daten als Sektoren
     * gelesen erfundene Daten sind. */
    if (kompression != 0) { fclose(f); return UFT_ERROR_NOT_SUPPORTED; }

    vdk_pd_t *p = calloc(1, sizeof(vdk_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->hdr_size = hdr_size;
    p->tracks = tracks;
    p->sides = sides;

    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks; disk->geometry.heads = p->sides;
    disk->geometry.sectors = VDK_SPT; disk->geometry.sector_size = VDK_SS;
    disk->geometry.total_sectors = (uint32_t)p->tracks * p->sides * VDK_SPT;
    return UFT_OK;
}

static void vdk_close(uft_disk_t *disk) {
    vdk_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t vdk_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    vdk_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    /* MF-1018: die obere Schranke fehlte ganz — eine Spur jenseits der
     * Geometrie lieferte 18 als fehlend gekennzeichnete Sektoren statt
     * einer Absage. */
    if (cyl >= p->tracks || head >= p->sides) return UFT_ERROR_INVALID_PARAM;
    uft_track_init(track, cyl, head);
    long off = (long)(p->hdr_size + ((size_t)cyl * p->sides + head) * VDK_SPT * VDK_SS);
    uint8_t buf[VDK_SS];
    for (int s = 0; s < VDK_SPT; s++) {
        if (fseek(p->file, off + s * VDK_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        /* MF-980: der kurze Lesevorgang wird GEMERKT, nicht nur
         * gefuellt. `uft_format_add_sector*()` legt jeden Sektor mit
         * `status = UFT_SECTOR_OK` und „CRC gueltig" an — die Fuellung
         * war damit von echten Daten nicht zu unterscheiden. Die Bytes
         * bleiben stehen, sie gelten nur nicht mehr als Messwert. */
        const bool kurz = (fread(buf, 1, VDK_SS, p->file) != VDK_SS);
        if (kurz) memset(buf, 0xE5, VDK_SS);
        uft_format_add_sector(track, (uint8_t)s, buf, VDK_SS, (uint8_t)cyl, (uint8_t)head);
        if (kurz) uft_format_mark_last_missing(track);
    }
    return UFT_OK;
}

static uft_error_t vdk_write_track(uft_disk_t *disk, int cyl, int head,
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

    vdk_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* MF-1018: beim Schreiben fehlte die obere Schranke ebenfalls, und
     * dort waere sie schwerer gewogen — ein `fwrite` jenseits des
     * Dateiendes VERLAENGERT die Datei. Das ist der fuenfte Fall von
     * „Leseseite geholt, Schreibseite uebersehen" nach
     * MF-519/529/931/1016; hier fehlte sie auf BEIDEN Seiten. */
    if (cyl >= p->tracks || head >= p->sides) return UFT_ERROR_INVALID_PARAM;
    long off = (long)(p->hdr_size + ((size_t)cyl * p->sides + head) * VDK_SPT * VDK_SS);
    for (size_t s = 0; s < track->sector_count && (int)s < VDK_SPT; s++) {
        if (fseek(p->file, off + (long)s * VDK_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[VDK_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, VDK_SS); data = pad;
        }
        if (fwrite(data, 1, VDK_SS, p->file) != VDK_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_vdk_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_vdk = {
    .name = "VDK", .description = "Tandy CoCo Virtual Disk",
    .extensions = "vdk;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = vdk_probe, .open = vdk_open, .close = vdk_close,
    .read_track = vdk_read_track, .write_track = vdk_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_vdk_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_vdk_features) / sizeof(uft_format_plugin_vdk_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(vdk)
