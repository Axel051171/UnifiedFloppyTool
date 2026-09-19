/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_disk2_io.c
 * @brief Waechter fuer UFTD — den Behaelter, der ALLES traegt (MF-1275).
 *
 * Drei Gruppen:
 *
 *   1. Rundlauf   Ein Modell mit ALLEM — Fluss (zwei Umdrehungen),
 *                 Bitstrom mit allen vier Nebenreihen, Sektoren mit
 *                 Daten und Bitlagen, ZWEI Dateisysteme, Metadaten,
 *                 Ableitungsregister, Befunde — wird gespeichert,
 *                 geladen, FELDWEISE verglichen und wieder gespeichert.
 *                 Die zweite Sicherung muss BYTEIDENTISCH sein.
 *   2. Fehler     Ein gekipptes Bit wird an SEINER Stelle gefunden, und
 *                 was davor stand, ist trotzdem geladen. Abgeschnitten,
 *                 fremde Datei, zu neue Fassung, unbekannter Block.
 *   3. Ehrlich    Eine GEFAELSCHTE Datei — ein Sektor mit Zuversicht 255
 *                 bei falscher CRC — wird beim LADEN abgewiesen. Laden
 *                 ist nie stiller als Speichern.
 */

#include "uft/core/uft_disk2.h"
#include "uft/core/uft_disk2_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)

static uft_d2_sector_t guter_sektor(uint8_t cyl, uint8_t sec, uint32_t len,
                                    uft_d2_deriv_id_t dv) {
    uft_d2_sector_t s;
    memset(&s, 0, sizeof(s));
    s.id_cyl = cyl; s.id_sec = sec; s.id_size_code = 2u;
    s.id_crc_ok = s.id_crc_known = true;
    s.data_len = len; s.dam = 0xFBu;
    s.data_crc_ok = s.data_crc_known = s.has_data = true;
    s.idam_bit = s.dam_bit = s.data_end_bit = SIZE_MAX;
    s.origin = UFT_D2_ORIGIN_CONTAINER; s.conf = UFT_D2_CONF_CERTAIN;
    s.deriv = dv;
    return s;
}

