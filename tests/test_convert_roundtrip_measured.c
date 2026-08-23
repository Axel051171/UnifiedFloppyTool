/**
 * @file test_convert_roundtrip_measured.c
 * @brief Was der vorhandene Korpus ueber die offenen Pfade hergibt (MF-533)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * Nach MF-526/527/532 stehen 31 der 44 Wandlungspfade auf UNGEPRUEFT und
 * werden vom Preflight-Tor abgewiesen. "31 offen" ist aber keine
 * Arbeitsanweisung — die Frage ist, woran jedes einzelne haengt.
 *
 * `scripts/audit_convert_backlog.py` teilt sie in drei Gruppen:
 *
 *     MESSBAR        9   Wandler UND Korpusdatei sind da
 *     KEIN KORPUS   12   Wandler da, Quellformat fehlt im Korpus
 *     KEIN WANDLER  10   der Dispatcher hat keinen Zweig
 *
 * Nur die erste Gruppe laesst sich ohne Beschaffung und ohne neuen Code
 * angehen. Zwei davon bilden **vollstaendige Rundlaeufe**, weil beide
 * Richtungen existieren und beide Formate im Korpus liegen:
 *
 *     D64 -> G64 -> D64
 *     G64 -> HFE -> G64
 *
 * ── Was dieser Test tut, und was nicht ───────────────────────────────────
 *
 * Er MISST. Er behauptet nicht, dass Bit-Identitaet herauskommen muss.
 *
 * Rot wird er, wenn eine Wandlung **abstuerzt** oder wenn eine Richtung
 * fehlschlaegt, die laut Dispatcher existiert — das waere ein Wandler, der
 * seinen eigenen Zweig nicht bedienen kann.
 *
 * Das Ergebnis der Byte-Vergleiche steht im Protokoll und ist die
 * Grundlage fuer einen Eintrag in `src/core/uft_roundtrip.c`. Die Regel
 * dort verlangt fuer LL einen Beweis der Bit-Identitaet und fuer LD einen
 * Beweis der Vollstaendigkeit der Verlustliste — dieser Test liefert die
 * Messung, nicht die Einstufung. Wer einstuft, muss beides gelesen haben.
 *
 * Gerufen werden die Wandler direkt, nicht ueber `uft_convert_file()` —
 * sonst pruefte der Test das Tor statt der Wandlung, und das Tor weist
 * genau diese Paare ja ab.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Die Wandler haben ZWEI Formen, und das ist keine Kosmetik:
 *
 *   6 Argumente, mit `src_path`:  d64_to_g64, g64_to_d64
 *   5 Argumente, ohne:            g64_to_hfe, hfe_to_g64
 *
 * MF-533: die erste Fassung dieses Tests deklarierte alle vier mit fuenf
 * Argumenten und rief damit die sechsstelligen falsch — `dst_path` bekam
 * den opts-Zeiger, `opts` den result-Zeiger, `result` Muell. Der Absturz
 * auf einer gueltigen D64 war meiner, nicht der des Werkzeugs.
 *
 * Die Deklarationen stehen jetzt woertlich so wie in
 * src/formats/uft_format_convert_internal.h. Wer einen internen
 * Einstiegspunkt direkt ruft, muss seine Signatur abschreiben, nicht
 * erraten — genau dafuer gibt es den Header. */
