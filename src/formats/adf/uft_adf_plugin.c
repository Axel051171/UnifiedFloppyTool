/**
 * @file uft_adf_plugin.c
 * @brief ADF (Amiga Disk File) Plugin-B — self-contained
 *
 * ADF is a headerless raw sector dump of Amiga floppy disks.
 * DD: 80 cyl × 2 heads × 11 spt × 512 = 901,120 bytes
 * HD: 80 cyl × 2 heads × 22 spt × 512 = 1,802,240 bytes
 */
#include "uft/uft_format_common.h"
#include "uft/uft_log.h"

#define ADF_DD_SIZE     901120
#define ADF_HD_SIZE     1802240
#define ADF_SECTOR_SIZE 512
#define ADF_SPT_DD      11
#define ADF_SPT_HD      22
#define ADF_TRACK_DD    (ADF_SPT_DD * ADF_SECTOR_SIZE)   /*  5632 */
#define ADF_TRACK_HD    (ADF_SPT_HD * ADF_SECTOR_SIZE)   /* 11264 */
#define ADF_TRACKS      160                              /* 80 Zyl. x 2 */

/**
 * @brief Wie vollstaendig ist diese Datei (MF-840)?
 *
 * ADF ist kopflos — die Groesse ist das einzige Merkmal ausser dem
 * Bootblock. Bis MF-840 verlangten Sonde und `open` **exakte**
 * Gleichheit mit 901 120 oder 1 802 240 Byte.
 *
 * Das verwirft einen dokumentierten Normalfall: `TransDisk` (Amiga,
 * ueber `trackdisk.device`) zieht mit `-s`/`-e` Spurbereiche ab, und
 * das Beispiel seines Autors lautet
 *
 *     transdisk >RAM:df1.adf.1 -d trackdisk 1 -s 0 -e 39
 *
 * — 40 Zylinder = 80 Spuren = **225 280 Byte**, mit einer `.2` als
 * Ergaenzung. Auf Amigas mit knapper RAM-Disk oder bei serieller
 * Uebertragung war das ueblich.
 *
 * Eine solche Datei traegt 80 **vollstaendige, gueltige** Spuren. Sie
 * zu verwerfen, weil 80 weitere fehlen, verliert alles statt nichts.
 *
 * Die drei Kurzfaelle sind an der Groesse unterscheidbar:
 *   Vielfaches der Spurlaenge      spurgenauer Teilabzug
 *   Vielfaches von 512, nicht das  an Blockgrenze abgebrochen
 *   kein Vielfaches von 512        KEIN ADF — an der Groesse nicht mehr
 *                                  erkennbar (siehe unten)
 */
typedef enum {
    ADF_EXT_NICHT_ADF = 0,
    ADF_EXT_VOLLSTAENDIG,
    ADF_EXT_TEILABZUG,        /* spurgenau kurz                     */
    ADF_EXT_BLOCKGENAU_KURZ   /* 512er-Vielfaches, nicht spurgenau  */
    /* Ein vierter Zustand „mitten im Block abgebrochen" waere denkbar,
     * ist hier aber ABSICHTLICH nicht aufgefuehrt: eine solche Datei
     * traegt kein Groessenmerkmal mehr und faellt auf ADF_EXT_NICHT_ADF.
     * Ein Aufzaehlungswert, den nie jemand erzeugt, ist dasselbe wie ein
     * Feld, das nie jemand setzt (MF-831). */
} adf_extent_t;

typedef struct {
    adf_extent_t extent;
    uint8_t      spt;             /* 11 oder 22            */
    unsigned     tracks_present;  /* vollstaendige Spuren  */
    size_t       rest;            /* Byte hinter der letzten ganzen Spur */
} adf_shape_t;

