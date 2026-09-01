/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file gen_xcopy_fixtures.c
 * @brief Fixture-Disketten für die X-Copy-Emulationssitzung (MF-771).
 *
 * Schreibt SCP-Dateien, die WinUAE und FS-UAE als Diskette einlegen
 * können. Die Fragen stehen in `docs/nachbau/XCOPY_EMULATIONSSITZUNG.md`.
 *
 * ── Warum GEMISCHTE Disketten ────────────────────────────────────────────
 *
 * Eine Diskette aus lauter Fixture-Spuren zeigt 160 rote Ziffern und sagt
 * damit weniger als eine, die **159 grüne Nullen und eine rote Ziffer**
 * zeigt: dort ist die Ziffer gegen ihre Nachbarn lesbar, und es steht
 * fest, dass X-Copy die Diskette überhaupt als Diskette annimmt. Eine
 * durchgehend unformatierte Diskette könnte X-Copy schon vor der
 * Spuranzeige abweisen — dann wäre die Sitzung ergebnislos, ohne dass man
 * wüsste warum.
 *
 * Die Fixture-Spur ist immer **Spur 40** (Zylinder 20, Kopf 0): weit genug
 * von Spur 0 (Bootblock, wo ein Fehler andere Reaktionen auslöst) und weit
 * genug vom Rand.
 *
 * ── Die Abnahme ist der eigentliche Punkt ────────────────────────────────
 *
 * Ein Fixture-Erzeuger, dessen Ausgabe niemand nachprüft, verlagert das
 * Problem nur: bei einem seltsamen Ergebnis in der Sitzung wüsste niemand,
 * ob X-Copy oder der Erzeuger schuld ist. Deshalb liest dieses Programm
 * jede geschriebene Datei mit UFTs **eigenem** SCP-Leser zurück und
 * dekodiert eine Nachbarspur mit `flux_decode_amiga()`. Erst wenn dort 11
 * Sektoren mit gültigen Prüfsummen herauskommen, gilt die Diskette als
 * brauchbar.
 *
 * ── E2 braucht eine Gegenprobe, und sie heißt E2b ────────────────────────
 *
 * Zwölf Sektoren passen bei 2000 ns **nicht** in eine Umdrehung (gemessen:
 * 104 448 Zellen nötig, 100 000 vorhanden). E2 schreibt deshalb mit
 * 1900 ns — schneller, wie es eine lange Spur als Kopierschutz tut.
 *
 * Damit ist jede Ziffer bei E2 doppeldeutig: **Sektorzahl oder Datenrate?**
 * `E2b` beantwortet das — elf Sektoren bei 1900 ns. Zeigt X-Copy dort
 * nichts und bei E2 etwas, liegt es an der Zahl; zeigt es bei beiden
 * dasselbe, an der Rate. Ohne E2b müsste die Sitzung wiederholt werden.
 *
 * Die Abnahme misst deshalb auch die **Zellendauer** der Fixture-Spur.
 * E2b trägt elf Sektoren wie jede normale Spur — ohne diese Messung wäre
 * nicht belegt, dass es überhaupt eine Gegenprobe ist.
 *
 * **E7** (Schreibstartpunkt) ist unter Emulation gar nicht beobachtbar —
 * siehe das Sitzungsblatt und P3-7 in `OPEN_ITEMS.md`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "flux_gen.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_scp_parser.h"
#include "uft/flux/uft_flux_decoder.h"
#include "uft/flux/uft_flux_histogram.h"

#define NTRACKS      160
#define FIXTURE_TRK   40
#define NACHBAR_TRK   41
#define CELL_NS       UFT_AMIGADOS_CELL_NS
#define ZELLEN        UFT_AMIGADOS_CELLS_PER_REV

