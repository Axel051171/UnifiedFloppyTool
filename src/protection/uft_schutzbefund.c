/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_schutzbefund.c
 * @brief Der Bericht mit zwei Listen, und der erste Detektor (MF-792).
 *
 * Begründung, Referenz und die Abstammungswarnung stehen im Kopf von
 * `include/uft/protection/uft_schutzbefund.h`.
 */
#include "uft/protection/uft_schutzbefund.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Taxonomie ───────────────────────────────────────────────────────── */

bool uft_schutz_braucht_fluss(uft_schutz_code_t code)
{
    /* Die sechs zeitbasierten Codes der Referenz. Auf einem dekodierten
     * Abbild sind sie GRUNDSAETZLICH nicht feststellbar — nicht „schwer",
     * sondern unmoeglich: das Format traegt die Information nicht. */
    switch (code) {
    case UFT_SCHUTZ_LGS:
    case UFT_SCHUTZ_SHS:
    case UFT_SCHUTZ_LGT:
    case UFT_SCHUTZ_SHT:
    case UFT_SCHUTZ_SBV:
    case UFT_SCHUTZ_NFA:
        return true;
    default:
        return false;
    }
}

const char *uft_schutz_code_name(uft_schutz_code_t c)
{
    switch (c) {
    case UFT_SCHUTZ_EXT: return "EXT"; case UFT_SCHUTZ_TNF: return "TNF";
    case UFT_SCHUTZ_SFT: return "SFT"; case UFT_SCHUTZ_DOI: return "DOI";
    case UFT_SCHUTZ_TLP: return "TLP"; case UFT_SCHUTZ_NOS: return "NOS";
    case UFT_SCHUTZ_SSZ: return "SSZ"; case UFT_SCHUTZ_IIF: return "IIF";
    case UFT_SCHUTZ_DSN: return "DSN"; case UFT_SCHUTZ_SWS: return "SWS";
    case UFT_SCHUTZ_NSD: return "NSD"; case UFT_SCHUTZ_SNI: return "SNI";
    case UFT_SCHUTZ_SND: return "SND"; case UFT_SCHUTZ_DCE: return "DCE";
    case UFT_SCHUTZ_DTT: return "DTT"; case UFT_SCHUTZ_HDG: return "HDG";
    case UFT_SCHUTZ_HDT: return "HDT"; case UFT_SCHUTZ_IDG: return "IDG";
    case UFT_SCHUTZ_ISS: return "ISS"; case UFT_SCHUTZ_PUT: return "PUT";
    case UFT_SCHUTZ_FZS: return "FZS"; case UFT_SCHUTZ_FZT: return "FZT";
    case UFT_SCHUTZ_LGS: return "LGS"; case UFT_SCHUTZ_SHS: return "SHS";
    case UFT_SCHUTZ_LGT: return "LGT"; case UFT_SCHUTZ_SHT: return "SHT";
    case UFT_SCHUTZ_SBV: return "SBV"; case UFT_SCHUTZ_NFA: return "NFA";
    default: return "??";
    }
}

const char *uft_uebersprungen_name(uft_uebersprungen_grund_t g)
{
    switch (g) {
    case UFT_UEBERSPRUNGEN_KEIN_FLUSS:
        return "Quelle traegt keine Zeitinformation";
    case UFT_UEBERSPRUNGEN_ZU_WENIG_UMDREHUNGEN:
        return "zu wenige Umdrehungen";
    case UFT_UEBERSPRUNGEN_KEIN_INDEX:
        return "kein Indexsignal";
    case UFT_UEBERSPRUNGEN_NICHT_DEKODIERBAR:
        return "Spur nicht dekodierbar";
    case UFT_UEBERSPRUNGEN_ABGESCHALTET:
        return "Detektor abgeschaltet";
    default:
        return "unbekannt";
    }
}

/* ── Bericht ─────────────────────────────────────────────────────────── */