/** Eine Diskette mit ALLEM, was das Modell kann. */
static uft_disk2_t *volles_modell(void) {
    uft_disk2_t *d = uft_d2_create();
    const uft_d2_deriv_id_t dv_scp = uft_d2_register_deriv(d,
        UFT_D2_LAYER_FLUX, UFT_D2_ORIGIN_CONTAINER, "scp_reader",
        "v1.9 footer=yes", 0u);
    const uft_d2_deriv_id_t dv_pll = uft_d2_register_deriv(d,
        UFT_D2_LAYER_FLUX, UFT_D2_ORIGIN_DERIVED, "kalman_pll",
        "cell=2000ns", 1u);
    const uft_d2_deriv_id_t dv_scan = uft_d2_register_deriv(d,
        UFT_D2_LAYER_BITSTREAM, UFT_D2_ORIGIN_DERIVED, "ibm_mfm_scanner",
        "past_index=yes", 1u);

    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    const uint32_t iv1[] = { 4000u, 4000u, 6000u };
    const uint32_t iv2[] = { 4000u, 4100u, 6000u };
    uft_d2_add_revolution(d, t, iv1, 3u, 200000000u, true,
                          UFT_D2_CONF_CERTAIN, dv_scp);
    uft_d2_add_revolution(d, t, iv2, 3u, 200400000u, true,
                          UFT_D2_CONF_CERTAIN, dv_scp);

    uint8_t bits[16];
    for (int i = 0; i < 16; ++i) bits[i] = (uint8_t)(i * 37u);
    uft_d2_conf_t conf[128]; memset(conf, 255, sizeof(conf)); conf[10] = 120u;
    uint8_t agree[128];      memset(agree, 2, sizeof(agree)); agree[10] = 1u;
    int16_t ph[128];  for (int i = 0; i < 128; ++i) ph[i] = (int16_t)(i - 64);
    uint16_t fc[128]; for (int i = 0; i < 128; ++i) fc[i] = (uint16_t)(1u + (i & 1u));
    uft_d2_set_bitstream(d, t, bits, 128u, conf, agree, 2u, ph, fc, 64u,
                         UFT_ENC_MFM, 2000u, dv_pll);

    uint8_t data[512];
    for (int i = 0; i < 512; ++i) data[i] = (uint8_t)i;
    uft_d2_sector_t s1 = guter_sektor(0u, 1u, 512u, dv_scan);
    s1.data = data; s1.idam_bit = 10u; s1.dam_bit = 30u; s1.data_end_bit = 100u;
    uft_d2_add_sector(d, t, &s1);
    uft_d2_sector_t s2 = guter_sektor(0u, 2u, 512u, dv_scan);
    s2.data = data; s2.data_crc_ok = false; s2.conf = 60u; s2.weak_bits = 3u;
    uft_d2_add_sector(d, t, &s2);

    uft_d2_track_t *t2 = uft_d2_track(d, 39u, 1u);
    uft_d2_sector_t s3 = guter_sektor(39u, 1u, 256u, dv_scan);
    s3.encoding = UFT_ENC_FM; s3.data = data;
    uft_d2_add_sector(d, t2, &s3);

    uft_d2_fs_t *fs = uft_d2_add_fs(d, UFT_D2_FS_FAT12, 200u, dv_scan);
    memcpy(fs->label, "TESTDISK", 9u);
    fs->cyl_from = 0u; fs->cyl_to = 39u;
    fs->counters_checked = true; fs->counters_consistent = false;
    uft_d2_entry_t e1; memset(&e1, 0, sizeof(e1));
    memcpy(e1.name, "HELLO.TXT", 10u);
    e1.size = 100u; e1.conf = UFT_D2_CONF_CERTAIN; e1.start_unit = 2u;
    uft_d2_entry_t e2; memset(&e2, 0, sizeof(e2));
    memcpy(e2.name, "_ELETED", 8u);
    e2.size = 300u; e2.deleted = true; e2.recoverable = true;
    e2.conf = 128u; e2.start_unit = 5u;
    uft_d2_add_entry(d, fs, &e1);
    uft_d2_add_entry(d, fs, &e2);

    uft_d2_fs_t *fs2 = uft_d2_add_fs(d, UFT_D2_FS_NONE_TRACKLOADER, 80u, dv_scan);
    fs2->cyl_from = 40u; fs2->cyl_to = 79u;

    uft_d2_add_meta(d, "Erzeuger", "IMD 1.18: 12/11/2004",
                    UFT_D2_META_FORMAT_FIELD);
    uft_d2_add_meta(d, "Kommentar", "Teac FD-55GFR", UFT_D2_META_FREE_TEXT);
    return d;
}

