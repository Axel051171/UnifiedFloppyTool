/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_convert_imd_img_belegt.c
 * @brief IMD -> IMG: der Beleg, den der Matrix-Eintrag verlangt (MF-1277)
 *
 * ── WARUM ES DIESEN TEST GIBT ────────────────────────────────────────────
 *
 * Gemessen vor MF-1277 fuehrte `g_conversion_paths[]` **46** Wandlungs-
 * pfade und `g_matrix[]` **17** Eintraege — **29** Pfade waren gebaut und
 * wurden vom Preflight abgewiesen:
 *
 *     Preflight ABORT: conversion pair is UNTESTED — not offered until an
 *     entry is added to the round-trip matrix with proof
 *
 * Das ist die Bauart aus MF-263/UFT-A01 und richtig. Gemessen hilft auch
 * `accept_data_loss` nicht: ein UNGEPRUEFTES Paar wird zu keinem Preis
 * angeboten. Der einzige Weg ist ein Eintrag MIT Beleg — und dieser Test
 * ist der Beleg.
 *
 * ── DIE MESSUNG ──────────────────────────────────────────────────────────
 *
 *     quelle : hxcfe_pc160.imd  164 785 Byte, von HxC erzeugt
 *     imd    : 40 Spuren, 40 Zylinder, 1 Kopf, 0 defekte, 0 fehlende
 *     wandler: 163 840 Byte, 40 Spuren
 *     gegen uft_pc160.img: 0 von 163 840 Byte abweichend
 *
 * **Warum das keine Gleichheit ohne Aussage ist** (MF-1039): die Quelle
 * kommt aus FREMDER Hand, und ein flaches Sektorabbild von
 * 163 840 = 40 x 1 x 8 x 512 Byte hat keine Wahlfreiheit — es IST der
 * Sektorinhalt. Dass HxCs IMD-Kodierung ueber unseren Leser byteweise
 * dieselben Nutzdaten ergibt, ist eine Aussage ueber den Leser.
 *
 * ── WARUM LOSSY_DOCUMENTED UND NICHT LOSSLESS ────────────────────────────
 *
 * Weil diese eine Diskette 0 defekte und 0 fehlende Sektoren hat, die
 * Verlustposten aber trotzdem anfallen. Neun sind es, ablesbar an
 * `uft_imd_image_t` — und der neunte ist der forensisch wichtige:
 * fehlende Sektoren werden mit `0xE5` gefuellt und sind danach von
 * echten `0xE5`-Sektoren nicht mehr zu unterscheiden. Der Wandler ZAEHLT
 * und MELDET sie; die Zieldatei kann den Unterschied nicht tragen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* `uft_format_plugin.h` ZUERST: `uft_core.h` nennt `struct
 * uft_format_plugin` und `struct uft_probe_ranking` in Parameterlisten,
 * und ohne vorherige Deklaration warnt gcc, dass ihr Gueltigkeitsbereich
 * dort endet. */
#include "uft/uft_format_plugin.h"
#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"
#include "uft/core/uft_roundtrip.h"
#include "uft/formats/uft_imd.h"
/* Der Wandler selbst — Gruppe 5 prueft das Fuellbyte ohne den Umweg
 * ueber die Datei-API, weil sie eine IMD im SPEICHER baut. */
#include "uft_format_convert_internal.h"

static int _fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); _fail++; } } while (0)

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es fuer diesen Test setzen"
#endif
#define QUELLE    UFT_CORPUS_DIR "/hxcfe_pc160.imd"
#define VERGLEICH UFT_CORPUS_DIR "/uft_pc160.img"

/* Gemessen, nicht gerundet. */
#define ROH_BYTE   163840u        /* 40 x 1 x 8 x 512 */
#define ROH_SPUREN 40

static uint8_t *lies(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long L = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (L <= 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)L);
    if (!b) { fclose(f); return NULL; }
    *n = fread(b, 1, (size_t)L, f);
    fclose(f);
    return b;
}

/* ═══════════ 1. Das Paar steht in der Matrix, und zwar als LD ═══════ */

