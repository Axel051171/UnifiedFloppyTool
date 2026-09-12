/**
 * @file uft_fds_plugin.c
 * @brief Famicom Disk System (FDS): der fwNES-Behaelter, und die 36 Byte,
 *        die es nicht gibt (MF-1038)
 *
 * ── Der Aufbau, nach der benannten Beschreibung ──────────────────
 *
 * Quelle sind **zwei** Seiten des nesdev-Wiki — Kanal *Spec* nach
 * MF-695, gelesen, keine Zeile Code:
 *
 *   `FDS_file_format`: der fwNES-Kopf ist **16 Byte** gross, Bytes 0-3
 *     sind woertlich „Constant $46 $44 $53 $1A („FDS“ followed by MS-DOS
 *     end-of-file)", Byte 4 ist „Number of disk sides", Bytes 5-15 sind
 *     „Zero filled padding". Eine Seite ist **genau 65 500 Byte**. Und:
 *     „Some .FDS images may omit the header."
 *   `FDS_disk_format`: je Seite folgen die Bloecke 1, 2, 3, 4, 3, 4, ...
 *     Block 1 (Disk info) ist $00-$39 = 58 Byte **mit** 2-Byte-CRC,
 *     Block 2 (File amount) $00-$03 = 4 Byte, Block 3 (File header)
 *     $00-$11 = 18 Byte. In .FDS-Dateien fehlen CRCs und Luecken, also
 *     56 / 2 / 16 Byte. Block 1 traegt bei Versatz **$01** die
 *     Zeichenfolge „*NINTENDO-HVC*".
 *
 * Zweite Hand, nur gelesen: MAMEs `formats/nes_dsk.cpp` (BSD-3-Clause,
 * Miodrag Milanovic). Es erkennt **nur ueber die Dateigroesse** — 65516,
 * 131016 und 262016 mit Kennung, 65500, 131000 und 262000 ohne — und
 * vergleicht dabei nur **drei** Byte (`memcmp(header, "FDS", 3)`), nicht
 * die vier der Beschreibung. UFT verlangt alle vier; das ist strenger und
 * folgt der Quelle.
 *
 * ── Die Abbildung auf Spuren ist UFTs EIGENE Konvention ───────────
 *
 * FDS ist kein Spur/Sektor-Format. UFT legt je Seite eine „Spur" mit
 * **128 virtuellen 512-Byte-Sektoren** ab. Das steht in keiner Quelle —
 * es ist eine Hausregel fuer die Werkzeugkette, und sie wird hier als
 * solche benannt statt als Formateigenschaft.
 *
 * **Und genau daran hing der erste Befund.** 128 x 512 ist 65 536, eine
 * Seite hat 65 500. Der Rest fuer den letzten Sektor ist
 * 65 500 - 127 x 512 = **476 Byte**; die alte Fassung meldete ihn
 * trotzdem mit 512 Byte, die letzten **36 davon mit Null gefuellt** — und
 * `uft_format_add_sector()` setzt jeden Sektor unbedingt auf
 * `UFT_SECTOR_OK` mit guten CRC-Flags (MF-980). Gemessen am Vorzustand:
 *
 *     V127 : 512 Byte gemeldet, Byte 476..511 alle null, Status 0 = OK
 *
 * Das verletzt „Keine erfundenen Daten", in der Gestalt von MF-1022
 * (`sap`s Fuellsektor galt als guter Sektor). Jetzt traegt Sektor 127
 * **476 Byte** — genau das, was in der Datei steht. Die Summe geht auf:
 * 127 x 512 + 476 = 65 500.
 *
 * ── Was `open` vorher annahm ──────────────────────────────
 *
 * Drei weitere Befunde, alle gemessen:
 *
 *   **(a) Der Kopf durfte luegen.** Eine Datei mit „8 Seiten" im Kopf und
 *   **einer** Seite Daten wurde geoeffnet (`open` = 0), als 8 Zylinder mit
 *   1024 Sektoren angesagt — und Seite 7 war dann nicht lesbar. Dieselbe
 *   Gestalt wie MF-1019 bei `dim`: die Sonde prueft die Groesse, das
 *   Oeffnen nicht.
 *
 *   **(b) Jede Datei, deren Groesse ein Vielfaches von 65 500 ist, ging
 *   auf.** `open` prueft **keine** Kennung — gemessen wurden 131 000
 *   Nullbytes als 2 Seiten mit 256 Sektoren geoeffnet und als gute Daten
 *   geliefert. Die Sonde war dabei ehrlich (Konfidenz 30, Band „nur die
 *   Groesse" nach MF-729); das Oeffnen hat ihre Zurueckhaltung
 *   aufgehoben. Weil eine FDS-Seite nach der Beschreibung **immer** mit
 *   Block 1 beginnt, ist die Kennung bei Versatz 1 das Merkmal — und der
 *   reine Groessenzweig der Sonde ist entfallen.
 *
 *   **(c) Die Polsterung wurde nicht geprueft.** Eine Datei mit 'A' in
 *   Byte 9 bekam Konfidenz **95**, genau wie die gueltige. Die
 *   Beschreibung sagt „Bytes 5-15: Zero filled padding"; jetzt gehoert
 *   das zum Merkmal, und ohne es gibt es keine 95.
 *
 * ── Was bewusst NICHT entschieden wird ─────────────────────
 *
 * `FDS_MAX_SIDES` ist **8**, und das ist eine Hausregel, keine Zahl aus
 * der Quelle: die Beschreibung nennt kein Maximum („No commercial FDS
 * game had an odd number of sides greater than 1" ist eine Aussage ueber
 * Spiele, nicht ueber das Format), und MAME kennt ueber die Groesse nur 1,
 * 2 und 4. Die Schranke ist harmlos, weil die Datei die angesagten Seiten
 * seit diesem Stand auch **tragen** muss.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FDS_FWNES_HEADER    16
#define FDS_SIDE_SIZE       65500
#define FDS_MAGIC           "FDS\x1A"
#define FDS_NINTENDO_SIG    "*NINTENDO-HVC*"
/* UFTs eigene Abbildung (siehe Dateikopf): 128 virtuelle Sektoren je
 * Seite, 512 Byte — aber 128 x 512 = 65 536 und eine Seite hat 65 500.
 * Der letzte Sektor traegt deshalb nur FDS_LETZTER Byte. Die alte Fassung
 * meldete auch dort 512 und fuellte mit Nullen. */
