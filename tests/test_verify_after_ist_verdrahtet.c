/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_verify_after_ist_verdrahtet.c
 * @brief `verify_after` war eine Zusage ohne Tat (MF-1307, Regel E-7)
 *
 * AUFRUFER / EINBINDUNG: `tests/CMakeLists.txt` haengt diesen Test an
 *   denselben Zweig wie `test_convert_imd_img_belegt` — er faehrt ueber
 *   `uft_convert_file()` und braucht die ganze Format-Schicht.
 * BERUEHRTE API: `uft_convert_verify_after()` und `uft_verify_bilanz_t`
 *   (beide neu in `include/uft/uft_format_plugin.h`), `uft_convert_file()`,
 *   `uft_convert_options_t::verify_after`, `uft_convert_result_t`.
 * DATENSCHEMA: liest `tests/corpus_free/uft_gepackt_pc720.td0` und
 *   `uft_gepackt_pc1200.td0` (MF-1297), schreibt Wegwerfziele ins
 *   Arbeitsverzeichnis und raeumt sie wieder ab.
 * ANWEISUNG (woertlich): „Und verify_after — E-7 — haengt an der
 *   Schnappschusspruefung, bevor der Eintrag gesetzt wird, sonst behauptet
 *   der Wandler eine Nachpruefung, die es wieder nicht gibt."
 *
 * ── DER BEFUND, DEN DIESER TEST BEWACHT ──────────────────────────────────
 *
 * `verify_after` wurde GESETZT (`src/core/uft_copy_plan.c`), KOPIERT
 * (`ext_opts.verify_after = options->verify_after`) und dem Bediener als
 * `ja` ANGEZEIGT (`src/gui/uft_save_image.cpp`) — und an keiner Stelle im
 * Baum ABGEFRAGT. Das ist Regel **E-7** in Reinform: ein Feld gesetzt,
 * kopiert und angezeigt, aber nie gelesen; und es ist zugleich die Klasse
 * `schreibzusage_ohne_tat` aus MF-883 — die Oberflaeche sagte eine
 * Nachpruefung zu, die es nicht gab.
 *
 * ── WAS DIE GRUPPEN TRENNEN ──────────────────────────────────────────────
 *
 * Gruppe 1 zeigt, dass die Pruefung LAEUFT und ZAEHLT. Sie belegt fuer
 * sich genommen NICHTS ueber ihre Urteilskraft — Quelle gegen sich selbst
 * ist gleich, komme was wolle (Klasse MF-1039, „eine Gleichheit ohne
 * Aussage"). Die Gruppen 2 und 3 sind deshalb Pflicht: sie zeigen, dass
 * sie auch NEIN sagen kann, und zwar an zwei verschiedenen Zweigen.
 *
 * Gruppe 4 ist der Beweis nach Regel **D2**: ein Aufrufer und ein Test,
 * der rot wird, wenn der Aufruf verschwindet.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"
#include "uft/formats/uft_td0.h"

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es fuer diesen Test setzen"
#endif

#define TD0_720   UFT_CORPUS_DIR "/uft_gepackt_pc720.td0"
#define TD0_1200  UFT_CORPUS_DIR "/uft_gepackt_pc1200.td0"

/* Gemessen an der Datei, die MF-1297 committet hat: 80 Zylinder, 2 Koepfe,
 * 9 Sektoren je Spur. Die Zahlen stehen hier ABSICHTLICH ein zweites Mal
 * und nicht als Rechnung aus der Quelle — ein Test, der seinen Pruefling
 * nach der Antwort fragt, prueft nichts (Klasse MF-1000). */
#define ERWARTET_SPUREN    160u
#define ERWARTET_SEKTOREN  1440u

static int fehler = 0;

#define CHECK(bedingung, ...)                                              \
    do {                                                                   \
        if (!(bedingung)) {                                                \
            printf("  [ROT] ");                                            \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            fehler++;                                                      \
        }                                                                  \
    } while (0)

static void bilanz_drucken(const char *was, uft_error_t rc,
                           const uft_verify_bilanz_t *b)
{
    printf("    %-34s rc=%-4d quelle=%d ziel=%d  %ux%u -> %ux%u  "
           "geprueft=%u abweichend=%u leer=%u unlesbar=%u sektoren=%u\n",
           was, (int)rc, (int)b->quelle_offen, (int)b->ziel_offen,
           b->quell_zylinder, b->quell_koepfe,
           b->ziel_zylinder,  b->ziel_koepfe,
           (unsigned)b->spuren_geprueft, (unsigned)b->spuren_abweichend,
           (unsigned)b->spuren_leer, (unsigned)b->spuren_unlesbar,
           (unsigned)b->sektoren_geprueft);
}

/* ═══════════ 1. Die Pruefung laeuft und zaehlt ═════════════════════════ */

static void t1_eichung_am_objekt(void)
{
    printf("Test 1: die Nachpruefung laeuft und zaehlt, was sie sieht\n");

    uft_verify_bilanz_t b;
    const uft_error_t rc = uft_convert_verify_after(TD0_720, TD0_720, NULL, &b);
    bilanz_drucken("720K gegen sich selbst", rc, &b);

    CHECK(rc == UFT_OK, "Quelle gegen sich selbst muss durchgehen; rc=%d",
          (int)rc);
    CHECK(b.quelle_offen && b.ziel_offen, "beide Dateien muessen offen sein");
    CHECK(b.spuren_geprueft == ERWARTET_SPUREN,
          "80 Zylinder x 2 Koepfe sind 160 Spuren, gemessen %u",
          (unsigned)b.spuren_geprueft);
    CHECK(b.sektoren_geprueft == ERWARTET_SEKTOREN,
          "160 Spuren x 9 Sektoren sind 1440, gemessen %u",
          (unsigned)b.sektoren_geprueft);
    CHECK(b.spuren_abweichend == 0u, "keine Spur darf abweichen, gemessen %u",
          (unsigned)b.spuren_abweichend);
    CHECK(b.spuren_unlesbar == 0u, "keine Spur darf unlesbar sein, gemessen %u",
          (unsigned)b.spuren_unlesbar);
}

/* ═══════════ 2. Sie kann NEIN sagen — Zweig Spurvergleich ══════════════ */

static void t2_rotbeweis_spuren_weichen_ab(void)
{
    printf("Test 2: zwei verschiedene Disketten muessen auffallen\n");

    /* 720K und 1.2M tragen BEIDE 80 Zylinder und 2 Koepfe — der
     * Geometriezweig kann sie also gar nicht trennen. Was sie trennt, ist
     * die Sektorzahl je Spur (9 gegen 15) und jedes Datenbyte. Genau
     * deshalb ist dieses Paar hier richtig: es prueft den SPURZWEIG und
     * nicht versehentlich den Geometriezweig. */
    uft_verify_bilanz_t b;
    const uft_error_t rc = uft_convert_verify_after(TD0_720, TD0_1200, NULL, &b);
    bilanz_drucken("720K gegen 1.2M", rc, &b);

    CHECK(rc != UFT_OK, "verschiedene Disketten duerfen NICHT durchgehen");
    CHECK(b.quelle_offen && b.ziel_offen,
          "beide sind lesbar — der Befund darf nicht am Oeffnen haengen");
    CHECK(b.quell_zylinder == b.ziel_zylinder &&
          b.quell_koepfe   == b.ziel_koepfe,
          "die Geometrien sind gleich (%ux%u gegen %ux%u) — sonst prueft "
          "diese Gruppe den falschen Zweig",
          b.quell_zylinder, b.quell_koepfe, b.ziel_zylinder, b.ziel_koepfe);
    CHECK(b.spuren_abweichend > 0u,
          "mindestens eine Spur muss abweichen, gemessen %u",
          (unsigned)b.spuren_abweichend);
}

/* ═══════════ 3. Sie kann NEIN sagen — Zweig „Ziel nicht lesbar" ════════ */

static void t3_rotbeweis_ziel_unlesbar(void)
{
    printf("Test 3: eine Datei, die niemand oeffnen kann, ist der "
           "staerkste Befund\n");

    uft_verify_bilanz_t b;
    const uft_error_t rc =
        uft_convert_verify_after(TD0_720, "uft_mf1307_gibt_es_nicht.img",
                                 NULL, &b);
    bilanz_drucken("720K gegen Nichtvorhandenes", rc, &b);

    CHECK(rc != UFT_OK, "ein nicht oeffenbares Ziel darf nicht durchgehen");
    CHECK(b.quelle_offen, "die Quelle ist lesbar");
    CHECK(!b.ziel_offen, "das Ziel ist es nicht — und die Bilanz muss das "
          "unterscheiden koennen (Klasse MF-883)");
}

/* ═══════════ 4. Der Aufruf im Verteiler ist lebendig ═══════════════════
 *
 * ── WAS DIE MESSUNG ZUERST ERGEBEN HAT ───────────────────────────────────
 *
 * Der erste Entwurf dieser Gruppe hat GEMESSEN statt behauptet, und das
 * Ergebnis hat den Entwurf umgeworfen. Gegen `uft_gepackt_pc720.td0`:
 *
 *     TD0 -> IMD   160 von 160 Spuren geprueft, 0 abweichend
 *     TD0 -> IMG   160 von 160 Spuren geprueft, 0 abweichend
 *
 * Beide Wege tragen die Daten also wirklich durch — `verify_after` aendert
 * dort NICHTS, und ein Test, der sich darauf stuetzt, kann gar nicht rot
 * werden (Klasse MF-1000 / Tor 64).
 *
 * **Auch ein Sektor OHNE Daten taugt nicht als Rotbeweis, und der Grund
 * steht im Kopf der Pruefung selbst:** sie vergleicht DATEN, nicht
 * MERKMALE. Ein TD0-Sektor mit `UFT_TD0_SEC_NODAT` wird von `uft_td0.c`
 * mit `0xE5` angelegt, und IMD wie IMG fuellen die Luecke mit demselben
 * Byte — beide Seiten sind byteweise gleich, waehrend das MERKMAL
 * verlorengeht. Dafuer ist die `lost_features`-Maske da, nicht diese
 * Pruefung.
 *
 * ── DER EINE WEG, AUF DEM DAS SCHREIBEN WIRKLICH BYTES VERLIERT ──────────
 *
 * `uft_td0_to_imd()` nimmt `t.sectors[0].data_len`, rechnet daraus EINEN
 * Groessencode und fuellt jeden weiteren Sektor darauf auf — **P3-524**.
 * Genau dieses Merkmal steht seit MF-1307 als `UFT_D2_FEAT_VAR_SECTOR_SZ`
 * in der Maske des Matrix-Eintrags TD0 -> IMD.
 *
 * **P3-524 sagt woertlich, ein Fixture dafuer gebe es nicht:** alle 1440
 * Sektoren von `libdsk_uftk_pc720.td0` tragen 512 Byte, der Fall ist im
 * Korpus unerreichbar. Diese Gruppe baut ihn — drei Sektoren mit 512, 256
 * und 512 Byte auf einer Spur — und misst damit ZWEI Dinge auf einmal:
 *
 *   1. dass der Aufruf im Verteiler lebendig ist (Regel **D2**), und
 *   2. dass die Maske, die MF-1307 in die Matrix geschrieben hat, eine
 *      GEMESSENE Aussage ist und keine vorsichtige Annahme.
 *
 * Die Pruefdatei entsteht im Arbeitsverzeichnis und wird abgeraeumt; sie
 * ist ein Rotbeweis-Werkzeug, kein Korpusstueck. */

static void sektor_schreiben(FILE *f, uint8_t cyl, uint8_t head, uint8_t nr,
                             uint8_t groessenkode, uint8_t fuellung)
{
    const uint16_t gr = (uint16_t)(128u << groessenkode);
    uint8_t daten[512];
    for (uint16_t i = 0; i < gr && i < sizeof(daten); i++) daten[i] = fuellung;

    /* MF-1296: das sechste Kopfbyte ist die Pruefsumme UEBER DIE DATEN,
     * und der Leser rechnet sie seither nach. `uft_td0_crc()` ist in
     * `test_td0_pruefsummen.c` an einer von LIBDSK geschriebenen Datei
     * geeicht (Dateikopf 0x6EDD, Kommentar 0xFEF2) — die Verankerung
     * liegt also ausserhalb dieses Baums. */
    const uint8_t crc = (uint8_t)(uft_td0_crc(daten, gr, 0u) & 0xFFu);
    const uint8_t hdr[6] = { cyl, head, nr, groessenkode, 0x00, crc };
    fwrite(hdr, 1, 6, f);

    const uint16_t len = (uint16_t)(1u + gr);   /* Methodenbyte + Rohdaten */
    const uint8_t lb[2] = { (uint8_t)(len & 0xFFu), (uint8_t)(len >> 8) };
    fwrite(lb, 1, 2, f);
    fputc(0, f);                                /* Verfahren 0 = roh */
    fwrite(daten, 1, gr, f);
}

/* Eine Spur, drei Sektoren, GEMISCHTE Groessen. Unverpackt ('T','D'), damit
 * die Datei ohne den Packer auskommt. Version < 0x10 heisst „kein
 * Kommentarkopf"; die Spurkopf-Pruefsumme steht auf 0 = „keine Aussage",
 * was `uft_td0.c` seit MF-1296 ausdruecklich zulaesst. */
static int baue_td0_mit_gemischten_groessen(const char *pfad)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    const uint8_t kopf[12] = { 0x54, 0x44, 0, 0, 0x00, 0, 0, 0, 0,
                               1 /*Seiten*/, 0, 0 };
    fwrite(kopf, 1, 12, f);
    const uint8_t spurkopf[4] = { 3 /*Sektoren*/, 0 /*Zyl*/, 0 /*Kopf*/,
                                  0 /*CRC = keine Aussage*/ };
    fwrite(spurkopf, 1, 4, f);
    sektor_schreiben(f, 0, 0, 1, 2, 0xA1);      /* 512 Byte */
    sektor_schreiben(f, 0, 0, 2, 1, 0xB2);      /* 256 Byte  <- der Fall */
    sektor_schreiben(f, 0, 0, 3, 2, 0xC3);      /* 512 Byte */
    const uint8_t ende[4] = { 0xFF, 0, 0, 0 };
    fwrite(ende, 1, 4, f);
    fclose(f);
    return 1;
}

