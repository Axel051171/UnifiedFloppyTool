/**
 * @file test_capability_manifest.c
 * @brief Eine erklärte Fähigkeit muss beweisbar sein (MF-658)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * 88 Plugins deklarieren ein Fähigkeits-Manifest — `Read`, `Write`,
 * `Create`, `Flux`, `Timing`, `Weak Bits`, `MultiRev`, je
 * `UNSUPPORTED / PARTIAL / SUPPORTED`. Zusammen rund 590 Aussagen.
 *
 * Gemessen vor diesem Test: **null** Stellen im Baum lesen sie.
 * `grep -rn feature_count src/` liefert 88 Deklarationen und 0 Leser.
 *
 * Solange niemand hinsieht, ist eine falsche Zeile folgenlos. Der Plan
 * `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md` will die Oberfläche
 * daraus steuern — welche Bedienelemente ein Format überhaupt anbietet.
 * In dem Moment wird aus einer falschen Zeile eine **falsche Zusage an
 * den Benutzer**, und wir hätten tote Knöpfe gegen Lügen getauscht.
 *
 * Deshalb steht dieses Tor VOR der Verdrahtung. Es ist der
 * Rotbeweis-Gedanke auf der Ebene der Architektur: erst messen, ob die
 * Grundlage trägt, dann darauf bauen.
 *
 * ── Was geprüft wird ─────────────────────────────────────────────────────
 *
 * Nur das mechanisch Entscheidbare — eine Fähigkeit, deren Träger ein
 * Funktionszeiger ist:
 *
 *     "Read"   = SUPPORTED  ->  open != NULL && read_track != NULL
 *     "Write"  = SUPPORTED  ->  write_track != NULL && CAP_WRITE gesetzt
 *     "Create" = SUPPORTED  ->  create != NULL
 *
 * `Flux`, `Timing`, `Weak Bits`, `MultiRev` haben keinen eigenen
 * Zeiger; sie sind hier bewusst NICHT geprüft. Sie zu raten wäre
 * schlimmer als sie offen zu lassen — der Test sagt am Ende, wie viele
 * Aussagen er nicht beurteilen konnte.
 *
 * Zusätzlich die Gegenrichtung, die genauso schadet: ein Plugin, das
 * eine Funktion HAT und sie als `UNSUPPORTED` führt. Die Oberfläche
 * würde ein Element ausblenden, das funktioniert. Das ist kein Fehler
 * mit Datenverlust, aber eine Unwahrheit — und es wird gemeldet.
 *
 * ── Was der Test NICHT kann ──────────────────────────────────────────────
 *
 * Er prüft, ob ein Zeiger da ist — nicht, ob die Funktion taugt. Ein
 * `write_track`, das `UFT_ERROR_NOT_SUPPORTED` zurückgibt, besteht
 * dieses Tor. Das ist die Grenze, und sie steht hier, damit niemand
 * mehr in die Zahl hineinliest, als sie trägt.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <string.h>

#define MAX_PLUGINS 256

static int fehler;
static int ungeprueft;
static int geprueft;

static uft_feature_support_t stufe(const uft_format_plugin_t *p,
                                   const char *name, bool *gefunden)
{
    *gefunden = false;
    if (!p->features || p->feature_count == 0) return UFT_FEATURE_UNSUPPORTED;
    for (size_t i = 0; i < p->feature_count; i++) {
        if (p->features[i].name && strcmp(p->features[i].name, name) == 0) {
            *gefunden = true;
            return p->features[i].status;
        }
    }
    return UFT_FEATURE_UNSUPPORTED;
}

static const char *stufenname(uft_feature_support_t s)
{
    switch (s) {
    case UFT_FEATURE_UNSUPPORTED: return "UNSUPPORTED";
    case UFT_FEATURE_PARTIAL:     return "PARTIAL";
    case UFT_FEATURE_SUPPORTED:   return "SUPPORTED";
    default:                      return "?";
    }
}

/* Eine Aussage gegen ihren Träger halten.
 *
 * @param traeger  ist die Funktion vorhanden?
 * @param zusatz   zusätzliche Bedingung (z. B. CAP-Flag); true, wenn keine
 */
