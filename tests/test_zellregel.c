/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_zellregel.c
 * @brief Abnahme von `uft_zellregel_messen()` — Herleitung, Handproben
 *        und Differenzlauf gegen den Fremdbestand (MF-1128)
 *
 * ── Was hier geprueft wird, in der Reihenfolge der Beweiskraft ────────────
 *
 * **1) Die Schranken werden HERGELEITET, nicht zitiert.** Die Konstanten
 * `UFT_ZELL_MFM_MAX_NULL/MAX_EINS` und `UFT_ZELL_FM_MAX_NULL` stehen im
 * Header mit einer Begruendung; hier wird sie ausgefuehrt. Der Test
 * kodiert **alle** Datenfolgen bis 12 Bit (8190 Faelle) nach den
 * Kodierregeln —
 *
 *     MFM: je Datenbit das Zellpaar (c, d), c = 1 genau dann, wenn das
 *          vorige UND das laufende Datenbit 0 sind
 *     FM:  je Datenbit das Zellpaar (1, d)
 *
 * — und misst mit der neuen Funktion, was dabei herauskommt. Erwartet:
 * MFM laengster Nulllauf **3**, laengster Einslauf **1**; FM laengster
 * Nulllauf **1**, Einslauf unbegrenzt. Damit haengen die Konstanten an
 * einer laufenden Rechnung und nicht an einem Satz. MF-1079 hat
 * dieselben MFM-Werte unabhaengig an zwei echten SPS-Abbildern
 * gemessen — Herleitung und Objekt, zwei Wege zum selben Wert.
 *
 * **2) Handproben.** Kleine Muster, deren Erwartung im Quelltext
 * nachgerechnet daneben steht, nicht erinnert.
 *
 * **3) Differenzlauf gegen den Fremdbestand.** `dtc_find_run_violation`
 * aus `src/dtc_components/src/bitbuffer.c` hat dieselbe Bedeutung. Ueber
 * **2000 Zufallspuffer** mit zufaelligen Schranken muessen beide
 * dieselbe erste Verstossstelle UND dieselbe Trefferzahl nennen. Der
 * Fremdbestand wird dabei nur **ausgefuehrt**; keine Zeile daraus ist
 * uebernommen, und die Datei wird nicht angefasst.
 *
 * Die Zufallszahlen kommen aus einem im Test festgelegten xorshift —
 * kein `rand()`, damit der Lauf auf jeder Maschine und in jeder
 * CI-Runde dieselbe Folge prueft. Ein Fund ist damit reproduzierbar
 * statt „manchmal rot".
 *
 * **4) GCR ueber die vorhandenen Tafeln.** Die 256 aneinandergelegten
 * CBM-4-zu-5-Kodes, jetzt durch die gemeinsame Funktion statt durch eine
 * vierte handgeschriebene Schleife. Erwartet laengster Nulllauf 2.
 *
 * **5) Die Bitordnung ist nicht dekorativ.** Derselbe Puffer MSB-zuerst
 * und LSB-zuerst gelesen muss verschiedene Verstossstellen liefern,
 * sonst wird der Parameter ignoriert (Klasse MF-1000: ein Argument, das
 * nichts tut).
 *
 * ── Rotbeweis ─────────────────────────────────────────────────────────────
 *
 * Eingebaut statt nebenbei: ein Puffer mit einem **gepflanzten** langen
 * Nulllauf an bekannter Stelle muss genau dort gemeldet werden. Trifft
 * die Funktion die Stelle nicht, faellt die Zusage — ohne dass jemand
 * den Test von aussen verstuemmeln muss.
 */

#include "uft/core/uft_zellregel.h"

#include <stdio.h>
#include <string.h>

/* Fremdbestand — nur AUSGEFUEHRT, nichts uebernommen. Deklariert wird
 * hier nur, was gebraucht wird. */
typedef struct { uint8_t *data; size_t bit_count; int writable; } dtc_bits;
void   dtc_bits_init(dtc_bits *b, void *p, size_t n, int w);
size_t dtc_find_run_violation(const dtc_bits *b, size_t start, size_t n,
                              unsigned max0, unsigned max1, int count);