#define FDS_VIRTUAL_SPT     128
#define FDS_VIRTUAL_SS      512
#define FDS_LETZTER         (FDS_SIDE_SIZE - (FDS_VIRTUAL_SPT - 1) * \
                             FDS_VIRTUAL_SS)     /* = 476 */
#define FDS_PAD_ANFANG      5       /* Bytes 5..15: „Zero filled padding" */
#define FDS_MAX_SIDES       8       /* Hausregel, keine Zahl der Quelle */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    bool        has_header;     /* fwNES header present */
    uint8_t     side_count;
    size_t      data_offset;    /* offset to first side data */
} fds_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

/**
 * @brief Erkennt eine FDS — und sagt nicht mehr, als sie gelesen hat.
 *
 * MF-1038: drei Aenderungen, jede gemessen (siehe Dateikopf).
 *   • die Polsterung gehoert zum Merkmal (Spec: „Bytes 5-15: Zero
 *     filled padding"); ohne sie gibt es keine 95;
 *   • ein Kopf, dessen Seitenzahl die Datei nicht deckt, wird
 *     ABGEWIESEN statt mit 70 angenommen — vorher sagte die Sonde ja
 *     und `open` antwortete -25 (Gestalt von MF-961/MF-1022/MF-1036);
 *   • der reine Groessenzweig ist entfallen. Eine FDS-Seite beginnt
 *     nach der Beschreibung IMMER mit Block 1, also ist die Kennung bei
 *     Versatz 1 das Merkmal; ohne sie war jedes Vielfache von 65 500
 *     eine FDS, und `open` nahm es an.
 */