static bool nennt_die_nachpruefung(const uft_convert_result_t *r)
{
    for (int i = 0; i < r->warning_count && i < 8; i++)
        if (strstr(r->warnings[i], "verify_after")) return true;
    return false;
}

/* Wandelt @p quelle nach IMD, einmal OHNE und einmal MIT `verify_after`.
 * Genau EIN Wahrheitswert unterscheidet die beiden Laeufe — alles andere
 * ist gleich, damit ein Unterschied im Ergebnis nur von ihm kommen kann. */
static void zwei_laeufe(const char *quelle, const char *ziel,
                        uft_convert_result_t *ohne_aus,
                        uft_convert_result_t *mit_aus)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.accept_data_loss = true;
    o.verify_after     = false;
    memset(ohne_aus, 0, sizeof(*ohne_aus));
    remove(ziel);
    uft_convert_file(quelle, ziel, UFT_FORMAT_IMD, &o, ohne_aus);
    remove(ziel);

    o.verify_after = true;
    memset(mit_aus, 0, sizeof(*mit_aus));
    uft_convert_file(quelle, ziel, UFT_FORMAT_IMD, &o, mit_aus);
    remove(ziel);
}

static void t4_die_verdrahtung_ist_lebendig(void)
{
    printf("Test 4: der Aufruf im Verteiler ist lebendig (Regel D2)\n");

    const char *quelle = "uft_mf1307_gemischte_groessen.td0";
    CHECK(baue_td0_mit_gemischten_groessen(quelle),
          "die Pruefdatei liess sich nicht schreiben");

    uft_convert_result_t ohne, mit;
    zwei_laeufe(quelle, "uft_mf1307_ziel.imd", &ohne, &mit);
    printf("    gemischte Groessen  ohne=%d (W%d)  mit=%d (W%d)\n",
           (int)ohne.success, ohne.warning_count,
           (int)mit.success,  mit.warning_count);
    for (int i = 0; i < mit.warning_count && i < 8; i++)
        printf("        W%d: %s\n", i, mit.warnings[i]);

    CHECK(ohne.success,
          "OHNE Nachpruefung meldet die Ebnung aus P3-524 weiterhin Erfolg "
          "— genau das ist der stille Verlust");
    CHECK(!mit.success,
          "MIT Nachpruefung muss der Erfolg zurueckgenommen werden. Faellt "
          "diese Zusage, ist der Aufruf im Verteiler verschwunden — sie IST "
          "der Rotbeweis nach Regel D2");
    CHECK(mit.error == UFT_ERROR_VERIFY_FAILED,
          "und der Fehlerkode muss die Nachpruefung nennen, gemessen %d",
          (int)mit.error);
    CHECK(nennt_die_nachpruefung(&mit),
          "eine Warnung muss `verify_after` nennen, sonst raet der Bediener");

    /* ── ANTI-TAUTOLOGIE ─────────────────────────────────────────────────
     *
     * Die Zusagen oben waeren auch dann gruen, wenn `verify_after` schlicht
     * JEDE Wandlung ablehnte. Dieselben zwei Laeufe an einer Diskette mit
     * EINHEITLICHER Sektorgroesse muessen deshalb BEIDE durchgehen —
     * gemessen 160 von 160 Spuren, 0 abweichend. */
    uft_convert_result_t g_ohne, g_mit;
    zwei_laeufe(TD0_720, "uft_mf1307_ziel_gut.imd", &g_ohne, &g_mit);
    printf("    einheitlich 512     ohne=%d (W%d)  mit=%d (W%d)\n",
           (int)g_ohne.success, g_ohne.warning_count,
           (int)g_mit.success,  g_mit.warning_count);
    CHECK(g_ohne.success && g_mit.success,
          "eine saubere Wandlung muss MIT Nachpruefung durchgehen — sonst "
          "prueft Gruppe 4 nur, dass `verify_after` alles ablehnt");

    remove(quelle);
}