/* Die CBM-Tafel des Baums, ueber ihren oeffentlichen Zugang. */
const uint8_t *gcr_get_encode_table(void);

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    printf("  %-4s %s%s%s\n", ok ? "ok" : "ROT", was,
           detail && *detail ? " - " : "", detail ? detail : "");
    if (ok) gruen++; else rot++;
}

/* ── ein festgelegter Zufall, damit ein Fund reproduzierbar ist ───────── */
static uint32_t zustand = 0x2F6E2B1u;
static uint32_t wuerfel(void)
{
    zustand ^= zustand << 13;
    zustand ^= zustand >> 17;
    zustand ^= zustand << 5;
    return zustand;
}

/* ── Zellen setzen (MSB zuerst), damit die Herleitung lesbar bleibt ───── */
static void setze(uint8_t *puf, size_t i, unsigned v)
{
    const uint8_t m = (uint8_t)(0x80u >> (i & 7u));
    if (v) puf[i >> 3] |= m; else puf[i >> 3] &= (uint8_t)~m;
}

/* ── 1) Herleitung: MFM und FM ueber alle Datenfolgen bis 12 Bit ──────── */
static void herleitung(void)
{
    uint8_t puf[8];                 /* 12 Datenbits -> 24 Zellen -> 3 Byte */
    unsigned mfm_n = 0, mfm_e = 0, fm_n = 0, fm_e = 0;
    unsigned long faelle = 0;

    for (unsigned k = 1; k <= 12u; k++) {
        for (unsigned long muster = 0; muster < (1UL << k); muster++) {
            /* MFM */
            memset(puf, 0, sizeof puf);
            unsigned vor = 0, z = 0;
            for (unsigned i = 0; i < k; i++) {
                const unsigned d = (unsigned)((muster >> (k - 1u - i)) & 1UL);
                setze(puf, z++, (vor == 0u && d == 0u) ? 1u : 0u);
                setze(puf, z++, d);
                vor = d;
            }
            uft_zellregel_t r;
            if (uft_zellregel_messen(puf, z, UFT_ZELL_MSB_ZUERST,
                                     UFT_ZELL_UNBEGRENZT,
                                     UFT_ZELL_UNBEGRENZT, &r)) {
                if (r.max_null > mfm_n) mfm_n = r.max_null;
                if (r.max_eins > mfm_e) mfm_e = r.max_eins;
            }

            /* FM */
            memset(puf, 0, sizeof puf);
            z = 0;
            for (unsigned i = 0; i < k; i++) {
                const unsigned d = (unsigned)((muster >> (k - 1u - i)) & 1UL);
                setze(puf, z++, 1u);
                setze(puf, z++, d);
            }
            if (uft_zellregel_messen(puf, z, UFT_ZELL_MSB_ZUERST,
                                     UFT_ZELL_UNBEGRENZT,
                                     UFT_ZELL_UNBEGRENZT, &r)) {
                if (r.max_null > fm_n) fm_n = r.max_null;
                if (r.max_eins > fm_e) fm_e = r.max_eins;
            }
            faelle++;
        }
    }

    char det[192];
    snprintf(det, sizeof det,
             "%lu Datenfolgen, MFM Nulllauf %u / Einslauf %u",
             faelle, mfm_n, mfm_e);
    pruefe("MFM: die Kodierregel ERGIBT die Konstanten des Headers "
           "(Nulllauf 3, Einslauf 1)",
           mfm_n == UFT_ZELL_MFM_MAX_NULL && mfm_e == UFT_ZELL_MFM_MAX_EINS,
           det);

    snprintf(det, sizeof det, "FM Nulllauf %u / Einslauf %u", fm_n, fm_e);
    pruefe("FM: Nulllauf 1, und der Einslauf ist UNBEGRENZT - deshalb "
           "steht dort keine erfundene Zahl",
           fm_n == UFT_ZELL_FM_MAX_NULL && fm_e > 2u, det);

    /* Gegenprobe zur Herleitung: mit den Schranken des Headers darf ein
     * MFM-Strom keinen Verstoss melden. */
    memset(puf, 0, sizeof puf);
    unsigned vor = 0, z = 0;
    for (unsigned i = 0; i < 12u; i++) {
        const unsigned d = (i * 7u) & 1u;
        setze(puf, z++, (vor == 0u && d == 0u) ? 1u : 0u);
        setze(puf, z++, d);
        vor = d;
    }
    pruefe("ein MFM-Strom haelt die MFM-Schranken",
           uft_zellregel_haelt(puf, z, UFT_ZELL_MSB_ZUERST,
                               UFT_ZELL_MFM_MAX_NULL,
                               UFT_ZELL_MFM_MAX_EINS), "");
}

