/**
 * @file test_c64_g64_gegen_nibscan.c
 * @brief Differenzlauf UFT <-> nibscan je Halbspur — die Eichung, die den
 *        Oracle-Eintrag aus `docs/ORACLES.md` von "Werkzeug mit Beleg" zu
 *        "zweite Hand" macht (MF-1503).
 *
 * MF-1502 hat `nibscan` als Oracle eingetragen und dabei selbst gesagt, was
 * fehlt: "NICHT gemessen und darum noch kein Urteil: der Differenzlauf
 * UFT <-> nibscan auf denselben Abbildern". Diese Datei liefert ihn.
 *
 * Das Oracle:
 *   nibscan aus `tools/uft-scout/work/nibtools` (Remote `rittwage/nibtools`,
 *   Apache-2.0 — Eigentuemer-Feststellung 2026-09-28, `docs/QUARANTINE.md:55`).
 *   Gebaut MF-1502 mit MinGW 13.1.0, 148 106 Byte, Bauzeile
 *     gcc -O1 -w -I<nib> -I<nib>/include/WINDOWS \
 *         nibscan.c gcr.c prot.c crc.c fileio.c md5.c lz.c
 *   Selbstauskunft: "nibscan - Commodore disk image scanner / comparator,
 *   (C) Peter Rittwage, Built Sep 28 2026". nibscan liegt NICHT im Baum
 *   (Kanal Oracle, MF-695): seine Zahlen stehen unten als Tafel, so wie
 *   `test_atr_groessen_gegen_jhallen.c` es mit jhallens Zahlen haelt.
 *
 * Der UFT-Pfad ist der der Oberflaeche, nicht ein Nachbau:
 *   g64_load() / g64_get_track() / ufm_c64_metrics_from_gcr() — dieselbe
 *   Kette wie `test_c64_protection_real_corpus.c::gui_path_analyze()`.
 *   Halbspurindex ist die Zaehlung der DATEI (MF-928): Spur t liegt auf
 *   2*(t-1), die Halbspur darueber auf 2*(t-1)+1.
 *
 * FUENF GROESSEN, DIE HIER ABSICHTLICH NICHT GEGENEINANDER STEHEN.
 * Jede einzelne davon haette einen Fehlbefund gegen UFT erzeugt; alle fuenf
 * sind vor dem Vergleich gemessen worden, nicht danach erklaert:
 *
 *   1. nibscans "bad GCR bytes" gegen UFTs `bad_gcr_count`.
 *      `check_bad_gcr()` (nibtools gcr.c) laeuft ueber JEDE Byteposition der
 *      Spur und zaehlt jedes ungueltige Byte. UFTs Zaehler wird nur nach
 *      einer erkannten Sync aufgerufen, ueber 8 Byte, und bricht beim ERSTEN
 *      ungueltigen Code mit `break` ab (`ufm_c64_metrics.c:117-118`) — er
 *      kann hoechstens 1 je Sync-Block zaehlen. Gemessen an bountybob:
 *      244 022 gegen 208. Festgehalten in
 *      t_bad_gcr_zaehler_ist_keine_bytezahl (P3-665).
 *   2. nibscans Fusszeile "N tracks with non-standard density" ist keine
 *      Gesamtzahl: `nibscan.c:537` zaehlt nur `track < 36*2`, gedruckt wird
 *      die Meldung fuer jede Spur. bountybob: 42 gedruckt, 40 in der Fusszeile,
 *      Differenz sind Spur 37.0 und 38.5.
 *   3. nibscans `[E2Sn]` gegen UFTs `sector_count`: nibscan iteriert ueber die
 *      SOLL-Sektornummern der Zone (`sector_map[track/2]`, gcr.c:28) und fragt
 *      "ist Sektor n da". UFT zaehlt die HEADER-BLOECKE, egal welche Nummer sie
 *      tragen — `ufm_c64_metrics.c:196-198` sagt das selbst. Auf einer
 *      Standarddiskette fallen beide zusammen, auf einer geschuetzten mit
 *      eigenem Format nicht. Deshalb steht unten eine Gleichung nur fuer das
 *      saubere Abbild und eine Richtung fuer das geschuetzte.
 *   4. Jenseits Spur 35 bedeutet "keine Fehlermarke" das GEGENTEIL von
 *      "alles gelesen". `nibscan.c:589-592`:
 *        if(track/2 <= 35) temp_errors = check_errors(...);
 *        else / * everything is a CBM error above track 35 * / temp_errors = 0;
 *      Festgehalten in t_jenseits_spur_35_zaehlt_nibscan_nicht_mit.
 *   5. Der Parser, der nibscans Ausgabe liest, muss `(density:0!=3?)` kennen.
 *      Eine Fassung, die nur `(density:N)` annahm, verwarf 42 von 71
 *      Spurzeilen STILL und lieferte 29 plausibel aussehende Spuren. Deshalb
 *      steht die Tafel unten vollstaendig da und ihre Laenge wird geprueft.
 *
 * Korpus (LOCAL-ONLY, `tests/corpus/` ist gitignored; Spielcode ist
 * urheberrechtlich geschuetzt — Herkunft und sha256 stehen im Manifest):
 *   c64pp_bountybob.g64      Bounty Bob Strikes Back [Big Five, 1985]
 *   c64pp_aliensyndrome.g64  Alien Syndrome [Sega, 1987]
 *
 * SKIP (exit 77), wenn die lokalen Abbilder fehlen (z.B. CI).
 */