/* E2 — zwoelf Sektoren passen bei Nennwert NICHT.
 *
 * Gemessen: ein Sektor belegt 8704 Zellen; elf sind 95 744 und eine
 * Umdrehung bei 2000 ns fasst 100 000. Zwoelf waeren 104 448 — der
 * Erzeuger bricht dort mit `passt=0` bei 96 096 Zellen ab. Bei 1900 ns
 * fasst dieselbe Umdrehung 105 263 Zellen, und der Dekoder liest 12.
 *
 * Das ist keine Kuenstlichkeit der Fixture: schneller schreiben, um mehr
 * unterzubringen, ist genau das, was „lange Spur" als Kopierschutz tut.
 * Nur die Fixture-Spur bekommt die kuerzere Zelle, die anderen 159
 * bleiben beim Nennwert. */
#define E2_CELL_NS    1900u
#define E2_ZELLEN     (UFT_AMIGADOS_REV_NS / E2_CELL_NS)
#define E2_SPT        12

static int fehler = 0;

/* ── Zellenströme für die Fixture-Spuren ─────────────────────────────── */

static uint32_t rnd_state = 20260901u;
static uint32_t rnd(void)
{
    rnd_state ^= rnd_state << 13; rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;  return rnd_state;
}

/* E1 — eine gelöschte Spur trägt praktisch keine Wechsel. Die Abstände
 * liegen weit jenseits jeder Zellendauer; genau daran erkennt der Baum
 * seit MF-764 „leer" statt „kein Sync". */
static size_t leere_spur(uint32_t *iv, size_t cap)
{
    size_t n = 0;
    uint64_t t = 0;
    while (n < cap && t < UFT_AMIGADOS_REV_NS) {
        uint32_t d = 40000u + (rnd() % 20000u);
        iv[n++] = d; t += d;
    }
    return n;
}

/* E4 — eine Spur, die statt $4489 durchgehend $448A trägt. Kein gültiger
 * AmigaDOS-Inhalt: die Frage ist allein, ob X-Copy das Muster überhaupt
 * als Marke annimmt. */
static size_t marken_spur(uint32_t *iv, size_t cap, uint16_t marke)
{
    size_t n = 0;
    long bitpos = 0, last = -1;
    uint64_t t = 0;
    while (n + 16 < cap && t < UFT_AMIGADOS_REV_NS) {
        for (int b = 15; b >= 0; b--) {
            long pos = bitpos + (15 - b);
            if (marke & (1u << b)) {
                if (last >= 0) {
                    uint32_t d = (uint32_t)(pos - last) * CELL_NS;
                    iv[n++] = d; t += d;
                }
                last = pos;
            }
        }
        bitpos += 16;
    }
    return n;
}

/* ── Eine ganze Diskette schreiben ───────────────────────────────────── */

typedef enum { SPUR_GUT, SPUR_LEER, SPUR_MARKE, SPUR_DEFEKT,
               SPUR_ZWOELF, SPUR_BEIDE_CHK,
               /* E2b: elf Sektoren, aber mit der SCHNELLEN Zelle.
                * Die Gegenprobe zu E2 — ohne sie ist jede Ziffer
                * dort doppeldeutig: Sektorzahl oder Datenrate? */
               SPUR_ELF_SCHNELL } spurart_t;

/* @param umdr        Umdrehungen je Spur (1 oder 2)
 * @param art         was auf FIXTURE_TRK steht
 * @param nur_umdr2   bei umdr==2: die Sonderbehandlung gilt NUR fuer
 *                    Umdrehung 2 (E3) bzw. NUR fuer Umdrehung 1 (E5) */
