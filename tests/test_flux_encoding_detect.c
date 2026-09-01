/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_flux_encoding_detect.c
 * @brief Die Kodierungserkennung ging am Mittelwert — und lag dreimal
 *        falsch (MF-766).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * `flux_detect_encoding()` teilte allein nach dem MITTLEREN Wechsel-
 * abstand ein, in vier handgewählten Schwellen. Gemessen an den
 * Konstanten dieses Baums selbst (`uft_flux_decoder.h:35-40`), mit dem
 * Wechselmuster **je** Kodierung:
 *
 *     MFM HD  2T/3T/4T x1000ns    Mittel  2973 ns -> MFM       ok
 *     MFM DD  2T/3T/4T x2000ns    Mittel  6022 ns -> GCR_C64   FALSCH
 *     FM      1T/2T    x4000ns    Mittel  5993 ns -> GCR_C64   FALSCH
 *     GCR C64 Sync + 1..3T        Mittel  5598 ns -> GCR_C64   ok
 *     GCR Apple 1..3T  x4000ns    Mittel  7943 ns -> GCR_C64   FALSCH
 *
 * Drei von fünf falsch, und die schwerste ist **DD-MFM**: die häufigste
 * Diskette überhaupt (PC 720K, Amiga 880K, Atari ST) ging an den
 * Commodore-GCR-Dekoder. `GCR_C64` war der Auffangwert für alles über
 * 5000 ns und hat deshalb nur zufällig gestimmt.
 *
 * ── Warum keine besseren Schwellen helfen ────────────────────────────────
 *
 * MFM DD (6022), FM (5993) und GCR C64 (5598) liegen innerhalb von 8 %.
 * Das ist kein Einstellungsfehler, das ist das **falsche Maß**.
 *
 * Das vorhandene Histogramm (MF-488) trennt sie am Verhältnismuster:
 *
 *     MFM      3 Berge, 2 : 3 : 4
 *     FM       2 Berge, 1 : 2
 *     GCR      3 Berge, 1 : 2 : 3   — C64 gegen Apple an der Zellendauer
 *
 * ── Der wichtigste Fall steht am Ende ────────────────────────────────────
 *
 * Fall 6 trägt gar kein sauberes Verhältnis. Er MUSS `FLUX_ENC_AUTO`
 * liefern — „unentschieden". Genau dieses Raten war der Fehler: eine
 * beschädigte oder geschützte Spur in den häufigsten Zweig zu schieben.
 * Wer die Erkennung später erweitert und diesen Fall auf ein Format
 * zwingt, findet den Rotbeweis hier schon vor.
 *
 * Eine eigene Eichung dazu: `h.confident` darf KEIN Tor sein. Gemessen
 * ist es nur für MFM wahr — als Bedingung genommen ließe es FM und GCR
 * gar nicht erst zu. Dieselbe Bandmodell-Falle wie in MF-765.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/flux/uft_flux_decoder.h"

static int fehler = 0;

/* Fester Startwert: ein Test, der bei jedem Lauf etwas anderes misst,
 * ist keiner. Derselbe Wert wie in der Messung, damit die Zahlen im
 * Kopfkommentar reproduzierbar sind. */
static uint32_t st = 7u;
static uint32_t rnd(void) { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return st; }

static const char *nm(flux_encoding_t e)
{
    switch (e) {
    case FLUX_ENC_MFM:       return "MFM";
    case FLUX_ENC_FM:        return "FM";
    case FLUX_ENC_GCR_C64:   return "GCR_C64";
    case FLUX_ENC_GCR_APPLE: return "GCR_APPLE";
    case FLUX_ENC_AMIGA:     return "AMIGA";
    default:                 return "AUTO (unentschieden)";
    }
}

enum { N = 4000 };

static void pruefe(const char *titel, const uint32_t *iv, flux_encoding_t soll)
{
    flux_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));
    if (flux_raw_from_ns_intervals(iv, N, &raw) != FLUX_OK) {
        printf("  FAIL %-32s Aufbau der Fixture fehlgeschlagen\n", titel);
        fehler++;
        return;
    }

    double sum = 0;
    for (int i = 0; i < N; i++) sum += iv[i];

    flux_encoding_t got = flux_detect_encoding(&raw);
    int ok = (got == soll);
    if (!ok) fehler++;
    printf("  %s %-30s Mittel %6.0f ns -> %-20s (erwartet %s)\n",
           ok ? "ok  " : "FAIL", titel, sum / N, nm(got), nm(soll));

    free(raw.transitions);
    free(raw.index_times);
}

int main(void)
{
    uint32_t *iv = (uint32_t *)malloc(N * sizeof(uint32_t));
    if (!iv) return 2;

    printf("test_flux_encoding_detect (MF-766)\n");

    /* MFM: Wechsel im Abstand von 2, 3 oder 4 Zellen. */
    for (int i = 0; i < N; i++) iv[i] = (2 + (rnd() % 3)) * 1000;
    pruefe("MFM HD  2T/3T/4T x1000ns", iv, FLUX_ENC_MFM);

    for (int i = 0; i < N; i++) iv[i] = (2 + (rnd() % 3)) * 2000;
    pruefe("MFM DD  2T/3T/4T x2000ns", iv, FLUX_ENC_MFM);

    /* FM: Takt in jeder Zelle, Datenpuls bei '1' — Abstand 1T oder 2T. */
    for (int i = 0; i < N; i++) iv[i] = (1 + (rnd() % 2)) * 4000;
    pruefe("FM      1T/2T x4000ns", iv, FLUX_ENC_FM);

    /* C64-GCR: hoechstens zwei 0-Bits am Stueck (1..3T), dazu die
     * Sync-Laeufe aus lauter 1-Bits (1T). */
    for (int i = 0; i < N; i++) iv[i] = (i % 40 < 10) ? 3200 : (1 + (rnd() % 3)) * 3200;
    pruefe("GCR C64 Sync + 1..3T", iv, FLUX_ENC_GCR_C64);

    /* Apple-GCR: dasselbe Muster, andere Zellendauer. Die Unterscheidung
     * haengt an den Konstanten des Baums (3200 gegen 4000), nicht an
     * einer neuen Zahl. */
    for (int i = 0; i < N; i++) iv[i] = (1 + (rnd() % 3)) * 4000;
    pruefe("GCR Apple 1..3T x4000ns", iv, FLUX_ENC_GCR_APPLE);

    /* DER WICHTIGSTE FALL: kein Verhaeltnismuster. Muss „unentschieden"
     * bleiben — nicht in den haeufigsten Zweig fallen. */
    for (int i = 0; i < N; i++) iv[i] = 1500 + (rnd() % 7000);
    pruefe("kein Muster -> unentschieden", iv, FLUX_ENC_AUTO);

    free(iv);
    printf("%s (%d Fehler)\n", fehler ? "FAIL" : "PASS", fehler);
    return fehler ? 1 : 0;
}