extern uft_error_t uftc_convert_d64_to_g64(const uint8_t *src_data, size_t src_size,
                                           const char *src_path, const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_g64_to_d64(const uint8_t *src_data, size_t src_size,
                                           const char *src_path, const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_g64_to_hfe(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_hfe_to_g64(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_g64_to_scp(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_scp_to_g64(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);
extern uft_error_t uftc_convert_sectors_to_hfe(const uint8_t *src_data, size_t src_size,
                                               const char *dst_path,
                                               uft_format_t src_format,
                                               const uft_convert_options_ext_t *opts,
                                               uft_convert_result_t *result);
extern uft_error_t uftc_convert_hfe_to_sectors(const uint8_t *src_data, size_t src_size,
                                               const char *src_path, const char *dst_path,
                                               uft_format_t dst_format,
                                               const uft_convert_options_ext_t *opts,
                                               uft_convert_result_t *result);

/* MF-534: EIN Adapter je Wandler, alle mit derselben Signatur.
 *
 * Die Wandler haben mindestens vier verschiedene Formen (5, 6 mit
 * src_path, 6 mit src_format, 7 mit beidem). Zweimal habe ich in MF-533
 * die falsche erwischt und einen Absturz erzeugt, der meiner war. Ein
 * Adapter je Wandler macht die Form zur Sache des Uebersetzers: passt sie
 * nicht, gibt es einen Fehler beim Bauen statt einen Absturz beim Laufen. */
#define ADAPT5(name) \
    static uft_error_t adapt_##name(const uint8_t *s, size_t n, \
                                    const char *sp, const char *dp, \
                                    const uft_convert_options_ext_t *o, \
                                    uft_convert_result_t *r) \
    { (void)sp; return name(s, n, dp, o, r); }

#define ADAPT6P(name) \
    static uft_error_t adapt_##name(const uint8_t *s, size_t n, \
                                    const char *sp, const char *dp, \
                                    const uft_convert_options_ext_t *o, \
                                    uft_convert_result_t *r) \
    { return name(s, n, sp, dp, o, r); }

ADAPT6P(uftc_convert_d64_to_g64)
ADAPT6P(uftc_convert_g64_to_d64)
ADAPT5(uftc_convert_g64_to_hfe)
ADAPT5(uftc_convert_hfe_to_g64)
ADAPT5(uftc_convert_g64_to_scp)
ADAPT5(uftc_convert_scp_to_g64)

static uft_error_t adapt_adf_to_hfe(const uint8_t *s, size_t n, const char *sp,
                                    const char *dp,
                                    const uft_convert_options_ext_t *o,
                                    uft_convert_result_t *r)
{ (void)sp; return uftc_convert_sectors_to_hfe(s, n, dp, UFT_FORMAT_ADF, o, r); }

static uft_error_t adapt_hfe_to_adf(const uint8_t *s, size_t n, const char *sp,
                                    const char *dp,
                                    const uft_convert_options_ext_t *o,
                                    uft_convert_result_t *r)
{ return uftc_convert_hfe_to_sectors(s, n, sp, dp, UFT_FORMAT_ADF, o, r); }

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

#define MAX_IMG (8 * 1024 * 1024)
static uint8_t a[MAX_IMG], b[MAX_IMG], c[MAX_IMG];

static int failures;

static size_t slurp(const char *path, uint8_t *dst)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, MAX_IMG, f);
    fclose(f);
    return n;
}

typedef uft_error_t (*conv_fn)(const uint8_t *, size_t, const char *,
                               const char *, const uft_convert_options_ext_t *,
                               uft_convert_result_t *);
typedef struct { const char *label; conv_fn fn; } conv_t;

/** Eine Richtung, mit Bericht. Gibt die Groesse des Ergebnisses zurueck
 *  (0 = fehlgeschlagen). */
static size_t one_way(conv_t cv,
                      const uint8_t *src, size_t src_len,
                      const char *src_path,
                      const char *out, uint8_t *dst_buf)
{
    const char *label = cv.label;
    /* MF-533: NICHT NULL uebergeben. Der Dispatcher baut
     * `uft_convert_options_ext_t ext_opts; memset(&ext_opts, 0, ...)` und
     * reicht `&ext_opts` weiter — er ruft die Wandler nie mit NULL. Wer
     * einen internen Einstiegspunkt direkt ruft, muss nachbauen, was der
     * Produktionsaufrufer uebergibt; sonst prueft er einen Pfad, den es
     * nicht gibt. Mit NULL stuerzte uftc_convert_d64_to_g64() ab — das
     * war mein Harness, nicht das Werkzeug. */
    uft_convert_options_ext_t opts;
    memset(&opts, 0, sizeof(opts));
    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));
    remove(out);
    /* Die sechsstellige Form bekommt `src_path` — so ruft der
     * Dispatcher sie auch (uft_format_convert_dispatch.c:190). */
    uft_error_t e = cv.fn(src, src_len, src_path, out, &opts, &r);
    if (e != UFT_OK) {
        printf("    FAIL %-14s -> %d%s%.70s\n", label, (int)e,
               r.warning_count > 0 ? "  " : "",
               r.warning_count > 0 ? r.warnings[0] : "");
        failures++;
        return 0;
    }
    size_t n = slurp(out, dst_buf);
    printf("    ok   %-14s -> %7zu Byte  (%d Spuren gewandelt, %d gescheitert)\n",
           label, n, r.tracks_converted, r.tracks_failed);
    return n;
}

