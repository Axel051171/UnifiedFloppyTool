/**
 * @file uft_dcm.c
 * @brief DCM (Disk COMpression) Format Plugin — full decompression
 *
 * DCM is a sector compression format for Atari 8-bit floppy disks.
 * Developed 1988 by Bob Puff (DiskCommunicator) for BBS distribution.
 *
 * Supports:
 *   - Single Density (SD: 90K, 720 sectors x 128 byte)
 *   - Enhanced Density (ED: 130K, 1040 sectors x 128 byte)
 *   - Double Density (DD: 180K, 720 sectors x 256 byte)
 *   - Quad Density (QD: 360K, 1440 sectors x 256 byte)
 *   - Multi-pass archives
 *   - All 5 compression types (Modify/Same/Compress/Change/Gap)
 *
 * MF-1053 - neu gefasst. Hier stand "CRITICAL: Sectors 1-3 are
 * ALWAYS 128 bytes, even on DD disks". Das ist die **ATR**-Konvention,
 * nicht die DCM-Anordnung, und es ist am Objekt entschieden: in einer
 * echten Doppeldichte-DCM liegt unter der FLACHEN Anordnung bei
 * Sektor 360 eine gueltige Atari-VTOC (02 c4 02 7c 00 - 708 Sektoren,
 * 124 frei) und bei Sektor 361 ein Verzeichniseintrag mit lesbarem
 * Namen ("MODPLUS"); unter der 128-Byte-Anordnung steht dort Rauschen.
 *
 * Referenz: `atrip/atrip/compressors/dcm.py` (Rob McMullen, GPL-2), im
 * Baum unter `tools/uft-scout/work/atrip/` - gelesen und AUSGEFUEHRT,
 * keine Zeile uebernommen; diese Umsetzung ist aus dem beobachteten
 * Verhalten eigenstaendig geschrieben (Muster MF-614, wie MF-1022 bei
 * libsap). Die Kopffelder sind zusaetzlich durch
 * `atrcopy/atrcopy/dcm.py` (GPL-2) belegt, das allerdings NUR den Kopf
 * liest und dann absagt (MF-1052). Abgenommen an zwei von atrips
 * Packer erzeugten Abbildern und an drei echten, historischen DCMs:
 * `tests/test_dcm_gegen_atrip.c`.
 *
 * Der Vorzustand konnte KEINE echte DCM lesen (`open` = -25), waehrend
 * die Sonde mit Konfidenz 90 zustimmte - die Gestalt von MF-961,
 * MF-1022 und MF-1036. Die vollstaendige Liste der Abweichungen steht
 * im Kopf des Tests.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Archivtyp. Hier stand zusaetzlich ein `DCM_MAGIC_F8 0xF8`; **beide**
 * Referenzen kennen nur 0xF9 und 0xFA, und keine der drei echten
 * Dateien traegt 0xF8. Erfunden, entfernt (MF-1053). */
#define DCM_MAGIC_FA        0xFA    /* Einzelarchiv                */
#define DCM_MAGIC_F9        0xF9    /* Teil eines Mehrteilarchivs  */

/* Blocktypen - gemessen, nicht uebernommen (MF-1053).
 *
 * Die alte Tafel war DURCHGEHEND anders belegt: 0x41 hiess dort
 * "roher Sektor" (das ist 0x47), 0x45 "Nullen" (das ist das
 * Durchgangsende), 0x46 "Ende" (das ist "wie voriger Sektor"), und
 * 0x47 gab es gar nicht. */
#define DCM_BLK_CHANGE_BEGIN  0x41  /* Index, dann rueckwaerts bis 0 */
#define DCM_BLK_DOS_SECTOR    0x42  /* 124 x ein Byte, dann 4 einzeln*/
#define DCM_BLK_RLE           0x43  /* woertlich und Lauf im Wechsel */
#define DCM_BLK_CHANGE_END    0x44  /* Index, dann vorwaerts bis Ende*/
#define DCM_BLK_PASS_END      0x45  /* Ende dieses Durchgangs        */
#define DCM_BLK_SAME          0x46  /* unveraendert wie der vorige   */
#define DCM_BLK_UNCOMPRESSED  0x47  /* voller Sektor, roh            */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint8_t*    disk_data;
    uint32_t    disk_size;
    uint16_t    sector_size;
    uint32_t    total_sectors;
    uint8_t     density;
} dcm_plugin_data_t;