#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"
#include "uft/protection/ufm_c64_metrics.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* ── Was nibscan sagt ─────────────────────────────────────────────────────
 * Je belegter Halbspurplatz: Laenge in Byte und die Zahl der [E2Sn]-Marken.
 * Abgelesen aus den Laeufen von MF-1503; der Parser ist gegen nibscans
 * eigene Fusszeilensumme geprueft (1330 Marken bei bountybob, 0 bei
 * aliensyndrome) und bricht ab, wenn er weniger Zeilen liest als
 * Spurmarken vorhanden sind.
 */
typedef struct { int ht; int byte; int fehlmarken; } nib_spur_t;

static const nib_spur_t NIB_ALIEN[] = {
    {  0, 7691, 0 }, {  2, 7691, 0 }, {  4, 7691, 0 }, {  6, 7691, 0 },
    {  8, 7691, 0 }, { 10, 7691, 0 }, { 12, 7691, 0 }, { 14, 7691, 0 },
    { 16, 7691, 0 }, { 18, 7691, 0 }, { 20, 7691, 0 }, { 22, 7691, 0 },
    { 24, 7691, 0 }, { 26, 7691, 0 }, { 28, 7691, 0 }, { 30, 7691, 0 },
    { 32, 7691, 0 }, { 34, 7141, 0 }, { 36, 7141, 0 }, { 38, 7141, 0 },
    { 40, 7141, 0 }, { 42, 7141, 0 }, { 44, 7141, 0 }, { 46, 7141, 0 },
    { 48, 6665, 0 }, { 50, 6665, 0 }, { 52, 6665, 0 }, { 54, 6665, 0 },
    { 56, 6665, 0 }, { 58, 6665, 0 }, { 60, 6249, 0 }, { 62, 6249, 0 },
    { 64, 6249, 0 }, { 66, 6249, 0 }, { 68, 6249, 0 }, { 70, 6249, 0 },
    { 72, 6249, 0 }, { 74, 6249, 0 }, { 76, 6249, 0 }, { 78, 6249, 0 },
};