static void roundtrip(const char *name, const char *corpus,
                      conv_t fwd, const char *mid_path,
                      conv_t back, const char *out_path)
{
    char src[1024];
    snprintf(src, sizeof(src), "%s/%s", UFT_CORPUS_DIR, corpus);
    size_t n0 = slurp(src, a);
    if (!n0) {
        printf("  %s: Korpusdatei fehlt (%s) — nicht messbar\n", name, src);
        return;
    }
    printf("  %s  (Quelle %s, %zu Byte)\n", name, corpus, n0);

    size_t n1 = one_way(fwd, a, n0, src, mid_path, b);
    if (!n1) { remove(mid_path); return; }

    size_t n2 = one_way(back, b, n1, mid_path, out_path, c);
    if (!n2) { remove(mid_path); remove(out_path); return; }

    if (n2 == n0 && memcmp(a, c, n0) == 0) {
        printf("    BITGLEICH — ein LOSSLESS-Eintrag waere hier belegt.\n");
    } else {
        size_t diff = 0, m = n2 < n0 ? n2 : n0;
        for (size_t i = 0; i < m; i++) if (a[i] != c[i]) diff++;
        printf("    nicht bitgleich: %zu -> %zu Byte (%+lld), "
               "%zu von %zu Bytes verschieden (%.1f %%)\n",
               n0, n2, (long long)n2 - (long long)n0, diff, m,
               m ? 100.0 * (double)diff / (double)m : 0.0);

        /* MF-535: WO die Abweichung liegt, nicht nur wieviel.
         *
         * "733 von 901120" sagt nichts darueber, ob ein Sektor ganz fehlt
         * oder tausend Sektoren je ein Bit verloren haben. Die
         * Verlustliste, die UFT_RT_LOSSY_DOCUMENTED verlangt, braucht
         * genau diese Unterscheidung — deshalb wird sie hier ausgezaehlt,
         * nach 512-Byte-Sektoren gruppiert.
         *
         * Nur bei kleinen Abweichungen (< 25 %): bei einem voellig
         * anderen Ergebnis ist die Sektor-Zuordnung bedeutungslos. */
        if (diff > 0 && diff < m / 4) {
            const size_t SEC = 512;
            size_t n_sec = m / SEC;
            size_t sec_touched = 0, sec_whole = 0, worst = 0, worst_at = 0;
            size_t first = (size_t)-1, last = 0;
            for (size_t s = 0; s < n_sec; s++) {
                size_t d = 0;
                for (size_t i = 0; i < SEC; i++)
                    if (a[s * SEC + i] != c[s * SEC + i]) d++;
                if (!d) continue;
                sec_touched++;
                if (d == SEC) sec_whole++;
                if (d > worst) { worst = d; worst_at = s; }
                if (first == (size_t)-1) first = s;
                last = s;
            }
            printf("      betroffen: %zu von %zu Sektoren a %zu Byte "
                   "(%zu davon vollstaendig)\n",
                   sec_touched, n_sec, SEC, sec_whole);
            if (sec_touched) {
                printf("      erster Sektor %zu, letzter %zu, "
                       "schlimmster %zu mit %zu/%zu Byte\n",
                       first, last, worst_at, worst, SEC);
                printf("      Spur %zu bis %zu (11 Sektoren je Spur)\n",
                       first / 11, last / 11);
            }
        }
    }
    remove(mid_path);
    remove(out_path);
}


/** Kreuzprobe: eine Wandlung gegen eine UNABHAENGIGE Referenz.
 *
 * MF-536: ein Rundlauf prueft eine Wandlung gegen sich selbst. Er kann
 * nicht unterscheiden, ob beide Richtungen richtig sind oder ob sich zwei
 * Fehler aufheben. Das Korpus enthaelt dieselbe Diskette zweimal —
 * vice_c1541_35trk.d64 und .g64, beide von VICE erzeugt. Damit laesst sich
 * G64 -> D64 gegen eine Referenz pruefen, die dieser Baum nicht gemacht
 * hat.
 *
 * Das ist die staerkere Messung: sie belegt nicht nur Umkehrbarkeit,
 * sondern Richtigkeit gegen eine benannte fremde Quelle (MF-498(a)). */
