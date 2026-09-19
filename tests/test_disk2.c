/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_disk2.c
 * @brief Waechter fuer das Zentrum (MF-1272).
 *
 * Die neun Gruppen des Entwurfs, mit den Namen dieses Baums — und je
 * Berichtigung aus dem Kopf von `uft_disk2.h` eine Zusage mehr, die
 * VOR der Berichtigung rot war (gemessen am Entwurf, siehe Commit).
 *
 * Die wichtigsten Gruppen: 4 (Zuversicht steigt nie ohne Beleg — die
 * eine Regel, die alles zusammenhaelt) und 7 (Verlustpruefung — was ein
 * Zielformat nicht tragen kann, geht nicht still verloren).
 */

#include "uft/core/uft_disk2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)

static const uft_d2_derivation_t k_from_scp = {
    .from_layer = UFT_D2_LAYER_FLUX, .origin = UFT_D2_ORIGIN_CONTAINER,
    .by = "scp_reader", .params = "v1.9, footer=yes"
};
static const uft_d2_derivation_t k_from_pll = {
    .from_layer = UFT_D2_LAYER_FLUX, .origin = UFT_D2_ORIGIN_DERIVED,
    .by = "kalman_pll", .params = "cell=2000ns"
};
static const uft_d2_derivation_t k_from_scan = {
    .from_layer = UFT_D2_LAYER_BITSTREAM, .origin = UFT_D2_ORIGIN_DERIVED,
    .by = "ibm_mfm_scanner", .params = "past_index=yes"
};

static uft_d2_sector_t good_sector(uint8_t cyl, uint8_t sec, uint32_t len) {
    uft_d2_sector_t s;
    memset(&s, 0, sizeof(s));
    s.id_cyl = cyl; s.id_head = 0u; s.id_sec = sec; s.id_size_code = 2u;
    s.id_crc_ok = true; s.id_crc_known = true;
    s.data_len = len; s.dam = 0xFBu;
    s.data_crc_ok = true; s.data_crc_known = true; s.has_data = true;
    s.idam_bit = SIZE_MAX; s.dam_bit = SIZE_MAX; s.data_end_bit = SIZE_MAX;
    s.origin = UFT_D2_ORIGIN_CONTAINER;
    s.conf = UFT_D2_CONF_CERTAIN;
    s.deriv = k_from_scan;
    return s;
}

static size_t diag_mit_code(const uft_disk2_t *d, const char *code) {
    size_t n = 0u;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, code) == 0) n++;
    return n;
}

/* ═══════════ 1. Spuren entstehen bei Bedarf, nie vorab ═══════════════ */

static void t1_tracks(void) {
    printf("Test 1: Spuren bei Bedarf, Ausdehnung gemessen\n");

    uft_disk2_t *d = uft_d2_create();
    CHECK(d != NULL, "create");
    CHECK(uft_d2_track_count(d) == 0u, "leer");

    uint16_t mc; uint8_t mh;
    CHECK(!uft_d2_extent(d, &mc, &mh), "keine Ausdehnung ohne Spuren");

    uft_d2_track_t *t = uft_d2_track(d, 39u, 1u);
    CHECK(t != NULL && t->cyl == 39u && t->head == 1u, "angelegt");
    CHECK(uft_d2_track(d, 39u, 1u) == t, "zweiter Zugriff liefert dieselbe");
    CHECK(uft_d2_track_count(d) == 1u, "genau eine");

    /* Die Ausdehnung ist GEMESSEN — 39/1 heisst nicht 40 Spuren x 2. */
    CHECK(uft_d2_extent(d, &mc, &mh) && mc == 39u && mh == 1u,
          "Ausdehnung 39/1, war %u/%u", (unsigned)mc, (unsigned)mh);
    CHECK(uft_d2_track_get(d, 0u, 0u) == NULL,
          "Spur 0 wurde NICHT angelegt — nichts wird vorab erfunden");
    printf("    eine Spur bei 39/1, Spur 0 existiert nicht\n");

    uft_d2_destroy(d);
}

/* ═══════════ 2. Umdrehungen bleiben getrennt ═════════════════════════ */