/* ═══════════ 5. Kein Fehlalarm an einem KOPFLOSEN Ziel ════════════════
 *
 * Ein falscher Alarm ist in diesem Baum so teuer wie ein stiller Verlust:
 * er schickt jemanden zur falschen Stelle. Diese Gruppe haelt den Fall
 * fest, der bei der Verdrahtung ZUERST rot wurde.
 *
 * `IMD -> IMG` (MF-1277) schreibt aus `hxcfe_pc160.imd` eine 163 840 Byte
 * grosse Datei, die gegen `uft_pc160.img` **0 abweichende Byte** hat. Die
 * erste Fassung der Nachpruefung meldete trotzdem einen Verlust — sie
 * sondierte das Ziel blind neu, und ein KOPFLOSES Abbild ist dabei eine
 * Rate-Aufgabe. */

static void t5_kein_fehlalarm_am_kopflosen_ziel(void)
{
    printf("Test 5: eine byteweise richtige Wandlung darf NICHT auffallen\n");

    const char *quelle = UFT_CORPUS_DIR "/hxcfe_pc160.imd";
    const char *ziel   = "uft_mf1307_pc160.img";

    uft_convert_options_t o = uft_convert_default_options();
    o.accept_data_loss = true;
    o.verify_after     = false;
    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));
    remove(ziel);
    const uft_error_t rc = uft_convert_file(quelle, ziel, UFT_FORMAT_IMG,
                                            &o, &r);
    printf("    Wandlung            rc=%d success=%d\n", (int)rc,
           (int)r.success);

    const uft_format_plugin_t *zp =
        uft_resolve_format_plugin(UFT_FORMAT_IMG, ziel, NULL);
    printf("    Zielplugin          %s\n", zp ? zp->name : "(keines)");

    uft_verify_bilanz_t mit_plugin, mit_sonde;
    const uft_error_t r1 = uft_convert_verify_after(quelle, ziel, zp,
                                                    &mit_plugin);
    const uft_error_t r2 = uft_convert_verify_after(quelle, ziel, NULL,
                                                    &mit_sonde);
    bilanz_drucken("Ziel mit genanntem Plugin", r1, &mit_plugin);
    bilanz_drucken("Ziel ueber die Sonde", r2, &mit_sonde);
    remove(ziel);

    CHECK(rc == UFT_OK && r.success, "die Wandlung selbst muss laufen");
    CHECK(r1 == UFT_OK,
          "mit genanntem Zielformat darf eine byteweise richtige Wandlung "
          "NICHT auffallen; rc=%d, %u von %u Spuren abweichend",
          (int)r1, (unsigned)mit_plugin.spuren_abweichend,
          (unsigned)mit_plugin.spuren_geprueft);

    /* ── ZWEI ANTI-TAUTOLOGIEN, BEIDE GEMESSEN ───────────────────────────
     *
     * 1. Die Zusage oben waere auch gruen, wenn die Nachpruefung gar nichts
     *    mehr beanstandete. Dass sie es tut, zeigt derselbe Aufruf ueber
     *    die SONDE: gemessen 40 von 40 Spuren abweichend an genau der
     *    Datei, die mit genanntem Plugin 0 abweichende hat. Damit ist das
     *    Nennen des Zielformats nicht Ausschmueckung, sondern der
     *    Unterschied zwischen Befund und Fehlalarm.
     *
     * 2. Die QUELLE sagt 42 Zylinder an und traegt auf 40 Sektoren — die
     *    zwei leeren duerfen nicht als Abweichung zaehlen (Regel D6), aber
     *    sie muessen SICHTBAR sein. Stuende `spuren_leer` auf 0, waere die
     *    Unterscheidung wieder verloren. Der Befund ueber die IMD-Quelle
     *    selbst steht als **P3-533**. */
    CHECK(r2 != UFT_OK,
          "ueber die blosse Sonde MUSS dieselbe Datei auffallen — sonst "
          "belegt die Zusage darueber nichts (gemessen: 40 von 40)");
    CHECK(mit_plugin.spuren_leer > 0u,
          "die leeren Quellspuren muessen eigens gezaehlt werden, gemessen %u",
          (unsigned)mit_plugin.spuren_leer);
}

int main(void)
{
    printf("=== verify_after ist verdrahtet (MF-1307) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("  [ROT] uft_register_all_formats() scheiterte\n");
        return 1;
    }

    t1_eichung_am_objekt();
    t2_rotbeweis_spuren_weichen_ab();
    t3_rotbeweis_ziel_unlesbar();
    t4_die_verdrahtung_ist_lebendig();
    t5_kein_fehlalarm_am_kopflosen_ziel();

    printf("=== %s ===\n", fehler ? "ROT" : "gruen");
    return fehler ? 1 : 0;
}
