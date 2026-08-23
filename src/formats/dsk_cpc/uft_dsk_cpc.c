/**
 * @file uft_dsk_cpc.c
 * @brief Amstrad CPC/Spectrum DSK Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define DSK_HEADER_SIZE     256
#define DSK_TRACK_INFO_SIZE 256

/* Offset der Spurgroessen-Tabelle im Disk Information Block, und wieviele
 * Eintraege dort ueberhaupt Platz haben.
 *
 * REFERENZ (MF-513): Extended DSK disk image file format, Kevin Thacker,
 * https://cpctech.cpcwiki.de/docs/extdsk.html — der Block ist 256 Byte
 * gross, "including track size table space", und die Tabelle beginnt bei
 * 0x34 mit `tracks x sides` Eintraegen.
 *
 * Daraus folgt die Schranke, sie ist nicht gewaehlt:
 *
 *      DSK_HEADER_SIZE - DSK_TRACK_TABLE_OFF = 256 - 52 = 204
 *
 * Die Spec nennt ausdruecklich KEIN Maximum fuer Spuren oder Seiten —
 * beide Felder sind ein Byte und lassen je 255 zu, also ein Produkt bis
 * 65025. Ein Kopf, der mehr als 204 behauptet, beschreibt eine Tabelle,
 * die in dem Block, den die Spec definiert, nicht existieren kann. */
#define DSK_TRACK_TABLE_OFF   0x34
#define DSK_MAX_TRACK_ENTRIES (DSK_HEADER_SIZE - DSK_TRACK_TABLE_OFF)   /* 204 */

/* Dasselbe eine Ebene tiefer: der Track Information Block ist ebenfalls
 * 256 Byte gross, "including the sector info list", die Liste beginnt bei
 * 0x18 mit 8 Byte je Eintrag. Also passen (256 - 0x18) / 8 = 29 hinein.
 * Gleiche Referenz wie oben. */
#define DSK_SECTOR_LIST_OFF     0x18
#define DSK_SECTOR_ENTRY_SIZE   8
#define DSK_MAX_SECTOR_ENTRIES  ((DSK_TRACK_INFO_SIZE - DSK_SECTOR_LIST_OFF) \
                                 / DSK_SECTOR_ENTRY_SIZE)               /* 29 */

static const uint16_t dsk_sector_sizes[8] = { 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

typedef struct {
    FILE*       file;
    bool        extended;
    uint8_t     tracks, sides;
    uint16_t    track_size;
    /* War [200] — vier zu wenig fuer den groessten Kopf, den die Spec
     * zulaesst, und ohne jede Pruefung befuellt (MF-513). */
    uint8_t     track_sizes[DSK_MAX_TRACK_ENTRIES];
} dsk_data_t;

bool dsk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 8) return false;
    if (memcmp(data, "EXTENDED", 8) == 0) { *confidence = 95; return true; }
    if (memcmp(data, "MV - CPC", 8) == 0) { *confidence = 95; return true; }
    return false;
}

