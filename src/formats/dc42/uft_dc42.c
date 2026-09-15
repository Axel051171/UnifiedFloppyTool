/**
 * @file uft_dc42.c
 * @brief Apple DiskCopy 4.2 Plugin
 *
 * DiskCopy 4.2 was Apple's standard disk image format for Mac OS.
 * Used for 400K, 800K, and 1.44MB Macintosh floppy images.
 *
 * Header layout (84 bytes):
 *   Offset  Size  Description
 *   0x00    1     Name length (Pascal string)
 *   0x01    63    Disk name
 *   0x40    4     Data size (BE32)
 *   0x44    4     Tag size (BE32) — 0 for most images
 *   0x48    4     Data checksum (BE32)
 *   0x4C    4     Tag checksum (BE32)
 *   0x50    1     Disk format (0=400K GCR, 1=800K GCR, 2=720K MFM, 3=1440K MFM)
 *   0x51    1     Format byte (0x12=Mac 400K, 0x22=Mac 800K, 0x24=ProDOS 800K)
 *   0x52    2     Magic (0x0100)
 *
 * Data follows immediately at offset 84.
 *
 * Reference: Apple DiskCopy 4.2 format (Inside Macintosh)
 *
 * ── MF-1140: die 3,5"-GCR-Anordnung ist ZONIERT, und 1590 von 1600
 *    Sektoren kamen von der falschen Stelle ─────────────────────────────
 *
 * `dc42_get_geometry()` setzte fuer 400K und 800K GCR fest `spt = 10` —
 * mit dem eigenen Kommentar „variable SPT, use avg". Eine
 * Apple-3,5"-Diskette hat aber eine **Zonentafel**: je 16 Spuren 12,
 * 11, 10, 9 und 8 Sektoren. Referenz ist MAMEs
 * `src/lib/formats/ap_dsk35.cpp` (BSD-3-Clause), `load()` Z. 558-562,
 * `int ns = 12 - (track/16);` — Kanal *Spec* nach MF-695, gelesen und
 * nicht uebernommen. In diesem Baum ist genau diese Tafel seit MF-1031
 * an `2img` gemessen und gegen floptool abgenommen (T1b).
 *
 * **Die Summe geht dabei auf, und das ist der ganze Punkt:**
 * 16 x (12+11+10+9+8) = 800 Sektoren je Seite, und 80 x 10 = 800. Die
 * gemeldete Gesamtzahl war also richtig, die Dateigroesse stimmte, und
 * keine Groessenpruefung konnte etwas sehen — waehrend fast jede Spur
 * an der falschen Stelle lag. Das ist woertlich die Lehre aus MF-1026
 * (`victor9k`: zwei Zonengrenzen um eins daneben, +1 und -1 heben sich
 * auf, 22 Spuren 512 Byte zu hoch gelesen): **eine Summe, die aufgeht,
 * sagt nichts ueber die Verteilung darin.**
 *
 * Gemessen am Vorzustand mit einer 800K-Datei, deren jeder Sektor seine
 * eigene Lage nennt:
 *
 *     open -> rc=0, gemeldet 80 Zylinder / 2 Koepfe / 10 Sektoren
 *     an ihrer eigenen Marke :   10
 *     abweichend             : 1590
 *     nicht lesbar           :    0
 *     erste Abweichung (0,1,0): erwartet "C00 H1 S00",
 *                               gelesen  "C00 H0 S10"
 *
 * Richtig waren allein die ersten zehn Sektoren von Spur 0 Kopf 0 — der
 * einzige Ort, an dem beide Modelle denselben Versatz ergeben. Nichts
 * wurde als fehlend gekennzeichnet, nichts abgesagt.
 *
 * **Und warum der Durchschreibfall das nicht gefangen hat:** Lese- und
 * Schreibseite benutzten DIESELBE falsche Versatzformel, ein Rundlauf
 * ist damit in sich stimmig. Das ist die Klasse MF-1009/MF-1028 (Packer
 * und Entpacker als Spiegelbilder derselben Erfindung) und genau der
 * Grund, warum `docs/WRITE_VERIFICATION_TIERS.md` die Stufe W1 von W2
 * trennt: W1 belegt, dass die Aenderung die Datei erreicht, nicht dass
 * sie an der richtigen Stelle landet.
 *
 * Der Versatz kommt jetzt aus `uft_2img_track_offset()` und die
 * Sektorzahl je Spur aus `uft_2img_zone_spt()` — **keine zweite Kopie
 * der Tafel**, sondern die bereits abgenommene. Dasselbe hat MF-1034
 * fuer `posix` getan, das seinen Versatz von `uft_logical_track_index()`
 * nimmt; eine zweite Kopie waere die Lage aus MF-1015 (drei
 * Pruefsummen) und MF-1026 (drei Victor-Geometrien). Dass die Funktion
 * im `2img`-Modul wohnt, ist historisch und kein Sachargument — ein
 * gemeinsamer Ort fuer die Apple-3,5"-Tafel waere saubererer; siehe
 * P3-395.
 *
 * ── Zweiter Befund: der angesagten Datenlaenge wurde geglaubt ─────────
 *
 * `data_size` aus dem Kopf entschied die Geometrie und war zugleich die
 * einzige Schranke — die WIRKLICHE Dateigroesse fragte niemand ab, und
 * `dc42_probe()` begann mit `(void)file_size;`. Damit war die kanonische
 * Strukturprobe `84 + data_size + tag_size == Dateigroesse` gar nicht
 * nachbaubar; das ist die MF-1029-Falle, gezaehlt der **achte** Fall in
 * diesem Baum, und dieselbe Gestalt wie B4 bei `2img` (MF-1031).
 *
 * Bei bekanntem Formatbyte wurde `data_size` ausserdem **gar nicht**
 * gegengeprueft: eine Datei, die sich als 800K ausgibt und 1 KB Daten
 * traegt, wurde mit 80 x 2 und 1600 angesagten Sektoren geoeffnet
 * (Gestalt MF-1019 bei `dim`, MF-1038 bei `fds`).
 *
 * ── Was ausdruecklich NICHT geaendert wird ────────────────────────────
 *
 * Die gespeicherte Datenpruefsumme bei 0x48 wird beim Oeffnen weiterhin
 * **nicht** geprueft, obwohl `dc42_data_checksum()` in dieser Datei
 * steht und die Schreibseite sie fuehrt. Eine Abweichung abzuweisen
 * waere gegen MF-830 („ein Befund darf den Zugang nicht versperren");
 * sie zu melden braucht einen Kanal fuer Befunde am Abbild, den es hier
 * noch nicht gibt. Steht als P3-396.
 */

