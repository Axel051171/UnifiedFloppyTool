/**
 * @file uft_json.c
 * @brief Umsetzung der JSON-Maskierregel. Der Anlass steht in
 *        `include/uft/util/uft_json.h` (MF-1241, Punkt `P3-482`).
 *
 * Die Tafel unten ist die des Urhebers — `write_json_string()` in
 * `src/core/uft_loss_report.c:37`, bis MF-1241 die einzige und
 * dateilokale Umsetzung im ganzen Baum (gemessen: 0 exportierte
 * Maskierfunktionen). Sie ist hier nicht neu erfunden, sondern
 * VERSCHOBEN; `uft_loss_report.c` ruft sie seit MF-1241, und damit
 * gibt es weiterhin genau EINE (`CLAUDE.md` §MF-1177).
 */

#include "uft/util/uft_json.h"

#include <stdio.h>
#include <string.h>

const char *uft_json_escape_byte(unsigned char c, char *buf)
{
    if (!buf) return NULL;

    switch (c) {
    case '"':  memcpy(buf, "\\\"", 3); return buf;
    case '\\': memcpy(buf, "\\\\", 3); return buf;
    case '\b': memcpy(buf, "\\b",  3); return buf;
    case '\f': memcpy(buf, "\\f",  3); return buf;
    case '\n': memcpy(buf, "\\n",  3); return buf;
    case '\r': memcpy(buf, "\\r",  3); return buf;
    case '\t': memcpy(buf, "\\t",  3); return buf;
    default:
        if (c < 0x20u) {
            /* `%04x` klein geschrieben wie im Urheber, damit das
             * Erzeugnis byteweise dasselbe bleibt. Sechs Zeichen plus
             * Null sind genau UFT_JSON_ESC_MAX. */
            snprintf(buf, UFT_JSON_ESC_MAX, "\\u%04x", (unsigned)c);
            return buf;
        }
        /* Bytes >= 0x80 fallen hierher und bleiben unveraendert. Das
         * ist Absicht: sie durchzulassen ist fuer UTF-8 richtig und
         * die einzige Wahl, die keine Annahme ueber die Kodierung
         * trifft. */
        return NULL;
    }
}

bool uft_json_escape(const char *s, char *out, size_t out_size,
                     size_t *needed)
{
    /* Erst rechnen, dann schreiben. Nur so kann die Funktion ABSAGEN
     * statt zu kappen — eine halb maskierte JSON-Zeichenkette waere
     * schlimmer als keine (Dauerregel D5, `DESIGN_PRINCIPLES.md`). */
    size_t braucht;
    char esc[UFT_JSON_ESC_MAX];

    if (!s) {
        braucht = 5u;                      /* "null" + Nullbyte */
    } else {
        braucht = 3u;                      /* zwei Anfuehrungszeichen
                                            * + Nullbyte */
        for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
            const char *e = uft_json_escape_byte(*p, esc);
            braucht += e ? strlen(e) : 1u;
        }
    }
    if (needed) *needed = braucht;

    if (!out || out_size == 0u) return false;
    if (braucht > out_size) {
        out[0] = '\0';
        return false;
    }

    if (!s) {
        memcpy(out, "null", 5);
        return true;
    }

    size_t n = 0;
    out[n++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        const char *e = uft_json_escape_byte(*p, esc);
        if (e) {
            const size_t l = strlen(e);
            memcpy(out + n, e, l);
            n += l;
        } else {
            out[n++] = (char)*p;
        }
    }
    out[n++] = '"';
    out[n] = '\0';
    return true;
}
