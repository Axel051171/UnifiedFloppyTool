/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * test_geometrie_kollision_schweigt_nicht.c — P3-182 / MF-921.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  DER BEFUND
 * ══════════════════════════════════════════════════════════════════════
 *
 * Tor 58 (MF-906) misst Geometrietabellen, deren Sonden bei der ERSTEN
 * passenden Zeile zurueckkehren. Steht dieselbe Gesamtgroesse zweimal,
 * wird das Abbild still als die **erste** Geometrie ausgegeben, obwohl
 * die Groesse die beiden nicht unterscheidet. Gemessen ueber alle
 * `src/**\/*.c`: 14 Tabellen dieser Gestalt, **3 Kollisionen**:
 *
 *   g_heathkit_geom   409600   40T/2H/512  gegen  80T/1H/512
 *   g_hitachi_geom    655360   80T/16S/256 gegen  80T/8S/512
 *   g_bk_geom         409600   80T/1H/512  gegen  40T/2H/512
 *
 * Das ist dieselbe Form wie die Guard-Kollision aus MF-881: **eine
 * Auswahl, die entscheidet, wo sie nicht entscheiden kann.**
 *
 * ── Warum das bei einem Forensik-Werkzeug zaehlt ────────────────────
 *
 * 409600 Byte koennen 40 Spuren doppelseitig ODER 80 Spuren einseitig
 * sein. Aus der Datei allein ist das **nicht entscheidbar** — die
 * Information steht auf dem Traeger, nicht im Abzug. Wer trotzdem eine
 * der beiden ausgibt, hat in der Haelfte der Faelle recht und sagt in
 * beiden Faellen dasselbe.
 *
 * ── Der Vertrag, den dieser Test durchsetzt ─────────────────────────
 *
 *   1. Die BYTES bleiben. Ein unklarer Aufbau ist kein Grund, den
 *      Abzug wegzuwerfen — „kein Bit verloren".
 *   2. Die GEOMETRIE wird nicht erfunden. Ist sie aus der Groesse
 *      nicht ableitbar, steht dort die **0** — die Nicht-gemessen-
 *      Kennung dieses Baums —, nicht die erstbeste Zeile.
 *   3. Die SONDE bleibt im Band „nur die Groesse" (30..49, MF-729).
 *      Sie hat nichts gelesen, was ueber die Dateilaenge hinausgeht.
 *
 * ── Was hier NICHT behauptet wird ───────────────────────────────────
 *
 * Alle drei Module sind Waisen (`docs/orphan_baseline.txt`): kein
 * Aufrufer ausserhalb der eigenen Datei, nicht in der Plugin-Liste.
 * **Kein Benutzer trifft sie heute.** Dieser Test misst also einen
 * Vertrag, keinen Vorfall — und genau darum ist er billig und
 * sinnvoll: er haelt die Kollision fest, BEVOR eine dieser Dateien
 * verdrahtet wird.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_heathkit.h"
#include "uft/formats/uft_hitachi_s1.h"
#include "uft/formats/uft_bk0010.h"

static int g_ok = 0;
static void ok(const char *n) { g_ok++; printf("  OK  %s\n", n); }

/* Ein Abzug der genannten Groesse, mit erkennbarem Inhalt. */
static uint8_t *abzug(size_t n)
{
    uint8_t *b = malloc(n);
    assert(b != NULL);
    for (size_t i = 0; i < n; i++) b[i] = (uint8_t)((i * 31 + 7) & 0xFF);
    return b;
}

static void schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    assert(f != NULL);
    assert(fwrite(b, 1, n, f) == n);
    fclose(f);
}

/* ═════════ 1. BK-0010: die Sonde gibt die Geometrie heraus ═════════ */

/* Ein Abzug, der wie eine ECHTE RT-11-Diskette aussieht — sonst misst
 * die Konfidenz-Pruefung unten nichts.
 *
 * MF-921, aus dem Mutations-Gegenbeweis gelernt: mit einem beliebigen
 * Muster blieb `uft_bk0010_probe()` ohnehin unter 50, und der Deckel
 * liess sich ersatzlos entfernen, ohne dass die Pruefung fiel. Eine
 * Zusicherung, die den geprueften Zweig nie erreicht, ist keine.
 *
 * Die drei Zugaben, die die Sonde vergibt, werden deshalb gezielt
 * ausgeloest: Pack-Cluster im Bereich 1..16 (+20), druckbare
 * Datentraegerkennung (+10), genug Nicht-Null-Bytes (+10). 35 + 40 = 75
 * — ohne Deckel laege die Zahl weit im Band „Struktur gelesen". */
static uint8_t *abzug_rt11(size_t n)
{
    uint8_t *b = abzug(n);
    uint8_t *home = b + 512;                 /* Block 1 */
    home[0x1C0] = 0x01; home[0x1C1] = 0x00;  /* Pack-Cluster = 1 */
    for (int j = 0; j < 12; j++) home[0x1F0 + j] = (uint8_t)('A' + j);
    return b;
}

static void t_bk_mehrdeutig(void)
{
    /* 409600 steht ZWEIMAL in g_bk_geom: 80T/1H und 40T/2H. */
    uint8_t *b = abzug_rt11(409600);
    int tracks = -1, heads = -1;
    bk_dos_type_t dos = (bk_dos_type_t)0;

    int conf = uft_bk0010_probe(b, 409600, &tracks, &heads, &dos);

    if (tracks != 0 || heads != 0) {
        printf("  FEHLER: die Sonde gibt %dT/%dH aus, obwohl 409600 Byte "
               "in g_bk_geom ZWEIMAL stehen (80T/1H und 40T/2H) — sie "
               "entscheidet, wo sie nicht entscheiden kann\n", tracks, heads);
        free(b);
        assert(0);
    }
    if (conf >= 50) {
        printf("  FEHLER: Konfidenz %d — das Band ab 50 heisst 'Struktur "
               "gelesen' (MF-729), gelesen wurde aber nur die Dateilaenge\n",
               conf);
        free(b);
        assert(0);
    }
    free(b);
    ok("BK: 409600 Byte -> Geometrie 0/0, Konfidenz im Groessen-Band");
}

