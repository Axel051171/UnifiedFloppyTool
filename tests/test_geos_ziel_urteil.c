/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_geos_ziel_urteil.c — eine D64 kann den GEOS-Bootschutz nicht
 * tragen, und das Werkzeug sagt es VORHER (MF-1333, Stufe 5).
 *
 * ── Die Zusage ───────────────────────────────────────────────────────
 * Der Schutz liegt in den Luecken ZWISCHEN den Sektoren. Ein Ziel, das
 * nur Sektoren speichert, kann ihn prinzipiell nicht darstellen — das
 * ist keine Einschraenkung der Umsetzung, sondern des Formats. Also
 * muss es ein HARTER Konflikt sein, keine Warnung, die man wegklickt.
 *
 * ── Und die zweite, die genauso wichtig ist ──────────────────────────
 * Das Urteil wird ABGELEITET, nicht getafelt. `uft_geos_ziel_pruefen()`
 * fragt `uft_format_traegt()` nach den Ebenen des Zielformats. Eine
 * zweite Liste in der Oberflaeche oder hier waere die Bauform aus
 * MF-1177 — dieselbe Groesse an zwei Stellen gerechnet, und die
 * Abweichung sieht aus wie ein Fehler in den Daten.
 *
 * Zusage 4 nagelt das fest: sie prueft, dass Urteil und Tafel
 * DIESELBE Antwort geben, fuer jedes getafelte Format.
 *
 * ── Die Belege ───────────────────────────────────────────────────────
 * `docs/format_specs/commodore/D64.TXT:18` — "comprised of 256 byte
 * sectors". `G64.TXT:47` — "simply the raw stream of GCR data", und
 * `:400` fuehrt den Kopfzwischenraum von neun Byte ausdruecklich auf.
 * Verfahren und Lueckenmuster: Beschreibung des Urhebers von GeoCopy
 * (`neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt`), Kanal *Spec*
 * nach MF-695.
 *
 * ── Was dieser Test NICHT abdeckt ────────────────────────────────────
 * Die Qt-Oberflaeche. Ob ein Auswahlfeld wirklich gesperrt wird, laesst
 * sich hier nicht pruefen; das braucht eine Klick-Sitzung, und die
 * Definition of Done dieses Baums verlangt sie ausdruecklich fuer
 * GUI-Aenderungen. Geprueft ist die REGEL, an die ein Widget gebunden
 * werden muss — mehr kann ein Kopftest nicht, und weniger waere eine
 * Zusage ohne Deckung.
 */

#include <stdio.h>
#include <string.h>

#include "uft/core/uft_copy_job.h"
#include "uft/core/uft_format_traegt.h"

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-66s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

