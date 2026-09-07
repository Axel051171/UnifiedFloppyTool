/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_rev_grenzen.c
 * @brief Umdrehungs-DAUERN sind keine Abtast-INDIZES (MF-951)
 *
 * ── Der Fehler, um den es geht ───────────────────────────────────────
 *
 * Ein Bericht (57. Durchgang) schlug fuer die HAL-Erweiterung vor:
 *
 *     rev_offsets[i] = fd->index_times[i];   // Sample-Index je Umdrehung
 *
 * Gemessen an `uft_gw_decode_flux_index_times()`
 * (`src/hal/uft_greaseweazle_full.c:1350`): `index_times[k]` ist die
 * **Tick-Dauer** der Umdrehung k. Ein Abtast-Index ist es nicht, und die
 * beiden haben nicht einmal dieselbe Groessenordnung — bei 72 MHz und
 * 200 ms Umdrehung stehen dort rund 14 400 000, waehrend der Strom
 * vielleicht 50 000 Abtastungen hat.
 *
 * Wer das eine fuer das andere haelt, schneidet die Umdrehungen an
 * willkuerlichen Stellen. Der Vergleich danach misst dann wieder den
 * Schnitt statt des Mediums — dieselbe Klasse wie MF-950, wo ein
 * Versatz von EINEM Bit 99,71 % einer gesunden Spur als schwach
 * meldete.
 *
 * ── Und der Baum wirft die Grenzen an drei Stellen weg ───────────────
 *
 * Gemessen MF-951:
 *
 *   `greaseweazle_backend.c:249`  kopiert nur `fd->samples`, dann
 *       `uft_gw_flux_free(fd)` — `index_times` sterben dort
 *   `uft_hal_unified.c` KryoFlux  holt sie und gibt sie sofort frei:
 *       „Free index array (not used in HAL interface currently)"
 *   `uft_hal_unified.c` SCP       liest je Umdrehung `rev_hdr[8]` =
 *       [index_time(4), data_len(4)] und benutzt nur die Laenge
 *
 * Der Bericht nannte nur Greaseweazle. Es sind alle drei.
 */
#include "uft/flux/uft_rev_grenzen.h"
#include "uft/hal/uft_greaseweazle_full.h"
#include "flux_gen/greaseweazle/flux_gen.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

TEST(drei_gleich_lange_umdrehungen_werden_getrennt)
{
    /* 30 Intervalle zu je 100 Ticks, drei Umdrehungen zu je 1000 Ticks.
     * Jede Umdrehung traegt also genau zehn Abtastungen. */
    uint32_t s[30];
    for (int i = 0; i < 30; i++) s[i] = 100u;
    const uint32_t dauern[3] = { 1000u, 1000u, 1000u };

    size_t v[8];
    size_t n = uft_rev_grenzen_aus_dauern(s, 30, dauern, 3, v, 8);

    if (n != 3 || v[0] != 0 || v[1] != 10 || v[2] != 20) {
        printf("\n      %zu Grenzen: %zu %zu %zu — erwartet 3: 0 10 20\n"
               "      ", n, n > 0 ? v[0] : 0, n > 1 ? v[1] : 0,
               n > 2 ? v[2] : 0);
        _fail++;
    }
}

TEST(die_dauer_ist_kein_index)
{
    /* DER ROTBEWEIS gegen den Vorschlag aus dem Bericht.
     *
     * Waeren `dauern` Abtast-Indizes, kaemen hier 1000/2000/3000 heraus
     * — Werte weit jenseits der 30 vorhandenen Abtastungen. Sie sind es
     * nicht: die Umrechnung muss 0/10/20 liefern. */
    uint32_t s[30];
    for (int i = 0; i < 30; i++) s[i] = 100u;
    const uint32_t dauern[3] = { 1000u, 1000u, 1000u };

    size_t v[8];
    size_t n = uft_rev_grenzen_aus_dauern(s, 30, dauern, 3, v, 8);

    for (size_t k = 0; k < n; k++) {
        if (v[k] >= 30) {
            printf("\n      Grenze %zu liegt bei %zu, es gibt nur 30 "
                   "Abtastungen\n"
                   "      -> Dauer als Index gelesen\n      ", k, v[k]);
            _fail++;
            return;
        }
    }
    ASSERT(n == 3);
}

