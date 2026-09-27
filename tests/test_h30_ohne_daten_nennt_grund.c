/* SPDX-License-Identifier: MIT */
/**
 * @file test_h30_ohne_daten_nennt_grund.c
 * @brief H-30 Schritt 3: kein Sektor ohne Daten ohne Grund (MF-1383)
 *
 * ── DIE OFFENE HAELFTE EINER REGEL ───────────────────────────────────
 *
 * H-30 Schritt 1 (MF-1370) hat die Regel „Abwesenheit traegt keine
 * Daten" an der einzigen Tuer ins Modell erzwungen:
 *
 *     uft_d2_add_sector():
 *       if (uft_d2_origin_is_absence(s->origin)
 *           && (s->has_data || s->data_len || s->conf))  -> ABSENCE_WITH_DATA
 *
 * Schritt 2 (MF-1371) hat 37 Kennzeichnungen in 34 Dateien dazu
 * gebracht, ihren Grund zu NENNEN, und Tor 62 haelt das fest.
 *
 * Die UMKEHRUNG haelt niemand: **keine Daten muss einen Grund nennen.**
 * Ein Sektor mit `has_data = false`, `data_len = 0`, `conf = 0` und
 * Herkunft `CONTAINER` kommt lautlos durch — das Modell fuehrt ihn als
 * „aus einer Abbilddatei", und es steht nichts darin. Gemessen an der
 * Bruecke ist der Weg dorthin offen:
 *
 *     src/core/uft_disk2_bridge.c
 *       out->has_data = !missing && out->data_len > 0u;
 *       if (!missing) out->origin = UFT_D2_ORIGIN_CONTAINER;
 *
 * Ein Sektor, den das Plugin NICHT als fehlend kennzeichnet, dessen
 * Datenzeiger aber NULL oder dessen Laenge 0 ist, landet genau dort.
 *
 * Warum das mehr ist als eine Unsauberkeit: die ganze H-30-Arbeit
 * existiert, damit ein Bediener „leer", „unbelegt", „fehlend" und
 * „ausgelassen" unterscheiden KANN. Ein Sektor ohne Daten und ohne
 * Grund ist in dieser Unterscheidung nicht vorhanden — er sieht aus wie
 * ein gelesener Sektor, der nichts enthielt, und genau das ist die
 * Aussage, die niemand belegen kann.
 *
 * ── WAS DIESER TEST MISST ────────────────────────────────────────────
 *
 * 1. Die Regel an der Tuer: ein Sektor ohne Daten mit einer
 *    NICHT-Abwesenheits-Herkunft wird abgewiesen, mit eigenem Befund.
 * 2. Die Gegenrichtung bleibt erlaubt: mit Daten und Herkunft
 *    `CONTAINER` geht durch; ohne Daten MIT Grund geht durch.
 * 3. Am Korpus: kein echtes Abbild erzeugt so einen Sektor. Diese
 *    Zusicherung ist NICHT geraten — der erste Lauf dieses Tests hat
 *    sie gemessen, bevor die Regel scharf gestellt wurde (siehe unten).
 *
 * ── DIE MESSUNG VOR DER REGEL ────────────────────────────────────────
 *
 * Reihenfolge nach der Hausregel „Messung vor Plan": der Korpusteil
 * lief zuerst als BERICHT, nicht als Zusicherung. Haetten echte
 * Plugins Sektoren ohne Daten und ohne Grund erzeugt — etwa einen
 * gefundenen Sektorkopf ohne Datenfeld —, waere eine Abweisung falsch
 * gewesen und die richtige Antwort ein eigener Grund fuer diesen Fall.
 * Das Ergebnis steht in `UFT_H30_S3_GEMESSEN` im Kopf des Korpusteils.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"   /* vor uft_core.h — dessen Prototypen
                                      * nennen struct uft_format_plugin */
#include "uft/uft_core.h"
#include "uft/uft_types.h"
#include "uft/core/uft_disk2.h"
#include "uft/core/uft_disk2_bridge.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; \
    printf("  FEHLGESCHLAGEN Zeile %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Ein Sektor, der alles richtig macht — Abweichungen setzt der Aufrufer. */
static void sektor_grundform(uft_d2_sector_t *s, uint8_t *daten, uint32_t len)
{
    memset(s, 0, sizeof(*s));
    s->id_cyl = 0; s->id_head = 0; s->id_sec = 1; s->id_size_code = 2;
    s->id_crc_known = false;
    s->data = daten;
    s->data_len = len;
    s->has_data = (daten != NULL && len != 0u);
    s->dam = 0xFBu;
    s->data_crc_known = false;
    s->idam_bit = s->dam_bit = s->data_end_bit = SIZE_MAX;
    s->encoding = UFT_ENC_UNKNOWN;
    s->origin = UFT_D2_ORIGIN_CONTAINER;
    s->conf = (daten != NULL && len != 0u) ? UFT_D2_CONF_UNVERIFIED
                                           : UFT_D2_CONF_NONE;
}

/* Zaehlt Befunde mit einer bestimmten Kennung. */
static size_t befunde_mit(const uft_disk2_t *d, const char *kennung)
{
    size_t n = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); i++) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        if (g && g->code && strcmp(g->code, kennung) == 0) n++;
    }
    return n;
}

