/**
 * @file uft_adf_plugin.c
 * @brief ADF (Amiga Disk File) Plugin-B — self-contained
 *
 * ADF is a headerless raw sector dump of Amiga floppy disks.
 * DD: 80 cyl × 2 heads × 11 spt × 512 = 901,120 bytes
 * HD: 80 cyl × 2 heads × 22 spt × 512 = 1,802,240 bytes
 */
#include "uft/uft_format_common.h"

#define ADF_DD_SIZE     901120
#define ADF_HD_SIZE     1802240
#define ADF_SECTOR_SIZE 512

typedef struct { FILE *file; uint8_t spt; } adf_pd_t;

static bool adf_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    if (file_size != ADF_DD_SIZE && file_size != ADF_HD_SIZE) return false;
    *confidence = 45;  /* MF-729: nur die Groesse */
    /* Amiga bootblock: "DOS\0" at offset 0 */
    if (size >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S')
        *confidence = 95;
    return true;
}

static uft_error_t adf_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t spt = (sz == ADF_HD_SIZE) ? 22 : 11;
    if (sz != ADF_DD_SIZE && sz != ADF_HD_SIZE) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    adf_pd_t *p = calloc(1, sizeof(adf_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->spt = spt;

    disk->plugin_data = p;
    disk->geometry.cylinders = 80;
    disk->geometry.heads = 2;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ADF_SECTOR_SIZE;
    disk->geometry.total_sectors = 80 * 2 * spt;
    return UFT_OK;
}

static void adf_close(uft_disk_t *disk) {
    adf_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t adf_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-522: gegen die Geometrie pruefen, die dieses Plugin bei `open`
     * SELBST gemeldet hat. Ohne diese Schranke rechnete die Zeile darunter
     * einen Offset aus beliebigen Koordinaten:
     *
     *   cyl=1000 -> Offset weit hinter dem Dateiende. `fseek` gelingt,
     *               `fwrite` verlaengert die Datei. Aus 880 KB wurden im
     *               Test 11 MB, und der Aufrufer bekam UFT_OK.
     *   cyl=-1   -> Offset konnte auf 0 zurueckfallen und damit SPUR 0
     *               ueberschreiben. Ein gueltiger Ort, erreicht ueber eine
     *               unsinnige Koordinate.
     *
     * Beides ist eine stille Veraenderung mit Erfolgsmeldung — genau das,
     * was DESIGN_PRINCIPLES verbietet. Gefunden von
     * tests/test_disk_write_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;
    if (cyl >= (int)disk->geometry.cylinders ||
        head >= (int)disk->geometry.heads) return UFT_ERROR_INVALID_PARAM;

    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    adf_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    long off = (long)((cyl * 2 + head) * p->spt * ADF_SECTOR_SIZE);
    uint8_t buf[ADF_SECTOR_SIZE];
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, off + s * ADF_SECTOR_SIZE, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, ADF_SECTOR_SIZE, p->file) != ADF_SECTOR_SIZE) {
            memset(buf, 0xE5, ADF_SECTOR_SIZE);
        }
        /* AmigaDOS numbers its sectors 0..10 (ARCH-20) */
        uft_format_add_sector_with_id(track, (uint8_t)s, buf, ADF_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t adf_write_track(uft_disk_t *disk, int cyl, int head,
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

    /* MF-522: gegen die Geometrie pruefen, die dieses Plugin bei `open`
     * SELBST gemeldet hat. Ohne diese Schranke rechnete die Zeile darunter
     * einen Offset aus beliebigen Koordinaten:
     *
     *   cyl=1000 -> Offset weit hinter dem Dateiende. `fseek` gelingt,
     *               `fwrite` verlaengert die Datei. Aus 880 KB wurden im
     *               Test 11 MB, und der Aufrufer bekam UFT_OK.
     *   cyl=-1   -> Offset konnte auf 0 zurueckfallen und damit SPUR 0
     *               ueberschreiben. Ein gueltiger Ort, erreicht ueber eine
     *               unsinnige Koordinate.
     *
     * Beides ist eine stille Veraenderung mit Erfolgsmeldung — genau das,
     * was DESIGN_PRINCIPLES verbietet. Gefunden von
     * tests/test_disk_write_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;
    if (cyl >= (int)disk->geometry.cylinders ||
        head >= (int)disk->geometry.heads) return UFT_ERROR_INVALID_PARAM;

    adf_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)((cyl * 2 + head) * p->spt * ADF_SECTOR_SIZE);
    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * ADF_SECTOR_SIZE, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[ADF_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, ADF_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, ADF_SECTOR_SIZE, p->file) != ADF_SECTOR_SIZE) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* Prinzip 7 Feature-Matrix */
static const uft_plugin_feature_t adf_features[] = {
    { "Standard DD (880 KB)",      UFT_FEATURE_SUPPORTED,   NULL },
    { "HD variant (1760 KB)",      UFT_FEATURE_SUPPORTED,   NULL },
    { "FFS / OFS filesystems",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Write / encode",            UFT_FEATURE_SUPPORTED,   NULL },
    { "Custom sector layouts",     UFT_FEATURE_UNSUPPORTED,
      "non-standard tracks require flux-level format (IPF/KFX/WOZ)" },
    { "Copy-protection signatures", UFT_FEATURE_UNSUPPORTED, NULL },
};

/* Prinzip 6 Kompatibilitätsmatrix — see docs/DESIGN_PRINCIPLES.md §6 */
/* Consumers that matter for ADF export, and what we actually know about them.
 *
 * MF-414: every entry here used to claim a verified result — WinUAE 5.3/4.x and
 * FS-UAE 3.1 "COMPATIBLE", FS-UAE <3.0 "INCOMPATIBLE" with a technical reason,
 * Amiga Explorer "COMPATIBLE, tested 2026-03", real hardware "PARTIAL, 85% of
 * test disks round-trip cleanly".
 *
 * None of it was ever measured. The table was copied verbatim out of the
 * ILLUSTRATIVE example in docs/DESIGN_PRINCIPLES.md section 6 (lines 282-287)
 * when the API was introduced; the commit says so itself ("1:1 aus dem
 * DESIGN_PRINCIPLES.md §6 Beispiel"). The copy even contradicts its source on
 * the one checkable field: the example marks three rows "CI-getestet", the code
 * shipped them with ci_tested = false. No CI job feeds ADF output to an
 * emulator, and the project owns no Amiga hardware (UFT-008, HIL tier NOT_RUN),
 * so the 85 % figure cannot have an origin.
 *
 * The consumer names are kept, because knowing WHICH targets matter is real
 * information. The verdicts are not: they are UNTESTED, which is what
 * uft_emu_compat_t has that value for. This turns a set of claims into a
 * to-do list. See KNOWN_ISSUES PRINC-1. */
static const uft_plugin_compat_entry_t adf_compat[] = {
    { "WinUAE 5.3",      UFT_EMU_UNTESTED,
      "no test feeds UFT-written ADF to WinUAE", NULL, false },
    { "WinUAE 4.x",      UFT_EMU_UNTESTED,
      "no test feeds UFT-written ADF to WinUAE", NULL, false },
    { "FS-UAE 3.1",      UFT_EMU_UNTESTED,
      "no test feeds UFT-written ADF to FS-UAE", NULL, false },
    { "FS-UAE <3.0",     UFT_EMU_UNTESTED,
      "the timing-track incompatibility was asserted, never observed", NULL, false },
    { "Amiga Explorer",  UFT_EMU_UNTESTED,
      "the 2026-03 test date had no run behind it", NULL, false },
    { "real Amiga hw",   UFT_EMU_UNTESTED,
      "no Amiga drive available; see UFT-008 (HIL bench delegated)", NULL, false },
};

const uft_format_plugin_t uft_format_plugin_adf = {
    .name = "ADF", .description = "Amiga Disk File",
    .extensions = "adf", .format = UFT_FORMAT_ADF,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = adf_plugin_probe, .open = adf_open,
    .close = adf_close, .read_track = adf_read_track,
    .write_track = adf_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* AmigaDOS Rom Kernel Manual covers layout; not every variant */
    .features = adf_features,
    .feature_count = sizeof(adf_features) / sizeof(adf_features[0]),
    .compat_entries = adf_compat,
    .compat_count = sizeof(adf_compat) / sizeof(adf_compat[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(adf)
