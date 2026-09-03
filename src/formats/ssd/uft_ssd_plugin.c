/**
 * @file uft_ssd_plugin.c
 * @brief SSD/DSD (BBC Micro) Plugin
 *
 * SSD = Single-Sided Disk image, DSD = Double-Sided.
 * Headerless raw 256-byte sectors, 10 sectors/track.
 *
 * Geometry:
 *   SSD: 80 × 1 × 10 × 256 = 204,800 (or 40 tracks = 102,400)
 *   DSD: 80 × 2 × 10 × 256 = 409,600 (or 40 tracks = 204,800)
 *
 * DSD interleave: track 0 side 0, track 0 side 1, track 1 side 0, ...
 *
 * Reference: BeebWiki SSD/DSD format
 */

#include "uft/uft_format_common.h"
#include "uft/uft_log.h"

#define SSD_SECTOR_SIZE     256

/* ── HADFS-Kennung (MF-836) ───────────────────────────────────────────────
 *
 * HADFS (J. G. Harston, BBC/Electron/Master) schreibt beim ANLEGEN einer
 * Diskette einen ECHTEN DFS-Katalog in die Sektoren 0/1 — als
 * Kompatibilitaetsmassnahme. Quelle: HADFS 6.10 Quelltext, `S.HADFS8`
 * §InstDFS:
 *
 *     LDA FSM+31:AND #&41:BNE InstNoDFS   \ Don't overwrite sectors 0/1
 *
 * Nur bei Flag-Bit 0 (NoDFS) oder Bit 6 (Locked) bleiben 0/1 unberuehrt.
 * Im Normalfall stehen dort Titel „HADFS" und die Eintraege `!Boot`, `$`,
 * `HADFSROM` (`S.HADFS9` §BootCode).
 *
 * Die Sonde unten prueft genau diesen Katalog und meldete darauf **85**,
 * also nach der Skala aus MF-729 „Merkmal getroffen". Getroffen wurde
 * aber ein Merkmal, das HADFS ABSICHTLICH hinterlegt hat; das eigentliche
 * Dateisystem beginnt bei Sektor 71 und blieb unsichtbar. Kein Absturz,
 * keine Warnung — ein plausibles falsches Ergebnis.
 *
 * HADFS prueft sich selbst an acht Byte (`S.HADFS9` §ChkJGH gegen
 * §JGHName; Puffer bei &F00, verglichen ab &F10 = Offset 16):
 *
 *     Sektor 70, Byte 16..23 = 00 28 43 29 4A 47 48 00   („\0(C)JGH\0")
 *
 * Dateioffset 70*256 + 16 = 0x4610, innerhalb von
 * UFT_PROBE_BUFFER_SIZE (65536).
 *
 * WAS HIER NICHT PASSIERT: HADFS wird nicht GELESEN. Ein neuer
 * Dateisystem-Leser fiele unter das Moratorium (EINFRIER-REGEL
 * MF-363/498). Diese Aenderung nimmt nur einen falschen Anspruch
 * zurueck — die Groesse stimmt weiter, das Merkmal wird nicht mehr
 * behauptet. */
#define SSD_HADFS_FSM_SECTOR  70u
#define SSD_HADFS_SIG_OFFSET  16u
#define SSD_HADFS_SIG_AT      (SSD_HADFS_FSM_SECTOR * SSD_SECTOR_SIZE \
                               + SSD_HADFS_SIG_OFFSET)   /* 0x4610 */

static const uint8_t SSD_HADFS_SIG[8] = {
    0x00, '(', 'C', ')', 'J', 'G', 'H', 0x00
};

/** true, wenn der Puffer die HADFS-Kennung an ihrer Stelle traegt.
 *
 *  Reicht der Puffer nicht bis dorthin, ist die Frage unbeantwortbar —
 *  und eine unbeantwortbare Frage darf keine Antwort erzwingen: dann
 *  false, also keine Herabsetzung. */
static bool ssd_traegt_hadfs_kennung(const uint8_t *data, size_t size)
{
    if (!data || size < SSD_HADFS_SIG_AT + sizeof(SSD_HADFS_SIG))
        return false;
    return memcmp(data + SSD_HADFS_SIG_AT, SSD_HADFS_SIG,
                  sizeof(SSD_HADFS_SIG)) == 0;
}
#define SSD_SPT             10

typedef struct {
    FILE*       file;
    uint8_t     cylinders;
    uint8_t     heads;
    bool        interleaved;    /* DSD: sides interleaved per track */
} ssd_data_t;