static int diskette_schreiben(const char *pfad, const uint8_t *adf,
                              int umdr, spurart_t art, int sonder_in_umdr)
{
    /* Der Puffer muss die LAENGSTE Spur fassen — die 12-Sektor-Spur
     * bei 1900 ns traegt mehr Zellen als eine bei 2000 ns. */
    size_t cap_max = (E2_ZELLEN > ZELLEN) ? E2_ZELLEN : ZELLEN;
    uint8_t  *cells = (uint8_t *)calloc((cap_max + 7) / 8 + 1, 1);
    uint32_t *iv    = (uint32_t *)malloc(cap_max * sizeof(uint32_t));
    if (!cells || !iv) { free(cells); free(iv); return -1; }

    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, (uint8_t)umdr);
    if (!w) { free(cells); free(iv); return -1; }

    int rc = 0;
    for (int trk = 0; trk < NTRACKS && rc == 0; trk++) {
        for (int r = 0; r < umdr && rc == 0; r++) {
            size_t n;
            int sonder = (trk == FIXTURE_TRK)
                      && (sonder_in_umdr < 0 || sonder_in_umdr == r);

            if (sonder && art == SPUR_LEER) {
                n = leere_spur(iv, ZELLEN);
            } else if (sonder && art == SPUR_MARKE) {
                n = marken_spur(iv, ZELLEN, 0x448A);
            } else {
                /* Sektor 5 ist das Ziel jedes Pruefsummen-Defekts:
                 * weit genug von Sektor 0 (den X-Copy zuerst sieht) und
                 * weit genug vom Spurende. */
                uft_amigados_defect_t d_dat   = { 5, true,  0, false };
                uft_amigados_defect_t d_beide = { 5, true,  0, true  };

                int  schnell = sonder && (art == SPUR_ZWOELF
                                       || art == SPUR_ELF_SCHNELL);
                int      spt  = (sonder && art == SPUR_ZWOELF)
                              ? E2_SPT : UFT_AMIGADOS_SPT;
                unsigned cell = schnell ? E2_CELL_NS : CELL_NS;
                size_t   cap  = UFT_AMIGADOS_REV_NS / cell;

                const uft_amigados_defect_t *def = NULL;
                if (sonder && art == SPUR_DEFEKT)     def = &d_dat;
                if (sonder && art == SPUR_BEIDE_CHK)  def = &d_beide;

                uft_amigados_cells_t c = { cells, cap, 0, 0 };
                memset(cells, 0, (cap_max + 7) / 8 + 1);
                if (!uft_amigados_build_track_n(&c, adf, (uint8_t)trk,
                                                spt, def)) {
                    /* Passt die Sektorzahl nicht in die Umdrehung, bricht
                     * der Erzeuger ab statt einen halben Sektor zu
                     * hinterlassen — und dann ist die Fixture falsch,
                     * nicht bloss knapp. */
                    rc = -2;
                    break;
                }
                n = uft_amigados_cells_to_intervals(&c, cell, iv, cap);
            }
            rc = scp_writer_add_track(w, trk / 2, trk % 2, iv, n,
                                      UFT_AMIGADOS_REV_NS, r);
        }
    }
    if (rc == 0) rc = scp_writer_save(w, pfad);

    scp_writer_free(w);
    free(cells);
    free(iv);
    return rc;
}

/* ── Abnahme: zurücklesen und eine NACHBARSPUR dekodieren ────────────── */

/* Nebenbefund, den die Abnahme braucht: die ZELLENDAUER.
 *
 * E2b traegt elf Sektoren wie jede normale Spur — der Unterschied
 * ist allein die Datenrate. Eine Abnahme, die nur Sektoren zaehlt,
 * koennte E2b nicht von einer gewoehnlichen Spur unterscheiden, und
 * die Gegenprobe waere wertlos: sie soll ja gerade zeigen, dass die
 * abweichende Rate ALLEIN keine Ziffer ausloest. */
