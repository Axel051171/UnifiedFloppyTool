/* SPDX-License-Identifier: GPL-2.0-or-later */
/** @file uft_disk2_bridge.c — Umsetzung von uft_disk2_bridge.h (MF-1272). */

#include "uft/core/uft_disk2_bridge.h"

#include <stdlib.h>
#include <string.h>

/* Eine Spur des alten Modells VOLLSTAENDIG freigeben.
 *
 * Der Baum hat zwei Aufraeumer fuer `uft_track_t`, und keiner deckt den
 * vollen Satz (gemessen MF-599, im Kopf von `uft_track_free()` benannt):
 * `uft_track_cleanup()` gibt Sektoren, `flux` und `raw_data` frei,
 * `uft_track_free()` zusaetzlich `confidence`, `weak_mask`, `flux_times`,
 * `revisions` — `raw_data` dort aber nur mit `owns_data`. Eine Spur aus
 * `read_track()` setzt `owns_data` nicht zuverlaessig. Deshalb: erst die
 * Felder, die nur `uft_track_free()` kennt, dann `uft_track_cleanup()`. */
static void spur_freigeben(uft_track_t *t) {
    free(t->confidence);   t->confidence = NULL;
    free(t->weak_mask);    t->weak_mask = NULL;
    free(t->flux_times);   t->flux_times = NULL;
    if (t->revisions) {
        for (size_t i = 0; i < t->revision_count; i++) free(t->revisions[i].data);
        free(t->revisions);
        t->revisions = NULL;
        t->revision_count = 0;
    }
    uft_track_cleanup(t);
}

static uft_d2_conf_t deckel(uft_d2_conf_t c, float plugin_conf) {
    /* Ein Plugin-Wert 0.0 ist die Vorgabe des Nullens, keine Messung. */
    if (plugin_conf <= 0.0f) return c;
    float f = plugin_conf;
    if (f > 1.0f) f = 1.0f;
    const uft_d2_conf_t p = (uft_d2_conf_t)(f * 255.0f + 0.5f);
    return p < c ? p : c;
}

static void sektor_uebersetzen(uft_d2_sector_t *out, const uft_sector_t *in,
                               uft_d2_deriv_id_t dv) {
    memset(out, 0, sizeof(*out));
    out->id_cyl       = in->id.cylinder;
    out->id_head      = in->id.head;
    out->id_sec       = in->id.sector;
    out->id_size_code = in->id.size_code;

    /* Bekannt ist nur, was das alte Modell AUSSPRICHT: eine Fehlerflagge
     * oder ein von 0 verschiedener Pruefwert. `UFT_SECTOR_OK` (= 0) setzt
     * `uft_format_add_sector()` unbedingt und ist deshalb kein Beleg. */
    const bool id_err = (in->status & UFT_SECTOR_ID_CRC_ERROR) != 0;
    out->id_crc_known = id_err || in->id.crc != 0;
    out->id_crc_ok    = out->id_crc_known && !id_err && in->id.crc_ok;

    const bool data_err = (in->status & UFT_SECTOR_CRC_ERROR) != 0;
    out->data_crc_known = data_err
                       || in->crc_stored != 0 || in->crc_calculated != 0;
    out->data_crc_ok = out->data_crc_known && !data_err
                    && in->crc_stored == in->crc_calculated;

    const bool missing = (in->status & UFT_SECTOR_MISSING) != 0;
    const uint32_t len = in->data_len ? (uint32_t)in->data_len
                                      : (uint32_t)in->data_size;
    out->data     = in->data;
    out->data_len = (in->data && len) ? len : 0u;
    out->has_data = !missing && out->data_len > 0u;

    out->dam = in->data_mark ? in->data_mark
             : ((in->deleted || (in->status & UFT_SECTOR_DELETED)) ? 0xF8u : 0u);

    /* Lage: drei Versatzfelder ohne Aussage, welches gilt — nicht geraten. */
    out->idam_bit = out->dam_bit = out->data_end_bit = SIZE_MAX;

    out->origin = missing ? UFT_D2_ORIGIN_PADDING : UFT_D2_ORIGIN_CONTAINER;

    /* Flackern: die per-Byte-Maske zaehlt markierte Bytes — eine
     * UNTERGRENZE der Bits. Die blosse Flagge ohne Maske sagt „mindestens
     * eines"; eingetragen wird genau das. */
    const bool weak = in->weak || (in->status & UFT_SECTOR_WEAK) != 0;
    if (in->weak_mask && in->data_size) {
        uint32_t n = 0u;
        for (size_t i = 0; i < in->data_size; i++) if (in->weak_mask[i]) n++;
        out->weak_bits = n ? n : (weak ? 1u : 0u);
    } else if (weak) {
        out->weak_bits = 1u;
    }

    /* Zuversicht — nach der Regel im Kopf von uft_disk2_bridge.h. */
    uft_d2_conf_t c;
    if (missing)                                        c = UFT_D2_CONF_NONE;
    else if (out->data_crc_known && !out->data_crc_ok)  c = UFT_D2_BRIDGE_CONF_BAD_CRC;
    else if (out->weak_bits)                            c = UFT_D2_CONF_UNVERIFIED;
    else if (out->data_crc_known && out->data_crc_ok)   c = UFT_D2_CONF_CERTAIN;
    else                                                c = UFT_D2_CONF_UNVERIFIED;
    out->conf = deckel(c, in->confidence);

    /* MF-1274: die Herkunft ist jetzt eine Kennung ins Register des
     * Modells, nicht ein Struct je Sektor. 1440 Sektoren tragen damit
     * EINEN Eintrag statt 1440 Kopien derselben zwei Zeichenketten — und
     * der Eintrag ist serialisierbar, ein Zeiger waere es nicht. */
    out->deriv = dv;
}

