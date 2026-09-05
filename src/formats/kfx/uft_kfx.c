/**
 * @file uft_kfx.c
 * @brief KryoFlux Stream Format Plugin
 *
 * Plugin-B wrapper around the existing kf_stream_parse() API in
 * uft_kfstream_air.c. KryoFlux stream files contain raw flux timing
 * data captured at ~24MHz (SCK clock).
 *
 * Each stream file represents one track (one revolution or more).
 * Track number is encoded in the filename (trackNN.S.raw).
 *
 * Reference: KryoFlux Stream Protocol, DTC documentation
 */

#include "uft/uft_format_common.h"
#include "uft/formats/kryoflux_checker.h"
#include "uft/uft_log.h"

/* ============================================================================
 * Forward declarations from uft_kfstream_air.c
 * ============================================================================ */

/* Status codes */
typedef enum {
    KFX_OK = 0,
    KFX_ERROR = -1
} kfx_status_t;

/* Stream data (simplified — full struct in uft_kfstream_air.c) */
typedef struct {
    double   sck_value;
    double   ick_value;
    double  *flux_values;       /* Flux intervals in seconds */
    size_t  *flux_stream_pos;
    size_t   flux_count;
    size_t   index_count;
    bool     valid;
    int      status;
    /* ... more fields exist but we only need flux_values/count */
} kf_stream_minimal_t;

/* These are defined in uft_kfstream_air.c */
extern int  kf_stream_parse(const uint8_t *data, size_t size, void *stream);
extern void kf_stream_free(void *stream);

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint8_t    *file_data;
    size_t      file_size;
} kfx_data_t;

/* ============================================================================
 * probe — look for KryoFlux OOB marker (0x0D) in first 512 bytes
 * ============================================================================ */

bool kfx_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)file_size;
    if (size < 16) return false;

    /* MF-919 (P3-171) — HIER WURDE DAS BYTE 0x0D GEZAEHLT.
     *
     * Der Rumpf lautete: `for (i..scan) if (data[i]==0x0D) oob_count++;`
     * und danach `>= 2 -> 45, true` / `>= 1 -> 35, true`. In 512
     * Zufallsbytes stehen erwartungsgemaess zwei 0x0D — die Sonde
     * konnte also NIE „nein" sagen. Gemessen (MF-893): 4097 Byte
     * Pseudozufall wurden angenommen und als `1 Zylinder x 1 Kopf,
     * 1 Sektor` gemeldet.
     *
     * MF-729 hatte die ZAHLEN gesenkt (80/40 -> 45/35). Das war
     * richtig und reichte nicht: eine gesenkte Konfidenz macht aus
     * einem Erkenner ohne „nein" keinen Erkenner. `uft_disk_open()`
     * nimmt den Sieger, nicht den Selbstbewussten.
     *
     * Jetzt wird STRUKTUR gelesen — und zwar von Code, den dieser Baum
     * seit jeher hat und nie gerufen hat: `uft_kfc_check_stream()` in
     * `src/formats/kryoflux/uft_kryoflux_checker.c` laeuft die
     * OOB-Kette ab und haelt die in den Bloecken eingebettete
     * Stromposition gegen die eigene Zaehlung. Das ist ein
     * 32-Bit-Vergleich; Zufall besteht ihn praktisch nicht.
     *
     * Der frueher hier stehende Verweis auf FMT-20 („dafuer braucht es
     * die Stream-Spezifikation, dtc ist nicht vorhanden") ist damit
     * ueberholt: die Kenntnis liegt ZWEIMAL im Baum, unabhaengig —
     * der Pruefer oben und `src/a8rawconv/rawdiskkf.cpp` (Avery Lee,
     * GPL-2.0-or-later). Beide stimmen in allen Opcodes ueberein.
     * Messung vor Plan.
     *
     * Die Sonde bekommt bewusst den GANZEN Puffer, nicht die ersten
     * 512 Byte: die Kette laesst sich nur als Ganzes pruefen — wer bei
     * 512 abschneidet, bricht sie mitten im Block ab und misst nichts.
     */
    uint32_t oob = 0, index = 0;
    if (!uft_kfc_stream_is_valid(data, size, &oob, &index)) {
        return false;
    }

    /* Band 50..79 = „Struktur gelesen" (MF-729). NICHT hoeher: ein
     * KryoFlux-Strom hat keine Kennung am Dateianfang, es gibt also
     * kein Merkmal zu treffen — nur einen Aufbau, der aufgeht. */
    *confidence = 75;
    return true;
}

/* ============================================================================
 * open — read entire file into memory
 * ============================================================================ */