static uft_error_t dsk_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[DSK_HEADER_SIZE];
    if (fread(header, 1, DSK_HEADER_SIZE, f) != DSK_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    bool extended = (memcmp(header, "EXTENDED", 8) == 0);
    if (!extended && memcmp(header, "MV - CPC", 8) != 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    /* Die Kopfzahlen sind Dateiinhalt, also beliebig. Geprueft wird VOR der
     * Allokation, damit ein missgebildeter Kopf gar nicht erst zu einem
     * halb aufgebauten Zustand fuehrt.
     *
     * MF-513: hier stand keine Pruefung. `tracks` und `sides` sind je ein
     * Byte, ihr Produkt also bis 65025 — kopiert wurden sie in ein Feld von
     * 200 Byte, gelesen aus einem Kopf von 256. Zwei Ueberlaeufe in einer
     * Zeile, der schreibende ueber eine calloc-te Struktur hinaus, mit
     * Inhalt aus der Datei. Gefunden von tests/test_disk_open_fuzz.c an
     * einer 511-Byte-Datei mit tracks=237, sides=190: 45030 Eintraege,
     * 44830 Byte ueber das Ziel. Windows meldete Heap-Korruption
     * (0xC0000374). Belegt durch tests/test_dsk_open_bounds.c. */
    const unsigned n_entries = (unsigned)header[0x30] * (unsigned)header[0x31];
    if (extended && n_entries > DSK_MAX_TRACK_ENTRIES) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    dsk_data_t* pdata = calloc(1, sizeof(dsk_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->extended = extended;
    pdata->tracks = header[0x30];
    pdata->sides = header[0x31];
    pdata->track_size = uft_read_le16(&header[0x32]);

    if (extended) {
        memcpy(pdata->track_sizes, &header[DSK_TRACK_TABLE_OFF], n_entries);
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->tracks;
    disk->geometry.heads = pdata->sides;
    disk->geometry.sectors = 9;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)pdata->tracks * pdata->sides * 9;

    return UFT_OK;
}

static void dsk_close(uft_disk_t* disk) {
    dsk_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* Dateioffset der Spur (cyl, head).
 *
 * MF-513: diese Rechnung stand zweimal woertlich da, in read_track und in
 * write_track. Dieselbe Tatsache an zwei Stellen laeuft auseinander; hier
 * war sie an beiden Stellen ungeprueft, und `track_idx` ist aus
 * Aufrufersicht beliebig. Bei cyl=1000 las die Schleife weit ueber
 * `track_sizes` hinaus. Jetzt eine Stelle, mit Schranke.
 *
 * Geprueft wird gegen zwei Dinge, und beide muessen gelten:
 *   - die Geometrie, die der Kopf selbst meldet (tracks x sides)
 *   - die Zahl der Eintraege, die im Block ueberhaupt Platz haben (204)
 */
static uft_error_t dsk_track_offset(const dsk_data_t* pdata, int cyl, int head,
                                    size_t* out_offset)
{
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    const long idx = (long)cyl * (long)pdata->sides + (long)head;
    const long have = (long)pdata->tracks * (long)pdata->sides;
    if (idx < 0 || idx >= have) return UFT_ERROR_INVALID_PARAM;

    size_t offset = DSK_HEADER_SIZE;
    if (pdata->extended) {
        if (idx > DSK_MAX_TRACK_ENTRIES) return UFT_ERROR_INVALID_PARAM;
        for (long i = 0; i < idx; i++)
            offset += (size_t)pdata->track_sizes[i] * 256u;
    } else {
        offset += (size_t)idx * pdata->track_size;
    }
    *out_offset = offset;
    return UFT_OK;
}

static uft_error_t dsk_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    dsk_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t offset;
    uft_error_t oerr = dsk_track_offset(pdata, cyl, head, &offset);
    if (oerr != UFT_OK) return oerr;

    if (fseek(pdata->file, offset, SEEK_SET) != 0) { return UFT_ERROR_FILE_READ; }
    uint8_t track_info[DSK_TRACK_INFO_SIZE];
    if (fread(track_info, 1, DSK_TRACK_INFO_SIZE, pdata->file) != DSK_TRACK_INFO_SIZE)
        return UFT_ERROR_FILE_READ;
    
    uint8_t sec_size_code = track_info[0x14];
    uint16_t sec_size = dsk_sector_sizes[sec_size_code & 7];

    /* Die Zahl bei 0x15 ist Dateiinhalt und darf bis 255 gehen. In den
     * Block passen aber nur so viele Eintraege, wie er lang ist:
     *
     * REFERENZ (MF-513): Extended DSK disk image file format, Kevin
     * Thacker, https://cpctech.cpcwiki.de/docs/extdsk.html — der Track
     * Information Block ist 256 Byte gross, "including the sector info
     * list"; die Liste beginnt bei 0x18 mit 8 Byte je Eintrag. Also:
     *
     *      (256 - 0x18) / 8 = 232 / 8 = 29
     *
     * Die Spec nennt kein Maximum fuer die Sektorzahl — deshalb steht die
     * Schranke hier und nicht im Vertrauen darauf, dass Dateien sich
     * benehmen. Vorher lief die Schleife bis 255 und las bei s=254 auf
     * track_info[0x18 + 2032] — 1800 Byte hinter einem 256-Byte-Feld auf
     * dem Stapel. Dieselbe 29 steht als DSK_MAX_SECTORS bereits in
     * uft_dsk_cpc_parser_v2.c; sie fehlte nur hier.
     *
     * Gekappt statt abgelehnt: die Eintraege, die im Block stehen, sind
     * echt und gehoeren gelesen. Was darueber hinaus behauptet wird,
     * existiert nicht — es wird weggelassen, nicht erfunden. */
    uint8_t num_sec = track_info[0x15];
    if (num_sec > DSK_MAX_SECTOR_ENTRIES)
        num_sec = DSK_MAX_SECTOR_ENTRIES;

    /* Der Puffer muss zur GROESSTEN tatsaechlichen Laenge passen, nicht zur
     * nominellen. Bei EDSK steht in 0x06-0x07 je Sektor eine eigene Laenge
     * (little endian, bis 65535) — genau dafuer ist das Feld da, "to
     * support sectors storing more than their nominal capacity". Vorher
     * wurde mit malloc(sec_size) alloziert und dann actual_size hinein
     * gelesen: bei sec_size=128 und actual_size=65535 ein
     * Schreibueberlauf von 65407 Byte auf der Halde, Inhalt aus der
     * Datei. */
    size_t cap = sec_size;
    for (int s = 0; s < num_sec; s++) {
        const uint8_t* si = &track_info[0x18 + s * 8];
        if (pdata->extended) {
            size_t a = uft_read_le16(&si[6]);
            if (a > cap) cap = a;
        }
    }

    uint8_t* sec_buf = malloc(cap);
    if (!sec_buf) return UFT_ERROR_NO_MEMORY;
    for (int s = 0; s < num_sec; s++) {
        uint8_t* sec_info = &track_info[0x18 + s * 8];
        uint8_t sec_id = sec_info[2];
        uint16_t actual_size = sec_size;

        if (pdata->extended && sec_info[6] + sec_info[7] > 0) {
            actual_size = uft_read_le16(&sec_info[6]);
        }
        if (actual_size == 0) continue;

        memset(sec_buf, 0xE5, cap);
        /* MF-513: hier stand `{ free(sec_buf); break; }` — und nach der
         * Schleife steht `free(sec_buf)` noch einmal. Ein DOPPELTES free,
         * ausgeloest von jeder Datei, die vor dem Ende ihres letzten
         * Sektors aufhoert. Also von genau dem Fall, fuer den dieses
         * Werkzeug gebaut ist: dem abgeschnittenen oder beschaedigten
         * Abbild. Das `break` genuegt; freigegeben wird an einer Stelle. */
        if (fread(sec_buf, 1, actual_size, pdata->file) != actual_size) break;
        uft_format_add_sector(track, sec_id - 1, sec_buf, sec_size, cyl, head);
        /* FDC status bytes: sec_info[4]=ST1, sec_info[5]=ST2
         * ST1 bit 5 (0x20) = Data Error (CRC error in data field)
         * ST2 bit 5 (0x20) = CRC Error in data
         * ST2 bit 6 (0x40) = Control Mark (deleted data) */
        if (track->sector_count > 0) {
            uint8_t st1 = sec_info[4], st2 = sec_info[5];
            /* uPD765: ST2 bit5 (DD) = CRC error in the DATA field; ST1 bit5
             * (DE) = CRC error detected — if DD is not also set, the error is
             * in the ID/address field. Separate the two faults. */
            if (st2 & 0x20)
                uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
            else if (st1 & 0x20)
                uft_sector_set_id_crc(&track->sectors[track->sector_count - 1], false);
            if (st2 & 0x40)
                track->sectors[track->sector_count - 1].deleted = true;
        }
    }
    free(sec_buf);
    
    return UFT_OK;
}

static uft_error_t dsk_write_track(uft_disk_t* disk, int cyl, int head,
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

    dsk_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    size_t offset;
    uft_error_t oerr = dsk_track_offset(pdata, cyl, head, &offset);
    if (oerr != UFT_OK) return oerr;

    /* Read track info block to get sector layout */
    if (fseek(pdata->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t track_info[DSK_TRACK_INFO_SIZE];
    if (fread(track_info, 1, DSK_TRACK_INFO_SIZE, pdata->file) != DSK_TRACK_INFO_SIZE)
        return UFT_ERROR_IO;

    uint8_t num_sec = track_info[0x15];
    uint8_t sec_size_code = track_info[0x14];
    uint16_t sec_size = dsk_sector_sizes[sec_size_code & 7];

    /* Sector data starts right after the track info block */
    size_t data_pos = offset + DSK_TRACK_INFO_SIZE;

    for (int s = 0; s < num_sec; s++) {
        uint8_t* sec_info = &track_info[0x18 + s * 8];
        uint16_t actual_size = sec_size;

        if (pdata->extended && sec_info[6] + sec_info[7] > 0) {
            actual_size = uft_read_le16(&sec_info[6]);
        }

        if (fseek(pdata->file, (long)data_pos, SEEK_SET) != 0) return UFT_ERROR_IO;

        if ((size_t)s < track->sector_count) {
            const uint8_t *src = track->sectors[s].data;
            size_t src_len = track->sectors[s].data_len;
            const uint8_t *data = src;
            uint8_t *pad = NULL;
            /* Never fwrite more than the sector buffer holds: if the sector is
             * absent or shorter than this slot's size (possible for EXTENDED
             * DSK with variable sector sizes), pad to actual_size with 0xE5
             * instead of reading past track->sectors[s].data (buffer over-read). */
            if (!src || src_len < (size_t)actual_size) {
                pad = malloc(actual_size);
                if (!pad) return UFT_ERROR_NO_MEMORY;
                memset(pad, 0xE5, actual_size);
                if (src && src_len > 0) memcpy(pad, src, src_len);
                data = pad;
            }
            if (fwrite(data, 1, actual_size, pdata->file) != actual_size) {
                free(pad);
                return UFT_ERROR_IO;
            }
            free(pad);
        }
        data_pos += actual_size;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_dsk_cpc_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dsk_cpc = {
    .name = "DSK",
    .description = "Amstrad CPC/Spectrum DSK",
    .extensions = "dsk",
    .version = 0x00010000,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = dsk_probe,
    .open = dsk_open,
    .close = dsk_close,
    .read_track = dsk_read_track,
    .write_track = dsk_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* CPCWiki publishes the standard DSK specification */
    .features = uft_format_plugin_dsk_cpc_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dsk_cpc_features) / sizeof(uft_format_plugin_dsk_cpc_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(dsk_cpc)