/* ── 2) Handproben, Erwartung im Quelltext nachgerechnet ─────────────── */
static void handproben(void)
{
    uft_zellregel_t r;
    char det[192];

    /* 0xA5 = 1010 0101, MSB zuerst: 1,0,1,0,0,1,0,1
     * Einsen 4; Paare 0; Nulllaeufe 1,2,1 -> max 2; Einslaeufe alle 1. */
    const uint8_t a[1] = { 0xA5 };
    pruefe("0xA5 wird gemessen",
           uft_zellregel_messen(a, 8, UFT_ZELL_MSB_ZUERST,
                                UFT_ZELL_UNBEGRENZT, UFT_ZELL_UNBEGRENZT, &r),
           "");
    snprintf(det, sizeof det,
             "Einsen %zu, Paare %zu, Nulllauf %u, Einslauf %u",
             r.einsen, r.paare, r.max_null, r.max_eins);
    pruefe("0xA5: 4 Einsen, 0 Paare, Nulllauf 2, Einslauf 1",
           r.einsen == 4 && r.paare == 0 && r.max_null == 2
           && r.max_eins == 1, det);

    /* 0x00 0x00 = 16 Nullen. */
    const uint8_t n[2] = { 0x00, 0x00 };
    (void)uft_zellregel_messen(n, 16, UFT_ZELL_MSB_ZUERST,
                               UFT_ZELL_UNBEGRENZT, UFT_ZELL_UNBEGRENZT, &r);
    snprintf(det, sizeof det, "Nulllauf %u, Einsen %zu", r.max_null, r.einsen);
    pruefe("16 Nullen: Nulllauf 16, keine Eins",
           r.max_null == 16 && r.einsen == 0, det);

    /* 0xFF = 8 Einsen: Einslauf 8, Paare 7. */
    const uint8_t e[1] = { 0xFF };
    (void)uft_zellregel_messen(e, 8, UFT_ZELL_MSB_ZUERST,
                               UFT_ZELL_UNBEGRENZT, UFT_ZELL_UNBEGRENZT, &r);
    snprintf(det, sizeof det, "Einslauf %u, Paare %zu", r.max_eins, r.paare);
    pruefe("8 Einsen: Einslauf 8, 7 Paare",
           r.max_eins == 8 && r.paare == 7, det);

    /* Teillaenge: nur die ersten 3 Zellen von 0xA5 (1,0,1). */
    (void)uft_zellregel_messen(a, 3, UFT_ZELL_MSB_ZUERST,
                               UFT_ZELL_UNBEGRENZT, UFT_ZELL_UNBEGRENZT, &r);
    snprintf(det, sizeof det, "Zellen %zu, Einsen %zu, Nulllauf %u",
             r.zellen, r.einsen, r.max_null);
    pruefe("die Zellzahl begrenzt wirklich (3 von 8)",
           r.zellen == 3 && r.einsen == 2 && r.max_null == 1, det);

    pruefe("0 Zellen: kein Fehler, kein Verstoss",
           uft_zellregel_messen(a, 0, UFT_ZELL_MSB_ZUERST, 1u, 1u, &r)
           && r.verstoesse == 0 && r.erster_bruch == SIZE_MAX, "");
    pruefe("NULL-Puffer wird abgewiesen",
           !uft_zellregel_messen(NULL, 8, UFT_ZELL_MSB_ZUERST, 1u, 1u, &r),
           "");
    pruefe("NULL-Ergebnis wird abgewiesen",
           !uft_zellregel_messen(a, 8, UFT_ZELL_MSB_ZUERST, 1u, 1u, NULL),
           "");
}