bool uft_d2_from_disk(uft_disk2_t *d, uft_disk_t *disk,
                      const uft_format_plugin_t *plugin,
                      uft_d2_bridge_stats_t *stats) {
    uft_d2_bridge_stats_t st;
    memset(&st, 0, sizeof(st));
    if (stats) *stats = st;
    if (!d || !disk) return false;

    if (!plugin) plugin = uft_disk_plugin(disk);
    if (!plugin || !plugin->read_track) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, -1, -1, -1,
                    "NO_READER", "Kein Plugin mit read_track — nichts "
                    "eingespeist.");
        return false;
    }
    const char *name = plugin->name ? plugin->name : "(unbenannt)";
    const uft_geometry_t *g = &disk->geometry;
    if (g->cylinders == 0 || g->heads == 0) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, -1, -1, -1,
                    "NO_GEOMETRY", "Plugin \"%s\" nennt %u Zylinder und %u "
                    "Koepfe — keine Spur zu lesen.", name,
                    (unsigned)g->cylinders, (unsigned)g->heads);
        return false;
    }

    uft_d2_add_meta(d, "Plugin", name, UFT_D2_META_SELF);

    /* Zwei Ableitungen, EINMAL registriert: eine je Schicht, die die
     * Bruecke fuellt. `source_gen` ist 0, weil die Bruecke aus einem
     * BEHAELTER speist und nicht aus einer tieferen Schicht dieses
     * Modells — es gibt keine Quelle, deren Generation veralten koennte. */
    const uft_d2_deriv_id_t dv_sect =
        uft_d2_register_deriv(d, UFT_D2_LAYER_SECTORS, UFT_D2_ORIGIN_CONTAINER,
                              "uft_d2_bridge", name, 0u);
    const uft_d2_deriv_id_t dv_bits =
        uft_d2_register_deriv(d, UFT_D2_LAYER_BITSTREAM,
                              UFT_D2_ORIGIN_CONTAINER, "uft_d2_bridge",
                              name, 0u);

    for (unsigned c = 0; c < g->cylinders; c++) {
        for (unsigned h = 0; h < g->heads; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            st.tracks_asked++;
            const uft_error_t rc = plugin->read_track(disk, (int)c, (int)h, &t);
            if (rc != UFT_OK) {
                st.tracks_failed++;
                spur_freigeben(&t);
                continue;
            }
            const bool hat_bits = t.raw_data && t.raw_bits > 0u;
            if (t.raw_data && t.raw_bits == 0u) st.raw_without_bits++;
            if (t.flux && t.flux_count)        st.tracks_with_flux++;
            if (t.weak_mask)                   st.tracks_with_weak_mask++;

            if (t.sector_count == 0u && !hat_bits) { spur_freigeben(&t); continue; }

            uft_d2_track_t *dt = uft_d2_track(d, (uint16_t)c, (uint8_t)h);
            if (!dt) { spur_freigeben(&t); return false; }
            if (t.encoding != UFT_ENC_UNKNOWN && dt->encoding == UFT_ENC_UNKNOWN)
                dt->encoding = t.encoding;

            if (hat_bits) {
                const uint32_t cell = t.bitrate ? (uint32_t)(1000000000u / t.bitrate) : 0u;
                /* Stimmen je Bit gibt es hier nicht: das alte Modell fuehrt
                 * keine Fusion je Bit, und eine erfundene Zahl waere genau
                 * das, was `agree` verhindern soll. Also NULL und 0. */
                if (uft_d2_set_bitstream(d, dt, t.raw_data, t.raw_bits,
                                         t.confidence, NULL, 0u, NULL, NULL,
                                         SIZE_MAX, t.encoding, cell, dv_bits))
                    st.bitstreams++;
            }

            if (t.sector_count) st.tracks_with_sectors++;
            for (size_t i = 0; i < t.sector_count; i++) {
                uft_d2_sector_t s;
                sektor_uebersetzen(&s, &t.sectors[i], dv_sect);
                if (uft_d2_add_sector(d, dt, &s)) st.sectors++;
                else st.sectors_rejected++;
            }
            spur_freigeben(&t);
        }
    }

    /* Was die Bruecke NICHT traegt, sagt sie — einmal je Klasse, nicht je
     * Spur, damit die Befundliste nicht ueberlaeuft. */
    if (st.raw_without_bits)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_BITSTREAM, -1, -1, -1,
                    "RAW_BITS_UNKNOWN", "%zu Spuren tragen Rohdaten ohne "
                    "Bitlaenge — Bitstrom nicht uebernommen, nicht aus Bytes "
                    "erfunden.", st.raw_without_bits);
    if (st.tracks_with_flux)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_FLUX, -1, -1, -1,
                    "FLUX_NOT_BRIDGED", "%zu Spuren tragen Fluss — die "
                    "Bruecke uebernimmt ihn nicht; Umdrehungen sind im alten "
                    "Modell nicht getrennt.", st.tracks_with_flux);
    if (st.tracks_with_weak_mask)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_BITSTREAM, -1, -1, -1,
                    "WEAK_MASK_NOT_BRIDGED", "%zu Spuren tragen eine "
                    "per-Bit-Weak-Maske — das Zentrum kennt sie nur als "
                    "Konfidenz je Bit; nicht uebersetzt.",
                    st.tracks_with_weak_mask);
    if (st.tracks_failed)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS, -1, -1, -1,
                    "TRACKS_FAILED", "%zu von %zu Spuren nicht lesbar.",
                    st.tracks_failed, st.tracks_asked);

    if (stats) *stats = st;
    return st.sectors > 0u || st.bitstreams > 0u;
}