static void t1_matrix(void)
{
    printf("Test 1: das Paar IMD->IMG steht als LOSSY-DOCUMENTED in der Matrix\n");
    const uft_roundtrip_status_t s =
        uft_roundtrip_status((uft_format_id_t)UFT_FORMAT_IMD,
                             (uft_format_id_t)UFT_FORMAT_IMG);
    CHECK(s != UFT_RT_UNTESTED,
          "vor MF-1277 war das Paar UNGEPRUEFT und wurde vom Preflight "
          "abgewiesen; jetzt muss ein Eintrag da sein");
    CHECK(s == UFT_RT_LOSSY_DOCUMENTED,
          "und zwar LOSSY-DOCUMENTED, nicht LOSSLESS — die Pruefdiskette "
          "hat 0 defekte Sektoren, die Metadaten gehen trotzdem verloren; "
          "gemessen: %s", uft_roundtrip_status_string(s));
    printf("    Status: %s\n", uft_roundtrip_status_string(s));
}

/* ═══════════ 2. Die Notiz NENNT die Verlustposten ══════════════════ */

static void t2_notiz_nennt_verlust(void)
{
    printf("Test 2: die Notiz nennt, WAS verloren geht\n");
    const char *n = uft_roundtrip_note((uft_format_id_t)UFT_FORMAT_IMD,
                                       (uft_format_id_t)UFT_FORMAT_IMG);
    CHECK(n != NULL && n[0] != '\0', "es gibt eine Notiz");
    if (!n) return;

    /* Ein Eintrag ohne benannten Verlust ist genau die Sorte Zusage, die
     * dieser Baum bei SCP<->HFE einmal teuer bezahlt hat (MF-527). Die
     * Stichworte stehen fuer die neun Posten aus `uft_imd_image_t`. */
    static const char *muss[] = { "0xE5", "smap", "mode", "stype", "Kommentar" };
    for (size_t i = 0; i < sizeof(muss) / sizeof(muss[0]); ++i)
        CHECK(strstr(n, muss[i]) != NULL,
              "die Notiz muss \"%s\" nennen — sonst ist der Verlust "
              "behauptet und nicht dokumentiert.\nNotiz: %s", muss[i], n);
    printf("    Notiz nennt die Posten (%zu Zeichen)\n", strlen(n));
}

/* ═══════════ 3. Die Wandlung laeuft — und liefert die Bytes ════════ */

static void t3_wandlung_laeuft(void)
{
    printf("Test 3: gesperrt ohne Zustimmung, laeuft mit Zustimmung\n");
    const char *ziel = "uft_imd_img_belegt_out.img";
    remove(ziel);

    /* ERSTE HAELFTE: ohne `accept_data_loss` bleibt es gesperrt.
     *
     * Das ist die Stufe, auf die der Eintrag das Paar gehoben hat. Vor
     * MF-1277 hiess die Absage „conversion pair is UNTESTED" und galt zu
     * JEDEM Preis; jetzt heisst sie „requires accept_data_loss=true" und
     * ist eine Frage an den Bediener. Wer den Verlust nicht ausdruecklich
     * annimmt, bekommt die Datei nicht — Prinzip 1 aus
     * `docs/DESIGN_PRINCIPLES.md`. */
    {
        uft_convert_options_t ohne = uft_convert_default_options();
        ohne.accept_data_loss = false;
        uft_convert_result_t r0;
        memset(&r0, 0, sizeof(r0));
        const uft_error_t rc0 = uft_convert_file(QUELLE, ziel,
                                                 UFT_FORMAT_IMG, &ohne, &r0);
        CHECK(rc0 != UFT_OK,
              "ohne accept_data_loss muss ein verlustbehafteter Pfad "
              "gesperrt bleiben; rc=%d", (int)rc0);
        remove(ziel);
    }

    /* ZWEITE HAELFTE: mit Zustimmung laeuft die Wandlung. */
    uft_convert_options_t opt = uft_convert_default_options();
    opt.accept_data_loss = true;
    uft_convert_result_t res;
    memset(&res, 0, sizeof(res));

    const uft_error_t rc = uft_convert_file(QUELLE, ziel,
                                            UFT_FORMAT_IMG, &opt, &res);
    CHECK(rc == UFT_OK,
          "vor MF-1277 antwortete der Preflight mit -40 (UNGEPRUEFT) und "
          "liess sich durch nichts umstimmen; jetzt muss die Wandlung MIT "
          "Zustimmung laufen. rc=%d, erste Warnung: %s",
          (int)rc, res.warning_count ? res.warnings[0] : "(keine)");
    CHECK(res.success, "und success melden");
    CHECK(res.tracks_converted == ROH_SPUREN,
          "40 Spuren hat die Diskette, gewandelt %d", res.tracks_converted);

    size_t n = 0, vn = 0;
    uint8_t *aus = lies(ziel, &n);
    uint8_t *ver = lies(VERGLEICH, &vn);
    CHECK(aus != NULL, "die Zieldatei ist da");
    CHECK(n == ROH_BYTE, "%u Byte erwartet, %zu geschrieben", ROH_BYTE, n);
    CHECK(ver != NULL && vn == ROH_BYTE,
          "der Vergleich uft_pc160.img hat %u Byte, gemessen %zu",
          ROH_BYTE, vn);

    if (aus && ver && n == vn) {
        size_t ab = 0, erste = (size_t)-1;
        for (size_t i = 0; i < n; ++i)
            if (aus[i] != ver[i]) { ab++; if (erste == (size_t)-1) erste = i; }
        CHECK(ab == 0,
              "%zu von %zu Byte weichen ab (erste bei %zu) — die Quelle "
              "kommt aus fremder Hand, das flache Abbild hat keine "
              "Wahlfreiheit, also muss es byteidentisch sein",
              ab, n, erste);
        printf("    %zu Byte, %zu abweichend gegen uft_pc160.img\n", n, ab);
    }
    free(aus); free(ver);
    remove(ziel);
}