static void t2_revolutions(void) {
    printf("Test 2: Umdrehungen GETRENNT, Indexzeit ist ein Befund\n");

    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);

    const uint32_t a[] = { 4000u, 4000u, 6000u };
    const uint32_t b[] = { 4000u, 4100u, 6000u };   /* leicht anders */
    CHECK(uft_d2_add_revolution(d, t, a, 3u, 200000000u, true, &k_from_scp),
          "rev 0");
    CHECK(uft_d2_add_revolution(d, t, b, 3u, 200400000u, true, &k_from_scp),
          "rev 1");

    CHECK(t->has_flux, "Flussschicht da");
    CHECK(t->flux.count == 2u, "zwei Umdrehungen, %zu", t->flux.count);
    CHECK(t->flux.revs[0].intervals[1] == 4000u &&
          t->flux.revs[1].intervals[1] == 4100u,
          "die beiden bleiben UNTERSCHIEDLICH — das ist der ganze Punkt");
    CHECK(t->flux.revs[0].intervals != a, "kopiert, nicht referenziert");
    CHECK(strcmp(t->flux.deriv.by, "scp_reader") == 0, "Herkunft steht dran");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_REVOLUTIONS, "Merkmal gemessen");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_INDEX_TIME, "Indexzeit gemessen");
    CHECK(uft_d2_diag_count(d) == 0u, "keine Befunde bei sauberer Lage");
    CHECK(t->flux.revs[0].conf == UFT_D2_CONF_CERTAIN,
          "vollstaendige Umdrehung: 255");

    /* Ohne Indexzeit: ein BEFUND, kein stilles Durchreichen. */
    uft_d2_add_revolution(d, t, a, 3u, 0u, true, &k_from_scp);
    CHECK(uft_d2_diag_count(d) == 1u, "ein Befund, %zu", uft_d2_diag_count(d));
    const uft_d2_diag_t *g = uft_d2_diag_at(d, 0u);
    CHECK(g && strcmp(g->code, "NO_INDEX_TIME") == 0, "und zwar der richtige");
    CHECK(g && g->sev == UFT_D2_DIAG_WARN, "als Warnung");
    printf("    3 Umdrehungen, die dritte ohne Indexzeit -> "
           "\"%.50s...\"\n", g ? g->text : "");

    /* MF-1272: unvollstaendig heisst UNVERIFIED — ein benannter Wert, kein
     * Literal 128, das zufaellig auch woanders stehen koennte. */
    uft_d2_add_revolution(d, t, a, 3u, 200000000u, false, &k_from_scp);
    CHECK(t->flux.revs[3].conf == UFT_D2_CONF_UNVERIFIED,
          "unvollstaendige Umdrehung traegt UNVERIFIED, hat %u",
          (unsigned)t->flux.revs[3].conf);
    CHECK(diag_mit_code(d, "REV_INCOMPLETE") == 1u, "REV_INCOMPLETE gemeldet");

    uft_d2_destroy(d);
}

/* ═══════════ 3. Bitstrom: gemessen, nicht geklemmt ═══════════════════ */

