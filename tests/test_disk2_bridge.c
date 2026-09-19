/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_disk2_bridge.c
 * @brief Die Bruecke traegt — an einer ECHTEN Datei und an einem
 *        gestellten Plugin (MF-1272).
 *
 * Zwei Teile, und beide sind noetig:
 *
 *  A  Das Korpus-86F (`fluxfox_sector_test_360k.86f`, im Git, also in
 *     CI) ueber `uft_format_plugin_86f` geoeffnet und in das Zentrum
 *     eingespeist. Belegt wird, dass die Bruecke NICHTS verliert und
 *     NICHTS erfindet: dieselbe Zahl Sektoren wie das Plugin direkt,
 *     byteidentische Daten, und — weil das 86F-Plugin keine Pruefsumme
 *     durchreicht (`uft_format_add_sector_with_id` setzt OK unbedingt)
 *     — `data_crc_known == false` und Zuversicht UNVERIFIED, nicht 255.
 *
 *  B  Ein gestelltes Plugin, dessen `read_track` genau die Faelle
 *     liefert, die das Korpus-86F NICHT hat: CRC-Fehler, fehlender
 *     Sektor, Weak-Flagge, Rohdaten ohne Bitlaenge. Ohne Teil B waere die
 *     Uebersetzung dieser Faelle unbelegt — der Korpus uebt nur den Weg
 *     „CRC nicht getragen".
 *
 * D2-Rotprobe: `uft_d2_add_sector()` in der Bruecke entfernen ->
 * Teil A meldet 0 statt der gemessenen Zahl (siehe Commit).
 */

#include "uft/core/uft_disk2_bridge.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es fuer diesen Test setzen"
#endif
#define KORPUS_86F UFT_CORPUS_DIR "/fluxfox_sector_test_360k.86f"

/* Gemessen MF-1272 an genau dieser Datei ueber `uft_format_plugin_86f`:
 * 160 Spuren mit Sektoren, 1440 Sektoren (172 Eintraege gefragt, die
 * Zylinder 80-85 der Tabelle sind leer). Die Zahl ist der Anker gegen
 * stilles Abdriften — der Vergleich mit der Direktzaehlung darunter
 * fiele nicht auf, wenn Plugin UND Bruecke gleich weniger lieferten.
 *
 * Zweite Quelle, unabhaengig von diesem Plugin: die Diskette liegt als
 * `fluxfox_sector_test_360k.img` daneben — 40 x 2 x 9 = 720 Sektoren
 * (`test_86f_pri_gegen_fluxfox.c`). Das 86F legt jeden Abbildzylinder
 * DOPPELT ab (`test_86f_spec_conformance.c`: „die Verdopplung: wie
 * Zylinder 0"), also 2 x 720 = 1440 und 2 x 40 x 2 = 160. */
#define GEMESSEN_SPUREN    160u
#define GEMESSEN_SEKTOREN  1440u

extern const uft_format_plugin_t uft_format_plugin_86f;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)

static size_t diag_mit_code(const uft_disk2_t *d, const char *code) {
    size_t n = 0u;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, code) == 0) n++;
    return n;
}

/* Dieselbe vollstaendige Freigabe wie in der Bruecke — zwei halbe
 * Aufraeumer im Baum (MF-599), siehe uft_disk2_bridge.c. */
static void spur_freigeben(uft_track_t *t) {
    free(t->confidence);  t->confidence = NULL;
    free(t->weak_mask);   t->weak_mask = NULL;
    free(t->flux_times);  t->flux_times = NULL;
    if (t->revisions) {
        for (size_t i = 0; i < t->revision_count; i++) free(t->revisions[i].data);
        free(t->revisions); t->revisions = NULL; t->revision_count = 0;
    }
    uft_track_cleanup(t);
}

/* ═══════════ A. Die echte Datei ══════════════════════════════════════ */

