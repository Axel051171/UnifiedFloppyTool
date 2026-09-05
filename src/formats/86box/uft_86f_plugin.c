/**
 * @file uft_86f_plugin.c
 * @brief 86F (86Box/PCem) Plugin-B wrapper
 *
 * 86F is a track-level format used by 86Box and PCem emulators.
 * Magic: "86BX" at offset 0, followed by geometry and track data.
 *
 * MF-622: hier stand, dieses Plugin umhuelle die vorhandene
 * `uft_86f_probe()` / `uft_86f_read()`-API. Das war nie so. Die
 * Funktionen lagen in src/formats/86box/uft_86box.c, und dieses
 * Plugin hat sie NIE gerufen — nachgemessen: der einzige Treffer
 * im ganzen Baum war dieser Kommentar selbst.
 *
 * Die Datei war unerreichbar (Verwaisten-Grundlinie) und ist mit
 * MF-622 geloescht.
 *
 * MF-708 berichtigt den Schlusssatz. Hier stand, dieses Plugin trage
 * die 86F-Unterstuetzung „allein" — MF-622 hatte EINEN unerreichbaren
 * 86F-Leser geloescht und daraus geschlossen, es sei der letzte
 * gewesen. Gemessen gibt es einen zweiten: `src/formats/pc/uft_86f.c`,
 * 477 Zeilen, im Build (`.pro:3249`), nicht in der Registry, ohne
 * Aufrufer im ganzen Baum — und mit dem **richtigen** Magic `"86BF"`.
 *
 * Das oben angegebene `"86BX"` ist nicht das Erkennungsmerkmal von
 * 86F. Die Spezifikation von 86Box (`docs/dev/formats/86f.rst`) und
 * eine zweite, unabhaengige Implementierung (`Digitoxin1/DiskImageTool`,
 * `ImageFormats/86F/86FImage.vb:8`) nennen beide `"86BF"`; der feste
 * Kopf ist 8 Byte lang, nicht 32. Das Plugin weist damit jede echte
 * 86F-Datei ab und meldet dabei Read/Write/Flux als SUPPORTED.
 *
 * Warum das hier NICHT einzeilig berichtigt wird, und was stattdessen
 * zu tun ist: `tests/test_86f_spec_conformance.c` (der Rotbeweis) und
 * `docs/OPEN_ITEMS.md` FMT-16.
 */
#include "uft/uft_format_common.h"

#define F86_MAGIC       "86BX"
#define F86_MAGIC_LEN   4
#define F86_HDR_SIZE    32

typedef struct {
    uint8_t *data;
    size_t   size;
    uint8_t  tracks;
    uint8_t  sides;
    uint8_t  disk_type;
} f86_pd_t;

/* Geometry from disk type byte */
static void f86_geometry(uint8_t dtype, uint8_t *cyl, uint8_t *heads,
                          uint8_t *spt, uint16_t *ss) {
    switch (dtype) {
        case 0x00: *cyl = 40; *heads = 2; *spt = 9;  *ss = 512; break; /* 360K */
        case 0x01: *cyl = 80; *heads = 2; *spt = 9;  *ss = 512; break; /* 720K */
        case 0x02: *cyl = 80; *heads = 2; *spt = 15; *ss = 512; break; /* 1.2M */
        case 0x03: *cyl = 80; *heads = 2; *spt = 18; *ss = 512; break; /* 1.44M */
        case 0x04: *cyl = 80; *heads = 2; *spt = 36; *ss = 512; break; /* 2.88M */
        default:   *cyl = 80; *heads = 2; *spt = 18; *ss = 512; break;
    }
}

static bool f86_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < F86_HDR_SIZE) return false;
    if (memcmp(d, F86_MAGIC, F86_MAGIC_LEN) == 0) {
        *c = 98;
        return true;
    }
    return false;
}

static uft_error_t f86_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < F86_HDR_SIZE) { free(data); return UFT_ERROR_FILE_OPEN; }
    if (memcmp(data, F86_MAGIC, F86_MAGIC_LEN) != 0) {
        free(data); return UFT_ERROR_FORMAT_INVALID;
    }

    f86_pd_t *p = calloc(1, sizeof(f86_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;
    p->disk_type = data[6];
    p->tracks = data[8];
    p->sides = data[7];

    uint8_t cyl, heads, spt;
    uint16_t ss;
    f86_geometry(p->disk_type, &cyl, &heads, &spt, &ss);
    if (p->tracks > 0) cyl = p->tracks;
    if (p->sides > 0) heads = p->sides;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;
    return UFT_OK;
}

static void f86_close(uft_disk_t *disk) {
    f86_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t f86_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    f86_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* 86F track table starts at offset 32, each entry is 12 bytes:
     * offset(4LE) + length(4LE) + flags(1) + sectors(1) + rpm(2LE) */
    int idx = cyl * (p->sides > 0 ? p->sides : 2) + head;
    size_t entry_off = F86_HDR_SIZE + (size_t)idx * 12;
    if (entry_off + 12 > p->size) return UFT_OK;

    uint32_t trk_off = uft_read_le32(p->data + entry_off);
    uint32_t trk_len = uft_read_le32(p->data + entry_off + 4);
    uint8_t flags = p->data[entry_off + 8];
    uint8_t nsec = p->data[entry_off + 9];

    if (trk_off == 0 || trk_off + trk_len > p->size) return UFT_OK;
    if (!(flags & 0x01)) return UFT_OK; /* Track not valid */

    /* 86F sector data: parse MFM-encoded track or raw sectors
     * For simplicity, treat as raw sector data if nsec > 0 */
    if (nsec > 0 && trk_len >= (uint32_t)nsec * disk->geometry.sector_size) {
        size_t pos = trk_off;
        for (int s = 0; s < nsec && pos + disk->geometry.sector_size <= p->size; s++) {
            uft_format_add_sector(track, (uint8_t)s,
                                  p->data + pos, disk->geometry.sector_size,
                                  (uint8_t)cyl, (uint8_t)head);
            pos += disk->geometry.sector_size;
        }
    } else {
        /* Store raw track data as sector 0 */
        uint16_t chunk = (trk_len > 65535) ? 65535 : (uint16_t)trk_len;
        uft_format_add_sector(track, 0, p->data + trk_off, chunk,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t f86_write_track(uft_disk_t *disk, int cyl, int head,
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

    f86_pd_t *p = disk->plugin_data;
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
     * Betroffen war auch der Wandlungspfad: `uft_disk_convert.c:41` zaehlt
     * `tracks_converted++` bei `UFT_OK` und schreibt danach nichts hinaus.
     *
     * Warum kein echter Schreiber gebaut wurde: die EINFRIER-REGEL
     * (MF-363/498) verlangt benannte Referenz, gemessene Zahlen und die
     * Referenz im Header. Neun Container-Schreiber gegen diese Lage waeren
     * neun Wetten. Die Zusage wahr zu machen ist der kleinere und richtige
     * Schritt — dieselbe Entscheidung wie MF-880 (PRO).
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_86f_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_SUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_86f = {
    .name = "86F", .description = "86Box/PCem Floppy Image",
    .extensions = "86f", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe = f86_probe, .open = f86_open,
    .close = f86_close, .read_track = f86_read_track,
    .write_track = f86_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_86f_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_86f_features) / sizeof(uft_format_plugin_86f_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(86f)
