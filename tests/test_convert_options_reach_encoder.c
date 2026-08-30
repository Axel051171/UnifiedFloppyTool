/**
 * @file test_convert_options_reach_encoder.c
 * @brief OPT-2: kommen die Wandlungs-Optionen beim Kodierer an? (MF-695)
 *
 * ── Warum dieser Test existiert ─────────────────────────────────────────
 *
 * MF-693 hat behauptet, die beiden Wandlungsketten in
 * `uft_format_convert_dispatch.c` divergierten, weil die Speicher-
 * Abkuerzung `d64_to_g64(d64, &g64, NULL, NULL)` ruft und `opts` nicht
 * anfasst. MF-694 hat das nachgemessen und berichtigt: `d64_to_g64`
 * faellt bei `NULL` auf `convert_get_defaults()` zurueck — und genau die
 * benutzt auch `uftc_convert_d64_to_g64()`
 * (`src/formats/uft_format_convert_sector.c:124-125`). Beide Ketten
 * erzeugen dieselben Bytes.
 *
 * Darunter lag der groessere Befund: **`convert_options_t` wird nirgends
 * aus `uft_convert_options_ext_t` befuellt.** Der Kodierer ist
 * steuerbar (`extended_tracks`, `include_halftracks`, `gap_fill`,
 * `sync_length`, `generate_errors`), aber es fuehrt kein Weg von der
 * oeffentlichen Wandlungs-API dorthin.
 *
 * ── Warum der urspruenglich geplante Rotbeweis nicht taugt ─────────────
 *
 * Geplant war: "D64->G64 mit gesetztem `opts`-Nudge muss sich vom
 * Nudge-losen Lauf unterscheiden". Der kann nicht feuern — es gibt kein
 * Feld, das den Kodierer erreicht, auf KEINEM der beiden Wege. Ein
 * Beweis, der nicht feuern kann, haette die spaetere Vereinigung als
 * "belegt" ausgewiesen, ohne etwas gesehen zu haben. Das ist die
 * Fehlerklasse, die dieser Baum mehrfach bezahlt hat.
 *
 * ── Der Beweis, der feuert: zwei Haelften ───────────────────────────────
 *
 *   A) Der Kodierer IST steuerbar. `d64_to_g64()` unmittelbar mit
 *      veraenderten `convert_options_t` gerufen MUSS andere Bytes
 *      liefern als mit den Vorgaben. Schlaegt das fehl, ist die
 *      Messanordnung kaputt und nicht der Baum — dann sagt dieser Test
 *      es, statt eine Null zu melden.
 *
 *   B) Die oeffentliche API steuert ihn NICHT. `uft_convert_memory()`
 *      mit JEDEM einzeln umgelegten Feld aus `uft_convert_options_ext_t`
 *      liefert byteidentische Ausgaben.
 *
 * A gruen und B "nie verschieden" zusammen belegen OPT-2: der Kodierer
 * kann gesteuert werden, der Aufrufer kann es nicht.
 *
 * ── Stand nach MF-695 ──────────────────────────────────────────────────
 *
 * Die Uebersetzung `uft_convert_options_ext_t -> convert_options_t` ist
 * gebaut (`uftc_c64_encoder_options()`), und `target_geometry.cylinders`
 * erreicht `extended_tracks`. Gemessen: **1 von 10** Feldern veraendert
 * die Ausgabe.
 *
 * Die uebrigen neun tun es zu Recht nicht — sie steuern Fluss-Synthese
 * und Dekodierung, nicht die GCR-Kodierung eines Sektorabbilds. Zwei
 * Kodierer-Parameter haben dagegen ueberhaupt keinen Leser
 * (`align_tracks`, `sync_length`, 0 Treffer in `uft_d64_g64.c`); sie
 * bekommen keine erfundene Zuordnung, das waere eine Einstellung, die
 * anzukommen scheint und nichts tut.
 *
 * Der Test schuetzt den Stand in BEIDE Richtungen: faellt die Zahl auf
 * 0, ist die Uebersetzung kaputt; steigt sie, ist eine dazugekommen und
 * gehoert benannt.
 *
 * ── Quelle der Eingabe ──────────────────────────────────────────────────
 *
 * `tests/corpus_free/vice_c1541_35trk.d64`, erzeugt von VICE 3.10 c1541
 * (`tests/corpus_manifest/manifest.json`). Ein echtes Abbild fremder
 * Hand, kein synthetisches — sonst pruefte der Test den eigenen
 * Erzeuger mit.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_types.h"
#include "uft/formats/c64/uft_d64_g64.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

/* MF-695: die Luecke ist teilweise geschlossen, also steht das hier auf
 * 0 und der Test VERLANGT, dass mindestens ein Feld ankommt.
 *
 * Der Uebergang ist gelaufen und war der Beweis: mit `ERWARTE_LUECKE 1`
 * und ohne `target_geometry` meldete der Test 0 von 9 und war gruen;
 * nach dem Einbau der Uebersetzung meldete er 1 von 10 und wurde ROT —
 * er hat seine eigene Zustandsaenderung gefangen, statt sie
 * durchzuwinken. Genau dafuer ist die Schranke da. */
