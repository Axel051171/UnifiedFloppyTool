/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_matrix_form.c
 * @brief Die Rundlauf-Matrix darf nicht mehr versprechen, als das Ziel
 *        tragen kann (MF-1283)
 *
 * ── WAS DIE FALSCHE FRAGE WAR ────────────────────────────────────────────
 *
 * Die Matrix kannte EINE Frage: ist der Rundlauf byteidentisch? Fuer ein
 * Ziel, das WENIGER traegt als die Quelle, ist das die falsche Frage.
 * `TD0 -> IMG -> TD0` kann nie identisch sein — nicht wegen eines Fehlers,
 * sondern weil IMG weder CRC-Zustand noch geloeschte Marken noch variable
 * Sektorgroessen noch einen Kommentar tragen KANN. Ein Tor, das dort
 * Byteidentitaet verlangt, sperrt den Wandler nicht als ungeprueft,
 * sondern als unmoeglich — und dann bleibt jeder verlustbehaftete Wandler
 * fuer immer gesperrt.
 *
 * ── UND WARUM DIE ALTE FORM NICHT PRUEFBAR WAR ───────────────────────────
 *
 * `LOSSY_DOCUMENTED` heisst „Verlust bekannt & in Kategorien
 * dokumentiert". Die Kategorien standen in `note` — als FLIESSTEXT. Kein
 * Tor kann einen Satz pruefen. Die Einstufung war damit eine Zusage ohne
 * Pruefmittel, dieselbe Lage wie eine Merkmalstafel mit „Read: SUPPORTED",
 * deren Leser jede Datei abweist (MF-961, MF-1015).
 *
 * Seit MF-1283 traegt der Eintrag eine MASKE, und die Differenz wird aus
 * `uft_format_traegt()` GERECHNET statt erzaehlt.
 */
#include <stdio.h>
#include <string.h>

#include "uft/core/uft_roundtrip.h"
#include "uft/core/uft_format_traegt.h"
#include "uft/core/uft_preflight.h"
#include "uft/uft_types.h"

static int _fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); _fail++; } } while (0)

/* Die fuenf Merkmale, die ein Sektorbehaelter ueber die blossen Nutzbytes
 * hinaus tragen kann. Sie stehen hier ein ZWEITES Mal, absichtlich: der
 * Test soll nicht dieselbe Quelle befragen wie der Prueflings-Code
 * (Klasse MF-1000). Weicht die Tafel ab, faellt es hier auf. */
#define ERWARTET_VERLUST_NACH_IMG                                  \
    (UFT_D2_FEAT_BAD_CRC       | UFT_D2_FEAT_DELETED_DAM |         \
     UFT_D2_FEAT_VAR_SECTOR_SZ | UFT_D2_FEAT_NO_DATA_SEC |         \
     UFT_D2_FEAT_METADATA)

/* ═══════ 1. Die Tafel sagt, was sie weiss — und was sie NICHT weiss ═══ */

static void t1_bekannt_und_unbekannt(void)
{
    printf("Test 1: die Tafel unterscheidet gemessen von ungemessen\n");

    const uft_format_traegt_t td0 = uft_format_traegt(UFT_FORMAT_TD0);
    const uft_format_traegt_t imd = uft_format_traegt(UFT_FORMAT_IMD);
    const uft_format_traegt_t img = uft_format_traegt(UFT_FORMAT_IMG);

    CHECK(td0.bekannt, "TD0 hat eine gemessene Zeile");
    CHECK(imd.bekannt, "IMD hat eine gemessene Zeile");
    CHECK(img.bekannt, "IMG hat eine gemessene Zeile");

    CHECK((td0.layers & (1u << UFT_D2_LAYER_SECTORS)) != 0u,
          "TD0 traegt die Sektorschicht");
    CHECK((img.features == 0u),
          "IMG traegt KEIN Merkmal ueber die Nutzbytes hinaus — gemessen "
          "0x%08x", img.features);
    CHECK(td0.quelle && strlen(td0.quelle) > 20,
          "jede Zeile nennt, WORAN sie gemessen ist");

    /* Und die Regel, an der alles haengt. */
    const uft_format_traegt_t fremd = uft_format_traegt(UFT_FORMAT_SCP);
    CHECK(!fremd.bekannt,
          "ein ungetafeltes Format ist UNBEKANNT");
    CHECK(fremd.features == 0u && fremd.layers == 0u,
          "und seine Masken sind leer — was NICHT heisst, es traege "
          "nichts");

    uint32_t v = 0xFFFFFFFFu;
    CHECK(!uft_format_verlust(UFT_FORMAT_TD0, UFT_FORMAT_SCP, &v),
          "mit einem unbekannten Format ist die Differenz NICHT "
          "feststellbar");
    CHECK(v == 0u, "und der Ausgabewert wird trotzdem gesetzt, nicht "
                   "stehen gelassen — gemessen 0x%08x", v);

    printf("    drei Zeilen gemessen, eine Nichtzeile richtig benannt\n");
}

