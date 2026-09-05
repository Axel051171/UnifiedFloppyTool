/**
 * @file uft_pro_plugin.c
 * @brief PRO (Atari 8-bit Protected) Plugin-B
 *
 * PRO stores Atari disk sectors with per-sector status flags
 * (phantom sectors, weak bits, CRC errors) for copy protection.
 *
 * Magic: "APRO" (0x4F525041 LE) or "KPRO" (0x4F52504B LE)
 * Layout: 16-byte header + sector entries (4 bytes each) + sector data
 */
#include "uft/uft_format_common.h"

#define PRO_MAGIC_APRO  0x4F525041u
#define PRO_MAGIC_KPRO  0x4F52504Bu
#define PRO_HEADER_SIZE 16
#define PRO_SECTOR_SIZE 128
#define PRO_MAX_TRACKS  77
#define PRO_MAX_SPT     26

typedef struct {
    uint8_t *file_data;
    size_t   file_size;
    uint8_t  tracks;
    uint8_t  spt;
} pro_pd_t;

static bool pro_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < PRO_HEADER_SIZE) return false;
    uint32_t sig = uft_read_le32(data);
    if (sig == PRO_MAGIC_APRO || sig == PRO_MAGIC_KPRO) {
        *confidence = 96;
        return true;
    }
    return false;
}

static uft_error_t pro_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);
    if (!raw || raw_size < PRO_HEADER_SIZE) { free(raw); return UFT_ERROR_FORMAT_INVALID; }

    uint32_t sig = uft_read_le32(raw);
    if (sig != PRO_MAGIC_APRO && sig != PRO_MAGIC_KPRO) { free(raw); return UFT_ERROR_FORMAT_INVALID; }

    pro_pd_t *p = calloc(1, sizeof(pro_pd_t));
    if (!p) { free(raw); return UFT_ERROR_NO_MEMORY; }
    p->file_data = raw;
    p->file_size = raw_size;

    /* Tracks and SPT from header bytes 4-5 */
    p->tracks = raw[7];
    p->spt = raw[6];
    if (p->tracks == 0 || p->tracks > PRO_MAX_TRACKS) p->tracks = 40;
    if (p->spt == 0 || p->spt > PRO_MAX_SPT) p->spt = 18;

    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = 1;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = PRO_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)p->tracks * p->spt;
    return UFT_OK;
}

static void pro_close(uft_disk_t *disk) {
    pro_pd_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t pro_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    pro_pd_t *p = disk->plugin_data;
    if (!p || !p->file_data || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    uint32_t total_secs = (uint32_t)p->tracks * p->spt;
    /* Sector table starts at offset 16, 4 bytes per entry */
    size_t table_size = total_secs * 4;
    size_t data_start = PRO_HEADER_SIZE + table_size;

    for (int s = 0; s < p->spt; s++) {
        uint32_t sec_idx = (uint32_t)cyl * p->spt + s;
        if (sec_idx >= total_secs) break;

        /* Sector entry: 2 bytes offset + 1 byte flags + 1 byte reserved */
        size_t entry_off = PRO_HEADER_SIZE + (size_t)sec_idx * 4;
        if (entry_off + 4 > p->file_size) break;

        uint8_t flags = p->file_data[entry_off + 2];
        size_t sec_data = data_start + (size_t)sec_idx * PRO_SECTOR_SIZE;
        if (sec_data + PRO_SECTOR_SIZE > p->file_size) break;

        uft_format_add_sector(track, (uint8_t)s,
                              p->file_data + sec_data, PRO_SECTOR_SIZE,
                              (uint8_t)cyl, 0);

        /* Mark special sectors */
        if (track->sector_count > 0) {
            if (flags & 0x02) track->sectors[track->sector_count - 1].weak = true;
            if (flags & 0x04) track->sectors[track->sector_count - 1].crc_ok = false;
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
    { "Weak Bits", UFT_FEATURE_SUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_pro = {
    .name = "PRO", .description = "Atari 8-bit Protected (APE Pro)",
    .extensions = "pro;atx", .format = UFT_FORMAT_DSK,
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
