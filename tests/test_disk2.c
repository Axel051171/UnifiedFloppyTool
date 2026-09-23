/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_disk2.c
 * @brief Waechter fuer das Zentrum, zweite Fassung (MF-1274).
 *
 * Acht Gruppen. Die ersten sechs decken, was in MF-1274 dazukam; die
 * siebte den Bericht, die achte die neun Garantien aus MF-1272 — sie
 * muessen unveraendert halten, sonst ist die zweite Fassung ein
 * Rueckschritt mit mehr Zeilen.
 *
 * Was hier NICHT steht: der Rundlauf durch den Behaelter. Der Behaelter
 * kommt als eigener Commit und bringt seinen eigenen Test mit — ein Tor
 * wird allein und zuerst committet.
 */

#include "uft/core/uft_disk2.h"
#include "uft/core/uft_source_facts.h"  /* MF-1318 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)

/** Ein Sektor, der seine 255 auch verdient: CRC getragen und richtig. */
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

static bool hat_befund(const uft_disk2_t *d, const char *code) {
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, code) == 0) return true;
    return false;
}

/* ═══════════ 1. Generationen und validate() ═════════════════════════ */

static void t1_generationen(void) {
    printf("Test 1: abgeleitete Objekte wissen, ob ihre Quelle noch die ist\n");
    uft_disk2_t *d = uft_d2_create();
    const uft_d2_deriv_id_t dv = uft_d2_register_deriv(d,
        UFT_D2_LAYER_BITSTREAM, UFT_D2_ORIGIN_DERIVED, "scanner", "", 1u);
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    uint8_t bits[8] = { 0xAAu, 0, 0, 0, 0, 0, 0, 0 };
    uft_d2_set_bitstream(d, t, bits, 64u, NULL, NULL, 0u, NULL, NULL,
                         SIZE_MAX, UFT_ENC_MFM, 2000u, dv);
    CHECK(t->bitstream.gen == 1u, "erster Bitstrom ist Gen 1, war %u",
          (unsigned)t->bitstream.gen);

    uft_d2_sector_t s = guter_sektor(0u, 1u, 8u, dv);
    s.idam_bit = 0u; s.dam_bit = 8u; s.data_end_bit = 40u;
    CHECK(uft_d2_add_sector(d, t, &s), "Sektor angenommen");
    CHECK(t->sectors.items[0].source_gen == 1u,
          "Sektor merkt sich Gen 1, war %u",
          (unsigned)t->sectors.items[0].source_gen);
    CHECK(uft_d2_validate(d) == 0u, "stimmig — keine neuen Befunde");

    /* Bitstrom ERSETZEN. Der Sektor ist jetzt veraltet. */
    const size_t vorher = uft_d2_diag_count(d);
    uft_d2_set_bitstream(d, t, bits, 64u, NULL, NULL, 0u, NULL, NULL,
                         SIZE_MAX, UFT_ENC_MFM, 2000u, dv);
    CHECK(t->bitstream.gen == 2u, "jetzt Gen 2, war %u",
          (unsigned)t->bitstream.gen);
    bool ersetzt = false;
    for (size_t i = vorher; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, "BITSTREAM_REPLACED") == 0)
            ersetzt = true;
    CHECK(ersetzt, "das Ersetzen wird gemeldet, waehrend Sektoren daran haengen");

    const size_t neu = uft_d2_validate(d);
    CHECK(neu >= 1u, "validate findet den veralteten Sektor, %zu neue Befunde",
          neu);
    CHECK(hat_befund(d, "STALE_DERIV"), "als STALE_DERIV");
    printf("    Bitstrom Gen 1 -> 2, Sektor aus Gen 1: STALE_DERIV\n");

    /* Bitlagen jenseits des Bitstroms. */
    uft_d2_sector_t weit = guter_sektor(0u, 2u, 8u, dv);
    weit.idam_bit = 999u; weit.source_gen = 2u;
    uft_d2_add_sector(d, t, &weit);
    uft_d2_validate(d);
    CHECK(hat_befund(d, "POS_BEYOND"), "idam_bit 999 in 64 Bit: POS_BEYOND");

    uft_d2_destroy(d);
}

/* ═══════════ 2. Ableitungsregister ══════════════════════════════════ */