void uft_schutz_bericht_init(uft_schutz_bericht_t *b)
{
    if (b) memset(b, 0, sizeof(*b));
}

void uft_schutz_bericht_frei(uft_schutz_bericht_t *b)
{
    if (!b) return;
    free(b->befunde);
    free(b->uebersprungen);
    memset(b, 0, sizeof(*b));
}

static bool platz(void **feld, size_t *anzahl, size_t *kap, size_t gr)
{
    if (*anzahl < *kap) return true;
    size_t neu = *kap ? *kap * 2 : 8;
    void *p = realloc(*feld, neu * gr);
    if (!p) return false;
    *feld = p;
    *kap = neu;
    return true;
}

bool uft_schutz_bericht_add(uft_schutz_bericht_t *b,
                            const uft_schutz_befund_t *f)
{
    if (!b || !f) return false;
    if (!platz((void **)&b->befunde, &b->befund_anzahl,
               &b->befund_kapazitaet, sizeof(*b->befunde)))
        return false;
    b->befunde[b->befund_anzahl++] = *f;
    return true;
}

bool uft_schutz_bericht_uebersprungen(uft_schutz_bericht_t *b,
                                      uft_schutz_code_t code,
                                      uft_uebersprungen_grund_t grund,
                                      uft_schutz_ort_t ort)
{
    if (!b) return false;
    if (!platz((void **)&b->uebersprungen, &b->uebersprungen_anzahl,
               &b->uebersprungen_kapazitaet, sizeof(*b->uebersprungen)))
        return false;
    uft_schutz_uebersprungen_t u = { code, grund, ort };
    b->uebersprungen[b->uebersprungen_anzahl++] = u;
    return true;
}

/* ── Registry ────────────────────────────────────────────────────────── */

/* Warum die Registry das Ueberspringen protokolliert und nicht der
 * Detektor: ein Detektor, der seinen eigenen Ausfall meldet, kann es
 * vergessen — und dann faellt der Fall in die Luecke, die diese ganze
 * Datei schliessen soll. Hier ist es strukturell nicht vergessbar. */
static bool bedarf_gedeckt(const uft_schutz_bedarf_t *bd,
                           const uft_schutz_probe_t *p,
                           uft_uebersprungen_grund_t *warum)
{
    if (p->umdrehungen < bd->min_umdrehungen) {
        *warum = UFT_UEBERSPRUNGEN_ZU_WENIG_UMDREHUNGEN;
        return false;
    }
    if (bd->braucht_fluss && (!p->fluss_ns || !p->fluss_anzahl)) {
        *warum = UFT_UEBERSPRUNGEN_KEIN_FLUSS;
        return false;
    }
    if (bd->braucht_index && !p->index_ns) {
        *warum = UFT_UEBERSPRUNGEN_KEIN_INDEX;
        return false;
    }
    if (bd->braucht_dekodiert && !p->dekodiert_vorhanden) {
        *warum = UFT_UEBERSPRUNGEN_NICHT_DEKODIERBAR;
        return false;
    }
    return true;
}

void uft_schutz_pruefe_alle(const uft_schutz_detektor_t *const *det,
                            size_t anzahl,
                            const uft_schutz_probe_t *probe,
                            uft_schutz_bericht_t *bericht)
{
    if (!det || !probe || !bericht) return;
    uft_schutz_ort_t ort = { probe->zylinder, probe->kopf, -1, 0, 0 };

    for (size_t i = 0; i < anzahl; i++) {
        if (!det[i] || !det[i]->pruefe) continue;
        uft_uebersprungen_grund_t warum;
        if (!bedarf_gedeckt(&det[i]->bedarf, probe, &warum)) {
            uft_schutz_bericht_uebersprungen(bericht, det[i]->code, warum, ort);
            continue;
        }
        det[i]->pruefe(det[i], probe, bericht);
    }
}

/* ── Detektor: lange/kurze Spur (LGT/SHT) ────────────────────────────── */