static void a_korpus_86f(void) {
    printf("Teil A: Korpus-86F ueber das Plugin in das Zentrum\n");

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    const uft_error_t rc = uft_format_plugin_86f.open(&disk, KORPUS_86F, true);
    CHECK(rc == UFT_OK, "open(%s) gab %d — die Datei liegt im Git, ein "
          "Fehlen ist ein Fehler, kein Skip", KORPUS_86F, (int)rc);
    if (rc != UFT_OK) return;

    /* Direktzaehlung ueber das Plugin — die erste Quelle. */
    size_t direkt_spuren = 0, direkt_sektoren = 0;
    for (unsigned c = 0; c < disk.geometry.cylinders; c++)
        for (unsigned h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t; memset(&t, 0, sizeof t);
            if (uft_format_plugin_86f.read_track(&disk, (int)c, (int)h, &t) == UFT_OK
                && t.sector_count) {
                direkt_spuren++;
                direkt_sektoren += t.sector_count;
            }
            spur_freigeben(&t);
        }
    printf("    Plugin direkt: %zu Spuren mit Sektoren, %zu Sektoren "
           "(Geometrie %u x %u)\n", direkt_spuren, direkt_sektoren,
           (unsigned)disk.geometry.cylinders, (unsigned)disk.geometry.heads);

    /* Die Bruecke. */
    uft_disk2_t *d = uft_d2_create();
    uft_d2_bridge_stats_t st;
    CHECK(uft_d2_from_disk(d, &disk, &uft_format_plugin_86f, &st), "Bruecke");
    printf("    Bruecke: %zu Spuren gefragt, %zu mit Sektoren, %zu Sektoren, "
           "%zu abgewiesen, %zu Bitstroeme\n", st.tracks_asked,
           st.tracks_with_sectors, st.sectors, st.sectors_rejected, st.bitstreams);

    CHECK(st.sectors == direkt_sektoren,
          "Bruecke %zu Sektoren, Plugin direkt %zu", st.sectors, direkt_sektoren);
    CHECK(st.tracks_with_sectors == direkt_spuren,
          "Bruecke %zu Spuren, Plugin direkt %zu", st.tracks_with_sectors, direkt_spuren);
    CHECK(uft_d2_track_count(d) == direkt_spuren,
          "im Zentrum %zu Spuren (nur die mit Inhalt), erwartet %zu",
          uft_d2_track_count(d), direkt_spuren);
    CHECK(st.sectors_rejected == 0u, "nichts abgewiesen, %zu", st.sectors_rejected);
    CHECK(st.bitstreams == 0u && st.raw_without_bits == 0u,
          "das 86F-Plugin liefert keinen Rohbitstrom an uft_track_t");
    CHECK(st.sectors == GEMESSEN_SEKTOREN && st.tracks_with_sectors == GEMESSEN_SPUREN,
          "Anker: %zu Sektoren auf %zu Spuren, gemessen MF-1272 waren es "
          "%u auf %u", st.sectors, st.tracks_with_sectors,
          GEMESSEN_SEKTOREN, GEMESSEN_SPUREN);

    /* Jeder Sektor: Herkunft, CRC-Ehrlichkeit, Zuversicht, Herkunftszeile. */
    size_t geprueft = 0, mit_crc = 0, mit_255 = 0, ohne_daten = 0;
    for (unsigned c = 0; c < disk.geometry.cylinders; c++)
        for (unsigned h = 0; h < disk.geometry.heads; h++) {
            const uft_d2_track_t *t = uft_d2_track_get(d, (uint16_t)c, (uint8_t)h);
            if (!t) continue;
            for (size_t i = 0; i < t->sectors.count; i++) {
                const uft_d2_sector_t *s = &t->sectors.items[i];
                geprueft++;
                if (s->data_crc_known) mit_crc++;
                if (s->conf == UFT_D2_CONF_CERTAIN) mit_255++;
                if (!s->has_data) ohne_daten++;
                if (s->origin != UFT_D2_ORIGIN_CONTAINER
                    || strcmp(s->deriv.by, "uft_d2_bridge") != 0
                    || s->deriv.params != uft_format_plugin_86f.name
                    || s->idam_bit != SIZE_MAX || s->weak_bits != 0u) {
                    CHECK(0, "Sektor C%u H%u R%u: Herkunft/Ableitung/Lage falsch",
                          c, h, (unsigned)s->id_sec);
                }
            }
        }
    CHECK(geprueft == st.sectors, "alle %zu erreichbar, %zu", st.sectors, geprueft);
    CHECK(mit_crc == 0u, "das 86F-Plugin reicht keine Pruefsumme durch — "
          "%zu Sektoren behaupten trotzdem eine", mit_crc);
    CHECK(mit_255 == 0u, "ohne Pruefsumme keine 255 — %zu tragen sie", mit_255);
    CHECK(ohne_daten == 0u, "%zu Sektoren ohne Daten", ohne_daten);

    /* Byteidentitaet an einer Spur, die das Konformitaetstest-Raster
     * kennt (Zyl 2 Kopf 0 = Abbild-Zylinder 1): Reihenfolge und Inhalt. */
    {
        uft_track_t t; memset(&t, 0, sizeof t);
        CHECK(uft_format_plugin_86f.read_track(&disk, 2, 0, &t) == UFT_OK, "Spur 2/0");
        const uft_d2_track_t *dt = uft_d2_track_get(d, 2u, 0u);
        CHECK(dt && dt->sectors.count == t.sector_count,
              "Spur 2/0: %zu im Zentrum, %zu im Plugin",
              dt ? dt->sectors.count : 0u, t.sector_count);
        size_t gleich = 0;
        if (dt)
            for (size_t i = 0; i < t.sector_count && i < dt->sectors.count; i++) {
                const uft_sector_t *a = &t.sectors[i];
                const uft_d2_sector_t *b = &dt->sectors.items[i];
                if (b->id_sec == a->id.sector && b->data_len == a->data_len
                    && b->data != a->data           /* KOPIE, nicht Zeiger */
                    && memcmp(b->data, a->data, a->data_len) == 0)
                    gleich++;
            }
        CHECK(gleich == t.sector_count, "Spur 2/0: %zu von %zu Sektoren "
              "byteidentisch und in Reihenfolge", gleich, t.sector_count);
        printf("    Spur 2/0: %zu von %zu Sektoren byteidentisch, als Kopie\n",
               gleich, t.sector_count);
        spur_freigeben(&t);
    }

    /* Schichten, Merkmale, Verlust — gemessen an der echten Diskette. */
    CHECK(uft_d2_layers(d) == (1u << UFT_D2_LAYER_SECTORS),
          "nur die Sektorschicht, Maske 0x%x", uft_d2_layers(d));
    const uint32_t f = uft_d2_features(d);
    CHECK(!(f & (UFT_D2_FEAT_BAD_CRC | UFT_D2_FEAT_WEAK_BITS | UFT_D2_FEAT_GAPS)),
          "keine erfundenen Merkmale, Maske 0x%x", f);
    uint32_t ll = 0, lf = 0;
    CHECK(uft_d2_check_loss(d, 1u << UFT_D2_LAYER_SECTORS, f, &ll, &lf),
          "nach einem Sektorformat geht von dieser Diskette nichts verloren");
    CHECK(!uft_d2_check_loss(d, 1u << UFT_D2_LAYER_FLUX, 0u, &ll, &lf)
          && (ll & (1u << UFT_D2_LAYER_SECTORS)),
          "nach einem reinen Flussformat ginge die Sektorschicht verloren");
    CHECK(uft_d2_diag_count_sev(d, UFT_D2_DIAG_ERROR) == 0u, "keine Fehlerbefunde");
    const char *pl = uft_d2_meta(d, "Plugin");
    CHECK(pl && strcmp(pl, uft_format_plugin_86f.name) == 0,
          "Metadatum Plugin = %s", pl ? pl : "(fehlt)");

    char buf[8192];
    const size_t need = uft_d2_report(d, buf, sizeof buf);
    CHECK(need < sizeof buf, "Bericht passt, braucht %zu", need);
    char erwartet[96];
    snprintf(erwartet, sizeof erwartet,
             "mit CRC-Angabe: 0 (davon falsch: 0), ohne CRC-Angabe: %zu", st.sectors);
    CHECK(strstr(buf, erwartet) != NULL, "Bericht sagt '%s':\n%s", erwartet, buf);
    printf("    --- Bericht ---\n%s    ---------------\n", buf);

    uft_d2_destroy(d);
    uft_format_plugin_86f.close(&disk);
}