TEST(ungleich_lange_umdrehungen)
{
    /* Drehzahlschwankung ist der Normalfall. Umdrehung 0 dauert 500,
     * Umdrehung 1 dauert 1500 Ticks. */
    uint32_t s[20];
    for (int i = 0; i < 20; i++) s[i] = 100u;
    const uint32_t dauern[2] = { 500u, 1500u };

    size_t v[4];
    size_t n = uft_rev_grenzen_aus_dauern(s, 20, dauern, 2, v, 4);

    if (n != 2 || v[0] != 0 || v[1] != 5) {
        printf("\n      %zu Grenzen: %zu %zu — erwartet 2: 0 5\n      ",
               n, n > 0 ? v[0] : 0, n > 1 ? v[1] : 0);
        _fail++;
    }

    size_t laengen[4];
    uft_rev_laengen_aus_grenzen(v, n, 20, laengen);
    if (laengen[0] != 5 || laengen[1] != 15) {
        printf("\n      Laengen %zu / %zu — erwartet 5 / 15\n      ",
               laengen[0], laengen[1]);
        _fail++;
    }
}

TEST(eine_abgebrochene_umdrehung_meldet_ihre_wahre_laenge)
{
    /* Der forensische Kern — und eine Berichtigung an mir selbst.
     *
     * Der Strom endet MITTEN in der dritten Umdrehung: 25 Abtastungen
     * zu 100 Ticks = 2500, die dritte braeuchte bis 3000.
     *
     * Die erste Fassung dieses Falls hiess „wird nicht behauptet" und
     * erwartete, dass die dritte VERSCHWIEGEN wird. Die Messung zeigte
     * das Gegenteil, und das Gegenteil ist richtig: sie BEGINNT im
     * Strom, also wird sie gemeldet — mit ihrer wahren Laenge von fuenf
     * Abtastungen statt der erwarteten zehn.
     *
     * Sie wegzulassen waere Datenverlust: fuer den Umdrehungsvergleich
     * ist ein Teilstueck brauchbar, weil ohnehin nur ueber die
     * gemeinsame Laenge verglichen wird (MF-949/950). Was NICHT
     * passieren darf, ist die volle Laenge zu melden — dann verglaeche
     * der Aufrufer gegen Nichts. */
    uint32_t s[25];
    for (int i = 0; i < 25; i++) s[i] = 100u;
    const uint32_t dauern[3] = { 1000u, 1000u, 1000u };

    size_t v[8];
    size_t n = uft_rev_grenzen_aus_dauern(s, 25, dauern, 3, v, 8);

    if (n != 3 || v[2] != 20) {
        printf("\n      %zu Grenzen — erwartet 3 (die dritte beginnt bei "
               "20 und ist unvollstaendig)\n      ", n);
        _fail++;
        return;
    }
    /* Ihre Laenge muss die WIRKLICH vorhandene sein, nicht die erwartete. */
    size_t laengen[8];
    uft_rev_laengen_aus_grenzen(v, n, 25, laengen);
    if (laengen[2] != 5) {
        printf("\n      dritte Umdrehung: %zu Abtastungen gemeldet, "
               "vorhanden sind 5\n      ", laengen[2]);
        _fail++;
    }
}

TEST(ein_strom_ohne_zweite_umdrehung_liefert_nur_eine_grenze)
{
    /* Gegenprobe: nur eine Umdrehung im Strom. Es darf keine zweite
     * Grenze erfunden werden — und der Aufrufer erkennt an der Eins,
     * dass kein Vergleich moeglich ist (MF-949 lehnt EINE Umdrehung
     * ausdruecklich ab). */
    uint32_t s[8];
    for (int i = 0; i < 8; i++) s[i] = 100u;
    const uint32_t dauern[2] = { 1000u, 1000u };   /* Geraet versprach zwei */

    size_t v[4];
    size_t n = uft_rev_grenzen_aus_dauern(s, 8, dauern, 2, v, 4);

    if (n != 1 || v[0] != 0) {
        printf("\n      %zu Grenzen bei einem Strom, der nur 800 von "
               "1000 Ticks traegt — erwartet 1\n      ", n);
        _fail++;
    }
}