#include "uft/uft_format_common.h"
/* MF-1140: die Apple-3,5"-Zonentafel und die zonierte Versatzrechnung
 * liegen im `2img`-Modul und sind dort seit MF-1031 gegen floptool
 * abgenommen. Abhaengigkeit statt Duplikat — Muster von MF-1034
 * (`posix` -> `uft_logical`). */
#include "uft/formats/uft_2img.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DC42_HEADER_SIZE    84
#define DC42_MAGIC          0x0100
#define DC42_SECTOR_SIZE    512
#define DC42_MAX_NAME       63

/* Disk format codes */
#define DC42_FMT_400K_GCR   0x00    /* Mac 400K single-sided GCR */
#define DC42_FMT_800K_GCR   0x01    /* Mac 800K double-sided GCR */
#define DC42_FMT_720K_MFM   0x02    /* 720K MFM (PC compat) */
#define DC42_FMT_1440K_MFM  0x03    /* 1.44M MFM (SuperDrive) */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint32_t    data_size;
    uint32_t    tag_size;
    uint8_t     disk_format;
    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     sectors_per_track;   /* zoniert: GROESSTE Zone (12) */
    /* MF-1140, am Ende angehaengt (Muster MF-866/993/1136) */
    int         zoniert;             /* 1 = Apple 3,5" mit Zonentafel */
    uint32_t    sektoren_gesamt;     /* wahre Summe, nicht cyl*heads*spt */
    uint32_t    nutzbar;             /* min(data_size, wirkliche Datei) */
} dc42_data_t;

/**
 * Sektoren dieser Spur. Zoniert kommt die Zahl aus der abgenommenen
 * Tafel des `2img`-Moduls, sonst aus der festen Geometrie.
 */
static int dc42_spt(const dc42_data_t *p, int cyl)
{
    return p->zoniert ? uft_2img_zone_spt(cyl) : p->sectors_per_track;
}

/* ============================================================================
 * Geometry from format code
 * ============================================================================ */

