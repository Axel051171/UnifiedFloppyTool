/**
 * @file uft_adf_ext.c
 * @brief Extended ADF (UAE-1ADF / ADF_EXT2) reader — Plugin-B
 *
 * The extended ADF format stores copy-protected / non-standard Amiga disks:
 * each track is either a normal AmigaDOS track (11/22 x 512 decoded sectors) or
 * a RAW MFM track (the uninterpreted bitstream — that is where the protection
 * lives). Standard ADF cannot represent the raw tracks, so this reader
 * preserves them in the track's raw_data buffer (like the G64/HFE bitstream
 * plugins) — no bit is dropped.
 *
 * Exact structure (verified byte-for-byte against the WinUAE source,
 * disk.cpp read_header_ext2 — Amiga is big-endian):
 *   0x00  "UAE-1ADF"                     (8 bytes ASCII magic)
 *   0x08  uint16 BE  reserved
 *   0x0A  uint16 BE  number of tracks    (default 160 = 2*80)
 *   0x0C  per-track table, N x 12 bytes:
 *           +0  uint16 BE reserved
 *           +2  uint8      (disk revolutions - 1)
 *           +3  uint8      type: 0 = AmigaDOS track, 1 = raw MFM
 *           +4  uint32 BE  available space for track in bytes (24-bit used)
 *           +8  uint32 BE  track length in bits (24-bit used)
 *   then each track's data (len bytes) sequentially, in table order.
 *
 * Classification: mixed — AmigaDOS tracks are Sektor-Image (Klasse 3), raw-MFM
 * tracks are Bitstream (Klasse 2). Read-only: writing an extended ADF (re-MFM-
 * encoding protection) is out of scope. Decoding raw-MFM tracks INTO sectors is
 * a follow-up (needs the Amiga MFM sector decoder); the raw bitstream is
 * preserved so nothing is lost meanwhile.
 *
 * BERICHTIGT MF-1222 — der letzte Halbsatz trug nur zur Haelfte. Die BYTES
 * waren bewahrt, die LAENGE nicht: der Tafeleintrag wurde als `td[12]`
 * eingelesen und ausgewertet wurden `td[3]` (Typ) und `be24(&td[4])`
 * (Bytes); die Bitlaenge bei +8 stand in der Datei, wurde gelesen und
 * verworfen, und `raw_bits` kam in dieser Datei 0 Mal vor. Gemessen an
 * einem Extended ADF von `disk-analyse` (160 Spuren): die Datei sagt
 * 100 150 Bit, `raw_size * 8` ergibt 100 152 — **zwei Bitstellen hinter
 * dem Spurende je Spur, 320 ueber die Diskette**. Fuer ein Format, dessen
 * Zweck Schutzspuren sind, ist das nicht nebensaechlich: eine lange Spur
 * ist um BITS laenger, nicht um Bytes. Seit MF-1222 reicht `read_track()`
 * die Angabe durch, die 0 inbegriffen (0 = die Datei nennt sie nicht), und
 * sagt AB, wenn mehr Bits beansprucht als Bytes gespeichert sind.
 *
 * Der Beleg dafuer, dass der Leser das FREMDE Erzeugnis richtig zerlegt,
 * ist `tests/test_adf_ext_gegen_disk_analyse.c`: 1760 von 1760 Sektoren an
 * ihrer eigenen Ortsmarke, 0 abweichend — fremde MFM-Kodierung
 * (`disk-analyse`, Unlicense), eigener Dekoder (`flux_decode_amiga_bits`).
 * Damit steht `adf_ext` auf T1b; vorher T2, weil `test_adf_ext_plugin`
 * seine Pruefdatei SELBST baut (Klasse MF-1009/MF-1028).
 *
 * NOCH OFFEN, benannt statt entschieden — `P3-475`: die DD/HD-Heuristik
 * `if (p->tracks[i].len > 20000) hd = 2` entscheidet die Sektorzahl
 * (11 oder 22) an der Spurlaenge. Fuer ROHE Spuren geht das auf (DD ~12,5
 * KB, HD ~25 KB); fuer AmigaDOS-Spuren (Typ 0) ist eine HD-Spur
 * 22 * 512 = 11 264 Byte und liegt damit UNTER der Schranke, also meldete
 * `disk->geometry.sectors` 11, waehrend `read_track()` aus `td->len / 512`
 * 22 Sektoren zurueckgibt. Das ist Arithmetik am Quelltext, KEINE Messung
 * an einer Datei — ein HD-Extended-ADF liegt nicht vor, und ob WinUAE die
 * Schranke nur auf rohe Spuren anwendet, ist nicht nachgelesen. Deshalb
 * wird hier nichts geaendert (S5: unklar, welche Seite falsch ist).
 */