/* ═══════════ 4. Der Preflight sperrt weiterhin, was ungeprueft ist ══ */

static void t4_preflight_sperrt_weiter(void)
{
    printf("Test 4: ein UNGEPRUEFTES Paar bleibt gesperrt\n");
    /* Anti-Tautologie: Gruppe 3 waere auch gruen, wenn der Preflight GAR
     * NICHT mehr sperrte. Diese Gruppe zeigt, dass er es weiterhin tut —
     * an einem Paar, das absichtlich noch ohne Eintrag ist. */
    const uft_roundtrip_status_t s =
        uft_roundtrip_status((uft_format_id_t)UFT_FORMAT_TD0,
                             (uft_format_id_t)UFT_FORMAT_IMD);
    CHECK(s == UFT_RT_UNTESTED,
          "TD0->IMD ist noch ohne Eintrag (Phase 2 des Plans); gemessen %s",
          uft_roundtrip_status_string(s));

    const char *ziel = "uft_imd_img_belegt_td0.imd";
    remove(ziel);
    uft_convert_options_t opt = uft_convert_default_options();
    uft_convert_result_t res;
    memset(&res, 0, sizeof(res));
    const uft_error_t rc = uft_convert_file(UFT_CORPUS_DIR "/libdsk_uftk_pc720.td0",
                                            ziel, UFT_FORMAT_IMD, &opt, &res);
    CHECK(rc != UFT_OK,
          "das Tor muss weiterhin sperren — sonst belegt Gruppe 3 nichts");
    printf("    TD0->IMD bleibt gesperrt (rc=%d)\n", (int)rc);
    remove(ziel);
}

/* ═══════════ 5. Die Luecke wird 0xE5 — und sie wird GEMELDET ═══════
 *
 * Nachgetragen, weil der Rotbeweis es verlangt hat: die Mutation
 * „Fuellbyte 0xE5 -> 0x00" ist beim ersten Lauf NICHT gefallen. Die
 * Gruppen 1–4 koennen sie nicht sehen, denn `hxcfe_pc160.imd` hat
 * 0 fehlende Sektoren — das Fuellbyte kommt dort nie zum Einsatz.
 *
 * Diese Gruppe baut eine IMD MIT einer Luecke: eine Spur, zwei
 * Sektoren, davon einer vom Typ UFT_IMD_SEC_UNAVAIL (0x00). */