/**
 * Geometrie aus dem Formatbyte, mit der Datenlaenge als Gegenprobe.
 *
 * MF-1140: `*spt` traegt bei zonierten Abbildern die GROESSTE Zone (12)
 * und `*gesamt` die wahre Sektorsumme — die Konvention, die `2img` seit
 * MF-1031 fuehrt (`IMG2_TAFEL.spt_max` / `.sektoren`). `cyl*heads*spt`
 * waere dort 1920 und damit falsch.
 *
 * Und die Datenlaenge wird jetzt GEGENGEPRUEFT statt geglaubt: vorher
 * bestimmte das Formatbyte die Geometrie allein, sodass eine Datei mit
 * `fmt = 800K` und 1 KB Daten mit 1600 angesagten Sektoren aufging
 * (Gestalt MF-1019/MF-1038).
 */
static bool dc42_get_geometry(uint8_t fmt, uint32_t data_size,
                              uint8_t *cyl, uint8_t *heads, uint8_t *spt,
                              int *zoniert, uint32_t *gesamt)
{
    uint32_t erwartet = 0;

    *zoniert = 0;
    switch (fmt) {
        case DC42_FMT_400K_GCR:
            /* 80 Spuren, EINE Seite, Zonentafel 12/11/10/9/8 = 800 */
            *cyl = 80; *heads = 1; *spt = 12; *zoniert = 1;
            *gesamt = 800; erwartet = 800u * DC42_SECTOR_SIZE;
            break;
        case DC42_FMT_800K_GCR:
            *cyl = 80; *heads = 2; *spt = 12; *zoniert = 1;
            *gesamt = 1600; erwartet = 1600u * DC42_SECTOR_SIZE;
            break;
        case DC42_FMT_720K_MFM:
            *cyl = 80; *heads = 2; *spt = 9;
            *gesamt = 1440; erwartet = 1440u * DC42_SECTOR_SIZE;
            break;
        case DC42_FMT_1440K_MFM:
            *cyl = 80; *heads = 2; *spt = 18;
            *gesamt = 2880; erwartet = 2880u * DC42_SECTOR_SIZE;
            break;
        default:
            erwartet = 0;
            break;
    }

    if (erwartet != 0) {
        /* Das Formatbyte sagt eine Laenge an; traegt die Datei sie
         * nicht, ist die Datei nicht das, was sie behauptet. Absagen
         * statt eine Geometrie zu melden, die nicht hineinpasst. */
        if (data_size != erwartet) return false;
        return true;
    }

    /* Rueckfall: aus der Datenlaenge, wenn sie restlos aufgeht. */
    if (data_size == 0 || data_size % DC42_SECTOR_SIZE != 0) return false;
    uint32_t total = data_size / DC42_SECTOR_SIZE;

    if (total == 800)  { *cyl = 80; *heads = 1; *spt = 12; *zoniert = 1;
                         *gesamt = 800;  return true; }
    if (total == 1600) { *cyl = 80; *heads = 2; *spt = 12; *zoniert = 1;
                         *gesamt = 1600; return true; }
    if (total == 1440) { *cyl = 80; *heads = 2; *spt = 9;
                         *gesamt = 1440; return true; }
    if (total == 2880) { *cyl = 80; *heads = 2; *spt = 18;
                         *gesamt = 2880; return true; }

    return false;
}

/* ============================================================================
 * probe
 * ============================================================================ */

