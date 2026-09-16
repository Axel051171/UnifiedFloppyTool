/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_referenz_rangierer.c
 * @brief P3-445 — ein Rangierer, der bei Gleichstand absagt, und der
 *        „unbekannt" von „widerspricht" trennt.
 *
 * WARUM DIESER TEST VOR DEM CODE STEHT
 * ------------------------------------
 * Gemessen (MF-1184) an der API der Zulieferung, nicht durch Nachlesen
 * der TSV: `uft_floppy_reference_match()` rangiert 113 Referenzsaetze und
 * sagt bei Gleichstand NICHT ab. Fuer 80x2x18x512 / 300 U/min / MFM
 * nennen **drei** Saetze je 100 Punkte — `wiki-apple-ii-05`,
 * `wiki-macintosh-02`, `wiki-atari-st-tt-falcon-03` —, je 7 Felder
 * verglichen, 0 abweichend, und **keiner ist der PC-Satz**. Bei PC 160 K
 * gewinnt `wiki-coleco-adam-01` vor dem IBM-Satz. Entschieden hat die
 * **Tabellenreihenfolge**.
 *
 * Eingebaut waere das eine Falschaussage mit Namen: „Apple II" fuer eine
 * PC-Diskette, in einem Erhaltungsprotokoll, wo niemand sie von einer
 * Messung unterscheiden kann.
 *
 * DIE BENANNTE REFERENZ STEHT IM BAUM (EINFRIER-REGEL (a) + (c))
 * --------------------------------------------------------------
 * Nicht erfunden, sondern die Regel dieses Baums, angewandt:
 *
 *   `src/core/uft_smart_open.c:428-438` — „A tie is not a detail of the
 *   detection, it is the detection. Say it in the warnings, where it
 *   travels with the result into any report." Die Warnung dort lautet
 *   woertlich: „'%s' gewinnt durch Registrierungsreihenfolge, nicht durch
 *   Evidenz."
 *
 *   `include/uft/uft_format_plugin.h` — `uft_probe_ranking` mit `tied`,
 *   `tied_listed`, `tied_with[]` (MF-729). Gemessen erreichbar: **3**
 *   Produktivdateien (`uft_format_plugin.c`, `uft_probe_format_impl.c`,
 *   `uft_smart_open.c`), 7 Tests.
 *
 *   Sonden-Doktrin (MF-1153), Regel 2 und 3: „bei Gleichstand gewinnt der
 *   ENGERE Anspruch; bleibt es gleich, gewinnt KEINER."
 *
 * ZWEI DEFEKTE, ZWEI ZUSAGEN-GRUPPEN
 * ----------------------------------
 * (1) Gleichstand: `tied`, `best_compared`, `ambiguous` — und bei
 *     `ambiguous` wird KEIN Sieger benannt.
 * (2) „unbekannt" ist nicht „widerspricht": in der Zulieferung zaehlt
 *     `score_field()` `possible` und `compared` VOR der Pruefung hoch, und
 *     `in_range()` verlangt `min != 0 && max != 0`. Gemessen bekommen 3
 *     von 3 Saetzen ohne Drehzahlangabe einen Abweichungszaehler. **Und
 *     im Baum ist das der Normalfall, nicht der Rand:** `uft_geometry_t`
 *     (`include/uft/uft_core.h:69-74`) traegt `cylinders`, `heads`,
 *     `sectors`, `sector_size` — und KEINE Drehzahl und KEINE Kodierung.
 *     Ein Aufrufer aus dem Baum liefert also immer eine Beobachtung mit
 *     Luecken. Dieselbe Unterscheidung wie MF-980 („das Format sagt 0xE5"
 *     gegen „hier wurde 0xE5 gelesen") und die Spaltenregel D6.
 *
 * WAS HIER NICHT GEPRUEFT WIRD
 * ----------------------------
 * Die RICHTIGKEIT der 113 Saetze. Sie stammen aus einer Sekundaerquelle
 * (Wikipedia, CC BY-SA 4.0, Schnappschuss 2026-09-16, URL im Header als
 * `UFT_FLOPPY_REFERENCE_SOURCE_URL`), und eine Sekundaerquelle hebt nach
 * `docs/ORACLES.md` keine Tier-Stufe. Geprueft wird der RANGIERER.
 */

#include "uft/formats/uft_floppy_reference.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-50s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

/* Eine Beobachtung, wie ein Aufrufer aus dem Baum sie liefern KANN.
 * `rpm` und `encoding` sind absichtlich getrennt setzbar, weil
 * `uft_geometry_t` sie nicht traegt. */
static uft_floppy_observation_t beob(uint16_t tracks, uint8_t sides,
                                     uint16_t spt, uint16_t bps,
                                     uint16_t rpm,
                                     uft_floppy_encoding_t enc)
{
    uft_floppy_observation_t o;
    memset(&o, 0, sizeof(o));
    o.tracks            = tracks;
    o.sides             = sides;
    o.sectors_per_track = spt;
    o.bytes_per_sector  = bps;
    o.rpm               = rpm;
    o.encoding          = enc;
    o.medium            = NULL;      /* unbekannt, nicht widersprechend */
    return o;
}

/* ── Der Bestand ueberhaupt ──────────────────────────────────────────── */

TEST(der_bestand_ist_da_und_nennt_seine_quelle)
{
    /* 113 Saetze, gemessen MF-1184 an der TSV mit 114 Zeilen (Kopf + 113).
     * Die Zahl steht hier, damit ein stiller Verlust auffaellt. */
    ASSERT(uft_floppy_reference_count() == 113u);
    ASSERT(uft_floppy_reference_get(0) != NULL);
    ASSERT(uft_floppy_reference_get(112) != NULL);
    ASSERT(uft_floppy_reference_get(113) == NULL);   /* keine Erfindung */

    /* Die Namensnennung ist Pflicht, nicht Zierde (CC BY-SA 4.0). Sie
     * reist als Makro mit dem Header und nicht nur in einer README. */
    ASSERT(strstr(UFT_FLOPPY_REFERENCE_SOURCE_URL, "wikipedia.org") != NULL);
}

TEST(die_tafel_ist_ihr_eigener_pruefstein)
{
    /* `calculated_payload_bytes` heisst „berechnet" und wurde von
     * niemandem gelesen — die Klasse „Bestand, nicht Faehigkeit"
     * (P3-204/MF-930) in einer frisch geschriebenen Datei. Gefunden hat
     * es die zweite Sitzung, die im selben Baum arbeitet; ihr Hinweis
     * ueberlebte die Torbehebung, weil ein Feld, das das Tor als
     * „geschrieben" sieht, trotzdem ungelesen sein kann.
     *
     * Hier wird daraus ein BELEG: wo eine Zeile eindeutig ist (min ==
     * max fuer Spuren, Seiten und Sektoren), muss das Feld dem Produkt
     * aus Spuren x Seiten x Sektoren x Sektorgroesse entsprechen. Damit
     * prueft die Tafel ihre eigene Arithmetik — die Gestalt von MF-869
     * (die CRCs standen auf der Diskette selbst) und MF-1013.
     *
     * Bereichszeilen bleiben ausdruecklich AUSSEN vor: eine Zeile mit
     * 13..16 Sektoren hat kein einzelnes Produkt, und eines zu waehlen
     * waere eine erfundene Zahl. Ihre Anzahl wird gemeldet, nicht
     * weggerundet (D6). */
    size_t i, n = uft_floppy_reference_count();
    unsigned stimmig = 0u, abweichend = 0u, bereich = 0u;

    for (i = 0; i < n; i++) {
        const uft_floppy_reference_t *r = uft_floppy_reference_get(i);
        uint64_t produkt;
        ASSERT(r != NULL);
        if (r->tracks_min != r->tracks_max ||
            r->sides_min  != r->sides_max  ||
            r->sectors_min != r->sectors_max) { bereich++; continue; }
        produkt = (uint64_t)r->tracks_min * r->sides_min *
                  r->sectors_min * r->bytes_per_sector;
        if (produkt == r->calculated_payload_bytes) stimmig++;
        else {
            abweichend++;
            printf("\n    %s: %llu gegen %llu  ", r->id,
                   (unsigned long long)produkt,
                   (unsigned long long)r->calculated_payload_bytes);
        }
    }
    printf("\n    stimmig=%u abweichend=%u Bereichszeilen=%u  ",
           stimmig, abweichend, bereich);

    /* Die EIGENSCHAFT zuerst: keine einzige eindeutige Zeile weicht ab. */
    ASSERT(abweichend == 0u);
    /* Dann die gemessenen Zahlen — und die Summe ZULETZT, weil sie
     * nichts ueber die Verteilung sagt (MF-1026). */
    ASSERT(stimmig == 98u);
    ASSERT(bereich == 15u);
    ASSERT(stimmig + abweichend + bereich == n);
}

/* ── Defekt 1: Gleichstand ───────────────────────────────────────────── */

TEST(bei_gleichstand_wird_kein_sieger_benannt)
{
    /* 1,44 M: 80 x 2 x 18 x 512, 300 U/min, MFM. Gemessen MF-1184 nennt
     * die alte Fassung hier DREI Saetze mit je 100 Punkten, keiner davon
     * der PC-Satz, und gibt den ersten der Tabelle als Sieger aus. */
    uft_floppy_ranking_t r;
    const uft_floppy_observation_t o =
        beob(80, 2, 18, 512, 300, UFT_FLOPPY_ENCODING_MFM);

    size_t i;
    bool pc_dabei = false;

    ASSERT(uft_floppy_reference_rank(&o, &r));
    printf("\n    1,44M: tied=%zu ambiguous=%d best_compared=%u  ",
           r.tied, (int)r.ambiguous, r.best_compared);

    /* **Gemessen sind es FUENF, nicht drei** — und das ist kein Detail.
     * Die alte Fassung nannte drei und liess den PC-Satz weg, weil ihr
     * zweiter Defekt ihm die fehlende Drehzahl als WIDERSPRUCH anrechnete.
     * Mit der Behebung ist der PC dabei, und die Absage steht trotzdem:
     * eine 1,44-M-Diskette ist an Spuren, Seiten, Sektoren, Sektorgroesse
     * und Kodierung von einer Mac-, Atari- oder PC-98-Diskette NICHT zu
     * unterscheiden. Das ist physikalisch wahr, und der Rangierer darf
     * darueber nicht hinweggehen. */
    ASSERT(r.tied == 5u);
    ASSERT(r.ambiguous);
    /* Und das ist der Kern: bei Mehrdeutigkeit gibt es KEINEN Index. */
    ASSERT(r.best_index == UFT_FLOPPY_REFERENCE_KEIN_INDEX);
    /* Sechs Felder verglichen, eines unbekannt (das Medium). */
    ASSERT(r.best_compared == 6u);
    ASSERT(r.best_widerspricht == 0u);
    ASSERT(r.best_unbekannt == 1u);

    /* Die schaerfste Zusage: der PC-Satz MUSS unter den Gleichrangigen
     * sein. Fehlte er, waere der zweite Defekt zurueck. */
    ASSERT(r.tied_listed == 5u);
    for (i = 0; i < r.tied_listed; i++) {
        const uft_floppy_reference_t *ref =
            uft_floppy_reference_get(r.tied_with[i]);
        ASSERT(ref != NULL);
        if (strcmp(ref->id, "wiki-ibm-pc-compatibles-16") == 0)
            pc_dabei = true;
    }
    ASSERT(pc_dabei);
}

TEST(ein_zusaetzliches_feld_loest_den_gleichstand_auf)
{
    /* Gegenrichtung, ohne die „sagt immer ab" genauso gruen waere — und
     * die eigentliche Rechtfertigung der Absage: sie ist keine
     * Sackgasse, sondern eine FRAGE, und ein weiteres Feld antwortet.
     *
     * Amiga DD, 80 x 2 x 11 x 512: gemessen bleiben ZWEI Saetze uebrig,
     * `wiki-amiga-02` (5 1/4 Zoll) und `wiki-amiga-03` (3 1/2 Zoll). Sie
     * unterscheiden sich in GENAU einem Feld — dem Medium —, und das
     * traegt `uft_geometry_t` nicht. Wird es geliefert, faellt der
     * Gleichstand.
     *
     * (Hier fiel eine Erwartung von mir: `P3-445` notiert fuer Amiga DD
     * `tied == 1`, gemessen an der ALTEN Fassung. Mit der Behebung sind
     * es zwei, und das ist richtiger — die alte Fassung hatte den
     * zweiten Satz ueber die Tabellenreihenfolge verworfen.) */
    uft_floppy_ranking_t ohne, mit;
    const uft_floppy_observation_t o_ohne =
        beob(80, 2, 11, 512, 300, UFT_FLOPPY_ENCODING_MFM);
    uft_floppy_observation_t o_mit = o_ohne;
    /* ASCII, absichtlich: `medium_code()` bildet „3.5", „3 1/2", „3½" und
     * die Tafelform „31<U+2044>2 inch" auf denselben Kode 35 ab. Eine
     * Bruchzeichen-Variante im Testquelltext waere eine Kodierungsfrage
     * ohne Gegenwert — und in diesem Baum sind Escapes in Quelltexten
     * schon oft genug unterwegs zerbrochen (MF-1096 Sperre 1). */
    o_mit.medium = "3.5 inch";

    ASSERT(uft_floppy_reference_rank(&o_ohne, &ohne));
    ASSERT(uft_floppy_reference_rank(&o_mit, &mit));
    printf("\n    AmigaDD ohne Medium: tied=%zu amb=%d | mit: tied=%zu amb=%d  ",
           ohne.tied, (int)ohne.ambiguous, mit.tied, (int)mit.ambiguous);

    /* Ohne das Medium: zwei, also Absage. */
    ASSERT(ohne.tied == 2u);
    ASSERT(ohne.ambiguous);
    ASSERT(ohne.best_index == UFT_FLOPPY_REFERENCE_KEIN_INDEX);
    ASSERT(ohne.best_unbekannt == 1u);          /* genau das Medium */

    /* Mit dem Medium: einer, benannt, und es ist die 3,5-Zoll-Fassung. */
    ASSERT(mit.tied == 1u);
    ASSERT(!mit.ambiguous);
    ASSERT(mit.best_index != UFT_FLOPPY_REFERENCE_KEIN_INDEX);
    ASSERT(mit.best_index < uft_floppy_reference_count());
    ASSERT(mit.best_unbekannt == 0u);           /* nichts bleibt offen */
    {
        const uft_floppy_reference_t *ref =
            uft_floppy_reference_get(mit.best_index);
        ASSERT(ref != NULL);
        ASSERT(strcmp(ref->id, "wiki-amiga-03") == 0);
    }
}

TEST(der_engere_anspruch_gewinnt_vor_der_tabellenreihenfolge)
{
    /* Regel 2 der Sonden-Doktrin. Zwei Saetze mit gleicher Punktzahl, aber
     * unterschiedlich vielen verglichenen Feldern: der mit MEHR
     * verglichenen Feldern gewinnt — nicht der, der zufaellig frueher in
     * der Tabelle steht.
     *
     * Geprueft wird die EIGENSCHAFT, nicht ein Beispiel: wenn ein Sieger
     * benannt ist, darf es keinen gleichpunktigen Satz mit MEHR
     * verglichenen Feldern geben. */
    uft_floppy_ranking_t r;
    const uft_floppy_observation_t o =
        beob(40, 1, 9, 512, 300, UFT_FLOPPY_ENCODING_MFM);

    ASSERT(uft_floppy_reference_rank(&o, &r));
    if (!r.ambiguous && r.best_index != UFT_FLOPPY_REFERENCE_KEIN_INDEX) {
        size_t i;
        for (i = 0; i < r.tied_listed; i++) {
            uft_floppy_ranking_t einzeln;
            ASSERT(uft_floppy_reference_rank_one(&o, r.tied_with[i],
                                                 &einzeln));
            ASSERT(einzeln.best_compared <= r.best_compared);
        }
    }
    printf("\n    40x1x9: tied=%zu ambiguous=%d  ",
           r.tied, (int)r.ambiguous);
}

/* ── Defekt 2: „unbekannt" ist nicht „widerspricht" ──────────────────── */

TEST(eine_fehlende_drehzahl_widerspricht_nicht)
{
    /* Der Normalfall im Baum: `uft_geometry_t` traegt keine Drehzahl.
     * Dieselbe Geometrie einmal MIT und einmal OHNE Drehzahl darf nicht
     * dazu fuehren, dass die Angabe ohne Drehzahl als WIDERSPRUCH zaehlt. */
    uft_floppy_ranking_t mit, ohne;
    const uft_floppy_observation_t o_mit =
        beob(80, 2, 11, 512, 300, UFT_FLOPPY_ENCODING_MFM);
    uft_floppy_observation_t o_ohne = o_mit;
    o_ohne.rpm = 0u;                 /* 0 = keine Angabe */

    ASSERT(uft_floppy_reference_rank(&o_mit, &mit));
    ASSERT(uft_floppy_reference_rank(&o_ohne, &ohne));
    printf("\n    mit rpm: wider=%u unbek=%u | ohne rpm: wider=%u unbek=%u  ",
           mit.best_widerspricht, mit.best_unbekannt,
           ohne.best_widerspricht, ohne.best_unbekannt);

    /* Kern der Zusage: fehlende Angabe erhoeht `unbekannt`, NICHT
     * `widerspricht`. */
    ASSERT(ohne.best_widerspricht == mit.best_widerspricht);
    ASSERT(ohne.best_unbekannt > mit.best_unbekannt);
}

TEST(ein_echter_widerspruch_zaehlt_als_widerspruch)
{
    /* Gegenrichtung: ohne sie waere „zaehlt nie einen Widerspruch"
     * genauso gruen. 4711 Sektoren je Spur traegt kein Referenzsatz.
     *
     * **Gefragt wird EINZELN, und der Grund ist eine Zusage der API:**
     * `best_*` traegt die Zahlen eines SIEGERS, und bei 4711 gibt es
     * keinen — alle Saetze widersprechen, also ist kein Kandidat da und
     * die `best_*`-Felder bleiben 0. Hier fiel eine zweite Erwartung von
     * mir; ich hatte `best_widerspricht` ueber alle Saetze abgefragt und
     * damit ein Feld, das es in dieser Lage nicht gibt. `rank_one()`
     * steht genau dafuer da. */
    uft_floppy_ranking_t einzeln, alle;
    const uft_floppy_observation_t o =
        beob(80, 2, 4711, 512, 300, UFT_FLOPPY_ENCODING_MFM);

    ASSERT(uft_floppy_reference_rank_one(&o, 0u, &einzeln));
    printf("\n    Satz 0: tr=%u wi=%u un=%u kand=%zu  ",
           einzeln.best_trifft, einzeln.best_widerspricht,
           einzeln.best_unbekannt, einzeln.kandidaten);
    /* Fuenf der sechs verglichenen Felder widersprechen, eines trifft
     * (die Sektorgroesse 512), das Medium bleibt unbekannt. */
    ASSERT(einzeln.best_widerspricht == 5u);
    ASSERT(einzeln.best_trifft == 1u);
    ASSERT(einzeln.best_unbekannt == 1u);
    /* Und trotz eines Treffers ist es KEIN Kandidat — ein Widerspruch
     * schliesst aus, gleich wie viel daneben passt. */
    ASSERT(einzeln.kandidaten == 0u);
    ASSERT(einzeln.best_index == UFT_FLOPPY_REFERENCE_KEIN_INDEX);

    /* Ueber alle Saetze: kein Kandidat, und deshalb keine `best_*`-Zahlen
     * — „nichts gefunden" wird nicht mit Zahlen ausgekleidet. */
    ASSERT(uft_floppy_reference_rank(&o, &alle));
    ASSERT(alle.kandidaten == 0u);
    ASSERT(alle.best_widerspricht == 0u);
    ASSERT(!alle.ambiguous);
}

TEST(ein_referenzsatz_ohne_angabe_widerspricht_nicht)
{
    /* **Das ist der Fall, den `P3-445` gemessen hat** — und meine erste
     * Fassung dieses Tests hat ihn VERFEHLT: sie prueft die
     * BEOBACHTUNGSseite (Drehzahl fehlt in der Beobachtung), waehrend der
     * Befund die REFERENZseite betrifft („3 von 3 Saetzen ohne
     * Drehzahlangabe bekommen einen Abweichungszaehler"). Die
     * Mutationsmatrix hat die Luecke gefunden, nicht ich.
     *
     * Gemessen am Bestand: **3 von 113** Saetzen haben `rpm_min == 0`
     * (Index 43, 94, 111) und **1** hat `ENCODING_UNKNOWN` (Index 94).
     * `wiki-sega-sf-7000-01` traegt BEIDES und ist damit der eine Satz,
     * an dem sich beide Haelften des Defekts pruefen lassen. */
    uft_floppy_ranking_t r;
    const uft_floppy_observation_t o =
        beob(40, 2, 16, 256, 300, UFT_FLOPPY_ENCODING_MFM);
    const uft_floppy_reference_t *sega = uft_floppy_reference_get(94u);

    ASSERT(sega != NULL);
    ASSERT(strcmp(sega->id, "wiki-sega-sf-7000-01") == 0);
    /* Die Voraussetzung wird GEPRUEFT, nicht angenommen: dieser Satz
     * nennt weder Drehzahl noch Kodierung. */
    ASSERT(sega->rpm_min == 0u && sega->rpm_max == 0u);
    ASSERT(sega->encoding == UFT_FLOPPY_ENCODING_UNKNOWN);

    ASSERT(uft_floppy_reference_rank_one(&o, 94u, &r));
    printf("\n    Sega: tr=%u wi=%u un=%u kand=%zu  ",
           r.best_trifft, r.best_widerspricht, r.best_unbekannt,
           r.kandidaten);

    /* Vier Felder treffen (Spuren, Seiten, Sektoren, Sektorgroesse),
     * KEINES widerspricht, drei bleiben unbekannt (Drehzahl, Kodierung,
     * Medium). Die alte Fassung haette hier zwei Widersprueche gezaehlt
     * und den Satz damit ausgeschlossen. */
    ASSERT(r.best_trifft == 4u);
    ASSERT(r.best_widerspricht == 0u);
    ASSERT(r.best_unbekannt == 3u);
    /* Und deshalb bleibt er ein Kandidat — was er sein muss. */
    ASSERT(r.kandidaten == 1u);
}

TEST(der_engere_anspruch_gewinnt_auch_wenn_er_spaeter_kommt)
{
    /* Regel 2 mit Zaehnen, und der Fall ist am Bestand gemessen:
     *
     *   idx  43  `wiki-commodore-900-01`            80x2x13..16x512, rpm 0
     *   idx 110  `wiki-victor-9000-act-sirius-1-02` dieselbe Geometrie, rpm 252
     *
     * Beide erreichen 100 Punkte. Der Satz mit WENIGER verglichenen
     * Feldern steht ZUERST — genau die Lage, in der die alte Fassung ihn
     * gewinnen liess. Mit Regel 2 gewinnt der spaetere, weil er mehr
     * behauptet und trotzdem trifft.
     *
     * Diese Zusage faengt zwei Mutationen: „engerer Anspruch gewinnt
     * nicht mehr" und „Gleichstand ohne Vergleich der Feldzahl".
     *
     * **GCR, nicht MFM** — und das ist keine Feinheit, sondern eine
     * gefallene Erwartung von mir: beide Saetze sind GCR-kodiert, und mit
     * MFM widersprach jeder von ihnen, sodass es gar keinen Kandidaten
     * gab. Gemessen am Bestand statt angenommen. */
    uft_floppy_ranking_t r;
    const uft_floppy_observation_t o =
        beob(80, 2, 14, 512, 252, UFT_FLOPPY_ENCODING_GCR);
    const uft_floppy_reference_t *ref;

    ASSERT(uft_floppy_reference_rank(&o, &r));
    printf("\n    80x2x14x512@252: tied=%zu amb=%d cmp=%u idx=%zu  ",
           r.tied, (int)r.ambiguous, r.best_compared, r.best_index);

    ASSERT(!r.ambiguous);
    ASSERT(r.tied == 1u);
    ASSERT(r.best_compared == 6u);          /* der engere Anspruch */
    ASSERT(r.best_index != UFT_FLOPPY_REFERENCE_KEIN_INDEX);
    ref = uft_floppy_reference_get(r.best_index);
    ASSERT(ref != NULL);
    ASSERT(strcmp(ref->id, "wiki-victor-9000-act-sirius-1-02") == 0);
}

TEST(viele_gleichrangige_werden_gezaehlt_auch_ueber_die_liste_hinaus)
{
    /* Eine Beobachtung mit EINEM Feld — nur die Sektorgroesse. Jeder Satz
     * mit 512 Byte trifft, keiner widerspricht, alle erreichen 100 bei
     * einem verglichenen Feld. Damit ist der Gleichstand groesser als die
     * Liste, und `tied` muss weiterzaehlen, wo `tied_listed` aufhoert —
     * wie `uft_probe_ranking.tied` gegen `tied_listed` (MF-729).
     *
     * Die Zahl steht ZULETZT und als gemessene; die Zusage davor ist die
     * Eigenschaft (MF-1026: eine Summe sagt nichts ueber die
     * Verteilung). */
    uft_floppy_ranking_t r;
    uft_floppy_observation_t o;
    memset(&o, 0, sizeof(o));
    o.bytes_per_sector = 512u;      /* alles andere unbekannt */

    ASSERT(uft_floppy_reference_rank(&o, &r));
    printf("\n    nur bps=512: kand=%zu tied=%zu listed=%zu cmp=%u  ",
           r.kandidaten, r.tied, r.tied_listed, r.best_compared);

    ASSERT(r.ambiguous);
    ASSERT(r.best_index == UFT_FLOPPY_REFERENCE_KEIN_INDEX);
    ASSERT(r.best_compared == 1u);
    /* Mehr Gleichrangige als Listenplaetze — und `tied` sagt die
     * Wahrheit, waehrend `tied_listed` bei der Kapazitaet stehenbleibt. */
    ASSERT(r.tied > UFT_FLOPPY_RANKING_MAX_TIED);
    ASSERT(r.tied_listed == UFT_FLOPPY_RANKING_MAX_TIED);
    ASSERT(r.tied == r.kandidaten);
}

TEST(eine_leere_beobachtung_passt_zu_nichts)
{
    /* Ein Satz, der zu KEINEM Feld der Beobachtung etwas sagt, ist kein
     * Kandidat — „nichts widerspricht" allein genuegt nicht. Ohne diese
     * Zusage waere „Kandidat, sobald kein Widerspruch" gruen, und dann
     * passten alle 113 Saetze zu einer Beobachtung ohne jede Angabe. */
    uft_floppy_ranking_t r;
    uft_floppy_observation_t o;
    memset(&o, 0, sizeof(o));       /* nichts gesetzt, encoding UNKNOWN */

    ASSERT(uft_floppy_reference_rank(&o, &r));
    printf("\n    leere Beobachtung: kand=%zu tied=%zu  ",
           r.kandidaten, r.tied);
    ASSERT(r.kandidaten == 0u);
    ASSERT(r.tied == 0u);
    ASSERT(!r.ambiguous);
    ASSERT(r.best_index == UFT_FLOPPY_REFERENCE_KEIN_INDEX);
}

TEST(eine_geometrie_ohne_jeden_treffer_ist_kein_gleichstand)
{
    /* Anti-Tautologie und D6: „nichts gefunden" und „mehrdeutig" sind
     * zwei verschiedene Ergebnisse, und keines davon ist ein Sieger. */
    uft_floppy_ranking_t r;
    const uft_floppy_observation_t o =
        beob(4711, 7, 4711, 4711, 4711, UFT_FLOPPY_ENCODING_MFM);

    ASSERT(uft_floppy_reference_rank(&o, &r));
    printf("\n    nur Unsinn: kandidaten=%zu tied=%zu ambiguous=%d  ",
           r.kandidaten, r.tied, (int)r.ambiguous);
    ASSERT(r.kandidaten == 0u);
    ASSERT(r.best_index == UFT_FLOPPY_REFERENCE_KEIN_INDEX);
    /* Und ausdruecklich NICHT mehrdeutig — es gibt nichts, worueber man
     * unentschieden sein koennte. */
    ASSERT(!r.ambiguous);
}

TEST(ohne_beobachtung_gibt_es_keine_zahlen)
{
    uft_floppy_ranking_t r;
    memset(&r, 0xFF, sizeof(r));
    ASSERT(!uft_floppy_reference_rank(NULL, &r));
    ASSERT(!uft_floppy_reference_rank_one(NULL, 0, &r));
    {
        const uft_floppy_observation_t o =
            beob(80, 2, 11, 512, 300, UFT_FLOPPY_ENCODING_MFM);
        ASSERT(!uft_floppy_reference_rank(&o, NULL));
        /* Ein Index jenseits des Bestands ist keine Absage mit Zahlen. */
        ASSERT(!uft_floppy_reference_rank_one(&o, 113u, &r));
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== P3-445: der Rangierer sagt bei Gleichstand ab ===\n");
    printf("--- Der Bestand ---\n");
    RUN(der_bestand_ist_da_und_nennt_seine_quelle);
    RUN(die_tafel_ist_ihr_eigener_pruefstein);
    printf("--- Defekt 1: Gleichstand ---\n");
    RUN(bei_gleichstand_wird_kein_sieger_benannt);
    RUN(ein_zusaetzliches_feld_loest_den_gleichstand_auf);
    RUN(der_engere_anspruch_gewinnt_vor_der_tabellenreihenfolge);
    RUN(der_engere_anspruch_gewinnt_auch_wenn_er_spaeter_kommt);
    RUN(viele_gleichrangige_werden_gezaehlt_auch_ueber_die_liste_hinaus);
    printf("--- Defekt 2: unbekannt gegen widerspricht ---\n");
    RUN(eine_fehlende_drehzahl_widerspricht_nicht);
    RUN(ein_referenzsatz_ohne_angabe_widerspricht_nicht);
    RUN(ein_echter_widerspruch_zaehlt_als_widerspruch);
    printf("--- D6 und Anti-Tautologie ---\n");
    RUN(eine_leere_beobachtung_passt_zu_nichts);
    RUN(eine_geometrie_ohne_jeden_treffer_ist_kein_gleichstand);
    RUN(ohne_beobachtung_gibt_es_keine_zahlen);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