static const nib_spur_t NIB_BOUNTY[] = {
    {  0, 6250, 21 }, {  1, 6250, 21 }, {  2, 6250, 21 }, {  3, 6250, 21 },
    {  4, 6250, 21 }, {  5, 6250, 21 }, {  6, 6250, 21 }, {  7, 6250, 21 },
    {  8, 6250, 21 }, {  9, 6250, 21 }, { 10, 6250, 21 }, { 11, 6250, 21 },
    { 12, 6250, 21 }, { 13, 6250, 21 }, { 14, 6250, 21 }, { 15, 6250, 21 },
    { 16, 6250, 21 }, { 17, 6250, 21 }, { 18, 6250, 21 }, { 19, 7142, 21 },
    { 20, 7142, 21 }, { 21, 7692, 21 }, { 22, 7692, 21 }, { 23, 7569, 21 },
    { 24, 6250, 21 }, { 25, 6250, 21 }, { 26, 6250, 21 }, { 27, 6250, 21 },
    { 28, 6250, 21 }, { 29, 6250, 21 }, { 30, 6250, 21 }, { 31, 7142, 21 },
    { 32, 7142, 21 }, { 33, 7141, 21 }, { 34, 7141,  0 }, { 35, 7142, 19 },
    { 36, 7142, 19 }, { 37, 7142, 19 }, { 38, 7142, 19 }, { 39, 7142, 19 },
    { 40, 7142, 19 }, { 41, 7142, 19 }, { 42, 7142, 19 }, { 43, 7142, 19 },
    { 44, 7142, 19 }, { 45, 7142, 19 }, { 46, 7142, 19 }, { 47, 7142, 19 },
    { 48, 6666, 18 }, { 49, 6666, 18 }, { 50, 6666, 18 }, { 51, 6666, 18 },
    { 52, 6666, 18 }, { 53, 6666, 18 }, { 54, 6666, 18 }, { 55, 6666, 18 },
    { 56, 6666, 18 }, { 57, 6666, 18 }, { 58, 6666, 18 }, { 59, 6666, 18 },
    { 60, 6666, 17 }, { 61, 6666, 17 }, { 62, 6666, 17 }, { 63, 6666, 17 },
    { 64, 6666, 17 }, { 65, 6666, 17 }, { 66, 6666, 17 }, { 67, 6666, 17 },
    { 68, 6666, 17 },
    /* Spur 37.0 und 38.5: nibscan prueft hier NICHT auf Sektoren
     * (nibscan.c:589, "everything is a CBM error above track 35"), also
     * heisst 0 hier "nicht gezaehlt", nicht "fehlerfrei". */
    { 72, 6666,  0 }, { 75, 6666,  0 },
};

/* KEINE Kopie von nibtools `sector_map` hier — und das ist kein Verzicht.
 *
 * Eine erste Fassung legte die Tafel aus gcr.c:28-35 als Array daneben, um
 * sie gegen `uft_cbm_sectors_per_track()` zu rechnen. Tor `audit_cbm_zonen.py`
 * hat den Commit dafuer abgewiesen: es fuehrt die CBM-Zonenlaengen mit
 * FALLENDER Grundlinie (23 Fundstellen im Baum, Richtung Zusammenfuehrung auf
 * `uft_cbm_track_capacity()`), und meine waere die 24. gewesen. Gegenprobe
 * gemessen: ohne diese Datei 23, mit ihr 24 — also genau diese Tafel.
 *
 * Die Aussage steht trotzdem, und zwar staerker: nibtools' Zonenwerte lassen
 * sich aus seiner AUSGABE ablesen statt aus seinem Quelltext. Auf einer Spur,
 * deren Sollsektoren nibscan samt und sonders vermisst, ist die Zahl der
 * `[E2Sn]`-Marken genau sein `sector_map[track/2]`. Genau das prueft
 * t_wo_nibscan_keinen_sollsektor_findet_...: sein `faelle == 68` haelt nur,
 * wenn fuer JEDE dieser 68 Spuren die Markenzahl mit UFTs
 * `uft_cbm_sectors_per_track()` uebereinstimmt. Waere eine Zone verschieden,
 * fiele die Spur aus der Zaehlung und die 68 waere kleiner.
 *
 * Ein Quelltextvergleich haette zwei Tafeln verglichen; so werden eine
 * Messung und eine Rechnung verglichen. Der Vollstaendigkeit halber: der
 * Vergleich beider Tafeln ist MF-1503 als Wegwerf-Messung gelaufen und ergab
 * 42 von 42 gleich, einschliesslich der Spuren 36-42; er steht in
 * `docs/ORACLES.md` beim nibscan-Eintrag. Ueber die Ausgabe belegt sind davon
 * die Spuren bis 35 — jenseits davon zaehlt nibscan nicht mehr mit (s.u.).
 */