static void cross_check(const char *name, const char *src_corpus,
                        const char *ref_corpus, conv_t cv, const char *out_path)
{
    char src[1024], ref[1024];
    snprintf(src, sizeof(src), "%s/%s", UFT_CORPUS_DIR, src_corpus);
    snprintf(ref, sizeof(ref), "%s/%s", UFT_CORPUS_DIR, ref_corpus);

    size_t n_src = slurp(src, a);
    size_t n_ref = slurp(ref, c);
    if (!n_src || !n_ref) {
        printf("  %s: Korpusdatei fehlt — nicht messbar\n", name);
        return;
    }
    printf("  %s\n", name);
    printf("    Quelle    %s  %zu Byte\n", src_corpus, n_src);
    printf("    Referenz  %s  %zu Byte  (unabhaengig, von VICE)\n",
           ref_corpus, n_ref);

    size_t n_out = one_way(cv, a, n_src, src, out_path, b);
    if (!n_out) { remove(out_path); return; }

    if (n_out == n_ref && memcmp(b, c, n_ref) == 0) {
        printf("    BITGLEICH zur Referenz — die Wandlung ist gegen eine\n");
        printf("    fremde Quelle belegt, nicht nur gegen sich selbst.\n");
    } else {
        size_t diff = 0, m = n_out < n_ref ? n_out : n_ref;
        for (size_t i = 0; i < m; i++) if (b[i] != c[i]) diff++;
        printf("    Abweichung zur Referenz: %zu -> %zu Byte, "
               "%zu von %zu verschieden (%.2f %%)\n",
               n_ref, n_out, diff, m,
               m ? 100.0 * (double)diff / (double)m : 0.0);
        const size_t SEC = 256;             /* D64-Sektor */
        size_t n_sec = m / SEC, touched = 0, whole = 0;
        for (size_t s = 0; s < n_sec; s++) {
            size_t d = 0;
            for (size_t i = 0; i < SEC; i++)
                if (b[s * SEC + i] != c[s * SEC + i]) d++;
            if (d) { touched++; if (d == SEC) whole++; }
        }
        printf("      betroffen: %zu von %zu Sektoren a %zu Byte "
               "(%zu davon vollstaendig)\n", touched, n_sec, SEC, whole);
        if (touched && touched <= 12) {
            /* Bei wenigen Treffern gehoert die Liste hin, nicht die
             * Zahl. "3 von 683" ist ein Befund; "Sektor 357, 358, 359"
             * ist eine Verlustliste. */
            printf("      Sektoren:");
            for (size_t s = 0; s < n_sec; s++) {
                size_t d = 0;
                for (size_t i = 0; i < SEC; i++)
                    if (b[s * SEC + i] != c[s * SEC + i]) d++;
                if (d) printf(" %zu(%zu B)", s, d);
            }
            printf("\n");
        }
    }
    remove(out_path);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("Messbare Rundlaeufe aus dem vorhandenen Korpus (MF-533)\n\n");

    {
        conv_t fwd  = { "D64->G64", adapt_uftc_convert_d64_to_g64 };
        conv_t back = { "G64->D64", adapt_uftc_convert_g64_to_d64 };
        roundtrip("D64 -> G64 -> D64", "vice_c1541_35trk.d64",
                  fwd, "uft_rtm_mid.g64", back, "uft_rtm_out.d64");
    }

    printf("\n");

    {
        conv_t fwd  = { "G64->HFE", adapt_uftc_convert_g64_to_hfe };
        conv_t back = { "HFE->G64", adapt_uftc_convert_hfe_to_g64 };
        roundtrip("G64 -> HFE -> G64", "vice_c1541_35trk.g64",
                  fwd, "uft_rtm_mid.hfe", back, "uft_rtm_out.g64");
    }

    printf("\n");
    {
        conv_t fwd  = { "G64->D64", adapt_uftc_convert_g64_to_d64 };
        conv_t back = { "D64->G64", adapt_uftc_convert_d64_to_g64 };
        roundtrip("G64 -> D64 -> G64", "vice_c1541_35trk.g64",
                  fwd, "uft_rtm_mid2.d64", back, "uft_rtm_out2.g64");
    }

    printf("\n");
    {
        conv_t fwd  = { "G64->SCP", adapt_uftc_convert_g64_to_scp };
        conv_t back = { "SCP->G64", adapt_uftc_convert_scp_to_g64 };
        roundtrip("G64 -> SCP -> G64", "vice_c1541_35trk.g64",
                  fwd, "uft_rtm_mid3.scp", back, "uft_rtm_out3.g64");
    }

    printf("\n");
    {
        conv_t fwd  = { "ADF->HFE", adapt_adf_to_hfe };
        conv_t back = { "HFE->ADF", adapt_hfe_to_adf };
        roundtrip("ADF -> HFE -> ADF", "xdftool_dd_ofs.adf",
                  fwd, "uft_rtm_mid4.hfe", back, "uft_rtm_out4.adf");
    }

    /* NICHT GEPRUEFT: D64 -> SCP. Der Dispatcher fuehrt das als
     * ZWEISTUFIGE Kette ueber G64 aus (uft_format_convert_dispatch.c:259),
     * es gibt keinen eigenen Wandler. Die Kette hier nachzubauen hiesse,
     * den Produktionspfad nachzubauen statt ihn zu benutzen — genau der
     * Fehler, der in diesem Baum schon zweimal falsche Zahlen erzeugt hat
     * (MF-494, MF-497).
     *
     * Ebenfalls nicht geprueft: ADF -> IMG. Das Paar steht in der
     * Wandlungstabelle, hat aber KEINEN Wandler — der Zweig
     * `src_class == SECTOR && dst_class == SECTOR` gibt ausdruecklich
     * UFT_ERR_NOT_IMPLEMENTED zurueck (UFT-A08). Ein Rundlauf waere eine
     * Messung an etwas, das es nicht gibt. */

    printf("\n");
    {
        conv_t g2d = { "G64->D64", adapt_uftc_convert_g64_to_d64 };
        cross_check("G64 -> D64, gegen die Korpus-D64 derselben Diskette",
                    "vice_c1541_35trk.g64", "vice_c1541_35trk.d64",
                    g2d, "uft_xc_out.d64");
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
