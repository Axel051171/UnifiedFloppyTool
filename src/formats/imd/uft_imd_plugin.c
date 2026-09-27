/**
 * @file uft_imd_plugin.c
 * @brief IMD (ImageDisk) Plugin-B wrapper
 *
 * IMD: ASCII header terminated by 0x1A, followed by track records.
 * Each track: mode(1) + cyl(1) + head(1) + nsec(1) + size_code(1)
 *           + sector_map(nsec) + optional cyl/head maps + sector data
 *
 * Sector data types: 0=unavail, 1=normal, 2=compressed(fill byte),
 *                    3=deleted, 4=del+comp, 5=error, 6=err+comp, 7=del+err, 8=del+err+comp
 *
 * Sector IDs (P0-18, MF-1348). Reference: MAME src/lib/formats/imd_dsk.cpp
 * (BSD-3-Clause, read only), imd_format::load():
 *     sects[i].track  = tnum.size() ? tnum[i] : track;   cylinder map, 0x80
 *     sects[i].head   = hnum.size() ? hnum[i] : head;    head map, 0x40
 *     sects[i].sector = snum[i];                         numbering map
 *     stype 0 -> no data
 * Until MF-1348 this plugin passed snum[i] to uft_format_add_sector(),
 * which takes a 0-based index and adds 1: every ID of every IMD file came
 * out one too high (the foreign hxcfe_pc160.imd, map 1..8, read as 2..9),
 * the cylinder/head maps were skipped, and "data unavailable" came out as
 * a good sector. Guarded by tests/test_imd_sektor_ids.c.
 *
 * NOT covered here (named): a truncated file loses its trailing sectors
 * without a marker, and a size code > 6 is read as 512 (MAME: 8192).
 * BERICHTIGT MF-1473: the second half no longer holds. open() rejects a
 * size code > 6 (and 0xFF), a head > 1, a mode > 5 and a sector record
 * type > 8, after Dave Dunfield's ImageDisk 1.20 source (neue-ideen/
 * floppy1/IMDSRC.ZIP: IMD.SRC |MODE| |SSIZE| |SNOTE| |HNOTE| and the
 * sector-record list, IMDU.C's range checks; COPY.TXT: free for any
 * reasonable purpose, attribution requested). Guarded by
 * tests/test_imd_ausser_spec_abgewiesen.c. The first half (truncation)
 * was closed by MF-1371.
 */
#include "uft/uft_format_common.h"

#define IMD_SIG "IMD "

typedef struct {
    uint8_t *data;
    size_t   size;
    uint8_t  max_cyl;
    uint8_t  max_head;
    uint8_t  max_spt;
    uint16_t sec_size;
} imd_pd_t;

static uint16_t imd_sec_size(uint8_t code) {
    if (code > 6) return 512;
    return (uint16_t)(128 << code);
}

static bool imd_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 4) return false;
    if (memcmp(data, IMD_SIG, 4) == 0) { *confidence = 95; return true; }
    return false;
}

/* Find end of IMD comment (0x1A byte) */
static size_t imd_skip_comment(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        if (data[i] == 0x1A) return i + 1;
    return size;
}

/* Scan geometry without extracting sector data.
 *
 * MF-1473: every track record is checked against Dunfield's value ranges
 * (ImageDisk 1.20, IMD.SRC |MODE| 00..05, |SSIZE| 00..06, |HNOTE| "HEAD
 * can only be 0 or 1", sector records 00..08; IMDU.C stops on the first
 * three with "... out of range"). A value outside them is rejected: the
 * record layout after it is unknown, and reading on invented geometry and
 * sectors (size code 7 read as 512 gave 66x3x67 from a one-sector file).
 * 0xFF, the per-sector size table, is only a SUGGESTED extension that
 * ImageDisk itself does not handle (|SNOTE|) — rejected as well (MF-1384:
 * the plugin does not carry mixed sizes). */