/* ── Rotbeweis: ein gepflanzter Lauf an SEINER Stelle ────────────────── */
static void gepflanzt(void)
{
    uint8_t puf[16];
    uft_zellregel_t r;
    char det[192];

    /* Grundmuster 0xAA = 1,0,1,0,… also: gerade Zellen 1, ungerade 0.
     * Dann werden die Zellen 40..44 auf 0 gesetzt.
     *
     * ACHTUNG, und das ist der Grund, warum die Erwartung hier
     * ausgerechnet und nicht geschaetzt steht: der Lauf ist NICHT fuenf
     * Zellen lang. Zelle 39 ist ungerade und damit schon 0, Zelle 45
     * ebenso — der gepflanzte Block waechst an seinen Nachbarn an, und
     * der Nulllauf reicht von 39 bis 45, also **7** Zellen.
     *
     * Bei max_null = 3 ist die erste ueberzaehlige Zelle die vierte des
     * Laufs: 39, 40, 41 sind erlaubt, ab **42** wird es ein Verstoss.
     * Verstoesse sind damit 42, 43, 44, 45 = **4**.
     *
     * Eine erste Fassung dieses Tests erwartete 5 / 2 / 43 und fiel
     * prompt — die Funktion hatte recht, die Erwartung nicht. Das steht
     * hier, weil es genau der Fehler ist, den dieser Test verhindern
     * soll: eine Zahl ohne Rechnung dahinter. */
    memset(puf, 0xAA, sizeof puf);
    for (size_t i = 40; i <= 44; i++) setze(puf, i, 0u);

    (void)uft_zellregel_messen(puf, 128, UFT_ZELL_MSB_ZUERST, 3u,
                               UFT_ZELL_UNBEGRENZT, &r);
    snprintf(det, sizeof det,
             "Nulllauf %u, Verstoesse %zu, erster bei %zu (erwartet 7/4/42)",
             r.max_null, r.verstoesse, r.erster_bruch);
    pruefe("der gepflanzte Lauf reicht von Zelle 39 bis 45 und wird bei "
           "max_null=3 ab Zelle 42 gemeldet, mit 4 Verstoessen",
           r.max_null == 7 && r.verstoesse == 4 && r.erster_bruch == 42,
           det);

    (void)uft_zellregel_messen(puf, 128, UFT_ZELL_MSB_ZUERST,
                               UFT_ZELL_UNBEGRENZT,
                               UFT_ZELL_UNBEGRENZT, &r);
    snprintf(det, sizeof det, "Nulllauf %u, Verstoesse %zu",
             r.max_null, r.verstoesse);
    pruefe("ohne Schranke: 0 Verstoesse, der Nulllauf 7 steht dennoch da",
           r.verstoesse == 0 && r.erster_bruch == SIZE_MAX
           && r.max_null == 7, det);
}

/* ── 3) Differenzlauf gegen den Fremdbestand ─────────────────────────── */
static void differenzlauf(void)
{
    /* Namen mit Vorsilbe, und der Grund ist gemessen: `LAEUFE` allein
     * ist in diesem Baum schon zweimal belegt —
     * `tests/test_probe_confidence_on_random.c` fuehrt es als Makro mit
     * dem Wert 100, `tests/test_kfx_sonde_sagt_nein.c:174` als lokale
     * Konstante mit 500; hier waeren es 2000. Heute sieht keine
     * Uebersetzungseinheit zwei davon, aber genau solche Paare sind die
     * Klasse MF-852 (gleicher Name, verschiedener Wert), und das Tor
     * `enum vs macro conflicts` hat den Commit deshalb zu Recht
     * abgewiesen. Umbenannt statt in die Ausnahmeliste eingetragen. */
    enum { DIFF_LAEUFE = 2000, DIFF_BYTES = 32 };
    uint8_t puf[DIFF_BYTES];
    size_t ungleich_stelle = 0, ungleich_zahl = 0;

    for (unsigned lauf = 0; lauf < DIFF_LAEUFE; lauf++) {
        for (int i = 0; i < DIFF_BYTES; i++)
            puf[i] = (uint8_t)(wuerfel() >> 11);
        const size_t zellen = 1u + (wuerfel() % (DIFF_BYTES * 8u));
        const unsigned m0 = wuerfel() % 6u;   /* 0 = unbegrenzt */
        const unsigned m1 = wuerfel() % 6u;

        uft_zellregel_t r;
        if (!uft_zellregel_messen(puf, zellen, UFT_ZELL_MSB_ZUERST,
                                  m0, m1, &r))
            continue;

        dtc_bits b;
        dtc_bits_init(&b, puf, zellen, 0);
        const size_t fremd_stelle =
            dtc_find_run_violation(&b, 0, zellen, m0, m1, 0);
        const size_t fremd_zahl =
            dtc_find_run_violation(&b, 0, zellen, m0, m1, 1);

        if (r.erster_bruch != fremd_stelle) ungleich_stelle++;
        if (r.verstoesse != fremd_zahl) ungleich_zahl++;
    }

    char det[224];
    snprintf(det, sizeof det,
             "%d Laeufe, Stelle ungleich %zu, Zahl ungleich %zu",
             DIFF_LAEUFE, ungleich_stelle, ungleich_zahl);
    pruefe("ueber 2000 Zufallspuffer nennen UFT und der Fremdbestand "
           "dieselbe erste Stelle und dieselbe Trefferzahl",
           ungleich_stelle == 0 && ungleich_zahl == 0, det);
}