static adf_extent_t adf_classify(size_t len, adf_shape_t *out)
{
    memset(out, 0, sizeof *out);
    if (len == 0) return out->extent = ADF_EXT_NICHT_ADF;

    /* Die VOLLSTAENDIGEN Groessen zuerst, und zwar beide, BEVOR
     * ueberhaupt an Teilabzuege gedacht wird.
     *
     * Der Grund ist eine Falle, in die die erste Fassung dieses Codes
     * gelaufen ist: 901 120 (volle DD-Diskette) ist zugleich ein
     * Vielfaches der HD-Spurlaenge (901120 / 11264 = 80). Eine Regel
     * „HD zuerst pruefen" stuft die vollstaendige DD-Diskette damit als
     * HD-TEILABZUG ein — gefangen von
     * `vollstaendiges_adf_bleibt_wie_bisher`. */
    if (len == (size_t)ADF_DD_SIZE) {
        out->spt = ADF_SPT_DD;
        out->tracks_present = ADF_TRACKS;
        return out->extent = ADF_EXT_VOLLSTAENDIG;
    }
    if (len == (size_t)ADF_HD_SIZE) {
        out->spt = ADF_SPT_HD;
        out->tracks_present = ADF_TRACKS;
        return out->extent = ADF_EXT_VOLLSTAENDIG;
    }

    /* Teilabzuege. DD zuerst, weil es der weit haeufigere Fall und der
     * von TransDisk dokumentierte ist. Eine Laenge, die durch BEIDE
     * Spurlaengen teilbar ist (z.B. 225 280 = 40 DD-Spuren = 20
     * HD-Spuren), ist echt mehrdeutig — die Groesse allein entscheidet
     * das nicht, und deshalb bleibt die Konfidenz dafuer unter 50. */
    if (len < (size_t)ADF_DD_SIZE && (len % ADF_TRACK_DD) == 0) {
        out->spt = ADF_SPT_DD;
        out->tracks_present = (unsigned)(len / ADF_TRACK_DD);
        return out->extent = ADF_EXT_TEILABZUG;
    }
    if (len < (size_t)ADF_HD_SIZE && (len % ADF_TRACK_HD) == 0) {
        out->spt = ADF_SPT_HD;
        out->tracks_present = (unsigned)(len / ADF_TRACK_HD);
        return out->extent = ADF_EXT_TEILABZUG;
    }
    if (len > (size_t)ADF_TRACK_HD * ADF_TRACKS)
        return out->extent = ADF_EXT_NICHT_ADF;
    if (len < (size_t)ADF_TRACK_DD)
        return out->extent = ADF_EXT_NICHT_ADF;

    /* Eine Datei, die nicht einmal ein Vielfaches der SEKTORgroesse ist,
     * traegt kein Merkmal mehr, an dem sie als ADF erkennbar waere. Der
     * Fall „mitten im Block abgebrochen" ist real, aber er ist an der
     * GROESSE nicht identifizierbar — und ihn trotzdem anzunehmen hiesse,
     * fast jede Datei dieser Groessenordnung zu beanspruchen.
     * `test_plugin_probe_real` verlangt genau das (901 119 muss durchfallen),
     * und die Forderung ist richtig. */
    if ((len % ADF_SECTOR_SIZE) != 0)
        return out->extent = ADF_EXT_NICHT_ADF;

    out->spt = ADF_SPT_DD;
    out->tracks_present = (unsigned)(len / ADF_TRACK_DD);
    out->rest = len % ADF_TRACK_DD;
    return out->extent = ADF_EXT_BLOCKGENAU_KURZ;
}

typedef struct {
    FILE    *file;
    uint8_t  spt;
    /* MF-840: wie viele Spuren die DATEI traegt. Die Geometrie meldet
     * weiter die ganze Diskette — die Datei ist ein Ausschnitt daraus,
     * nicht eine kleinere Diskette. */
    unsigned tracks_present;
    adf_extent_t extent;
} adf_pd_t;