/* ═══════ 2. Die Differenz wird GERECHNET ═════════════════════════════ */

static void t2_differenz_ist_gerechnet(void)
{
    printf("Test 2: was verloren geht, kommt aus den Zeilen\n");
    uint32_t v = 0u;

    CHECK(uft_format_verlust(UFT_FORMAT_TD0, UFT_FORMAT_IMD, &v),
          "TD0 und IMD sind beide getafelt");
    CHECK(v == 0u,
          "IMD traegt dieselben Merkmale wie TD0 — kein Merkmalsverlust, "
          "gemessen 0x%08x", v);

    CHECK(uft_format_verlust(UFT_FORMAT_TD0, UFT_FORMAT_IMG, &v),
          "TD0 und IMG sind beide getafelt");
    CHECK(v == ERWARTET_VERLUST_NACH_IMG,
          "nach IMG gehen genau fuenf Merkmale verloren — erwartet "
          "0x%08x, gemessen 0x%08x", (unsigned)ERWARTET_VERLUST_NACH_IMG, v);

    CHECK(uft_format_verlust(UFT_FORMAT_IMD, UFT_FORMAT_IMG, &v),
          "IMD und IMG sind beide getafelt");
    CHECK(v == ERWARTET_VERLUST_NACH_IMG,
          "und aus IMD heraus dieselben fuenf — gemessen 0x%08x", v);

    printf("    TD0->IMD: 0 · TD0->IMG und IMD->IMG: je 5 Merkmale\n");
}

/* ═══════ 3. Das ZUVIEL-Versprechen faellt ════════════════════════════ */

static void t3_zuviel_faellt(void)
{
    printf("Test 3: nur das Zuviel-Versprechen wird abgewiesen\n");

    CHECK(uft_preflight_widerspruch(UFT_FORMAT_TD0, UFT_FORMAT_IMG,
                                    UFT_RT_LOSSLESS) != NULL,
          "„identisch\" bei einem aermeren Ziel ist ein Widerspruch");

    CHECK(uft_preflight_widerspruch(UFT_FORMAT_TD0, UFT_FORMAT_IMD,
                                    UFT_RT_LOSSLESS) == NULL,
          "bei gleich reichem Ziel ist „identisch\" erlaubt");

    /* ── NACHGEZOGEN MF-1307: diese Gegenprobe IST WEGGEFALLEN ────────
     *
     * Hier stand, mit dieser Begruendung:
     *
     *     „Unvollstaendige Verlustliste: fuer TD0->IMG gibt es (noch)
     *      keinen Eintrag, die Maske ist also 0 — und 0 deckt fuenf
     *      Merkmale nicht."
     *     CHECK(uft_preflight_widerspruch(TD0, IMG, LOSSY_DOCUMENTED)
     *           != NULL, ...)
     *
     * Das Wort „noch" war der ganze Halt. Seit MF-1307 traegt TD0->IMG
     * einen Eintrag MIT vollstaendiger Maske, der Widerspruch bleibt also
     * zu Recht aus.
     *
     * **Und es gibt keinen Ersatz im Baum**, gemessen: die Merkmalstafel
     * fuehrt DREI Formate (TD0, IMD, IMG); nichtleer ist die gerechnete
     * Differenz nur bei TD0->IMG und IMD->IMG, und beide haben jetzt
     * einen Eintrag. Ein Paar mit „Maske kleiner als Differenz" laesst
     * sich damit nicht mehr herstellen, ohne die Tafel oder die Matrix zu
     * veraendern — und das waere die Zahl als Motiv (MF-1077).
     *
     * Die Lehre steht als **P3-534**: die Gegenprobe dieses Tors war ein
     * Paar, das bloss NOCH NICHT erledigt war. Ein Tor, dessen Rotbeweis
     * am Rueckstand haengt, verliert ihn, sobald jemand den Rueckstand
     * abarbeitet.
     *
     * Was bleibt und weiterhin traegt: die Zusage DARUEBER zeigt, dass
     * `uft_preflight_widerspruch()` ueberhaupt NEIN sagen kann — „identisch"
     * bei einem aermeren Ziel faellt. Nur die Spielart „zu kurze Maske"
     * ist unbeaufsichtigt. */
    CHECK(uft_preflight_widerspruch(UFT_FORMAT_TD0, UFT_FORMAT_IMG,
                                    UFT_RT_LOSSY_DOCUMENTED) == NULL,
          "TD0->IMG traegt seit MF-1307 die volle Maske — der Widerspruch "
          "muss ausbleiben");

    /* IMD->IMG hat seit MF-1283 die vollstaendige Maske. */
    CHECK(uft_preflight_widerspruch(UFT_FORMAT_IMD, UFT_FORMAT_IMG,
                                    UFT_RT_LOSSY_DOCUMENTED) == NULL,
          "eine vollstaendige Verlustliste geht durch");

    /* Pessimismus ist erlaubt: mehr zu benennen als gerechnet faellt NICHT. */
    CHECK(uft_preflight_widerspruch(UFT_FORMAT_TD0, UFT_FORMAT_IMD,
                                    UFT_RT_LOSSY_DOCUMENTED) == NULL,
          "ohne gerechneten Verlust deckt auch eine leere Liste alles ab "
          "— Pessimismus ist erlaubt, nur Zuviel-Versprechen nicht");

    /* Und ohne zwei gemessene Zeilen wird gar nicht geurteilt. */
    CHECK(uft_preflight_widerspruch(UFT_FORMAT_TD0, UFT_FORMAT_SCP,
                                    UFT_RT_LOSSLESS) == NULL,
          "bei einem ungetafelten Format urteilt die Probe NICHT — sonst "
          "waere sie eine Mauer aus Unwissen");

    printf("    vier Zweige geprueft, Pessimismus bleibt erlaubt\n");
}

