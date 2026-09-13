/**
 * @file test_endung_nennt_kein_fremdes_format.c
 * @brief Eine Endung im Plugin-Eintrag waehlt das ZIEL beim Schreiben -
 *        und `dim` beanspruchte `.xdf` (MF-1087)
 *
 * ── Was die Endung entscheidet ──────────────────────────────────────────
 *
 * `uft_resolve_format_plugin()` waehlt das Plugin, in dessen Format
 * geschrieben werden soll. Seine Leiter steht im Quelltext:
 *
 *     1. die Format-ID, wenn genau EIN Plugin sie traegt
 *     2. die Endung von `path_hint`, unter den Plugins mit dieser ID
 *     3. nichts
 *
 * Stufe 1 traegt hier fast nie: **131 der 137 Eintraege fuehren
 * `UFT_FORMAT_DSK`**. Also entscheidet in der Regel die **Endung**, und
 * der Kommentar an der Stelle begruendet das ausdruecklich - die Datei
 * gibt es noch nicht, `"out.d81"` ist eine Absichtserklaerung, und die
 * Absicht IST das Zielformat.
 *
 * Genau deshalb ist eine falsche Endung im Plugin-Eintrag kein
 * Schoenheitsfehler: sie leitet einen Schreibvorgang um.
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uft_format_plugin_dim` heisst **„Sharp X68000 Disk Image"**, sein
 * ganzer Kopf (MF-1037) handelt von den vier X68000-Medienbytes - und
 * seine Endungsliste lautete `"dim;xdf"`. Das Wort **XDF kommt in der
 * ganzen Datei sonst nicht vor**.
 *
 * XDF ist IBMs *eXtended Density Format* und ein anderes Format. MF-1064
 * hat dazu bereits gemessen: `uft_xdf_api.c` ist **nicht registriert**,
 * es gibt fuer XDF **kein Plugin**. Und `.xdf` wird im ganzen Baum von
 * **genau einem** Eintrag beansprucht - diesem.
 *
 * Beides zusammen heisst: wer `out.xdf` als Ziel angibt, bekommt den
 * **X68000-DIM-Schreiber**. Eine Absicht wird still in eine andere
 * uebersetzt - dieselbe Klasse wie MF-1064 („die Liste nennt, was
 * gelesen werden SOLL"), nur auf der Schreibseite und mit Folgen.
 *
 * ── Wie es aufgefallen ist ──────────────────────────────────────────────
 *
 * Nicht durch Lesen. Der Erzeuger-Zensus kennt seit MF-1087 auch
 * `floptool`, und in seiner Tafel stand danach genau eine Zeile als
 * „Werkzeug sagt RW, Kanal UNGEMESSEN": `dim` gegen floptools **`xdf`**
 * (`XDF disk image [xdf,hdm,2hd]`). Der Zensus hatte recht mit der
 * Zuordnung und unrecht mit dem Kandidaten - die Endung, ueber die er
 * zuordnete, gehoert dem DIM-Eintrag nicht.
 *
 * ── Was dieser Test festhaelt ───────────────────────────────────────────
 *
 * Nicht nur den Einzelfall. Die allgemeine Zusage ist: **keine Endung
 * darf auf ein Plugin zeigen, dessen Format sie nicht ist** - geprueft
 * an der einen Stelle, die im Baum mechanisch entscheidbar ist, naemlich
 * `.xdf`, fuer das es nach MF-1064 gar kein Plugin gibt.
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/** Beansprucht `pl` die Endung `ext`? Trennzeichen wie im Produktivcode. */
static int beansprucht(const uft_format_plugin_t *pl, const char *ext)
{
    const char *s;
    size_t want = strlen(ext);
    if (!pl || !pl->extensions) return 0;
    for (s = pl->extensions; *s; ) {
        const char *start;
        size_t len, i;
        while (*s == ';' || *s == ',' || *s == ' ' || *s == '.') s++;
        if (!*s) break;
        start = s;
        while (*s && *s != ';' && *s != ',' && *s != ' ') s++;
        len = (size_t)(s - start);
        if (len != want) continue;
        for (i = 0; i < len; i++)
            if (tolower((unsigned char)start[i]) != tolower((unsigned char)ext[i]))
                break;
        if (i == len) return 1;
    }
    return 0;
}

int main(void)
{
    char det[300];
    size_t n, i, traeger_xdf = 0;
    const uft_format_plugin_t *ziel;
    size_t kandidaten = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Endung nennt kein fremdes Format - MF-1087\n");
    printf("==========================================\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats() scheitert\n");
        return 1;
    }
    n = uft_get_format_count();

    /* ── 1. Kein Eintrag beansprucht `.xdf` ────────────────────────── */
    {
        const char *wer = "keiner";
        for (i = 0; i < n; i++) {
            const uft_format_plugin_t *pl = uft_get_format_by_index(i);
            if (beansprucht(pl, "xdf")) {
                traeger_xdf++;
                wer = pl->name ? pl->name : "?";
            }
        }
        snprintf(det, sizeof det, "%zu Eintraege, zuletzt \"%s\"",
                 traeger_xdf, wer);
        pruefe("kein Plugin beansprucht die Endung `.xdf` - MF-1064 hat "
               "gemessen, dass es fuer XDF ueberhaupt kein registriertes "
               "Plugin gibt; wer sie trotzdem fuehrt, faengt fremde "
               "Schreibabsichten ab", traeger_xdf == 0, det);
    }

    /* ── 2. `out.xdf` loest auf NICHTS auf ─────────────────────────── */
    ziel = uft_resolve_format_plugin(UFT_FORMAT_DSK, "out.xdf", &kandidaten);
    snprintf(det, sizeof det, "%s (unter %zu Kandidaten)",
             ziel && ziel->name ? ziel->name : "NULL", kandidaten);
    pruefe("ein Ziel `out.xdf` waehlt KEIN Plugin - vor MF-1087 kam hier "
           "\"Sharp X68000 Disk Image\" heraus, weil `uft_format_plugin_dim` "
           "die Endung `dim;xdf` fuehrte und `.xdf` sonst niemand "
           "beansprucht; eine Absicht wurde damit still in eine andere "
           "uebersetzt", ziel == NULL, det);

    /* ── 3. Die Gegenprobe: `.dim` selbst bleibt erreichbar ────────── */
    {
        size_t traeger_dim = 0;
        for (i = 0; i < n; i++)
            if (beansprucht(uft_get_format_by_index(i), "dim")) traeger_dim++;
        snprintf(det, sizeof det, "%zu Eintraege fuehren `.dim`",
                 traeger_dim);
        pruefe("`.dim` fuehren weiterhin ZWEI Eintraege (`dim` und "
               "`dim_atari`) - die Endung wird nicht aus dem Eintrag "
               "genommen, nur die fremde daneben", traeger_dim == 2, det);
    }

    /* ── 4. Und bei zwei Anwaertern raet der Verteiler nicht ───────── */
    ziel = uft_resolve_format_plugin(UFT_FORMAT_DSK, "out.dim", &kandidaten);
    snprintf(det, sizeof det, "%s", ziel && ziel->name ? ziel->name : "NULL");
    pruefe("ein Ziel `out.dim` waehlt ebenfalls KEIN Plugin - zwei "
           "Eintraege beanspruchen die Endung, und der Verteiler sagt "
           "dann ausdruecklich ab statt den ersten zu nehmen "
           "(\"two plugins claim it: still a guess\")", ziel == NULL, det);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