TEST(eine_fuehrende_null_ist_keine_umdrehung_eine_mittlere_schon)
{
    /* Diesen Fall hat die MUTATIONSPROBE gefunden — meine Behandlung
     * der Nullen war UNGEPRUEFT.
     *
     * Der Kettentest erwartet, dass die Kette bei den (unstimmigen)
     * Generator-Dauern EINE Grenze liefert. Genau das liefert sie auch,
     * wenn die fuehrende Null gar nicht uebersprungen wird — der
     * Kettentest kann die beiden also nicht unterscheiden. Zwei
     * Mutationen blieben deshalb gruen.
     *
     * Hier stehen beide Richtungen getrennt:
     *
     *   FUEHREND: Greaseweazle setzt bei `index_sync` einen Indexpuls an
     *   den Stromanfang; `index_times[0]` ist dann 0, und die Dauer der
     *   ersten Umdrehung steht in `index_times[1]`. Ueberspringen.
     *
     *   MITTENDRIN: eine Umdrehung ohne Dauer gibt es nicht. Abbrechen —
     *   alles danach waere geraten. */
    uint32_t s[30];
    for (int i = 0; i < 30; i++) s[i] = 100u;
    size_t v[8];

    /* Fuehrende Null: 0, 1000, 1000 -> drei Grenzen bei 0, 10, 20. */
    const uint32_t fuehrend[3] = { 0u, 1000u, 1000u };
    size_t n = uft_rev_grenzen_aus_dauern(s, 30, fuehrend, 3, v, 8);
    if (n != 3 || v[0] != 0 || v[1] != 10 || v[2] != 20) {
        printf("\n      fuehrende Null: %zu Grenzen (%zu %zu %zu) — "
               "erwartet 3: 0 10 20\n      ", n,
               n > 0 ? v[0] : 0, n > 1 ? v[1] : 0, n > 2 ? v[2] : 0);
        _fail++;
        return;
    }

    /* Null MITTEN in der Liste: 0, 1000, 0, 1000 -> nach der zweiten
     * Umdrehung ist Schluss, die vierte Angabe wird nicht mehr
     * verwertet. */
    const uint32_t mittendrin[4] = { 0u, 1000u, 0u, 1000u };
    n = uft_rev_grenzen_aus_dauern(s, 30, mittendrin, 4, v, 8);
    if (n != 2 || v[0] != 0 || v[1] != 10) {
        printf("\n      Null mittendrin: %zu Grenzen (%zu %zu) — erwartet "
               "2: 0 10\n      ", n, n > 0 ? v[0] : 0, n > 1 ? v[1] : 0);
        _fail++;
    }
}

TEST(unbrauchbare_eingaben_liefern_null)
{
    uint32_t s[4] = { 100u, 100u, 100u, 100u };
    const uint32_t d[1] = { 100u };
    size_t v[4];

    ASSERT(uft_rev_grenzen_aus_dauern(NULL, 4, d, 1, v, 4) == 0);
    ASSERT(uft_rev_grenzen_aus_dauern(s, 0, d, 1, v, 4) == 0);
    ASSERT(uft_rev_grenzen_aus_dauern(s, 4, NULL, 1, v, 4) == 0);
    ASSERT(uft_rev_grenzen_aus_dauern(s, 4, d, 0, v, 4) == 0);
    ASSERT(uft_rev_grenzen_aus_dauern(s, 4, d, 1, NULL, 4) == 0);
    ASSERT(uft_rev_grenzen_aus_dauern(s, 4, d, 1, v, 0) == 0);

    /* Eine Dauer von null ist keine Umdrehung. */
    const uint32_t null_dauer[2] = { 0u, 1000u };
    ASSERT(uft_rev_grenzen_aus_dauern(s, 4, null_dauer, 2, v, 4) == 1);
}

