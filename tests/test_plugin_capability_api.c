/**
 * @file test_plugin_capability_api.c
 * @brief Das Manifest steuert die Oberfläche — und zwar richtig (MF-660)
 *
 * Stufe 2 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`.
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * Nicht „die API kompiliert", sondern: **verschiedene Formate bekommen
 * verschiedene Bedienelemente, und zwar die, die ihr Manifest ansagt.**
 * Eine Abfrage, die für jedes Format dasselbe liefert, wäre nutzlos —
 * genau das prüft `formate_unterscheiden_sich()`.
 *
 * ── Die drei Stufen und ihre Wirkung ─────────────────────────────────────
 *
 *     SUPPORTED    -> UFT_CONTROL_SHOW           sichtbar, bedienbar
 *     PARTIAL      -> UFT_CONTROL_SHOW_LIMITED   sichtbar, mit Hinweis
 *     UNSUPPORTED  -> UFT_CONTROL_HIDE           ausgeblendet
 *
 * `PARTIAL` wird bewusst NICHT versteckt: eine eingeschränkte Fähigkeit
 * zu verbergen nimmt dem Benutzer eine Information, die er hat. Der
 * Header verlangt für `PARTIAL` ohnehin eine Begründung
 * (`uft_format_plugin.h:90`) — die wird der Hinweistext.
 *
 * ── Und der Fall, der die ganze Stufe rechtfertigt ───────────────────────
 *
 * Die Oberfläche führt in `src/formattab.cpp` eine ZWEITE
 * Fähigkeits-Tabelle (`m_formatInfo`, 25 handgeschriebene Einträge).
 * Gemessen widerspricht sie dem Manifest an fünf Stellen: EDSK, G64,
 * NIB, SCP und TD0 versprechen dort Weak-Bit-Fähigkeit, die das Plugin
 * als `UNSUPPORTED` führt.
 *
 * Die beiden Tabellen beantworten verschiedene Fragen — „kann das
 * FORMAT das tragen?" gegen „tut UNSER CODE das?". Für ein
 * Bedienelement zählt die zweite. Dieser Test hält für G64 fest, welche
 * Antwort gilt, damit die Auflösung nicht später still zurückgedreht
 * wird.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <string.h>

static int fehler;

#define ASSERT_MSG(bed, ...)                                               \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

static const uft_format_plugin_t *hole(const char *name)
{
    const uft_format_plugin_t *p = uft_get_format_plugin_by_name(name);
    if (!p) {
        printf("  FAIL Plugin \"%s\" nicht in der Registry\n", name);
        fehler++;
    }
    return p;
}

/* 1. Die Abfrage liest wirklich das Manifest — nicht irgendetwas. */
static void test_liest_das_manifest(void)
{
    const uft_format_plugin_t *hfe = hole("HFE");
    if (!hfe) return;

    /* Aus dem HFE-Manifest, nach MF-659 nachgemessen. */
    ASSERT_MSG(uft_plugin_feature_state(hfe, "HFE v1 (original)")
                   == UFT_FEATURE_SUPPORTED,
               "HFE: \"HFE v1 (original)\" sollte SUPPORTED sein");
    ASSERT_MSG(uft_plugin_feature_state(hfe, "HFE v2")
                   == UFT_FEATURE_UNSUPPORTED,
               "HFE: \"HFE v2\" sollte UNSUPPORTED sein (MF-659)");
    ASSERT_MSG(uft_plugin_feature_state(hfe, "Per-track bitrate")
                   == UFT_FEATURE_PARTIAL,
               "HFE: \"Per-track bitrate\" sollte PARTIAL sein (MF-659)");
}