int main(void)
{
    printf("Eine D64 kann den Bootschutz nicht tragen (MF-1333)\n");

    /* ── 1. Vorbedingung: beide Formate sind getafelt ───────────────*/
    {
        uft_format_traegt_t d64 = uft_format_traegt(UFT_FORMAT_D64);
        uft_format_traegt_t g64 = uft_format_traegt(UFT_FORMAT_G64);
        if (!d64.bekannt || !g64.bekannt) {
            printf("  [ROT] D64 oder G64 fehlt in uft_format_traegt() — "
                   "Vorbedingung traegt nicht\n");
            return 1;
        }
        zusage(1, "Vorbedingung: D64 und G64 sind in der Tafel gefuehrt");
        zusage(d64.quelle && strstr(d64.quelle, "D64.TXT") != NULL,
               "die D64-Zeile nennt ihre Fundstelle");
        zusage(g64.quelle && strstr(g64.quelle, "G64.TXT") != NULL,
               "die G64-Zeile nennt ihre Fundstelle");
    }

    /* ── 2. DIE ZUSAGE: D64 ist ein harter Konflikt ─────────────────*/
    {
        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_EXAKT,
                                     UFT_FORMAT_D64, true)
               == UFT_GEOS_ZIEL_KEINE_LUECKEN,
               "D64 + exakt erhalten: KEINE_LUECKEN");
        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_NACHBAU,
                                     UFT_FORMAT_D64, true)
               == UFT_GEOS_ZIEL_KEINE_LUECKEN,
               "D64 + rekonstruieren: KEINE_LUECKEN");
        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_AUTO,
                                     UFT_FORMAT_D64, true)
               == UFT_GEOS_ZIEL_KEINE_LUECKEN,
               "D64 + automatisch: KEINE_LUECKEN");

        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_AUS,
                                     UFT_FORMAT_D64, false)
               == UFT_GEOS_ZIEL_OK,
               "D64 + ignorieren: erlaubt — es wird nichts versprochen");
    }

    /* ── 3. G64 traegt es ───────────────────────────────────────────*/
    {
        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_EXAKT,
                                     UFT_FORMAT_G64, true)
               == UFT_GEOS_ZIEL_OK,
               "G64 + exakt erhalten (Quelle mit Rohspur): erlaubt");
        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_NACHBAU,
                                     UFT_FORMAT_G64, false)
               == UFT_GEOS_ZIEL_OK,
               "G64 + rekonstruieren ohne Rohspur: erlaubt");

        /* Der feine Unterschied, den die Analyse ausdruecklich
         * verlangt: "exakt" und "nachgebaut" duerfen nicht dasselbe
         * heissen. Ohne Rohspur gibt es nichts zu UEBERNEHMEN. */
        zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_EXAKT,
                                     UFT_FORMAT_G64, false)
               == UFT_GEOS_ZIEL_QUELLE_OHNE_ROHSPUR,
               "G64 + exakt OHNE Rohspur: abgesagt, nicht nachgebaut");
    }

    /* ── 4. Urteil und Tafel sagen DASSELBE ─────────────────────────
     *
     * Die eigentliche Zusage gegen eine zweite Tafel: fuer JEDES
     * Format, das `uft_format_traegt()` kennt, muss das Urteil genau
     * dann `KEINE_LUECKEN` lauten, wenn die Tafel weder Bitstrom- noch
     * Flussebene fuehrt. Weicht eines ab, sind es zwei Rechnungen. */
    {
        const uft_format_id_t ids[] = {
            UFT_FORMAT_TD0, UFT_FORMAT_IMD, UFT_FORMAT_IMG,
            UFT_FORMAT_XFD, UFT_FORMAT_ADF,
            UFT_FORMAT_D64, UFT_FORMAT_G64
        };
        const unsigned n = (unsigned)(sizeof ids / sizeof ids[0]);
        unsigned einig = 0, getafelt = 0;

        for (unsigned i = 0; i < n; i++) {
            uft_format_traegt_t t = uft_format_traegt(ids[i]);
            if (!t.bekannt) continue;
            getafelt++;

            unsigned luecken_ebenen =
                (1u << UFT_D2_LAYER_BITSTREAM) | (1u << UFT_D2_LAYER_FLUX);
            int tafel_sagt_nein = (t.layers & luecken_ebenen) == 0u;
            int urteil_sagt_nein =
                uft_geos_ziel_pruefen(UFT_GEOS_AKTION_NACHBAU, ids[i], true)
                    == UFT_GEOS_ZIEL_KEINE_LUECKEN;

            if (tafel_sagt_nein == urteil_sagt_nein) einig++;
        }

        zusage(getafelt == n,
               "alle sieben abgefragten Formate sind getafelt");
        zusage(einig == getafelt,
               "Urteil und Tafel sind sich bei JEDEM einig — eine Rechnung");
    }

    /* ── 5. Ein ungetafeltes Format wird NICHT geraten ──────────────*/
    {
        /* Dass SCP Fluss traegt, ist offensichtlich — aber die Tafel
         * sagt es nicht, und "offensichtlich" ist keine Messung. */
        uft_format_traegt_t scp = uft_format_traegt(UFT_FORMAT_SCP);
        if (!scp.bekannt) {
            zusage(uft_geos_ziel_pruefen(UFT_GEOS_AKTION_EXAKT,
                                         UFT_FORMAT_SCP, true)
                   == UFT_GEOS_ZIEL_UNBEKANNT,
                   "ungetafeltes Format: UNBEKANNT statt geraten");
        } else {
            zusage(1, "SCP ist inzwischen getafelt — Fall entfaellt");
        }
    }

    /* ── 6. Jedes Urteil hat einen Satz, jede Aktion einen Namen ────*/
    {
        const uft_geos_ziel_urteil_t urteile[] = {
            UFT_GEOS_ZIEL_OK, UFT_GEOS_ZIEL_KEINE_LUECKEN,
            UFT_GEOS_ZIEL_UNBEKANNT, UFT_GEOS_ZIEL_QUELLE_OHNE_ROHSPUR
        };
        int alle = 1;
        for (unsigned i = 0; i < 4; i++) {
            const char *s = uft_geos_ziel_urteil_text(urteile[i]);
            if (!s || !s[0]) alle = 0;
        }
        zusage(alle, "jedes Urteil hat einen Satz fuer den Bediener");

        const uft_geos_aktion_t aktionen[] = {
            UFT_GEOS_AKTION_AUS, UFT_GEOS_AKTION_AUTO,
            UFT_GEOS_AKTION_EXAKT, UFT_GEOS_AKTION_NACHBAU
        };
        alle = 1;
        for (unsigned i = 0; i < 4; i++) {
            const char *s = uft_geos_aktion_name(aktionen[i]);
            if (!s || !s[0]) alle = 0;
        }
        zusage(alle, "jede Aktion hat einen Namen");

        /* Und das Lueckenmuster sagt "nicht gemessen", wo nichts
         * gemessen wurde — nicht "keiner" (MF-1311). */
        zusage(strstr(uft_geos_gap_name(UFT_GEOS_GAP_UNBEKANNT),
                      "nicht gemessen") != NULL,
               "ein ungemessenes Lueckenmuster heisst 'nicht gemessen'");
        zusage(strstr(uft_geos_gap_name(UFT_GEOS_GAP_ORIGINAL),
                      "67") != NULL,
               "das Originalmuster nennt sein Byte");
    }

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}