/* ═══════════ B. Ein gestelltes Plugin fuer die Faelle, die A nicht hat ═ */

static uft_error_t fake_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *t) {
    (void)disk;
    memset(t, 0, sizeof *t);
    t->cylinder = cyl; t->head = head;
    if (cyl == 0 && head == 0) {
        t->sectors = calloc(4, sizeof(uft_sector_t));
        t->sector_count = 4;
        for (int i = 0; i < 4; i++) {
            uft_sector_t *s = &t->sectors[i];
            s->id.cylinder = 0; s->id.head = 0; s->id.sector = (uint8_t)(i + 1);
            s->id.size_code = 2;
            s->data = malloc(512); memset(s->data, 0x11 * (i + 1), 512);
            s->data_len = 512; s->data_size = 512; s->data_mark = 0xFB;
        }
        /* 1: CRC gemessen und richtig */
        t->sectors[0].crc_stored = 0x1234; t->sectors[0].crc_calculated = 0x1234;
        t->sectors[0].crc_ok = true;
        /* 2: CRC gemessen und falsch, per Flagge */
        t->sectors[1].status = UFT_SECTOR_CRC_ERROR;
        t->sectors[1].crc_stored = 0x1234; t->sectors[1].crc_calculated = 0x5678;
        /* 3: fehlt — Fuellmaterial des Formats */
        t->sectors[2].status = UFT_SECTOR_MISSING;
        /* 4: schwach, nur die Flagge */
        t->sectors[3].weak = true;
        t->sectors[3].confidence = 0.5f;   /* Plugin sagt: halbe Zuversicht */
        return UFT_OK;
    }
    if (cyl == 1 && head == 0) {
        /* Rohdaten OHNE Bitlaenge: darf nicht als Bitstrom erfunden werden. */
        t->raw_data = malloc(100); memset(t->raw_data, 0xAA, 100);
        t->raw_size = 100; t->raw_bits = 0;
        return UFT_OK;
    }
    if (cyl == 2 && head == 0) {
        /* Rohdaten MIT Bitlaenge und Bitrate: wird Bitstrom mit Zellbreite. */
        t->raw_data = malloc(100); memset(t->raw_data, 0x55, 100);
        t->raw_size = 100; t->raw_bits = 793; t->bitrate = 250000;
        t->encoding = UFT_ENC_MFM;
        return UFT_OK;
    }
    return UFT_ERROR_IO;
}

