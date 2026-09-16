/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_xfd_qd_serpentine.c
 * @brief XF551 Quad Density: 40 Spuren, und die Rueckseite laeuft
 *        rueckwaerts (MF-1175)
 *
 * ── Der Befund, und er steckte in ZWEI Lesern ───────────────────────────
 *
 * XFD Quad Density (360 K, 1440 Sektoren zu 256 Byte = 368 640 Byte) ist
 * eine ZWEISEITIGE 40-Spur-Diskette, und Seite 1 ist spiegelbildlich
 * beschrieben. Gemessen am Vorzustand:
 *
 *   src/formats/atari/uft_xfd_parser_v2.c:57   XFD_TRACKS_QD = 80
 *   src/formats/xfd/uft_xfd.c:83               `fs > 266240` -> return false
 *   src/formats/xfd/uft_xfd.c:127              `heads = 1` fest verdrahtet,
 *                                              und (1440+17)/18 = 80 folgt
 *
 * Der erste Leser SAGT die 80 als Konstante, der zweite RECHNET sie sich
 * aus — derselbe Fehler, zweimal unabhaengig entstanden. Klasse
 * MF-519/MF-529, und der eine Fund verriet den anderen nicht.
 *
 * ── Zwei unabhaengige Quellen, und sie stimmen ueberein ─────────────────
 *
 * Quelle A: Erwin Reuss, „Die Formate der XF551", Compy-Shop-Magazin 4/88.
 *           Vier Stuetzstellen: Sektor 1 auf Track 0, Sektor 720 auf
 *           Track 39, Sektor 721 auf Track 39, Sektor 1440 auf Track 0.
 * Quelle B: a8rawconv 0.95, `src/a8rawconv/diskxfd.cpp` im eigenen Baum
 *           (GPL-2-or-later, Orakel — ausgefuehrt und gelesen, nicht
 *           portiert). Zeilen 67-73: fuer `1440 * 256` gilt tracks = 40,
 *           sides = 2, sectors_per_track = 18. Zeilen 92-116: Seite 1
 *           beginnt bei `data += spt * sides * tracks * ss` (= 368 640)
 *           und laeuft RUECKWAERTS.
 *
 * Nachgerechnet ueber alle 1440 Sektoren: **Spur und Kopf stimmen bei
 * 1440 von 1440 ueberein**, nicht nur an den vier Stuetzstellen.
 *
 * ── Was AUSDRUECKLICH NICHT belegt ist (S1) ─────────────────────────────
 *
 * Die Sektornummer INNERHALB einer Seite-1-Spur. Das Anordnungsmodul
 * numeriert aufsteigend-logisch (Sektor 1440 -> physisch 18), a8rawconv
 * absteigend in Leserichtung (Sektor 1440 -> physisch 1). Gemessen weichen
 * **720 von 1440** ab — genau die Seite-1-Sektoren. Reuss nennt nur
 * Spuren, kein Korpusabbild entscheidet es.
 *
 * Dieser Test prueft deshalb SPUR UND KOPF und nagelt die Sektornummer auf
 * Seite 1 NICHT fest. Wer sie spaeter belegt, erweitert hier — er muss
 * nichts umschreiben.
 *
 * ── Rot-Probe (D1) ──────────────────────────────────────────────────────
 *
 * Der alte Stand rechnete `zylinder = index / 18` ohne Spiegelung. Fuer
 * Sektor 721 (Index 720) ergibt das Zylinder **40** — eine Spur, die es
 * auf einer 40-Spur-Diskette nicht gibt —, fuer Sektor 1440 Zylinder
 * **79**. Richtig sind 39 und 0. Die Probe prueft zusaetzlich, dass der
 * alte Stand nicht ZUFAELLIG richtig liegt.
 */
#include "uft/core/uft_sector_order.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_xfd;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, msg); _fail++; return; } } while (0)

