/*
 * @file test_json_maskierung_eine_tafel.c
 * @brief Rotbeweis zu MF-1241 (Punkt `P3-482`) — die JSON-Maskierregel
 *        steht an EINER Stelle, und der XDF-Verteiler erreicht sie.
 *
 * DER BEFUND, GEMESSEN BEI MF-1235: `xdf_api_process_json()` bettete
 * den Fehlertext des Kerns in sein Erzeugnis ein, und der Text traegt
 * den Pfad. Vorzustand, gemessen:
 *
 *     {"success": false, "error": "Cannot open file: }"}
 *
 * — ein unmaskiertes `}` aus dem Pfad, das das JSON zerbricht, in dem
 * es steht. Ein gewoehnlicher Windows-Pfad traegt `\`, was dasselbe
 * taete. MF-1235 hat sich deshalb mit `"error_code"` behelfen muessen
 * und den TEXT WEGGELASSEN: eine staerkere Zusage, aber ein Verlust an
 * Auskunft.
 *
 * Der Grund, warum es nicht sofort behoben wurde, ist gemessen: der
 * Baum hatte GENAU EINEN JSON-Maskierer — `write_json_string()` in
 * `src/core/uft_loss_report.c:37` —, und der ist `static` und schreibt
 * in einen `FILE*`. Aus `src/formats/xdf/` unerreichbar. Eine zweite
 * Kopie waere „eine Groesse, zwei Rechnungen" gewesen (`CLAUDE.md`
 * §MF-1177, in diesem Baum fuenfmal bezahlt).
 *
 * WAS DIESER TEST ALSO BEWEIST, IN DREI STUECKEN:
 *   1) die Tafel — Byte hinein, Ersetzung heraus, jede Zeile einzeln;
 *   2) die Puffer-Form sagt AB statt zu kappen (Dauerregel D5) und
 *      NENNT die noetige Groesse;
 *   3) der XDF-Verteiler liefert den Text WIEDER, maskiert, und sein
 *      Erzeugnis ist ein gueltiges JSON-Objekt.
 *
 * WARUM ER ROT WERDEN KANN — und zwar an drei verschiedenen Stellen:
 *   - Stueck 1+2 konnten vor diesem Commit nicht einmal BINDEN:
 *     `uft_json_escape_byte`/`uft_json_escape` gab es nicht (gemessen
 *     `git grep -hoE '\b[a-z_]*escape[a-z_]*\s*\('` ueber `src` und
 *     `include`: 0 exportierte Namen).
 *   - Stueck 3 ist gegen den Vorzustand von `uft_xdf_api_impl.c`
 *     gelaufen und faellt dort: das Feld `"error"` fehlt.
 *   - Die Gueltigkeitspruefung unten ist NICHT der Maskierer selbst,
 *     sondern ein eigener Zustandsautomat. Ein Test, der den Pruefling
 *     nach seinem eigenen Urteil fragt, ist ein geschlossener Kreis —
 *     die Gestalt von `apridisk` (MF-1009) und `qrst` (MF-1028).
 *
 * DER ANKER GEGEN DEN LEERLAUF liegt ausserhalb dieser Datei:
 * `tests/test_loss_report.c::json_escape_special_chars` prueft seit
 * langem, dass `\"quotes\"`, `\\` und `\n` woertlich in der
 * `.loss.json` stehen. Wenn die Verschiebung der Tafel deren Ausgabe
 * um ein Byte aendert, faellt dieser fremde Test — und genau das macht
 * ihn zur Gegenprobe fuer den Umbau, den dieser Test nicht sieht.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/util/uft_json.h"
#include "uft/xdf/uft_xdf_api.h"

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

/* Ein eigener, kleiner Gueltigkeitspruefer fuer ein JSON-OBJEKT.
 *
 * Er prueft die EIGENSCHAFT, nicht den Erzeuger: ausserhalb von
 * Zeichenketten muessen die Klammern aufgehen, jede Zeichenkette muss
 * enden, und INNERHALB einer Zeichenkette darf kein rohes
 * Steuerzeichen und kein unmaskiertes `"` stehen. Er ruft absichtlich
 * NICHTS aus `uft_json.h` — sonst waere die Antwort die des Prueflings.
 *
 * Was er NICHT prueft, und das gehoert gesagt: Schluessel-Wert-Syntax,
 * Zahlenformate, Kommata. Er ist ein Zerbrechlichkeits-Pruefer, kein
 * JSON-Leser. Dass er nein sagen KANN, ist unten vorgefuehrt. */
static int json_objekt_ok(const char *s)
{
    if (!s || *s != '{') return 0;

    int tiefe = 0;
    int in_kette = 0;

    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (in_kette) {
            if (*p == '\\') {
                if (!p[1]) return 0;      /* Gegenschraegstrich am Ende */
                ++p;                      /* das maskierte Zeichen */
                continue;
            }
            if (*p == '"')   { in_kette = 0; continue; }
            if (*p < 0x20)   return 0;    /* rohes Steuerzeichen */
            continue;
        }
        if (*p == '"') { in_kette = 1; continue; }
        if (*p == '{') {
            tiefe++;
        } else if (*p == '}') {
            if (--tiefe < 0) return 0;
        }
    }
    return !in_kette && tiefe == 0;
}