static void t2_register(void) {
    printf("Test 2: Ableitungen sind registriert, nicht Freitext\n");
    uft_disk2_t *d = uft_d2_create();
    CHECK(uft_d2_deriv_count(d) == 0u, "leer");
    CHECK(uft_d2_deriv(d, UFT_D2_DERIV_NONE) == NULL,
          "Kennung 0 heisst KEINE und darf kein Objekt treffen");

    const uft_d2_deriv_id_t a = uft_d2_register_deriv(d, UFT_D2_LAYER_FLUX,
        UFT_D2_ORIGIN_DERIVED, "kalman_pll", "cell=2000ns run=2", 3u);
    const uft_d2_deriv_id_t b = uft_d2_register_deriv(d, UFT_D2_LAYER_FLUX,
        UFT_D2_ORIGIN_DERIVED, "kalman_pll", "cell=2000ns run=3", 3u);
    CHECK(a == 1u && b == 2u, "Kennungen 1 und 2, waren %u %u",
          (unsigned)a, (unsigned)b);
    CHECK(uft_d2_deriv_count(d) == 2u, "zwei registriert");
    const uft_d2_derivation_t *v = uft_d2_deriv(d, a);
    CHECK(v && strcmp(v->by, "kalman_pll") == 0 && v->source_gen == 3u,
          "Inhalt stimmt");

    /* KOPIERT, nicht gezeigt — das ist der Unterschied zu MF-1272, und er
     * ist der Grund, warum die Herkunft ueberhaupt serialisierbar ist. */
    char fluechtig[32];
    memcpy(fluechtig, "verschwindet_gleich", 20u);
    const uft_d2_deriv_id_t c = uft_d2_register_deriv(d, UFT_D2_LAYER_FLUX,
        UFT_D2_ORIGIN_DERIVED, fluechtig, "x", 0u);
    memset(fluechtig, 'X', sizeof(fluechtig));
    CHECK(strcmp(uft_d2_deriv(d, c)->by, "verschwindet_gleich") == 0,
          "der Name wurde KOPIERT, nicht gezeigt: \"%s\"",
          uft_d2_deriv(d, c)->by);

    /* Zu lang wird gekuerzt und terminiert, nicht ueber das Feld hinaus
     * geschrieben. */
    char lang[200];
    memset(lang, 'A', sizeof(lang) - 1u); lang[sizeof(lang) - 1u] = '\0';
    const uft_d2_deriv_id_t e = uft_d2_register_deriv(d, UFT_D2_LAYER_FLUX,
        UFT_D2_ORIGIN_DERIVED, lang, lang, 0u);
    CHECK(strlen(uft_d2_deriv(d, e)->by) == UFT_D2_DERIV_BY - 1u,
          "auf %u gekuerzt, war %zu", (unsigned)UFT_D2_DERIV_BY - 1u,
          strlen(uft_d2_deriv(d, e)->by));
    CHECK(strlen(uft_d2_deriv(d, e)->params) == UFT_D2_DERIV_PARAMS - 1u,
          "Parameter auf %u gekuerzt", (unsigned)UFT_D2_DERIV_PARAMS - 1u);

    /* „Welche Sektoren stammen aus Lauf 2?" ist jetzt eine Abfrage. */
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    uft_d2_sector_t s1 = guter_sektor(0u, 1u, 8u, a);
    uft_d2_sector_t s2 = guter_sektor(0u, 2u, 8u, b);
    uft_d2_sector_t s3 = guter_sektor(0u, 3u, 8u, a);
    uft_d2_add_sector(d, t, &s1);
    uft_d2_add_sector(d, t, &s2);
    uft_d2_add_sector(d, t, &s3);
    unsigned aus_a = 0u;
    for (size_t i = 0; i < t->sectors.count; ++i)
        if (t->sectors.items[i].deriv == a) aus_a++;
    CHECK(aus_a == 2u, "zwei aus Lauf 2, gezaehlt %u", aus_a);
    printf("    Abfrage \"aus Lauf 2\": %u Sektoren\n", aus_a);

    uft_d2_destroy(d);
}

/* ═══════════ 3. Stimmen je Bit ══════════════════════════════════════ */

static void t3_stimmen(void) {
    printf("Test 3: \"3 von 5\" ist nachpruefbar\n");
    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    uint8_t bits[8];
    memset(bits, 0, sizeof(bits));
    uint8_t agree[64]; memset(agree, 5, sizeof(agree)); agree[7] = 3u;
    uft_d2_conf_t conf[64]; memset(conf, 255, sizeof(conf)); conf[7] = 153u;

    /* Stimmen ohne Umdrehungszahl: abgewiesen. Eine nicht deutbare Zahl
     * ist schlimmer als keine. */
    CHECK(!uft_d2_set_bitstream(d, t, bits, 64u, conf, agree, 0u, NULL, NULL,
                                SIZE_MAX, UFT_ENC_MFM, 2000u, 0u),
          "agree ohne nrevs_fused ist nicht deutbar");
    CHECK(hat_befund(d, "VOTES_NO_BASE"), "und es wird gesagt, warum");
    CHECK(!t->has_bitstream, "und nichts ist halb gesetzt");

    CHECK(uft_d2_set_bitstream(d, t, bits, 64u, conf, agree, 5u, NULL, NULL,
                               SIZE_MAX, UFT_ENC_MFM, 2000u, 0u), "mit 5");
    CHECK(t->bitstream.agree && t->bitstream.agree[7] == 3u
          && t->bitstream.nrevs_fused == 5u, "Bit 7: 3 von 5");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_VOTES, "Merkmal „Stimmen je Bit\"");
    printf("    Bit 7: Konfidenz %u UND 3 von 5 Stimmen — beides gespeichert\n",
           (unsigned)t->bitstream.bit_conf[7]);
    uft_d2_destroy(d);
}

/* ═══════════ 4. Mehrere Dateisysteme ════════════════════════════════ */