/* XF551 Quad Density, aus den Quellen: 40 Zylinder, 2 Koepfe, 18 Sektoren
 * zu 256 Byte. Absichtlich als Literale und nicht aus dem Pruefling
 * geholt (Tor 64, MF-1000). */
#define QD_CYL   40u
#define QD_HEADS  2u
#define QD_SPT   18u
#define QD_SS   256u
#define QD_TOTAL (QD_CYL * QD_HEADS * QD_SPT)      /* 1440 */
#define QD_BYTES (QD_TOTAL * QD_SS)                /* 368 640 */

static const uft_order_geometry_t QD = {
    .cylinders = QD_CYL, .heads = QD_HEADS,
    .sectors = QD_SPT, .sector_size = QD_SS, .active_head = 0u
};

/** Der VORZUSTAND: 80 Zylinder, keine Spiegelung. So rechneten beide
 *  Leser — der eine ueber XFD_TRACKS_QD = 80, der andere ueber
 *  (total + spt - 1) / spt mit heads = 1. */
static uint16_t alter_stand_zylinder(uint32_t index)
{
    return (uint16_t)(index / QD_SPT);
}

/* ── Die vier Stuetzstellen von Reuss ────────────────────────────────── */

TEST(reuss_vier_stuetzstellen)
{
    /* Geprueft werden SPUR und KOPF. Die Sektornummer auf Seite 1 ist
     * unbelegt (S1, siehe Kopf) und steht hier absichtlich nicht. */
    struct { uint32_t sektor; uint8_t kopf; uint16_t spur; } s[] = {
        {    1u, 0u,  0u },   /* Reuss: Sektor 1    auf Track 0  */
        {  720u, 0u, 39u },   /* Reuss: Sektor 720  auf Track 39 */
        {  721u, 1u, 39u },   /* Reuss: Sektor 721  auf Track 39 */
        { 1440u, 1u,  0u },   /* Reuss: Sektor 1440 auf Track 0  */
    };
    for (unsigned i = 0; i < sizeof(s) / sizeof(s[0]); i++) {
        uft_chs_t chs;
        ASSERT(uft_order_index_to_chs(UFT_ORDER_SERPENTINE, &QD,
                                      s[i].sektor - 1u, &chs));
        ASSERT(chs.head == s[i].kopf);
        ASSERT(chs.cyl  == s[i].spur);
        /* Die Sektornummer muss im gueltigen Bereich liegen — mehr wird
         * ueber sie nicht behauptet. */
        ASSERT(chs.sector >= 1u && chs.sector <= QD_SPT);
    }
}

/* ── ROT-PROBE (D1): der alte Stand liefert etwas anderes ────────────── */

TEST(rot_probe_alter_stand_weicht_ab)
{
    /* Sektor 721, Index 720: alt 40, richtig 39.
     * Sektor 1440, Index 1439: alt 79, richtig 0. */
    ASSERT(alter_stand_zylinder(720u)  == 40u);
    ASSERT(alter_stand_zylinder(1439u) == 79u);

    /* Und die Gegenprobe gegen einen zufaelligen Treffer (D1): waere der
     * alte Stand hier richtig, wuerde dieser Test nichts beweisen. */
    CHECK(!(alter_stand_zylinder(720u) == 39u),
          "ROT-PROBE verfehlt: der alte Stand liefert fuer Sektor 721 "
          "ZUFAELLIG die richtige Spur — dann ist dieser Test wertlos");
    CHECK(!(alter_stand_zylinder(1439u) == 0u),
          "ROT-PROBE verfehlt: der alte Stand liefert fuer Sektor 1440 "
          "ZUFAELLIG die richtige Spur — dann ist dieser Test wertlos");

    /* Und 40 bzw. 79 sind nicht nur andere Zahlen, sie sind UNMOEGLICH:
     * eine 40-Spur-Diskette hat die Zylinder 0..39. */
    ASSERT(alter_stand_zylinder(720u)  >= QD_CYL);
    ASSERT(alter_stand_zylinder(1439u) >= QD_CYL);
}