/* ============================================================================
 * Sector offset helpers (boot sector exception)
 * ============================================================================ */

/* Die Anordnung ist FLACH - jeder Sektor traegt die volle
 * Sektorgroesse, auch die Sektoren 1 bis 3. Siehe Kopfkommentar:
 * am Objekt entschieden, nicht aus einer Tafel uebernommen. */
static size_t dcm_sector_offset(uint32_t sec, uint16_t ss) {
    if (sec < 1) return 0;
    return (size_t)(sec - 1) * ss;
}

static uint16_t dcm_sec_size(uint32_t sec, uint16_t ss) {
    (void)sec;
    return ss;
}

/* Die Dichtetafel hat DREI Eintraege. Die alte fuehrte vier, mit 1 und
 * 2 vertauscht und einer erfundenen 3. Belegt an drei echten Dateien
 * (MF-1052): Kennbyte 0x21 (Dichte 1) entpackt zu 184 320 Byte =
 * 720 x 256, Kennbyte 0x81 (Dichte 0) zu 92 160 = 720 x 128. */
static bool dcm_density_geometry(uint8_t density, uint16_t *ss,
                                 uint32_t *total)
{
    switch (density) {
    case 0: *ss = 128; *total = 720;  return true;
    case 1: *ss = 256; *total = 720;  return true;
    case 2: *ss = 128; *total = 1040; return true;
    default: return false;
    }
}

/* ============================================================================
 * Decompression engine
 * ============================================================================ */

typedef struct {
    const uint8_t *d;
    size_t         n;
    size_t         pos;
    bool           leer;        /* Strom zu Ende gelesen */
} dcm_strom_t;

static uint8_t dcm_next(dcm_strom_t *s)
{
    if (s->pos >= s->n) { s->leer = true; return 0; }
    return s->d[s->pos++];
}

/* Die Sektornummer steht als LE16 im Strom - und zwar NACH dem Block,
 * nicht davor. Der Vorzustand las sie davor und nur, wenn das hohe Bit
 * GESETZT war; beides umgekehrt. */
static uint32_t dcm_next_sector(dcm_strom_t *s)
{
    uint32_t lo = dcm_next(s);
    uint32_t hi = dcm_next(s);
    return hi * 256u + lo;
}