static void t3_bitstream(void) {
    printf("Test 3: Bitstrom mit gemessener Laenge und optionaler Konfidenz\n");

    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 5u, 0u);

    /* 104 000 Bit — mehr als 0x1900 Byte (= 51 200 Bit). Ein Leser, der
     * auf die DMK-Schreiblaenge klemmt, verloere die Haelfte. */
    const size_t nbits = 104000u;
    uint8_t *bits = calloc((nbits + 7u) / 8u, 1u);
    for (size_t i = 0; i < nbits / 8u; ++i) bits[i] = (uint8_t)(i * 7u);

    CHECK(uft_d2_set_bitstream(d, t, bits, nbits, NULL, NULL, NULL,
                               SIZE_MAX, UFT_ENC_MFM, 2000u, &k_from_pll),
          "setzen");
    CHECK(t->bitstream.nbits == 104000u,
          "Laenge GEMESSEN: 104000 erwartet, %zu", t->bitstream.nbits);
    CHECK(t->bitstream.bit_conf == NULL,
          "ohne Konfidenz bleibt es NULL — nicht mit 255 aufgefuellt");
    CHECK(t->encoding == UFT_ENC_MFM, "Kodierung je Spur");
    printf("    104000 Bit — nicht auf 51200 geklemmt\n");

    /* Zwei Befunde: keine Konfidenz, keine Indexlage. Beides sagt, was
     * DARAUF nicht messbar ist. */
    CHECK(uft_d2_diag_count(d) == 2u, "zwei Befunde, %zu", uft_d2_diag_count(d));
    const uft_d2_diag_t *g0 = uft_d2_diag_at(d, 0u);
    const uft_d2_diag_t *g1 = uft_d2_diag_at(d, 1u);
    CHECK(g0 && strcmp(g0->code, "NO_BIT_CONF") == 0, "NO_BIT_CONF");
    CHECK(g1 && strcmp(g1->code, "NO_INDEX_BIT") == 0, "NO_INDEX_BIT");
    CHECK(!(uft_d2_features(d) & UFT_D2_FEAT_WEAK_BITS),
          "ohne Konfidenz KEIN Weak-Bit-Merkmal — es ist nicht messbar");

    /* Mit Konfidenz: das Merkmal erscheint. */
    uft_d2_conf_t *conf = malloc(nbits);
    memset(conf, 255, nbits);
    conf[500] = 100u;   /* ein flackerndes Bit */
    uft_d2_set_bitstream(d, t, bits, nbits, conf, NULL, NULL,
                         0u, UFT_ENC_MFM, 2000u, &k_from_pll);
    CHECK(t->bitstream.bit_conf && t->bitstream.bit_conf[500] == 100u,
          "Konfidenz je Bit uebernommen");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_WEAK_BITS,
          "jetzt ist Weak-Bit ein Merkmal");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_GAPS, "Bitstrom heisst Luecken");
    printf("    mit Konfidenz je Bit: Weak-Bit-Merkmal gemessen\n");

    free(bits); free(conf);
    uft_d2_destroy(d);
}

/* ═══════════ 4. Zuversicht steigt nie ohne Beleg ═════════════════════ */

static void t4_confidence_rule(void) {
    printf("Test 4: Zuversicht 255 nur mit Beleg — die eine Regel\n");

    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);

    /* Sauber: geht. */
    uft_d2_sector_t ok = good_sector(0u, 1u, 512u);
    CHECK(uft_d2_add_sector(d, t, &ok), "CRC ok, Herkunft Abbild -> 255 erlaubt");

    /* CRC falsch und trotzdem 255: ABGEWIESEN. */
    uft_d2_sector_t bad = good_sector(0u, 2u, 512u);
    bad.data_crc_ok = false;
    CHECK(!uft_d2_add_sector(d, t, &bad),
          "CRC falsch mit 255 muss abgewiesen werden");
    CHECK(t->sectors.count == 1u, "und nicht aufgenommen, %zu", t->sectors.count);

    const uft_d2_diag_t *g = uft_d2_diag_at(d, uft_d2_diag_count(d) - 1u);
    CHECK(g && strcmp(g->code, "CONF_UNEARNED") == 0 && g->sev == UFT_D2_DIAG_ERROR,
          "mit Fehlerbefund");
    printf("    CRC falsch + 255 -> \"%.55s...\"\n", g ? g->text : "");

    /* Dieselbe Lage mit ehrlicher Zuversicht: geht. */
    bad.conf = 60u;
    CHECK(uft_d2_add_sector(d, t, &bad), "mit Zuversicht 60 erlaubt");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_BAD_CRC, "und als Merkmal gemessen");

    /* Rekonstruiert mit 255: ABGEWIESEN. */
    uft_d2_sector_t rec = good_sector(0u, 3u, 512u);
    rec.origin = UFT_D2_ORIGIN_RECONSTRUCTED;
    CHECK(!uft_d2_add_sector(d, t, &rec), "rekonstruiert mit 255 abgewiesen");
    rec.conf = 40u;
    CHECK(uft_d2_add_sector(d, t, &rec), "rekonstruiert mit 40 erlaubt");

    /* Flackernde Bits mit 255: ABGEWIESEN. */
    uft_d2_sector_t wk = good_sector(0u, 4u, 512u);
    wk.weak_bits = 3u;
    CHECK(!uft_d2_add_sector(d, t, &wk), "flackernd mit 255 abgewiesen");

    /* Fuellmaterial mit 255: ABGEWIESEN. */
    uft_d2_sector_t pad = good_sector(0u, 5u, 512u);
    pad.origin = UFT_D2_ORIGIN_PADDING;
    CHECK(!uft_d2_add_sector(d, t, &pad), "Fuellmaterial mit 255 abgewiesen");
    printf("    rekonstruiert / flackernd / CRC falsch / Fuellmaterial: "
           "255 nie ohne Beleg\n");

    uft_d2_destroy(d);
}

