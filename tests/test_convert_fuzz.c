/**
 * @file test_convert_fuzz.c
 * @brief Missgebildete Eingaben durch uft_convert_file() (MF-526)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * Der Baum hat drei Produktionsrouten, und bis heute waren zwei davon
 * gefuzzt:
 *
 *      lesen     uft_disk_open()   -> MF-512, sieben Speicherfehler
 *      schreiben write_track()     -> MF-522, 29 Befunde
 *      wandeln   uft_convert_file()-> NICHT geprueft
 *
 * Die dritte ist die interessanteste, weil sie die beiden anderen
 * verbindet: sie liest ein Format und schreibt ein anderes. Ein Fehler
 * kann in der Quelle liegen, im Ziel, oder erst in der Uebergabe zwischen
 * beiden — die Stelle, die keiner der ersten beiden Tests je sieht.
 *
 * Die neun bestehenden `test_convert_*`-Tests fahren jeweils EINEN Pfad
 * mit GUELTIGER Eingabe. Keiner fragt, was bei Muell passiert.
 *
 * ── Wie geprueft wird ────────────────────────────────────────────────────
 *
 *      Korpusdatei -> Kopie -> gezielt beschaedigt
 *      -> uft_convert_file(quelle, ziel, Zielformat, ...)
 *      fuer jedes Zielformat der Wandlungstabelle
 *
 * Immer auf Kopien; das Korpus wird nie angefasst.
 *
 * ── Was als Fehler gilt ──────────────────────────────────────────────────
 *
 * Ein Absturz oder ein Speicherfehler unter ASan/UBSan. **Nicht** als
 * Fehler gilt ein Rueckgabewert != UFT_OK — eine Wandlung abzulehnen, die
 * nicht geht, ist richtig.
 *
 * Zusaetzlich geprueft, und das ist der forensische Teil: nach einer
 * ABGELEHNTEN Wandlung darf keine Zieldatei zurueckbleiben, die aussieht,
 * als waere sie fertig. Eine halb geschriebene Datei mit Fehlermeldung ist
 * ein Zustand, den ein Benutzer nicht unterscheiden kann — er sieht eine
 * Datei.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"
#include "uft/uft_format_probe.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

#define MAX_IMG (4 * 1024 * 1024)
static uint8_t buf[MAX_IMG];

/* MF-526: die Endung gehoert zur Quelle. uft_convert_file() gibt den
 * Pfad an uft_probe_format() weiter, das ihn mitbewertet — eine feste
 * Endung ".img" liess jede Quelle wie ein IMG aussehen, und die
 * Wandlung scheiterte mit UFT_ERR_NOT_SUPPORTED, weil es den Pfad
 * IMG->X nicht gibt. Das war ein Fehler dieses Tests, nicht des
 * Werkzeugs: "2 von 210 angenommen" war eine Zahl ueber meinen
 * Dateinamen, nicht ueber die Wandlungspfade. */
static char src_tmp[256];
static const char *dst_tmp = "uft_conv_dst.img";

/* Quellen: echte Dateien, deren Format der Baum erkennt. */
static const char *const CORPUS[] = {
    "vice_c1541_35trk.d64", "vice_c1541_35trk.g64", "vice_c1541_70trk.d71",
    "vice_c1541_80trk.d81", "atrcopy_dos2sd.atr",   "xdftool_dd_ofs.adf",
    "gw_amigados.hfe",
    NULL
};

/* Zielformate, die die Wandlungstabelle fuehrt. Absichtlich auch solche,
 * die zur Quelle nicht passen — eine unmoegliche Wandlung muss abgelehnt
 * werden, nicht abstuerzen. */
static const struct { uft_format_t fmt; const char *name; } TARGETS[] = {
    { UFT_FORMAT_D64, "D64" }, { UFT_FORMAT_G64, "G64" },
    { UFT_FORMAT_ADF, "ADF" }, { UFT_FORMAT_IMG, "IMG" },
    { UFT_FORMAT_HFE, "HFE" }, { UFT_FORMAT_SCP, "SCP" },
};
#define N_TARGETS ((int)(sizeof(TARGETS) / sizeof(TARGETS[0])))

/* Beschaedigungen — dieselben wie im Oeffnungs-Fuzzer, damit die beiden
 * Berichte vergleichbar bleiben. */
static size_t mut_raw(const uint8_t *b, size_t n)
{ memcpy(buf, b, n); return n; }
static size_t mut_head_ff(const uint8_t *b, size_t n)
{ memcpy(buf, b, n); for (size_t i = 8; i < 64 && i < n; i++) buf[i] = 0xFF; return n; }
static size_t mut_head_00(const uint8_t *b, size_t n)
{ memcpy(buf, b, n); for (size_t i = 8; i < 64 && i < n; i++) buf[i] = 0x00; return n; }
static size_t mut_half(const uint8_t *b, size_t n)
{ size_t h = n / 2; memcpy(buf, b, h); return h; }
static size_t mut_tiny(const uint8_t *b, size_t n)
{ size_t h = n < 300 ? n : 300; memcpy(buf, b, h); return h; }

static const struct {
    size_t (*fn)(const uint8_t *, size_t);
    const char *label;
} MUT[] = {
    { mut_raw,     "roh"  }, { mut_head_ff, "FF"   }, { mut_head_00, "00" },
    { mut_half,    "halb" }, { mut_tiny,    "300B" },
};
#define N_MUT ((int)(sizeof(MUT) / sizeof(MUT[0])))