static uft_error_t dcm_decompress(const uint8_t *data, size_t data_size,
                                   uint8_t *out, uint32_t out_size,
                                   uint16_t ss, uint32_t total)
{
    dcm_strom_t s;
    uint8_t current[256];
    bool last_pass = false;
    unsigned erwarteter_durchgang = 1;

    if (!data || !out || ss == 0 || ss > sizeof current)
        return UFT_ERROR_INVALID_STATE;

    s.d = data; s.n = data_size; s.pos = 0; s.leer = false;
    memset(current, 0, sizeof current);

    while (!last_pass) {
        uint8_t typ, flags;
        uint32_t cur;

        typ = dcm_next(&s);
        if (s.leer) return UFT_ERROR_IO;
        if (typ != DCM_MAGIC_FA && typ != DCM_MAGIC_F9)
            return UFT_ERROR_FORMAT_INVALID;

        /* Bits 0-4 Durchgangsnummer, Bits 5-6 Dichte, Bit 7 letzter
         * Durchgang. Der Vorzustand las die Dichte aus Bits 0-4 und das
         * Ende-Bit aus Bit 6 - also aus der Dichte. */
        flags = dcm_next(&s);
        if (s.leer) return UFT_ERROR_IO;
        if ((unsigned)(flags & 0x1F) != erwarteter_durchgang)
            return UFT_ERROR_FORMAT_INVALID;
        last_pass = (flags & 0x80) != 0;

        cur = dcm_next_sector(&s);
        if (s.leer) return UFT_ERROR_IO;

        for (;;) {
            uint8_t blk;
            size_t off;

            blk = dcm_next(&s);
            if (s.leer) return UFT_ERROR_IO;

            if (blk == DCM_BLK_PASS_END) {
                erwarteter_durchgang = (erwarteter_durchgang + 1u) % 32u;
                break;
            }

            switch (blk & 0x7F) {
            case DCM_BLK_CHANGE_BEGIN: {
                int i = (int)dcm_next(&s);
                while (i >= 0) { current[i] = dcm_next(&s); i--; }
                break;
            }
            case DCM_BLK_DOS_SECTOR: {
                uint8_t v;
                /* Dieser Block legt genau die Bytes 0..127 fest; auf
                 * einer 256-Byte-Diskette bliebe der Rest stehen. Die
                 * Referenz setzt ihn nur bei 128 Byte ein. */
                if (ss < 128) return UFT_ERROR_FORMAT_INVALID;
                v = dcm_next(&s);
                memset(current, v, 124);
                current[124] = dcm_next(&s);
                current[125] = dcm_next(&s);
                current[126] = dcm_next(&s);
                current[127] = dcm_next(&s);
                break;
            }
            case DCM_BLK_RLE: {
                unsigned index = 0;
                while (index < ss) {
                    unsigned vorher = index, ende;
                    uint8_t fuell;

                    /* woertlich bis `ende` */
                    ende = dcm_next(&s);
                    if (index > 0 && ende == 0) ende = 256;
                    if (ende > sizeof current) ende = sizeof current;
                    while (index < ende) current[index++] = dcm_next(&s);

                    if (index < ss) {
                        /* Lauf eines Fuellbytes bis `ende` */
                        ende = dcm_next(&s);
                        if (index > 0 && ende == 0) ende = 256;
                        if (ende > sizeof current) ende = sizeof current;
                        fuell = dcm_next(&s);
                        while (index < ende) current[index++] = fuell;
                    }
                    if (s.leer) return UFT_ERROR_IO;
                    /* Kommt der Lauf nicht voran, ist der Strom kaputt.
                     * Ohne diese Schranke liefe die Schleife, bis der
                     * Strom leer ist, und meldete IO statt Format. */
                    if (index == vorher) return UFT_ERROR_FORMAT_INVALID;
                }
                break;
            }
            case DCM_BLK_CHANGE_END: {
                unsigned i = dcm_next(&s);
                while (i < ss) { current[i] = dcm_next(&s); i++; }
                break;
            }
            case DCM_BLK_SAME:
                break;
            case DCM_BLK_UNCOMPRESSED: {
                unsigned i;
                for (i = 0; i < ss; i++) current[i] = dcm_next(&s);
                break;
            }
            default:
                return UFT_ERROR_FORMAT_INVALID;
            }
            if (s.leer) return UFT_ERROR_IO;

            if (cur < 1 || cur > total) return UFT_ERROR_FORMAT_INVALID;
            off = dcm_sector_offset(cur, ss);
            if (off + ss > out_size) return UFT_ERROR_FORMAT_INVALID;
            memcpy(out + off, current, ss);

            /* Hohes Bit GESETZT heisst: der naechste Sektor folgt
             * implizit. Sonst steht seine Nummer hier im Strom. */
            if (blk > 0x80) {
                cur++;
            } else {
                cur = dcm_next_sector(&s);
                if (s.leer) return UFT_ERROR_IO;
            }
        }
    }
    return UFT_OK;
}

/* ============================================================================
 * probe
 * ============================================================================ */

/* Konfidenz 70 und nicht mehr 90 (MF-1053). Geprueft sind jetzt DREI
 * Dinge - Archivtyp, Durchgangsnummer und Gueltigkeit der Dichte -,
 * also Band "Struktur gelesen" (50..79) nach MF-729. 90 stand fuer
 * "Merkmal getroffen" und beruhte auf einem einzigen Byte. */