/** Feldweiser Vergleich. Was hier nicht steht, ist nicht geprueft. */
static bool gleich(const uft_disk2_t *a, const uft_disk2_t *b,
                   char *warum, size_t n) {
#define DIFF(cond, ...) do { if (cond) { snprintf(warum, n, __VA_ARGS__); \
                                         return false; } } while (0)
    DIFF(uft_d2_track_count(a) != uft_d2_track_count(b), "Spurzahl");
    DIFF(uft_d2_deriv_count(a) != uft_d2_deriv_count(b), "Ableitungen");
    DIFF(uft_d2_meta_count(a)  != uft_d2_meta_count(b),  "Metadaten");
    DIFF(uft_d2_fs_count(a)    != uft_d2_fs_count(b),    "Dateisysteme");
    DIFF(uft_d2_diag_count(a)  != uft_d2_diag_count(b),  "Befunde: %zu / %zu",
         uft_d2_diag_count(a), uft_d2_diag_count(b));

    for (size_t i = 0; i < uft_d2_track_count(a); ++i) {
        const uft_d2_track_t *x = uft_d2_track_at(a, i);
        const uft_d2_track_t *y = uft_d2_track_get(b, x->cyl, x->head);
        DIFF(!y, "Spur C%u H%u fehlt", (unsigned)x->cyl, (unsigned)x->head);
        DIFF(x->encoding != y->encoding, "Spurkodierung C%u", (unsigned)x->cyl);
        DIFF(x->unformatted != y->unformatted, "unformatiert");

        DIFF(x->flux.count != y->flux.count, "Umdrehungszahl C%u", (unsigned)x->cyl);
        DIFF(x->flux.gen != y->flux.gen, "Flussgeneration");
        DIFF(x->flux.deriv != y->flux.deriv, "Flussherkunft");
        for (size_t r = 0; r < x->flux.count; ++r) {
            const uft_d2_rev_t *p = &x->flux.revs[r], *q = &y->flux.revs[r];
            DIFF(p->count != q->count, "Flusszahl");
            DIFF(p->index_time_ns != q->index_time_ns, "Indexzeit");
            DIFF(p->complete != q->complete, "Vollstaendigkeit");
            DIFF(p->conf != q->conf, "Umdrehungskonfidenz");
            DIFF(p->count && memcmp(p->intervals, q->intervals,
                                    p->count * 4u) != 0, "Flussdaten");
        }

        DIFF(x->has_bitstream != y->has_bitstream, "Bitstrom da/nicht");
        if (x->has_bitstream) {
            const uft_d2_bitstream_t *p = &x->bitstream, *q = &y->bitstream;
            DIFF(p->nbits != q->nbits, "nbits");
            DIFF(p->gen != q->gen, "Bitstromgeneration");
            DIFF(p->deriv != q->deriv, "Bitstromherkunft");
            DIFF(p->index_bit != q->index_bit, "index_bit");
            DIFF(p->nrevs_fused != q->nrevs_fused, "nrevs_fused");
            DIFF(p->encoding != q->encoding, "Bitstromkodierung");
            DIFF(p->cell_ns != q->cell_ns, "Zellzeit");
            DIFF(memcmp(p->bits, q->bits, (p->nbits + 7u) / 8u) != 0, "bits");
            DIFF((p->bit_conf == NULL) != (q->bit_conf == NULL), "conf da/nicht");
            DIFF((p->agree == NULL) != (q->agree == NULL), "agree da/nicht");
            if (p->bit_conf) DIFF(memcmp(p->bit_conf, q->bit_conf, p->nbits) != 0, "bit_conf");
            if (p->agree) DIFF(memcmp(p->agree, q->agree, p->nbits) != 0, "agree");
            if (p->phase_q8) DIFF(memcmp(p->phase_q8, q->phase_q8, p->nbits * 2u) != 0, "phase");
            if (p->flux_count) DIFF(memcmp(p->flux_count, q->flux_count, p->nbits * 2u) != 0, "flux_count");
        }

        DIFF(x->sectors.count != y->sectors.count, "Sektorzahl C%u",
             (unsigned)x->cyl);
        DIFF(x->sectors.gen != y->sectors.gen, "Sektorgeneration");
        for (size_t k = 0; k < x->sectors.count; ++k) {
            const uft_d2_sector_t *p = &x->sectors.items[k];
            const uft_d2_sector_t *q = &y->sectors.items[k];
            DIFF(p->id_sec != q->id_sec || p->id_cyl != q->id_cyl
                 || p->id_head != q->id_head || p->id_size_code != q->id_size_code,
                 "Sektorkopf %zu", k);
            DIFF(p->conf != q->conf || p->origin != q->origin, "Zuversicht/Herkunft");
            DIFF(p->source_gen != q->source_gen || p->deriv != q->deriv,
                 "Sektorherkunft");
            DIFF(p->data_len != q->data_len, "data_len");
            DIFF(p->idam_bit != q->idam_bit || p->dam_bit != q->dam_bit
                 || p->data_end_bit != q->data_end_bit, "Bitlage");
            DIFF(p->weak_bits != q->weak_bits || p->fuzzy_bits != q->fuzzy_bits,
                 "weak/fuzzy");
            DIFF(p->encoding != q->encoding, "Sektorkodierung");
            DIFF(p->dam != q->dam, "Adressmarke");
            DIFF(p->data_crc_known != q->data_crc_known
                 || p->data_crc_ok != q->data_crc_ok
                 || p->id_crc_known != q->id_crc_known
                 || p->id_crc_ok != q->id_crc_ok, "CRC-Zustand");
            DIFF(p->has_data != q->has_data, "has_data");
            if (p->data_len) DIFF(memcmp(p->data, q->data, p->data_len) != 0,
                                  "Sektordaten");
        }
    }

    for (size_t i = 0; i < uft_d2_fs_count(a); ++i) {
        const uft_d2_fs_t *p = uft_d2_fs_at((uft_disk2_t *)a, i);
        const uft_d2_fs_t *q = uft_d2_fs_at((uft_disk2_t *)b, i);
        DIFF(p->kind != q->kind || p->kind_conf != q->kind_conf, "FS-Kopf %zu", i);
        DIFF(p->cyl_from != q->cyl_from || p->cyl_to != q->cyl_to, "FS-Bereich");
        DIFF(p->head_mask != q->head_mask, "FS-Kopfmaske");
        DIFF(strcmp(p->label, q->label) != 0, "FS-Etikett");
        DIFF(p->count != q->count || p->deleted_count != q->deleted_count,
             "FS-Eintraege");
        DIFF(p->counters_checked != q->counters_checked
             || p->counters_consistent != q->counters_consistent, "FS-Zaehler");
        DIFF(p->deriv != q->deriv || p->source_gen != q->source_gen, "FS-Herkunft");
        for (size_t k = 0; k < p->count; ++k)
            DIFF(strcmp(p->entries[k].name, q->entries[k].name) != 0
                 || p->entries[k].conf != q->entries[k].conf
                 || p->entries[k].size != q->entries[k].size
                 || p->entries[k].start_unit != q->entries[k].start_unit
                 || p->entries[k].deleted != q->entries[k].deleted
                 || p->entries[k].recoverable != q->entries[k].recoverable,
                 "Eintrag %zu", k);
    }

    for (size_t i = 0; i < uft_d2_meta_count(a); ++i) {
        const uft_d2_meta_t *p = uft_d2_meta_at(a, i);
        const uft_d2_meta_t *q = uft_d2_meta_at(b, i);
        DIFF(strcmp(p->key, q->key) != 0 || strcmp(p->value, q->value) != 0
             || p->src != q->src, "Metadatum %zu", i);
    }

    for (size_t i = 1; i <= uft_d2_deriv_count(a); ++i) {
        const uft_d2_derivation_t *p = uft_d2_deriv(a, (uft_d2_deriv_id_t)i);
        const uft_d2_derivation_t *q = uft_d2_deriv(b, (uft_d2_deriv_id_t)i);
        DIFF(!p || !q || strcmp(p->by, q->by) != 0
             || strcmp(p->params, q->params) != 0
             || p->source_gen != q->source_gen || p->from_layer != q->from_layer
             || p->origin != q->origin, "Ableitung %zu", i);
    }

    DIFF(uft_d2_features(a) != uft_d2_features(b), "Merkmale %08X != %08X",
         (unsigned)uft_d2_features(a), (unsigned)uft_d2_features(b));
    DIFF(uft_d2_layers(a) != uft_d2_layers(b), "Schichten");