static void t4_mehrere_fs(void) {
    printf("Test 4: eine Diskette, zwei Dateisysteme\n");
    uft_disk2_t *d = uft_d2_create();
    for (uint16_t c = 0; c < 80u; ++c) uft_d2_track(d, c, 0u);

    uft_d2_fs_t *a = uft_d2_add_fs(d, UFT_D2_FS_FAT12, 200u, 0u);
    CHECK(a != NULL, "erstes Dateisystem");
    a->cyl_from = 0u; a->cyl_to = 39u;
    uft_d2_fs_t *b = uft_d2_add_fs(d, UFT_D2_FS_NONE_TRACKLOADER, 90u, 0u);
    CHECK(b != NULL, "zweites Dateisystem");
    b->cyl_from = 40u; b->cyl_to = 79u;

    CHECK(uft_d2_fs_count(d) == 2u, "zwei, gezaehlt %zu", uft_d2_fs_count(d));
    CHECK(uft_d2_fs_at(d, 1u)->kind == UFT_D2_FS_NONE_TRACKLOADER,
          "das zweite ist der Trackloader");
    CHECK(uft_d2_fs_at(d, 2u) == NULL, "ein drittes gibt es nicht");
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_MULTI_FS, "Merkmal MULTI_FS");
    CHECK(uft_d2_layers(d) & (1u << UFT_D2_LAYER_FILESYSTEM),
          "die Dateisystemschicht ist da");

    /* Ein Dateisystem, das jenseits der Spuren beginnt. */
    uft_d2_fs_t *c = uft_d2_add_fs(d, UFT_D2_FS_CBMDOS, 50u, 0u);
    c->cyl_from = 90u; c->cyl_to = 100u;
    uft_d2_validate(d);
    CHECK(hat_befund(d, "FS_RANGE"),
          "FS_RANGE fuer Zylinder 90 bei hoechster Spur 79");
    printf("    FAT12 auf 0..39, Trackloader auf 40..79, drittes jenseits "
           "-> FS_RANGE\n");
    uft_d2_destroy(d);
}

/* ═══════════ 5. Gerechnet statt gespeichert ═════════════════════════ */

static void t5_gerechnet(void) {
    printf("Test 5: crosses_index und Kodierung werden gerechnet\n");
    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    uint8_t bits[16];
    memset(bits, 0, sizeof(bits));
    uft_d2_set_bitstream(d, t, bits, 128u, NULL, NULL, 0u, NULL, NULL,
                         64u, UFT_ENC_MFM, 2000u, 0u);

    uft_d2_sector_t s = guter_sektor(0u, 1u, 8u, 0u);
    s.idam_bit = 10u; s.dam_bit = 30u; s.data_end_bit = 100u;
    CHECK(uft_d2_sector_crosses_index(t, &s), "10..100 kreuzt Index 64");
    /* Beide Felder muessen hinter den Index: das ID-Feld 10..70 kreuzt
     * Index 64 sehr wohl (IOI), auch wenn nur das Datenfeld verschoben
     * wurde. Genau diese Unterscheidung ist der Zweck der Funktion. */
    s.idam_bit = 66u; s.dam_bit = 70u;
    CHECK(!uft_d2_sector_crosses_index(t, &s), "66..100 kreuzt nicht");
    /* Der Fall, der die Unterscheidung TRAEGT: das Adressfeld liegt VOR
     * dem Index, das Datenfeld dahinter. Wer nur `dam_bit` prueft, sagt
     * hier „kreuzt nicht" — und uebersieht genau das IOI-Schutzmuster.
     * Ohne diese Zusage waere die Unterscheidung nicht bewacht. */
    s.idam_bit = 10u; s.dam_bit = 70u; s.data_end_bit = 100u;
    CHECK(uft_d2_sector_crosses_index(t, &s),
          "Adressfeld bei 10, Datenfeld ab 70, Index 64: das ID-Feld kreuzt "
          "(IOI) — eine Pruefung nur auf dam_bit sieht das nicht");
    s.idam_bit = 10u; s.dam_bit = 30u;
    const size_t merk = t->bitstream.index_bit;
    t->bitstream.index_bit = SIZE_MAX;
    CHECK(!uft_d2_sector_crosses_index(t, &s),
          "ohne bekannte Indexlage wird nichts behauptet");
    t->bitstream.index_bit = merk;

    CHECK(uft_d2_sector_encoding(t, &s) == UFT_ENC_MFM, "Sektor erbt die Spur");
    s.encoding = UFT_ENC_FM;
    CHECK(uft_d2_sector_encoding(t, &s) == UFT_ENC_FM, "die eigene gewinnt");
    uft_d2_add_sector(d, t, &s);
    CHECK(uft_d2_features(d) & UFT_D2_FEAT_MIXED_ENC,
          "FM-Sektor auf MFM-Spur = gemischte Kodierung");
    printf("    Index 64: 10..100 kreuzt, 66..100 nicht; FM auf MFM = gemischt\n");

    /* Spur und Bitstrom widersprechen sich. `UFT_ENC_GCR` aus dem Entwurf
     * gibt es hier nicht: der Baum fuehrt GCR nach Familie getrennt
     * (CBM, Apple 5.25", Apple 3.5", Victor) — 20 Kodierungen statt vier. */
    t->encoding = UFT_ENC_GCR_CBM;
    uft_d2_validate(d);
    CHECK(hat_befund(d, "ENC_MISMATCH"), "ENC_MISMATCH Spur GCR / Bitstrom MFM");
    uft_d2_destroy(d);
}

