/**
 * @file test_fdc_gap3.c
 * @brief „Passt nicht" kam als GROESSTMOEGLICHE Luecke heraus (MF-1167)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uft_fdc_calc_gap3()` rechnet den Zwischenraum aus der Spurkapazitaet:
 *
 *     uint32_t data_space = sectors * (sector_size + overhead + 2);
 *     uint32_t gap_space  = track_capacity - track_overhead - data_space;
 *     uint32_t gap3       = gap_space / sectors;
 *     if (gap3 < 10)  gap3 = 10;
 *     if (gap3 > 255) gap3 = 255;
 *
 * `gap_space` ist VORZEICHENLOS. Passt das Format nicht in die Spur, laeuft
 * die Subtraktion ueber, `gap3` wird riesig — und dann greift die OBERE
 * Klemme. Die Funktion liefert fuer ein Format, das ueberhaupt nicht passt,
 * 255: die groesstmoegliche Luecke.
 *
 * Gemessen am Vorzustand:
 *
 *     1,44 M 18x512, Kapazitaet 12500   data=10332  gap_space=      2022  -> 112  richtig
 *     720 K  18x512, Kapazitaet  6250   data=10332  gap_space=4294963068  -> 255  FALSCH
 *     720 K   9x512, Kapazitaet  6250   data= 5166  gap_space=       938  -> 104  richtig
 *     1,2 M  21x512, Kapazitaet 10416   data=12054  gap_space=4294965512  -> 255  FALSCH
 *
 * Bei 720 K mit 18 Sektoren zu 512 Byte stehen 10 332 Byte Nutzdaten einer
 * Spurkapazitaet von 6250 Byte gegenueber — es fehlen ueber 4000 Byte, und
 * die Antwort lautete „nimm 255".
 *
 * ── Was die Zulieferung benannt hat und was nicht ───────────────────────
 *
 * Die Eigentuemer-Zulieferung (OmniFlop-Analyse §3.4) nennt die UNTERE
 * Klemme: „if (gap3 < 10) gap3 = 10; ← erzeugt eine unlesbare Diskette".
 * Das ist richtig als Entwurfskritik — aber im Ueberlauffall wird sie NIE
 * erreicht. Der Weg fuehrt ueber die obere Klemme, und 255 ist die
 * schaedlichere Antwort: 10 sieht nach „eng" aus, 255 nach „reichlich Platz".
 *
 * ── Die Regel, und woher sie kommt ──────────────────────────────────────
 *
 * Aus derselben Zulieferung, und sie ist die richtige:
 *
 *     „Passt nicht. NICHT auf ein Minimum klemmen — das erzeugte eine
 *      Diskette, die kein Laufwerk lesen kann, und verschwieg den Grund."
 *
 * Dieselbe Doktrin hat der Eigentuemer fuer den PLL ausgesprochen
 * (`uft_pll.c`, aus der Franaszek-RLL-Zulieferung): Verstoesse MELDEN statt
 * klemmen. Und es ist die Gestalt von MF-1022 und MF-1040, wo ein Kuerzen
 * als Erfolg gemeldet wurde.
 *
 * Seit MF-1167 gibt die Funktion in diesem Fall 0 zurueck — denselben Wert
 * wie fuer `sectors == 0`, und mit derselben Bedeutung: es gibt keinen
 * gueltigen Zwischenraum. Der Kopf sagt es jetzt.
 *
 * ── Und die Funktion hat keinen Aufrufer ────────────────────────────────
 *
 * Gemessen: `uft_fdc_calc_gap3` kommt im ganzen Baum nur an zwei Stellen
 * vor — ihrer Definition und ihrer Deklaration. Die Korrektur ist damit
 * VORBEUGEND, nicht behebend; kein heute erzeugtes Abbild traegt die 255.
 * Das ist die dritte unerreichbare Rechenstelle dieser Runde nach
 * `hfe_create()` (MF-1166) und den zwei JV3-Waisen — Klasse P3-204.
 *
 * ── Rotbeweis ───────────────────────────────────────────────────────────
 *
 * Fall 1 und 4 sind vor MF-1167 gruen (die zwei passenden Formate). Fall 2
 * und 3 fallen.
 */
#include "uft/formats/uft_fdc_gaps.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ── Fall 1: ein Format, das passt ───────────────────────────────────── */

TEST(anderthalb_megabyte_passt_und_bleibt)
{
    /* Gruen vor UND nach MF-1167. Steht zuerst, damit belegt ist, dass die
     * Korrektur nicht einfach alles ablehnt. 18 x 512 bei 12 500 Byte
     * Spurkapazitaet laesst 2022 Byte fuer die Zwischenraeume. */
    uint8_t g = uft_fdc_calc_gap3(12500u, 18u, 512u, true);
    ASSERT(g == 112u);
}

/* ── Fall 2: ROTBEWEIS — 18 Sektoren in eine 720-K-Spur ──────────────── */

TEST(achtzehn_sektoren_passen_nicht_in_720k)
{
    /* 18 x (512 + 62) = 10 332 Byte Nutzdaten bei 6250 Byte Spur. Vor
     * MF-1167 lief `gap_space` ueber und die Antwort war 255 — die
     * groesstmoegliche Luecke fuer ein Format, dem ueber 4000 Byte fehlen. */
    uint8_t g = uft_fdc_calc_gap3(6250u, 18u, 512u, true);
    ASSERT(g == 0u);        /* 0 heisst: es gibt keinen gueltigen Zwischenraum */
}

/* ── Fall 3: ROTBEWEIS — 21 Sektoren in eine 1,2-M-Spur ──────────────── */

TEST(einundzwanzig_sektoren_passen_nicht_in_1_2m)
{
    /* Derselbe Fall, den die Zulieferung als „1.2M 21x512: PASST NICHT
     * (gap3=0)" auffuehrt. Vor MF-1167 kam auch hier 255 heraus. */
    uint8_t g = uft_fdc_calc_gap3(10416u, 21u, 512u, true);
    ASSERT(g == 0u);
}

/* ── Fall 4: Gegenprobe — 9 Sektoren passen weiterhin ────────────────── */

TEST(neun_sektoren_passen_weiterhin)
{
    /* Gruen vor UND nach MF-1167. 9 x 574 = 5166 Byte bei 6250 Byte Spur. */
    uint8_t g = uft_fdc_calc_gap3(6250u, 9u, 512u, true);
    ASSERT(g == 104u);
}

/* ── Fall 5: die vorhandene Absage bleibt ────────────────────────────── */

TEST(null_sektoren_bleiben_null)
{
    /* Die Funktion gab schon immer 0 fuer `sectors == 0` zurueck. MF-1167
     * gibt dem Wert 0 eine zweite, GLEICHBEDEUTENDE Lesart — „kein
     * gueltiger Zwischenraum" — und macht ihn nicht mehrdeutig. */
    ASSERT(uft_fdc_calc_gap3(12500u, 0u, 512u, true) == 0u);
}

int main(void)
{
    printf("=== FDC-Zwischenraum: passt nicht ist keine Luecke (MF-1167) ===\n");
    RUN(anderthalb_megabyte_passt_und_bleibt);
    RUN(achtzehn_sektoren_passen_nicht_in_720k);
    RUN(einundzwanzig_sektoren_passen_nicht_in_1_2m);
    RUN(neun_sektoren_passen_weiterhin);
    RUN(null_sektoren_bleiben_null);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