/* Die Grenze, bis zu der nibscans Sektorurteil ueberhaupt eines ist. */
#define NIB_SEKTORURTEIL_BIS_SPUR 35

/* ── Was UFT sagt ─────────────────────────────────────────────────────── */

typedef struct {
    int ht;
    uint32_t byte;          /* bitcell_count / 8 */
    uint32_t sektoren;      /* Header-Bloecke auf der Rohspur */
    uint32_t bad_gcr;
    int halbspur;
} uft_spur_t;

#define MAX_SPUREN 128

static const char *img(const char *name) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_RESTRICTED_DIR, name);
    return p;
}

/** Sammelt UFTs Spurtafel ueber den Pfad der Oberflaeche.
 *  @return Zahl der belegten Plaetze, oder -1 wenn das Abbild fehlt. */
static int uft_tafel(const char *name, uft_spur_t *out, int max) {
    g64_image_t *g = NULL;
    if (g64_load(img(name), &g) != 0 || !g) return -1;

    int n = 0;
    for (int ht = 0; ht < G64_MAX_TRACKS && n < max; ht++) {
        const uint8_t *d = NULL;
        size_t len = 0;
        uint8_t speed = 0;
        if (g64_get_track(g, ht, &d, &len, &speed) != 0 || !d || len == 0)
            continue;

        ufm_c64_track_metrics_t m;
        memset(&m, 0, sizeof(m));
        if (!ufm_c64_metrics_from_gcr(d, len, ht, UFM_C64_SPEED_ZONE_AUTO, &m))
            continue;

        out[n].ht = ht;
        out[n].byte = m.bitcell_count / 8u;
        out[n].sektoren = m.sector_count;
        out[n].bad_gcr = m.bad_gcr_count;
        out[n].halbspur = m.has_half_track ? 1 : 0;
        n++;
    }
    g64_free(g);
    return n;
}

static const uft_spur_t *finde(const uft_spur_t *t, int n, int ht) {
    for (int i = 0; i < n; i++) if (t[i].ht == ht) return &t[i];
    return NULL;
}

/* Gefuellt von main(), damit jeder Test dieselbe Messung sieht. */
static uft_spur_t g_alien[MAX_SPUREN], g_bounty[MAX_SPUREN];
static int g_n_alien = 0, g_n_bounty = 0;

#define N_NIB_ALIEN  ((int)(sizeof(NIB_ALIEN) / sizeof(NIB_ALIEN[0])))
#define N_NIB_BOUNTY ((int)(sizeof(NIB_BOUNTY) / sizeof(NIB_BOUNTY[0])))

/* ── 1. Spurbestand ───────────────────────────────────────────────────── */

TEST(spurbestand_stimmt_mit_nibscan) {
    /* Die Zahl belegter Halbspurplaetze ist die erste Groesse, bei der zwei
     * Werkzeuge unabhaengig dasselbe sagen muessen. Dass 71 die richtige Zahl
     * ist, haelt auch `test_c64_protection_real_corpus.c:145` fest — dort als
     * Grund, warum eine Schleife nicht bei Platz 2 beginnen darf (sie las 69). */
    ASSERT(g_n_alien == N_NIB_ALIEN);
    ASSERT(g_n_alien == 40);
    ASSERT(g_n_bounty == N_NIB_BOUNTY);
    ASSERT(g_n_bounty == 71);

    for (int i = 0; i < N_NIB_ALIEN; i++)
        ASSERT(finde(g_alien, g_n_alien, NIB_ALIEN[i].ht) != NULL);
    for (int i = 0; i < N_NIB_BOUNTY; i++)
        ASSERT(finde(g_bounty, g_n_bounty, NIB_BOUNTY[i].ht) != NULL);
}