TEST(der_ueberhang_zaehlt_zur_naechsten_umdrehung)
{
    /* Ein Wechsel faellt selten genau auf die Indexmarke. Laeuft die
     * Summe ueber das Soll hinaus, gehoert der Ueberhang zur naechsten
     * Umdrehung — sonst wandert die Grenze mit jeder Umdrehung weiter
     * nach hinten, und der Fehler waechst.
     *
     * Hier: Intervalle zu 300, Umdrehungen zu 1000. Nach 4 Abtastungen
     * sind 1200 Ticks gelaufen, 200 davon gehoeren schon zur zweiten. */
    uint32_t s[20];
    for (int i = 0; i < 20; i++) s[i] = 300u;
    const uint32_t dauern[3] = { 1000u, 1000u, 1000u };

    size_t v[8];
    size_t n = uft_rev_grenzen_aus_dauern(s, 20, dauern, 3, v, 8);
    ASSERT(n >= 3);

    /* Ohne Ueberhangsrechnung waeren die Grenzen 0, 4, 8 (jedes Mal
     * aufgerundet). Mit ihr: 0, 4, 7 — denn die zweite Umdrehung
     * beginnt mit 200 Ticks Vorsprung. */
    if (v[1] != 4 || v[2] != 7) {
        printf("\n      Grenzen %zu %zu %zu — erwartet 0 4 7 "
               "(Ueberhang beruecksichtigt)\n      ", v[0], v[1], v[2]);
        _fail++;
    }
}

/* ── Die ganze Kette, ohne Hardware ──────────────────────────────────
 *
 * Alles oben ist Arithmetik mit von Hand gesetzten Zahlen. Der Beleg,
 * der traegt, laeuft ueber einen ECHTEN Greaseweazle-Flussstrom:
 *
 *   `tests/flux_gen/greaseweazle/flux_gen.c` erzeugt einen Strom mit
 *   Index-Opcodes und meldet die erwarteten Zeiten in
 *   `rev_index_ticks[]` — das ist die zweite Hand.
 *
 *   `uft_gw_decode_flux_stream()` und `uft_gw_decode_flux_index_times()`
 *   sind der Produktionsdekoder aus `src/hal/uft_greaseweazle_full.c`.
 *
 *   `uft_rev_grenzen_aus_dauern()` ist das Neue.
 *
 * Dieses Projekt hat keine Hardware (MF-310). Der Generator ist die
 * Grenze dessen, was ohne Geraet belegbar ist; wo er vom echten Geraet
 * abweicht, steht in `tests/emulators/greaseweazle/DIVERGENCES.md`. */