/* ═══════════ 6. Zuversichtsregel an allen Schichten ═════════════════ */

static void t6_zuversicht(void) {
    printf("Test 6: 255 nur mit Beleg — an ALLEN Schichten\n");
    uft_disk2_t *d = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(d, 0u, 0u);
    const uint32_t iv[] = { 4000u };

    CHECK(!uft_d2_add_revolution(d, t, iv, 1u, 200000000u, false, 255u, 0u),
          "Schicht 1: unvollstaendige Umdrehung mit 255 abgewiesen");
    CHECK(uft_d2_add_revolution(d, t, iv, 1u, 200000000u, false,
                                UFT_D2_CONF_UNVERIFIED, 0u),
          "mit UNVERIFIED erlaubt");

    uft_d2_sector_t bad = guter_sektor(0u, 1u, 8u, 0u);
    bad.data_crc_ok = false;
    CHECK(!uft_d2_add_sector(d, t, &bad),
          "Schicht 3: falsche CRC mit 255 abgewiesen");
    uft_d2_sector_t pad = guter_sektor(0u, 2u, 8u, 0u);
    pad.origin = UFT_D2_ORIGIN_PADDING;
    CHECK(!uft_d2_add_sector(d, t, &pad),
          "Fuellmaterial mit 255 abgewiesen — es stand nichts auf dem Traeger");
    uft_d2_sector_t weak = guter_sektor(0u, 3u, 8u, 0u);
    weak.weak_bits = 1u;
    CHECK(!uft_d2_add_sector(d, t, &weak), "Weak Bits mit 255 abgewiesen");

    uft_d2_fs_t *fs = uft_d2_add_fs(d, UFT_D2_FS_FAT12, 200u, 0u);
    uft_d2_entry_t brk;
    memset(&brk, 0, sizeof(brk));
    memcpy(brk.name, "X", 2u); brk.chain_broken = true;
    brk.conf = UFT_D2_CONF_CERTAIN;
    CHECK(!uft_d2_add_entry(d, fs, &brk),
          "Schicht 4: abgerissene Kette mit 255 abgewiesen — MF-1272 liess "
          "das durch");
    brk.conf = 80u;
    CHECK(uft_d2_add_entry(d, fs, &brk), "mit 80 erlaubt");
    uft_d2_entry_t del;
    memset(&del, 0, sizeof(del));
    memcpy(del.name, "D", 2u); del.deleted = true;
    del.conf = UFT_D2_CONF_CERTAIN;
    CHECK(!uft_d2_add_entry(d, fs, &del), "geloescht mit 255 abgewiesen");

    CHECK(uft_d2_diag_count_sev(d, UFT_D2_DIAG_ERROR) >= 5u,
          "jede Abweisung ist ein FEHLER-Befund, gezaehlt %zu",
          uft_d2_diag_count_sev(d, UFT_D2_DIAG_ERROR));
    printf("    Umdrehung, Sektor, Eintrag: dieselbe Regel, drei Schichten\n");
    uft_d2_destroy(d);
}

/* ═══════════ 7. Der Bericht ═════════════════════════════════════════ */

static void t7_bericht(void) {
    printf("Test 7: der Bericht sagt, was er nicht zeigt\n");
    uft_disk2_t *d = uft_d2_create();
    for (int i = 0; i < 45; ++i)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_SECTORS, 0, 0, i,
                    "N", "Notiz %d", i);
    uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, 0, 0, 46,
                "LATE", "spaeter Fehler");

    char buf[8192];
    const size_t need = uft_d2_report(d, buf, sizeof(buf));
    CHECK(need < sizeof(buf), "passt in 8 KiB, gebraucht %zu", need);
    CHECK(strncmp(buf, "FEHLER: 1", 9) == 0,
          "die Fehlerzahl steht OBEN, der Bericht beginnt mit:\n%.40s", buf);
    CHECK(strstr(buf, "und 6 weitere, davon 1 FEHLER") != NULL,
          "ein Fehler auf Platz 46 muss in der Kuerzungszeile stehen");
    printf("    46 Befunde, Fehler auf 46: \"... und 6 weitere, davon 1 FEHLER\"\n");

    /* Der Rueckgabewert ist die BENOETIGTE Laenge (Bauform snprintf) —
     * ein Aufrufer, der ihn fuer die geschriebene haelt, kuerzt still. */
    char klein[64];
    const size_t need2 = uft_d2_report(d, klein, sizeof(klein));
    CHECK(need2 == need, "dieselbe benoetigte Laenge, %zu gegen %zu",
          need2, need);
    CHECK(strlen(klein) == sizeof(klein) - 1u,
          "der kleine Puffer ist voll und terminiert, %zu", strlen(klein));
    CHECK(need2 > sizeof(klein), "und der Rueckgabewert sagt, dass gekuerzt wurde");
    uft_d2_destroy(d);

    /* Die Sektorzeile unterscheidet „CRC falsch" von „CRC nicht getragen" —
     * ein Abbild ohne Pruefsumme darf kein Urteil in EINE Richtung
     * bekommen (MF-662, MF-1273). */
    uft_disk2_t *e = uft_d2_create();
    uft_d2_track_t *t = uft_d2_track(e, 0u, 0u);
    for (uint8_t i = 0; i < 3u; ++i) {
        uft_d2_sector_t s = guter_sektor(0u, i, 256u, 0u);
        s.data_crc_known = false; s.data_crc_ok = false;
        s.conf = UFT_D2_CONF_UNVERIFIED;
        uft_d2_add_sector(e, t, &s);
    }
    char b2[4096];
    uft_d2_report(e, b2, sizeof(b2));
    CHECK(strstr(b2, "Sektoren: 3 — mit CRC-Angabe: 0 (davon falsch: 0), "
                     "ohne CRC-Angabe: 3") != NULL,
          "drei Zahlen statt einer:\n%s", b2);
    CHECK(strncmp(b2, "Traeger:", 8) == 0,
          "ohne Fehler beginnt der Bericht mit der Ausdehnung");
    uft_d2_destroy(e);
}