/* ── 2. Spurlaenge je Spur ────────────────────────────────────────────── */

TEST(spurlaenge_je_spur_stimmt_mit_nibscan) {
    /* Die staerkste Eichgroesse: beide Seiten lesen sie aus derselben
     * G64-Spur, ohne Rechnung dazwischen. UFT: bitcell_count / 8, wobei
     * bitcell_count = gcr_len * 8 (`ufm_c64_metrics.c:147-148`). */
    for (int i = 0; i < N_NIB_ALIEN; i++) {
        const uft_spur_t *u = finde(g_alien, g_n_alien, NIB_ALIEN[i].ht);
        ASSERT(u && u->byte == (uint32_t)NIB_ALIEN[i].byte);
    }
    for (int i = 0; i < N_NIB_BOUNTY; i++) {
        const uft_spur_t *u = finde(g_bounty, g_n_bounty, NIB_BOUNTY[i].ht);
        ASSERT(u && u->byte == (uint32_t)NIB_BOUNTY[i].byte);
    }
}

/* ── 3. Halbspuren ───────────────────────────────────────────────────── */

TEST(halbspuren_stimmen_mit_nibscan) {
    /* nibscan schreibt die Halbspur als `t.5`, UFT fuehrt `has_half_track`.
     * Unter der Zaehlung der DATEI (MF-928) ist das die Paritaet des Platzes. */
    int uft_halbe = 0, nib_halbe = 0;
    for (int i = 0; i < N_NIB_BOUNTY; i++) {
        const uft_spur_t *u = finde(g_bounty, g_n_bounty, NIB_BOUNTY[i].ht);
        ASSERT(u != NULL);
        int nib = (NIB_BOUNTY[i].ht % 2 == 1) ? 1 : 0;
        ASSERT(u->halbspur == nib);
        uft_halbe += u->halbspur;
        nib_halbe += nib;
    }
    ASSERT(uft_halbe == nib_halbe);
    ASSERT(uft_halbe == 35);

    /* Und die Gegenprobe: das saubere Abbild hat keine. */
    for (int i = 0; i < N_NIB_ALIEN; i++) {
        const uft_spur_t *u = finde(g_alien, g_n_alien, NIB_ALIEN[i].ht);
        ASSERT(u && u->halbspur == 0);
    }
}

/* ── 4. Sektoren, sauberes Abbild: Gleichung ─────────────────────────── */

TEST(sauberes_abbild_liefert_alle_sollsektoren) {
    /* Hier — und nur hier — duerfen nibscans Sektorurteil und UFTs
     * Headerzahl gleichgesetzt werden: nibscan vermisst keinen einzigen
     * Sollsektor, also traegt jeder gefundene Header auch eine Sollnummer. */
    uint32_t summe = 0, soll_summe = 0;
    for (int i = 0; i < N_NIB_ALIEN; i++) {
        const uft_spur_t *u = finde(g_alien, g_n_alien, NIB_ALIEN[i].ht);
        ASSERT(u != NULL);
        ASSERT(NIB_ALIEN[i].fehlmarken == 0);
        int spur = NIB_ALIEN[i].ht / 2 + 1;
        int soll = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        ASSERT(u->sektoren == (uint32_t)soll);
        summe += u->sektoren;
        soll_summe += (uint32_t)soll;
    }
    ASSERT(summe == soll_summe);
    ASSERT(summe == 768);
}

/* ── 5. Sektoren, geschuetztes Abbild: Richtung ──────────────────────── */