TEST(die_ganze_kette_an_einem_erzeugten_gw_strom)
{
    enum { REVS = 3, PRO_REV = 4000, MAX_S = 64u * 1024u };

    uft_gw_flux_params_t p;
    memset(&p, 0, sizeof p);
    p.seed                = 0x5EEDu;
    p.revolutions         = REVS;
    p.index_period_ns     = 200000000u;    /* 200 ms = 300 U/min */
    p.transitions_per_rev = PRO_REV;
    p.sample_freq_hz      = 72000000u;     /* Greaseweazle F7 */

    uft_gw_flux_capture_t cap;
    memset(&cap, 0, sizeof cap);
    if (uft_gw_flux_gen_clean(&p, &cap) != UFT_GW_FLUX_GEN_OK) {
        printf("\n      Generator lieferte keinen Strom\n      ");
        _fail++;
        return;
    }

    static uint32_t samples[MAX_S];
    uint32_t freq = 0;
    uint32_t n = uft_gw_decode_flux_stream(cap.bytes, cap.bytes_len,
                                           samples, MAX_S, &freq);

    uint32_t dauern[8] = { 0 };
    uint32_t k = uft_gw_decode_flux_index_times(cap.bytes, cap.bytes_len,
                                                dauern, 8);

    if (n == 0 || k < REVS) {
        printf("\n      %u Abtastungen, %u Indexzeiten — erwartet >0 und "
               ">=%d\n      ", n, k, REVS);
        _fail++;
        goto ende;
    }

    /* DIE ENTSCHEIDENDE ZAHL. Waeren die Indexzeiten Abtast-Indizes,
     * laegen sie unter `n` (hier 12 000). Sie sind Tick-Dauern und
     * liegen um Groessenordnungen darueber — genau das, was der Bericht
     * verwechselt hat.
     *
     * Der ERSTE Eintrag ist dabei 0 und keine Dauer: Greaseweazle setzt
     * bei `index_sync` einen Indexpuls an den Stromanfang. Gemessen:
     *     index_times = 0, 17 867 232, 3 437 856, 3 463 488 */
    uint32_t groesste = 0;
    for (uint32_t x = 0; x < k; x++)
        if (dauern[x] > groesste) groesste = dauern[x];
    if (groesste <= n) {
        printf("\n      groesste Indexzeit %u bei %u Abtastungen — das "
               "saehe wie ein Index aus\n      ", groesste, n);
        _fail++;
        goto ende;
    }

    /* Der Strom traegt insgesamt weniger Ticks, als der Dekoder fuer
     * EINE Umdrehung meldet. Das ist die Unstimmigkeit oben, in Zahlen:
     * ~10,4 Mio geflossen gegen 17,9 Mio behauptet. */
    uint64_t gesamt = 0;
    for (uint32_t x = 0; x < n; x++) gesamt += samples[x];
    if (gesamt >= groesste) {
        printf("\n      Strom traegt %llu Ticks, groesste Indexzeit %u — "
               "die Unstimmigkeit waere behoben\n"
               "      -> dieser Fall ist ueberholt und gehoert erweitert\n"
               "      ", (unsigned long long)gesamt, groesste);
        _fail++;
        goto ende;
    }

    /* UND JETZT DER PUNKT: die Umrechnung erfindet daraufhin KEINE
     * Grenzen. Sie meldet die eine, die sie belegen kann — den Anfang —
     * und hoert auf.
     *
     * Ein Aufrufer sieht an der Eins, dass kein Umdrehungsvergleich
     * moeglich ist; `uft_fuse_revolutions()` lehnt eine einzelne
     * Umdrehung ausdruecklich ab (MF-949). Die Kette bricht also an der
     * richtigen Stelle, statt drei Umdrehungen an willkuerlichen
     * Schnitten zu behaupten. */
    size_t v[8];
    size_t g = uft_rev_grenzen_aus_dauern(samples, n, dauern, k, v, 8);

    if (g != 1 || v[0] != 0) {
        printf("\n      %zu Grenzen aus Dauern, die der Strom nicht "
               "traegt — erwartet 1 (nur der Anfang)\n      ", g);
        _fail++;
        goto ende;
    }

    /* KEIN Abgleich gegen `cap.rev_index_ticks[]` — und das ist ein
     * Befund, kein Versaeumnis.
     *
     * Gemessen (MF-951): der Generator meldet 14 400 000 Ticks je
     * Umdrehung und legt genau diesen Wert in die N28-Nutzlast des
     * Index-Opcodes. Der Produktionsdekoder setzt dagegen die
     * greaseweazle-Formel um — dort ist die Nutzlast nur der REST vom
     * letzten Wechsel bis zum Indexpuls, der zur aufgelaufenen
     * Flusszeit ADDIERT wird:
     *
     *     index.append(ticks_since_index + ticks + val)
     *
     * Deshalb liest der Dekoder 17 867 232 = 3 467 232 (wirklich
     * geflossen) + 14 400 000 (Nutzlast). Die beiden meinen
     * Verschiedenes.
     *
     * Ein Abgleich hier wuerde also diese Unstimmigkeit messen, nicht
     * die Umrechnung, um die es geht. Sie steht als eigener Punkt in
     * `docs/OPEN_ITEMS.md`; die Divergenzliste des Emulators fuehrt sie
     * noch nicht — D-8 betrifft den Jitter, nicht die Bedeutung der
     * Nutzlast. */
    (void)cap.rev_index_ticks;

ende:
    uft_gw_flux_gen_free(&cap);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Umdrehungsgrenzen aus Dauern (MF-951) ===\n");
    RUN(drei_gleich_lange_umdrehungen_werden_getrennt);
    RUN(die_dauer_ist_kein_index);
    RUN(ungleich_lange_umdrehungen);
    RUN(eine_abgebrochene_umdrehung_meldet_ihre_wahre_laenge);
    RUN(ein_strom_ohne_zweite_umdrehung_liefert_nur_eine_grenze);
    RUN(eine_fuehrende_null_ist_keine_umdrehung_eine_mittlere_schon);
    RUN(unbrauchbare_eingaben_liefern_null);
    RUN(der_ueberhang_zaehlt_zur_naechsten_umdrehung);
    RUN(die_ganze_kette_an_einem_erzeugten_gw_strom);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