#undef DIFF
    return true;
}

/* ═══════════ 1. Rundlauf ════════════════════════════════════════════ */

static void t1_rundlauf(void) {
    printf("Test 1: speichern -> laden -> identisch\n");
    uft_disk2_t *a = volles_modell();
    const uint32_t feat_a = uft_d2_features(a);

    uint8_t *buf = NULL; size_t n = 0u;
    uftd_result_t r = uftd_save(a, &buf, &n);
    CHECK(r.code == UFTD_OK, "speichern: %s", uftd_err_name(r.code));
    CHECK(n > 1000u, "Groesse plausibel, %zu Byte", n);
    CHECK(buf && memcmp(buf, "UFTD", 4u) == 0, "die Kennung steht vorn");

    uft_disk2_t *b = NULL;
    r = uftd_load(buf, n, &b);
    CHECK(r.code == UFTD_OK, "laden: %s (%s bei %zu)", uftd_err_name(r.code),
          r.what ? r.what : "", r.offset);
    CHECK(b != NULL, "Modell da");

    char warum[160] = "";
    CHECK(b && gleich(a, b, warum, sizeof(warum)),
          "Unterschied nach dem Rundlauf: %s", warum);
    CHECK(b && uft_d2_features(b) == feat_a,
          "die Merkmale sind aus den DATEN gerechnet, nicht geladen: "
          "%08X == %08X", (unsigned)uft_d2_features(b), (unsigned)feat_a);

    const uft_d2_track_t *t = b ? uft_d2_track_get(b, 0u, 0u) : NULL;
    CHECK(t && t->flux.count == 2u && t->bitstream.agree && t->bitstream.phase_q8
          && t->bitstream.flux_count && t->sectors.count == 2u,
          "alle vier Nebenreihen und beide Sektoren sind da");
    CHECK(t && t->sectors.items[1].weak_bits == 3u
          && t->sectors.items[1].conf == 60u,
          "der schlechte Sektor ist noch schlecht");
    if (b)
        printf("    %zu Byte, %zu Spuren, %zu Dateisysteme, %zu Ableitungen — "
               "identisch\n", n, uft_d2_track_count(b), uft_d2_fs_count(b),
               uft_d2_deriv_count(b));

    /* Doppelter Rundlauf: das Speichern des GELADENEN ergibt dieselben
     * Bytes. Der Entwurf erzeugte beim Laden die Einspeise-Befunde ein
     * zweites Mal — der Befundblock waere bei jeder Sicherung gewachsen. */
    uint8_t *buf2 = NULL; size_t n2 = 0u;
    if (b) uftd_save(b, &buf2, &n2);
    CHECK(n2 == n, "die zweite Sicherung ist gleich gross, %zu gegen %zu", n2, n);
    CHECK(n2 == n && buf && buf2 && memcmp(buf, buf2, n) == 0,
          "die GANZE Datei ist byteidentisch — auch der Befundblock");
    printf("    Doppelrundlauf: %zu Byte vollstaendig byteidentisch\n", n);

    free(buf); free(buf2);
    uft_d2_destroy(a); uft_d2_destroy(b);
}