/* ── Die Achse als Ganzes ────────────────────────────────────────────── */

TEST(serpentine_deckt_alle_1440_ab)
{
    ASSERT(uft_order_sector_count(UFT_ORDER_SERPENTINE, &QD) == QD_TOTAL);

    /* Jede Spur muss genau 18 Sektoren tragen, und jede der 80
     * Spur/Kopf-Kombinationen genau einmal vorkommen. Ohne diese Zusage
     * koennte die Abbildung an den vier Stuetzstellen stimmen und
     * dazwischen doppelt belegen. */
    unsigned char belegt[QD_CYL][QD_HEADS];
    memset(belegt, 0, sizeof(belegt));
    for (uint32_t i = 0; i < QD_TOTAL; i++) {
        uft_chs_t chs;
        ASSERT(uft_order_index_to_chs(UFT_ORDER_SERPENTINE, &QD, i, &chs));
        ASSERT(chs.cyl < QD_CYL);
        ASSERT(chs.head < QD_HEADS);
        belegt[chs.cyl][chs.head]++;
    }
    for (unsigned c = 0; c < QD_CYL; c++)
        for (unsigned h = 0; h < QD_HEADS; h++)
            ASSERT(belegt[c][h] == QD_SPT);

    /* und ein Index hinter dem Ende wird abgewiesen statt gerechnet */
    uft_chs_t chs;
    ASSERT(!uft_order_index_to_chs(UFT_ORDER_SERPENTINE, &QD, QD_TOTAL, &chs));
}

TEST(umkehrung_ist_invers)
{
    /* Die Umkehrung muss fuer alle 1440 Sektoren zurueckfuehren. Das ist
     * die Zusage, die der Header verspricht — hier fuer die Anordnung
     * geprueft, um die es geht. */
    for (uint32_t i = 0; i < QD_TOTAL; i++) {
        uft_chs_t chs;
        uint32_t zurueck = 0xFFFFFFFFu;
        ASSERT(uft_order_index_to_chs(UFT_ORDER_SERPENTINE, &QD, i, &chs));
        ASSERT(uft_order_chs_to_index(UFT_ORDER_SERPENTINE, &QD, &chs,
                                      &zurueck));
        ASSERT(zurueck == i);
    }
}

TEST(serpentine_ist_nicht_chs)
{
    /* Gegenprobe: waere SERPENTINE dasselbe wie CHS, braeuchte man die
     * Anordnung nicht. Gemessen muessen sich die beiden auf Seite 1
     * unterscheiden. */
    unsigned anders = 0u, gleich = 0u;
    for (uint32_t i = 0; i < QD_TOTAL; i++) {
        uft_chs_t a, b;
        ASSERT(uft_order_index_to_chs(UFT_ORDER_SERPENTINE, &QD, i, &a));
        ASSERT(uft_order_index_to_chs(UFT_ORDER_CHS, &QD, i, &b));
        if (a.cyl == b.cyl && a.head == b.head) gleich++; else anders++;
    }
    ASSERT(anders > 0u);
    ASSERT(gleich + anders == QD_TOTAL);
}

/* ── S1: was hier NICHT geprueft wird, und warum ─────────────────────── */