#include "uft/uft_format_common.h"

#define ADFEXT_MAX_TRACKS   176   /* 2 * 88 headroom */

typedef struct {
    uint8_t  type;    /* 0 = AmigaDOS, 1 = raw MFM */
    uint32_t offset;  /* absolute file offset of track data */
    uint32_t len;     /* track data length in bytes */
    /* MF-1222: die Spurlaenge in BIT, Feld +8 des Tafeleintrags. Sie stand
     * die ganze Zeit in der Datei und wurde gelesen und weggeworfen: der
     * Eintrag wird als `td[12]` eingelesen, benutzt wurden nur `td[3]`
     * (Typ) und `be24(&td[4])` (Bytes). Gemessen an einem Extended ADF von
     * disk-analyse: die Datei sagt 100 150 Bit je Spur, `raw_size * 8`
     * ergibt 100 152 — zwei Bitstellen HINTER dem Spurende, und wer die
     * Bytezahl hochrechnet, liest sie mit. Bei einem Format, dessen Zweck
     * Schutzspuren sind, ist die Bitlaenge das eigentliche Datum: eine
     * lange Spur ist um BITS laenger, nicht um Bytes. `uft_track_t` fuehrt
     * mit `raw_bits` seit immer das Feld dafuer. */
    uint32_t bits;    /* track length in bits, 0 = von der Datei nicht genannt */
} adfext_track_t;

typedef struct {
    FILE   *file;
    int     num_tracks;
    int     num_secs;   /* 11 (DD) or 22 (HD) */
    adfext_track_t tracks[ADFEXT_MAX_TRACKS];
} adfext_pd_t;

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be24(const uint8_t *p) { /* WinUAE uses the low 24 bits */
    return ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static bool adfext_probe(const uint8_t *data, size_t size, size_t fs, int *c) {
    (void)fs;
    if (size < 12) return false;
    if (memcmp(data, "UAE-1ADF", 8) != 0) return false;
    *c = 95;
    return true;
}

static uft_error_t adfext_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "UAE-1ADF", 8) != 0) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    int num_tracks = be16(&hdr[10]);   /* 0x0A: reserved @0x08, count @0x0A */
    if (num_tracks <= 0 || num_tracks > ADFEXT_MAX_TRACKS) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    adfext_pd_t *p = calloc(1, sizeof(adfext_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->num_tracks = num_tracks;

    long offs = 8 + 2 + 2 + (long)num_tracks * 12;   /* start of track data */
    int hd = 1;
    for (int i = 0; i < num_tracks; i++) {
        uint8_t td[12];
        if (fread(td, 1, 12, f) != 12) { free(p); fclose(f); return UFT_ERROR_IO; }
        p->tracks[i].type   = td[3];
        p->tracks[i].len    = be24(&td[4]);
        p->tracks[i].bits   = be24(&td[8]);   /* MF-1222 */
        p->tracks[i].offset = (uint32_t)offs;
        if (p->tracks[i].len > 20000) hd = 2;   /* WinUAE ddhd heuristic */
        offs += p->tracks[i].len;
    }
    p->num_secs = (hd > 1) ? 22 : 11;

    disk->plugin_data = p;
    disk->geometry.cylinders = num_tracks / 2;
    disk->geometry.heads = 2;
    disk->geometry.sectors = p->num_secs;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)(num_tracks / 2) * 2 * p->num_secs;
    return UFT_OK;
}