/* ═══════════ 2. Fehler werden an ihrer Stelle gefunden ══════════════ */

static void t2_fehler(void) {
    printf("Test 2: ein gekipptes Bit wird an SEINER Stelle gefunden\n");
    uft_disk2_t *a = volles_modell();
    uint8_t *buf = NULL; size_t n = 0u;
    uftd_save(a, &buf, &n);
    uft_disk2_t *b = NULL;

    /* Gekipptes Bit mitten in einem Block. */
    uint8_t *kaputt = malloc(n);
    CHECK(kaputt != NULL, "Puffer");
    if (kaputt) {
        memcpy(kaputt, buf, n);
        size_t trak = 0u;
        for (size_t i = 16u; i + 4u < n; ++i)
            if (memcmp(kaputt + i, "TRAK", 4u) == 0) { trak = i; break; }
        CHECK(trak > 0u, "ein TRAK-Block ist da");
        kaputt[trak + 40u] ^= 0x01u;
        uftd_result_t r = uftd_load(kaputt, n, &b);
        CHECK(r.code == UFTD_E_CRC, "Pruefsumme erwartet, war %s",
              uftd_err_name(r.code));
        CHECK(strcmp(r.block, "TRAK") == 0, "im TRAK-Block, war \"%s\"", r.block);
        CHECK(r.offset == trak, "an Versatz %zu, gemeldet %zu", trak, r.offset);
        CHECK(b != NULL, "das Teilmodell ist trotzdem da");
        CHECK(b && uft_d2_meta_count(b) == 2u,
              "die Metadaten VOR der Stelle sind geladen, %zu",
              b ? uft_d2_meta_count(b) : 0u);
        CHECK(b && uft_d2_diag_count_sev(b, UFT_D2_DIAG_ERROR) >= 1u,
              "mit einem Fehlerbefund, der sagt wo");
        printf("    gekipptes Bit in TRAK -> Pruefsumme bei %zu, Metadaten "
               "davor erhalten\n", trak);
        uft_d2_destroy(b); b = NULL;
        free(kaputt);
    }

    /* Abgeschnitten. */
    uftd_result_t r = uftd_load(buf, n - 20u, &b);
    CHECK(r.code == UFTD_E_TRUNCATED, "abgeschnitten, war %s",
          uftd_err_name(r.code));
    CHECK(b != NULL, "Teilmodell da");
    uft_d2_destroy(b); b = NULL;

    /* Fremde Datei. */
    r = uftd_load((const uint8_t *)"SINCLAIR....", 12u, &b);
    CHECK(r.code == UFTD_E_MAGIC && b == NULL,
          "keine UFTD-Kennung -> gar kein Modell");

    /* Eine hoehere Fassung wird ABGEWIESEN. Die Bedingung des Entwurfs
     * konnte das nie: `(ver >> 8) > (UFTD_VERSION >> 8)` ist fuer jede
     * Fassung unter 256 falsch. */
    uint8_t *neuer = malloc(n);
    CHECK(neuer != NULL, "Puffer");
    if (neuer) {
        memcpy(neuer, buf, n);
        neuer[4] = 2u; neuer[5] = 0u;
        r = uftd_load(neuer, n, &b);
        CHECK(r.code == UFTD_E_VERSION, "Fassung 2 abgewiesen, war %s",
              uftd_err_name(r.code));
        CHECK(b == NULL, "und kein halbes Modell");
        free(neuer);
    }

    /* Ein unbekannter Block wird uebersprungen und GEMELDET. */
    size_t endpos = 0u;
    for (size_t i = 16u; i + 4u < n; ++i)
        if (memcmp(buf + i, "END ", 4u) == 0) endpos = i;
    CHECK(endpos > 0u, "der END-Block ist da");
    uint8_t *erweitert = malloc(n + 16u);
    CHECK(erweitert != NULL, "Puffer");
    if (erweitert && endpos) {
        memcpy(erweitert, buf, endpos);
        const uint8_t blk[12] = { 'X','Y','Z','W', 4u,0u,0u,0u, 1u,2u,3u,4u };
        memcpy(erweitert + endpos, blk, 12u);
        const uint32_t c = uftd_crc32(blk + 8, 4u);
        erweitert[endpos + 12u] = (uint8_t)c;
        erweitert[endpos + 13u] = (uint8_t)(c >> 8);
        erweitert[endpos + 14u] = (uint8_t)(c >> 16);
        erweitert[endpos + 15u] = (uint8_t)(c >> 24);
        memcpy(erweitert + endpos + 16u, buf + endpos, n - endpos);
        r = uftd_load(erweitert, n + 16u, &b);
        CHECK(b != NULL, "geladen");
        bool unbekannt = false;
        for (size_t i = 0; b && i < uft_d2_diag_count(b); ++i)
            if (strcmp(uft_d2_diag_at(b, i)->code, "UNKNOWN_BLOCK") == 0)
                unbekannt = true;
        CHECK(unbekannt, "der Block XYZW wird GEMELDET, nicht verschluckt");
        printf("    Block XYZW: uebersprungen und gemeldet — "
               "vorwaertsvertraeglich\n");
        uft_d2_destroy(b); b = NULL;
        free(erweitert);
    }

    free(buf);
    uft_d2_destroy(a);
}