/* ═══════════ 8. Die alten Garantien halten ══════════════════════════ */

static void t8_alte_garantien(void) {
    printf("Test 8: die Garantien aus MF-1272\n");
    uft_disk2_t *d = uft_d2_create();

    CHECK(uft_d2_track_get(d, 0u, 0u) == NULL,
          "keine Spur wird auf Vorrat angelegt");
    uft_d2_track_t *t = uft_d2_track(d, 39u, 1u);
    CHECK(t != NULL, "Spur 39/1 angelegt");
    uint16_t mc; uint8_t mh;
    CHECK(uft_d2_extent(d, &mc, &mh) && mc == 39u && mh == 1u,
          "Ausdehnung GEMESSEN: C%u H%u", (unsigned)mc, (unsigned)mh);
    CHECK(uft_d2_track_get(d, 39u, 1u) == t, "der Index findet sie wieder");
    CHECK(uft_d2_track_at(d, 0u) == t && uft_d2_track_at(d, 1u) == NULL,
          "und die Aufzaehlung auch");

    const uint32_t iv[] = { 1u, 2u };
    uft_d2_add_revolution(d, t, iv, 2u, 0u, true, UFT_D2_CONF_CERTAIN, 0u);
    CHECK(hat_befund(d, "NO_INDEX_TIME"),
          "eine Umdrehung ohne Indexzeit wird gesagt, nicht geraten");

    /* Nicht geklemmt: 104 000 Bit bleiben 104 000 Bit. */
    uint8_t *bits = calloc(13000u, 1u);
    CHECK(bits != NULL, "Puffer");
    if (bits) {
        uft_d2_set_bitstream(d, t, bits, 104000u, NULL, NULL, 0u, NULL, NULL,
                             SIZE_MAX, UFT_ENC_MFM, 0u, 0u);
        CHECK(t->bitstream.nbits == 104000u, "nicht geklemmt, %zu",
              t->bitstream.nbits);
        free(bits);
    }

    /* Verlust nach .img: die Flussschicht faellt weg, und es wird benannt. */
    const uint32_t img = 1u << UFT_D2_LAYER_SECTORS;
    uint32_t ll = 0u, lf = 0u;
    CHECK(!uft_d2_check_loss(d, img, 0u, &ll, &lf)
          && (ll & (1u << UFT_D2_LAYER_FLUX)),
          "Verlust nach .img benannt, verlorene Schichten %08X", ll);

    /* Ein Eintragsname von voller Feldlaenge bleibt terminiert. */
    uft_d2_fs_t *fs = uft_d2_add_fs(d, UFT_D2_FS_FAT12, 100u, 0u);
    uft_d2_entry_t lang;
    memset(&lang, 0, sizeof(lang));
    memset(lang.name, 'N', UFT_D2_ENTRY_NAME);   /* ohne Null! */
    lang.conf = UFT_D2_CONF_CERTAIN;
    CHECK(uft_d2_add_entry(d, fs, &lang), "langer Name angenommen");
    CHECK(strlen(fs->entries[fs->count - 1u].name) == UFT_D2_ENTRY_NAME - 1u,
          "und NUL-terminiert, Laenge %zu",
          strlen(fs->entries[fs->count - 1u].name));

    /* Metadaten: dynamisch bis zur Grenze, dann ein BEFUND statt Stille. */
    for (unsigned i = 0; i < UFT_D2_MAX_META + 4u; ++i) {
        char k[32];
        snprintf(k, sizeof(k), "k%u", i);
        uft_d2_add_meta(d, k, "v", UFT_D2_META_FREE_TEXT);
    }
    CHECK(uft_d2_meta_count(d) == UFT_D2_MAX_META, "bei %u gedeckelt, %zu",
          (unsigned)UFT_D2_MAX_META, uft_d2_meta_count(d));
    CHECK(hat_befund(d, "META_OVERFLOW"),
          "und der Ueberlauf wird gemeldet — in MF-1272 fiel er STILL heraus");

    /* Befunde: 511 angenommen, der 512. wird zur Ueberlaufmeldung. */
    while (uft_d2_diag_count(d) < UFT_D2_MAX_DIAG - 1u)
        uft_d2_diag(d, UFT_D2_DIAG_INFO, UFT_D2_LAYER_FLUX, 0, 0, 0, "F", "x");
    CHECK(uft_d2_diag_count(d) == UFT_D2_MAX_DIAG - 1u,
          "511 gespeichert, %zu", uft_d2_diag_count(d));
    CHECK(!uft_d2_diag(d, UFT_D2_DIAG_INFO, UFT_D2_LAYER_FLUX, 0, 0, 0,
                       "F", "einer zuviel"),
          "der 512. wird ABGEWIESEN");
    CHECK(uft_d2_diag_count(d) == UFT_D2_MAX_DIAG, "und wird zur Meldung, %zu",
          uft_d2_diag_count(d));
    CHECK(strcmp(uft_d2_diag_at(d, UFT_D2_MAX_DIAG - 1u)->code,
                 "DIAG_OVERFLOW") == 0,
          "auf dem reservierten letzten Platz");

    printf("    Vorab-Anlage, Ausdehnung, Klemmen, Verlust, Ueberlauf: halten\n");
    uft_d2_destroy(d);
}

