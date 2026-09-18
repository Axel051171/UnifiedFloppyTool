/*
 * @file test_xdf_json_verteiler.c
 * @brief Rotbeweis zu MF-1235 — `xdf_api_process_json()` waehlte den
 *        Befehl mit sechs `strstr()` ueber die GANZE Zeichenkette, und
 *        drei seiner sechs Zweige gaben nicht initialisierten Heap als
 *        JSON zurueck.
 *
 * REFERENZ (EINFRIER-REGEL, Bedingung a): es gibt KEINE fremde
 * Beschreibung dieses Befehlsschemas — die Funktion hat im ganzen Baum
 * **0 Aufrufer** (`git grep xdf_api_process_json` liefert nur ihre
 * Definition in `uft_xdf_api_impl.c:936` und den Prototyp in
 * `uft_xdf_api.h:591`). Die Grundlage ist deshalb eine MESSUNG AM
 * PRODUKTIONSPFAD, die VOR der Korrektur steht: eine Wegwerf-Sonde hat
 * die Funktion selbst gerufen und ihre Antworten gedruckt. Gemessen am
 * Vorzustand (2026-09-18, gcc 13.1.0):
 *
 *   {"command":"close"}                      -> {"success": false}
 *                                               18 Byte
 *   {"command":"close","label":"analyze"}    -> {"success": false,
 *                                                "confidence": 0.00}
 *                                               38 Byte — ANALYZE lief,
 *                                               nicht close
 *   {"command":"close","note":"open"}        -> 6 Byte, erstes Byte 0x50,
 *                                               kein JSON
 *   {"command":"open"}   (ohne "path":)      -> dieselben 6 Byte
 *   {"command":"info"}   (ohne offene Datei) -> dieselben 6 Byte
 *   {"command":"grid"}   (ohne offene Datei) -> dieselben 6 Byte
 *   {"command":"open","path":""}             -> {"success": false,
 *                                    "error": "Cannot open file: }"}
 *                                               der Pfad ist `}`
 *   {"command":"wetter"}                     -> {"error":
 *                                                "Unknown command"}
 *
 * WARUM das keine Kleinigkeit ist, auch ohne Aufrufer:
 *
 *  1. Ein harmloses NEBENFELD kippt den Befehl. Die Pruefungen laufen in
 *     der Reihenfolge open, analyze, info, grid, close — die ERSTE
 *     Fundstelle gewinnt, egal in welchem Feld sie steht.
 *  2. `result` kommt aus einem blanken `malloc(4096)`, und drei Zweige
 *     haben KEIN `else`. Was dann herauskommt, ist Heap-Rest. Dass er
 *     hier reproduzierbar `0x50 ...` lautet, macht ihn nicht definiert.
 *  3. Der leere Pfad: die Schleife ueberspringt ALLE Anfuehrungszeichen
 *     und laeuft in den Rest des JSON. Aus `""` wird `}` — und das
 *     unmaskierte `}` in der Fehlermeldung zerbricht das JSON, in dem
 *     es steht. Eine Fehlermeldung, die den Leser ihres Formats
 *     verwirrt, ist schlimmer als keine.
 *  4. Der Pfad wird bei 255 Zeichen STILL gekappt und dann GEOEFFNET
 *     (`while (… && i < 255)`, `uft_xdf_api_impl.c:951`). Der Header
 *     nennt keine Laengengrenze (`xdf_api_open(xdf_api_t*, const char*)`,
 *     `uft_xdf_api.h:270`), es ist also eine Hausregel. Dauerregel D5:
 *     niemals klemmen, melden.
 *
 *     ZURUECKGENOMMEN: hier stand „und ein still gekappter Pfad oeffnet
 *     die FALSCHE Datei". Das ist NICHT belegt. Der erste Lauf dieses
 *     Tests hat es widerlegt: mit 300 `x` meldet der Vorzustand einen
 *     Fehler (304 Byte Antwort), weil sich auch der gekappte Pfad nicht
 *     oeffnen laesst. Um das Oeffnen der falschen Datei zu zeigen,
 *     braeuchte es einen gekappten Pfad, der eine EXISTIERENDE Datei
 *     trifft — dafuer liegt kein Fall im Korpus. Belegt ist allein: die
 *     Laenge wird nicht genannt, und es wird mit dem gekappten Pfad
 *     geoeffnet. Das reicht fuer D5 und mehr wird nicht behauptet.
 *
 * WAS DIESER TEST AUSDRUECKLICH NICHT ENTSCHEIDET (S5): welcher
 * SCHLUESSEL den Befehl traegt. `{"note":"open"}` und
 * `{"command":"open"}` sind fuer eine Teilstring-Suche dasselbe, und
 * ohne Aufrufer ist der Vertrag unbestimmt. Festgenagelt wird deshalb
 * nur, was unter JEDER Lesart gilt: bei MEHRDEUTIGKEIT wird abgesagt
 * statt geraten (dieselbe Doktrin wie `logical` seit MF-1032 und `cpm`
 * seit MF-1039), jeder Zweig liefert ein DEFINITES JSON-Objekt, und
 * nichts wird still gekappt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/xdf/uft_xdf_api.h"

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

static int hat(const char *heu, const char *nadel)
{
    return heu && strstr(heu, nadel) != NULL;
}

/* Antwort einsammeln und die Grundzusagen pruefen, die fuer JEDE
 * Antwort gelten. Gibt die Zeichenkette zurueck; der Aufrufer gibt sie
 * frei. */