/* ─────────────────────────────────────────────────────────────────────
 *  1. Die Regel an der Tuer.
 * ───────────────────────────────────────────────────────────────────── */
static void t_tuer(void)
{
    printf("1. die Tuer ins Modell\n");

    uint8_t daten[512];
    memset(daten, 0xE5, sizeof(daten));

    /* (a) mit Daten, Herkunft CONTAINER — muss durchgehen. */
    {
        uft_disk2_t *d = uft_d2_create();
        CHECK(d != NULL, "uft_d2_create lieferte NULL");
        if (!d) return;
        uft_d2_track_t *t = uft_d2_track(d, 0, 0);
        uft_d2_sector_t s;
        sektor_grundform(&s, daten, (uint32_t)sizeof(daten));
        CHECK(uft_d2_add_sector(d, t, &s),
              "ein gewoehnlicher Sektor mit Daten wurde abgewiesen");
        uft_d2_destroy(d);
    }

    /* (b) ohne Daten, MIT Grund — muss durchgehen (Schritt 1). */
    {
        uft_disk2_t *d = uft_d2_create();
        if (!d) { CHECK(0, "uft_d2_create lieferte NULL"); return; }
        uft_d2_track_t *t = uft_d2_track(d, 0, 0);
        uft_d2_sector_t s;
        sektor_grundform(&s, NULL, 0u);
        s.origin = UFT_D2_ORIGIN_UNAVAILABLE;
        CHECK(uft_d2_add_sector(d, t, &s),
              "Abwesenheit MIT Grund wurde abgewiesen");
        uft_d2_destroy(d);
    }

    /* (c) DIE ZEILE: ohne Daten, OHNE Grund — muss abgewiesen werden.
     *
     * Vor MF-1383 gab `uft_d2_add_sector()` hier `true` zurueck und
     * schrieb keinen einzigen Befund. Das Modell fuehrte den Sektor als
     * „aus einer Abbilddatei" mit nichts darin. */
    {
        uft_disk2_t *d = uft_d2_create();
        if (!d) { CHECK(0, "uft_d2_create lieferte NULL"); return; }
        uft_d2_track_t *t = uft_d2_track(d, 0, 0);
        uft_d2_sector_t s;
        sektor_grundform(&s, NULL, 0u);
        s.origin = UFT_D2_ORIGIN_CONTAINER;    /* kein Grund */
        CHECK(!uft_d2_add_sector(d, t, &s),
              "ein Sektor OHNE Daten und OHNE Grund wurde angenommen");
        CHECK(befunde_mit(d, "ABSENCE_WITHOUT_REASON") == 1u,
              "erwartet 1 Befund ABSENCE_WITHOUT_REASON, gemessen %u",
              (unsigned)befunde_mit(d, "ABSENCE_WITHOUT_REASON"));
        uft_d2_destroy(d);
    }

    /* (d) dieselbe Lage mit PADDING — Fuellmaterial IST ein Grund, aber
     *     Fuellmaterial ohne Daten ist keines: es gibt nichts zu fuellen.
     *     Auch das muss fallen. */
    {
        uft_disk2_t *d = uft_d2_create();
        if (!d) { CHECK(0, "uft_d2_create lieferte NULL"); return; }
        uft_d2_track_t *t = uft_d2_track(d, 0, 0);
        uft_d2_sector_t s;
        sektor_grundform(&s, NULL, 0u);
        s.origin = UFT_D2_ORIGIN_PADDING;
        CHECK(!uft_d2_add_sector(d, t, &s),
              "PADDING ohne Daten wurde angenommen — es gibt nichts zu "
              "fuellen");
        uft_d2_destroy(d);
    }
}

/* ─────────────────────────────────────────────────────────────────────
 *  2. Am Korpus — die Messung, die der Regel vorausging.
 *
 *  UFT_H30_S3_GEMESSEN: siehe die Ausgabe dieses Abschnitts. Der erste
 *  Lauf lief als Bericht; die Zusicherung `== 0` steht erst da, seit
 *  die Zahl gemessen ist.
 * ───────────────────────────────────────────────────────────────────── */