/* ═══════════ 3. Laden ist nie stiller als Speichern ═════════════════ */

static void t3_gefaelscht(void) {
    printf("Test 3: eine gefaelschte Datei wird beim LADEN abgewiesen\n");
    /* Ein Modell mit genau EINEM guten Sektor. */
    uft_disk2_t *a = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(a, 0u, 0u);
    uft_d2_sector_t s = guter_sektor(0u, 1u, 4u, 0u);
    uint8_t data[4] = { 1u, 2u, 3u, 4u };
    s.data = data;
    CHECK(uft_d2_add_sector(a, t, &s), "der gute Sektor geht rein");

    uint8_t *buf = NULL; size_t n = 0u;
    uftd_save(a, &buf, &n);

    /* Die CRC-Flagge im SECT-Block kippen: aus „CRC stimmt" wird „CRC
     * falsch", die Zuversicht bleibt 255. Danach die Blockpruefsumme neu
     * rechnen, damit die Datei formal HEIL ist — nur inhaltlich gelogen.
     * Genau dagegen muss die Zuversichtsregel beim Laden greifen. */
    size_t sect = 0u;
    for (size_t i = 16u; i + 4u < n; ++i)
        if (memcmp(buf + i, "SECT", 4u) == 0) { sect = i; break; }
    CHECK(sect > 0u, "ein SECT-Block ist da");
    if (sect) {
        /* Aufbau: "SECT"(4) Laenge(4) | gen(4) count(4) | id_cyl id_head
         * id_sec id_size_code flags ... — das Flaggenbyte ist das fuenfte
         * Byte der Sektordaten. */
        const size_t flags_at = sect + 8u + 8u + 4u;
        buf[flags_at] = (uint8_t)(buf[flags_at] & (uint8_t)~4u);
        const uint32_t len = (uint32_t)buf[sect + 4u]
                           | ((uint32_t)buf[sect + 5u] << 8)
                           | ((uint32_t)buf[sect + 6u] << 16)
                           | ((uint32_t)buf[sect + 7u] << 24);
        const uint32_t c = uftd_crc32(buf + sect + 8u, len);
        for (int k = 0; k < 4; ++k)
            buf[sect + 8u + len + (size_t)k] = (uint8_t)(c >> (8 * k));
        /* Der umschliessende TRAK-Block und die Gesamt-CRC stimmen danach
         * nicht mehr — das ist in Ordnung: geprueft wird, ob die
         * Zuversichtsregel greift, und die greift VOR der Gesamt-CRC. */
        uft_disk2_t *b = NULL;
        uftd_load(buf, n, &b);
        bool unearned = false;
        size_t sektoren = 0u;
        if (b) {
            for (size_t i = 0; i < uft_d2_diag_count(b); ++i)
                if (strcmp(uft_d2_diag_at(b, i)->code, "CONF_UNEARNED") == 0)
                    unearned = true;
            const uft_d2_track_t *bt = uft_d2_track_get(b, 0u, 0u);
            if (bt) sektoren = bt->sectors.count;
        }
        CHECK(unearned,
              "ein Sektor mit Zuversicht 255 bei falscher CRC muss beim Laden "
              "als CONF_UNEARNED auffallen");
        CHECK(sektoren == 0u,
              "und er darf NICHT im Modell landen; %zu sind es doch", sektoren);
        printf("    Sektor 255 bei falscher CRC: abgewiesen, CONF_UNEARNED\n");
        uft_d2_destroy(b);
    }
    free(buf);
    uft_d2_destroy(a);
}