TEST(seite_1_sektornummer_bleibt_unbelegt)
{
    /* Diese Zusage ist absichtlich SCHWACH, und das ist der Punkt: die
     * Sektornummer innerhalb einer Seite-1-Spur ist nicht belegt. Das
     * Modul numeriert aufsteigend-logisch, a8rawconv absteigend in
     * Leserichtung; gemessen weichen 720 von 1440 ab, und Reuss nennt nur
     * Spuren. Geprueft wird deshalb nur, dass die Nummer im gueltigen
     * Bereich liegt und dass jede Spur jede Nummer genau einmal traegt —
     * das ist eine Aussage ueber Konsistenz, nicht ueber das Medium. */
    static unsigned char gesehen[QD_CYL][QD_HEADS][QD_SPT + 1u];
    memset(gesehen, 0, sizeof(gesehen));
    for (uint32_t i = 0; i < QD_TOTAL; i++) {
        uft_chs_t chs;
        ASSERT(uft_order_index_to_chs(UFT_ORDER_SERPENTINE, &QD, i, &chs));
        ASSERT(chs.sector >= 1u && chs.sector <= QD_SPT);
        gesehen[chs.cyl][chs.head][chs.sector]++;
    }
    for (unsigned c = 0; c < QD_CYL; c++)
        for (unsigned h = 0; h < QD_HEADS; h++)
            for (unsigned s = 1u; s <= QD_SPT; s++)
                ASSERT(gesehen[c][h][s] == 1u);
}

/* ── Das PLUGIN, nicht nur die Achse ─────────────────────────────────────
 *
 * Die Zusagen oben pruefen die Anordnung fuer sich. Sie sagen nichts
 * darueber, ob der registrierte Leser sie auch benutzt — und genau diese
 * Luecke ist in diesem Baum die teuerste Gestalt (MF-1039: eine Sonde, die
 * nie zustimmen KONNTE; MF-635: Koennen ohne Tuer). Also wird der Leser hier
 * am Objekt gefahren.
 *
 * Die Pruefdatei baut der Test SELBST, und das ist hier zulaessig, weil er
 * keine Stufe hebt: geprueft wird UFTs eigene Geometrierechnung gegen die
 * ZAHLEN zweier fremder Quellen, nicht ein Abbild fremder Hand. Im Korpus
 * liegt keine einzige 360-K-XFD (gemessen: `tests/corpus_free` fuehrt genau
 * `atrcopy_dos2sd.atr` und `.xfd`, beide 90 K) — solange das so ist, bleibt
 * QD ohne Fremdbeleg, und der Tiereintrag sagt das.
 *
 * Jeder Sektor BENENNT SICH SELBST mit seiner logischen Nummer. Damit sagt
 * ein Leseergebnis nicht nur, DASS etwas kam, sondern ob die richtige Stelle
 * getroffen war (Verfahren aus MF-1020).
 */

#define QD_PRUEFDATEI "uft_xfd_qd_probe.xfd"

/** Logische Sektornummer aus den Nutzbytes zurueckgewinnen. 0 = nicht
 *  erkannt — und das ist ein Fehlschlag, kein Sonderfall. */
static uint32_t qd_nummer_aus(const uint8_t *d, size_t len)
{
    unsigned n = 0u;
    if (!d || len < 16u) return 0u;
    if (memcmp(d, "UFT-QD S", 8) != 0) return 0u;
    if (sscanf((const char *)d + 8, "%4u", &n) != 1) return 0u;
    return (uint32_t)n;
}

static bool qd_baue_pruefdatei(void)
{
    FILE *f = fopen(QD_PRUEFDATEI, "wb");
    if (!f) return false;
    for (uint32_t n = 1u; n <= QD_TOTAL; n++) {
        uint8_t sek[QD_SS];
        memset(sek, 0xE5, sizeof(sek));
        /* "UFT-QD S0001" … "UFT-QD S1440" — vier Stellen, feste Breite. */
        snprintf((char *)sek, sizeof(sek), "UFT-QD S%04u", (unsigned)n);
        if (fwrite(sek, 1, sizeof(sek), f) != sizeof(sek)) {
            fclose(f); return false;
        }
    }
    return fclose(f) == 0;
}