/* ═══════ 4. DIE RATSCHE: die lebende Matrix ist in sich stimmig ══════ */

static void t4_matrix_ist_stimmig(void)
{
    printf("Test 4: KEIN Eintrag der Matrix verspricht zu viel\n");
    size_t n = 0;
    const uft_roundtrip_entry_t *e = uft_roundtrip_entries(&n);
    CHECK(e != NULL && n > 0, "die Matrix ist lesbar");
    if (!e) return;

    size_t geprueft = 0;
    for (size_t i = 0; i < n; ++i) {
        const char *w =
            uft_preflight_widerspruch(e[i].from, e[i].to, e[i].status);
        CHECK(w == NULL,
              "Eintrag %zu (%u -> %u, %s) widerspricht der gemessenen "
              "Merkmalsdifferenz: %s",
              i, (unsigned)e[i].from, (unsigned)e[i].to,
              uft_roundtrip_status_short(e[i].status), w ? w : "");
        uint32_t v = 0u;
        if (uft_format_verlust(e[i].from, e[i].to, &v)) geprueft++;
    }

    printf("    %zu Eintraege, davon %zu mit gemessener Differenz\n",
           n, geprueft);
    CHECK(geprueft > 0,
          "mindestens ein Paar muss wirklich geprueft werden, sonst ist "
          "diese Gruppe gruen aus dem falschen Grund");
}

/* ═══════ 5. Das Preflight urteilt entsprechend ═══════════════════════ */

static void t5_preflight_bleibt_richtig(void)
{
    printf("Test 5: IMD->IMG verhaelt sich unveraendert\n");

    uft_preflight_opts_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.dry_run             = true;   /* nichts schreiben, nur urteilen */
    opt.source_format_name  = "IMD";
    opt.target_format_name  = "IMG";
    opt.uft_version         = "test";

    uft_preflight_plan_t plan;

    opt.accept_data_loss = false;
    uft_preflight_check(UFT_FORMAT_IMD, UFT_FORMAT_IMG,
                        "a.imd", "b.img", &opt, &plan);
    CHECK(plan.decision == UFT_PREFLIGHT_ABORT_NEED_CONSENT,
          "ohne Zustimmung gesperrt — gemessen %s",
          uft_preflight_decision_string(plan.decision));

    opt.accept_data_loss = true;
    uft_preflight_check(UFT_FORMAT_IMD, UFT_FORMAT_IMG,
                        "a.imd", "b.img", &opt, &plan);
    CHECK(plan.decision == UFT_PREFLIGHT_OK,
          "mit Zustimmung erlaubt — gemessen %s",
          uft_preflight_decision_string(plan.decision));

    printf("    die neue Probe aendert das Urteil von MF-1277 nicht\n");
}

int main(void)
{
    printf("=== test_matrix_form (MF-1283) ===\n\n");
    t1_bekannt_und_unbekannt();
    t2_differenz_ist_gerechnet();
    t3_zuviel_faellt();
    t4_matrix_ist_stimmig();
    t5_preflight_bleibt_richtig();

    /* NICHT ISOLIERBAR, und das gehoert gesagt statt verschwiegen: der
     * Zweig „Ziel ohne Sektorschicht" ist heute unerreichbar, weil alle
     * drei getafelten Formate eine Sektorschicht tragen. Er wird pruefbar,
     * sobald ein Archiv- oder Einzeldateiformat seine Zeile bekommt —
     * dasselbe Muster wie die zehnte Mutation in MF-1028. */
    printf("\n%s (%d Fehler)\n", _fail ? "FEHLGESCHLAGEN" : "BESTANDEN",
           _fail);
    return _fail ? 1 : 0;
}
