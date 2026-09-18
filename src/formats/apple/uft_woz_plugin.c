/**
 * @file uft_woz_plugin.c
 * @brief WOZ (Apple II) Plugin-B wrapper
 *
 * Wraps the existing woz_load/woz_get_track_525/woz_free API
 * from uft_woz.c into the format plugin interface.
 */
#include "uft/uft_format_common.h"
#include "uft/formats/apple/uft_woz.h"
#include "uft/uft_format_probe.h"   /* MF-1231: uft_format_variant_t */
/* MF-1066: der oracle-gepruefte Apple-GCR-Abtaster. Vorhandener Code,
 * hier nur verdrahtet — siehe die Begruendung an woz_plugin_read_track(). */
#include "uft/formats/apple/uft_apple_gcr.h"

static bool woz_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < 8) return false;
    uint32_t sig = uft_read_le32(data);
    if (sig == WOZ_SIGNATURE_V1 || sig == WOZ_SIGNATURE_V2) {
        *confidence = 98;
        return true;
    }
    return false;
}

static uft_error_t woz_plugin_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    woz_image_t *img = NULL;
    int rc = woz_load(path, &img);
    if (rc != 0 || !img) return UFT_ERROR_FORMAT_INVALID;

    disk->plugin_data = img;
    disk->geometry.cylinders = img->is_525 ? 35 : 80;
    disk->geometry.heads = img->is_525 ? 1 : (img->info.disk_sides ? img->info.disk_sides : 1);
    disk->geometry.sectors = img->is_525 ? 16 : 12;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders *
                                   disk->geometry.heads * disk->geometry.sectors;
    return UFT_OK;
}

static void woz_plugin_close(uft_disk_t *disk) {
    woz_image_t *img = (woz_image_t *)disk->plugin_data;
    if (img) { woz_free(img); disk->plugin_data = NULL; }
}