TEST(wo_nibscan_keinen_sollsektor_findet_behauptet_uft_keine_vollstaendigkeit) {
    /* Auf dem geschuetzten Abbild beantworten die beiden Zahlen verschiedene
     * Fragen (siehe Kopf, Punkt 3), also steht hier eine Richtung statt einer
     * Gleichung: wo nibscan KEINEN Sollsektor findet, darf UFT nicht
     * behaupten, die Standardgeometrie sei vollstaendig da.
     *
     * Gemessen MF-1503: 68 solche Plaetze, 0 Verstoesse. Die frueheren
     * "44 Abweichungen" einer Gleichungspruefung waren keine.
     *
     * UND DIESE ZUSAGE TRAEGT EINE ZWEITE: die Zahl 68 haelt nur, wenn fuer
     * jede dieser Spuren `fehlmarken == soll` gilt — also wenn nibtools'
     * Zonenwert, abgelesen aus seiner eigenen AUSGABE, mit UFTs
     * `uft_cbm_sectors_per_track()` uebereinstimmt. Waere eine Zone
     * verschieden, fiele die Spur aus der Zaehlung und 68 waere kleiner.
     * Deshalb liegt hier keine Kopie der Zonentafel daneben (siehe den
     * Hinweis bei NIB_SEKTORURTEIL_BIS_SPUR und `audit_cbm_zonen.py`):
     * verglichen werden eine Messung und eine Rechnung, nicht zwei Tafeln. */
    int faelle = 0;
    for (int i = 0; i < N_NIB_BOUNTY; i++) {
        int spur = NIB_BOUNTY[i].ht / 2 + 1;
        if (spur > NIB_SEKTORURTEIL_BIS_SPUR) continue;
        int soll = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        if (NIB_BOUNTY[i].fehlmarken != soll) continue;

        const uft_spur_t *u = finde(g_bounty, g_n_bounty, NIB_BOUNTY[i].ht);
        ASSERT(u != NULL);
        ASSERT(u->sektoren < (uint32_t)soll);
        faelle++;
    }
    ASSERT(faelle == 68);

    /* Und die Unterscheidungskraft selbst: das geschuetzte Abbild darf nicht
     * annaehernd so viele Header liefern wie das saubere. Gemessen MF-1503:
     * 82 gegen 768, bei MEHR Spuren (71 gegen 40). */
    uint32_t bounty_summe = 0;
    for (int i = 0; i < g_n_bounty; i++) bounty_summe += g_bounty[i].sektoren;
    ASSERT(bounty_summe == 82);
}

/* ── 6. Die Semantikfalle jenseits Spur 35 ───────────────────────────── */

TEST(jenseits_spur_35_zaehlt_nibscan_nicht_mit) {
    /* Diese Zusage schuetzt keinen Code, sondern den naechsten Leser dieser
     * Datei: er darf die 0 in den letzten zwei Tafelzeilen nicht als
     * "fehlerfrei" lesen. `nibscan.c:589-592` prueft nur `track/2 <= 35`,
     * mit dem Kommentar "everything is a CBM error above track 35".
     *
     * Gemessen MF-1503: beide Plaetze tragen 0 Marken UND UFT findet dort 0
     * Header. Waere die 0 ein Fehlerfrei-Urteil, waere das ein Widerspruch;
     * so ist es dieselbe Aussage. */
    /* Dass es solche Plaetze ueberhaupt geben darf, sagt UFT selbst: seine
     * 1541-Familie reicht bis Spur 42, so weit wie nibtools `sector_map`. */
    ASSERT(uft_cbm_max_track(UFT_CBM_1541) == 42);

    int geprueft = 0;
    for (int i = 0; i < N_NIB_BOUNTY; i++) {
        int spur = NIB_BOUNTY[i].ht / 2 + 1;
        if (spur <= NIB_SEKTORURTEIL_BIS_SPUR) continue;
        const uft_spur_t *u = finde(g_bounty, g_n_bounty, NIB_BOUNTY[i].ht);
        ASSERT(u != NULL);
        ASSERT(NIB_BOUNTY[i].fehlmarken == 0);   /* nicht gezaehlt */
        ASSERT(u->sektoren == 0);                /* und nichts gefunden */
        geprueft++;
    }
    ASSERT(geprueft == 2);
}