static uft_error_t kfx_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    (void)read_only;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data) return UFT_ERROR_FILE_OPEN;

    if (file_size < 16) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* MF-919 (P3-171): hier stand NICHTS ausser der Groessenpruefung.
     * Wer dieses Plugin ausdruecklich waehlte, bekam fuer JEDE Datei
     * ab 16 Byte ein `UFT_OK` und danach eine erfundene Geometrie.
     * Die Sonde allein reicht nicht — sie ist eine Empfehlung, `open()`
     * ist die Tuer. */
    uint32_t oob = 0, index = 0;
    if (!uft_kfc_stream_is_valid(file_data, file_size, &oob, &index)) {
        UFT_WARN("KFX: '%s' ist kein KryoFlux-Strom "
                 "(%u OOB-Bloecke, %u Indexmarken, Positionskette nicht "
                 "schluessig)", path, oob, index);
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    kfx_data_t *pdata = calloc(1, sizeof(kfx_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    pdata->file_data = file_data;
    pdata->file_size = file_size;

    disk->plugin_data = pdata;
    /* KryoFlux: eine Datei = eine Spur; die Geometrie der Diskette ist
     * aus einer einzelnen Spurdatei nicht ableitbar.
     *
     * MF-919: hier stand `sectors = 1` und `total_sectors = 1`. Dieses
     * Plugin dekodiert KEINEN Sektor — es haelt einen Flussstrom. Eine
     * Eins an dieser Stelle ist eine Zusage an jeden Aufrufer, der die
     * Geometrie liest, und sie war unwahr. */
    disk->geometry.cylinders     = 1;
    disk->geometry.heads         = 1;
    disk->geometry.sectors       = 0;   /* nichts dekodiert */
    disk->geometry.sector_size   = 0;   /* Fluss = variabel */
    disk->geometry.total_sectors = 0;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void kfx_close(uft_disk_t *disk)
{
    kfx_data_t *pdata = disk->plugin_data;
    if (pdata) {
        free(pdata->file_data);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — parse flux stream and return raw data
 * ============================================================================ */

static uft_error_t kfx_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    kfx_data_t *pdata = disk->plugin_data;
    if (!pdata || cyl != 0 || head != 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* MF-919 (P3-171) — HIER WURDE EIN SEKTOR ERFUNDEN.
     *
     * Der Rumpf lautete:
     *
     *     uint16_t chunk = (file_size > 65535) ? 65535 : file_size;
     *     uft_format_add_sector(track, 0, file_data, chunk, 0, 0);
     *
     * Damit wurden die ROHEN DATEIBYTES eines Flussstroms als „Sektor 0"
     * ausgegeben. Ein KryoFlux-Strom ist Fluss: Zeitabstaende zwischen
     * Magnetuebergaengen, verschachtelt mit OOB-Bloecken. Er enthaelt
     * keinen Sektor, bevor ihn jemand dekodiert hat — und dieses Plugin
     * dekodiert ihn nicht.
     *
     * Das ist die Klasse aus MF-883, nur andersherum: dort wurde ein
     * Schreibvorgang gemeldet, der nicht stattfand; hier ein Sektor,
     * der nicht gelesen wurde. Und es kam obendrein mit einer stillen
     * Kuerzung: alles ueber 65535 Byte fiel weg (MF-877-Klasse).
     *
     * Was jetzt passiert: der Strom wird als Rohdaten der Spur
     * durchgereicht — VOLLSTAENDIG, ohne Kuerzung — und als das
     * benannt, was er ist. Kein Sektor, keine Erfindung.
     *
     * Was damit NICHT geloest ist und ausdruecklich offen bleibt: die
     * Zellzeiten aus dem Strom zu gewinnen. Der Pruefer hat mit
     * `extract_flux_values()` bereits eine Umsetzung dafuer; sie an
     * `track->flux_*` zu haengen ist ein eigener Eingriff mit eigener
     * Messung. Bis dahin gilt die Merkmalstafel unten. */
    uint8_t *kopie = malloc(pdata->file_size);
    if (!kopie) return UFT_ERROR_NO_MEMORY;
    memcpy(kopie, pdata->file_data, pdata->file_size);

    track->raw_data     = kopie;
    track->raw_size     = pdata->file_size;
    track->raw_len      = pdata->file_size;
    track->raw_capacity = pdata->file_size;
    track->owns_data    = true;
    track->encoding     = UFT_ENC_UNKNOWN;

    return UFT_OK;
}

/* ============================================================================
 * write_track — DOCUMENTED NOT_IMPLEMENTED per spec §1.3 Option 1.
 *
 * KFX is a KryoFlux streaming capture: raw flux transition timings
 * emitted by hardware, not sector data. Writing requires synthesizing
 * an analogous stream from decoded sectors.
 *
 * Implementation steps:
 *   1. Encode sectors → MFM bitstream (address marks, sync, CRC, gap
 *      bytes). Needs a proper MFM encoder — no `uft_encode_standard_track`
 *      or equivalent exists in the tree yet.
 *   2. Convert MFM cells → flux intervals in ns (bit-clock × {2,3,4}).
 *   3. Generate Oscillator Overflow (OVL) codes for intervals > 0xFFFF
 *      ticks; emit Nop1/Nop2/Nop3 between.
 *   4. Emit Index pulse (OOB 0x02) at revolution boundaries.
 *   5. Emit StreamEnd (OOB 0x03) with correct hw_status bits.
 *   6. Framing: pack into the KF stream format with the OOB escape.
 *
 * Estimated effort: ~300 lines total including the shared MFM encoder
 * — enough that a dedicated commit with test vectors is required
 * (spec §7.3 threshold).
 * Blocker: no MFM encoder in the tree + no round-trip test corpus
 * for KF streams.
 * Workaround: use SCP or HFE for flux-level writes.
 * ============================================================================ */

static uft_error_t kfx_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    (void)disk; (void)cyl; (void)head; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_kfx_features[] = {
    /* MF-919: „Read SUPPORTED" war zu viel gesagt. Der Strom wird
     * geoeffnet, strukturell geprueft und roh durchgereicht — dekodiert
     * wird er nicht. */
    { "Read", UFT_FEATURE_PARTIAL,
      "Strom wird strukturell geprueft und roh durchgereicht; "
      "keine Sektor- oder Zelldekodierung" },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_SUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_kfx = {
    .name         = "KFX",
    .description  = "KryoFlux Stream Format",
    .extensions   = "raw",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe        = kfx_probe,
    .open         = kfx_open,
    .close        = kfx_close,
    .read_track   = kfx_read_track,
    .write_track  = kfx_write_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_kfx_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_kfx_features) / sizeof(uft_format_plugin_kfx_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(kfx)