static int vergleich_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void spurlaenge_pruefe(const uft_schutz_detektor_t *selbst,
                              const uft_schutz_probe_t *p,
                              uft_schutz_bericht_t *bericht)
{
    (void)selbst;
    if (p->umdrehungen < 2 || p->umdrehungen > 64) return;

    /* MFM-Byte je Umdrehung: ein Byte sind 16 Zellen, eine Zelle
     * 2000 ns bei DD. Die Laenge ergibt sich damit aus der SUMME der
     * Wechselabstaende, nicht aus ihrer Zahl. */
    double laenge[64];
    size_t n = 0;
    for (size_t r = 0; r < p->umdrehungen && n < 64; r++) {
        if (!p->fluss_ns[r] || p->fluss_anzahl[r] == 0) continue;
        double ns = 0.0;
        for (size_t i = 0; i < p->fluss_anzahl[r]; i++)
            ns += (double)p->fluss_ns[r][i];
        laenge[n++] = ns / (16.0 * 2000.0);
    }
    if (n < 2) return;

    /* Der MEDIAN, nicht ein Einzelwert. Eine einzelne Messung kann an
     * einer Drehzahlschwankung des Lesegeraets haengen — und dann
     * meldete man einen Schutz, wo keiner ist. Genau deshalb steht
     * `min_umdrehungen = 2` im Bedarf. */
    qsort(laenge, n, sizeof(laenge[0]), vergleich_d);
    double mittel = (n % 2) ? laenge[n / 2]
                            : (laenge[n / 2 - 1] + laenge[n / 2]) / 2.0;

    double abw = (mittel - UFT_SCHUTZ_SPURLAENGE_NENN)
               / UFT_SCHUTZ_SPURLAENGE_NENN;
    if (fabs(abw) < UFT_SCHUTZ_SPURLAENGE_TOL) return;   /* unauffaellig */

    /* Wie viele Umdrehungen tragen den Befund mit? Gezaehlt, nicht
     * geschaetzt — das ist der ganze Sinn von `uft_schutz_halt_t`. */
    int einig = 0;
    for (size_t i = 0; i < n; i++) {
        double a = (laenge[i] - UFT_SCHUTZ_SPURLAENGE_NENN)
                 / UFT_SCHUTZ_SPURLAENGE_NENN;
        if ((a > 0) == (abw > 0) && fabs(a) >= UFT_SCHUTZ_SPURLAENGE_TOL)
            einig++;
    }

    uft_schutz_befund_t f;
    memset(&f, 0, sizeof(f));
    f.code        = (abw > 0) ? UFT_SCHUTZ_LGT : UFT_SCHUTZ_SHT;
    f.beleg       = UFT_BELEG_GEMESSEN;
    f.halt.umdrehungen_geprueft = (int)n;
    f.halt.umdrehungen_einig    = einig;
    f.ort.zylinder = p->zylinder;
    f.ort.kopf     = p->kopf;
    f.ort.sektor   = -1;
    f.messgroesse  = "spur_mfm_byte";
    f.messwert     = mittel;
    uft_schutz_bericht_add(bericht, &f);
}

const uft_schutz_detektor_t uft_schutz_detektor_spurlaenge = {
    .code = UFT_SCHUTZ_LGT,
    .bedarf = { .min_umdrehungen = 2, .braucht_fluss = true,
                .braucht_index = true, .braucht_dekodiert = false },
    .pruefe = spurlaenge_pruefe,
    .name = "Spurlaenge (LGT/SHT)"
};

static const uft_schutz_detektor_t *const EINGEBAUT[] = {
    &uft_schutz_detektor_spurlaenge,
};

const uft_schutz_detektor_t *const *uft_schutz_detektoren(size_t *anzahl)
{
    if (anzahl) *anzahl = sizeof(EINGEBAUT) / sizeof(EINGEBAUT[0]);
    return EINGEBAUT;
}