static void adfext_close(uft_disk_t *disk) {
    adfext_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t adfext_read_track(uft_disk_t *disk, int cyl, int head,
                                      uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    adfext_pd_t *p = disk->plugin_data;
    if (!p || !p->file || head < 0 || head > 1) return UFT_ERROR_INVALID_STATE;
    int idx = cyl * 2 + head;
    if (idx < 0 || idx >= p->num_tracks) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    const adfext_track_t *td = &p->tracks[idx];
    if (fseek(p->file, (long)td->offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    if (td->type == 0) {
        /* AmigaDOS: len bytes = (len/512) decoded 512-byte sectors. */
        int nsec = (int)(td->len / 512);
        uint8_t buf[512];
        for (int s = 0; s < nsec; s++) {
            /* MF-980: der kurze Lesevorgang wird GEMERKT, nicht nur gefuellt.
             * `uft_format_add_sector*()` legt jeden Sektor mit
             * `status = UFT_SECTOR_OK` und „CRC gueltig" an — die
             * Fuellung war damit von echten Daten nicht zu
             * unterscheiden. */
            const bool kurz = (fread(buf, 1, 512, p->file) != 512);
            if (kurz) memset(buf, 0, 512);
            /* AmigaDOS sectors are 0-based (ARCH-20) */
            uft_format_add_sector_with_id(track, (uint8_t)s, buf, 512,
                                  (uint8_t)cyl, (uint8_t)head);
            if (kurz) uft_format_mark_last_missing(track);
        }
    } else {
        /* Raw MFM (type 1): preserve the uninterpreted bitstream so the
         * protection track is not lost (decode to sectors is a follow-up). */
        if (td->len == 0 || td->len > (16u * 1024 * 1024)) return UFT_OK;
        /* MF-1222: die Datei macht ZWEI Angaben zur Spur — Bytes (+4) und
         * Bits (+8). Beansprucht sie mehr Bits, als sie Bytes speichert,
         * widersprechen sich die beiden, und die Bits gewinnen liesse
         * einen Verbraucher hinter den Puffer lesen. Hier wird deshalb
         * ABGESAGT und nicht gekappt: ein gekappter Wert saehe wie eine
         * Messung aus (D5, MF-1040). Fuer die gemessene Datei greift der
         * Zweig nicht — 100 150 <= 12 519 * 8 = 100 152. */
        if ((uint64_t)td->bits > (uint64_t)td->len * 8u)
            return UFT_ERROR_FORMAT_INVALID;
        uint8_t *raw = malloc(td->len);
        if (!raw) return UFT_ERROR_NO_MEMORY;
        if (fread(raw, 1, td->len, p->file) != td->len) { free(raw); return UFT_ERROR_IO; }
        track->raw_data = raw;
        track->raw_size = td->len;
        /* MF-595: `uft_track_release()` gibt NUR frei, wenn diese Fahne
         * steht (uft_unified_types.c:256). Wer einen eigenen Puffer an die
         * Spur haengt und sie stehen laesst, leckt ihn — und der Aufrufer
         * kann nichts dafuer, er raeumt ja korrekt auf. Gemessen im
         * CI-Leckbericht ueber g64_read_slot(). */
        track->owns_data = true;
        track->raw_len  = td->len;
        /* MF-1222: durchgereicht, wie die Datei es sagt — die 0 inbegriffen.
         * Eine 0 heisst „die Datei nennt die Bitlaenge nicht"; sie hier auf
         * `len * 8` zu setzen waere eine erfundene Zahl an der Stelle, an
         * der eine fehlende steht (MF-980: „das Format sagt X" und „hier
         * wurde X gelesen" sind zwei Aussagen). */
        track->raw_bits = td->bits;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_adf_ext_features[] = {
    { "Read (AmigaDOS + raw-MFM tracks)", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "extended-ADF write (re-MFM-encode protection) not implemented" },
    { "Raw-MFM sector decode", UFT_FEATURE_UNSUPPORTED,
      "type-1 tracks are preserved as raw MFM (raw_data); MFM->sector decode is a follow-up" },
    { "Copy-protection preservation", UFT_FEATURE_SUPPORTED,
      "raw MFM tracks are kept verbatim, no bit dropped" },
    { "Flux / timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_adf_ext = {
    .name = "ExtADF", .description = "Extended ADF (UAE-1ADF, copy-protected Amiga)",
    .extensions = "adf", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = adfext_probe, .open = adfext_open, .close = adfext_close,
    .read_track = adfext_read_track, .write_track = NULL,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* structure from WinUAE disk.cpp read_header_ext2 */
    .features = uft_format_plugin_adf_ext_features,
    .feature_count = sizeof(uft_format_plugin_adf_ext_features) / sizeof(uft_format_plugin_adf_ext_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(adf_ext)