/* ═══════════ 5. Tatsachen werden benannt, nicht geglaettet ═══════════ */

static void t5_facts(void) {
    printf("Test 5: doppelte Nummern, ungleiche Groessen, fremder Zylinder\n");

    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 10u, 0u);

    uft_d2_sector_t s1 = good_sector(10u, 1u, 1024u);
    uft_d2_sector_t s2 = good_sector(10u, 2u, 1024u);
    uft_d2_sector_t s3 = good_sector(10u, 3u, 512u);    /* Ensoniq SQ80 */
    uft_d2_sector_t s1b = good_sector(10u, 1u, 1024u);  /* Duplikat */
    uft_d2_sector_t sx = good_sector(77u, 4u, 1024u);   /* fremder Zylinder */

    CHECK(uft_d2_add_sector(d, t, &s1), "1");
    CHECK(uft_d2_add_sector(d, t, &s2), "2");
    CHECK(uft_d2_add_sector(d, t, &s3), "3, andere Groesse");
    CHECK(uft_d2_add_sector(d, t, &s1b), "1 noch einmal — ERLAUBT");
    CHECK(uft_d2_add_sector(d, t, &sx), "Zylinder 77 auf Spur 10 — ERLAUBT");
    CHECK(t->sectors.count == 5u, "alle fuenf da, %zu", t->sectors.count);

    const uint32_t f = uft_d2_features(d);
    CHECK(f & UFT_D2_FEAT_VAR_SECTOR_SZ, "ungleiche Groessen gemessen");
    CHECK(f & UFT_D2_FEAT_DUP_SECTORS, "doppelte Nummer gemessen");

    /* Und jede Tatsache hat einen Befund. */
    CHECK(diag_mit_code(d, "DUP_SECTOR") == 1u, "ein DUP_SECTOR-Befund");
    CHECK(diag_mit_code(d, "ID_CYL_MISMATCH") == 1u, "ein ID_CYL_MISMATCH-Befund");
    printf("    5 Sektoren aufgenommen, 2 Befunde — nichts geglaettet, "
           "nichts verworfen\n");
    uft_d2_destroy(d);

    /* MF-1272: ein Adressfeld OHNE Datenfeld hat keine Groesse. Steht es
     * an erster Stelle, darf es die Spur nicht als „ungleiche Groessen"
     * ausweisen — im Entwurf war Sektor 0 der Bezug, und genau das geschah. */
    d = uft_d2_create();
    t = uft_d2_track(d, 3u, 0u);
    uft_d2_sector_t snd = good_sector(3u, 1u, 0u);
    snd.has_data = false; snd.data_crc_known = false;
    uft_d2_sector_t a = good_sector(3u, 2u, 512u);
    uft_d2_sector_t b = good_sector(3u, 3u, 512u);
    CHECK(uft_d2_add_sector(d, t, &snd), "ID ohne Daten");
    CHECK(uft_d2_add_sector(d, t, &a) && uft_d2_add_sector(d, t, &b), "zwei mit 512");
    CHECK(!(uft_d2_features(d) & UFT_D2_FEAT_VAR_SECTOR_SZ),
          "zwei gleiche Datenfelder plus ein leeres Adressfeld sind KEINE "
          "ungleichen Groessen");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_NO_DATA_SEC, "aber ein Sektor ohne Daten");
    printf("    Adressfeld ohne Daten zuerst: keine erfundene Groessenabweichung\n");
    uft_d2_destroy(d);
}

/* ═══════════ 6. Dateisystem: geloescht, abgerissen, Zaehler ══════════ */