static uft_error_t woz_plugin_read_track(uft_disk_t *disk, int cyl, int head,
                                          uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    woz_image_t *img = (woz_image_t *)disk->plugin_data;
    if (!img) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    const uint8_t *bits = NULL;
    uint32_t bit_count = 0;
    int rc;

    if (img->is_525) {
        rc = woz_get_track_525(img, cyl * 4, &bits, &bit_count);  /* quarter-track */
    } else {
        rc = woz_get_track_35(img, cyl, head, &bits, &bit_count);
    }

    if (rc != 0 || !bits || bit_count == 0) return UFT_OK;

    /* MF-1066: hier wurde der ROHE BITSTROM als „Sektor 0" abgelegt —
     * ein Sektor je Spur, waehrend `woz_plugin_open()` weiter oben
     * `disk->geometry.sectors = 16` meldet.
     *
     * Gemessen an einem `to_woz2`-Erzeugnis (35 Spuren, 6-and-2):
     * **35 Sektoren statt 560**, und `track->raw_data` blieb dabei leer,
     * es kam also auch kein Bitstrom an der dafuer vorgesehenen Stelle
     * an. Das ist die Gestalt von MF-796, wo `edsk` „9 Sektoren" meldete
     * und fuer jede Spur keinen einzigen lieferte — still, mit `UFT_OK`.
     *
     * **Der Dekoder dafuer liegt seit MF-948 im Baum und ist
     * oracle-geprueft:** `uft_apple_gcr_scan_track()` beherrscht beide
     * Kodierungen (6-and-2 und 5-and-3, am Adressvorspann erkannt, nicht
     * vom Aufrufer mitgegeben), laeuft GENAU eine Umdrehung (MF-715 hat
     * dort die Doppelsektor-Falle gemessen: 595 statt 560) und wird von
     * `uft_nib.c` in Produktion gerufen. Ihn hier zu rufen ist das
     * Verdrahten vorhandenen, unerreichbaren Codes — von der
     * EINFRIER-REGEL ausdruecklich erlaubt, und es kommt keine Zeile
     * neuer Dekodierlogik hinzu.
     *
     * Das Aufrufmuster ist woertlich das aus `uft_nib.c:117-137`. */
    if (img->is_525) {
        uft_a2_sector_t sek[UFT_A2_SECTORS_16 * 2];
        int n = uft_apple_gcr_scan_track(bits, bit_count, sek,
                                         sizeof(sek) / sizeof(sek[0]));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                if (sek[i].track != (uint8_t)cyl) continue;
                /* Ein Feld in einer Kodierung, die die Einheit nicht
                 * beherrscht (DOS-3.2-Bootsektor, MF-721), traegt KEINE
                 * Bytes. Es wird uebergangen statt geraten. */
                if (!sek[i].has_data || sek[i].alt_encoding) continue;
                /* `sector` stammt aus dem Adressfeld — es IST die Nummer
                 * auf der Diskette und darf nicht verschoben werden
                 * (ARCH-20). */
                uft_format_add_sector_with_id(track, sek[i].sector,
                                              sek[i].data,
                                              UFT_A2_SECTOR_SIZE,
                                              (uint8_t)cyl, (uint8_t)head);
                if (!sek[i].data_checksum_ok && track->sector_count > 0)
                    uft_sector_set_crc(
                        &track->sectors[track->sector_count - 1], false);
            }
            return UFT_OK;
        }
        /* Kein Adressfeld gefunden — eine ungeformte oder
         * kopiergeschuetzte Spur. Der Bitstrom bleibt erhalten, siehe
         * unten; verschwiegen wird er nicht. */
    }

    /* 3,5 Zoll und Spuren ohne lesbare Adressfelder: der rohe Bitstrom
     * als ein Pseudosektor 0.
     *
     * **Das ist fuer 3,5 Zoll eine offene Stelle, keine Loesung.**
     * Apple-3,5"-Disketten sind zonenaufgezeichnet (8 bis 12 Sektoren je
     * Spur), und `uft_apple_gcr_scan_track()` ist der 5,25"-Abtaster —
     * er kennt die GCR-Variante der 3,5"-Laufwerke nicht. Die Geometrie
     * meldet dort weiterhin fest 12, geliefert wird ein Pseudosektor;
     * verzeichnet als P3-352. */
    {
        uint32_t byte_count = (bit_count + 7) / 8;
        uint16_t chunk = (byte_count > 65535) ? 65535 : (uint16_t)byte_count;
        /* The comment above says sector 0, so it must BE 0 (ARCH-20) */
        uft_format_add_sector_with_id(track, 0, bits, chunk, (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

/* Prinzip 7 Feature-Matrix */
static const uft_plugin_feature_t woz_features[] = {
    { "WOZ1 container",            UFT_FEATURE_SUPPORTED,   NULL },
    { "WOZ2 container",            UFT_FEATURE_SUPPORTED,   NULL },
    { "WOZ2.1 container",          UFT_FEATURE_SUPPORTED,   NULL },
    { "Quarter-track positions",   UFT_FEATURE_SUPPORTED,   NULL },
    { "Weak bits (FLUX chunk)",    UFT_FEATURE_PARTIAL,
      "FLUX chunk parsed (has_flux flag set, flux_tracks[] populated); "
      "per-track flux transitions not yet surfaced via read_track — "
      "woz_get_flux_track() exists but is currently unbridged" },
    { "INFO + META metadata",      UFT_FEATURE_PARTIAL,
      "read only; write not implemented" },
    { "Write / encode",            UFT_FEATURE_UNSUPPORTED, NULL },
};

/* ── Die zwei Kennungen, die dieses Plugin unterscheidet (MF-1231) ───
 *
 * Neu ist hier keine Zahl: `WOZ_SIGNATURE_V1` ('WOZ1') und
 * `WOZ_SIGNATURE_V2` ('WOZ2') stehen in
 * `include/uft/formats/apple/uft_woz.h:47-48`, und `woz_plugin_probe`
 * entscheidet an ihnen. Die Oberflaeche nannte dieselben zwei als
 * "WOZ 1.0" und "WOZ 2.0".
 *
 * **v2.1 bekommt bewusst KEINEN eigenen Eintrag.** Der Name des
 * Plugins sagt "v1/v2/v2.1", und die Merkmalstafel fuehrt alle drei —
 * aber v2.1 traegt dieselbe Kennung 'WOZ2' wie v2, sie unterscheiden
 * sich erst im INFO-Chunk. Ein dritter Eintrag mit demselben
 * `validate` waere nicht unterscheidbar; das faellt im Test als V3.
 * Solange `woz_load()` die Unterfassung nicht herausgibt, ist v2.1
 * hier nicht benennbar.
 *
 * Und die Richtung ist auch hier der Gewinn: WOZ fuehrt **kein**
 * `UFT_FORMAT_CAP_WRITE` und hat kein `write_track`. */
static int woz_variante_ist_v1(const uint8_t *d, size_t n)
{
    return (d && n >= 4 && uft_read_le32(d) == WOZ_SIGNATURE_V1) ? 1 : 0;
}

static int woz_variante_ist_v2(const uint8_t *d, size_t n)
{
    return (d && n >= 4 && uft_read_le32(d) == WOZ_SIGNATURE_V2) ? 1 : 0;
}

static const char WOZ_KEIN_SCHREIBER[] =
    "UFT liest WOZ, schreibt es aber nicht: das Plugin hat kein "
    "write_track und beansprucht kein UFT_FORMAT_CAP_WRITE.";

static const uft_format_variant_t woz_variants[] = {
    { .name = "WOZ 1.0", .description = "Applesauce WOZ v1, Kennung 'WOZ1'",
      .base_format = UFT_FORMAT_WOZ,
      .validate = woz_variante_ist_v1,
      .can_read = true, .can_write = false,
      .write_note = WOZ_KEIN_SCHREIBER,
      .is_write_default = false },
    { .name = "WOZ 2.0", .description =
          "Applesauce WOZ v2 (auch v2.1), Kennung 'WOZ2'",
      .base_format = UFT_FORMAT_WOZ,
      .validate = woz_variante_ist_v2,
      .can_read = true, .can_write = false,
      .write_note = WOZ_KEIN_SCHREIBER,
      .is_write_default = false },
};

const uft_format_plugin_t uft_format_plugin_woz = {
    .name = "WOZ", .description = "Apple II WOZ (v1/v2/v2.1)",
    .extensions = "woz", .format = UFT_FORMAT_WOZ,
    .variants = woz_variants,
    .variant_count = sizeof(woz_variants) / sizeof(woz_variants[0]),
    /* Capabilities reflect what read_track() actually surfaces — flux/weak
     * are parsed in woz.c but not yet returned via the plugin API, so we do
     * NOT advertise CAP_FLUX / CAP_WEAK_BITS here. Re-enable after the
     * woz_get_flux_track() bridge lands. */
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = woz_plugin_probe, .open = woz_plugin_open,
    .close = woz_plugin_close, .read_track = woz_plugin_read_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* Applesauce project publishes full WOZ 1/2/2.1 specs */
    .features = woz_features,
    .feature_count = sizeof(woz_features) / sizeof(woz_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(woz)