static void t_bk_eindeutig_bleibt(void)
{
    /* 819200 steht nur EINMAL — hier MUSS die Geometrie herauskommen.
     * Ohne diese Gegenprobe waere „gib immer 0 aus" ein gruener Fix. */
    uint8_t *b = abzug(819200);
    int tracks = -1, heads = -1;
    bk_dos_type_t dos = (bk_dos_type_t)0;
    (void)uft_bk0010_probe(b, 819200, &tracks, &heads, &dos);
    if (tracks != 80 || heads != 2) {
        printf("  FEHLER: 819200 ist eindeutig (80T/2H), die Sonde gibt "
               "aber %dT/%dH — die Erkennung ist zu weit zurueckgenommen\n",
               tracks, heads);
        free(b);
        assert(0);
    }
    free(b);
    ok("BK: 819200 Byte bleibt eindeutig 80T/2H (Gegenprobe)");
}

/* ═════════ 2. Heathkit: der Leser setzt die Geometrie ═════════════ */

static void t_heathkit_mehrdeutig(void)
{
    const char *pfad = "test_kollision_heathkit.img";
    uint8_t *b = abzug(409600);
    schreibe(pfad, b, 409600);
    free(b);

    uft_heathkit_image_t *img = NULL;
    int rc = uft_heathkit_read(pfad, &img);
    remove(pfad);
    assert(rc == 0 && img != NULL);

    /* Die Bytes bleiben — das ist Bedingung 1. */
    assert(img->data != NULL);
    assert(img->size == 409600);

    if (img->tracks != 0 || img->heads != 0) {
        printf("  FEHLER: der Leser meldet %dT/%dH, obwohl 409600 in "
               "g_heathkit_geom ZWEIMAL steht (40T/2H und 80T/1H)\n",
               img->tracks, img->heads);
        assert(0);
    }
    free(img->data); free(img);
    ok("Heathkit: 409600 Byte -> Daten erhalten, Geometrie 0/0");
}

static void t_heathkit_eindeutig_bleibt(void)
{
    const char *pfad = "test_kollision_heathkit2.img";
    uint8_t *b = abzug(819200);
    schreibe(pfad, b, 819200);
    free(b);

    uft_heathkit_image_t *img = NULL;
    int rc = uft_heathkit_read(pfad, &img);
    remove(pfad);
    assert(rc == 0 && img != NULL);
    if (img->tracks != 80 || img->heads != 2) {
        printf("  FEHLER: 819200 ist eindeutig (80T/2H), gelesen wurde "
               "%dT/%dH\n", img->tracks, img->heads);
        assert(0);
    }
    free(img->data); free(img);
    ok("Heathkit: 819200 Byte bleibt eindeutig 80T/2H (Gegenprobe)");
}

/* ═════════ 3. Hitachi S1 ══════════════════════════════════════════ */

static void t_hitachi_mehrdeutig(void)
{
    const char *pfad = "test_kollision_hitachi.img";
    uint8_t *b = abzug(655360);
    schreibe(pfad, b, 655360);

    int conf = uft_hitachi_s1_probe(b, 655360);
    free(b);
    if (conf >= 50) {
        printf("  FEHLER: Konfidenz %d fuer eine Groesse, die in "
               "g_hitachi_geom ZWEIMAL steht (16S/256 und 8S/512)\n", conf);
        remove(pfad);
        assert(0);
    }

    uft_hitachi_s1_image_t *img = NULL;
    int rc = uft_hitachi_s1_read(pfad, &img);
    remove(pfad);
    assert(rc == 0 && img != NULL);
    assert(img->data != NULL && img->size == 655360);

    if (img->sectors != 0 || img->sector_size != 0) {
        printf("  FEHLER: der Leser meldet %d Sektoren zu %d Byte, obwohl "
               "655360 sowohl 16x256 als auch 8x512 sein kann\n",
               img->sectors, img->sector_size);
        assert(0);
    }
    free(img->data); free(img);
    ok("Hitachi: 655360 Byte -> Daten erhalten, Sektorangaben 0");
}

static void t_hitachi_eindeutig_bleibt(void)
{
    uint8_t *b = abzug(256256);         /* 77T/26S/128 — nur einmal */
    int conf = uft_hitachi_s1_probe(b, 256256);
    free(b);
    if (conf <= 0) {
        printf("  FEHLER: eine eindeutige Groesse (256256) wird gar nicht "
               "mehr beansprucht (Konfidenz %d)\n", conf);
        assert(0);
    }
    ok("Hitachi: 256256 Byte bleibt beansprucht (Gegenprobe)");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("test_geometrie_kollision_schweigt_nicht — P3-182 (MF-921)\n");
    t_bk_mehrdeutig();
    t_bk_eindeutig_bleibt();
    t_heathkit_mehrdeutig();
    t_heathkit_eindeutig_bleibt();
    t_hitachi_mehrdeutig();
    t_hitachi_eindeutig_bleibt();
    printf("%d Pruefungen gruen\n", g_ok);
    return 0;
}
