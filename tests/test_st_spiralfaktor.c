/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_st_spiralfaktor.c
 * @brief Stessuns Formel gegen seine eigene Messreihe (MF-858).
 *
 * Quelle: Juergen Stessun, „Wie schnell sind Disketten zu laden?",
 * ST-Computer 12/1989, abgedruckt in `LESETEST.HLP` (FCopy Pro 1.2,
 * `UTILITY/LESETEST/`).
 *
 * Alle Erwartungswerte sind GEMESSENE Werte aus den zwei Tabellen der
 * Quelle, keine berechneten. Eine Formel, die nur gegen ihre eigene
 * Rechnung geprueft wird, prueft nichts.
 */
#include "uft/formats/st/uft_st_order.h"

#include <math.h>
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

#define SEK   512u      /* Sektorgroesse ST */
#define UPM   300.0     /* Umdrehungen je Minute */

typedef struct { uint8_t spt, il; int16_t spiral; double kbs; } messwert_t;

/* Tabelle 1 der Quelle: MIT Fastload. */
static const messwert_t MIT_FASTLOAD[] = {
    { 9, 1, 0, 22.48 }, { 9, 1, 1, 20.24 }, { 9, 1, 2, 18.41 },
    { 9, 1, 3, 16.87 }, { 9, 1, 4, 15.57 },
    {10, 1, 0, 25.01 }, {10, 1, 1, 22.72 }, {10, 1, 2, 20.83 },
    {10, 1, 3, 19.23 }, {10, 1, 4, 17.85 },
    {11, 2, 0, 13.75 }, {11, 2, 1, 13.15 }, {11, 2, 2, 12.61 },
    {11, 2, 3, 12.12 }, {11, 2, 4, 11.64 },
};

/* Tabelle 2: OHNE Fastload. Der Sprung zwischen SPIR 1 und 2 ist der
 * eigentliche Gegenstand. */
static const messwert_t OHNE_FASTLOAD[] = {
    { 9, 1, 0, 11.31 }, { 9, 1, 2, 18.39 }, { 9, 1, 3, 16.87 },
    {10, 1, 0, 12.56 }, {10, 1, 1, 11.96 }, {10, 1, 2, 20.79 },
    {10, 1, 3, 19.23 },
};

TEST(die_formel_traegt_ihre_messreihe)
{
    /* DER ROTBEWEIS fuer die Formel: fuenfzehn gemessene Werte, keiner
     * berechnet. Toleranz 0,05 kB/s — die Quelle gibt zwei
     * Nachkommastellen an. */
    double schlimmster = 0.0;
    for (size_t i = 0; i < sizeof MIT_FASTLOAD / sizeof *MIT_FASTLOAD; i++) {
        const messwert_t *m = &MIT_FASTLOAD[i];
        double v = uft_st_speed(m->spt, m->il, m->spiral, SEK, UPM);
        double d = fabs(v - m->kbs);
        if (d > schlimmster) schlimmster = d;
        if (d > 0.05) {
            printf("\n      %u SpT IL%u SPIR%d: %.2f statt %.2f (Abw %.3f)\n"
                   "      ", m->spt, m->il, m->spiral, v, m->kbs, d);
            _fail++;
            return;
        }
    }
    /* Die tatsaechlich gemessene groesste Abweichung: 0,020 kB/s. Wird
     * sie schlechter, hat jemand an der Konstante gedreht. */
    if (schlimmster > 0.03) {
        printf("\n      groesste Abweichung %.3f — war 0,020\n      ",
               schlimmster);
        _fail++;
    }
}

TEST(die_konstante_ist_keine_konstante)
{
    /* Die Quelle setzt 2,5 ein und sagt, woraus sie entsteht:
     * Sektorlaenge in kB mal Umdrehungen je Sekunde. Hier wird das
     * ausgerechnet — Voraussetzung dafuer, dass die Formel auch fuer
     * andere Laufwerke gilt. */
    ASSERT(fabs(uft_st_speed_konstante(512, 300.0) - 2.5) < 1e-9);

    /* 1024-Byte-Sektoren bei 300 U/min: doppelt so viel je Sektor. */
    ASSERT(fabs(uft_st_speed_konstante(1024, 300.0) - 5.0) < 1e-9);

    /* 8-Zoll-Laufwerk, 360 U/min, 512 B: 0,5 * 6 = 3,0. */
    ASSERT(fabs(uft_st_speed_konstante(512, 360.0) - 3.0) < 1e-9);

    /* Unbrauchbare Angaben ergeben 0, keine Zahl, die wie eine
     * Rechnung aussieht. */
    ASSERT(uft_st_speed_konstante(0, 300.0) == 0.0);
    ASSERT(uft_st_speed_konstante(512, 0.0) == 0.0);
}

