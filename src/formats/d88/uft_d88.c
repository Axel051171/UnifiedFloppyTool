/**
 * @file uft_d88.c
 * @brief NEC PC-88/98 D88 format core
 * @version 3.8.0
 */
#include "uft/uft_format_common.h"

#define D88_HEADER 0x2B0

/* MF-625: the layout is documented with TWO header sizes, and this reader
 * accepted only one. <https://www.pc98.org/project/doc/d88.html> — "Total
 * Size: 688 or 672 bytes"; the track table at 0x20 holds 164 entries
 * (modern) or 160 (older tools). MAME's d88_dsk.cpp names the same pair:
 * the first track offset must be 0x02A0 or 0x02B0.
 *
 * The variant is not stored anywhere, but it is FORCED by the data rather
 * than guessed: entries 160..163 would occupy 0x2A0..0x2AF, so if a track's
 * data begins at 0x2A0 those sixteen bytes cannot also be table entries.
 * The smallest non-zero offset among the first 160 entries therefore decides
 * it exactly. 160 entries is a full 80-cylinder double-sided disk, so a
 * 672-byte header loses no reachable track. */
#define D88_HEADER_160 0x2A0
#define D88_TRACKS_160 160
#define D88_TRACKS_164 164

/** How many track-table entries this image has: 160 or 164, decided by the
 *  lowest non-zero offset among the entries that exist in BOTH variants. */
static int d88_track_entries(const uint8_t* tbl_base)
{
    uint32_t lowest = 0;
    for (int i = 0; i < D88_TRACKS_160; i++) {
        uint32_t o = uft_read_le32(tbl_base + i * 4);
        if (o != 0 && (lowest == 0 || o < lowest)) lowest = o;
    }
    return (lowest == D88_HEADER_160) ? D88_TRACKS_160 : D88_TRACKS_164;
}

typedef struct { FILE* file; uint8_t media; uint32_t track_off[164]; } d88_data_t;

/* MF-447: this probe claimed EVERY file in tests/corpus_free at confidence 90.
 *
 * It read two bytes' worth of evidence — `dsz <= file_size` and a media byte in
 * {0, 0x10, 0x20} — and reported near-certainty. In a D64, offset 0x1B is
 * ordinary sector data that happens to be 0x00, and the LE32 at 0x1C is
 * ordinary sector data that happens to be smaller than the file. So D88 won the
 * probe for D64, D67, D71, D80, D82, ADF, ATR and G64 alike, outranking each
 * format's own probe. It never surfaced because the registry was empty in every
 * process (ARCH-9): the probe loop had one entry, or none.
 *
 * The rewrite checks what d88_open() and d88_read_track() actually rely on —
 * the track offset table at 0x20 — and, decisively, scales the confidence to
 * the evidence instead of announcing 90 from a coincidence. Nothing here is a
 * new claim about the D88 specification; every test mirrors a field the reader
 * in this same file already dereferences.
 *
 * Honest limits: tests/corpus_free holds no real D88, so the positive path is
 * verified only against a synthetic header built from the layout this reader
 * uses (tests/test_probe_conflicts.c) — that proves the probe accepts what the
 * reader can read, not that it accepts every file a PC-88 ever wrote. The
 * negative path is verified against twelve real images of other formats. */
bool d88_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < D88_HEADER) return false;

    const uint32_t dsz   = uft_read_le32(data + 0x1C);
    const uint8_t  media = data[0x1B];
    const uint8_t  prot  = data[0x1A];

    /* A disk cannot be smaller than its own header. The lower bound is the
     * SMALLER of the two documented header sizes (MF-625) — measuring against
     * 0x2B0 rejected every 160-entry image outright. */
    if (dsz < D88_HEADER_160 || dsz > file_size) return false;
    if (media != 0x00 && media != 0x10 && media != 0x20 &&
        media != 0x30 && media != 0x40) return false;

    const int entries = d88_track_entries(data + 0x20);
    const uint32_t hdr_len = (entries == D88_TRACKS_160) ? D88_HEADER_160
                                                        : D88_HEADER;

    /* The track offset table is the part the reader uses. Every non-zero entry
     * must point inside the image and past the header, and the used entries
     * must ascend — d88_read_track() seeks straight to track_off[idx]. */
    uint32_t prev = 0;
    int used = 0;
    for (int i = 0; i < entries; i++) {
        uint32_t off = uft_read_le32(data + 0x20 + i * 4);
        if (off == 0) continue;                 /* unformatted track */
        if (off < hdr_len || off >= dsz) return false;
        if (off <= prev) return false;          /* tracks do not overlap */
        prev = off;
        used++;
    }
    if (used == 0) return false;                /* no track is reachable */

    /* Evidence, graded. The name field is ASCII with NUL padding and the
     * write-protect byte is 0x00 or 0x10; both are structure this reader does
     * not touch, so they raise confidence rather than gate the match. */
    int conf = 60;
    if (dsz == file_size) conf += 20;           /* single-disk image */
    if (prot == 0x00 || prot == 0x10) conf += 10;

    bool name_ok = true;
    for (int i = 0; i < 17; i++) {
        uint8_t c = data[i];
        if (c == 0x00) continue;
        if (c < 0x20 || c == 0x7F) { name_ok = false; break; }
    }
    if (name_ok) conf += 5;

    *confidence = conf;
    return true;
}