/* ═══════════ 4. Was ueberlaufen ist, laeuft auch danach ueber ═══════
 *
 * Zwei Zaehler haben kein oeffentliches Lesegeraet: wie viele Befunde
 * und wie viele Metadaten gar nicht erst gespeichert wurden. Gehen sie
 * beim Sichern verloren, hat die geladene Datei einen ANDEREN Bericht
 * als das Modell, aus dem sie stammt — eine stille Aenderung genau der
 * Art, gegen die der Behaelter gebaut ist.
 *
 * Der Entwurf sicherte `diag_hidden`/`diag_hidden_err`, aber nicht
 * `meta_hidden`; und er lud die Befunde ueber `uft_d2_diag()`, womit
 * eine volle Befundliste beim Laden ihren eigenen Ueberlauf-Eintrag
 * noch einmal als Ueberlauf gezaehlt haette. */
static void t4_ueberlauf(void) {
    printf("Test 4: was ueberlaufen ist, laeuft auch nach dem Rundlauf ueber\n");
    uft_disk2_t *a = uft_d2_create();
    uft_d2_track(a, 0u, 0u);

    for (unsigned i = 0; i < UFT_D2_MAX_META + 7u; ++i) {
        char k[UFT_D2_META_KEY];
        snprintf(k, sizeof(k), "schluessel%u", i);
        uft_d2_add_meta(a, k, "wert", UFT_D2_META_FREE_TEXT);
    }
    /* Mehr Befunde als Plaetze, darunter FEHLER hinter der Grenze. */
    for (unsigned i = 0; i < UFT_D2_MAX_DIAG + 9u; ++i)
        uft_d2_diag(a, (i % 100u == 99u) ? UFT_D2_DIAG_ERROR : UFT_D2_DIAG_NOTE,
                    UFT_D2_LAYER_SECTORS, 0, 0, (int)(i & 0xFFu), "VOLL",
                    "Befund %u", i);

    CHECK(uft_d2_meta_count(a) == UFT_D2_MAX_META, "Metadaten gedeckelt, %zu",
          uft_d2_meta_count(a));
    CHECK(uft_d2_diag_count(a) == UFT_D2_MAX_DIAG, "Befunde gedeckelt, %zu",
          uft_d2_diag_count(a));

    char vorher[65536];
    const size_t need = uft_d2_report(a, vorher, sizeof(vorher));
    CHECK(need < sizeof(vorher), "der Bericht passt, %zu Byte", need);

    uint8_t *buf = NULL; size_t n = 0u;
    uftd_result_t r = uftd_save(a, &buf, &n);
    CHECK(r.code == UFTD_OK, "speichern: %s", uftd_err_name(r.code));

    uft_disk2_t *b = NULL;
    r = uftd_load(buf, n, &b);
    CHECK(r.code == UFTD_OK, "laden: %s", uftd_err_name(r.code));

    CHECK(b && uft_d2_diag_count(b) == uft_d2_diag_count(a),
          "gleich viele Befunde, %zu gegen %zu",
          b ? uft_d2_diag_count(b) : 0u, uft_d2_diag_count(a));
    CHECK(b && uft_d2_meta_count(b) == uft_d2_meta_count(a),
          "gleich viele Metadaten");

    char nachher[65536];
    if (b) uft_d2_report(b, nachher, sizeof(nachher));
    CHECK(b && strcmp(vorher, nachher) == 0,
          "der Bericht ist Zeichen fuer Zeichen derselbe — sonst ist ein "
          "verdeckter Zaehler verloren gegangen");

    /* Und byteidentisch, wie im vollen Modell. */
    uint8_t *buf2 = NULL; size_t n2 = 0u;
    if (b) uftd_save(b, &buf2, &n2);
    CHECK(n2 == n && buf && buf2 && memcmp(buf, buf2, n) == 0,
          "auch hier byteidentisch, %zu gegen %zu", n2, n);
    printf("    %zu Metadaten und %zu Befunde ueber der Grenze: nach dem "
           "Rundlauf derselbe Bericht\n", (size_t)7u, (size_t)9u);

    free(buf); free(buf2);
    uft_d2_destroy(a); uft_d2_destroy(b);
}

int main(void) {
    printf("=== test_disk2_io (UFTD, MF-1275) ===\n\n");
    t1_rundlauf();
    t2_fehler();
    t3_gefaelscht();
    t4_ueberlauf();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN", g_fail);
    return g_fail ? 1 : 0;
}