/* ── 7. P3-665: der Zaehler traegt den Namen nicht ───────────────────── */

TEST(bad_gcr_zaehler_ist_keine_bytezahl) {
    /* Feststellung, kein Fix. `ufm_c64_metrics.c:117-118` zaehlt hoechstens
     * EINMAL je Sync-Block und bricht dann ab; `ufm_c64_scheme_detect.c:167`
     * legt darauf die Schwelle `> 10` und meldet "Invalid GCR byte
     * sequences" mit Konfidenz 70 — ein Text fuer die Byte-Lesart.
     *
     * Gemessen MF-1503 an bountybob: nibscan 244 022 ungueltige Byte, UFT
     * Summe 208 ueber 71 Spuren, Hoechstwert je Spur 15, und damit GENAU EINE
     * Spur ueber der Schwelle. Bei aliensyndrome: nibscan 223, UFT 0.
     *
     * Dieser Test ist die Sperre gegen die stille halbe Aenderung: wer den
     * Zaehler auf Bytes umstellt, wird hier rot und muss die Schwelle
     * mitziehen. Er sagt NICHT, dass eine der beiden Lesarten richtig ist —
     * das ist P3-665 und braucht eine Entscheidung, keinen Test. */
    uint32_t summe = 0, hoechst = 0, ueber_schwelle = 0;
    for (int i = 0; i < g_n_bounty; i++) {
        summe += g_bounty[i].bad_gcr;
        if (g_bounty[i].bad_gcr > hoechst) hoechst = g_bounty[i].bad_gcr;
        if (g_bounty[i].bad_gcr > 10) ueber_schwelle++;
    }
    ASSERT(summe == 208);
    ASSERT(hoechst == 15);
    ASSERT(ueber_schwelle == 1);

    /* Der Groessenordnungsabstand selbst, damit er benannt bleibt: nibscans
     * Bytezahl ist mehr als das Tausendfache von UFTs Blockzahl. */
    ASSERT(summe * 1000u < 244022u);

    uint32_t alien_summe = 0;
    for (int i = 0; i < g_n_alien; i++) alien_summe += g_alien[i].bad_gcr;
    ASSERT(alien_summe == 0);   /* nibscan sah dort 223 Byte */
}

int main(void) {
    printf("=== C64 G64: Differenzlauf gegen nibscan (MF-1503) ===\n");

    g_n_alien = uft_tafel("c64pp_aliensyndrome.g64", g_alien, MAX_SPUREN);
    g_n_bounty = uft_tafel("c64pp_bountybob.g64", g_bounty, MAX_SPUREN);
    if (g_n_alien < 0 || g_n_bounty < 0) {
        printf("SKIP: local-only corpus images absent (%s)\n",
               UFT_CORPUS_RESTRICTED_DIR);
        return SKIP_EXIT;
    }
    printf("  UFT gelesen: %d Plaetze (alien), %d Plaetze (bounty)\n",
           g_n_alien, g_n_bounty);

    RUN(spurbestand_stimmt_mit_nibscan);
    RUN(spurlaenge_je_spur_stimmt_mit_nibscan);
    RUN(halbspuren_stimmen_mit_nibscan);
    RUN(sauberes_abbild_liefert_alle_sollsektoren);
    RUN(wo_nibscan_keinen_sollsektor_findet_behauptet_uft_keine_vollstaendigkeit);
    RUN(jenseits_spur_35_zaehlt_nibscan_nicht_mit);
    RUN(bad_gcr_zaehler_ist_keine_bytezahl);

    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
