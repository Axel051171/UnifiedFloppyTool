/**
 * @file uft_json.h
 * @brief Die JSON-Maskierregel dieses Baums — an EINER Stelle.
 *
 * ANLASS, gemessen (MF-1241, Punkt `P3-482`): dieser Baum hatte GENAU
 * EINEN JSON-Maskierer, `write_json_string()` in
 * `src/core/uft_loss_report.c:37`. Er ist vollstaendig und richtig,
 * aber `static` und auf einen `FILE*` schreibend — von anderswo also
 * unerreichbar.
 *
 * Der Bedarf war kein Gedankenspiel. MF-1235 hat gemessen, dass
 * `xdf_api_process_json()` den Pfad einer Datei in seine Fehlermeldung
 * einbettete und damit das JSON zerbrach:
 *
 *     {"success": false, "error": "Cannot open file: }"}
 *
 * — ein unmaskiertes `}` aus dem Pfad. Und ein gewoehnlicher
 * Windows-Pfad traegt `\`, was dasselbe taete. MF-1235 hat sich
 * deshalb mit `"error_code"` behelfen muessen und den Text
 * WEGGELASSEN: eine staerkere Zusage (jeder Zweig ist unbedingt
 * gueltiges JSON), aber ein Verlust an Auskunft.
 *
 * Eine ZWEITE Kopie der Maskierregel zu schreiben waere „eine Groesse,
 * zwei Rechnungen" gewesen — der Grundsatz aus `CLAUDE.md` §MF-1177,
 * in diesem Baum fuenfmal bezahlt (MF-1015 drei Pruefsummen, MF-1026
 * drei Victor-Geometrien, MF-1032/1034 die vier Anordnungsgesetze
 * zweimal einzeln wiederentdeckt). Deshalb liegt die Regel jetzt hier,
 * und `uft_loss_report.c` ruft sie.
 *
 * WARUM ZWEI FUNKTIONEN UND NICHT EINE: die beiden Aufrufer haben
 * verschiedene Gestalt. Der Verlustbericht schreibt fortlaufend in
 * einen Strom und kennt die Laenge seiner Beschreibungen nicht; ein
 * Umbau auf „erst puffern, dann schreiben" braeuchte eine Groesse, die
 * niemand kennt, und waere die naechste stille Kappung. Der
 * JSON-Verteiler dagegen hat einen festen 4096-Byte-Puffer. Die REGEL
 * steht deshalb in `uft_json_escape_byte()` — ein Byte hinein, seine
 * Ersetzung heraus —, und beide Formen bauen darauf. Es gibt genau
 * eine Tafel.
 */

#ifndef UFT_JSON_H
#define UFT_JSON_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Laengster Ersatz: `\u00xx` plus Null. */
#define UFT_JSON_ESC_MAX 7

/**
 * Die Ersetzung fuer EIN Byte, oder NULL.
 *
 * @param c    das Byte
 * @param buf  Arbeitspuffer des Aufrufers, mindestens
 *             `UFT_JSON_ESC_MAX` Byte
 * @return `buf`, gefuellt mit der nullterminierten Ersetzung, oder
 *         **NULL**, wenn das Byte unveraendert uebernommen wird
 *
 * Die Tafel ist die des Urhebers in `uft_loss_report.c`: `"` und `\`
 * bekommen einen Gegenschraegstrich, `\b \f \n \r \t` ihre Kuerzel,
 * alles unter 0x20 wird `\u00xx`, alles andere bleibt. Bytes >= 0x80
 * bleiben ausdruecklich UNVERAENDERT — sie durchzulassen ist richtig
 * fuer UTF-8-Eingaben und die einzige Wahl, die keine Annahme ueber
 * die Kodierung trifft.
 */
const char *uft_json_escape_byte(unsigned char c, char *buf);

/**
 * Schreibt @p s als JSON-Zeichenkette MIT Anfuehrungszeichen nach
 * @p out.
 *
 * @param s        nullterminiert; **NULL ergibt das Literal `null`**
 *                 ohne Anfuehrungszeichen — genau wie im Urheber
 * @param out      Ziel
 * @param out_size dessen Groesse
 * @param needed   optional: die noetige Groesse **einschliesslich**
 *                 Nullbyte. Wird auch bei Erfolg gesetzt.
 * @return true, wenn alles passte
 *
 * **Sie KAPPT NICHT.** Passt es nicht, bleibt @p out eine leere
 * Zeichenkette (sofern `out_size > 0`), der Rueckgabewert ist false,
 * und @p needed nennt die Zahl. Dauerregel D5: melden, nicht klemmen —
 * eine halb maskierte JSON-Zeichenkette waere schlimmer als keine,
 * weil sie den Leser des Formats verwirrt statt ihn abzuweisen.
 */
bool uft_json_escape(const char *s, char *out, size_t out_size,
                     size_t *needed);

#ifdef __cplusplus
}
#endif

#endif /* UFT_JSON_H */