static void t_korpus(void)
{
    printf("\n2. am Korpus: erzeugt ein echtes Abbild so einen Sektor?\n");

    /* Die Dateimenge kommt aus dem VERZEICHNIS, nicht aus einer Liste im
     * Quelltext (MF-636). Eine gepflegte Zweierliste haette genau die
     * Abbilder geprueft, die der Autor im Sinn hatte — ein neues
     * Korpusabbild entzieht sich ihr still. */
    DIR *dir = opendir(UFT_CORPUS_DIR);
    CHECK(dir != NULL, "Korpusverzeichnis nicht lesbar: %s", UFT_CORPUS_DIR);
    if (!dir) return;

    size_t gesehen = 0, geoeffnet = 0, summe_ohne_grund = 0;
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char pfad[1024];
        snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, e->d_name);
        gesehen++;

        uft_disk_t *disk = uft_disk_open(pfad, true);
        if (!disk) {
            /* Kein Fehlschlag: `corpus_free` enthaelt auch Nicht-Abbilder
             * (Manifeste, Beschreibungen). Gezaehlt wird, wie viele
             * wirklich geoeffnet wurden — bleibt die Zahl 0, faellt der
             * Abschnitt, statt still zu schweigen. */
            continue;
        }
        geoeffnet++;
        uft_disk2_t *d = uft_d2_create();
        if (!d) { uft_disk_close(disk); CHECK(0, "uft_d2_create"); continue; }

        uft_d2_bridge_stats_t st;
        memset(&st, 0, sizeof(st));
        const bool ok = uft_d2_from_disk(d, disk, uft_disk_plugin(disk), &st);

        size_t sektoren = 0, ohne_daten = 0, ohne_grund = 0;
        for (size_t i = 0; i < uft_d2_track_count(d); i++) {
            const uft_d2_track_t *t = uft_d2_track_at(d, i);
            if (!t || !t->has_sectors) continue;
            for (size_t j = 0; j < t->sectors.count; j++) {
                const uft_d2_sector_t *s = &t->sectors.items[j];
                sektoren++;
                if (s->has_data || s->data_len) continue;
                ohne_daten++;
                if (!uft_d2_origin_is_absence(s->origin)) ohne_grund++;
            }
        }
        printf("     %-40s %s  %5zu Sektoren, %3zu ohne Daten, "
               "%3zu OHNE GRUND\n",
               e->d_name, ok ? "ok " : "rot", sektoren, ohne_daten,
               ohne_grund);

        /* MESSUNG (P3-599, MF-1431): sagt die Bruecke ab, nennt das Modell den
         * Grund in seinen Befunden. Sie auszugeben ist der Schritt, den
         * der Registereintrag vor jeder Aenderung verlangt. */
        if (!ok) {
            printf("        Bruecke sagt ab — %zu Befund(e), %zu Spuren:\n",
                   uft_d2_diag_count(d), uft_d2_track_count(d));
            for (size_t q = 0; q < uft_d2_diag_count(d) && q < 12u; q++) {
                const uft_d2_diag_t *g = uft_d2_diag_at(d, q);
                if (!g) continue;
                printf("        [%s] C%d H%d S%d: %s\n",
                       g->code ? g->code : "?", g->cyl, g->head, g->sector,
                       g->text ? g->text : "");
            }
        }
        summe_ohne_grund += ohne_grund;

        uft_d2_destroy(d);
        uft_disk_close(disk);
    }
    closedir(dir);

    printf("     -> %zu Einträge gesehen, %zu als Abbild geöffnet\n",
           gesehen, geoeffnet);

    /* Ein Abschnitt, der nichts geoeffnet hat, hat nichts geprueft — und
     * das darf nicht wie „bestanden" aussehen (MF-1340). */
    CHECK(geoeffnet > 0u,
          "kein einziges Korpusabbild geoeffnet (%zu Einträge gesehen) — "
          "dieser Abschnitt hat NICHTS geprueft", gesehen);
    CHECK(summe_ohne_grund == 0u,
          "%zu Sektoren im Korpus ohne Daten und ohne Grund",
          summe_ohne_grund);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_h30_ohne_daten_nennt_grund (MF-1383) ===\n\n");
    if (uft_register_all_formats() != UFT_OK) {
        printf("uft_register_all_formats scheiterte\n");
        return 1;
    }
    t_tuer();
    t_korpus();
    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