static bool uft_dcm_plugin_probe(const uint8_t *data, size_t size,
                                  size_t file_size, int *confidence)
{
    uint8_t flags;
    uint16_t ss;
    uint32_t total;

    (void)file_size;
    if (!data || size < 4) return false;
    if (data[0] != DCM_MAGIC_FA && data[0] != DCM_MAGIC_F9)
        return false;

    flags = data[1];
    /* Der erste Durchgang traegt immer die Nummer 1 - beide
     * Referenzen weisen alles andere als erstes Stueck ab. */
    if ((flags & 0x1F) != 1) return false;
    if (!dcm_density_geometry((uint8_t)((flags >> 5) & 3), &ss, &total))
        return false;

    if (confidence) *confidence = 70;
    return true;
}

/* ============================================================================
 * open — decompress entire DCM into memory buffer
 * ============================================================================ */

static uft_error_t dcm_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data || file_size < 5) { free(file_data); return UFT_ERROR_FORMAT_INVALID; }

    uint8_t magic = file_data[0];
    uint8_t density;
    uint16_t ss;
    uint32_t total, disk_size;

    if (magic != DCM_MAGIC_FA && magic != DCM_MAGIC_F9) {
        free(file_data); return UFT_ERROR_FORMAT_INVALID;
    }

    /* Die Dichte steht in den Bits 5-6, nicht in 0-4 - dort steht die
     * Durchgangsnummer. Weil der erste Durchgang immer 1 ist, waehlte
     * der Vorzustand fuer JEDE DCM dieselbe Geometrie (MF-1053). */
    density = (uint8_t)((file_data[1] >> 5) & 3);
    if (!dcm_density_geometry(density, &ss, &total)) {
        free(file_data); return UFT_ERROR_FORMAT_INVALID;
    }

    /* Flach: jeder Sektor traegt die volle Sektorgroesse. */
    disk_size = total * (uint32_t)ss;

    uint8_t *disk_buf = calloc(1, disk_size);
    if (!disk_buf) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    uft_error_t err = dcm_decompress(file_data, file_size, disk_buf, disk_size, ss, total);
    free(file_data);

    if (err != UFT_OK) { free(disk_buf); return err; }

    dcm_plugin_data_t *p = calloc(1, sizeof(dcm_plugin_data_t));
    if (!p) { free(disk_buf); return UFT_ERROR_NO_MEMORY; }

    p->disk_data = disk_buf;
    p->disk_size = disk_size;
    p->sector_size = ss;
    p->total_sectors = total;
    p->density = density;

    disk->plugin_data = p;
    disk->geometry.cylinders = (total + 17) / 18;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = total;
    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void dcm_close(uft_disk_t *disk)
{
    dcm_plugin_data_t *p = disk->plugin_data;
    if (p) { free(p->disk_data); free(p); disk->plugin_data = NULL; }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t dcm_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dcm_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->disk_data || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    for (int s = 0; s < 18; s++) {
        uint32_t sec_num = (uint32_t)(cyl * 18 + s + 1);
        if (sec_num > p->total_sectors) break;

        uint16_t sz = dcm_sec_size(sec_num, p->sector_size);
        size_t off = dcm_sector_offset(sec_num, p->sector_size);
        if (off + sz > p->disk_size) break;

        uft_format_add_sector(track, (uint8_t)s,
                              p->disk_data + off, sz,
                              (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

/* ============================================================================
 * write_track — modify decompressed in-memory buffer
 * ============================================================================ */

static uft_error_t dcm_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    dcm_plugin_data_t *p = disk->plugin_data;
    if (!p || !p->disk_data || head != 0) return UFT_ERROR_INVALID_STATE;
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

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_dcm_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-883: schreibt nur in den Speicher — kein fwrite in der Datei, kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dcm = {
    .name         = "DCM",
    .description  = "Atari 8-bit Compressed Disk (DiskCOMm)",
    .extensions   = "dcm",
    .version      = 0x00020000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe        = uft_dcm_plugin_probe,
    .open         = dcm_open,
    .close        = dcm_close,
    .read_track   = dcm_read_track,
    .write_track  = dcm_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dcm_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dcm_features) / sizeof(uft_format_plugin_dcm_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(dcm)