#define ERWARTE_LUECKE 0

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long g = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (g <= 0) { fclose(f); return NULL; }
    uint8_t *p = (uint8_t *)malloc((size_t)g);
    if (p && fread(p, 1, (size_t)g, f) != (size_t)g) { free(p); p = NULL; }
    fclose(f);
    if (p) *n = (size_t)g;
    return p;
}

/* ── Haelfte A: ist der Kodierer ueberhaupt steuerbar? ──────────────── */
static bool kodierer_reagiert(const uint8_t *d64_bytes, size_t d64_len)
{
    d64_image_t *d64 = NULL;
    if (d64_load_buffer(d64_bytes, d64_len, &d64) != 0 || !d64)
        return false;

    convert_options_t vorgabe;
    convert_get_defaults(&vorgabe);

    uint8_t *a = NULL, *b = NULL;
    size_t na = 0, nb = 0;
    g64_image_t *g = NULL;

    if (d64_to_g64(d64, &g, &vorgabe, NULL) == 0 && g) {
        g64_save_buffer(g, &a, &na);
        g64_free(g);
        g = NULL;
    }

    /* Ein Kodierer-Parameter, der die Bytes zwingend aendert: die
     * Fuellung der Luecken. Sie steht in jeder Spur. */
    convert_options_t anders = vorgabe;
    anders.gap_fill = (uint8_t)(vorgabe.gap_fill ^ 0xFF);

    if (d64_to_g64(d64, &g, &anders, NULL) == 0 && g) {
        g64_save_buffer(g, &b, &nb);
        g64_free(g);
    }
    d64_free(d64);

    bool verschieden = (a && b) && (na != nb || memcmp(a, b, na) != 0);
    printf("  A: Kodierer mit gap_fill 0x%02X vs 0x%02X -> %s "
           "(%zu / %zu Byte)\n",
           vorgabe.gap_fill, anders.gap_fill,
           verschieden ? "VERSCHIEDEN" : "identisch", na, nb);
    free(a);
    free(b);
    return verschieden;
}

/* ── Haelfte B: erreicht IRGENDEIN Feld der oeffentlichen API ihn? ──── */
struct feld { const char *name; void (*setze)(uft_convert_options_ext_t *); };

static void s_preserve_errors(uft_convert_options_ext_t *o)
{ o->preserve_errors = true; }
static void s_preserve_weak(uft_convert_options_ext_t *o)
{ o->preserve_weak_bits = true; }
static void s_verify_after(uft_convert_options_ext_t *o)
{ o->verify_after = true; }
static void s_synth_cell(uft_convert_options_ext_t *o)
{ o->synthetic_cell_time_us = 3.25; }
static void s_synth_jitter(uft_convert_options_ext_t *o)
{ o->synthetic_jitter_percent = 7.0; }
static void s_synth_revs(uft_convert_options_ext_t *o)
{ o->synthetic_revolutions = 5; }
static void s_retries(uft_convert_options_ext_t *o)
{ o->decode_retries = 9; }
static void s_multirev(uft_convert_options_ext_t *o)
{ o->use_multiple_revs = true; }
static void s_interpolate(uft_convert_options_ext_t *o)
{ o->interpolate_errors = true; }
/* MF-695: die 40/42-Spur-Wahl. `extended_tracks` im Kodierer erzeugt 42
 * statt der Abbild-Spurzahl (`uft_d64_g64.c:975`); 35 ist die
 * 1541-Norm, alles darueber ist genau dieser Fall. */
static void s_geometrie(uft_convert_options_ext_t *o)
{ o->target_geometry.cylinders = 42; }

