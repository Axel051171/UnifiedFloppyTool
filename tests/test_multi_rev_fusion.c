/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_multi_rev_fusion.c
 * @brief Der Umdrehungsvergleich, den niemand ruft (MF-949)
 *
 * ── Wie dieser Test entstand ─────────────────────────────────────────
 *
 * Ein Bericht (57. Durchgang) schlug vor, eine
 * Cross-Revolution-Weak-Bit-Erkennung neu zu BAUEN: dieselbe
 * Bitposition ueber mehrere Umdrehungen vergleichen, statt eine
 * Einzelmessung gegen ein Zeitfenster zu halten. Begruendet mit dem
 * Cyclone/Deep-Nibble-Prinzip: *eine* Lesung kann ein schwaches Bit
 * nicht von einer ungewoehnlichen, aber stabilen Kodierung
 * unterscheiden.
 *
 * Das Prinzip stimmt. Der Vorschlag war trotzdem falsch adressiert:
 * **der Baum hat den Vergleich bereits** —
 * `src/algorithms/advanced/uft_multi_rev_fusion.c`, 272 Zeilen, mit
 * Stimmenzaehlung je Bitposition und `weak_bit`-Kennzeichen.
 *
 * Gemessen: alle **sechs** exportierten Funktionen haben ausserhalb
 * ihrer eigenen Datei **null** Nennungen — keinen Aufrufer und nicht
 * einmal einen Prototyp. Die Datei steht in `UnifiedFloppyTool.pro:791`
 * und wird uebersetzt. Dieselbe Klasse wie die DeepRead-Module
 * (MF-627/767), `d64_write_track_gcr()` (P3-116) und
 * `uft_flx_mfi_open()` (P3-117).
 *
 * ── Warum dieser Test VOR jeder Verdrahtung kommt ────────────────────
 *
 * Ungerufener Code ist ungepruefter Code. Ihn zu verdrahten, ohne ihn
 * zu messen, hiesse eine Zusage einzubauen statt einer Faehigkeit —
 * genau das Muster, das dieser Baum wiederholt gefunden hat.
 *
 * ── Was gemessen wird ────────────────────────────────────────────────
 *
 * 1. findet er widerspruechliche Bits ueberhaupt?
 * 2. meldet er bei identischen Umdrehungen nichts?  (Gegenprobe)
 * 3. was tut er bei EINER Umdrehung?                (der Kernfall)
 * 4. was tut er bei UNTERSCHIEDLICH LANGEN Umdrehungen?
 * 5. faellt ein Widerspruch von 1 zu 4 durch die Schwelle?
 *
 * Fall 3 ist der forensisch entscheidende. Eine einzelne Lesung KANN
 * die Frage nicht beantworten — das ist der ganze Punkt des
 * Cyclone-Prinzips. Wer darauf „keine schwachen Bits" antwortet,
 * erfindet eine Aussage. Der Baum kann es besser: `uft_bitstream_
 * recovery.c:136` sagt in derselben Lage ausdruecklich ab
 * („ambiguous — refuse").
 *
 * Fall 4 ist die Speicherfrage. Umdrehungen sind in der Wirklichkeit
 * verschieden lang — Drehzahlschwankung ist der Normalfall, nicht die
 * Ausnahme. Die Schnittstelle nimmt EINE Laenge fuer alle.
 */
#include "uft/algorithms/uft_multi_rev_fusion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define BITS   64u
#define BYTES  (BITS / 8u)

/** Setzt Bit @p pos in @p buf (hoechstwertiges Bit zuerst je Byte). */
static void bit_setzen(uint8_t *buf, size_t pos, int wert)
{
    if (wert) buf[pos / 8] |=  (uint8_t)(1u << (7 - (pos % 8)));
    else      buf[pos / 8] &= (uint8_t)~(1u << (7 - (pos % 8)));
}

TEST(widerspruechliche_umdrehungen_werden_gefunden)
{
    /* Drei Umdrehungen, gleicher Inhalt bis auf Bit 17 und Bit 40:
     * dort widerspricht genau eine Umdrehung den anderen beiden. */
    uint8_t r0[BYTES], r1[BYTES], r2[BYTES];
    memset(r0, 0xA5, BYTES); memcpy(r1, r0, BYTES); memcpy(r2, r0, BYTES);
    bit_setzen(r1, 17, !((r0[17 / 8] >> (7 - 17 % 8)) & 1));
    bit_setzen(r2, 40, !((r0[40 / 8] >> (7 - 40 % 8)) & 1));

    const uint8_t *revs[3] = { r0, r1, r2 };
    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 3, BITS, NULL, &f) == true);

    size_t pos[8];
    size_t n = uft_get_weak_bit_positions(&f, pos, 8);

    if (n != 2 || pos[0] != 17 || pos[1] != 40) {
        printf("\n      %zu schwache Bits gemeldet", n);
        for (size_t i = 0; i < n && i < 8; i++) printf(" @%zu", pos[i]);
        printf(", erwartet 2 @17 @40\n      ");
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

TEST(identische_umdrehungen_melden_nichts)
{
    /* Gegenprobe. Ohne sie wuerde ein Detektor, der IMMER meldet, den
     * Fall darueber ebenfalls bestehen. */
    uint8_t r0[BYTES];
    memset(r0, 0x5C, BYTES);
    const uint8_t *revs[4] = { r0, r0, r0, r0 };

    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 4, BITS, NULL, &f) == true);

    if (f.weak_bit_count != 0) {
        printf("\n      %zu schwache Bits bei vier identischen "
               "Umdrehungen\n      ", f.weak_bit_count);
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

TEST(eine_einzige_umdrehung_wird_abgelehnt)
{
    /* DER ROTBEWEIS, und der forensisch wichtigste Fall.
     *
     * Eine einzelne Lesung KANN nicht sagen, ob ein Bit schwach ist —
     * das ist der ganze Inhalt des Cyclone-Prinzips. Vor MF-949 gab die
     * Fusion hier `true` zurueck, `confidence = 1.0` fuer jedes Bit und
     * `weak_bit_count = 0`: die Antwort „keine schwachen Bits" auf eine
     * Frage, die niemand beantworten konnte.
     *
     * Das ist Prinzip 1 („Keine erfundenen Daten"). Der Baum kann es
     * besser — `uft_bitstream_recovery.c:136` sagt in derselben Lage
     * ausdruecklich ab. */
    uint8_t r0[BYTES];
    memset(r0, 0x33, BYTES);
    const uint8_t *revs[1] = { r0 };

    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    bool ok = uft_fuse_revolutions(revs, 1, BITS, NULL, &f);

    if (ok) {
        printf("\n      EINE Umdrehung wurde beantwortet: %zu schwache "
               "Bits, Konfidenz %.2f\n"
               "      -> eine Aussage, die aus einer Lesung nicht "
               "ableitbar ist\n      ",
               f.weak_bit_count, (double)f.overall_confidence);
        uft_fused_bitstream_free(&f);
        _fail++;
    }
}

TEST(unterschiedlich_lange_umdrehungen_lesen_nicht_ueber_den_rand)
{
    /* Umdrehungen sind in der Wirklichkeit verschieden lang —
     * Drehzahlschwankung ist der Normalfall. Die alte Schnittstelle nahm
     * EINE Laenge fuer alle und las bei der kuerzeren ueber den Rand.
     *
     * Ohne ASan faellt das nicht zwingend auf; deshalb wird hier die
     * Absage GEPRUEFT, nicht der Absturz abgewartet. */
    uint8_t lang[BYTES];
    uint8_t kurz[BYTES / 2];
    memset(lang, 0x0F, sizeof lang);
    memset(kurz, 0x0F, sizeof kurz);

    const uint8_t *revs[2]   = { lang, kurz };
    const size_t   laengen[2] = { BITS, BITS / 2 };

    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);

    /* Die laengenbewusste Fassung darf nur so weit vergleichen, wie ALLE
     * Umdrehungen reichen — und muss sagen, wie weit das war. */
    bool ok = uft_fuse_revolutions_laengen(revs, laengen, 2, NULL, &f);
    if (!ok) {
        printf("\n      laengenbewusste Fassung lehnte ab\n      ");
        _fail++;
        return;
    }
    if (f.bit_count != BITS / 2) {
        printf("\n      verglichen wurden %zu Bits, gemeinsam sind nur "
               "%u\n      ", f.bit_count, BITS / 2);
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

TEST(ein_widerspruch_von_eins_zu_vier_geht_nicht_verloren)
{
    /* Die Schwelle `weak_threshold = 0.8` vergleicht mit `<`. Bei fuenf
     * Umdrehungen ergibt 4:1 die Konfidenz 0.8 — und `0.8 < 0.8` ist
     * falsch. Ein Bit, das in EINER von fuenf Lesungen widersprach,
     * wurde damit NICHT gemeldet.
     *
     * Genau dieser Fall ist der, den das Cyclone-Prinzip meint: der
     * seltene Widerspruch ist das Signal, nicht das Rauschen.
     *
     * Die Schwelle bleibt — sie beantwortet eine andere, ebenfalls
     * nuetzliche Frage („wie sicher ist der Mehrheitswert"). Daneben
     * steht jetzt die Tatsachenfrage: haben sich die Umdrehungen
     * ueberhaupt widersprochen? Die Zahlen dafuer lagen bereits in
     * `vote_ones`/`vote_zeros`; sie wurden nur nicht ausgewertet. */
    uint8_t r[5][BYTES];
    for (int i = 0; i < 5; i++) memset(r[i], 0x00, BYTES);
    bit_setzen(r[3], 9, 1);          /* eine von fuenf widerspricht */

    const uint8_t *revs[5] = { r[0], r[1], r[2], r[3], r[4] };
    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 5, BITS, NULL, &f) == true);

    ASSERT(f.bits != NULL);
    if (!f.bits[9].revolutions_disagreed) {
        printf("\n      Bit 9: %u Einsen von 5 — als einig gemeldet\n"
               "      -> der seltene Widerspruch ist genau das Signal\n"
               "      ", f.bits[9].vote_ones);
        _fail++;
    }
    /* Und die Gegenprobe an einer einigen Stelle. */
    if (f.bits[10].revolutions_disagreed) {
        printf("\n      Bit 10 als uneinig gemeldet, obwohl alle 5 "
               "gleich lasen\n      ");
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

TEST(der_mehrheitswert_ist_der_haeufigste)
{
    /* Diese Probe fehlte in der ersten Fassung, und die Mutationsprobe
     * hat es gezeigt: den Mehrheitswert umzudrehen
     * (`(einsen > nullen) ? 0 : 1`) blieb GRUEN. Geprueft wurden nur die
     * Befunde — nicht das Ergebnis.
     *
     * `uft_fused_to_bytes()` liefert aber genau das, was ein Dekoder
     * danach liest. Ein umgedrehter Mehrheitswert waere die stillste
     * denkbare Datenverfaelschung. */
    uint8_t r0[BYTES], r1[BYTES], r2[BYTES];
    memset(r0, 0xFF, BYTES);
    memset(r1, 0xFF, BYTES);
    memset(r2, 0x00, BYTES);          /* eine Umdrehung widerspricht ueberall */

    const uint8_t *revs[3] = { r0, r1, r2 };
    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 3, BITS, NULL, &f) == true);

    /* 2 von 3 lasen 1 -> Mehrheit ist 1, Konfidenz 2/3. */
    for (size_t i = 0; i < f.bit_count; i++) {
        if (f.bits[i].value != 1u) {
            printf("\n      Bit %zu: Mehrheitswert %u bei 2:1 fuer 1\n      ",
                   i, f.bits[i].value);
            _fail++;
            goto ende;
        }
    }

    /* Und dasselbe noch einmal ueber die Byte-Ausgabe, denn das ist der
     * Weg, den echte Nutzdaten nehmen. */
    {
        uint8_t aus[BYTES];
        memset(aus, 0, sizeof aus);
        size_t n = uft_fused_to_bytes(&f, aus, sizeof aus);
        if (n != BYTES) {
            printf("\n      %zu Bytes statt %u\n      ", n, BYTES);
            _fail++;
            goto ende;
        }
        for (size_t i = 0; i < BYTES; i++) {
            if (aus[i] != 0xFF) {
                printf("\n      Byte %zu: %02X statt FF\n      ", i, aus[i]);
                _fail++;
                goto ende;
            }
        }
    }

    /* Gegenprobe in die andere Richtung — sonst bestuende der Fall auch
     * bei einem Erzeuger, der immer 1 liefert. */
    {
        uft_fused_bitstream_t g;
        memset(&g, 0, sizeof g);
        const uint8_t *umgekehrt[3] = { r2, r2, r0 };
        ASSERT(uft_fuse_revolutions(umgekehrt, 3, BITS, NULL, &g) == true);
        for (size_t i = 0; i < g.bit_count; i++) {
            if (g.bits[i].value != 0u) {
                printf("\n      Gegenprobe Bit %zu: %u bei 2:1 fuer 0\n"
                       "      ", i, g.bits[i].value);
                _fail++;
                break;
            }
        }
        uft_fused_bitstream_free(&g);
    }

ende:
    uft_fused_bitstream_free(&f);
}

