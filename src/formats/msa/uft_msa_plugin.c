/**
 * @file uft_msa_plugin.c
 * @brief MSA (Atari ST compressed) Plugin-B wrapper
 *
 * Decompresses MSA to raw ST image in memory, then serves sectors.
 */
#include "uft/uft_format_common.h"

/* From uft_msa.c */
extern int uft_msa_probe(const uint8_t *data, size_t len);

/* MSA header, 10 Byte, alle Felder big-endian (MF-460).
 *
 * Feld fuer Feld gegen SAMdisk geprueft (MIT, src/samdisk/msa.cpp:9-16):
 *
 *   0-1  0x0E 0x0F  Magic
 *   2-3  Sektoren pro Spur
 *   4-5  Seiten MINUS EINS      <- SAMdisk: "MSB/LSB of sides-1"
 *   6-7  Startspur (0-basiert)
 *   8-9  Endspur   (0-basiert)
 *
 * Das "minus eins" stand hier nicht. Der Code darunter rechnet es richtig
 * (`+ 1`, wie samdisk/msa.cpp:44 `dh.abSides[1] + 1`) — es fehlte nur in der
 * Beschreibung, und eine Beschreibung, die eine Off-by-one verschweigt, ist
 * die Vorlage fuer die naechste. */
#define MSA_MAGIC 0x0E0F

typedef struct {
    uint8_t *st_data;   /* Decompressed raw ST image */
    size_t   st_size;
    uint8_t  cyl;
    uint8_t  heads;
    uint8_t  spt;
    uint8_t  start_trk; /* first cylinder the file records (MF-1427) */
} msa_plugin_data_t;

/* MSA RLE decompressor (marker 0xE5).
 *
 * MF-1427: returns false for anything SAMdisk's ReadMSA() refuses
 * (src/samdisk/msa.cpp, MIT): an E5 block with fewer than 4 bytes left
 * (:96-97), a run past the end of the track (:106-107), and an expansion
 * that is not exactly the track size (:119-120). Before, a cut E5 block
 * was copied as data, a long run was silently clipped, and a short
 * expansion left the rest of the track as calloc zeros under UFT_OK.
 *
 * A run of length 0 is still accepted: SAMdisk refuses it, Hatari
 * (floppies/msa.c) tolerates it — the references disagree. */
static bool msa_decompress_track(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t track_size)
{
    size_t sp = 0, dp = 0;
    while (sp < src_len) {
        if (src[sp] == 0xE5) {
            if (src_len - sp < 4) return false;
            uint8_t val = src[sp + 1];
            size_t count = ((size_t)src[sp + 2] << 8) | src[sp + 3];
            sp += 4;
            if (count > track_size - dp) return false;
            memset(dst + dp, val, count);
            dp += count;
        } else {
            if (dp >= track_size) return false;
            dst[dp++] = src[sp++];
        }
    }
    return dp == track_size;
}

static bool msa_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 10) return false;
    uint16_t magic = ((uint16_t)data[0] << 8) | data[1];
    if (magic != MSA_MAGIC) return false;

    /* MF-460: bis hier war Schluss — Magic getroffen, Konfidenz 95, fertig.
     * Zwei Bytes sind duenn, seit die Registry wirklich alle Plugins fragt
     * (MF-447), und dieselbe Klasse wie d88_probe(), das jede Datei im Korpus
     * mit 90 beanspruchte.
     *
     * Geprueft wird jetzt, was msa_plugin_open() unmittelbar darunter ohnehin
     * verlangt, plus die Nullbyte-Pruefung, die SAMdisk vornimmt
     * (src/samdisk/msa.cpp:38-40): die oberen Bytes aller vier Felder sind
     * bei jeder realen MSA null, weil kein Atari-Laufwerk 256 Sektoren, 256
     * Seiten oder 256 Spuren hat. */
    uint16_t spt   = ((uint16_t)data[2] << 8) | data[3];
    uint16_t sides = (uint16_t)((((uint16_t)data[4] << 8) | data[5]) + 1);
    uint16_t start = ((uint16_t)data[6] << 8) | data[7];
    uint16_t end   = ((uint16_t)data[8] << 8) | data[9];

    if (spt == 0 || spt > 18) return false;
    if (sides == 0 || sides > 2) return false;
    if (end < start) return false;

    int conf = 70;
    if (!data[2] && !data[4] && !data[6] && !data[8]) conf += 20;  /* MSBs null */
    if (end < 90) conf += 5;                     /* keine Atari-Diskette hat mehr */

    *confidence = conf;
    return true;
}