TEST(sonde_nimmt_quad_density_an)
{
    /* ROT-PROBE: die Obergrenze der Sonde war `fs > 266240 -> return false`.
     * 368 640 ist groesser, also wurde eine gueltige Atari-Diskette
     * ABGEWIESEN — nicht falsch gelesen, sondern gar nicht erkannt. */
    CHECK(QD_BYTES > 266240u,
          "ROT-PROBE verfehlt: QD passt unter die alte Schranke 266 240 — "
          "dann war die Datei nie abgewiesen und dieser Test wertlos");

    uint8_t kopf[512];
    memset(kopf, 0, sizeof(kopf));
    int konf = -1;
    ASSERT(uft_format_plugin_xfd.probe(kopf, sizeof(kopf), QD_BYTES, &konf));
    /* 40 und nicht 82: der hoehere Wert braucht einen Atari-DOS-Bootsektor,
     * und der Nullpuffer hier hat keinen. Genauso festgenagelt wie in
     * test_corpus_xfd (MF-426). */
    ASSERT(konf == 40);
}

TEST(open_meldet_40_spuren_und_2_koepfe)
{
    /* ROT-PROBE: der alte Stand hatte `heads = 1` fest verdrahtet, und die
     * Zylinderzahl folgte aus (total + spt - 1) / spt. */
    const unsigned alt_zyl = (QD_TOTAL + QD_SPT - 1u) / QD_SPT;
    ASSERT(alt_zyl == 80u);
    CHECK(alt_zyl != QD_CYL,
          "ROT-PROBE verfehlt: die alte Rechnung liefert ZUFAELLIG 40 "
          "Zylinder — dann ist dieser Test wertlos");

    ASSERT(qd_baue_pruefdatei());

    uft_disk_t d;
    memset(&d, 0, sizeof(d));
    d.read_only = true;
    ASSERT(uft_format_plugin_xfd.open(&d, QD_PRUEFDATEI, true) == UFT_OK);

    ASSERT(d.geometry.cylinders    == QD_CYL);
    ASSERT(d.geometry.heads        == QD_HEADS);
    ASSERT(d.geometry.sectors      == QD_SPT);
    ASSERT(d.geometry.sector_size  == QD_SS);
    ASSERT(d.geometry.total_sectors == QD_TOTAL);

    uft_format_plugin_xfd.close(&d);
    remove(QD_PRUEFDATEI);
}

TEST(leser_findet_die_rueckseite_gespiegelt)
{
    /* Die Stuetzstellen von Reuss, gemessen DURCH DAS PLUGIN. Geprueft wird
     * die MENGE der logischen Nummern je Spur — die ist aus BEIDEN Quellen
     * belegt. Welche der 18 die Nummer 1 traegt, ist es nicht (S1), und
     * steht hier deshalb absichtlich nicht. */
    ASSERT(qd_baue_pruefdatei());

    uft_disk_t d;
    memset(&d, 0, sizeof(d));
    d.read_only = true;
    ASSERT(uft_format_plugin_xfd.open(&d, QD_PRUEFDATEI, true) == UFT_OK);

    struct { int cyl, head; uint32_t erste, letzte; const char *quelle; } f[] = {
        {  0, 0,    1u,   18u, "Reuss: Sektor 1 auf Track 0"            },
        { 39, 0,  703u,  720u, "Reuss: Sektor 720 auf Track 39"         },
        { 39, 1,  721u,  738u, "Reuss: Sektor 721 auf Track 39"         },
        {  0, 1, 1423u, 1440u, "Reuss: Sektor 1440 auf Track 0"         },
    };

    unsigned geprueft = 0u;
    for (unsigned i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        uft_track_t t;
        memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_xfd.read_track(&d, f[i].cyl, f[i].head, &t)
               == UFT_OK);
        ASSERT(t.sector_count == QD_SPT);

        unsigned char gesehen[QD_SPT];
        memset(gesehen, 0, sizeof(gesehen));
        for (size_t s = 0; s < t.sector_count; s++) {
            const uint32_t n = qd_nummer_aus(t.sectors[s].data,
                                             t.sectors[s].data_len);
            ASSERT(n >= f[i].erste && n <= f[i].letzte);
            gesehen[n - f[i].erste]++;
            geprueft++;
        }
        /* jede der 18 Nummern GENAU einmal — eine Spur, die 18-mal
         * denselben Sektor liefert, haette den Bereich auch getroffen */
        for (unsigned k = 0; k < QD_SPT; k++) ASSERT(gesehen[k] == 1u);

        for (size_t s = 0; s < t.sector_count; s++) free(t.sectors[s].data);
        free(t.sectors);
        free(t.raw_data);
    }
    ASSERT(geprueft == 4u * QD_SPT);   /* ein leerer Durchlauf waere wertlos */

    /* Und Kopf 2 gibt es nicht — eine Absage, keine gerechnete Lage. */
    uft_track_t leer;
    memset(&leer, 0, sizeof(leer));
    ASSERT(uft_format_plugin_xfd.read_track(&d, 0, 2, &leer) != UFT_OK);

    uft_format_plugin_xfd.close(&d);
    remove(QD_PRUEFDATEI);
}