TEST(zusammenhaengende_bereiche_werden_zusammengefasst)
{
    /* `find_weak_regions()` gehoert zu den sechs ungerufenen Funktionen.
     * Wer sie verdrahtet, sollte wissen, dass sie rechnet. */
    uint8_t r0[BYTES], r1[BYTES];
    memset(r0, 0x00, BYTES);
    memset(r1, 0x00, BYTES);
    for (size_t p = 20; p < 25; p++) bit_setzen(r1, p, 1);

    const uint8_t *revs[2] = { r0, r1 };
    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 2, BITS, NULL, &f) == true);

    uft_weak_region_t reg[4];
    size_t n = uft_find_weak_regions(&f, reg, 4);
    if (n != 1 || reg[0].start_bit != 20 || reg[0].length != 5) {
        printf("\n      %zu Bereiche, erster @%zu Laenge %zu — erwartet "
               "1 @20 Laenge 5\n      ", n,
               n ? reg[0].start_bit : 0, n ? reg[0].length : 0);
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

TEST(die_gewichtung_aus_qualitaetsmerkmalen_rechnet)
{
    /* Diesen Fall hat ein TOR gefunden, nicht ich.
     *
     * `scripts/audit_dead_fields.py` zaehlt Felder in oeffentlichen
     * Headern, die im ganzen Baum nirgends GESCHRIEBEN werden. Mein
     * Header hob die Zahl von 1167 auf 1169 — gemessen, indem er einmal
     * beiseitegelegt wurde. Die beiden waren `pll_confidence` und
     * `crc_rate` aus `uft_revolution_quality_t`.
     *
     * Der Grund: ich hatte sechs der sieben exportierten Funktionen
     * geprueft und `uft_calculate_revolution_weights()` uebersehen. Ein
     * Eingabe-Verbund, den niemand fuellt, ist eine Zusage ohne
     * Einloesung — genau die Klasse, die dieser Baum wiederholt findet.
     *
     * Die Grundlinie anzuheben waere die falsche Antwort gewesen. */
    uft_revolution_quality_t q[3];
    memset(q, 0, sizeof q);

    /* Umdrehung 1 gut, 2 mittel, 3 schlecht. */
    q[0].pll_confidence = 1.0f; q[0].sync_quality = 1.0f; q[0].crc_rate = 1.0f;
    q[1].pll_confidence = 0.5f; q[1].sync_quality = 0.5f; q[1].crc_rate = 0.5f;
    q[2].pll_confidence = 0.0f; q[2].sync_quality = 0.0f; q[2].crc_rate = 0.0f;

    float w[3] = { -1.0f, -1.0f, -1.0f };
    uft_calculate_revolution_weights(q, 3, w);

    /* Der Header sagt zu: Summe = num_revs. Das ist die Zusage, an der
     * sich die Funktion messen lassen muss. */
    float summe = w[0] + w[1] + w[2];
    if (summe < 2.99f || summe > 3.01f) {
        printf("\n      Gewichtssumme %.4f, zugesagt 3.0\n      ",
               (double)summe);
        _fail++;
    }
    /* Und die Reihenfolge: besser gelesen heisst mehr Gewicht. */
    if (!(w[0] > w[1] && w[1] > w[2])) {
        printf("\n      Gewichte %.3f / %.3f / %.3f — nicht fallend\n      ",
               (double)w[0], (double)w[1], (double)w[2]);
        _fail++;
    }

    /* Gegenprobe: ohne jede Qualitaetsaussage darf NICHT alles auf null
     * fallen. Waeren alle Gewichte 0, waere die Gewichtssumme in der
     * Fusion 0 und die Konfidenz jedes Bits undefiniert. Dann lieber
     * gleich gewichten — und das ist eine Entscheidung, die geprueft
     * gehoert, nicht eine, die man dem Zufall ueberlaesst. */
    memset(q, 0, sizeof q);
    float g[3] = { -1.0f, -1.0f, -1.0f };
    uft_calculate_revolution_weights(q, 3, g);
    if (g[0] != 1.0f || g[1] != 1.0f || g[2] != 1.0f) {
        printf("\n      ohne Qualitaetsaussage: %.2f / %.2f / %.2f, "
               "erwartet 1/1/1\n      ",
               (double)g[0], (double)g[1], (double)g[2]);
        _fail++;
    }
}

TEST(die_gewichte_wirken_auf_das_ergebnis)
{
    /* Und die Gewichte muessen ANKOMMEN. Eine Gewichtsfunktion, deren
     * Ergebnis die Fusion nicht erreicht, waere eine weitere Tuer ohne
     * Leser — im selben Modul.
     *
     * Zwei Umdrehungen widersprechen sich ueberall. Ungewichtet steht es
     * 1:1, der Mehrheitswert faellt auf 0 (weil `einsen > nullen` bei
     * Gleichstand falsch ist). Mit dreifachem Gewicht auf der
     * Einsen-Umdrehung muss er auf 1 kippen. */
    uft_fused_bitstream_t f;
    uint8_t einsen[BYTES], nullen[BYTES];
    memset(einsen, 0xFF, BYTES);
    memset(nullen, 0x00, BYTES);
    const uint8_t *revs[2] = { einsen, nullen };

    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 2, BITS, NULL, &f) == true);
    ASSERT(f.bits[0].value == 0u);          /* Gleichstand -> 0 */
    uft_fused_bitstream_free(&f);

    float w[2] = { 3.0f, 1.0f };
    uft_fusion_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.weak_threshold     = 0.8f;
    cfg.strong_threshold   = 0.95f;
    cfg.weight_by_timing   = true;
    cfg.revolution_weights = w;

    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions(revs, 2, BITS, &cfg, &f) == true);
    if (f.bits[0].value != 1u) {
        printf("\n      gewichtet 3:1 fuer die Einsen — Mehrheitswert "
               "bleibt %u\n      ", f.bits[0].value);
        _fail++;
    }
    /* Die TATSACHE bleibt davon unberuehrt: die Umdrehungen haben
     * widersprochen, egal wie man sie gewichtet. */
    if (!f.bits[0].revolutions_disagreed) {
        printf("\n      Gewichtung hat den Widerspruch verdeckt\n      ");
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Umdrehungsvergleich: rechnet er? (MF-949) ===\n");
    RUN(widerspruechliche_umdrehungen_werden_gefunden);
    RUN(identische_umdrehungen_melden_nichts);
    RUN(eine_einzige_umdrehung_wird_abgelehnt);
    RUN(unterschiedlich_lange_umdrehungen_lesen_nicht_ueber_den_rand);
    RUN(ein_widerspruch_von_eins_zu_vier_geht_nicht_verloren);
    RUN(der_mehrheitswert_ist_der_haeufigste);
    RUN(zusammenhaengende_bereiche_werden_zusammengefasst);
    RUN(die_gewichtung_aus_qualitaetsmerkmalen_rechnet);
    RUN(die_gewichte_wirken_auf_das_ergebnis);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
