/**
 * @file uft_stx_plugin.c
 * @brief STX (Pasti) Plugin-B — Atari ST protected format
 *
 * STX preserves sector timing and fuzzy bit masks for copy protection.
 * Magic: "RSY\0" (4 bytes). 16-byte file header + track descriptors.
 *
 * Reference: Pasti specification (Jean Louis-Guerin)
 */
#include "uft/uft_format_common.h"
#include "uft/formats/stx/uft_stx_air.h"
#include "uft/uft_log.h"

/*
 * MF-854: dieser Leser parst STX nicht mehr selbst.
 *
 * Der Baum trug VIER STX-Leser. Registriert war dieser — 179 Zeilen —,
 * waehrend `src/formats/stx/uft_stx_air.c` (~950 Zeilen, erklaerter Port
 * von AIR `pasti/PastiRead.cs`) vollstaendig las und KEINEN Aufrufer
 * hatte. MF-847 hat hier fuenf Feldfehler behoben; P3-93 hat die
 * eigentliche Frage dem Eigentuemer vorgelegt, weil sie zwei Folgen
 * hat, die nicht meine sind:
 *
 *   (a) sie zieht eine GPL-3.0-only-Uebersetzungseinheit in den
 *       REGISTRIERTEN Pfad (MF-698 hatte die Bindung fuer das
 *       Gesamtwerk bereits angenommen, die Reichweite waechst);
 *   (b) sie macht `STX_MAX_SECTORS` scharf.
 *
 * Der Eigentuemer hat entschieden: verdrahten. Dieselbe Bauform wie
 * MF-837 (DMS: 668 Zeilen eigener Entpacker wichen der Bibliothek).
 *
 * Was dieser Leser dadurch dazugewinnt: Timing-Saetze, Spurbilder mit
 * Sync-Offset, CRC-Pruefung, Macrodos/Speedlock-Nachbildung fuer
 * Revision 0 — und vor allem EINE Stelle, an der STX gelesen wird.
 */

#define STX_MAGIC_0 'R'
#define STX_MAGIC_1 'S'
#define STX_MAGIC_2 'Y'
#define STX_MAGIC_3 '\0'
#define STX_HEADER_SIZE 16

typedef struct {
    stx_air_handle_t *air;
} stx_pd_t;

static bool stx_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < STX_HEADER_SIZE) return false;
    if (data[0] == STX_MAGIC_0 && data[1] == STX_MAGIC_1 &&
        data[2] == STX_MAGIC_2 && data[3] == STX_MAGIC_3) {
        *confidence = 97;
        return true;
    }
    return false;
}

static uft_error_t stx_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    size_t raw_size = 0;
    uint8_t *raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;

    stx_air_handle_t *air = uft_stx_air_open(raw, raw_size);
    free(raw);                       /* der Port haelt eigene Kopien */
    if (!air) return UFT_ERROR_FORMAT_INVALID;

    stx_pd_t *p = calloc(1, sizeof(stx_pd_t));
    if (!p) { uft_stx_air_close(air); return UFT_ERROR_NO_MEMORY; }
    p->air = air;

    disk->plugin_data = p;
    disk->geometry.cylinders = uft_stx_air_cylinders(air);
    disk->geometry.heads = 2;
    disk->geometry.sectors = 9;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors =
        (uint32_t)disk->geometry.cylinders * 2 * 9;
    return UFT_OK;
}

static void stx_close(uft_disk_t *disk) {
    stx_pd_t *p = disk->plugin_data;
    if (!p) return;
    uft_stx_air_close(p->air);
    free(p);
    disk->plugin_data = NULL;
}

static uft_error_t stx_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. Gefunden an opus_read_track() von
     * tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    stx_pd_t *p = disk->plugin_data;
    if (!p || !p->air) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);

    if (!uft_stx_air_track_present(p->air, cyl, head)) return UFT_OK;

    /* MF-854 / P3-58: der Verlust wird BENANNT, nicht verschwiegen.
     *
     * Der Port fasst hoechstens 32 Sektordeskriptoren je Spur. Das ist
     * eine Grenze DIESER Umsetzung, nicht des Formats: `sectorCount` ist
     * ein uint16, und belegte Extremfaelle liegen weit darueber —
     * „Sherman M4" fuehrt 70 Sektoren je Spur (DrCoolZic, Atari Copy
     * Protection Rev 1.4, Klasse NOS).
     *
     * Solange die Klemme ruhte (der Port hatte keinen Aufrufer), war das
     * folgenlos. Mit der Verdrahtung ist sie scharf — und „Kein Bit
     * verloren" heisst nicht, dass nie etwas fehlt, sondern dass
     * Fehlendes benannt wird. */
    int angekuendigt = uft_stx_air_track_announced(p->air, cyl, head);
    int vorhanden    = uft_stx_air_track_stored(p->air, cyl, head);
    if (angekuendigt > vorhanden) {
        UFT_WARN("STX %d/%d: Datei meldet %d Sektoren, dieser Leser haelt "
                 "hoechstens %d — %d verworfen. Grenze der Umsetzung, "
                 "nicht des Formats.",
                 cyl, head, angekuendigt, vorhanden,
                 angekuendigt - vorhanden);
        track->errors++;
    }

    for (int i = 0; i < vorhanden; i++) {
        uft_stx_sector_view_t sv;
        if (!uft_stx_air_sector(p->air, cyl, head, i, &sv)) continue;
        if (!sv.data || sv.size == 0) continue;

        /* Die Sektornummer kommt aus R (Adressfeld-Byte 0x0A) und ist
         * 1-basiert; UFT zaehlt ab 0 (MF-335). */
        uft_format_add_sector(track,
                              sv.id_number > 0 ? (uint8_t)(sv.id_number - 1) : 0,
                              sv.data, (uint16_t)sv.size,
                              (uint8_t)cyl, (uint8_t)head);

        if (track->sector_count > 0) {
            uft_sector_t *dst = &track->sectors[track->sector_count - 1];
            if (sv.crc_error) uft_sector_set_crc(dst, false);
            if (sv.deleted)   dst->deleted = true;
        }
    }
    return UFT_OK;
}

/* NOTE: write_track omitted by design — STX stores fuzzy-bit streams and
 * per-sector timing that cannot be regenerated from sector data alone.
 * The Pasti format is purpose-built to preserve ST copy-protection, so
 * round-tripping through a sector-level write would destroy protection. */
/* Prinzip 7 Feature-Matrix */
static const uft_plugin_feature_t stx_features[] = {
    { "Standard MFM sectors",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Weak sectors (fuzzy)",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Custom sector timing",     UFT_FEATURE_SUPPORTED,   NULL },
    { "Long tracks",              UFT_FEATURE_PARTIAL,
      "detected and preserved; not regenerated on re-encode" },
    { "Write / encode",           UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_stx = {
    .name = "STX", .description = "Atari ST Pasti (Protected)",
    .extensions = "stx", .format = UFT_FORMAT_STX,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_TIMING | UFT_FORMAT_CAP_VERIFY,
    .probe = stx_plugin_probe, .open = stx_open,
    .close = stx_close, .read_track = stx_read_track,
    .verify_track = uft_weak_bit_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* Pasti never had a public spec */
    .features = stx_features,
    .feature_count = sizeof(stx_features) / sizeof(stx_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(stx)