TEST(ohne_fastload_kostet_spiral_unter_zwei_eine_umdrehung)
{
    /* Was die Quelle NICHT ausgerechnet hat: sie druckt beide Tabellen
     * nebeneinander und erklaert den Sprung in Worten. Rechnet man die
     * Zeitdifferenz aus, wird daraus eine Regel — ab Spiralfaktor 2
     * kostet der Spurwechsel nichts, darunter eine ganze Umdrehung.
     *
     * Der Fall 9 SpT / SPIR 1 fehlt hier bewusst: er kostet gemessen
     * 0,88 statt 0,99 Umdrehungen und hat einen eigenen Fall unten. */
    for (size_t i = 0; i < sizeof OHNE_FASTLOAD / sizeof *OHNE_FASTLOAD; i++) {
        const messwert_t *m = &OHNE_FASTLOAD[i];
        double v = uft_st_speed_no_fastload(m->spt, m->il, m->spiral,
                                            SEK, UPM);
        double d = fabs(v - m->kbs);
        if (d > 0.1) {
            printf("\n      %u SpT IL%u SPIR%d: %.2f statt %.2f (Abw %.3f)\n"
                   "      ", m->spt, m->il, m->spiral, v, m->kbs, d);
            _fail++;
            return;
        }
    }
}

TEST(der_eine_ausreisser_ist_benannt_nicht_weggemittelt)
{
    /* UNRESOLVED: 9 SpT bei SPIR 1 kostet gemessen 0,88 Umdrehungen,
     * alle anderen Werte 0,99 oder 0,00. Elf Prozent Unterschied — zu
     * gross fuer Messrauschen, unerklaert.
     *
     * Das Modell rechnet mit der vollen Umdrehung und liegt hier
     * daneben. Dieser Fall haelt fest, WIE WEIT — damit die Abweichung
     * eine Zahl hat statt einer Fussnote, und damit sie auffaellt,
     * falls jemand das Modell aendert. */
    const double gemessen = 11.28;
    double v = uft_st_speed_no_fastload(9, 1, 1, SEK, UPM);

    /* Das Modell sagt rund 10,66; gemessen sind 11,28. */
    double d = fabs(v - gemessen);
    if (d < 0.1) {
        printf("\n      Modell trifft jetzt (%.2f gegen %.2f) — der "
               "Ausreisser ist erklaert?\n      ", v, gemessen);
        _fail++;          /* dann gehoert dieser Fall neu geschrieben */
        return;
    }
    if (d > 0.8) {
        printf("\n      Abweichung %.2f, erwartet rund 0,62 "
               "(Modell %.2f gegen gemessen %.2f)\n      ",
               d, v, gemessen);
        _fail++;
    }
}

TEST(die_beiden_regime_gehen_bei_spiral_zwei_auseinander)
{
    /* Gegenprobe: unterhalb von SPIR 2 muessen sich die beiden Formeln
     * deutlich unterscheiden, ab SPIR 2 zusammenfallen. Ohne diesen
     * Fall waere eine Fassung gruen, in der `no_fastload` einfach die
     * normale Formel aufruft. */
    for (int16_t sp = 0; sp <= 1; sp++) {
        double a = uft_st_speed(9, 1, sp, SEK, UPM);
        double b = uft_st_speed_no_fastload(9, 1, sp, SEK, UPM);
        ASSERT(b < a * 0.7);
    }
    for (int16_t sp = 2; sp <= 4; sp++) {
        double a = uft_st_speed(9, 1, sp, SEK, UPM);
        double b = uft_st_speed_no_fastload(9, 1, sp, SEK, UPM);
        ASSERT(fabs(a - b) < 1e-9);
    }
}

/* ─── Messung aus der Reihenfolge ───────────────────────────────────── */

/** Baut eine Spur mit vorgegebener physikalischer Sektorfolge. */
static void spur_bauen(uft_track_t *t, const uint8_t *folge, size_t n)
{
    memset(t, 0, sizeof *t);
    t->sectors = (uft_sector_t *)calloc(n, sizeof(uft_sector_t));
    t->sector_count = n;
    for (size_t i = 0; i < n; i++) t->sectors[i].id.sector = folge[i];
}