/* t9 — die Projektion sagt dieselben Zahlen wie der Bericht (MF-1318).
 *
 * `uft_d2_report()` rechnet sechs Zahlen und schreibt sie in einen Text;
 * bis MF-1318 gab es keine Funktion, die sie zurueckgibt. Die neue
 * Projektion rechnet sie ein zweites Mal — und genau das ist die Gefahr:
 * zwei Rechnungen derselben Groesse driften (MF-1177, MF-1015).
 *
 * Deshalb wird hier nicht die Projektion gegen erwartete Konstanten
 * geprueft, sondern gegen den BERICHT. Laeuft einer von beiden weg,
 * faellt der Test — und das ist der Zweck. */
static void t9_projektion(void) {
    printf("t9: Projektion gegen Bericht\n");

    uft_source_facts_t f;

    /* Ohne Modell ist ALLES ungemessen — ausdruecklich, nicht durch
     * Nullen, die etwas behaupten. */
    CHECK(uft_d2_facts(NULL, &f), "NULL-Modell ist kein Fehler");
    CHECK(f.ebenen == UFT_FAKT_UNGEMESSEN, "Ebenen ohne Modell nicht ungemessen");
    CHECK(f.sektoren == UFT_FAKT_UNGEMESSEN, "Sektoren ohne Modell nicht ungemessen");
    CHECK(f.sectors_total == 0u, "Sektorzahl ohne Modell nicht 0");
    CHECK(!uft_d2_facts(NULL, NULL), "fehlendes Ziel wird nicht abgewiesen");

    /* Die Vorgabe ist UNGEMESSEN, und das haengt daran, dass sie 0 ist.
     * Wer die Aufzaehlung umsortiert, bricht jede genullte Struktur. */
    CHECK((int)UFT_FAKT_UNGEMESSEN == 0,
          "UNGEMESSEN ist nicht 0 — dann behauptet jede genullte Struktur etwas");

    uft_disk2_t *d = uft_d2_create();
    CHECK(d != NULL, "kein Modell");
    if (!d) return;

    const uft_d2_deriv_id_t dv = uft_d2_register_deriv(
        d, UFT_D2_LAYER_SECTORS, UFT_D2_ORIGIN_CONTAINER, "t9", "", 0u);
    uft_d2_track_t *tr = uft_d2_track(d, 0u, 0u);
    CHECK(tr != NULL, "keine Spur");
    if (!tr) { uft_d2_destroy(d); return; }

    /* Drei Sektoren mit drei verschiedenen CRC-Lagen — genau die
     * Dreiteilung aus MF-1272: gut, falsch, unbekannt. Ohne alle drei
     * bliebe die Zusage "drei Zahlen statt einer" unbelegt.
     *
     * Der Helfer `guter_sektor()` steht oben; ihn zu benutzen statt die
     * Felder erneut zu setzen ist derselbe Grundsatz wie in der
     * Projektion selbst: EINE Stelle, nicht zwei. */
    uft_d2_sector_t s1 = guter_sektor(0u, 1u, 8u, dv);
    CHECK(uft_d2_add_sector(d, tr, &s1), "guter Sektor abgewiesen");

    uft_d2_sector_t s2 = guter_sektor(0u, 2u, 8u, dv);
    s2.data_crc_ok = false;          /* getragen, aber falsch */
    s2.conf = UFT_D2_CONF_UNVERIFIED;    /* CERTAIN braucht einen Beleg */
    CHECK(uft_d2_add_sector(d, tr, &s2), "falscher Sektor abgewiesen");

    uft_d2_sector_t s3 = guter_sektor(0u, 3u, 8u, dv);
    s3.data_crc_known = false;       /* gar keine Aussage */
    s3.conf = UFT_D2_CONF_UNVERIFIED;
    CHECK(uft_d2_add_sector(d, tr, &s3), "Sektor ohne CRC-Angabe abgewiesen");

    CHECK(uft_d2_facts(d, &f), "Projektion scheitert");
    CHECK(f.sektoren == UFT_FAKT_GEMESSEN, "Sektoren nicht als gemessen gemeldet");
    CHECK(f.sectors_total == 3u, "Sektorzahl %zu statt 3", f.sectors_total);
    CHECK(f.crc_checked == 2u, "mit CRC-Angabe %zu statt 2", f.crc_checked);
    CHECK(f.crc_bad == 1u, "davon falsch %zu statt 1", f.crc_bad);
    CHECK(f.crc_unknown == 1u, "ohne CRC-Angabe %zu statt 1", f.crc_unknown);

    /* Die beiden Bitmasken-Gruppen am ECHTEN Modell, nicht nur am NULL-Fall.
     * Die Luecke hat die eigene Mutationsmatrix gefunden: eine Mutation, die
     * `ebenen` auf UNGEMESSEN setzte, blieb gruen — geprueft war bis dahin
     * nur, dass sie OHNE Modell ungemessen bleibt. */
    CHECK(f.ebenen == UFT_FAKT_GEMESSEN, "Ebenen nicht als gemessen gemeldet");
    /* Die Maske ist `1u << UFT_D2_LAYER_*`, nicht der Aufzaehlungswert
     * selbst: SECTORS ist 2, das gesetzte Bit ist 4. Die erste Fassung
     * dieser Zusage nagelte die Zahl fest, ohne zu lesen, wie sie
     * gebaut wird. */
    CHECK((f.layers & (1u << UFT_D2_LAYER_SECTORS)) != 0u,
          "Sektorebene fehlt in der Ebenenmaske (0x%X)", (unsigned)f.layers);
    CHECK(f.merkmale == UFT_FAKT_GEMESSEN, "Merkmale nicht als gemessen gemeldet");
    CHECK(f.umdrehungen == UFT_FAKT_GEMESSEN,
          "Umdrehungen nicht als gemessen gemeldet");
    CHECK(f.revs_total == 0u, "Umdrehungen %zu statt 0", f.revs_total);

    /* Der eigentliche Punkt: dieselben Zahlen stehen im Bericht. Steht
     * dort etwas anderes, sind die beiden Rechnungen auseinandergelaufen. */
    char text[4096];
    (void)uft_d2_report(d, text, sizeof(text));
    char erwartet[256];
    snprintf(erwartet, sizeof(erwartet),
             "Sektoren: %zu \xE2\x80\x94 mit CRC-Angabe: %zu (davon falsch: %zu), "
             "ohne CRC-Angabe: %zu",
             f.sectors_total, f.crc_checked, f.crc_bad, f.crc_unknown);
    CHECK(strstr(text, erwartet) != NULL,
          "Bericht und Projektion sind auseinandergelaufen — gesucht: %s",
          erwartet);

    /* PLL ist die EINZIGE Gruppe, die unbedingt eine Luecke ist: das
     * Modell hat kein Feld dafuer. Ein UNGEMESSEN waere hier zu schwach —
     * ein zweiter Anlauf liefert nichts anderes. */
    CHECK(f.pll == UFT_FAKT_KEIN_ERZEUGER, "PLL nicht als Luecke benannt");

    /* Fluss FOLGT dem Modell. Dieses hier traegt keine Umdrehungen, also
     * Luecke — und die Gegenprobe steht gleich darunter, denn eine Zusage,
     * die nur eine Richtung prueft, ist eine halbe Zusage (MF-1037). */
    CHECK(f.fluss == UFT_FAKT_KEIN_ERZEUGER,
          "ohne Umdrehungen ist Fluss keine Luecke");

    /* Weak ist MESSBAR — die per-Sektor-Angabe traegt die Bruecke, nur die
     * per-Bit-Maske nicht. Ohne markierte Sektoren ist das eine gemessene
     * Null, kein Schweigen. */
    CHECK(f.weak == UFT_FAKT_GEMESSEN, "Weak nicht als gemessen gemeldet");
    CHECK(f.weak_sektoren == 0u, "Weak-Sektoren %zu statt 0", f.weak_sektoren);
    CHECK(f.weak_bits_min == 0u, "Weak-Bits %zu statt 0", f.weak_bits_min);

    /* Gegenprobe Weak: ein Sektor mit Flackern muss beide Zahlen bewegen,
     * und `weak_bits_min` summiert Weak UND Fuzzy. */
    uft_d2_sector_t s4 = guter_sektor(0u, 4u, 8u, dv);
    s4.weak_bits = 3u; s4.fuzzy_bits = 2u;
    s4.conf = UFT_D2_CONF_UNVERIFIED;
    CHECK(uft_d2_add_sector(d, tr, &s4), "flackernder Sektor abgewiesen");
    CHECK(uft_d2_facts(d, &f), "Projektion scheitert nach dem vierten Sektor");
    CHECK(f.weak_sektoren == 1u, "Weak-Sektoren %zu statt 1", f.weak_sektoren);
    CHECK(f.weak_bits_min == 5u, "Weak-Bits %zu statt 5 (3+2)", f.weak_bits_min);

    /* Und einer mit NUR Fuzzy-Bits. Ohne ihn war die Zusage blind gegen
     * ein `&&` statt `||` — gemessen: die Mutation blieb gruen, weil der
     * Zeuge oben beide Sorten traegt. Eine Oder-Bedingung braucht einen
     * Zeugen je Seite. */
    uft_d2_sector_t s5 = guter_sektor(0u, 5u, 8u, dv);
    s5.fuzzy_bits = 7u;
    s5.conf = UFT_D2_CONF_UNVERIFIED;
    CHECK(uft_d2_add_sector(d, tr, &s5), "nur-Fuzzy-Sektor abgewiesen");
    CHECK(uft_d2_facts(d, &f), "Projektion scheitert nach dem fuenften Sektor");
    CHECK(f.weak_sektoren == 2u, "Weak-Sektoren %zu statt 2", f.weak_sektoren);
    CHECK(f.weak_bits_min == 12u, "Weak-Bits %zu statt 12 (3+2+7)",
          f.weak_bits_min);

    /* Und einer OHNE CRC-Angabe, der trotzdem flackert. Die Zaehlung laeuft
     * in derselben Schleife wie die CRC-Klassen, und die springen mit
     * `continue` weiter — steht die Weak-Zeile hinter dem Sprung, faellt
     * genau dieser Sektor unter den Tisch. Die beiden Eigenschaften sind
     * unabhaengig: „niemand hat die Pruefsumme geprueft" sagt nichts
     * darueber, ob die Bits flackern. */
    uft_d2_sector_t s6 = guter_sektor(0u, 6u, 8u, dv);
    s6.data_crc_known = false;
    s6.weak_bits = 4u;
    s6.conf = UFT_D2_CONF_UNVERIFIED;
    CHECK(uft_d2_add_sector(d, tr, &s6), "Sektor ohne CRC mit Flackern abgewiesen");
    CHECK(uft_d2_facts(d, &f), "Projektion scheitert nach dem sechsten Sektor");
    CHECK(f.weak_sektoren == 3u,
          "Weak-Sektoren %zu statt 3 — ein Sektor ohne CRC-Angabe faellt "
          "aus der Weak-Zaehlung", f.weak_sektoren);
    CHECK(f.weak_bits_min == 16u, "Weak-Bits %zu statt 16 (3+2+7+4)",
          f.weak_bits_min);
    CHECK(f.crc_unknown == 2u, "ohne CRC-Angabe %zu statt 2", f.crc_unknown);

    /* Gegenprobe Fluss: sobald das Modell eine Umdrehung traegt, ist Fluss
     * GEMESSEN. Ohne diese Haelfte haette die Struktur `revs_total > 0`
     * neben „kein Erzeuger fuer Fluss" tragen koennen — ein Widerspruch
     * in sich selbst. */
    const uint32_t ticks[4] = { 2000u, 2000u, 2000u, 2000u };
    CHECK(uft_d2_add_revolution(d, tr, ticks, 4u, 200000u, true,
                                UFT_D2_CONF_UNVERIFIED, dv),
          "Umdrehung abgewiesen");
    CHECK(uft_d2_facts(d, &f), "Projektion scheitert nach der Umdrehung");
    CHECK(f.revs_total == 1u, "Umdrehungen %zu statt 1", f.revs_total);
    CHECK(f.fluss == UFT_FAKT_GEMESSEN,
          "mit einer Umdrehung im Modell ist Fluss immer noch eine Luecke");

    /* Die Erkennung kommt NICHT aus dem Modell — ohne Scheibe bleibt sie
     * ungemessen, und die Projektion darf sie nicht erfinden. */
    CHECK(f.erkennung == UFT_FAKT_UNGEMESSEN,
          "die Projektion hat eine Erkennung erfunden");
    CHECK(uft_facts_erkennung(NULL, &f), "NULL-Scheibe ist kein Fehler");
    CHECK(f.erkennung == UFT_FAKT_UNGEMESSEN,
          "NULL-Scheibe hat eine Erkennung gesetzt");
    CHECK(!uft_facts_erkennung(NULL, NULL), "fehlendes Ziel wird nicht abgewiesen");

    /* Jeder Zustand hat einen Namen, ein unbekannter keinen — sonst
     * koennte ein Bericht einen Wert benennen, den es nicht gibt. */
    CHECK(uft_fakt_zustand_name(UFT_FAKT_UNGEMESSEN) != NULL, "UNGEMESSEN ohne Namen");
    CHECK(uft_fakt_zustand_name(UFT_FAKT_GEMESSEN) != NULL, "GEMESSEN ohne Namen");
    CHECK(uft_fakt_zustand_name(UFT_FAKT_KEIN_ERZEUGER) != NULL, "KEIN_ERZEUGER ohne Namen");
    CHECK(uft_fakt_zustand_name((uft_fakt_zustand_t)99) == NULL,
          "ein unbekannter Zustand hat einen Namen bekommen");

    printf("    Projektion, Dreiteilung, Gleichlauf mit dem Bericht: halten\n");
    uft_d2_destroy(d);
}

int main(void) {
    printf("=== test_disk2 (zweite Fassung, MF-1274) ===\n\n");
    t1_generationen();
    t2_register();
    t3_stimmen();
    t4_mehrere_fs();
    t5_gerechnet();
    t6_zuversicht();
    t7_bericht();
    t8_alte_garantien();
    t9_projektion();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN", g_fail);
    return g_fail ? 1 : 0;
}
