/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d2_bruecke_am_korpus.c
 * @brief Eine gescheiterte `read_track()` ist keine Aussage ueber die Spur —
 *        gemessen an zwei sauberen Korpusabbildern ueber den ECHTEN Weg.
 *
 * Der Weg ist derselbe wie im Analysator (`DiskAnalyzerWindow::
 * traegerBericht`): `uft_disk_open()` -> `uft_d2_from_disk()` ->
 * `uft_d2_querpruefung()`. Kein Modell von Hand, kein Ersatz-Plugin.
 *
 * Der Befund, den dieser Test festhaelt (Review der Tuer Stufe 1+3):
 * `d88_read_track()` (src/formats/d88/uft_d88.c) gibt
 * `UFT_ERROR_INVALID_ARG` zurueck, wenn der Spurversatz 0 ist — und die
 * Datei selbst nennt diesen Fall „0 = unformatted". Die Bruecke schrieb
 * daraus JE Spur ein WARN „die Spur ist nicht gelesen, nicht leer". An
 * `hxcfe_pc160.d88` (eine saubere PC-160K-Diskette, fremd erzeugt von
 * hxcfe) waren das gemessen **120** WARN, an `hxcfe_uftk_nec_2d.d77`
 * **80**. „nicht leer" ist dort eine erfundene Aussage; der Rueckgabewert
 * belegt weder „unlesbar" noch „unformatiert".
 *
 * Verlangt wird seither:
 *   1. kein WARN TRACK_UNREADABLE — der Rueckgabewert traegt es nicht;
 *   2. ein NOTE je LAUF (aufeinanderfolgende Zylinder, derselbe
 *      Rueckgabewert, dieselbe Kopfmenge), nicht je Spur — die Regel steht
 *      im Kommentar der Bruecke selbst („einmal je Klasse, nicht je Spur,
 *      damit die Befundliste nicht ueberlaeuft");
 *   3. der Text behauptet weder „nicht leer" noch „unlesbar" als Tatsache;
 *   4. die Querpruefung nennt keine dieser Spuren „fehlend".
 *
 * Die erwarteten Laeufe sind am Abbild abgezaehlt, nicht aus dem Plugin
 * geholt: der Test zaehlt die gescheiterten Spuren selbst (Summe
 * TRACKS_FAILED) und leitet die Laeufe aus einem zweiten, eigenen
 * `read_track()`-Durchgang ab — dieselbe Quelle wie die Bruecke waere kein
 * Beleg (Klasse MF-1000).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"   /* vor uft_core.h: dessen Prototypen
                                      * nennen struct uft_format_plugin */
#include "uft/uft_core.h"            /* uft_disk_open / uft_disk_close */
#include "uft/uft_types.h"
#include "uft/core/uft_disk2.h"
#include "uft/core/uft_disk2_bridge.h"

uft_error_t uft_register_all_formats(void);

static int g_fail = 0, g_pass = 0;
#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++;              \
    printf("    FAIL @%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } \
    } while (0)

static size_t zaehle(const uft_disk2_t *d, const char *code,
                     uft_d2_diag_sev_t sev) {
    size_t n = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        if (strcmp(g->code, code) == 0 && g->sev == sev) n++;
    }
    return n;
}

static size_t zaehle_code(const uft_disk2_t *d, const char *code) {
    size_t n = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, code) == 0) n++;
    return n;
}

/* Eigene Zaehlung der Laeufe: je Zylinder die Signatur (Kopfmaske der
 * gescheiterten Koepfe, Rueckgabewert je Kopf); ein Lauf endet, wo sich
 * die Signatur aendert. Zylinder ohne Fehlschlag beenden jeden Lauf. */
typedef struct { unsigned maske; int rc[2]; } sig_t;

static size_t laeufe_selbst_zaehlen(uft_disk_t *disk, size_t *gescheitert) {
    const uft_format_plugin_t *p = uft_disk_plugin(disk);
    const unsigned zyl = disk->geometry.cylinders;
    const unsigned koepfe = disk->geometry.heads;
    size_t laeufe = 0;
    sig_t vor = { 0u, { 0, 0 } };
    *gescheitert = 0;
    for (unsigned c = 0; c < zyl; ++c) {
        sig_t s = { 0u, { 0, 0 } };
        for (unsigned h = 0; h < koepfe && h < 2u; ++h) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            const uft_error_t rc = p->read_track(disk, (int)c, (int)h, &t);
            if (rc != UFT_OK) {
                s.maske |= 1u << h;
                s.rc[h] = (int)rc;
                (*gescheitert)++;
            }
            /* Wie `spur_freigeben()` der Bruecke: `uft_track_cleanup()`
             * allein kennt diese vier Felder nicht (MF-599). */
            free(t.confidence); free(t.weak_mask); free(t.flux_times);
            if (t.revisions) {
                for (size_t i = 0; i < t.revision_count; i++)
                    free(t.revisions[i].data);
                free(t.revisions);
            }
            t.confidence = NULL; t.weak_mask = NULL; t.flux_times = NULL;
            t.revisions = NULL; t.revision_count = 0;
            uft_track_cleanup(&t);
        }
        if (s.maske && (s.maske != vor.maske || s.rc[0] != vor.rc[0]
                        || s.rc[1] != vor.rc[1]))
            laeufe++;
        vor = s;
    }
    return laeufe;
}