static bool ssd_detect(size_t file_size, uint8_t *cyl, uint8_t *heads)
{
    uint32_t track_size = SSD_SPT * SSD_SECTOR_SIZE;
    if (file_size == 0 || file_size % track_size != 0) return false;

    uint32_t total = (uint32_t)(file_size / track_size);
    if (total == 40)  { *cyl = 40; *heads = 1; return true; }
    if (total == 80)  { *cyl = 80; *heads = 1; return true; }
    if (total == 160) { *cyl = 80; *heads = 2; return true; }
    return false;
}

static bool uft_ssd_plugin_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    uint8_t cyl, heads;
    if (!ssd_detect(file_size, &cyl, &heads)) return false;
    *confidence = 30;

    /* BBC Micro DFS catalog: sector 1 bytes 0x100-0x107 contain
     * the disk title (continued from sector 0). Sector 1 byte 0x104
     * holds sector count (low byte), typically 0x20 for 40-track. */
    if (size >= 0x108) {
        uint8_t sec_count_lo = data[0x107];
        /* Valid sector counts: 0x90=400, 0x20=800, 0xA0=1280 */
        if (sec_count_lo == 0x90 || sec_count_lo == 0x20 || sec_count_lo == 0xA0)
            *confidence = 82;
        /* Boot option (bits 4-5 of byte 0x106) should be 0-3 */
        if ((data[0x106] >> 4) <= 3 && sec_count_lo > 0)
            *confidence = 85;
    }

    /* MF-836: Der Katalog kann ECHT und die Diskette trotzdem kein
     * DFS-Volume sein — HADFS legt ihn absichtlich dort ab. Traegt
     * Sektor 70 die HADFS-Kennung, wird der Anspruch auf das gesenkt,
     * was dann noch stimmt: die Groesse.
     *
     * 30 ist die Skalenstufe „nur die Groesse" (MF-729). Bewusst KEIN
     * `return false` — die Datei IST ein Acorn-Abbild mit 10 Sektoren zu
     * 256 Byte, und ihr den Zugriff zu verweigern waere schlechter als
     * eine ehrliche niedrige Zahl. */
    if (ssd_traegt_hadfs_kennung(data, size)) {
        UFT_WARN("SSD: Sektor 70 traegt die HADFS-Kennung \"(C)JGH\" — "
                 "der DFS-Katalog in Sektor 0/1 ist HADFS' "
                 "Kompatibilitaetseintrag, kein eigenstaendiges "
                 "DFS-Volume; das Dateisystem beginnt bei Sektor 71");
        *confidence = 30;
    }
    return true;
}

static uft_error_t ssd_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads;
    if (!ssd_detect((size_t)fs, &cyl, &heads)) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    ssd_data_t *p = calloc(1, sizeof(ssd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->cylinders = cyl; p->heads = heads;
    p->interleaved = (heads == 2);

    disk->plugin_data = p;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = SSD_SPT;
    disk->geometry.sector_size = SSD_SECTOR_SIZE;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * SSD_SPT;
    return UFT_OK;
}

static void ssd_close(uft_disk_t *disk)
{
    ssd_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t ssd_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    ssd_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset;
    if (p->interleaved) {
        /* DSD: cyl0/side0, cyl0/side1, cyl1/side0, ... */
        offset = (long)((cyl * 2 + head) * SSD_SPT * SSD_SECTOR_SIZE);
    } else {
        offset = (long)(cyl * SSD_SPT * SSD_SECTOR_SIZE);
    }

    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[SSD_SECTOR_SIZE];
    for (int s = 0; s < SSD_SPT; s++) {
        if (fread(buf, 1, SSD_SECTOR_SIZE, p->file) != SSD_SECTOR_SIZE)
            return UFT_ERROR_IO;
        uft_format_add_sector(track, (uint8_t)s, buf, SSD_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t ssd_write_track(uft_disk_t *disk, int cyl, int head,
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

    ssd_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long offset = p->interleaved ?
        (long)((cyl * 2 + head) * SSD_SPT * SSD_SECTOR_SIZE) :
        (long)(cyl * SSD_SPT * SSD_SECTOR_SIZE);
    for (size_t s = 0; s < track->sector_count && (int)s < SSD_SPT; s++) {
        if (fseek(p->file, offset + (long)s * SSD_SECTOR_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[SSD_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, SSD_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, SSD_SECTOR_SIZE, p->file) != SSD_SECTOR_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_ssd_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_ssd = {
    .name = "SSD", .description = "BBC Micro SSD/DSD",
    .extensions = "ssd;dsd", .format = UFT_FORMAT_SSD,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = uft_ssd_plugin_probe, .open = ssd_open, .close = ssd_close,
    .read_track = ssd_read_track, .write_track = ssd_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_ssd_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_ssd_features) / sizeof(uft_format_plugin_ssd_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(ssd)
