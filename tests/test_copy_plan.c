/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_copy_plan.c
 * @brief Vier Dimensionen, und ein Konflikt gilt nur innerhalb einer
 *        Stufe (MF-1232)
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * K1  Die vier Achsen sind unabhaengig: alle 6 x 5 x 4 x 3 = 360
 *     Kombinationen sind konstruierbar. Mit einer einzigen Achse waeren
 *     es 11 — das ist der ganze Anlass.
 * K2  **`read.passes` x `write.enabled` ist KEIN Konflikt.** Die zentrale
 *     Zusage: das gelieferte Parametermodell fuehrt ihn als HART
 *     (`k_cfl_passes[]`), und fuer einen Kopiervorgang ist das falsch —
 *     fuenfmal lesen, einmal schreiben ist der Normalfall.
 * K3  Zwei Parameter DERSELBEN Stufe sind sehr wohl konfliktfaehig. Ohne
 *     diese Gegenprobe waere K2 auch dann gruen, wenn die Funktion
 *     schlicht immer `false` gaebe (Klasse `erkenner_der_nie_nein_sagt`).
 * K4  Die Erhaltung PROTECTED verlangt mindestens die Bitstromebene.
 * K5  PROTECTED ohne TIMING **und** WEAK_BITS ist ein harter Befund —
 *     gemessen (MF-1231) sagt KEIN Plugin beide zugleich zu.
 * K6  EVIDENCE und FAST widersprechen sich.
 * K7  EVIDENCE erzwingt: Quelle nicht beschreibbar, kein Verlust, Hash
 *     und Herkunft.
 * K8  DEEP erzwingt 5 Durchlaeufe, 5 Umdrehungen und eine Statusdatei.
 * K9  Reihenfolge: die Sicherheit gewinnt ueber die Strategie. Auf der
 *     Bitstromebene ist `allow_loss` sonst einstellbar; unter EVIDENCE
 *     ist es auf false festgelegt.
 * K10 Ebenenverteilung: `layout.gap3` ist auf der Dateiebene ausdruecklich
 *     verboten und auf der Spurebene aktiv.
 * K11 Auf der Flussebene sind die gemessenen Quellwerte nur lesbar.
 * K12 Namen: gueltige Werte ergeben einen Namen, ungueltige NULL.
 * K13 `check()` zaehlt auch ohne Puffer richtig.
 * K14 Bitgenau ist nicht Fluss-bitgenau: `flux-timing-exact` auf der
 *     Bitstromebene ist ein Befund.
 *
 * K15-K23 sind spaeter dazugekommen und stehen bei ihrem Code; dass sie
 * hier fehlen, ist eine Luecke dieser Aufzaehlung und keine Aussage
 * ueber ihren Wert.
 * K24 Die LAENGENABFRAGE des JSON-Schreibers (Nullzeiger, Groesse 0).
 *     K22 misst den zu kleinen Puffer; den Weg, den die Oberflaeche
 *     geht, misst erst K24 — und ohne ihn haengt die ganze Anzeige an
 *     einem ungeprueften Zweig (MF-1238).
 * K25 Die acht Faehigkeitsflaggen haben je einen eigenen Namen, und
 *     „keine", „mehrere" und „unbekannt" ergeben NULL statt einer
 *     erfundenen Zeichenkette. Gefahren ueber `uft_copy_cap_count()`,
 *     nicht ueber eine abgeschriebene Liste (MF-636/MF-1238).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>   /* MF-1238: K24 misst mit eigenem Puffer */

#include "uft/core/uft_copy_plan.h"
/* MF-1263: fuer `uft_convert_options_t` — P7 prueft, was der Plan in
 * den Optionen einer echten Wandlung setzt. */
#include "uft/uft_types.h"

static int fehler = 0;

#define PRUEFE(bed, ...)                                                   \
    do {                                                                   \
        if (!(bed)) {                                                      \
            printf("  [ROT] ");                                            \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            fehler++;                                                      \
        }                                                                  \
    } while (0)

static bool hat_befund(const uft_copy_plan_t *p, uint32_t caps,
                       const char *id, bool *hart)
{
    uft_copy_finding_t f[16];
    size_t n = uft_copy_plan_check(p, caps, f, 16);
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; i++) {
        if (f[i].id && strcmp(f[i].id, id) == 0) {
            if (hart) *hart = f[i].hard;
            return true;
        }
    }
    return false;
}

static const char *erzwungen(const uft_copy_plan_t *p, const char *param)
{
    uft_copy_enforced_t e[64];
    size_t n = uft_copy_plan_enforced(p, e, 64);
    if (n > 64) n = 64;
    const char *letzter = NULL;
    for (size_t i = 0; i < n; i++)
        if (e[i].param && strcmp(e[i].param, param) == 0)
            letzter = e[i].value;    /* spaetere Dimension gewinnt */
    return letzter;
}