static int gute_sektoren(const uint8_t *buf, size_t sz, int spur, int umdr,
                         int *out_kopf_fehler, double *out_cell_ns)
{
    uft_scp_track_data_t td;
    memset(&td, 0, sizeof(td));
    if (uft_scp_read_track_memory(buf, sz, spur, &td) != UFT_SCP_OK)
        return -1;

    flux_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));
    flux_status_t rs = (td.revolution_count > umdr)
        ? flux_raw_from_ns_intervals(td.revolutions[umdr].flux_data,
                                     td.revolutions[umdr].flux_count, &raw)
        : FLUX_ERR_NO_DATA;
    int sektoren = 0, gut = 0, kopf = 0;
    if (rs == FLUX_OK) {
        flux_decoded_track_t trk;
        flux_decoder_options_t o;
        memset(&trk, 0, sizeof(trk));
        memset(&o, 0, sizeof(o));
        flux_decode_amiga(&raw, &trk, &o);
        sektoren = (int)trk.sector_count;
        for (int i = 0; i < sektoren; i++) {
            if (trk.sectors[i].data_crc_ok && trk.sectors[i].id_crc_ok) gut++;
            /* Die KOPFfehler getrennt zaehlen — sonst faellt E6 (Kopf UND
             * Daten falsch) mit E5 (nur Daten falsch) auf dieselbe Zahl
             * guter Sektoren, und die Abnahme koennte die beiden Fixtures
             * nicht auseinanderhalten. */
            if (!trk.sectors[i].id_crc_ok) kopf++;
        }
        free(raw.transitions);
        free(raw.index_times);
    }

    (void)sektoren;
    if (out_kopf_fehler) *out_kopf_fehler = kopf;
    if (out_cell_ns) {
        /* `flux_data` sind ABSTAENDE, keine kumulierten Uebergangszeiten —
         * `uft_flux_histogram_cell_ns_from_transitions()` waere die falsche
         * Funktion fuer diese Datenform und lieferte still 0. */
        *out_cell_ns = 0.0;
        if (td.revolution_count > umdr) {
            uft_flux_hist_result_t h;
            memset(&h, 0, sizeof(h));
            uft_flux_histogram_analyze(td.revolutions[umdr].flux_data,
                                       td.revolutions[umdr].flux_count,
                                       100, &h);
            if (h.confident && h.cell_ns > 0.0)
                *out_cell_ns = h.cell_ns;
            else if (h.peak_count > 0)
                /* MFM: der erste Berg liegt bei 2 Zellen. */
                *out_cell_ns = h.peaks[0].center_ns / 2.0;
        }
    }
    uft_scp_free_track(&td);
    return gut;
}

/* Die Abnahme hat ZWEI Hälften, und die zweite ist die wichtigere.
 *
 * Dass die NACHBARSPUR sauber dekodiert, beweist nur: die Diskette ist
 * eine gültige AmigaDOS-Diskette. Dass die FIXTURE-SPUR abweicht,
 * beweist es nicht — eine still normal gebaute Fixture käme damit
 * durch, und die Sitzung würde eine Frage stellen, die auf der
 * Diskette gar nicht steht. Genau diese Lücke hatte die erste Fassung.
 *
 * `soll_u0` / `soll_u1` sind die erwarteten guten Sektoren auf Spur 40
 * je Umdrehung; `soll_u1 < 0` heißt „diese Fixture hat nur eine".
 *
 * Die zweite Umdrehung getrennt zu prüfen ist bei **E5** der ganze
 * Punkt: dort liegt der Defekt in Umdrehung 0 und die heile Fassung in
 * Umdrehung 1 — das ist die Lage, in der X-Copy eine Spur „rettet".
 * Eine Abnahme, die nur Umdrehung 0 ansieht, würde eine Fixture
 * durchlassen, in der auch Umdrehung 1 defekt ist, und die Frage E5
 * wäre auf der Diskette gar nicht gestellt. */