/* Bequemer Vergleich einer Byte-Ersetzung. `erwartet == NULL` heisst
 * „das Byte wird unveraendert uebernommen". */
static int ersetzung_ist(unsigned char c, const char *erwartet)
{
    char buf[UFT_JSON_ESC_MAX];
    const char *e = uft_json_escape_byte(c, buf);
    if (!erwartet) return e == NULL;
    return e != NULL && strcmp(e, erwartet) == 0;
}

int main(void)
{
    printf("=== MF-1241 / P3-482: eine Tafel, und der Verteiler "
           "erreicht sie ===\n");

    /* ---- 1) DIE TAFEL, Zeile fuer Zeile -------------------------- */
    printf("\n1) die Tafel des Urhebers, Byte fuer Byte\n");
    ZUSAGE(ersetzung_ist('"',  "\\\""), "`\"`  -> \\\"");
    ZUSAGE(ersetzung_ist('\\', "\\\\"), "`\\`  -> \\\\");
    ZUSAGE(ersetzung_ist('\b', "\\b"),  "0x08 -> \\b");
    ZUSAGE(ersetzung_ist('\f', "\\f"),  "0x0c -> \\f");
    ZUSAGE(ersetzung_ist('\n', "\\n"),  "0x0a -> \\n");
    ZUSAGE(ersetzung_ist('\r', "\\r"),  "0x0d -> \\r");
    ZUSAGE(ersetzung_ist('\t', "\\t"),  "0x09 -> \\t");

    /* Die Steuerzeichen ohne Kuerzel, klein geschrieben wie im
     * Urheber — eine Abweichung hier waere eine stille Aenderung der
     * `.loss.json`. */
    ZUSAGE(ersetzung_ist(0x01, "\\u0001"), "0x01 -> \\u0001");
    ZUSAGE(ersetzung_ist(0x1f, "\\u001f"), "0x1f -> \\u001f (klein)");

    /* DIE GRENZEN, in beide Richtungen. Ohne sie waere „maskiert
     * richtig" auch mit einer Regel gruen, die ALLES maskiert. */
    printf("\n2) die Grenzen — was NICHT angefasst wird\n");
    ZUSAGE(ersetzung_ist(0x20, NULL), "0x20 (Leerzeichen) bleibt");
    ZUSAGE(ersetzung_ist('A',  NULL), "'A' bleibt");
    ZUSAGE(ersetzung_ist('/',  NULL), "'/' bleibt (JSON verlangt es nicht)");
    ZUSAGE(ersetzung_ist(0x80, NULL), "0x80 bleibt — UTF-8 durchgelassen");
    ZUSAGE(ersetzung_ist(0xff, NULL), "0xff bleibt");

    /* Der NULL-Puffer darf nicht schreiben. */
    ZUSAGE(uft_json_escape_byte('"', NULL) == NULL,
           "ohne Puffer: NULL, kein Schreibversuch");

    /* ---- 3) DIE PUFFER-FORM: melden, nicht kappen ----------------- */
    printf("\n3) die Puffer-Form sagt AB statt zu kappen (D5)\n");
    {
        char out[64];
        size_t noetig = 0;

        ZUSAGE(uft_json_escape(NULL, out, sizeof out, &noetig)
               && strcmp(out, "null") == 0,
               "NULL ergibt das Literal null, ohne Anfuehrungszeichen");
        ZUSAGE(noetig == 5, "und nennt dafuer 5 Byte");

        ZUSAGE(uft_json_escape("a", out, sizeof out, &noetig)
               && strcmp(out, "\"a\"") == 0,
               "\"a\" kommt MIT Anfuehrungszeichen zurueck");
        ZUSAGE(noetig == 4, "und nennt dafuer 4 Byte");

        /* Ein Windows-Pfad mit allem, was weh tut. */
        const char *pfad = "c:\\weg\"mit\"}\n.scp";
        ZUSAGE(uft_json_escape(pfad, out, sizeof out, &noetig),
               "ein Pfad mit \\ \" } und Zeilenumbruch passt in 64 Byte");
        ZUSAGE(strstr(out, "\\\\") != NULL,  "der \\ ist maskiert");
        ZUSAGE(strstr(out, "\\\"") != NULL,  "das \" ist maskiert");
        ZUSAGE(strstr(out, "\\n") != NULL,   "der Umbruch ist maskiert");
        ZUSAGE(strchr(out, '}') != NULL,
               "das } bleibt — es ist INNERHALB einer Kette harmlos");
    }
    {
        /* DIE ABSAGE, und die Gegenprobe dazu: derselbe Aufruf mit der
         * GENANNTEN Groesse muss gelingen. Ohne diese zweite Haelfte
         * waere „sagt ab" auch mit einer Regel gruen, die immer absagt
         * (Klasse MF-1014). */
        const char *lang = "ein Pfad, der nicht passt";
        char klein[8];
        size_t noetig = 0;

        ZUSAGE(!uft_json_escape(lang, klein, sizeof klein, &noetig),
               "zu kleiner Puffer: false");
        ZUSAGE(klein[0] == '\0',
               "und der Puffer ist LEER, nicht halb gefuellt");
        ZUSAGE(noetig == strlen(lang) + 3,
               "und `needed` nennt die Zahl (Laenge + 2 Zeichen + Null)");

        char passend[64];
        ZUSAGE(noetig <= sizeof passend
               && uft_json_escape(lang, passend, noetig, NULL),
               "GEGENPROBE: mit genau dieser Groesse gelingt es");
        ZUSAGE(strlen(passend) + 1 == noetig,
               "und die genannte Zahl war exakt, nicht grosszuegig");

        /* Kein Puffer ueberhaupt: `needed` wird trotzdem gesetzt —
         * genau so fragt ein Aufrufer nach der Groesse. */
        size_t frage = 0;
        ZUSAGE(!uft_json_escape(lang, NULL, 0, &frage) && frage == noetig,
               "ohne Puffer: false, aber `needed` antwortet");
    }

    /* ---- 4) DER XDF-VERTEILER ERREICHT DIE TAFEL ------------------ */
    printf("\n4) der XDF-Verteiler liefert den Fehlertext wieder\n");
    {
        xdf_api_t *api = xdf_api_create();
        if (!api) {
            printf("   [ROT] xdf_api_create() gab NULL\n");
            rot++;
        } else {
            /* Ein Pfad, der nicht existiert UND Zeichen traegt, die
             * ein JSON zerbrechen: zwei Gegenschraegstriche und ein
             * `}`. Ein `"` kann hier nicht stehen — der Verteiler
             * liest den Pfad als GENAU EINE JSON-Zeichenkette und
             * endet am naechsten `"` (MF-1235). */
            const char *befehl =
                "{\"command\":\"open\",\"path\":\"c:\\nirgends}\\x.img\"}";

            char *erg = xdf_api_process_json(api, befehl);
            ZUSAGE(erg != NULL, "der Verteiler antwortet");

            if (erg) {
                printf("      Erzeugnis: %s\n", erg);

                /* SPERREN ZUERST: wir sehen wirklich den Fehlerzweig.
                 * Ohne sie sagt „`error` ist da" nichts, weil ein
                 * gelungenes `open` gar keinen Text haette. */
                ZUSAGE(strstr(erg, "\"success\": false") != NULL,
                       "SPERRE: das Oeffnen ist gescheitert");
                ZUSAGE(strstr(erg, "\"error_code\"") != NULL,
                       "SPERRE: der maschinenlesbare Code bleibt da");
                ZUSAGE(strstr(xdf_api_get_error(api), "nirgends") != NULL,
                       "SPERRE: der Kerntext traegt den Pfad wirklich");
                ZUSAGE(strchr(xdf_api_get_error(api), '\\') != NULL,
                       "SPERRE: und darin steht ein roher \\");

                /* DER BEWEIS. */
                ZUSAGE(strstr(erg, "\"error\"") != NULL,
                       "der Fehlertext ist WIEDER im Erzeugnis");
                ZUSAGE(strstr(erg, "\\\\") != NULL,
                       "und sein \\ steht maskiert darin");
                ZUSAGE(json_objekt_ok(erg),
                       "das Ganze ist ein gueltiges JSON-Objekt");

                xdf_api_free_json(erg);
            }

            /* GEGENPROBE: ein Zweig OHNE Fehler darf kein `"error"`
             * tragen. Ohne diese Zeile waere die Zusage oben auch mit
             * einer Umsetzung gruen, die in JEDEN Zweig ein `error`
             * schreibt. */
            char *zu = xdf_api_process_json(api, "{\"command\":\"close\"}");
            ZUSAGE(zu != NULL && json_objekt_ok(zu),
                   "GEGENPROBE: `close` ergibt gueltiges JSON");
            ZUSAGE(zu != NULL && strstr(zu, "\"error\"") == NULL,
                   "GEGENPROBE: und es traegt kein `error`-Feld");
            if (zu) xdf_api_free_json(zu);

            xdf_api_destroy(api);
        }
    }

    /* ---- 5) DER PRUEFER SELBST KANN NEIN SAGEN -------------------- */
    printf("\n5) der Gueltigkeitspruefer kann nein sagen\n");
    ZUSAGE(json_objekt_ok("{\"a\": \"}\"}"),
           "eine Klammer INNERHALB einer Kette zaehlt nicht mit");
    ZUSAGE(!json_objekt_ok("{\"a\": \"unbeendet}"),
           "eine unbeendete Kette faellt");
    ZUSAGE(!json_objekt_ok("{\"a\": \"x\""),
           "eine offene Klammer faellt");
    ZUSAGE(!json_objekt_ok("{\"a\": \"roh\nes\"}"),
           "ein rohes Steuerzeichen in der Kette faellt");
    ZUSAGE(!json_objekt_ok("nicht mal ein Objekt"),
           "was nicht mit { beginnt, faellt");

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