bool fds_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    if (!data || size < 56) return false;

    /* (1) Mit fwNES-Kopf: vier Byte Kennung, Polsterung null, und die
     *     Datei muss die angesagten Seiten wirklich tragen. */
    if (size >= FDS_FWNES_HEADER && memcmp(data, FDS_MAGIC, 4) == 0) {
        size_t i;
        size_t erwartet;
        if (data[4] < 1 || data[4] > FDS_MAX_SIDES) return false;
        for (i = FDS_PAD_ANFANG; i < FDS_FWNES_HEADER; i++)
            if (data[i] != 0) return false;
        erwartet = FDS_FWNES_HEADER + (size_t)data[4] * FDS_SIDE_SIZE;
        if (file_size < erwartet) return false;
        *confidence = 95;
        return true;
    }

    /* (2) Ohne Kopf: Block 1 traegt die Kennung bei Versatz 1, und die
     *     Groesse muss ein ganzes Vielfaches einer Seite sein. */
    if (memcmp(data + 1, FDS_NINTENDO_SIG, 14) == 0
        && file_size >= FDS_SIDE_SIZE
        && file_size % FDS_SIDE_SIZE == 0
        && file_size / FDS_SIDE_SIZE <= FDS_MAX_SIDES) {
        *confidence = 90;
        return true;
    }

    return false;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t fds_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) {
        /* Fallback to read-only if r+b fails */
        f = fopen(path, "rb");
        read_only = true;
    }
    if (!f) return UFT_ERROR_FILE_OPEN;
    disk->read_only = read_only;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    size_t file_size = (size_t)fs;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Check for fwNES header */
    uint8_t hdr[FDS_FWNES_HEADER];
    if (fread(hdr, 1, FDS_FWNES_HEADER, f) != FDS_FWNES_HEADER) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    fds_data_t *pdata = calloc(1, sizeof(fds_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;

    /* MF-1038, Befund (a)+(b)+(c): `open` prueft jetzt dasselbe wie die
     * Sonde. Vorher nahm es einen Kopf beim Wort, ohne dass die Datei die
     * Seiten trug (gemessen: „8 Seiten", eine Seite Daten, open = 0,
     * Seite 7 nicht lesbar), und im kopflosen Fall pruefte es KEINE
     * Kennung — 131 000 Nullbytes gingen als zwei Seiten auf. */
    if (memcmp(hdr, FDS_MAGIC, 4) == 0) {
        size_t i;
        size_t erwartet;
        for (i = FDS_PAD_ANFANG; i < FDS_FWNES_HEADER; i++) {
            if (hdr[i] != 0) {
                free(pdata); fclose(f);
                return UFT_ERROR_FORMAT_INVALID;
            }
        }
        pdata->has_header = true;
        pdata->side_count = hdr[4];
        pdata->data_offset = FDS_FWNES_HEADER;
        if (pdata->side_count == 0
            || pdata->side_count > FDS_MAX_SIDES) {
            free(pdata); fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
        }
        erwartet = FDS_FWNES_HEADER
                 + (size_t)pdata->side_count * FDS_SIDE_SIZE;
        if (file_size < erwartet) {
            free(pdata); fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
        }
    } else {
        /* Kopflos: Block 1 muss da sein, und die Groesse muss eine ganze
         * Zahl von Seiten sein. `hdr` traegt nur 16 Byte, die Kennung ist
         * 14 Byte ab Versatz 1 — also wird von vorn gelesen. */
        uint8_t b1[56];
        if (fseek(f, 0, SEEK_SET) != 0
            || fread(b1, 1, sizeof(b1), f) != sizeof(b1)
            || memcmp(b1 + 1, FDS_NINTENDO_SIG, 14) != 0) {
            free(pdata); fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
        }
        if (file_size < FDS_SIDE_SIZE
            || file_size % FDS_SIDE_SIZE != 0) {
            free(pdata); fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
        }
        pdata->has_header = false;
        pdata->data_offset = 0;
        pdata->side_count = (uint8_t)(file_size / FDS_SIDE_SIZE);
        if (pdata->side_count == 0
            || pdata->side_count > FDS_MAX_SIDES) {
            free(pdata); fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
        }
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->side_count; /* 1 cyl per side */
    disk->geometry.heads = 1;
    disk->geometry.sectors = FDS_VIRTUAL_SPT;
    disk->geometry.sector_size = FDS_VIRTUAL_SS;
    disk->geometry.total_sectors =
        (uint32_t)pdata->side_count * FDS_VIRTUAL_SPT;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void fds_close(uft_disk_t *disk)
{
    fds_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — each "cylinder" is one disk side
 * ============================================================================ */

static uft_error_t fds_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    fds_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl >= pdata->side_count)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset = (long)(pdata->data_offset + (size_t)cyl * FDS_SIDE_SIZE);
    if (fseek(pdata->file, offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* MF-1038: der letzte virtuelle Sektor traegt FDS_LETZTER = 476 Byte,
     * nicht 512. Vorher wurden 512 gemeldet und die letzten 36 mit Null
     * gefuellt — `uft_format_add_sector()` setzt jeden Sektor unbedingt
     * auf UFT_SECTOR_OK (MF-980), also waren 36 erfundene Byte je Seite
     * von echten Daten nicht zu unterscheiden. 127 x 512 + 476 = 65 500. */
    uint8_t buf[FDS_VIRTUAL_SS];
    size_t remaining = FDS_SIDE_SIZE;

    for (int s = 0; s < FDS_VIRTUAL_SPT && remaining > 0; s++) {
        size_t chunk = (remaining < FDS_VIRTUAL_SS) ? remaining
                                                   : FDS_VIRTUAL_SS;
        memset(buf, 0, FDS_VIRTUAL_SS);

        if (fread(buf, 1, chunk, pdata->file) != chunk)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              chunk, (uint8_t)cyl, 0);
        remaining -= chunk;
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track — write virtual sectors back to the FDS file
 * ============================================================================ */

static uft_error_t fds_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    fds_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (head != 0 || cyl >= pdata->side_count)
        return UFT_ERROR_INVALID_STATE;

    long base = (long)(pdata->data_offset + (size_t)cyl * FDS_SIDE_SIZE);
    size_t remaining = FDS_SIDE_SIZE;

    for (size_t s = 0; s < track->sector_count &&
         (int)s < FDS_VIRTUAL_SPT && remaining > 0; s++) {
        size_t chunk = (remaining < FDS_VIRTUAL_SS) ? remaining : FDS_VIRTUAL_SS;
        long off = base + (long)(s * FDS_VIRTUAL_SS);
        if (fseek(pdata->file, off, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[FDS_VIRTUAL_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0, FDS_VIRTUAL_SS); data = pad;
        }
        if (fwrite(data, 1, chunk, pdata->file) != chunk)
            return UFT_ERROR_IO;
        remaining -= chunk;
    }
    fflush(pdata->file);
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_fds_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_fds = {
    .name         = "FDS",
    .description  = "Famicom Disk System",
    .extensions   = "fds",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = fds_probe,
    .open         = fds_open,
    .close        = fds_close,
    .read_track   = fds_read_track,
    .write_track  = fds_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_fds_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_fds_features) / sizeof(uft_format_plugin_fds_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(fds)