static void pruefe(const uft_format_plugin_t *p, const char *feature,
                   bool traeger, const char *traeger_name, bool zusatz,
                   const char *zusatz_name)
{
    bool gefunden = false;
    uft_feature_support_t s = stufe(p, feature, &gefunden);
    if (!gefunden) return;          /* nicht deklariert = keine Aussage */
    geprueft++;

    if (s == UFT_FEATURE_SUPPORTED && !traeger) {
        printf("  ZU VIEL  %-14s \"%s\" = SUPPORTED, aber %s ist NULL\n",
               p->name ? p->name : "(namenlos)", feature, traeger_name);
        fehler++;
        return;
    }
    if (s == UFT_FEATURE_SUPPORTED && !zusatz) {
        printf("  ZU VIEL  %-14s \"%s\" = SUPPORTED, aber %s fehlt\n",
               p->name ? p->name : "(namenlos)", feature, zusatz_name);
        fehler++;
        return;
    }
    if (s == UFT_FEATURE_UNSUPPORTED && traeger && zusatz) {
        printf("  ZU WENIG %-14s \"%s\" = UNSUPPORTED, aber %s ist da\n",
               p->name ? p->name : "(namenlos)", feature, traeger_name);
        fehler++;
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    const uft_format_plugin_t *plugins[MAX_PLUGINS];
    size_t n = uft_list_format_plugins(plugins, MAX_PLUGINS);
    printf("Fähigkeits-Manifest gegen die Wirklichkeit (MF-658)\n");
    printf("%zu Plugins in der Registry\n\n", n);
    if (n == 0) {
        printf("FEHLER: keine Plugins registriert\n");
        return 1;
    }

    int ohne_manifest = 0;
    for (size_t i = 0; i < n; i++) {
        const uft_format_plugin_t *p = plugins[i];
        if (!p) continue;
        if (!p->features || p->feature_count == 0) { ohne_manifest++; continue; }

        pruefe(p, "Read",   p->open && p->read_track,
               "open/read_track", true, NULL);
        pruefe(p, "Write",  p->write_track != NULL, "write_track",
               (p->capabilities & UFT_FORMAT_CAP_WRITE) != 0,
               "UFT_FORMAT_CAP_WRITE");
        pruefe(p, "Create", p->create != NULL, "create", true, NULL);

        /* PARTIAL ohne Begruendung. Der Header sagt: `note` ist
         * "Pflicht bei PARTIAL um die Einschraenkung zu erklaeren"
         * (uft_format_plugin.h:90). Eine Einschraenkung, die sich nicht
         * erklaert, kann die Oberflaeche auch nicht anzeigen — und der
         * Plan sieht genau diesen Text als Hinweis am Bedienelement vor. */
        for (size_t k = 0; k < p->feature_count; k++) {
            if (p->features[k].status != UFT_FEATURE_PARTIAL) continue;
            geprueft++;
            if (!p->features[k].note || !p->features[k].note[0]) {
                printf("  STUMM    %-14s \"%s\" = PARTIAL ohne Begruendung "
                       "(note ist Pflicht)\n",
                       p->name ? p->name : "(namenlos)",
                       p->features[k].name ? p->features[k].name : "?");
                fehler++;
            }
        }

        /* Ohne eigenen Traeger — gezaehlt, nicht beurteilt. */
        static const char *ohne_traeger[] = {
            "Flux", "Timing", "Weak Bits", "MultiRev"
        };
        for (size_t k = 0; k < sizeof(ohne_traeger)/sizeof(ohne_traeger[0]); k++) {
            bool gefunden = false;
            uft_feature_support_t s = stufe(p, ohne_traeger[k], &gefunden);
            (void)s;
            if (gefunden) ungeprueft++;
        }
    }

    printf("\n%d Aussagen mechanisch geprüft, %d nicht beurteilbar "
           "(kein eigener Funktionszeiger)\n", geprueft, ungeprueft);
    if (ohne_manifest)
        printf("%d Plugins ohne Manifest — sie sagen nichts und lügen "
               "damit auch nicht\n", ohne_manifest);
    printf("\n%s (%d Widersprüche)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