static void abbild(const char *name, size_t erwartet_gescheitert,
                   size_t erwartet_laeufe) {
    char pfad[1024];
    snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, name);
    printf("%s\n", name);
    uft_disk_t *disk = uft_disk_open(pfad, true);
    CHECK(disk != NULL, "uft_disk_open(%s) lieferte NULL", pfad);
    if (!disk) return;
    printf("    Plugin %s, Geometrie %u x %u\n",
           uft_disk_plugin(disk) ? uft_disk_plugin(disk)->name : "(keins)",
           (unsigned)disk->geometry.cylinders, (unsigned)disk->geometry.heads);

    size_t gescheitert = 0;
    const size_t laeufe = laeufe_selbst_zaehlen(disk, &gescheitert);
    printf("    selbst gezaehlt: %zu Spuren gescheitert, %zu Laeufe\n",
           gescheitert, laeufe);
    /* Die Zahlen des Abbilds stehen hier fest, damit ein geaendertes
     * Plugin den Test nicht still leerlaufen laesst. */
    CHECK(gescheitert == erwartet_gescheitert,
          "gescheiterte Spuren %zu, erwartet %zu", gescheitert,
          erwartet_gescheitert);
    CHECK(laeufe == erwartet_laeufe, "Laeufe %zu, erwartet %zu", laeufe,
          erwartet_laeufe);

    uft_disk2_t *d = uft_d2_create();
    uft_d2_bridge_stats_t st;
    CHECK(uft_d2_from_disk(d, disk, uft_disk_plugin(disk), &st),
          "Bruecke speiste nichts ein");
    CHECK(st.tracks_failed == gescheitert,
          "Bruecke: %zu gescheitert, selbst gezaehlt %zu", st.tracks_failed,
          gescheitert);

    const size_t warn = zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_WARN);
    const size_t note = zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_NOTE);
    printf("    TRACK_UNREADABLE: %zu WARN, %zu NOTE\n", warn, note);
    CHECK(warn == 0u, "%zu WARN TRACK_UNREADABLE — ein Rueckgabewert belegt "
          "keine unlesbare Spur", warn);
    CHECK(note == laeufe, "%zu NOTE TRACK_UNREADABLE, erwartet einer je Lauf "
          "(%zu)", note, laeufe);
    CHECK(zaehle_code(d, "TRACK_UNREADABLE") == laeufe,
          "keine weitere Schwere fuer TRACK_UNREADABLE");

    /* Gezaehlt statt je Befund gemeldet: vor der Behebung waren es 120
     * Befunde, und 120 gleiche Fehlzeilen verdecken die Summe. */
    size_t behauptet = 0, ohne_rc = 0, ohne_grenze = 0, gezeigt = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        if (strcmp(g->code, "TRACK_UNREADABLE") != 0) continue;
        if (gezeigt++ < 4u)
            printf("      [%d] %s C%d H%d: %s\n", (int)g->sev, g->code,
                   (int)g->cyl, (int)g->head, g->text);
        if (strstr(g->text, "nicht leer")) behauptet++;
        if (!strstr(g->text, "rc=")) ohne_rc++;
        if (!strstr(g->text, "sagt der Rueckgabewert nicht")) ohne_grenze++;
    }
    CHECK(behauptet == 0u, "%zu Texte behaupten „nicht leer\"", behauptet);
    CHECK(ohne_rc == 0u, "%zu Texte nennen den Rueckgabewert nicht (rc=)",
          ohne_rc);
    CHECK(ohne_grenze == 0u, "%zu Texte sagen nicht, was der Rueckgabewert "
          "NICHT belegt", ohne_grenze);

    /* Die Querpruefung darf daraus keine Luecke machen. */
    const size_t vorher = uft_d2_diag_count(d);
    uft_d2_querpruefung(d);
    for (size_t i = vorher; i < uft_d2_diag_count(d); ++i) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        printf("      neu: %s: %s\n", g->code, g->text);
        /* The check writes "fehlt:" and the code SEC_GAP_VS_BRACKET; a
         * search for "fehlend" could never match (reviewer note). */
        CHECK(strstr(g->text, "fehlt:") == NULL
                  && strstr(g->code, "SEC_GAP") == NULL
                  && strstr(g->code, "MISSING") == NULL,
              "Querpruefung nennt eine Spur fehlend: %s %s", g->code, g->text);
    }
    CHECK(uft_d2_diag_count(d) == vorher,
          "Querpruefung meldete %zu Befunde an einer sauberen Diskette",
          uft_d2_diag_count(d) - vorher);

    uft_d2_destroy(d);
    uft_disk_close(disk);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_d2_bruecke_am_korpus ===\n\n");
    if (uft_register_all_formats() != UFT_OK) {
        printf("uft_register_all_formats scheiterte\n");
        return 1;
    }
    /* Die Zahlen sind am Abbild gemessen (siehe Kopf): PC-160K ist
     * einseitig mit 40 Zylindern, der D88-Kopf nennt 80 x 2. */
    abbild("hxcfe_pc160.d88", 120u, 2u);
    abbild("hxcfe_uftk_nec_2d.d77", 80u, 1u);
    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