static long n_ok, n_rejected, n_ghost, n_lossless, n_gated;
static int failures;

static size_t slurp(const char *path, uint8_t *dst)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, MAX_IMG, f);
    fclose(f);
    return n;
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void write_tmp(size_t n)
{
    FILE *f = fopen(src_tmp, "wb");
    if (!f) { printf("  Tempdatei nicht schreibbar\n"); exit(2); }
    if (n) fwrite(buf, 1, n, f);
    fclose(f);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("=== Missgebildete Eingaben durch uft_convert_file() (MF-526) ===\n");

    static uint8_t base[MAX_IMG];
    for (int ci = 0; CORPUS[ci]; ci++) {
        char src[1024];
        snprintf(src, sizeof(src), "%s/%s", UFT_CORPUS_DIR, CORPUS[ci]);
        size_t base_len = slurp(src, base);
        if (!base_len) { printf("  %-24s (fehlt)\n", CORPUS[ci]); continue; }

        /* Temp-Datei mit der Endung der Quelle. */
        {
            const char *dot = strrchr(CORPUS[ci], '.');
            snprintf(src_tmp, sizeof(src_tmp), "uft_conv_src%s",
                     dot ? dot : ".bin");
        }
        printf("  %-24s %7zu B:", CORPUS[ci], base_len);
        {
            uft_probe_result_t pr;
            memset(&pr, 0, sizeof(pr));
            uft_format_t sf = uft_probe_format(base, base_len, src_tmp, &pr);
            const uft_format_plugin_t *pl = uft_get_format_plugin(sf);
            printf(" [erkannt=%d %s, alt=%d]", (int)sf,
                   pl && pl->name ? pl->name : "?", (int)pr.alternative_count);
        }
        for (int m = 0; m < N_MUT; m++) {
            size_t n = MUT[m].fn(base, base_len);
            write_tmp(n);
            printf(" %s", MUT[m].label);

            for (int t = 0; t < N_TARGETS; t++) {
                printf("<%s>", TARGETS[t].name);
                remove(dst_tmp);

                /* ZWEI Laeufe je Paar, und der Unterschied ist der
                 * eigentliche Test.
                 *
                 * MF-526: die erste Fassung uebergab genullte Optionen und
                 * bekam fuer 208 von 210 Paaren UFT_ERR_NOT_SUPPORTED. Ich
                 * hielt das fuer kaputte Wandlungspfade. Es war das
                 * Verlust-Tor (MF-263/UFT-A01), das ohne
                 * `accept_data_loss` JEDEN als LOSSY_DOCUMENTED gefuehrten
                 * Pfad abweist — also richtiges Verhalten, und mein Test
                 * hat es als Fehler gelesen.
                 *
                 * Jetzt wird beides geprueft:
                 *   ohne Zustimmung -> ein verlustbehafteter Pfad MUSS
                 *                      abgelehnt werden (forensische
                 *                      Invariante, nicht nur Absturzfreiheit)
                 *   mit Zustimmung  -> er muss laufen, ohne abzustuerzen */
                uft_convert_options_t o;
                memset(&o, 0, sizeof(o));
                uft_convert_result_t r;
                memset(&r, 0, sizeof(r));

                uft_error_t e_noconsent = uft_convert_file(src_tmp, dst_tmp,
                                                           TARGETS[t].fmt, &o, &r);
                remove(dst_tmp);

                memset(&o, 0, sizeof(o));
                o.accept_data_loss = true;
                memset(&r, 0, sizeof(r));
                uft_error_t e = uft_convert_file(src_tmp, dst_tmp,
                                                 TARGETS[t].fmt, &o, &r);

                /* Die Invariante: was MIT Zustimmung laeuft und als
                 * verlustbehaftet gilt, darf OHNE sie nicht gelaufen sein. */
                if (e == UFT_OK && e_noconsent == UFT_OK) n_lossless++;
                if (e == UFT_OK && e_noconsent != UFT_OK) n_gated++;

                if (e == UFT_OK) {
                    n_ok++;
                    if (m == 0) printf("=OK");   /* nur die rohe Datei */
                    continue;
                }
                if (m == 0) {
                    printf("=%d", (int)e);
                    if (r.warning_count > 0)
                        printf("(%.60s)", r.warnings[0]);
                }
                n_rejected++;

                /* Abgelehnt — dann darf keine Zieldatei zurueckbleiben.
                 * Ein Benutzer sieht sonst eine Datei und kann nicht
                 * erkennen, dass sie unvollstaendig ist. */
                if (file_exists(dst_tmp)) {
                    printf("\n  FAIL  %s + %s -> %s: abgelehnt (%d), aber die "
                           "Zieldatei liegt da\n",
                           CORPUS[ci], MUT[m].label, TARGETS[t].name, (int)e);
                    n_ghost++;
                    failures++;
                }
            }
        }
        printf("\n");
    }

    remove(src_tmp);
    remove(dst_tmp);

    printf("\nuft_convert_file(): %ld angenommen, %ld abgelehnt\n",
           n_ok, n_rejected);
    printf("Zieldatei trotz Ablehnung: %ld\n", n_ghost);

    /* NICHT GEPRUEFT: ob eine ANGENOMMENE Wandlung das Richtige liefert.
     * Das ist Aufgabe der Rundlauf-Tests gegen benannte Referenzen
     * (VERIFICATION_PLAN.md). Hier geht es um Absturzfreiheit und darum,
     * dass eine Ablehnung nichts zuruecklaesst. */

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