static uft_error_t d88_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[D88_HEADER];
    if (fread(hdr, 1, D88_HEADER, f) != D88_HEADER) { fclose(f); return UFT_ERROR_IO; }
    d88_data_t* p = calloc(1, sizeof(d88_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->media = hdr[0x1B];
    /* Only the entries this image actually has (MF-625). On a 160-entry
     * image the remaining four stay 0 = unformatted, so d88_read_track()
     * refuses them instead of seeking into the first track's sector header. */
    const int entries = d88_track_entries(&hdr[0x20]);
    for (int i = 0; i < entries; i++) p->track_off[i] = uft_read_le32(&hdr[0x20 + i*4]);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = (p->media == 0x20) ? 77 : 80;
    disk->geometry.heads = 2;
    disk->geometry.sectors = (p->media == 0x20) ? 8 : 16;
    disk->geometry.sector_size = (p->media == 0x20) ? 1024 : 256;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders * disk->geometry.heads * disk->geometry.sectors;

    /* Read actual geometry from first track's sector headers rather than
     * relying solely on the media type byte (which is often wrong for
     * non-standard or custom-formatted disks). */
    (void)fseek(f, 0, SEEK_END);  /* best-effort geometry detection */
    long file_size = ftell(f);
    if (p->track_off[0] > 0 && p->track_off[0] < (uint32_t)file_size - 16) {
        uint8_t trk_hdr[16];
        if (fseek(f, p->track_off[0], SEEK_SET) == 0 &&
            fread(trk_hdr, 1, 16, f) == 16) {
            uint16_t sec_size  = trk_hdr[14] | ((uint16_t)trk_hdr[15] << 8);
            uint16_t sec_count = trk_hdr[4]  | ((uint16_t)trk_hdr[5] << 8);
            if (sec_size > 0 && sec_size <= 8192 && sec_count > 0 && sec_count <= 64) {
                disk->geometry.sector_size = sec_size;
                disk->geometry.sectors = sec_count;
            }
        }
    }

    return UFT_OK;
}

static void d88_close(uft_disk_t* disk) {
    d88_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d88_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    d88_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    int idx = cyl * 2 + head;
    if (idx >= 164 || p->track_off[idx] == 0) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    if (fseek(p->file, p->track_off[idx], SEEK_SET) != 0) { return UFT_ERROR_INVALID_ARG; }
    uint8_t sec_hdr[16];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        if (fread(sec_hdr, 1, 16, p->file) != 16) break;
        uint16_t dsize = uft_read_le16(&sec_hdr[14]);
        if (dsize == 0 || dsize > 8192) break;
        
        uint8_t* buf = malloc(dsize);
        if (!buf) break;
        if (fread(buf, 1, dsize, p->file) != dsize) { free(buf); break; }
        uft_format_add_sector(track, sec_hdr[2] - 1, buf, dsize, cyl, head);
        /* D88 sector header (pc98.org spec / MAME d88_dsk):
         *   +07 DDAM flag  (0x00 normal, 0x10 deleted-data mark)
         *   +08 FDC status (0x00 normal, 0xA0 ID CRC error, 0xB0 data CRC
         *                   error, 0xE0 no address mark, 0xF0 no data)
         *   +09..0D reserved (NP2Kai: seek time @+09, RPM @+0D)
         * The previous code read the status from +0D (a reserved/RPM byte),
         * so deleted and CRC errors were taken from the wrong offset. */
        if (track->sector_count > 0) {
            uint8_t ddam = sec_hdr[7];
            uint8_t st   = sec_hdr[8];
            /* 0xA0 = ID-field (address-mark) CRC error, 0xB0 = data-field CRC
             * error — kept separate (header vs data fault, different
             * protection relevance) instead of collapsing both into crc_ok. */
            if (st == 0xA0)
                uft_sector_set_id_crc(&track->sectors[track->sector_count - 1], false);
            if (st == 0xB0)
                uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
            if (ddam == 0x10)
                track->sectors[track->sector_count - 1].deleted = true;
        }
        free(buf);
    }
    return UFT_OK;
}

static uft_error_t d88_write_track(uft_disk_t* disk, int cyl, int head,
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

    d88_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int idx = cyl * 2 + head;
    if (idx >= 164 || p->track_off[idx] == 0) return UFT_ERROR_INVALID_ARG;

    if (fseek(p->file, p->track_off[idx], SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t sec_hdr[16];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        long hdr_pos = ftell(p->file);
        if (hdr_pos < 0) return UFT_ERROR_IO;
        if (fread(sec_hdr, 1, 16, p->file) != 16) break;
        uint16_t dsize = uft_read_le16(&sec_hdr[14]);
        if (dsize == 0 || dsize > 8192) break;

        /* Write sector data if we have a matching sector in track */
        if ((size_t)s < track->sector_count) {
            const uint8_t *data = track->sectors[s].data;
            uint8_t *pad = NULL;
            if (!data || track->sectors[s].data_len == 0) {
                pad = malloc(dsize);
                if (!pad) return UFT_ERROR_NO_MEMORY;
                memset(pad, 0xE5, dsize);
                data = pad;
            }
            if (fwrite(data, 1, dsize, p->file) != dsize) { free(pad); return UFT_ERROR_IO; }
            free(pad);
        } else {
            /* Skip past this sector's data */
            if (fseek(p->file, (long)dsize, SEEK_CUR) != 0) return UFT_ERROR_IO;
        }
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_d88_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_d88 = {
    .name = "D88", .description = "PC-88/PC-98", .extensions = "d88;88d;d98",
    .format = UFT_FORMAT_D88, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = d88_probe, .open = d88_open, .close = d88_close,
    .read_track = d88_read_track, .write_track = d88_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_d88_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_d88_features) / sizeof(uft_format_plugin_d88_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(d88)