static void t6_filesystem(void) {
    printf("Test 6: Dateisystemschicht mit Wiederherstellbarkeit\n");

    uft_disk2_t *d = uft_d2_create();
    uft_d2_fs_t *fs = uft_d2_fs(d);
    fs->kind = UFT_D2_FS_FAT12;
    fs->kind_conf = 200u;
    strcpy(fs->label, "TESTDISK");

    uft_d2_entry_t live = { .name = "HELLO.TXT", .size = 100u,
                            .conf = 255u, .start_unit = 2u };
    uft_d2_entry_t del  = { .name = "_ELETED.TXT", .size = 300u,
                            .deleted = true, .recoverable = true,
                            .conf = 128u, .start_unit = 5u };
    uft_d2_entry_t brk  = { .name = "BROKEN.BIN", .size = 4000u,
                            .chain_broken = true, .conf = 80u,
                            .start_unit = 9u };
    CHECK(uft_d2_add_entry(d, &live), "lebend");
    CHECK(uft_d2_add_entry(d, &del), "geloescht");
    CHECK(uft_d2_add_entry(d, &brk), "abgerissen");

    CHECK(fs->count == 3u && fs->deleted_count == 1u, "Zaehlung");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_DELETED_FILES,
          "geloeschte Dateien als Merkmal");
    CHECK(uft_d2_layers(d) & (1u << UFT_D2_LAYER_FILESYSTEM), "Schicht da");

    /* Ein abgerissener Eintrag erzeugt einen Befund, der VERSUCH sagt. */
    bool found = false;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, "CHAIN_BROKEN") == 0 &&
            strstr(uft_d2_diag_at(d, i)->text, "VERSUCH")) found = true;
    CHECK(found, "CHAIN_BROKEN muss 'VERSUCH' sagen");
    printf("    3 Eintraege: lebend, geloescht+wiederherstellbar, "
           "abgerissen=Versuch\n");

    /* MF-1272: ein Name, der das Feld ganz fuellt, wird beim Einspeisen
     * terminiert — sonst laese der Bericht hinter das Feld. */
    uft_d2_entry_t lang;
    memset(&lang, 0, sizeof(lang));
    memset(lang.name, 'A', sizeof(lang.name));   /* 64 x 'A', kein NUL */
    lang.conf = 10u;
    CHECK(uft_d2_add_entry(d, &lang), "langer Name angenommen");
    CHECK(fs->entries[3].name[63] == '\0', "und terminiert");
    CHECK(strlen(fs->entries[3].name) == 63u, "auf 63 Zeichen, %zu",
          strlen(fs->entries[3].name));

    uft_d2_destroy(d);
}

/* ═══════════ 7. Verlustpruefung ══════════════════════════════════════ */