static bool adf_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    adf_shape_t shape;
    switch (adf_classify(file_size, &shape)) {
    case ADF_EXT_NICHT_ADF:
        return false;
    case ADF_EXT_VOLLSTAENDIG:
        *confidence = 45;             /* MF-729: nur die Groesse */
        break;
    case ADF_EXT_TEILABZUG:
        /* Spurgenau kurz. Die Groesse allein sagt hier weniger als bei
         * einer vollen Diskette — ein Vielfaches von 5632 ist auch
         * anderes. Unterhalb von 50, also kein Merkmalsanspruch. */
        *confidence = 35;
        break;
    case ADF_EXT_BLOCKGENAU_KURZ:
        /* 512er-Vielfaches, aber nicht spurgenau — abgebrochene
         * Uebertragung. Nur mitnehmen, damit die Datei ueberhaupt
         * angeboten wird. Der Bootblock hebt das ABSICHTLICH nicht an:
         * `DOS` sind drei Byte, und die Groesse traegt hier nichts. */
        *confidence = 25;
        return true;
    default:
        return false;
    }
    /* Amiga bootblock: "DOS\0" at offset 0. Ein Teilabzug ab Spur 0 hat
     * ihn; einer ab `-s 40` nicht — das ist kein Mangel, sondern die
     * zweite Haelfte eines Paares. */
    if (size >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S')
        *confidence = (shape.extent == ADF_EXT_VOLLSTAENDIG) ? 95 : 70;
    return true;
}

static uft_error_t adf_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    adf_shape_t shape;
    if (adf_classify((size_t)sz, &shape) == ADF_EXT_NICHT_ADF) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    uint8_t spt = shape.spt;

    /* MF-840: den Zustand MELDEN, nicht verschweigen. */
    switch (shape.extent) {
    case ADF_EXT_TEILABZUG:
        UFT_WARN("ADF-Teilabzug: %u von %u Spuren vorhanden (%u Sekt./Spur). "
                 "Erzeugt von einem Werkzeug mit Spurbereichsangabe (z.B. "
                 "TransDisk -s/-e); eine ergaenzende Datei mit den restlichen "
                 "Spuren kann daneben liegen.",
                 shape.tracks_present, (unsigned)ADF_TRACKS, spt);
        break;
    case ADF_EXT_BLOCKGENAU_KURZ:
        UFT_WARN("ADF endet an einer Blockgrenze, aber nicht an einer "
                 "Spurgrenze: %u vollstaendige Spuren plus %zu Byte — "
                 "Uebertragung vermutlich abgebrochen",
                 shape.tracks_present, shape.rest);
        break;
    default: break;
    }

    adf_pd_t *p = calloc(1, sizeof(adf_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->spt = spt;
    p->tracks_present = shape.tracks_present;
    p->extent = shape.extent;

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

    /* MF-840: eine Spur, die die DATEI nicht traegt, ist FEHLEND — nicht
     * leer. Hier stand darunter
     *
     *     if (fread(...) != ADF_SECTOR_SIZE) memset(buf, 0xE5, ...);
     *
     * und damit kam eine Spur jenseits des Dateiendes als 0xE5-Fuellung
     * mit UFT_OK zurueck. 0xE5 ist die AmigaDOS-Formatfuellung — ein
     * Fehlschlag war von einer leeren, formatierten Spur nicht zu
     * unterscheiden. Dieselbe Klasse, die MF-837 aus dem DMS-Plugin
     * entfernt hat: der Verlust wird als Datum ausgegeben.
     *
     * Ohne diese Schranke waere das Annehmen von Teilabzuegen (oben) ein
     * Rueckschritt gewesen — die 80 fehlenden Spuren waeren als leere
     * Diskette erschienen. */
    unsigned trk = (unsigned)(cyl * 2 + head);
    if (trk >= p->tracks_present) {
        UFT_WARN("ADF: Spur %u (Zyl. %d, Kopf %d) liegt jenseits des "
                 "Dateiendes — die Datei traegt %u von %u Spuren",
                 trk, cyl, head, p->tracks_present, (unsigned)ADF_TRACKS);
        return UFT_ERROR_NOT_FOUND;
    }

    long off = (long)((cyl * 2 + head) * p->spt * ADF_SECTOR_SIZE);
    uint8_t buf[ADF_SECTOR_SIZE];
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, off + s * ADF_SECTOR_SIZE, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, ADF_SECTOR_SIZE, p->file) != ADF_SECTOR_SIZE) {
            /* Innerhalb der gezaehlten Spuren darf das nicht vorkommen —
             * wenn doch, ist die Datei unter uns geschrumpft. Melden,
             * nicht fuellen. */
            UFT_WARN("ADF: Sektor %d der Spur %u nicht lesbar, obwohl die "
                     "Datei ihn tragen sollte", s, trk);
            return UFT_ERROR_IO;
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