static void t5_luecke_ist_0xE5_und_gemeldet(void)
{
    printf("Test 5: eine fehlende Sektorstelle wird 0xE5 und wird gesagt\n");

    /* IMD: "IMD " + Kopftext + 0x1A, dann je Spur:
     *      mode, cyl, head, nsectors, size_code, smap[], dann je Sektor
     *      ein Typbyte (0x01 = normal + 128 Datenbytes, 0x00 = fehlt). */
    uint8_t imd[256];
    size_t k = 0;
    /* Der Kopf ist dem echten `hxcfe_pc160.imd` nachgebildet, samt
     * CR/LF nach dem Zeitstempel — ohne das weist der Leser die Datei
     * ab (gemessen: „IMD parse failed (error -1)"). */
    const char *kopf = "IMD 1.18: 01/01/2026 00:00:00\r\n";
    memcpy(imd + k, kopf, strlen(kopf)); k += strlen(kopf);
    imd[k++] = UFT_IMD_COMMENT_END;      /* 0x1A */
    imd[k++] = 0;                        /* mode: 500 kbps FM           */
    imd[k++] = 0;                        /* Zylinder 0                  */
    imd[k++] = 0;                        /* Kopf 0                      */
    imd[k++] = 2;                        /* zwei Sektoren               */
    imd[k++] = 0;                        /* Groessenkode 0 = 128 Byte   */
    imd[k++] = 1; imd[k++] = 2;          /* smap: Sektor 1 und 2        */
    imd[k++] = UFT_IMD_SEC_NORMAL;       /* Sektor 1: Daten folgen      */
    for (unsigned i = 0; i < 128u; ++i) imd[k++] = 0xAA;
    imd[k++] = UFT_IMD_SEC_UNAVAIL;      /* Sektor 2: FEHLT             */

    uft_convert_options_ext_t opt;
    memset(&opt, 0, sizeof(opt));
    uft_convert_result_t res;
    memset(&res, 0, sizeof(res));
    uint8_t *aus = NULL;
    size_t n = 0;

    const uft_error_t rc = uftc_imd_to_img_mem(imd, k, &opt, &res, &aus, &n);
    CHECK(rc == UFT_OK, "die Wandlung laeuft, rc=%d", (int)rc);
    CHECK(n == 256u, "zwei Sektoren zu 128 Byte = 256, geliefert %zu", n);

    if (aus && n == 256u) {
        CHECK(aus[0] == 0xAA && aus[127] == 0xAA,
              "der vorhandene Sektor steht unveraendert da");
        size_t e5 = 0;
        for (size_t i = 128; i < 256; ++i) if (aus[i] == 0xE5) e5++;
        CHECK(e5 == 128u,
              "die Luecke MUSS mit 0xE5 gefuellt sein — das ist die "
              "Konvention, an der ein Leser sie ueberhaupt erkennen kann; "
              "gezaehlt %zu von 128", e5);
    }

    /* Und sie wird GESAGT. Ein Fuellbyte ohne Meldung waere erfundener
     * Inhalt: 0xE5 aus einer Luecke sieht aus wie 0xE5 vom Traeger. */
    bool gemeldet = false;
    for (int i = 0; i < res.warning_count && i < 8; ++i)
        if (strstr(res.warnings[i], "unavailable") || strstr(res.warnings[i], "bad"))
            gemeldet = true;
    CHECK(gemeldet,
          "der fehlende Sektor muss in einer Warnung stehen — sonst ist "
          "das Fuellbyte eine stille Erfindung. %d Warnungen: %s",
          res.warning_count,
          res.warning_count ? res.warnings[res.warning_count - 1] : "(keine)");
    printf("    1 Sektor fehlt -> 128x 0xE5, und es steht in der Warnung\n");
    free(aus);
}

int main(void)
{
    printf("=== test_convert_imd_img_belegt (MF-1277) ===\n\n");
    if (uft_register_all_formats() != UFT_OK) {
        printf("REGISTRY FEHLER\n");
        return 1;
    }
    t1_matrix();
    t2_notiz_nennt_verlust();
    t3_wandlung_laeuft();
    t4_preflight_sperrt_weiter();
    t5_luecke_ist_0xE5_und_gemeldet();
    printf("\n%s (%d Fehler)\n", _fail ? "FEHLGESCHLAGEN" : "BESTANDEN", _fail);
    return _fail ? 1 : 0;
}