static bool imd_scan_geometry(const uint8_t *data, size_t size, size_t pos,
                               uint8_t *max_cyl, uint8_t *max_head,
                               uint8_t *max_spt, uint16_t *sec_size) {
    *max_cyl = 0; *max_head = 0; *max_spt = 0; *sec_size = 512;
    while (pos + 5 <= size) {
        uint8_t mode = data[pos];
        uint8_t cyl = data[pos + 1];
        uint8_t head_raw = data[pos + 2];
        uint8_t head = head_raw & 0x0F;
        uint8_t nsec = data[pos + 3];
        uint8_t scode = data[pos + 4];
        if (mode > 5 || head > 1 || scode > 6) return false;
        uint16_t ss = imd_sec_size(scode);

        if (cyl > *max_cyl) *max_cyl = cyl;
        if (head > *max_head) *max_head = head;
        if (nsec > *max_spt) *max_spt = nsec;
        *sec_size = ss;

        pos += 5;
        /* sector numbering map */
        pos += nsec;
        /* optional cylinder map */
        if (head_raw & 0x80) pos += nsec;
        /* optional head map */
        if (head_raw & 0x40) pos += nsec;
        /* sector data */
        for (int s = 0; s < nsec && pos < size; s++) {
            uint8_t dtype = data[pos++];
            if (dtype > 8) return false;
            switch (dtype) {
                case 0: break; /* unavailable */
                case 2: case 4: case 6: case 8:
                    pos += 1; break; /* compressed: 1 fill byte */
                default:
                    pos += ss; break; /* normal/deleted/error: full data */
            }
        }
    }
    return true;
}

static uft_error_t imd_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *data = uft_read_file(path, &file_size);
    if (!data || file_size < 4) { free(data); return UFT_ERROR_FILE_OPEN; }
    if (memcmp(data, IMD_SIG, 4) != 0) { free(data); return UFT_ERROR_FORMAT_INVALID; }

    imd_pd_t *p = calloc(1, sizeof(imd_pd_t));
    if (!p) { free(data); return UFT_ERROR_NO_MEMORY; }
    p->data = data;
    p->size = file_size;

    size_t start = imd_skip_comment(data, file_size);
    if (!imd_scan_geometry(data, file_size, start,
                           &p->max_cyl, &p->max_head, &p->max_spt, &p->sec_size)) {
        free(data); free(p);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->max_cyl + 1;
    disk->geometry.heads = p->max_head + 1;
    disk->geometry.sectors = p->max_spt;
    disk->geometry.sector_size = p->sec_size;
    disk->geometry.total_sectors = (uint32_t)(p->max_cyl + 1) *
                                   (p->max_head + 1) * p->max_spt;
    return UFT_OK;
}