static const struct feld FELDER[] = {
    { "preserve_errors",          s_preserve_errors },
    { "preserve_weak_bits",       s_preserve_weak },
    { "verify_after",             s_verify_after },
    { "synthetic_cell_time_us",   s_synth_cell },
    { "synthetic_jitter_percent", s_synth_jitter },
    { "synthetic_revolutions",    s_synth_revs },
    { "decode_retries",           s_retries },
    { "use_multiple_revs",        s_multirev },
    { "interpolate_errors",       s_interpolate },
    { "target_geometry.cylinders", s_geometrie },
};

static uint8_t *wandle(const uint8_t *src, size_t n,
                       const struct feld *f, size_t *aus_n)
{
    uft_convert_options_ext_t o;
    memset(&o, 0, sizeof(o));
    if (f) f->setze(&o);

    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));
    uint8_t *dst = NULL;
    *aus_n = 0;
    uft_error_t e = uft_convert_memory(src, n, UFT_FORMAT_D64,
                                       &dst, aus_n, UFT_FORMAT_G64,
                                       &o, &r);
    if (e != UFT_OK) { free(dst); return NULL; }
    return dst;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("OPT-2: erreichen die Wandlungs-Optionen den Kodierer? "
           "(MF-695)\n\n");

    char pfad[1024];
    snprintf(pfad, sizeof(pfad), "%s/vice_c1541_35trk.d64", UFT_CORPUS_DIR);
    size_t n = 0;
    uint8_t *d64 = lies(pfad, &n);
    if (!d64) {
        printf("  FAIL Korpus-Abbild fehlt: %s\n", pfad);
        return 1;
    }
    printf("  Eingabe: %s (%zu Byte, VICE 3.10 c1541)\n\n", pfad, n);

    /* ── A ─────────────────────────────────────────────────────────── */
    bool steuerbar = kodierer_reagiert(d64, n);
    PRUEFE(steuerbar,
           "der Kodierer reagiert nicht einmal auf seine EIGENEN Optionen. "
           "Dann misst Haelfte B nichts, und dieser Test darf keine Null "
           "melden — die Messanordnung ist kaputt, nicht der Baum");
    if (!steuerbar) { free(d64); return 1; }

    /* ── B ─────────────────────────────────────────────────────────── */
    printf("\n");
    size_t n0 = 0;
    uint8_t *g0 = wandle(d64, n, NULL, &n0);
    if (!g0) {
        printf("  FAIL D64->G64 ueber uft_convert_memory() schlug fehl\n");
        free(d64);
        return 1;
    }
    printf("  B: Vergleichslauf ohne Optionen: %zu Byte\n", n0);

    int wirksam = 0;
    for (size_t i = 0; i < sizeof(FELDER) / sizeof(FELDER[0]); i++) {
        size_t ni = 0;
        uint8_t *gi = wandle(d64, n, &FELDER[i], &ni);
        bool anders = gi && (ni != n0 || memcmp(gi, g0, n0) != 0);
        if (anders) wirksam++;
        printf("     %-26s -> %s\n", FELDER[i].name,
               gi ? (anders ? "andere Bytes" : "byteidentisch")
                  : "Wandlung fehlgeschlagen");
        free(gi);
    }
    free(g0);
    free(d64);

    printf("\n  %d von %zu Feldern haben die Ausgabe veraendert.\n",
           wirksam, sizeof(FELDER) / sizeof(FELDER[0]));

#if ERWARTE_LUECKE
    PRUEFE(wirksam == 0,
           "%d Feld(er) wirken jetzt — die Luecke aus OPT-2 ist teilweise "
           "geschlossen. Das ist gut, aber `ERWARTE_LUECKE` steht noch auf "
           "1: den Ist-Stand hier nachziehen, damit die Zahl nicht still "
           "driftet", wirksam);
    if (wirksam == 0)
        printf("  ok   OPT-2 bestaetigt: der Kodierer ist steuerbar, die "
               "oeffentliche API steuert ihn NICHT.\n"
               "       `convert_options_t` wird nirgends aus "
               "`uft_convert_options_ext_t` befuellt —\n"
               "       `preserve_errors` erreicht die Fehlerkarte nicht, "
               "die 40/42-Spur-Wahl nicht `extended_tracks`.\n");
#else
    PRUEFE(wirksam > 0,
           "kein einziges Feld erreicht den Kodierer, obwohl "
           "ERWARTE_LUECKE auf 0 steht — die Uebersetzung "
           "uft_convert_options_ext_t -> convert_options_t fehlt oder "
           "wirkt nicht");
#endif

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
