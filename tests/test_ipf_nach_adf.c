/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_ipf_nach_adf.c
 * @brief IPF -> ADF is offered and places exactly what the IPF holds — and
 *        refuses an incomplete disk instead of filling it (P3-707, MF-1606).
 *
 * Before: the conversion table listed IPF -> ADF, but the round-trip matrix
 * had no entry and the dispatcher no converter; the preflight gate refused
 * the pair as UNTESTED at any price (MF-567 had removed the verdict for
 * exactly that reason). Reading IPF worked (the plugin decodes both
 * encoders since MF-1373); a user could not turn one into an ADF.
 *
 * The inputs are written by a THIRD PARTY from a deterministic UFT source:
 * disk-analyse (keirf/disk-utilities, Unlicense), CAPS encoder, from an ADF
 * in which every sector names itself (`UFT-K Ccc Hh Sss ` every 17 bytes,
 * tests/corpus_manifest/gen_ipf_corpus.py).
 *
 *  A  tests/corpus_free/disk_analyse_uftk_amiga.ipf — cylinders 0..3 only
 *     (CI). The converter must find all 88 sectors of those 4 cylinders, and
 *     REFUSE the ADF (an ADF cannot say "this sector is not there"; zeros
 *     would stand as data), writing no file.
 *  B  tests/corpus/disk_analyse_uftk_amiga_80.ipf plus its source
 *     tests/corpus/uftk_amiga.adf — all 80 cylinders (local only; skipped,
 *     by name, where absent). The ADF must equal the source byte for byte.
 *  C  without accept_data_loss the pair stays closed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static uint8_t *lies(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    *n = 0;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long g = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (g > 0) ? malloc((size_t)g) : NULL;
    if (!b || fread(b, 1, (size_t)g, f) != (size_t)g) { fclose(f); free(b); return NULL; }
    fclose(f);
    *n = (size_t)g;
    return b;
}

static int gibt_es(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static const char *erste_warnung(const uft_convert_result_t *r, const char *mit)
{
    for (int i = 0; i < r->warning_count; i++)
        if (strstr(r->warnings[i], mit)) return r->warnings[i];
    return NULL;
}

int main(void)
{
    char quelle[600], ziel[600], h[400];
    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(ziel, sizeof ziel, "%s/uft_mf1606.adf", t);
    printf("IPF -> ADF (MF-1606)\n");
    /* as main() does (MF-447): without it the registry is empty and no
     * format is detected at all */
    if (uft_register_all_formats() != UFT_OK) {
        printf("REGISTRY FEHLER\n");
        return 1;
    }

    /* C: closed without consent */
    snprintf(quelle, sizeof quelle, "%s/disk_analyse_uftk_amiga.ipf", UFT_CORPUS_DIR);
    {
        uft_convert_options_t ohne = uft_convert_default_options();
        ohne.accept_data_loss = false;
        uft_convert_result_t r;
        memset(&r, 0, sizeof r);
        remove(ziel);
        uft_error_t rc = uft_convert_file(quelle, ziel, UFT_FORMAT_ADF, &ohne, &r);
        snprintf(h, sizeof h, "rc=%d", (int)rc);
        pruefe("C: ohne accept_data_loss bleibt IPF -> ADF zu", rc != UFT_OK && !gibt_es(ziel), h);
        remove(ziel);
    }

    /* A: four cylinders — all 88 sectors found, ADF refused, no file */
    {
        uft_convert_options_t opt = uft_convert_default_options();
        opt.accept_data_loss = true;
        uft_convert_result_t r;
        memset(&r, 0, sizeof r);
        remove(ziel);
        uft_error_t rc = uft_convert_file(quelle, ziel, UFT_FORMAT_ADF, &opt, &r);
        const char *w = erste_warnung(&r, "of 1760");
        snprintf(h, sizeof h, "rc=%d, %d Sektoren, erste Warnung: %s", (int)rc,
                 r.sectors_converted, r.warning_count ? r.warnings[0] : "(keine)");
        pruefe("A: der Wandler laeuft (keine UNGEPRUEFT-Absage mehr)", w != NULL, h);
        pruefe("A: alle 88 Sektoren der Zylinder 0..3 gefunden, keiner verworfen",
               r.sectors_converted == 88 && r.sectors_failed == 0, h);
        pruefe("A: unvollstaendige Diskette abgelehnt, keine Datei geschrieben",
               rc != UFT_OK && !r.success && !gibt_es(ziel), h);
        remove(ziel);
    }

    /* B: all 80 cylinders (local) — byte-identical to the source ADF */
    {
        char voll[600], adf[600];
        snprintf(voll, sizeof voll, "%s/../corpus/disk_analyse_uftk_amiga_80.ipf", UFT_CORPUS_DIR);
        snprintf(adf, sizeof adf, "%s/../corpus/uftk_amiga.adf", UFT_CORPUS_DIR);
        if (!gibt_es(voll) || !gibt_es(adf)) {
            printf("  [--] B uebersprungen: %s oder die Quell-ADF fehlt (nur lokal)\n", voll);
        } else {
            uft_convert_options_t opt = uft_convert_default_options();
            opt.accept_data_loss = true;
            uft_convert_result_t r;
            memset(&r, 0, sizeof r);
            remove(ziel);
            uft_error_t rc = uft_convert_file(voll, ziel, UFT_FORMAT_ADF, &opt, &r);
            size_t na = 0, nq = 0;
            uint8_t *aus = lies(ziel, &na), *soll = lies(adf, &nq);
            size_t diff = 0;
            for (size_t i = 0; aus && soll && i < na && i < nq; i++)
                diff += (aus[i] != soll[i]);
            snprintf(h, sizeof h, "rc=%d, %zu Byte geschrieben, %zu Byte abweichend, %d Sektoren, erste Warnung: %s",
                     (int)rc, na, diff, r.sectors_converted,
                     r.warning_count ? r.warnings[0] : "(keine)");
            pruefe("B: 80 Zylinder -> ADF byteidentisch zur Quelle (901120 Byte)",
                   rc == UFT_OK && aus && soll && na == 901120 && nq == 901120 && diff == 0, h);
            free(aus);
            free(soll);
            remove(ziel);
        }
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