int main(void)
{
    printf("== Kopierplan in vier Dimensionen (MF-1232) ==\n");

    /* K1 — die Achsen sind unabhaengig */
    {
        size_t echt = 0, mit_auto = 0;
        for (int l = 0; l < UFT_COPY_LEVEL_N; l++)
        for (int s = 0; s < UFT_READ_STRATEGY_N; s++)
        for (int e = 0; e < UFT_PRESERVE_N; e++)
        for (int p = 0; p < UFT_POLICY_N; p++) {
            uft_copy_plan_t pl = uft_copy_plan_default();
            pl.level        = (uft_copy_level_t)l;
            pl.strategy     = (uft_read_strategy_t)s;
            pl.preservation = (uft_preservation_t)e;
            pl.policy       = (uft_copy_policy_t)p;
            (void)uft_copy_plan_check(&pl, 0xFFFFFFFFu, NULL, 0);
            mit_auto++;
            if (l < UFT_COPY_LEVEL_ECHT) echt++;
        }
        /* 6 echte Ebenen x 5 x 4 x 3 = 360 Plaene, die wirklich
         * gerechnet werden — und mit der Zeile „Automatisch" sind es
         * 420 Zeilen in der Auswahl. Beide Zahlen stehen hier, damit
         * niemand die eine fuer die andere haelt. */
        PRUEFE(echt == 360, "K1: %zu echte Kombinationen, 360 erwartet", echt);
        PRUEFE(mit_auto == 420,
               "K1: %zu Kombinationen mit Automatik, 420 erwartet", mit_auto);
    }

    /* K2 — der eigentliche Punkt */
    PRUEFE(!uft_copy_conflict_applies("read.passes", "write.enabled"),
           "K2: read.passes x write.enabled gilt als Konflikt — genau der "
           "Fehler, den die Stufen beheben sollen");
    PRUEFE(uft_copy_param_stage("read.passes") == UFT_STAGE_READ,
           "K2: read.passes liegt nicht in der Lesestufe");
    PRUEFE(uft_copy_param_stage("write.enabled") == UFT_STAGE_WRITE,
           "K2: write.enabled liegt nicht in der Schreibstufe");

    /* K3 — und die Funktion sagt nicht immer nein */
    PRUEFE(uft_copy_conflict_applies("read.passes", "read.retries"),
           "K3: zwei Parameter DERSELBEN Stufe gelten nicht als "
           "konfliktfaehig — dann sagt die Funktion nie ja");
    PRUEFE(uft_copy_param_stage("gibtesnicht.foo") == UFT_STAGE_N,
           "K3: ein unbekannter Namensraum wird eingeordnet statt "
           "als unbekannt gemeldet");
    PRUEFE(!uft_copy_conflict_applies("gibtesnicht.a", "gibtesnicht.b"),
           "K3: zwei unbekannte Parameter gelten als konfliktfaehig");

    /* K4 — Erhaltung verlangt eine Mindestebene */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.preservation = UFT_PRESERVE_PROTECTED;
        p.level = UFT_COPY_SECTOR;
        bool hart = false;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "ebene_zu_hoch", &hart) && hart,
               "K4: PROTECTED auf der Sektorebene ergibt keinen harten "
               "Befund");
        p.level = UFT_COPY_FLUX;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "ebene_zu_hoch", NULL),
               "K4: PROTECTED auf der Flussebene meldet faelschlich "
               "eine zu niedrige Ebene");
    }

    /* K5 — und das Format muss es tragen */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.preservation = UFT_PRESERVE_PROTECTED;
        p.level = UFT_COPY_FLUX;
        bool hart = false;
        const uint32_t nur_timing = UFT_CAP_FLUX_IO | UFT_CAP_TIMING;
        PRUEFE(hat_befund(&p, nur_timing, "schutz_nicht_tragbar", &hart) && hart,
               "K5: TIMING allein reicht fuer PROTECTED — gemessen braucht "
               "es auch WEAK_BITS");
        const uint32_t beide = nur_timing | UFT_CAP_WEAK_BITS;
        PRUEFE(!hat_befund(&p, beide, "schutz_nicht_tragbar", NULL),
               "K5: mit beiden Flaggen wird PROTECTED trotzdem abgelehnt");
    }

    /* K6 — Beweis und Eile vertragen sich nicht */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.policy = UFT_POLICY_EVIDENCE;
        p.strategy = UFT_READ_FAST;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "beweis_und_eile", NULL),
               "K6: EVIDENCE mit FAST wird nicht beanstandet");
        p.strategy = UFT_READ_DEEP;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "beweis_und_eile", NULL),
               "K6: EVIDENCE mit DEEP wird faelschlich beanstandet");
    }

    /* K7 — was EVIDENCE erzwingt */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.policy = UFT_POLICY_EVIDENCE;
        const char *v;
        v = erzwungen(&p, "source.write_enabled");
        PRUEFE(v && strcmp(v, "false") == 0,
               "K7: EVIDENCE sperrt die Quelle nicht (%s)", v ? v : "nichts");
        v = erzwungen(&p, "allow_loss");
        PRUEFE(v && strcmp(v, "false") == 0,
               "K7: EVIDENCE erlaubt Verlust (%s)", v ? v : "nichts");
        PRUEFE(erzwungen(&p, "hash.enabled") != NULL,
               "K7: EVIDENCE verlangt keinen Hash");
        PRUEFE(erzwungen(&p, "provenance.enabled") != NULL,
               "K7: EVIDENCE verlangt keine Herkunft");
    }

    /* K8 — was DEEP erzwingt */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.strategy = UFT_READ_DEEP;
        const char *v = erzwungen(&p, "read.passes");
        PRUEFE(v && strcmp(v, "5") == 0,
               "K8: DEEP setzt read.passes nicht auf 5 (%s)", v ? v : "nichts");
        v = erzwungen(&p, "read.revolutions");
        PRUEFE(v && strcmp(v, "5") == 0,
               "K8: DEEP setzt read.revolutions nicht auf 5 (%s)",
               v ? v : "nichts");
        PRUEFE(erzwungen(&p, "status_file") != NULL,
               "K8: DEEP verlangt keine Statusdatei");
    }

    /* K9 — die Sicherheit gewinnt ueber die Strategie */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_BITSTREAM;
        p.policy = UFT_POLICY_NORMAL;
        const char *frei = erzwungen(&p, "allow_loss");
        PRUEFE(frei == NULL,
               "K9: allow_loss ist ohne Richtlinie schon festgelegt (%s)",
               frei ? frei : "");
        p.policy = UFT_POLICY_EVIDENCE;
        const char *fest = erzwungen(&p, "allow_loss");
        PRUEFE(fest && strcmp(fest, "false") == 0,
               "K9: unter EVIDENCE ist allow_loss nicht auf false");
    }

    /* K10 — Ebenenverteilung */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_FILE;
        PRUEFE(uft_copy_param_state(&p, "layout.gap3", NULL)
                   == UFT_PSTATE_FORBIDDEN,
               "K10: layout.gap3 ist auf der Dateiebene nicht verboten");
        p.level = UFT_COPY_TRACK;
        PRUEFE(uft_copy_param_state(&p, "layout.gap3", NULL)
                   == UFT_PSTATE_ACTIVE,
               "K10: layout.gap3 ist auf der Spurebene nicht aktiv");
    }

    /* K11 — gemessene Quellwerte sind nur lesbar */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_FLUX;
        PRUEFE(uft_copy_param_state(&p, "measured_rpm", NULL)
                   == UFT_PSTATE_READONLY,
               "K11: measured_rpm ist auf der Flussebene einstellbar");
    }

    /* K12 — Namen */
    PRUEFE(uft_copy_level_name(UFT_COPY_FLUX) != NULL, "K12: Ebene ohne Namen");
    PRUEFE(uft_copy_level_name(UFT_COPY_LEVEL_N) == NULL,
           "K12: ungueltige Ebene bekommt einen Namen");
    PRUEFE(uft_copy_strategy_name(UFT_READ_SALVAGE) != NULL,
           "K12: Strategie ohne Namen");
    PRUEFE(uft_copy_policy_name(UFT_POLICY_N) == NULL,
           "K12: ungueltige Richtlinie bekommt einen Namen");

    /* K13 — zaehlen ohne Puffer */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.preservation = UFT_PRESERVE_PROTECTED;
        p.level = UFT_COPY_SECTOR;
        size_t ohne = uft_copy_plan_check(&p, 0, NULL, 0);
        uft_copy_finding_t f[16];
        size_t mit = uft_copy_plan_check(&p, 0, f, 16);
        PRUEFE(ohne == mit && ohne > 0,
               "K13: Zaehlung ohne Puffer (%zu) weicht ab von der mit (%zu)",
               ohne, mit);
        PRUEFE(!uft_copy_plan_is_executable(&p, 0),
               "K13: ein Plan mit hartem Befund gilt als ausfuehrbar");
    }

    /* K14 — bitgenau ist nicht flussgleich */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.preservation = UFT_PRESERVE_BIT_EXACT;
        p.level = UFT_COPY_BITSTREAM;
        p.exact_kind = UFT_EXACT_FLUX_TIMING;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "genauigkeit_zu_hoch", NULL),
               "K14: flux-timing-exact ohne Flussebene wird nicht "
               "beanstandet");
        p.exact_kind = UFT_EXACT_TRACK_BIT;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "genauigkeit_zu_hoch", NULL),
               "K14: track-bit-exact auf der Bitstromebene wird "
               "faelschlich beanstandet");
    }

    /* ── MF-1234: die elf Punkte, die in der ersten Fassung fehlten ── */

    /* K15 — „Automatisch" ist eine Zeile der Auswahl, aber keine Ebene,
     *       auf der gerechnet wird. Sie muss aufgeloest werden, und die
     *       Aufloesung folgt NUR den Faehigkeiten. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_AUTO;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "ebene_offen", NULL),
               "K15: ein unaufgeloestes AUTO wird nicht gemeldet");

        uft_copy_plan_t f = uft_copy_plan_resolve(&p, UFT_CAP_FLUX_IO);
        PRUEFE(f.level == UFT_COPY_FLUX,
               "K15: mit Fluss muss AUTO auf die Flussebene gehen");
        uft_copy_plan_t s = uft_copy_plan_resolve(&p, 0);
        PRUEFE(s.level == UFT_COPY_SECTOR,
               "K15: ohne jede Zusage bleibt die Sektorebene");
        uft_copy_plan_t d = uft_copy_plan_resolve(&p, UFT_CAP_FILESYSTEM);
        PRUEFE(d.level == UFT_COPY_FILE,
               "K15: mit erkanntem Dateisystem die Dateiebene");

        uft_copy_plan_t k = uft_copy_plan_default();
        k.level = UFT_COPY_TRACK;
        PRUEFE(uft_copy_plan_resolve(&k, UFT_CAP_FLUX_IO).level
                   == UFT_COPY_TRACK,
               "K15: resolve() aendert eine getroffene Wahl");
    }

    /* K16 — TrackCopy hat zwei Spielarten, und „raw" gilt nur dort. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_TRACK;
        p.track_mode = UFT_TRACK_RAW;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "spurart_ohne_spurebene", NULL),
               "K16: raw auf der Spurebene wird beanstandet");
        p.level = UFT_COPY_SECTOR;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "spurart_ohne_spurebene", NULL),
               "K16: raw ausserhalb der Spurebene wird durchgelassen");
        PRUEFE(uft_copy_track_mode_name(UFT_TRACK_RAW) != NULL &&
               uft_copy_track_mode_name(UFT_TRACK_MODE_N) == NULL,
               "K16: die Spurart hat keine sauberen Namen");
    }

    /* K17 — vier GCR-Verfahren, nicht ein Schalter. */
    {
        PRUEFE(UFT_GCR_N == 4, "K17: es sind nicht vier GCR-Verfahren");
        for (int i = 0; i < UFT_GCR_N; i++)
            PRUEFE(uft_copy_gcr_name((uft_gcr_variant_t)i) != NULL,
                   "K17: GCR-Verfahren %d ohne Namen", i);
        PRUEFE(uft_copy_gcr_name(UFT_GCR_N) == NULL,
               "K17: ungueltiges GCR-Verfahren bekommt einen Namen");
    }

    /* K18 — sieben Abstimmungsverfahren, und nur bei Consensus. */
    {
        PRUEFE(UFT_VOTE_N == 7, "K18: es sind nicht sieben Verfahren");
        for (int i = 0; i < UFT_VOTE_N; i++)
            PRUEFE(uft_copy_vote_name((uft_vote_method_t)i) != NULL,
                   "K18: Verfahren %d ohne Namen", i);
        uft_copy_plan_t p = uft_copy_plan_default();
        p.vote = UFT_VOTE_PER_BIT;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "abstimmung_ohne_consensus", NULL),
               "K18: ein Verfahren ohne Consensus wird durchgelassen");
        p.strategy = UFT_READ_CONSENSUS;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "abstimmung_ohne_consensus", NULL),
               "K18: mit Consensus wird das Verfahren beanstandet");
    }

    /* K19 — requiresCapabilities je PARAMETER, nicht nur je Plan. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_FLUX;

        PRUEFE(uft_copy_param_requires("flux.dewarp")
                   == (uint32_t)UFT_CAP_FLUX_IO,
               "K19: flux.dewarp verlangt keine Flussfaehigkeit");
        PRUEFE(uft_copy_param_requires("source") == 0u,
               "K19: ein allgemeiner Parameter verlangt etwas");

        PRUEFE(uft_copy_param_state_caps(&p, 0, "flux.dewarp", NULL)
                   == UFT_PSTATE_HIDDEN,
               "K19: ohne Flusszusage bleibt flux.dewarp sichtbar");
        PRUEFE(uft_copy_param_state_caps(&p, UFT_CAP_FLUX_IO,
                                         "flux.dewarp", NULL)
                   == UFT_PSTATE_ACTIVE,
               "K19: mit Flusszusage ist flux.dewarp nicht aktiv");
    }

    /* K20 — BAMCopy nur bei Commodore-BAM UND erkanntem Dateisystem. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_FILE;
        p.file_special = UFT_FILE_BAM;
        bool hart = false;
        PRUEFE(hat_befund(&p, UFT_CAP_FILESYSTEM, "bam_ohne_bam", &hart) && hart,
               "K20: BAM ohne Commodore-BAM wird durchgelassen");
        const uint32_t beides = (uint32_t)UFT_CAP_FILESYSTEM |
                                (uint32_t)UFT_CAP_CBM_BAM;
        PRUEFE(!hat_befund(&p, beides, "bam_ohne_bam", NULL),
               "K20: mit BAM und Dateisystem wird trotzdem abgelehnt");
        PRUEFE(uft_copy_param_state_caps(&p, UFT_CAP_FILESYSTEM,
                                         "bam.validate", NULL)
                   == UFT_PSTATE_HIDDEN,
               "K20: bam.validate erscheint ohne Commodore-BAM");
    }

    /* K21 — Hash: SHA-256 Pflicht, CRC32 nie allein. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.policy = UFT_POLICY_EVIDENCE;
        p.strategy = UFT_READ_STANDARD;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "hash_ohne_sha256", NULL),
               "K21: die Vorgabe traegt kein SHA-256");
        p.hashes = (uint32_t)UFT_HASH_CRC32;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "hash_ohne_sha256", NULL),
               "K21: fehlendes SHA-256 wird nicht beanstandet");
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "hash_nur_crc32", NULL),
               "K21: CRC32 allein wird durchgelassen");
    }

    /* K22 — der Plan als JSON, in der vorgegebenen Gestalt. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_FLUX;
        p.strategy = UFT_READ_CONSENSUS;
        p.preservation = UFT_PRESERVE_PROTECTED;
        p.policy = UFT_POLICY_EVIDENCE;
        p.vote = UFT_VOTE_WEIGHTED_CONFIDENCE;

        char j[4096];
        size_t len = uft_copy_plan_to_json(&p, j, sizeof(j));
        PRUEFE(len > 0 && len < sizeof(j),
               "K22: JSON leer oder zu gross (%zu)", len);
        PRUEFE(strstr(j, "\"copyPlan\"") != NULL,
               "K22: kein copyPlan-Abschnitt");
        PRUEFE(strstr(j, "\"level\": \"flux\"") != NULL,
               "K22: die Ebene steht nicht als flux darin");
        PRUEFE(strstr(j, "\"strategy\": \"consensus\"") != NULL,
               "K22: die Strategie fehlt");
        PRUEFE(strstr(j, "\"preservation\": \"protected\"") != NULL,
               "K22: die Erhaltung fehlt");
        PRUEFE(strstr(j, "\"policy\": \"evidence\"") != NULL,
               "K22: die Richtlinie fehlt");
        PRUEFE(strstr(j, "\"preserve\"") != NULL,
               "K22: kein preserve-Abschnitt");
        PRUEFE(strstr(j, "\"weakBits\": true") != NULL,
               "K22: preserve.weakBits fehlt oder ist nicht camelCase");
        PRUEFE(strstr(j, "\"evidence\"") != NULL,
               "K22: kein evidence-Abschnitt");
        PRUEFE(strstr(j, "\"hash\": [\"sha256\"]") != NULL,
               "K22: der Hashsatz fehlt");

        /* Ein zu kleiner Puffer muss ERKENNBAR abschneiden, nicht
         * still. Dieselbe Zusage wie bei snprintf. */
        char klein[16];
        size_t voll = uft_copy_plan_to_json(&p, klein, sizeof(klein));
        PRUEFE(voll >= sizeof(klein),
               "K22: ein zu kleiner Puffer meldet keine Kuerzung");
        PRUEFE(klein[sizeof(klein) - 1] == '\0',
               "K22: der gekuerzte Puffer ist nicht nullterminiert");
    }

    /* K23 — bitgenau haengt am ZIEL, nicht nur an der Ebene. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.preservation = UFT_PRESERVE_BIT_EXACT;
        p.exact_kind = UFT_EXACT_FLUX_TIMING;
        p.level = UFT_COPY_BITSTREAM;
        PRUEFE(hat_befund(&p, 0xFFFFFFFFu, "genauigkeit_zu_hoch", NULL),
               "K23: Flusszeiten auf der Bitstromebene durchgelassen");
        p.level = UFT_COPY_FLUX;
        PRUEFE(!hat_befund(&p, 0xFFFFFFFFu, "genauigkeit_zu_hoch", NULL),
               "K23: Flusszeiten auf der Flussebene beanstandet");
        PRUEFE(uft_copy_exact_name(UFT_EXACT_TRACK_BIT) != NULL,
               "K23: die Spielart hat keinen Namen");
    }

    /* K24 — die Laengenabfrage (MF-1238).
     *
     * K22 misst den zu KLEINEN Puffer. Der Weg, den die Oberflaeche
     * geht, ist ein anderer: erst die Laenge mit einem Nullzeiger holen,
     * dann genau so viel bereitstellen. Ohne diese Zusage haengt die
     * ganze Anzeige an einem ungeprueften Zweig — genau die Lage, die
     * MF-1029 als toten Groessenrueckfall gefunden hat. */
    {
        uft_copy_plan_t p = uft_copy_plan_default();
        p.level = UFT_COPY_FLUX;
        p.strategy = UFT_READ_CONSENSUS;
        p.policy = UFT_POLICY_EVIDENCE;

        const size_t laenge = uft_copy_plan_to_json(&p, NULL, 0);
        PRUEFE(laenge > 0, "K24: die Laengenabfrage meldet 0");

        char *b = (char *)malloc(laenge + 1);
        PRUEFE(b != NULL, "K24: kein Speicher");
        if (b) {
            const size_t voll = uft_copy_plan_to_json(&p, b, laenge + 1);
            PRUEFE(voll == laenge,
                   "K24: zwei verschiedene Laengen (%zu gegen %zu)",
                   laenge, voll);
            PRUEFE(strlen(b) == laenge,
                   "K24: geschrieben wurden %zu statt %zu Byte",
                   strlen(b), laenge);
            /* Und der Inhalt ist wirklich der Plan — sonst waere die
             * Laenge die Laenge von irgendetwas. */
            PRUEFE(strstr(b, "\"level\": \"flux\"") != NULL,
                   "K24: der Text ist nicht dieser Plan");
            free(b);
        }
        /* Ein Nullplan bleibt ein Nullplan, kein Absturz. */
        PRUEFE(uft_copy_plan_to_json(NULL, NULL, 0) == 0,
               "K24: ein Nullplan meldet eine Laenge");
    }

    /* K25 — die acht Faehigkeitsflaggen haben Namen (MF-1238).
     *
     * Gefahren wird ueber `uft_copy_cap_count()`, nicht ueber eine
     * abgeschriebene Liste: eine gepflegte Aufzaehlung veraltet still
     * (MF-636). Und die Gegenprobe gehoert dazu, weil eine Funktion,
     * die nie NULL sagt, keine Auskunft gibt (erkenner_der_nie_nein). */
    {
        const size_t n = uft_copy_cap_count();
        PRUEFE(n == 8, "K25: %zu Flaggen statt 8", n);

        uint32_t gesehen = 0;
        for (size_t i = 0; i < n; i++) {
            const uft_copy_caps_t c = uft_copy_cap_at(i);
            const char *nm = uft_copy_cap_name(c);
            PRUEFE(c != UFT_CAP_NONE, "K25: Flagge %zu ist NONE", i);
            PRUEFE(nm != NULL && nm[0] != '\0',
                   "K25: Flagge %zu ohne Namen", i);
            PRUEFE((gesehen & (uint32_t)c) == 0,
                   "K25: Flagge %zu kommt zweimal", i);
            gesehen |= (uint32_t)c;
            /* Keine zwei tragen denselben Namen. */
            for (size_t j = 0; j < i; j++)
                PRUEFE(strcmp(nm, uft_copy_cap_name(uft_copy_cap_at(j))) != 0,
                       "K25: Flagge %zu und %zu heissen gleich", i, j);
        }

        PRUEFE(uft_copy_cap_name(UFT_CAP_NONE) == NULL,
               "K25: „keine Flagge\" hat einen Namen");
        PRUEFE(uft_copy_cap_name((uft_copy_caps_t)
                   ((uint32_t)UFT_CAP_FLUX_IO | (uint32_t)UFT_CAP_GCR)) == NULL,
               "K25: eine Kombination hat einen Namen bekommen");
        PRUEFE(uft_copy_cap_name((uft_copy_caps_t)(1u << 20)) == NULL,
               "K25: eine unbekannte Flagge hat einen Namen bekommen");
        PRUEFE(uft_copy_cap_at(n) == UFT_CAP_NONE,
               "K25: hinter der letzten Flagge steht noch etwas");
    }

    /* K26 — das Kopier-Tor (MF-1309).
     *
     * Gemessen VOR dem Code: `uft_copy_plan_is_executable()` war fertig
     * gebaut und hatte im ganzen Baum NULL Aufrufer, waehrend drei
     * Ausfuehrungspfade — `decodejob.cpp:400`, `toolstab.cpp:493`,
     * `uft_save_image.cpp:265` — ungeprueft
     * `uft_copy_plan_to_convert_options()` riefen. Ein Plan mit hartem
     * Befund erreichte damit die Wandlung.
     *
     * Was hier NICHT behauptet wird: dass der Wandlungspfad offen war.
     * `uft_preflight_check()` steht in `uft_format_convert_dispatch.c`
     * und sperrt UNTESTED und IMPOSSIBLE. Ungeprueft blieben die Befunde
     * des PLANS, nicht das Formatpaar.
     *
     * Geprueft werden genau die harten Befunde, die OHNE
     * Faehigkeitsmaske gelten. Die capsabhaengigen bleiben ausdruecklich
     * draussen — mit der heutigen, gemessen zu groben Maske wuerde das
     * Tor falsch absagen. */
    {
        const char *grund = "vorbelegt";

        /* Der Vorgabeplan geht durch. Stuende hier eine Absage, wuerde
         * das Tor jede Wandlung sperren und waere unbrauchbar. */
        uft_copy_plan_t ok26 = uft_copy_plan_default();
        PRUEFE(uft_copy_plan_gate(&ok26, &grund) == UFT_COPY_ALLOW,
               "K26: der Vorgabeplan wird abgewiesen");
        PRUEFE(grund == NULL, "K26: bei ALLOW bleibt ein Grund stehen");

        /* Eine ungueltige Ebene ist hart und capsfrei. */
        uft_copy_plan_t ung26 = uft_copy_plan_default();
        ung26.level = (uft_copy_level_t)999;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate(&ung26, &grund) == UFT_COPY_DENY,
               "K26: ungueltige Ebene wird durchgelassen");
        PRUEFE(grund != NULL && strcmp(grund, "ebene_ungueltig") == 0,
               "K26: Grund ist '%s' statt 'ebene_ungueltig'",
               grund ? grund : "(NULL)");

        /* Beweis und Eile — der Befund, an dem der forensische Anspruch
         * haengt. Ein EvidenceCopy mit Schnelllesung ist kein Beweis. */
        uft_copy_plan_t eil26 = uft_copy_plan_default();
        eil26.policy   = UFT_POLICY_EVIDENCE;
        eil26.strategy = UFT_READ_FAST;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate(&eil26, &grund) == UFT_COPY_DENY,
               "K26: Beweis mit Schnelllesung wird durchgelassen");
        PRUEFE(grund != NULL && strcmp(grund, "beweis_und_eile") == 0,
               "K26: Grund ist '%s' statt 'beweis_und_eile'",
               grund ? grund : "(NULL)");

        /* Kein Plan ist keine Erlaubnis. */
        grund = NULL;
        PRUEFE(uft_copy_plan_gate(NULL, &grund) == UFT_COPY_DENY,
               "K26: NULL-Plan wird durchgelassen");
        PRUEFE(grund != NULL && strcmp(grund, "kein_plan") == 0,
               "K26: Grund ist '%s' statt 'kein_plan'",
               grund ? grund : "(NULL)");

        /* Die Faehigkeitsfrage bleibt ausgeklammert — festgenagelt,
         * damit niemand sie stillschweigend hineinzieht. Ein Plan auf
         * der Flussebene ist FUER DAS TOR stimmig; ob ein Format ihn
         * traegt, entscheidet `uft_copy_plan_check()` mit der echten
         * Maske an anderer Stelle. Die Gegenprobe zeigt, dass dort sehr
         * wohl harte Befunde stehen — sonst waere die Ausklammerung
         * folgenlos und dieser Fall aussagelos. */
        uft_copy_plan_t fl26 = uft_copy_plan_default();
        fl26.level = UFT_COPY_FLUX;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate(&fl26, &grund) == UFT_COPY_ALLOW,
               "K26: die Flussebene allein fuehrt zur Absage");

        uft_copy_finding_t f26[16];
        size_t n26 = uft_copy_plan_check(&fl26, 0u, f26, 16);
        if (n26 > 16) n26 = 16;
        size_t hart26 = 0;
        for (size_t i = 0; i < n26; i++)
            if (f26[i].hard) hart26++;
        PRUEFE(hart26 > 0,
               "K26: mit leerer Maske meldet check() keinen harten Befund "
               "— dann klammert das Tor nichts aus");
    }

    /* K27 — das Tor mit Faehigkeitsfrage, drei Ausgaenge (MF-1311).
     *
     * Der dritte Wert `UFT_COPY_NEEDS_MEASUREMENT` wurde in MF-1309
     * ausdruecklich NICHT eingefuehrt, weil ihn niemand erzeugen konnte.
     * Hier wird belegt, dass er jetzt einen Erzeuger hat — und dass die
     * drei Ausgaenge wirklich drei verschiedene Lagen beschreiben. */
    {
        const char *grund = NULL;

        /* Ein Plan ohne Faehigkeitsforderung: unbekannte Maske aendert
         * nichts, er geht durch. Sonst waere NEEDS_MEASUREMENT nur ein
         * zweites Wort fuer "keine Maske gesetzt". */
        uft_copy_plan_t schlicht = uft_copy_plan_default();
        schlicht.level = UFT_COPY_SECTOR;
        schlicht.caps_bekannt = false;
        PRUEFE(uft_copy_plan_gate_caps(&schlicht, &grund) == UFT_COPY_ALLOW,
               "K27: ein Plan ohne Faehigkeitsforderung verlangt Messung");

        /* Die Flussebene verlangt FLUX_IO. Ohne gemessene Maske ist die
         * Antwort weder ja noch nein. */
        uft_copy_plan_t fluss = uft_copy_plan_default();
        fluss.level = UFT_COPY_FLUX;
        fluss.caps_bekannt = false;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate_caps(&fluss, &grund)
                   == UFT_COPY_NEEDS_MEASUREMENT,
               "K27: ungemessene Maske ergibt kein NEEDS_MEASUREMENT");
        PRUEFE(grund != NULL && strcmp(grund, "kein_fluss") == 0,
               "K27: Grund ist '%s' statt 'kein_fluss'",
               grund ? grund : "(NULL)");

        /* Dieselbe Lage, aber GEMESSEN und leer: jetzt ist es ein Nein. */
        fluss.caps_bekannt = true;
        fluss.caps = 0u;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate_caps(&fluss, &grund) == UFT_COPY_DENY,
               "K27: gemessene leere Maske ergibt kein DENY");

        /* Und gemessen MIT der Flagge: ja. Ohne diesen Fall waere das
         * Tor nur eine aufwendige Absage. */
        fluss.caps = (uint32_t)UFT_CAP_FLUX_IO;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate_caps(&fluss, &grund) == UFT_COPY_ALLOW,
               "K27: gemessene passende Maske ergibt kein ALLOW");

        /* Ein harter capsfreier Befund schlaegt jede Maske: er ist DENY,
         * nicht NEEDS_MEASUREMENT. Sonst koennte ein widerspruechlicher
         * Plan sich durch eine fehlende Messung retten. */
        uft_copy_plan_t eil = uft_copy_plan_default();
        eil.policy   = UFT_POLICY_EVIDENCE;
        eil.strategy = UFT_READ_FAST;
        eil.caps_bekannt = false;
        grund = NULL;
        PRUEFE(uft_copy_plan_gate_caps(&eil, &grund) == UFT_COPY_DENY,
               "K27: ein Widerspruch rettet sich durch fehlende Messung");
    }

    /* K28 — die Bit-Genauigkeit sagt dasselbe wie die Beschreibung
     *       (MF-1312).
     *
     * Gemessen trugen VOR dieser Aenderung alle 17 Profile
     * `UFT_EXACT_SECTOR`. Zwei davon versprechen in ihrem eigenen Text
     * mehr. Der Widerspruch erreichte drei Ausgaenge, einer davon eine
     * Datei — deshalb ist das kein Anzeigefehler. */
    {
        const uft_copy_profile_t *flux = NULL, *bit = NULL;
        for (size_t i = 0; i < uft_copy_profile_count(); i++) {
            const uft_copy_profile_t *pr = uft_copy_profile(i);
            if (!pr) continue;
            if (strcmp(pr->id, "fluxcopy") == 0) flux = pr;
            if (strcmp(pr->id, "bitexact") == 0) bit  = pr;
        }
        PRUEFE(flux != NULL && bit != NULL,
               "K28: fluxcopy oder bitexact nicht gefunden");

        if (flux)
            PRUEFE(flux->plan.exact_kind == UFT_EXACT_FLUX_TIMING,
                   "K28: FluxCopy verspricht Flusszeiten und traegt %d",
                   (int)flux->plan.exact_kind);

        /* Bittreu traegt die ASPIRATION; die Deckelung macht daraus auf
         * einem Sektorpaar wieder SECTOR. Beide Seiten werden geprueft,
         * sonst waere die Deckelung nicht belegt. */
        if (bit) {
            PRUEFE(bit->plan.exact_kind == UFT_EXACT_FLUX_TIMING,
                   "K28: Bittreu traegt nicht die hoechste Genauigkeit");

            const uft_copy_plan_t auf_sektor =
                uft_copy_plan_resolve(&bit->plan, 0u);
            PRUEFE(auf_sektor.level == UFT_COPY_SECTOR,
                   "K28: leere Maske fuehrt nicht auf die Sektorebene");
            PRUEFE(auf_sektor.exact_kind == UFT_EXACT_SECTOR,
                   "K28: die Genauigkeit wurde auf der Sektorebene nicht "
                   "gedeckelt — dann meldet check() 'genauigkeit_zu_hoch' "
                   "und das Profil waere unbrauchbar");

            const uft_copy_plan_t auf_fluss =
                uft_copy_plan_resolve(&bit->plan, (uint32_t)UFT_CAP_FLUX_IO);
            PRUEFE(auf_fluss.exact_kind == UFT_EXACT_FLUX_TIMING,
                   "K28: auf der Flussebene wurde faelschlich gedeckelt");
        }

        /* Die Deckelung fasst NUR BIT_EXACT an. Ein bedeutungsloses Feld
         * still zu aendern waere genau die Sorte Nebenwirkung, die hier
         * nicht vorkommen darf. */
        uft_copy_plan_t logisch = uft_copy_plan_default();
        logisch.preservation = UFT_PRESERVE_LOGICAL;
        logisch.level        = UFT_COPY_SECTOR;
        logisch.exact_kind   = UFT_EXACT_FLUX_TIMING;
        const uft_copy_plan_t unberuehrt =
            uft_copy_plan_resolve(&logisch, 0u);
        PRUEFE(unberuehrt.exact_kind == UFT_EXACT_FLUX_TIMING,
               "K28: exact_kind wurde ausserhalb von BIT_EXACT angefasst");
    }

    /* K29 — die Kennungspaare, und die Unterscheidung, die dabei
     *       zweimal uebersehen wurde (MF-1314).
     *
     * Gemessen: 155 Parameter, 0 exakte Dubletten, 8 Paare der Form
     * `<praefix>.<rest>` neben einem eigenstaendigen `<rest>`.
     *
     * Zwei Gutachten haben daraus "doppelte Parameterfamilien, Gefahr
     * widerspruechlicher Zustaende" gemacht. Das trifft auf die HAELFTE
     * nicht zu, und der Unterschied ist wichtig genug fuer eine Zusage:
     *
     *   vier GEOMETRIE-Paare — `source.geometry.*` und
     *   `target.geometry.*` neben `geometry.*` — sind kein Defekt,
     *   sondern genau die Quell-/Ziel-Trennung, die dieselben Gutachten
     *   an anderer Stelle FORDERN. Wer sie zusammenlegte, machte das
     *   Modell schlechter.
     *
     *   vier LAYOUT-Paare — `layout.preserve_{gaps,sync,track_length,
     *   crc_errors}` neben den praefixlosen — sind die offene Frage.
     *   Gemessen erreicht die Divergenz heute keinen Bediener:
     *   `uft_copy_param_state()` hat 0 produktive Aufrufer, und das
     *   einzige Glied, das ein Faehigkeitstor ueberlebt
     *   (`layout.preserve_crc_errors`), ist an kein Bedienelement
     *   gebunden. Latent, nicht am Produktionspfad.
     *
     * Diese Zusage ist ein TOR mit fallender Grundlinie: die Zahl darf
     * sinken, nie steigen. Damit kann ein neues Paar nicht unbemerkt
     * entstehen — und das ist der Punkt, denn genau so sind die acht
     * entstanden. */
    {
        const size_t n = uft_copy_param_count();
        PRUEFE(n == 155, "K29: %zu Parameter statt 155", n);

        size_t paare = 0, geo = 0, lay = 0;
        for (size_t i = 0; i < n; i++) {
            const char *a = uft_copy_param_id(i);
            if (!a) continue;
            const char *punkt = strchr(a, '.');
            if (!punkt || !punkt[1]) continue;

            for (size_t j = 0; j < n; j++) {
                const char *b = (i == j) ? NULL : uft_copy_param_id(j);
                if (!b || strcmp(b, punkt + 1) != 0) continue;
                paare++;
                if (strstr(a, "geometry") != NULL) geo++;
                else if (strncmp(a, "layout.", 7) == 0) lay++;
                break;
            }
        }
        PRUEFE(paare <= 8, "K29: %zu Kennungspaare — die Grundlinie ist 8 "
                           "und darf nur sinken", paare);
        PRUEFE(geo == 4, "K29: %zu Geometriepaare statt 4 — das ist die "
                         "gewollte Quell-/Ziel-Trennung", geo);
        PRUEFE(lay == 4, "K29: %zu layout-Paare statt 4 — das ist die "
                         "offene Frage", lay);
    }

    /* ── MF-1236: die Profile ────────────────────────────────────────
     *
     * Ein Profil ist ein Name fuer einen Plan. Die beiden Fehler, die
     * die Messung im Entwurf gefunden hat, stehen hier als Zusage —
     * damit sie nicht zurueckkommen. */

    /* P1 — jedes Profil ist vollstaendig und auffindbar. */
    {
        const size_t n = uft_copy_profile_count();
        PRUEFE(n > 0, "P1: keine Profile");
        for (size_t i = 0; i < n; i++) {
            const uft_copy_profile_t *p = uft_copy_profile(i);
            PRUEFE(p != NULL, "P1: Profil %zu ist NULL", i);
            if (!p) continue;
            PRUEFE(p->id && p->id[0], "P1: Profil %zu ohne Kennung", i);
            PRUEFE(p->name && p->name[0], "P1: %s ohne Namen", p->id);
            PRUEFE(p->text && p->text[0], "P1: %s ohne Beschreibung", p->id);
            PRUEFE(uft_copy_profile_by_id(p->id) == p,
                   "P1: %s ist ueber seine Kennung nicht auffindbar", p->id);
        }
        PRUEFE(uft_copy_profile(n) == NULL,
               "P1: der Index hinter der Tafel liefert etwas");
        PRUEFE(uft_copy_profile_by_id("gibtesnicht") == NULL,
               "P1: eine unbekannte Kennung liefert ein Profil");
        PRUEFE(uft_copy_profile_by_id(NULL) == NULL,
               "P1: NULL liefert ein Profil");
        printf("  %zu Profile geprueft\n", n);
    }

    /* P2 — KEIN Profil traegt einen strukturellen harten Befund.
     *
     * Das ist der Fund an BAMCopy: es stand auf `layout`, und die
     * Erhaltung „Spurlayout" verlangt mindestens die Spurebene. Mit
     * vollen Faehigkeiten darf kein Profil mehr an der EIGENEN
     * Zusammenstellung scheitern — was am FORMAT scheitert, ist eine
     * andere Frage und steht in P6. */
    {
        for (size_t i = 0; i < uft_copy_profile_count(); i++) {
            const uft_copy_profile_t *p = uft_copy_profile(i);
            if (!p) continue;
            uft_copy_plan_t plan = p->plan;
            uft_copy_finding_t f[16];
            size_t n = uft_copy_plan_check(&plan, 0xFFFFFFFFu, f, 16);
            if (n > 16) n = 16;
            for (size_t k = 0; k < n; k++) {
                if (!f[k].hard) continue;
                PRUEFE(false, "P2: %s traegt den harten Befund '%s' — ein "
                       "Profil, das immer abgelehnt wird, gehoert nicht "
                       "ins Auswahlfeld", p->name, f[k].id);
            }
        }
    }

    /* P3 — kein Profil doppelt: weder Kennung noch Plan.
     *
     * Das ist der Fund an Cyclone und ProtectedCopy: gleiches Viertupel,
     * zwei Namen. Fuer den Bediener sind zwei Eintraege, die dasselbe
     * tun, nicht unterscheidbar. */
    {
        const size_t n = uft_copy_profile_count();
        for (size_t i = 0; i < n; i++) {
            const uft_copy_profile_t *a = uft_copy_profile(i);
            for (size_t j = i + 1; j < n; j++) {
                const uft_copy_profile_t *b = uft_copy_profile(j);
                if (!a || !b) continue;
                PRUEFE(strcmp(a->id, b->id) != 0,
                       "P3: Kennung '%s' kommt doppelt vor", a->id);
                /* Verglichen wird der GANZE Plan, nicht nur die vier
                 * Hauptachsen. DOSCopy und BAMCopy unterscheiden sich
                 * allein in `file_special`, Cyclone und NibbleCopy in
                 * Strategie und Erhaltung — wer nur das Viertupel
                 * vergleicht, prueft an genau diesen Stellen vorbei. */
                const bool gleich =
                    a->plan.level == b->plan.level &&
                    a->plan.strategy == b->plan.strategy &&
                    a->plan.preservation == b->plan.preservation &&
                    a->plan.policy == b->plan.policy &&
                    a->plan.file_special == b->plan.file_special &&
                    a->plan.track_mode == b->plan.track_mode &&
                    a->plan.gcr == b->plan.gcr &&
                    a->plan.vote == b->plan.vote &&
                    a->plan.exact_kind == b->plan.exact_kind;
                PRUEFE(!gleich,
                       "P3: %s und %s ergeben denselben Plan — zwei Namen, "
                       "eine Sache", a->name, b->name);
            }
        }
    }

    /* P4 — Verfuegbarkeit: wer ablehnt, nennt den Grund. */
    {
        const uft_copy_profile_t *bam = uft_copy_profile_by_id("bamcopy");
        PRUEFE(bam != NULL, "P4: BAMCopy fehlt");
        if (bam) {
            const char *grund = NULL;
            PRUEFE(!uft_copy_profile_available(bam, 0, "ADF", &grund),
                   "P4: BAMCopy ist fuer ADF verfuegbar");
            PRUEFE(grund && grund[0],
                   "P4: die Ablehnung nennt keinen Grund");
            /* Der Behelf traegt, solange die Flagge leer ist ... */
            PRUEFE(uft_copy_profile_available(bam, 0, "D64", NULL),
                   "P4: BAMCopy ist fuer D64 nicht verfuegbar");
            /* ... und die FLAGGE traegt ohne ihn. */
            const uint32_t beides = (uint32_t)UFT_CAP_CBM_BAM |
                                    (uint32_t)UFT_CAP_FILESYSTEM;
            PRUEFE(uft_copy_profile_available(bam, beides, "WOZ", NULL),
                   "P4: mit gesetzten Flaggen zaehlt der Formatname noch");
        }
        const uft_copy_profile_t *sc = uft_copy_profile_by_id("sectorcopy");
        PRUEFE(sc && uft_copy_profile_available(sc, 0, NULL, NULL),
               "P4: SectorCopy verlangt etwas");
        PRUEFE(!uft_copy_profile_available(NULL, 0, NULL, NULL),
               "P4: ein NULL-Profil gilt als verfuegbar");
    }

    /* P5 — der Behelf vergleicht GLIEDWEISE, nicht als Teilzeichenkette.
     *
     * „DO" steckt in „DOS"; wer mit strstr vergliche, haette NibbleCopy
     * fuer jedes DOS-Format freigegeben. Die Falle ist belegt. */
    {
        const uft_copy_profile_t *nib = uft_copy_profile_by_id("nibblecopy");
        PRUEFE(nib != NULL, "P5: NibbleCopy fehlt");
        if (nib) {
            PRUEFE(uft_copy_profile_available(nib, 0, "DO", NULL),
                   "P5: DO steht in der Liste und wird abgelehnt");
            PRUEFE(!uft_copy_profile_available(nib, 0, "DOS", NULL),
                   "P5: DOS wird angenommen, weil DO darin steckt");
            PRUEFE(uft_copy_profile_available(nib, 0, "d64", NULL),
                   "P5: die Schreibweise entscheidet");
        }
    }

    /* P4b — DER KERN BEHAUPTET KEINE MESSUNG, DIE ER NICHT HAT (P3-508).
     *
     * `uft_copy_profile_available()` bekommt eine Merkmalsmaske und sonst
     * nichts. Es kann daraus NICHT unterscheiden, ob eine Flagge
     *
     *   (a) gemessen wurde und fehlt, oder
     *   (b) gar nicht gemessen werden KONNTE.
     *
     * Trotzdem sagte es „kein GCR-Format erkannt", „keine Commodore-BAM
     * erkannt", „kein Dateisystem erkannt" — eine Aussage UEBER die
     * Diskette. Der einzige Produktivaufrufer ist der Formatreiter, und
     * der zeigt zehn Zeilen weiter „nicht feststellbar (dieser Reiter
     * oeffnet kein Abbild)" fuer dieselben vier Flaggen: zwei Saetze
     * ueber dieselbe Flagge, in einem Dialog.
     *
     * Das ist MF-980 in neuer Gestalt („das Format sagt 0xE5" gegen
     * „hier wurde 0xE5 gelesen") und die Missionszeile „Keine erfundenen
     * Daten".
     *
     * ZWEITER BEFUND IN DERSELBEN FUNKTION: die Begruendung kam aus
     * einer `if`-Kette, die 6 der 8 Flaggen der Tafel aufzaehlte —
     * `Timing` und `schwache Bits` fielen in den Sammelzweig. Eine
     * Aufzaehlung neben einer Tafel (MF-636/D3).
     *
     * Die Zusage prueft deshalb ZWEI Dinge, und beide fallen am
     * Vorzustand: keine Begruendung behauptet einen Befund, und jede
     * nennt die fehlende Faehigkeit beim Namen der Tafel. */
    {
        size_t geprueft = 0, behauptet = 0, ohne_namen = 0;
        for (size_t i = 0; i < uft_copy_profile_count(); i++) {
            const uft_copy_profile_t *p = uft_copy_profile(i);
            if (!p || !p->braucht) continue;
            const char *grund = NULL;
            /* Ein Formatname, den KEIN Behelf fuehrt — sonst traegt der
             * Behelf und es gibt gar keine Ablehnung. */
            if (uft_copy_profile_available(p, 0u, "KEIN-FORMAT", &grund))
                continue;
            geprueft++;
            if (!grund) { ohne_namen++; continue; }

            /* (1) Kein Befund ueber das Medium.
             *
             * Zwei Pruefungen, und die zweite ist die wichtigere: das
             * Wort „erkannt" faengt genau die vier gemessenen
             * Falschaussagen, aber nur sie — „kein GCR gefunden" kaeme
             * durch. Deshalb wird zusaetzlich die GESTALT festgenagelt:
             * eine Begruendung aus der Tafel sagt, was das Profil
             * VERLANGT, und endet auf „nicht zugesagt". Wer wieder Prosa
             * ueber die Diskette einsetzt, faellt hier auf — egal mit
             * welchem Wort. */
            const size_t lg = strlen(grund);
            const char *schwanz = "nicht zugesagt";
            const size_t ls = strlen(schwanz);
            const int gestalt = (strncmp(grund, "setzt ", 6) == 0)
                             && (lg >= ls)
                             && (strcmp(grund + lg - ls, schwanz) == 0);
            if (strstr(grund, "erkannt") || !gestalt) {
                behauptet++;
                printf("  P4b: `%s` behauptet einen Befund: \"%s\"\n",
                       p->id, grund);
            }

            /* (2) Die fehlende Faehigkeit wird benannt — mit dem Namen,
             *     den die Tafel des Kerns fuehrt. */
            int genannt = 0;
            for (size_t k = 0; k < uft_copy_cap_count(); k++) {
                const uft_copy_caps_t c = uft_copy_cap_at(k);
                if (!(p->braucht & (uint32_t)c)) continue;
                const char *n = uft_copy_cap_name(c);
                if (n && strstr(grund, n)) { genannt = 1; break; }
            }
            if (!genannt) {
                ohne_namen++;
                printf("  P4b: `%s` nennt keine Faehigkeit: \"%s\"\n",
                       p->id, grund);
            }
        }
        PRUEFE(geprueft > 0,
               "P4b: kein Profil lehnte ab — die Probe misst nichts");
        PRUEFE(behauptet == 0,
               "P4b: %zu Begruendung(en) behaupten einen Befund ueber die "
               "Diskette, den der Kern nicht hat", behauptet);
        PRUEFE(ohne_namen == 0,
               "P4b: %zu Begruendung(en) nennen die fehlende Faehigkeit "
               "nicht beim Namen der Tafel", ohne_namen);
        printf("  %zu Ablehnungen geprueft, %zu behauptet, %zu ohne Namen\n",
               geprueft, behauptet, ohne_namen);
    }

    /* P5b — EIN GCR-BEHELF DARF KEIN MFM-FORMAT FUEHREN (P3-510).
     *
     * `nibblecopy` verlangt `UFT_CAP_GCR` und fuehrte `D81` in seinem
     * Behelf. Das 1581 schreibt MFM. Damit bot der Reiter ein
     * GCR-Verfahren auf einem Format an, das keines ist — und zwar
     * genau dann, wenn die Flagge NICHT zugesagt ist, also wenn niemand
     * widersprechen kann.
     *
     * VIER ZEUGEN IM EIGENEN BAUM, keiner von aussen:
     *   src/formats/d81/uft_d81_parser_v2.c:9  „MFM encoding (not GCR!)"
     *   src/detect/mfm/mfm_detect.c:87/115     MFM_GEOM_CBM_1581,
     *                                          MFM_FS_CBM_1581 — die 1581
     *                                          ist eine Geometrie des
     *                                          MFM-Erkenners
     *   src/formats/commodore/d81.c:100        „Analyzer(D81): no GCR
     *                                          timing ... preserved"
     *   uft_copy_plan.c `cyclone`              verlangt ebenfalls GCR und
     *                                          fuehrt D81 NICHT
     *
     * Der vierte ist der staerkste: der Baum widersprach sich selbst,
     * und die richtige Seite lag bereits darin.
     *
     * `bamcopy` fuehrt D81 WEITERHIN und zu Recht — es verlangt
     * `CBM_BAM | FILESYSTEM`, und eine BAM ist Dateisystemebene,
     * unabhaengig von der Kodierung des Mediums.
     *
     * GRENZE DIESER ZUSAGE, damit sie nicht mehr verspricht als sie
     * haelt: sie ist ein REGRESSIONSNAGEL, keine Ableitung. Der Baum
     * traegt **kein** maschinenlesbares Kodierungsfeld je Format —
     * `uft_format_plugin_t` hat keines, und `uft_encoding_t` sitzt an
     * Spur- und Geometriestrukturen. Ein falscher NEUER Eintrag faellt
     * hier also nicht auf; nur die Rueckkehr dieses einen. Was es
     * braeuchte, steht als Frage in `P3-510`. */
    {
        size_t gcr_profile = 0, mit_d81 = 0;
        for (size_t i = 0; i < uft_copy_profile_count(); i++) {
            const uft_copy_profile_t *p = uft_copy_profile(i);
            if (!p || !(p->braucht & (uint32_t)UFT_CAP_GCR)) continue;
            gcr_profile++;
            /* Ueber die oeffentliche API, nicht ueber die Zeichenkette:
             * geprueft wird das VERHALTEN, das der Bediener sieht. */
            if (uft_copy_profile_available(p, 0u, "D81", NULL)) {
                mit_d81++;
                printf("  P5b: `%s` verlangt GCR und nimmt D81 (MFM)\n",
                       p->id);
            }
        }
        PRUEFE(gcr_profile >= 2,
               "P5b: weniger als zwei GCR-Profile — die Probe misst nichts");
        PRUEFE(mit_d81 == 0,
               "P5b: %zu GCR-Profil(e) bieten sich fuer D81 an, obwohl das "
               "1581 MFM schreibt", mit_d81);

        /* Gegenprobe: der Behelf traegt weiterhin, wo er hingehoert —
         * sonst waere die Behebung eine pauschale Verweigerung. */
        const uft_copy_profile_t *nib = uft_copy_profile_by_id("nibblecopy");
        PRUEFE(nib && uft_copy_profile_available(nib, 0u, "G64", NULL),
               "P5b: NibbleCopy lehnt G64 ab — zu viel entfernt");
        const uft_copy_profile_t *bam = uft_copy_profile_by_id("bamcopy");
        PRUEFE(bam && uft_copy_profile_available(bam, 0u, "D81", NULL),
               "P5b: BAMCopy lehnt D81 ab — eine BAM ist Dateisystemebene");
        printf("  %zu GCR-Profile geprueft, %zu mit D81\n",
               gcr_profile, mit_d81);
    }

    /* P7 — DER PLAN WIRKT: die Abbildung auf die Wandlungsoptionen
     *      (MF-1263, `P3-509`).
     *
     * Bis dahin hatte der Plan ausser seinem Reiter keinen Leser. Seit
     * MF-1263 traegt `uft_copy_plan_to_convert_options()` ihn in die
     * Optionen, die `uft_convert_file()` wirklich liest.
     *
     * DIE ERWARTUNG STEHT HIER EIN ZWEITES MAL, und das ist Absicht
     * (Klasse MF-1000): wer die Tafel `k_strategie[]` befragt, um zu
     * pruefen, was aus `k_strategie[]` wird, prueft nichts. Die Zahlen
     * unten stammen aus der Vorgabe je Lesestrategie:
     *
     *   FAST       read.retries 0   read.revolutions 1
     *   STANDARD   read.retries 3   read.revolutions 2
     *   DEEP       read.retries 10  read.revolutions 5
     *   CONSENSUS  — keine Zahlen, aber consensus.enabled = true
     *   SALVAGE    read.retries „hoch" — ein FORDERUNGSWORT
     *
     * Die Startwerte sind absichtlich NICHT die Vorgaben der Wandlung:
     * `99` und `false` setzt keine Strategie, also ist „unveraendert"
     * beobachtbar statt geraten. */
    {
        struct { uft_read_strategy_t s; const char *name;
                 uint32_t retries; int revs; } erw[] = {
            { UFT_READ_FAST,      "FAST",       0u, 0 },
            { UFT_READ_STANDARD,  "STANDARD",   3u, 1 },
            { UFT_READ_DEEP,      "DEEP",      10u, 1 },
            /* keine Zahl -> 99 bleibt; consensus.enabled -> revs an */
            { UFT_READ_CONSENSUS, "CONSENSUS", 99u, 1 },
            /* „hoch" ist keine Zahl, und es gibt keine Umdrehungszahl */
            { UFT_READ_SALVAGE,   "SALVAGE",   99u, 0 },
        };
        const size_t n_erw = sizeof(erw) / sizeof(erw[0]);
        PRUEFE(n_erw == (size_t)UFT_READ_STRATEGY_N,
               "P7: %zu Erwartungen fuer %d Strategien — eine fehlt",
               n_erw, (int)UFT_READ_STRATEGY_N);

        for (size_t i = 0; i < n_erw; i++) {
            uft_copy_plan_t pl;
            memset(&pl, 0, sizeof(pl));
            pl.strategy = erw[i].s;
            pl.policy   = UFT_POLICY_NORMAL;

            uft_convert_options_t o;
            memset(&o, 0, sizeof(o));
            o.decode_retries    = 99u;
            o.use_multiple_revs = false;
            o.verify_after      = true;      /* muss NORMAL abschalten */

            uft_copy_plan_to_convert_options(&pl, &o);

            PRUEFE(o.decode_retries == erw[i].retries,
                   "P7: %s setzt decode_retries auf %u statt %u",
                   erw[i].name, o.decode_retries, erw[i].retries);
            PRUEFE((int)o.use_multiple_revs == erw[i].revs,
                   "P7: %s setzt use_multiple_revs auf %d statt %d",
                   erw[i].name, (int)o.use_multiple_revs, erw[i].revs);
            PRUEFE(o.verify_after == false,
                   "P7: %s mit NORMAL laesst verify_after an", erw[i].name);
        }

        /* Die Richtlinie ist die zweite Achse, die ankommt. */
        const struct { uft_copy_policy_t p; const char *name; bool v; } pol[] = {
            { UFT_POLICY_NORMAL,   "NORMAL",   false },
            { UFT_POLICY_VERIFY,   "VERIFY",   true  },
            { UFT_POLICY_EVIDENCE, "EVIDENCE", true  },
        };
        PRUEFE(sizeof(pol) / sizeof(pol[0]) == (size_t)UFT_POLICY_N,
               "P7: nicht jede Richtlinie ist geprueft");
        for (size_t i = 0; i < sizeof(pol) / sizeof(pol[0]); i++) {
            uft_copy_plan_t pl;
            memset(&pl, 0, sizeof(pl));
            pl.strategy = UFT_READ_STANDARD;
            pl.policy   = pol[i].p;
            uft_convert_options_t o;
            memset(&o, 0, sizeof(o));
            o.verify_after = !pol[i].v;      /* absichtlich verkehrt */
            uft_copy_plan_to_convert_options(&pl, &o);
            PRUEFE(o.verify_after == pol[i].v,
                   "P7: %s setzt verify_after auf %d statt %d",
                   pol[i].name, (int)o.verify_after, (int)pol[i].v);
        }

        /* NULL darf nichts anfassen — sonst waere „kein Plan" eine
         * stille Aenderung. */
        {
            uft_convert_options_t o;
            memset(&o, 0, sizeof(o));
            o.decode_retries = 77u;
            uft_copy_plan_to_convert_options(NULL, &o);
            PRUEFE(o.decode_retries == 77u,
                   "P7: ein NULL-Plan veraendert die Optionen");
            uft_copy_plan_t pl;
            memset(&pl, 0, sizeof(pl));
            uft_copy_plan_to_convert_options(&pl, NULL);  /* darf nicht stuerzen */
        }
        printf("  %zu Strategien und %zu Richtlinien erreichen die Wandlung\n",
               n_erw, sizeof(pol) / sizeof(pol[0]));
    }

    /* P8 — EINE FORDERUNG IST KEIN WERT (MF-1264, Review-Befund #5).
     *
     * `uft_copy_enforced_t::value` ist eine Zeichenkette und trug DREI
     * Dinge, die ein Verbraucher nicht auseinanderhalten konnte.
     * Gemessen ueber alle Wertetafeln: 46 Wahrheitswerte, 13
     * Forderungen, 9 Zahlen — und **kein** echter Zeichenkettenwert.
     *
     * „hoch" sagt, dass jemand eine Zahl waehlen MUSS, nicht dass die
     * Zahl „hoch" ist. Genau daran waere MF-1263 beinahe gescheitert:
     * `strtoul("hoch")` ergibt 0, also eine erfundene Zahl.
     *
     * BERICHTIGT gegen den Befund: der Review nannte „vier Wertarten".
     * Gemessen sind es DREI; die vier waren die vier Forderungswoerter.
     *
     * Diese Zusage prueft die Einteilung an Faellen, deren Antwort
     * feststeht, UND dass in den echten Tafeln nichts vorkommt, was
     * keiner der drei Arten angehoert. */
    {
        const struct { const char *v; uft_wert_art_t art; } faelle[] = {
            { "0",        UFT_WERT_ZAHL      },
            { "10",       UFT_WERT_ZAHL      },
            { "true",     UFT_WERT_WAHRHEIT  },
            { "false",    UFT_WERT_WAHRHEIT  },
            { "hoch",     UFT_WERT_FORDERUNG },
            { "noetig",   UFT_WERT_FORDERUNG },
            { "Pflicht",  UFT_WERT_FORDERUNG },
            { "gewaehlt", UFT_WERT_FORDERUNG },
            /* Die Fallen: eine leere Zeichenkette ist KEINE Zahl, und
             * „3x" auch nicht — sonst waere `strtoul` zurueck. */
            { "",         UFT_WERT_FORDERUNG },
            { "3x",       UFT_WERT_FORDERUNG },
            { "TRUE",     UFT_WERT_FORDERUNG },   /* Schreibweise zaehlt */
        };
        for (size_t i = 0; i < sizeof(faelle)/sizeof(faelle[0]); i++)
            PRUEFE(uft_copy_wert_art(faelle[i].v) == faelle[i].art,
                   "P8: \"%s\" wird als %d eingeteilt statt als %d",
                   faelle[i].v, (int)uft_copy_wert_art(faelle[i].v),
                   (int)faelle[i].art);
        PRUEFE(uft_copy_wert_art(NULL) == UFT_WERT_FORDERUNG,
               "P8: NULL ist keine Forderung");

        /* Und am ECHTEN Bestand: jeder erzwungene Wert traegt seine Art,
         * und keiner ist eine unerkannte Zeichenkette. Wer der Tafel
         * einen echten Textwert hinzufuegt, faellt hier auf — dann ist
         * eine vierte Art faellig, keine stille Umdeutung. */
        size_t gezaehlt[3] = {0,0,0}, fremd = 0;
        for (int s = 0; s < UFT_READ_STRATEGY_N; s++) {
            for (int e = 0; e < UFT_PRESERVE_N; e++) {
                for (int po = 0; po < UFT_POLICY_N; po++) {
                    uft_copy_plan_t pl; memset(&pl, 0, sizeof(pl));
                    pl.strategy = (uft_read_strategy_t)s;
                    pl.preservation = (uft_preservation_t)e;
                    pl.policy = (uft_copy_policy_t)po;
                    uft_copy_enforced_t ez[96];
                    const size_t m = uft_copy_plan_enforced(&pl, ez, 96);
                    for (size_t i = 0; i < m && i < 96; i++) {
                        if (!ez[i].value) { fremd++; continue; }
                        if (ez[i].art > UFT_WERT_FORDERUNG) { fremd++; continue; }
                        gezaehlt[ez[i].art]++;
                        /* Die Art muss zum Wert passen — nicht bloss
                         * gesetzt sein. */
                        if (ez[i].art != uft_copy_wert_art(ez[i].value))
                            fremd++;
                    }
                }
            }
        }
        PRUEFE(fremd == 0,
               "P8: %zu erzwungene Werte tragen eine Art, die nicht zu "
               "ihrem Wert passt", fremd);
        PRUEFE(gezaehlt[UFT_WERT_FORDERUNG] > 0,
               "P8: keine einzige Forderung im Bestand — die Probe misst "
               "nichts");
        printf("  erzwungene Werte: %zu Zahlen, %zu Wahrheitswerte, "
               "%zu Forderungen\n", gezaehlt[UFT_WERT_ZAHL],
               gezaehlt[UFT_WERT_WAHRHEIT], gezaehlt[UFT_WERT_FORDERUNG]);
    }

    /* P9 — STUFE UND ABSCHNITT SIND ZWEI FRAGEN (Review-Befund #6,
     *      Praemisse BERICHTIGT — MF-1264).
     *
     * Der Review las die beiden als „dieselbe Zuordnung zweimal
     * gerechnet". Gemessen sind es zwei verschiedene Fragen:
     *
     *   `uft_copy_param_stage()` beantwortet WANN ein Parameter gilt —
     *   und dient dazu, Konflikte einzugrenzen („ein Konflikt gilt nur
     *   innerhalb einer Stufe", Kopf von `uft_copy_plan.h`).
     *   `abschnitt()` beantwortet, WO er im JSON erscheint.
     *
     * `write.verify` ist deshalb in BEIDEM richtig: Abschnitt „write",
     * Stufe VERIFY. Kein Widerspruch, sondern zwei Achsen.
     *
     * Was WIRKLICH eine zweite Aufzaehlung ist: die Rueckfall-Liste der
     * Namensvorsilben IN `uft_copy_param_stage()`. Sie greift nur fuer
     * Parameter, die NICHT in der Tafel stehen — und wenn sie der Tafel
     * widerspricht, bekommt ein neu hinzugefuegtes Geschwister die
     * falsche Stufe. Gemessen ist `write.verify` die einzige Stelle, an
     * der Vorsilbe (WRITE) und Tafel (VERIFY) auseinandergehen.
     *
     * Diese Zusage haelt fest, dass die Tafel gewinnt. */
    {
        PRUEFE(uft_copy_param_stage("write.verify") == UFT_STAGE_VERIFY,
               "P9: die Tafel verliert gegen die Vorsilbe — `write.verify` "
               "ist VERIFY, nicht WRITE");
        /* Ein Geschwister, das NICHT in der Tafel steht, bekommt die
         * Vorsilbe. Das ist die bekannte Grenze, und sie ist hier
         * festgehalten statt entdeckt zu werden. */
        PRUEFE(uft_copy_param_stage("write.verify_zweimal") == UFT_STAGE_WRITE,
               "P9: unbekanntes `write.*` bekommt nicht die Vorsilbe");
        /* Und eine unbekannte Vorsilbe ergibt „nicht eingeordnet" —
         * nicht stillschweigend READ. */
        PRUEFE(uft_copy_param_stage("voellig.unbekannt") == UFT_STAGE_N,
               "P9: unbekannte Vorsilbe faellt still auf eine Stufe zurueck");
    }

    /* P6 — und die gemessene Wahrheit bleibt sichtbar: die
     * Kopierschutz-Profile scheitern am FORMAT, nicht an sich selbst. */
    {
        const uft_copy_profile_t *pc = uft_copy_profile_by_id("protectedcopy");
        PRUEFE(pc != NULL, "P6: ProtectedCopy fehlt");
        if (pc) {
            uft_copy_plan_t plan = uft_copy_plan_resolve(&pc->plan, 0);
            PRUEFE(hat_befund(&plan, 0, "schutz_nicht_tragbar", NULL),
                   "P6: ProtectedCopy meldet ohne Timing/Weak-Bits nichts");
            const uint32_t voll = 0xFFFFFFFFu;
            uft_copy_plan_t p2 = uft_copy_plan_resolve(&pc->plan, voll);
            PRUEFE(!hat_befund(&p2, voll, "schutz_nicht_tragbar", NULL),
                   "P6: mit beiden Flaggen wird es trotzdem abgelehnt");
        }
    }

    /* Ein Lauf ueber die ganze Tafel — er darf nicht abstuerzen, und
     * jeder Bezeichner muss eine Stufe haben. */
    {
        size_t n = uft_copy_param_count();
        PRUEFE(n > 0, "die Parametertafel ist leer");
        uft_copy_plan_t p = uft_copy_plan_default();
        size_t ohne_stufe = 0;
        for (size_t i = 0; i < n; i++) {
            const char *id = uft_copy_param_id(i);
            PRUEFE(id != NULL, "Parameter %zu ohne Namen", i);
            if (!id) continue;
            if (uft_copy_param_stage(id) == UFT_STAGE_N) ohne_stufe++;
            (void)uft_copy_param_state(&p, id, NULL);
        }
        PRUEFE(ohne_stufe == 0,
               "%zu Parameter der Tafel haben keine Stufe", ohne_stufe);
        printf("  %zu Parameter in der Tafel, alle mit Stufe\n", n);
    }

    if (fehler) {
        printf("ROT: %d Befund(e)\n", fehler);
        return 1;
    }
    printf("GRUEN\n");
    return 0;
}