static void t7_loss(void) {
    printf("Test 7: was ein Zielformat nicht tragen kann, geht nicht "
           "still verloren\n");

    /* Ein Traeger mit allem: Fluss, Bitstrom mit Konfidenz, Sektoren mit
     * schlechter CRC, geloeschte Dateien, Metadaten. */
    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    const uint32_t iv[] = { 4000u, 4000u };
    uft_d2_add_revolution(d, t, iv, 2u, 200000000u, true, &k_from_scp);
    uft_d2_add_revolution(d, t, iv, 2u, 200000000u, true, &k_from_scp);
    uint8_t bits[64] = {0}; uft_d2_conf_t conf[512]; memset(conf, 255, 512);
    uft_d2_set_bitstream(d, t, bits, 512u, conf, NULL, NULL, 0u,
                         UFT_ENC_MFM, 2000u, &k_from_pll);
    uft_d2_sector_t s = good_sector(0u, 1u, 512u);
    s.data_crc_ok = false; s.conf = 60u;
    uft_d2_add_sector(d, t, &s);
    uft_d2_entry_t e = { .name = "X", .deleted = true, .conf = 100u };
    uft_d2_add_entry(d, &e);
    uft_d2_add_meta(d, "Erzeuger", "UnifiedFloppyTool", UFT_D2_META_SELF);

    /* Ziel: ein flaches .img — nur Sektoren, keine Merkmale. */
    const uint32_t img_layers = 1u << UFT_D2_LAYER_SECTORS;
    uint32_t lost_l = 0u, lost_f = 0u;
    CHECK(!uft_d2_check_loss(d, img_layers, 0u, &lost_l, &lost_f),
          "nach .img geht etwas verloren");
    CHECK(lost_l & (1u << UFT_D2_LAYER_FLUX), "Fluss verloren");
    CHECK(lost_l & (1u << UFT_D2_LAYER_BITSTREAM), "Bitstrom verloren");
    CHECK(lost_l & (1u << UFT_D2_LAYER_FILESYSTEM), "Dateisystemsicht verloren");
    CHECK(lost_f & UFT_D2_FEAT_REVOLUTIONS, "Umdrehungen verloren");
    CHECK(lost_f & UFT_D2_FEAT_WEAK_BITS, "Weak-Bits verloren");
    CHECK(lost_f & UFT_D2_FEAT_BAD_CRC,
          "CRC-Zustand verloren — ein .img macht aus einem schlechten "
          "Sektor einen guten");
    CHECK(lost_f & UFT_D2_FEAT_DELETED_FILES, "geloeschte Dateien verloren");
    CHECK(lost_f & UFT_D2_FEAT_METADATA, "Metadaten verloren");

    unsigned n = 0u;
    for (unsigned b = 0; b < UFT_D2_FEAT_COUNT; ++b) if (lost_f & (1u << b)) n++;
    printf("    -> .img: 3 Schichten und %u Merkmale gehen verloren — "
           "BENANNT\n", n);

    /* Ziel: SCP — Fluss mit Umdrehungen, aber keine Sektorschicht. Ein
     * SCP traegt keine CRC-Zustaende, weil es keine Sektoren kennt. */
    const uint32_t scp_layers = 1u << UFT_D2_LAYER_FLUX;
    const uint32_t scp_feat = UFT_D2_FEAT_REVOLUTIONS | UFT_D2_FEAT_INDEX_TIME
                            | UFT_D2_FEAT_METADATA;
    uft_d2_check_loss(d, scp_layers, scp_feat, &lost_l, &lost_f);
    CHECK(!(lost_f & UFT_D2_FEAT_REVOLUTIONS), "SCP traegt Umdrehungen");
    CHECK(!(lost_f & UFT_D2_FEAT_METADATA), "und Metadaten");
    CHECK(lost_l & (1u << UFT_D2_LAYER_SECTORS),
          "aber die abgeleitete Sektorschicht steht nicht in der Datei");
    printf("    -> .scp: Umdrehungen bleiben, abgeleitete Schichten "
           "nicht\n");

    /* Ziel: ein Format, das ALLES traegt — nichts verloren. */
    CHECK(uft_d2_check_loss(d, 0xFu, 0xFFFFu, &lost_l, &lost_f),
          "in ein vollstaendiges Format geht nichts verloren");
    CHECK(lost_l == 0u && lost_f == 0u, "beide Masken null");

    uft_d2_destroy(d);
}

/* ═══════════ 8. Befunde laufen nie stumm ueber ═══════════════════════ */

static void t8_overflow(void) {
    printf("Test 8: Befund- und Metadatenueberlauf werden gemeldet\n");

    uft_disk2_t *d = uft_d2_create();
    unsigned angenommen = 0u;
    for (unsigned i = 0; i < 600u; ++i)
        if (uft_d2_diag(d, UFT_D2_DIAG_INFO, UFT_D2_LAYER_SECTORS, 0, 0, (int)i,
                        "FILL", "Befund %u", i))
            angenommen++;

    const size_t n = uft_d2_diag_count(d);
    CHECK(n == UFT_D2_MAX_DIAG, "gedeckelt bei %u, %zu", UFT_D2_MAX_DIAG, n);
    /* MF-1272: der letzte Platz ist von Anfang an reserviert. 511
     * angenommen, der 512. abgewiesen — im Entwurf bekam der 512. ein
     * `true` und wurde danach still ueberschrieben. */
    CHECK(angenommen == UFT_D2_MAX_DIAG - 1u,
          "genau %u angenommen, %u", UFT_D2_MAX_DIAG - 1u, angenommen);
    const uft_d2_diag_t *last = uft_d2_diag_at(d, n - 1u);
    CHECK(last && strcmp(last->code, "DIAG_OVERFLOW") == 0,
          "der LETZTE Platz ist die Ueberlaufmeldung");
    CHECK(last && last->sev == UFT_D2_DIAG_WARN, "als Warnung");
    CHECK(last && strstr(last->text, "UNVOLLSTAENDIG") != NULL, "und sagt es");
    const uft_d2_diag_t *vorletzt = uft_d2_diag_at(d, n - 2u);
    CHECK(vorletzt && strcmp(vorletzt->code, "FILL") == 0
          && strcmp(vorletzt->text, "Befund 510") == 0,
          "der 511. Befund steht unversehrt davor: %s", vorletzt ? vorletzt->text : "");

    char buf[4096];
    uft_d2_report(d, buf, sizeof(buf));
    CHECK(strstr(buf, "UNVOLLSTAENDIG") != NULL,
          "im Bericht ganz oben, nicht am Ende:\n%.120s", buf);
    printf("    600 Befunde -> 511 + 1 Ueberlaufmeldung, im Bericht "
           "zuerst\n");

    uft_d2_destroy(d);
}