/* 2. Unbekanntes wird verneint, nicht geraten. */
static void test_unbekanntes_wird_verneint(void)
{
    const uft_format_plugin_t *hfe = hole("HFE");
    if (!hfe) return;

    ASSERT_MSG(uft_plugin_feature_state(hfe, "Kaffee kochen")
                   == UFT_FEATURE_UNSUPPORTED,
               "eine nie deklarierte Fähigkeit muss UNSUPPORTED sein");
    ASSERT_MSG(!uft_plugin_declares_feature(hfe, "Kaffee kochen"),
               "declares_feature muss den Unterschied kennen");
    ASSERT_MSG(uft_plugin_declares_feature(hfe, "HFE v2"),
               "ausdrücklich verneint ist trotzdem deklariert");

    /* NULL darf nicht knallen — die Oberfläche fragt auch ohne Plugin. */
    ASSERT_MSG(uft_plugin_feature_state(NULL, "Flux")
                   == UFT_FEATURE_UNSUPPORTED,
               "NULL-Plugin muss UNSUPPORTED liefern, nicht abstürzen");
    ASSERT_MSG(uft_plugin_control_visibility(NULL, "Flux")
                   == UFT_CONTROL_HIDE,
               "NULL-Plugin muss HIDE liefern");
}

/* 3. PARTIAL wird gezeigt, nicht versteckt — und trägt seinen Hinweis. */
static void test_partial_zeigt_und_erklaert(void)
{
    const uft_format_plugin_t *hfe = hole("HFE");
    if (!hfe) return;

    ASSERT_MSG(uft_plugin_control_visibility(hfe, "Per-track bitrate")
                   == UFT_CONTROL_SHOW_LIMITED,
               "PARTIAL muss SHOW_LIMITED ergeben, nicht HIDE");

    const char *note = uft_plugin_feature_note(hfe, "Per-track bitrate");
    ASSERT_MSG(note && note[0],
               "PARTIAL ohne Hinweistext — die Oberfläche hätte nichts "
               "anzuzeigen");
}

/* 4. Der Kern: verschiedene Formate, verschiedene Bedienelemente.
 *
 * Ohne diesen Test wäre die ganze Stufe nicht belegt — eine Abfrage,
 * die überall dasselbe sagt, steuert nichts. */
static void test_formate_unterscheiden_sich(void)
{
    const uft_format_plugin_t *plugins[256];
    size_t n = uft_list_format_plugins(plugins, 256);
    ASSERT_MSG(n > 0, "keine Plugins registriert");
    if (n == 0) return;

    static const char *merkmale[] = { "Flux", "Weak Bits", "Write", "MultiRev" };
    int unterschiedlich = 0;

    for (size_t m = 0; m < sizeof(merkmale)/sizeof(merkmale[0]); m++) {
        int zeigen = 0, verstecken = 0;
        for (size_t i = 0; i < n; i++) {
            if (!plugins[i] || !plugins[i]->features) continue;
            switch (uft_plugin_control_visibility(plugins[i], merkmale[m])) {
            case UFT_CONTROL_HIDE:          verstecken++; break;
            default:                        zeigen++;     break;
            }
        }
        printf("  %-12s zeigen=%-4d verstecken=%d\n",
               merkmale[m], zeigen, verstecken);
        if (zeigen > 0 && verstecken > 0) unterschiedlich++;
    }

    ASSERT_MSG(unterschiedlich >= 2,
               "mindestens zwei Merkmale müssen die Formate trennen — "
               "sonst steuert das Manifest nichts (nur %d)", unterschiedlich);
}

/* 5. Der gemessene Widerspruch, festgehalten.
 *
 * `m_formatInfo["G64"]` in src/formattab.cpp führt supportsWeakBits =
 * true. Das Plugin sagt UNSUPPORTED. Für ein Bedienelement gilt das
 * Plugin — dieser Test hält das fest, damit die Auflösung nicht später
 * still zurückgedreht wird. Ändert sich das G64-Manifest mit Begründung,
 * gehört dieser Test mitgeändert; das ist der Zweck. */
static void test_g64_weak_bits_bleibt_verneint(void)
{
    const uft_format_plugin_t *g64 = hole("G64");
    if (!g64) return;

    ASSERT_MSG(uft_plugin_control_visibility(g64, "Weak Bits")
                   == UFT_CONTROL_HIDE,
               "G64 führt \"Weak Bits\" als UNSUPPORTED — das Element "
               "gehört ausgeblendet, egal was m_formatInfo behauptet");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }
    printf("Manifest steuert die Oberfläche (MF-660)\n\n");

    test_liest_das_manifest();
    test_unbekanntes_wird_verneint();
    test_partial_zeigt_und_erklaert();
    test_formate_unterscheiden_sich();
    test_g64_weak_bits_bleibt_verneint();

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