static void b_gestelltes_plugin(void) {
    printf("Teil B: gestelltes Plugin — CRC falsch, fehlend, schwach, "
           "Rohdaten ohne Bitlaenge\n");

    uft_format_plugin_t fake;
    memset(&fake, 0, sizeof fake);
    fake.name = "fake";
    fake.read_track = fake_read_track;

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.geometry.cylinders = 4;   /* Zyl 3 scheitert absichtlich */
    disk.geometry.heads = 1;

    uft_disk2_t *d = uft_d2_create();
    uft_d2_bridge_stats_t st;
    CHECK(uft_d2_from_disk(d, &disk, &fake, &st), "Bruecke");
    CHECK(st.tracks_asked == 4u && st.tracks_failed == 1u,
          "4 gefragt, 1 gescheitert: %zu/%zu", st.tracks_asked, st.tracks_failed);
    CHECK(st.sectors == 4u && st.sectors_rejected == 0u,
          "alle vier Sektoren angenommen — die Regel wies keinen ab, weil die "
          "Bruecke keine 255 ohne Beleg vergibt: %zu/%zu",
          st.sectors, st.sectors_rejected);
    CHECK(st.raw_without_bits == 1u && st.bitstreams == 1u,
          "eine Spur Rohdaten ohne Bitlaenge (gezaehlt), eine mit (Bitstrom): "
          "%zu/%zu", st.raw_without_bits, st.bitstreams);

    const uft_d2_track_t *t = uft_d2_track_get(d, 0u, 0u);
    CHECK(t && t->sectors.count == 4u, "Spur 0/0 mit 4 Sektoren");
    if (t && t->sectors.count == 4u) {
        const uft_d2_sector_t *s = t->sectors.items;
        CHECK(s[0].data_crc_known && s[0].data_crc_ok && s[0].conf == UFT_D2_CONF_CERTAIN,
              "Sektor 1: CRC bekannt+ok -> 255, hat known=%d ok=%d conf=%u",
              s[0].data_crc_known, s[0].data_crc_ok, (unsigned)s[0].conf);
        CHECK(s[1].data_crc_known && !s[1].data_crc_ok
              && s[1].conf == UFT_D2_BRIDGE_CONF_BAD_CRC,
              "Sektor 2: CRC bekannt+falsch -> BAD_CRC (%u), hat %u",
              UFT_D2_BRIDGE_CONF_BAD_CRC, (unsigned)s[1].conf);
        CHECK(s[2].origin == UFT_D2_ORIGIN_PADDING && !s[2].has_data
              && s[2].conf == UFT_D2_CONF_NONE,
              "Sektor 3: fehlend -> Fuellmaterial, keine Daten, Zuversicht 0");
        CHECK(s[3].weak_bits == 1u && s[3].conf == 128u,
              "Sektor 4: Weak-Flagge -> Untergrenze 1, Zuversicht min(UNVERIFIED, "
              "0.5*255=128) = 128, hat weak=%u conf=%u",
              s[3].weak_bits, (unsigned)s[3].conf);
        CHECK(s[0].dam == 0xFBu, "Datenmarke uebernommen");
    }
    const uint32_t f = uft_d2_features(d);
    CHECK((f & UFT_D2_FEAT_BAD_CRC) && (f & UFT_D2_FEAT_WEAK_BITS)
          && (f & UFT_D2_FEAT_NO_DATA_SEC) && (f & UFT_D2_FEAT_GAPS),
          "Merkmale gemessen: BAD_CRC, WEAK, NO_DATA, GAPS — Maske 0x%x", f);
    CHECK(diag_mit_code(d, "DATA_CRC") == 1u, "ein DATA_CRC-Befund");
    CHECK(diag_mit_code(d, "RAW_BITS_UNKNOWN") == 1u, "RAW_BITS_UNKNOWN gemeldet");
    CHECK(diag_mit_code(d, "TRACKS_FAILED") == 1u, "TRACKS_FAILED gemeldet");

    const uft_d2_track_t *t2 = uft_d2_track_get(d, 2u, 0u);
    CHECK(t2 && t2->has_bitstream && t2->bitstream.nbits == 793u
          && t2->bitstream.cell_ns == 4000u && t2->encoding == UFT_ENC_MFM,
          "Spur 2/0: Bitstrom 793 Bit, Zelle 4000 ns aus 250 kbps, MFM");
    CHECK(uft_d2_track_get(d, 1u, 0u) == NULL,
          "Spur 1/0 (Rohdaten ohne Bitlaenge, keine Sektoren) wird NICHT angelegt");

    char buf[4096];
    uft_d2_report(d, buf, sizeof buf);
    CHECK(strstr(buf, "mit CRC-Angabe: 2 (davon falsch: 1), ohne CRC-Angabe: 2") != NULL,
          "Sektorzeile:\n%s", buf);
    printf("    4 Sektoren: 255 / 64 / 0 / 128 — jede Zahl aus ihrer Regel\n");

    uft_d2_destroy(d);
}

int main(void) {
    printf("=== test_disk2_bridge (MF-1272) ===\n\n");
    a_korpus_86f();
    b_gestelltes_plugin();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN", g_fail);
    return g_fail ? 1 : 0;
}