static void imd_plugin_close(uft_disk_t *disk) {
    imd_pd_t *p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t imd_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    imd_pd_t *p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t pos = imd_skip_comment(p->data, p->size);

    while (pos + 5 <= p->size) {
        uint8_t trk_cyl = p->data[pos + 1];
        uint8_t head_raw = p->data[pos + 2];
        uint8_t trk_head = head_raw & 0x0F;
        uint8_t nsec = p->data[pos + 3];
        uint8_t scode = p->data[pos + 4];
        uint16_t ss = imd_sec_size(scode);
        bool is_target = ((int)trk_cyl == cyl && (int)trk_head == head);

        pos += 5;

        /* P0-18 (MF-1348): the three maps carry the sector HEADER fields,
         * exactly as MAME imd_format::load() reads them (see file head).
         * They are read only if the whole record head fits in the file. */
        const size_t karten = (size_t)nsec * (1u + ((head_raw & 0x80) ? 1u : 0u)
                                               + ((head_raw & 0x40) ? 1u : 0u));
        if (pos + karten > p->size) break;      /* truncated record head */
        /* sector numbering map */
        const uint8_t *sec_map = p->data + pos;
        pos += nsec;
        /* optional cylinder map */
        const uint8_t *cyl_map = NULL;
        if (head_raw & 0x80) { cyl_map = p->data + pos; pos += nsec; }
        /* optional head map */
        const uint8_t *head_map = NULL;
        if (head_raw & 0x40) { head_map = p->data + pos; pos += nsec; }

        /* sector data */
        for (int s = 0; s < nsec; s++) {
            /* The ID is what the sector header says: numbering map as is
             * (uft_format_add_sector() would add 1 — it takes an INDEX),
             * cylinder/head from their maps when present. */
            const uint8_t id_sec = sec_map[s];
            const uint8_t id_cyl = cyl_map ? cyl_map[s] : (uint8_t)cyl;
            const uint8_t id_head = head_map ? head_map[s] : (uint8_t)head;
            /* P3-558 / H-30 step 2 (MF-1371): the record head names this
             * sector, the FILE ends before its data (or its type byte).
             * Before, the loop stopped at `pos < size` and the rest of the
             * record vanished silently. The sector is reported with its ID,
             * the bytes that are in the file kept, the rest filled — and
             * marked TRUNCATED: the source ended, nothing is said about the
             * medium. Rot-Beweis: test_imd_sektor_ids, Faelle 4 und 5. */
            if (pos >= p->size) {
                if (is_target) {
                    uint8_t fill[8192];
                    memset(fill, 0xE5, ss);
                    uft_format_add_sector_with_id(track, id_sec, fill, ss,
                                                  id_cyl, id_head);
                    uft_format_mark_last_truncated(track);
                }
                continue;
            }
            uint8_t dtype = p->data[pos++];
            if (dtype == 0) {
                /* "Sector data unavailable - could not be read": the sector
                 * header exists, its data does not. The fill stays (no bit
                 * lost) but is marked as NOT read (MF-980) — before P0-18
                 * it came out as a good sector full of 0xE5. */
                if (is_target) {
                    uint8_t fill[8192];
                    memset(fill, 0xE5, ss);
                    uft_format_add_sector_with_id(track, id_sec, fill, ss,
                                                  id_cyl, id_head);
                    /* H-30: mit Grund — IMD Typ 0 heisst „could not be
                     * read": dort STAND etwas, nicht lesbar. */
                    uft_format_mark_last_unavailable(track);
                }
                continue;
            }
            /* IMD dtype: 1/2=normal, 3/4=deleted, 5/6=CRC error,
             * 7/8=deleted+CRC error. Even=compressed, odd=raw. */
            bool compressed = (dtype == 2 || dtype == 4 || dtype == 6 || dtype == 8);
            bool is_deleted = (dtype >= 3 && dtype <= 4) || (dtype >= 7);
            bool is_crc_err = (dtype >= 5);
            bool angelegt = false;
            if (compressed) {
                if (is_target) {
                    uint8_t fill_buf[8192];
                    const bool da = pos < p->size;
                    memset(fill_buf, da ? p->data[pos] : 0xE5, ss);
                    uft_format_add_sector_with_id(track, id_sec, fill_buf, ss,
                                                  id_cyl, id_head);
                    if (!da) uft_format_mark_last_truncated(track);
                    angelegt = true;
                }
                pos += 1;
            } else {
                if (is_target) {
                    if (pos + ss <= p->size) {
                        uft_format_add_sector_with_id(track, id_sec,
                                                      p->data + pos, ss,
                                                      id_cyl, id_head);
                    } else {
                        /* cut inside the data: keep what the file holds */
                        uint8_t teil[8192];
                        const size_t da = p->size - pos;
                        memcpy(teil, p->data + pos, da);
                        memset(teil + da, 0xE5, ss - da);
                        uft_format_add_sector_with_id(track, id_sec, teil, ss,
                                                      id_cyl, id_head);
                        uft_format_mark_last_truncated(track);
                    }
                    angelegt = true;
                }
                pos += ss;
            }
            /* Propagate IMD sector status flags — only onto the sector
             * created in THIS iteration. Before MF-1371 a sector whose data
             * lay past the end was not created, and its flags hit the
             * PREVIOUS one (measured: a "deleted" mark moved one back). */
            if (angelegt && track->sector_count > 0) {
                if (is_crc_err)
                    uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
                if (is_deleted)
                    track->sectors[track->sector_count - 1].deleted = true;
            }
        }

        if (is_target) break;
    }
    return UFT_OK;
}

/* Write track: modifies in-memory IMD data for raw (uncompressed) sectors.
 * Compressed sectors (type 2/4/6/8) are converted to raw type in-place if
 * the new data differs from the fill byte, expanding the buffer as needed.
 * For simplicity, we only write to sectors that are already raw (type 1/3/5/7). */
static uft_error_t imd_plugin_write_track(uft_disk_t *disk, int cyl, int head,
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

    imd_pd_t *p = disk->plugin_data;
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

static const uft_plugin_feature_t uft_format_plugin_imd_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_imd = {
    .name = "IMD", .description = "ImageDisk",
    .extensions = "imd", .format = UFT_FORMAT_IMD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = imd_plugin_probe, .open = imd_plugin_open,
    .close = imd_plugin_close, .read_track = imd_plugin_read_track,
    .write_track = imd_plugin_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* Dave Dunfield published IMD.TXT with full format details */
    .features = uft_format_plugin_imd_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_imd_features) / sizeof(uft_format_plugin_imd_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(imd)