static void spur_frei(uft_track_t *t) { free(t->sectors); t->sectors = NULL; }

TEST(interleave_aus_der_reihenfolge)
{
    /* Interleave 1: 1,2,3,… — Sektor 2 folgt unmittelbar auf 1. */
    const uint8_t fortlaufend[9] = { 1,2,3,4,5,6,7,8,9 };
    uft_track_t t;
    spur_bauen(&t, fortlaufend, 9);
    ASSERT(uft_st_interleave_messen(&t) == 1);
    spur_frei(&t);

    /* Interleave 2: 1,6,2,7,3,8,4,9,5 — zwischen 1 und 2 liegt einer. */
    const uint8_t il2[9] = { 1,6,2,7,3,8,4,9,5 };
    spur_bauen(&t, il2, 9);
    ASSERT(uft_st_interleave_messen(&t) == 2);
    spur_frei(&t);
}

TEST(spiral_aus_zwei_spuren)
{
    const uint8_t a[9] = { 1,2,3,4,5,6,7,8,9 };
    const uint8_t b[9] = { 3,4,5,6,7,8,9,1,2 };   /* Anfang um 2 versetzt */
    uft_track_t t0, t1;
    spur_bauen(&t0, a, 9);
    spur_bauen(&t1, b, 9);

    ASSERT(uft_st_spiral_messen(&t0, &t1) == 2);

    uft_st_order_t o;
    uft_st_order_messen(&t0, &t1, &o);
    ASSERT(o.gemessen);
    ASSERT(o.spt == 9);
    ASSERT(o.spiral == 2);

    spur_frei(&t0); spur_frei(&t1);
}

TEST(ohne_spur_keine_messung)
{
    /* Gegenprobe: `gemessen` darf nicht bei leerer Eingabe wahr werden.
     * Spiralfaktor 0 ist ein GUELTIGER Wert (TOS 1.0) — wer das Flag
     * nicht prueft, liest eine Null als Messung. */
    uft_st_order_t o;
    uft_st_order_messen(NULL, NULL, &o);
    ASSERT(!o.gemessen);
    ASSERT(o.spiral < 0);
}

/* ─── Herkunft ──────────────────────────────────────────────────────── */

TEST(tos_herkunft_aus_dem_spiralfaktor)
{
    ASSERT(uft_st_tos_herkunft(0, 0, 9)  == UFT_TOS_100);
    ASSERT(uft_st_tos_herkunft(2, 2, 9)  == UFT_TOS_102_PLUS);
    ASSERT(uft_st_tos_herkunft(3, 2, 9)  == UFT_TOS_104_INOFF);

    /* Mehr als 10 Sektoren schliessen Desktop-Formatierung aus —
     * unabhaengig vom Spiralfaktor. */
    ASSERT(uft_st_tos_herkunft(2, 2, 11) == UFT_TOS_NICHT_DESKTOP);

    /* Unbekannter Spiralfaktor ergibt keine Herkunft, keine Vermutung. */
    ASSERT(uft_st_tos_herkunft(-1, -1, 9) == UFT_TOS_UNBEKANNT);

    /* Ein Wert, den keine TOS-Fassung erzeugt. */
    ASSERT(uft_st_tos_herkunft(5, 5, 9)  == UFT_TOS_NICHT_DESKTOP);
}

TEST(jede_herkunft_hat_einen_text)
{
    /* Gegenprobe: kein Zweig ohne Klartext, und keiner NULL. */
    for (int h = UFT_TOS_UNBEKANNT; h <= UFT_TOS_NICHT_DESKTOP; h++) {
        const char *s = uft_st_tos_herkunft_text((uft_tos_herkunft_t)h);
        ASSERT(s != NULL);
        ASSERT(s[0] != '\0');
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Atari ST: Spiralfaktor und Interleave (MF-858) ===\n");
    RUN(die_formel_traegt_ihre_messreihe);
    RUN(die_konstante_ist_keine_konstante);
    RUN(ohne_fastload_kostet_spiral_unter_zwei_eine_umdrehung);
    RUN(der_eine_ausreisser_ist_benannt_nicht_weggemittelt);
    RUN(die_beiden_regime_gehen_bei_spiral_zwei_auseinander);
    RUN(interleave_aus_der_reihenfolge);
    RUN(spiral_aus_zwei_spuren);
    RUN(ohne_spur_keine_messung);
    RUN(tos_herkunft_aus_dem_spiralfaktor);
    RUN(jede_herkunft_hat_einen_text);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