TEST(tafel_traegt_sich_selbst)
{
    /* MF-1175: `open` weist eine Tafelzeile ab, deren Spur-, Kopf- und
     * Sektorzahl nicht auf ihre Gesamtsektorzahl fuehrt. Das ist die Zeile,
     * die in `uft_xfd_parser_v2.c` gefehlt hat — dort standen 80 Spuren,
     * 2 Koepfe, 18 Sektoren und 1440 gesamt NEBENEINANDER.
     *
     * Hier wird die Bedingung selbst gemessen, nicht der Ausgang: das
     * Widerspruchspaar 80 x 2 x 18 ergibt 2880 und nicht 1440. */
    const uft_order_geometry_t falsch = {
        .cylinders = 80u, .heads = QD_HEADS,
        .sectors = QD_SPT, .sector_size = QD_SS, .active_head = 0u
    };
    ASSERT(uft_order_sector_count(UFT_ORDER_SERPENTINE, &falsch) == 2880u);
    ASSERT(uft_order_sector_count(UFT_ORDER_SERPENTINE, &falsch) != QD_TOTAL);
    /* und die richtige Zeile geht auf */
    ASSERT(uft_order_sector_count(UFT_ORDER_SERPENTINE, &QD) == QD_TOTAL);
}

int main(void)
{
    printf("=== XFD Quad Density: 40 Spuren, Rueckseite rueckwaerts "
           "(MF-1175) ===\n");
    RUN(reuss_vier_stuetzstellen);
    printf("--- ROT-PROBE gegen den alten Stand ---\n");
    RUN(rot_probe_alter_stand_weicht_ab);
    printf("--- die Anordnung als Ganzes ---\n");
    RUN(serpentine_deckt_alle_1440_ab);
    RUN(umkehrung_ist_invers);
    RUN(serpentine_ist_nicht_chs);
    printf("--- S1: unbelegt bleibt unbelegt ---\n");
    RUN(seite_1_sektornummer_bleibt_unbelegt);
    printf("--- das registrierte Plugin am Objekt ---\n");
    RUN(sonde_nimmt_quad_density_an);
    RUN(open_meldet_40_spuren_und_2_koepfe);
    RUN(leser_findet_die_rueckseite_gespiegelt);
    RUN(tafel_traegt_sich_selbst);

    /* BERICHTIGT MF-1176: die drei Plugin-Zusagen raeumen ihre Pruefdatei
     * am Ende selbst weg — aber `ASSERT` kehrt bei Fehlschlag SOFORT
     * zurueck, und dann wird das `remove()` nie erreicht. Gemessen am
     * Rotbeweis-Lauf gegen den Vorzustand: `uft_xfd_qd_probe.xfd` blieb mit
     * 368 640 Byte im Arbeitsbaum liegen. Ein Test, der bei Fehlschlag
     * Muell hinterlaesst, ist die kleine Schwester von
     * `test_convert_leaves_no_ghost` — deshalb hier, hinter allem, noch
     * einmal. Ein `remove()` auf eine nicht vorhandene Datei ist
     * folgenlos. */
    remove(QD_PRUEFDATEI);

    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