/* ═══════════ 9. Der Bericht ══════════════════════════════════════════ */

static void t9_report(void) {
    printf("Test 9: Bericht\n");

    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    const uint32_t iv[] = { 4000u };
    uft_d2_add_revolution(d, t, iv, 1u, 200000000u, true, &k_from_scp);
    uft_d2_add_revolution(d, t, iv, 1u, 0u, true, &k_from_scp);
    uft_d2_sector_t s = good_sector(0u, 1u, 512u);
    uft_d2_add_sector(d, t, &s);
    uft_d2_sector_t u = good_sector(0u, 2u, 512u);
    u.data_crc_known = false; u.data_crc_ok = false; u.conf = UFT_D2_CONF_UNVERIFIED;
    uft_d2_add_sector(d, t, &u);
    uft_d2_add_meta(d, "Erzeuger", "IMD 1.18: 12/11/2004", UFT_D2_META_FORMAT_FIELD);
    uft_d2_fs_t *fs = uft_d2_fs(d);
    fs->kind = UFT_D2_FS_NONE_TRACKLOADER; fs->kind_conf = 80u;

    char buf[4096];
    const size_t p = uft_d2_report(d, buf, sizeof(buf));
    CHECK(p > 0u, "leer");
    CHECK(p == strlen(buf), "ungekuerzt: Rueckgabe %zu = Laenge %zu", p, strlen(buf));
    CHECK(strncmp(buf, "WARNUNG", 7) == 0, "Warnung zuerst:\n%.100s", buf);
    CHECK(strstr(buf, "gemessen") != NULL, "Ausdehnung als gemessen");
    CHECK(strstr(buf, "ohne Indexzeit: 1") != NULL, "Indexzeit-Luecke");
    CHECK(strstr(buf, "Trackloader") != NULL, "Dateisystem: keines");
    CHECK(strstr(buf, "IMD 1.18") != NULL, "Metadaten");
    /* MF-1272: drei Zahlen statt „falsche CRC: 0" — ein Sektor mit
     * Pruefsumme, einer ohne Angabe. */
    CHECK(strstr(buf, "mit CRC-Angabe: 1 (davon falsch: 0), ohne CRC-Angabe: 1")
          != NULL, "Sektorzeile unterscheidet 'falsch' von 'nicht getragen':\n%s", buf);
    printf("    --- Beispielbericht ---\n%s", buf);
    printf("    -----------------------\n");

    /* MF-1272: eine Kuerzung ist am Rueckgabewert erkennbar. */
    char klein[64];
    const size_t need = uft_d2_report(d, klein, sizeof(klein));
    CHECK(need == p, "benoetigte Laenge unabhaengig vom Puffer: %zu vs %zu", need, p);
    CHECK(need >= sizeof(klein), "und groesser als der kleine Puffer");
    CHECK(strlen(klein) == sizeof(klein) - 1u, "Puffer voll und terminiert");
    CHECK(strncmp(klein, buf, sizeof(klein) - 1u) == 0, "Anfang identisch");
    printf("    64-Byte-Puffer: Rueckgabe %zu sagt 'gekuerzt'\n", need);

    uft_d2_destroy(d);
}

int main(void) {
    printf("=== test_disk2 (MF-1272) ===\n\n");
    t1_tracks();
    t2_revolutions();
    t3_bitstream();
    t4_confidence_rule();
    t5_facts();
    t6_filesystem();
    t7_loss();
    t8_overflow();
    t9_report();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN",
           g_fail);
    return g_fail ? 1 : 0;
}