static char *antwort(xdf_api_t *api, const char *befehl, const char *name)
{
    char *r = xdf_api_process_json(api, befehl);
    char merk[160];

    snprintf(merk, sizeof merk, "%s: Antwort ist nicht NULL", name);
    ZUSAGE(r != NULL, merk);
    if (!r) return NULL;

    /* Nullterminierung SELBST nachsehen, bevor `strlen` darauf laeuft —
     * sonst waere der Test sein eigener Ueberlauf. */
    size_t len = 4096;
    for (size_t i = 0; i < 4096; i++) {
        if (r[i] == '\0') { len = i; break; }
    }
    snprintf(merk, sizeof merk,
             "%s: in 4096 Byte steht eine Null (Laenge %zu)", name, len);
    ZUSAGE(len < 4096, merk);

    snprintf(merk, sizeof merk,
             "%s: beginnt mit '{' (gemessen 0x%02X)", name,
             (unsigned char)r[0]);
    ZUSAGE(r[0] == '{', merk);

    return r;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("MF-1235 — der JSON-Verteiler von XDF\n");

    xdf_api_t *api = xdf_api_create();
    if (!api) {
        printf("   [ROT] xdf_api_create() gab NULL\n");
        return 1;
    }

    /* -- 1) Bezugsfall: ein einzelner Befehl wird ausgefuehrt --------- */
    printf("\n1) close allein\n");
    char *r = antwort(api, "{\"command\":\"close\"}", "close");
    ZUSAGE(hat(r, "\"success\""), "close: nennt \"success\"");
    ZUSAGE(!hat(r, "\"confidence\""),
           "close: nennt NICHT \"confidence\" (das waere analyze)");
    free(r);

    /* -- 2) Ein Nebenfeld darf den Befehl nicht kippen ---------------- */
    printf("\n2) dasselbe close mit einem Nebenfeld\n");
    r = antwort(api, "{\"command\":\"close\",\"label\":\"analyze\"}",
                "close+analyze");
    /* Vorzustand: `{"success": false, "confidence": 0.00}` — analyze
     * lief. Zugesagt wird eine ABSAGE, nicht das stille Ausfuehren des
     * einen oder des anderen. */
    ZUSAGE(hat(r, "\"error\""),
           "close+analyze: sagt AB statt zu raten");
    ZUSAGE(!hat(r, "\"confidence\""),
           "close+analyze: hat analyze NICHT ausgefuehrt");
    free(r);

    r = antwort(api, "{\"command\":\"close\",\"note\":\"open\"}",
                "close+open");
    ZUSAGE(hat(r, "\"error\""), "close+open: sagt AB statt zu raten");
    free(r);

    /* -- 3) Kein Zweig darf ohne Antwort zurueckkehren ---------------- */
    printf("\n3) die drei Zweige ohne else\n");
    r = antwort(api, "{\"command\":\"open\"}", "open ohne path");
    ZUSAGE(hat(r, "\"error\""),
           "open ohne \"path\": nennt einen Fehler statt Heap-Rest");
    free(r);

    r = antwort(api, "{\"command\":\"info\"}", "info");
    ZUSAGE(hat(r, "\"error\""),
           "info ohne offene Datei: nennt einen Fehler");
    free(r);

    r = antwort(api, "{\"command\":\"grid\"}", "grid");
    ZUSAGE(hat(r, "\"error\""),
           "grid ohne offene Datei: nennt einen Fehler");
    free(r);

    /* -- 4) Der Zweig, der schon im Vorzustand richtig war ------------ */
    printf("\n4) unbekannter Befehl (war richtig, wird festgenagelt)\n");
    r = antwort(api, "{\"command\":\"wetter\"}", "unbekannt");
    ZUSAGE(hat(r, "\"error\""), "unbekannt: nennt einen Fehler");
    free(r);

    /* -- 5) Der leere Pfad -------------------------------------------- */
    printf("\n5) leerer Pfad\n");
    r = antwort(api, "{\"command\":\"open\",\"path\":\"\"}", "leerer Pfad");
    ZUSAGE(hat(r, "\"error\""), "leerer Pfad: nennt einen Fehler");
    /* Vorzustand: `"Cannot open file: }"` — der Pfad WAR `}`. */
    ZUSAGE(!hat(r, "Cannot open file: }"),
           "leerer Pfad: nimmt NICHT das '}' als Pfad");
    free(r);

    /* -- 6) Ein zu langer Pfad wird abgesagt, nicht gekappt (D5) ------ */
    printf("\n6) Pfad ueber 255 Zeichen\n");
    {
        char lang[512];
        size_t p = 0;
        p += (size_t)snprintf(lang + p, sizeof lang - p,
                              "{\"command\":\"open\",\"path\":\"");
        for (int i = 0; i < 300 && p < sizeof lang - 8; i++) {
            lang[p++] = 'x';
        }
        snprintf(lang + p, sizeof lang - p, "\"}");

        r = antwort(api, lang, "langer Pfad");

        /* BERICHTIGT beim ersten Lauf. Hier standen zuerst zwei Zusagen
         * — „nennt einen Fehler" und „meldet keinen Erfolg" —, und
         * BEIDE waren im Vorzustand GRUEN: der auf 255 Zeichen gekappte
         * Pfad laesst sich ebenso wenig oeffnen wie der ganze, also kam
         * ohnehin ein `"error"`. Sie waren gruen aus dem falschen Grund
         * und haetten die Kappung nicht bemerkt (Klasse MF-1014).
         *
         * Sie bleiben als SPERRE stehen, aber die Entscheidung faellt
         * jetzt an zwei Zusagen, die den Unterschied wirklich sehen:
         * die Laenge muss BENANNT werden, und es darf kein Oeffnen mit
         * dem gekappten Pfad versucht worden sein. */
        ZUSAGE(hat(r, "\"error\""),
               "Pfad > 255: nennt einen Fehler (Sperre, nicht Beweis)");
        ZUSAGE(!hat(r, "\"success\": true"),
               "Pfad > 255: meldet keinen Erfolg (Sperre)");
        ZUSAGE(hat(r, "too long") || hat(r, "zu lang"),
               "Pfad > 255: die LAENGE wird benannt, nicht nur ein "
               "Oeffnungsfehler");
        ZUSAGE(!hat(r, "Cannot open file: xxx"),
               "Pfad > 255: es wurde NICHT mit dem gekappten Pfad "
               "geoeffnet");
        free(r);
    }

    /* -- 7) TAUTOLOGIE-SPERRE: die Absage darf nicht alles treffen ---- */
    printf("\n7) Gegenproben — die Absage darf NICHT ueberschiessen\n");
    /* Ein Befehlswort im PFAD ist keine Mehrdeutigkeit: gesucht wird
     * `"analyze"` MIT beiden Anfuehrungszeichen, und in
     * `"path":"C:/x/analyze.xdf"` steht vor `analyze` ein `/` und
     * dahinter ein `.`. */
    r = antwort(api, "{\"command\":\"open\",\"path\":\"C:/x/analyze.xdf\"}",
                "Pfad mit dem Wort analyze");
    ZUSAGE(!hat(r, "mehrdeutig") && !hat(r, "ambiguous"),
           "ein Befehlswort IM Pfad ist keine Mehrdeutigkeit");
    ZUSAGE(hat(r, "\"error\"") || hat(r, "\"success\""),
           "und die Antwort ist trotzdem definit");
    free(r);

    xdf_api_destroy(api);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