/* ── 4) GCR ueber die vorhandene Tafel, jetzt gemeinsam gemessen ─────── */
static void gcr(void)
{
    const uint8_t *t = gcr_get_encode_table();
    unsigned max_paar = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++) {
            /* zwei 5-Bit-Kodes, MSB zuerst in zwei Byte gelegt */
            const uint32_t w = ((uint32_t)t[i] << 5) | (uint32_t)t[j];
            uint8_t puf[2];
            puf[0] = (uint8_t)(w >> 2);
            puf[1] = (uint8_t)((w & 3u) << 6);
            uft_zellregel_t r;
            if (uft_zellregel_messen(puf, 10, UFT_ZELL_MSB_ZUERST,
                                     UFT_ZELL_UNBEGRENZT,
                                     UFT_ZELL_UNBEGRENZT, &r)
                && r.max_null > max_paar)
                max_paar = r.max_null;
        }
    char det[128];
    snprintf(det, sizeof det, "256 Paare, laengster Nulllauf %u", max_paar);
    pruefe("CBM-GCR: die Selbsttaktbedingung, gemessen durch die "
           "gemeinsame Funktion statt durch eine vierte eigene Schleife",
           max_paar == UFT_ZELL_GCR45_MAX_NULL, det);
}

/* ── 5) Die Bitordnung tut wirklich etwas ────────────────────────────── */
static void ordnung(void)
{
    /* 0x0F = 0000 1111.
     * MSB zuerst: 0,0,0,0,1,1,1,1 — bei max_null=2 ist die erste
     *   ueberzaehlige Nullzelle die dritte, Index 2.
     * LSB zuerst: 1,1,1,1,0,0,0,0 — dort beginnt der Nulllauf bei
     *   Index 4, die dritte Nullzelle ist Index 6. */
    const uint8_t p[1] = { 0x0F };
    uft_zellregel_t a, b;
    (void)uft_zellregel_messen(p, 8, UFT_ZELL_MSB_ZUERST, 2u,
                               UFT_ZELL_UNBEGRENZT, &a);
    (void)uft_zellregel_messen(p, 8, UFT_ZELL_LSB_ZUERST, 2u,
                               UFT_ZELL_UNBEGRENZT, &b);
    char det[160];
    snprintf(det, sizeof det, "MSB erster Bruch %zu, LSB erster Bruch %zu",
             a.erster_bruch, b.erster_bruch);
    pruefe("MSB- und LSB-Ordnung liefern verschiedene Verstossstellen - "
           "der Parameter wird nicht ignoriert",
           a.erster_bruch == 2 && b.erster_bruch == 6, det);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Zellregel: Herleitung, Handproben, Differenzlauf (MF-1128)\n");
    printf("==========================================================\n");

    printf("\n1) Die Schranken werden hergeleitet, nicht zitiert\n");
    herleitung();

    printf("\n2) Handproben\n");
    handproben();

    printf("\n3) Gepflanzter Lauf an bekannter Stelle\n");
    gepflanzt();

    printf("\n4) Differenzlauf gegen src/dtc_components (nur ausgefuehrt)\n");
    differenzlauf();

    printf("\n5) GCR ueber die vorhandene Tafel\n");
    gcr();

    printf("\n6) Die Bitordnung\n");
    ordnung();

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