bool dc42_probe(const uint8_t *data, size_t size, size_t file_size,
                int *confidence)
{
    if (size < DC42_HEADER_SIZE) return false;

    /* Check magic at offset 82 (BE16) */
    uint16_t magic = ((uint16_t)data[82] << 8) | data[83];
    if (magic != DC42_MAGIC) return false;

    /* Name length must be sane */
    if (data[0] > DC42_MAX_NAME) return false;

    /* Data size must be non-zero */
    uint32_t data_size = uft_read_be32(data + 0x40);
    if (data_size == 0) return false;

    /* MF-1140: hier stand `(void)file_size;` — und damit war die
     * kanonische Strukturprobe des Formats nicht nachbaubar. Der Kopf
     * sagt beide Laengen an, also MUSS
     *
     *     84 + data_size + tag_size == Dateigroesse
     *
     * gelten. Achter Fall der MF-1029-Falle in diesem Baum, und
     * dieselbe Gestalt wie B4 bei `2img` (MF-1031), wo dieselbe Probe
     * am verworfenen `file_size` scheiterte.
     *
     * Die Kennung allein traegt hier wenig: `0x0100` sind ZWEI Byte,
     * eines davon null — das ist nach MF-729 kein Merkmal, sondern
     * Struktur. Mit der Laengengleichung wird daraus eine echte
     * Aussage; ohne sie bleibt die Konfidenz im Band „Struktur
     * gelesen". */
    uint32_t tag_size = uft_read_be32(data + 0x44);
    int laenge_stimmt = 0;
    if (file_size > 0) {
        uint64_t erwartet = (uint64_t)DC42_HEADER_SIZE
                          + (uint64_t)data_size + (uint64_t)tag_size;
        if (erwartet != (uint64_t)file_size) return false;
        laenge_stimmt = 1;
    }

    /* Disk format must be known */
    uint8_t fmt = data[0x50];
    if (fmt > DC42_FMT_1440K_MFM) {
        *confidence = 50;
    } else {
        /* 92 nur, wenn die Laengengleichung wirklich geprueft wurde;
         * sonst sind es Kennung und Formatbyte, also Struktur (MF-729). */
        *confidence = laenge_stimmt ? 92 : 70;
    }
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t dc42_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[DC42_HEADER_SIZE];
    if (fread(hdr, 1, DC42_HEADER_SIZE, f) != DC42_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    uint16_t magic = ((uint16_t)hdr[82] << 8) | hdr[83];
    if (magic != DC42_MAGIC) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint32_t data_size = uft_read_be32(hdr + 0x40);
    uint32_t tag_size = uft_read_be32(hdr + 0x44);
    uint8_t fmt = hdr[0x50];

    /* MF-1140: die WIRKLICHE Dateigroesse holen. Vorher war
     * `data_size` — eine Angabe AUS DEM KOPF — zugleich Geometriequelle
     * und einzige Schranke; die Datei selbst wurde nie gefragt. */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long dateigroesse = ftell(f);
    if (dateigroesse < DC42_HEADER_SIZE) { fclose(f); return UFT_ERROR_IO; }

    uint8_t cyl, heads, spt;
    int zoniert = 0;
    uint32_t gesamt = 0;
    if (!dc42_get_geometry(fmt, data_size, &cyl, &heads, &spt,
                           &zoniert, &gesamt)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    dc42_data_t *pdata = calloc(1, sizeof(dc42_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->data_size = data_size;
    pdata->tag_size = tag_size;
    pdata->disk_format = fmt;
    pdata->cylinders = cyl;
    pdata->heads = heads;
    pdata->sectors_per_track = spt;
    pdata->zoniert = zoniert;
    pdata->sektoren_gesamt = gesamt;

    /* Die Schranke ist das MINIMUM aus angesagter und wirklicher
     * Laenge. Eine gekuerzte Datei liefert damit die Spuren, die
     * wirklich da sind, und sagt fuer die uebrigen ab — statt hinter
     * das Dateiende zu lesen (Klasse MF-1040: kuerzen und OK melden
     * ist der Defekt) oder eine Geometrie zu melden, die nicht
     * hineinpasst (Klasse MF-1019/MF-1038). */
    uint32_t vorhanden = (uint32_t)(dateigroesse - DC42_HEADER_SIZE);
    pdata->nutzbar = (data_size < vorhanden) ? data_size : vorhanden;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    /* Zoniert ist das die GROESSTE Zone; die Spur nennt ihre eigene
     * Sektorzahl selbst (`2img`-Konvention seit MF-1031). */
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = DC42_SECTOR_SIZE;
    disk->geometry.total_sectors = gesamt;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void dc42_close(uft_disk_t *disk)
{
    dc42_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t dc42_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dc42_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    /* MF-1140: die negative Koordinate wird ABGEWIESEN, bevor mit ihr
     * gerechnet wird. Vorher stand hier nur die obere Schranke, und
     * `(uint32_t)cyl` machte aus -1 einen riesigen Index — die Klasse
     * MF-519/MF-529/MF-931, in diesem Baum viermal gemessen.
     * `uft_2img_track_offset()` prueft es ebenfalls und gibt -1; beide
     * Wege sind absichtlich da, weil die Schranke hier auch die
     * Sektorzahl deckt. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    const int spt = dc42_spt(pdata, cyl);
    if (spt <= 0) return UFT_ERROR_INVALID_STATE;

    /* Versatz aus der abgenommenen Rechnung des `2img`-Moduls: nicht
     * zoniert linear, zoniert als Praefixsumme ueber die Zonentafel. */
    long offset = uft_2img_track_offset(cyl, head, pdata->zoniert,
                                        pdata->heads,
                                        pdata->sectors_per_track,
                                        DC42_SECTOR_SIZE,
                                        DC42_HEADER_SIZE);
    if (offset < 0) return UFT_ERROR_INVALID_PARAM;

    /* Schranke gegen das, was WIRKLICH da ist (MF-1140) */
    uint64_t track_end = (uint64_t)(offset - DC42_HEADER_SIZE)
                       + (uint64_t)spt * DC42_SECTOR_SIZE;
    if (track_end > (uint64_t)pdata->nutzbar)
        return UFT_ERROR_INVALID_STATE;

    if (fseek(pdata->file, offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t buf[DC42_SECTOR_SIZE];

    for (int s = 0; s < spt; s++) {
        if (fread(buf, 1, DC42_SECTOR_SIZE, pdata->file) != DC42_SECTOR_SIZE)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              DC42_SECTOR_SIZE,
                              (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track
 * ============================================================================ */

/* Apple DiskCopy 4.2 data checksum: for each big-endian 16-bit word, add it
 * to a 32-bit accumulator (ignoring overflow) then rotate the accumulator
 * right by one bit. Verified against the DiscFerret / Mini vMac references. */
static uint32_t dc42_data_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint16_t w = (uint16_t)(((uint16_t)data[i] << 8) | data[i + 1]);
        sum += w;
        sum = (sum >> 1) | (sum << 31);   /* rotate right 1 (32-bit) */
    }
    return sum;
}

/* Recompute the whole-data-fork checksum and update the header at 0x48.
 * write_track modifies sector data, so leaving the stored checksum stale
 * produces a DC42 image that spec-conformant tools (real DiskCopy, emulators)
 * flag as corrupt — silent corruption of the image's integrity metadata. */
static void dc42_update_data_checksum(dc42_data_t *pdata)
{
    if (!pdata || !pdata->file || pdata->data_size == 0) return;
    uint8_t *buf = (uint8_t *)malloc(pdata->data_size);
    if (!buf) return;
    fflush(pdata->file);
    if (fseek(pdata->file, DC42_HEADER_SIZE, SEEK_SET) == 0 &&
        fread(buf, 1, pdata->data_size, pdata->file) == pdata->data_size) {
        uint32_t ck = dc42_data_checksum(buf, pdata->data_size);
        uint8_t be[4] = { (uint8_t)(ck >> 24), (uint8_t)(ck >> 16),
                          (uint8_t)(ck >> 8),  (uint8_t)ck };
        if (fseek(pdata->file, 0x48, SEEK_SET) == 0)
            fwrite(be, 1, 4, pdata->file);
        fflush(pdata->file);
    }
    free(buf);
}

static uft_error_t dc42_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    dc42_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* MF-1140: negative Koordinaten zuerst — beim SCHREIBEN wiegt das
     * schwerer als beim Lesen, weil der Index bestimmt, WOHIN
     * geschrieben wird (so begruendet in MF-529). */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    const int spt = dc42_spt(pdata, cyl);
    if (spt <= 0) return UFT_ERROR_INVALID_STATE;

    long offset = uft_2img_track_offset(cyl, head, pdata->zoniert,
                                        pdata->heads,
                                        pdata->sectors_per_track,
                                        DC42_SECTOR_SIZE,
                                        DC42_HEADER_SIZE);
    if (offset < 0) return UFT_ERROR_INVALID_PARAM;

    uint64_t track_end = (uint64_t)(offset - DC42_HEADER_SIZE)
                       + (uint64_t)spt * DC42_SECTOR_SIZE;
    if (track_end > (uint64_t)pdata->nutzbar)
        return UFT_ERROR_INVALID_STATE;

    for (size_t s = 0; s < track->sector_count && (int)s < spt; s++) {
        if (fseek(pdata->file, offset + (long)s * DC42_SECTOR_SIZE,
                  SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[DC42_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, DC42_SECTOR_SIZE);
            data = pad;
        }
        if (fwrite(data, 1, DC42_SECTOR_SIZE, pdata->file) != DC42_SECTOR_SIZE)
            return UFT_ERROR_IO;
    }
    /* Keep the DC42 data checksum in sync with the modified data. */
    dc42_update_data_checksum(pdata);
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_dc42_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dc42 = {
    .name         = "DC42",
    .description  = "Apple DiskCopy 4.2",
    .extensions   = "dc42;image;img",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DC42,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = dc42_probe,
    .open         = dc42_open,
    .close        = dc42_close,
    .read_track   = dc42_read_track,
    .write_track  = dc42_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dc42_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dc42_features) / sizeof(uft_format_plugin_dc42_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(dc42)