static void abnehmen(const char *pfad, const char *titel,
                     int soll_u0, int soll_u1, int soll_kopf,
                     unsigned soll_cell_ns)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) { printf("  FAIL %-34s nicht lesbar\n", titel); fehler++; return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        printf("  FAIL %-34s Lesefehler\n", titel);
        fehler++; free(buf); fclose(f); return;
    }
    fclose(f);

    int kopf = 0;
    double cell = 0.0;
    int nachbar = gute_sektoren(buf, (size_t)sz, NACHBAR_TRK, 0, NULL, NULL);
    int u0 = gute_sektoren(buf, (size_t)sz, FIXTURE_TRK, 0, &kopf, &cell);
    int u1 = (soll_u1 >= 0)
           ? gute_sektoren(buf, (size_t)sz, FIXTURE_TRK, 1, NULL, NULL) : -1;
    free(buf);

    /* Die Zellendauer nur prüfen, wo sie die Frage trägt.
     *
     * E2b trägt elf Sektoren wie jede normale Spur — der Unterschied ist
     * allein die Datenrate. Eine Abnahme, die nur Sektoren zählt, könnte
     * E2b nicht von einer gewöhnlichen Spur unterscheiden, und die
     * Gegenprobe wäre wertlos: sie soll ja gerade zeigen, dass die
     * abweichende Rate **allein** keine Ziffer auslöst.
     *
     * Toleranz 3 %: das Histogramm bestimmt die Zelle aus
     * Abstandsschwerpunkten, nicht aus dem Sollwert. */
    int cell_ok = (soll_cell_ns == 0)
               || (cell > 0.0
                   && fabs(cell - soll_cell_ns) <= soll_cell_ns * 0.03);

    int ok = (nachbar == 11) && (u0 == soll_u0) && (u1 == soll_u1)
          && (kopf == soll_kopf) && cell_ok;
    if (!ok) fehler++;
    printf("  %s %-30s Nachbar %2d  Spur40 u0=%2d u1=%2d kopf=%d cell=%4.0f"
           "  (soll %2d/%2d/%d/%u)\n",
           ok ? "ok  " : "FAIL", titel, nachbar, u0, u1, kopf, cell,
           soll_u0, soll_u1, soll_kopf, soll_cell_ns);
    (void)sz;
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    const char *out = (argc > 1) ? argv[1] : ".";
    char pfad[512];

    printf("gen_xcopy_fixtures (MF-771) -> %s\n", out);

    size_t adf_bytes = (size_t)NTRACKS * 11 * 512;
    uint8_t *adf = (uint8_t *)malloc(adf_bytes);
    if (!adf) return 2;
    uft_amigados_fill_pattern(adf, adf_bytes);

    struct { const char *datei; const char *titel;
             int umdr; spurart_t art; int sonder_in; int soll_u0, soll_u1, soll_kopf; unsigned soll_cell; } plan[] = {
      { "E1_leere_spur.scp",     "E1  Spur 40 leer",              1, SPUR_LEER,   -1,  0, -1, 0,    0 },
      { "E3a_kein_sync_u2.scp",  "E3a Sync fehlt in Umdrehung 2", 2, SPUR_LEER,    1, 11,  0, 0,    0 },
      { "E3b_eine_umdrehung.scp","E3b nur eine Umdrehung",        1, SPUR_GUT,    -1, 11, -1, 0,    0 },
      { "E4_marke_448A.scp",     "E4  Spur 40 traegt $448A",      1, SPUR_MARKE,  -1,  0, -1, 0,    0 },
      { "E5_gerettet.scp",       "E5  Umdr. 1 defekt, 2 heil",    2, SPUR_DEFEKT,  0, 10, 11, 0,    0 },
      { "E2_zwoelf_sektoren.scp", "E2  Spur 40 mit 12 Sektoren",   1, SPUR_ZWOELF, -1, 12, -1, 0, 1900 },
      { "E6_kopf_und_daten.scp",  "E6  Kopf UND Daten falsch",     1, SPUR_BEIDE_CHK, -1, 10, -1, 1,    0 },
      { "E2b_elf_bei_1900ns.scp", "E2b elf Sektoren, 1900 ns",     1, SPUR_ELF_SCHNELL, -1, 11, -1, 0, 1900 },
    };

    for (size_t i = 0; i < sizeof(plan) / sizeof(plan[0]); i++) {
        snprintf(pfad, sizeof(pfad), "%s/%s", out, plan[i].datei);
        if (diskette_schreiben(pfad, adf, plan[i].umdr,
                               plan[i].art, plan[i].sonder_in) != 0) {
            printf("  FAIL %-34s Schreiben fehlgeschlagen\n", plan[i].titel);
            fehler++;
            continue;
        }
        abnehmen(pfad, plan[i].titel, plan[i].soll_u0, plan[i].soll_u1,
                 plan[i].soll_kopf, plan[i].soll_cell);
    }

    free(adf);
    printf("%s (%d Fehler)\n", fehler ? "FAIL" : "PASS", fehler);
    return fehler ? 1 : 0;
}