static uft_error_t msa_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t file_size = 0;
    uint8_t *msa = uft_read_file(path, &file_size);
    if (!msa || file_size < 10) { free(msa); return UFT_ERROR_FORMAT_INVALID; }

    uint16_t magic = ((uint16_t)msa[0] << 8) | msa[1];
    if (magic != MSA_MAGIC) { free(msa); return UFT_ERROR_FORMAT_INVALID; }

    uint16_t spt = ((uint16_t)msa[2] << 8) | msa[3];
    uint16_t sides = (((uint16_t)msa[4] << 8) | msa[5]) + 1;
    uint16_t start_trk = ((uint16_t)msa[6] << 8) | msa[7];
    uint16_t end_trk = ((uint16_t)msa[8] << 8) | msa[9];

    if (spt == 0 || spt > 18 || sides == 0 || sides > 2 || end_trk < start_trk) {
        free(msa); return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t cyl = (uint8_t)(end_trk + 1);
    size_t track_size = (size_t)spt * 512;
    size_t st_size = (size_t)cyl * sides * track_size;
    uint8_t *st = calloc(1, st_size);
    if (!st) { free(msa); return UFT_ERROR_NO_MEMORY; }

    /* Decompress each track.
     *
     * MF-1427: every inconsistency used to `break` out of the loop and
     * still return UFT_OK, leaving the rest of the calloc'd image as zeros
     * — a file cut 100 bytes short opened "fine" with 9198 invented zero
     * bytes. SAMdisk's ReadMSA() (src/samdisk/msa.cpp:64-120) throws for a
     * short track header, a stored length of 0 or above the track size,
     * short data and bad RLE; Hatari (floppies/msa.c) refuses the short
     * file as "Premature end of file". So does this reader now.
     * Test: tests/test_msa_kaputt_ist_kein_ok.c. */
    size_t pos = 10;
    for (int t = start_trk; t <= end_trk; t++) {
        for (int s = 0; s < (int)sides; s++) {
            size_t data_len;
            bool ok = (pos + 2 <= file_size);
            if (ok) {
                data_len = ((size_t)msa[pos] << 8) | msa[pos + 1];
                pos += 2;
                ok = data_len != 0 && data_len <= track_size
                     && data_len <= file_size - pos;
            }
            if (ok) {
                size_t dst_off = ((size_t)t * sides + s) * track_size;
                if (data_len == track_size)
                    memcpy(st + dst_off, msa + pos, track_size);  /* raw */
                else
                    ok = msa_decompress_track(msa + pos, data_len,
                                              st + dst_off, track_size);
                pos += data_len;
            }
            if (!ok) {
                free(msa); free(st);
                return UFT_ERROR_FORMAT_INVALID;
            }
        }
    }
    free(msa);

    msa_plugin_data_t *p = calloc(1, sizeof(msa_plugin_data_t));
    if (!p) { free(st); return UFT_ERROR_NO_MEMORY; }
    p->st_data = st; p->st_size = st_size;
    p->cyl = cyl; p->heads = (uint8_t)sides; p->spt = (uint8_t)spt;
    p->start_trk = (uint8_t)start_trk;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = sides;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)cyl * sides * spt;
    return UFT_OK;
}

static void msa_plugin_close(uft_disk_t *disk) {
    msa_plugin_data_t *p = disk->plugin_data;
    if (p) { free(p->st_data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t msa_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    msa_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->st_data) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    /* MF-1427: cylinders below the start track are not in the file. They
     * came back as nine zero sectors marked OK. Same convention as SCP
     * (uft_scp_plugin.c:254-260): "not recorded" is a statement about the
     * capture, not about the disk, and gets TRACK_NOT_FOUND. SAMdisk only
     * writes the tracks start..end (msa.cpp:59-126). */
    if (cyl < (int)p->start_trk) return UFT_ERROR_TRACK_NOT_FOUND;

    size_t off = ((size_t)cyl * p->heads + head) * p->spt * 512;
    uint8_t buf[512];
    for (int s = 0; s < p->spt; s++) {
        size_t soff = off + (size_t)s * 512;
        if (soff + 512 > p->st_size) break;
        memcpy(buf, p->st_data + soff, 512);
        uft_format_add_sector(track, (uint8_t)s, buf, 512, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* Write track: modifies decompressed ST buffer in memory.
 * The original MSA file is NOT modified (re-compression not supported).
 * This enables format conversion workflows (read MSA -> modify -> write as ST). */
static uft_error_t msa_plugin_write_track(uft_disk_t *disk, int cyl, int head,
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

    msa_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->st_data) return UFT_ERROR_INVALID_STATE;
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

static const uft_plugin_feature_t uft_format_plugin_msa_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_msa = {
    .name = "MSA", .description = "Atari ST Compressed (Magic Shadow)",
    .extensions = "msa", .format = UFT_FORMAT_MSA,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = msa_plugin_probe, .open = msa_plugin_open,
    .close = msa_plugin_close, .read_track = msa_plugin_read_track,
    .write_track = msa_plugin_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_msa_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_msa_features) / sizeof(uft_format_plugin_msa_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(msa)
