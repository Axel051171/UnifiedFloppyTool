/**
 * @file test_hfe_kodierung_nicht_erfunden.c
 * @brief SCP -> HFE and G64 -> HFE write the encoding they know into the
 *        HFE header — not a fixed ISO/IBM MFM (MF-1440).
 *
 * The HFE header's `track_encoding` said HFE_ENC_ISOIBM_MFM (0x00) as a
 * fixed value in three converters:
 *
 *   - SCP -> HFE PLL-decodes flux into cells and never determines the
 *     encoding. Proof input: tests/corpus_free/gw_fm_acorn_3trk.scp, an
 *     FM capture measured three times (corpus manifest, MF-1142 —
 *     greaseweazle's own decoder reads it back as "IBM FM (10/10
 *     sectors)"). Its HFE said MFM. The honest value is HFE_ENC_UNKNOWN
 *     (0xFF): greaseweazle (26690f8) writes it into the HFE it produces
 *     from a sector image (measured 2026-09-27 on a D81: byte 11 = 0xFF),
 *     and UFT's own HFE reader maps 0xFF to UFT_ENC_UNKNOWN.
 *   - G64 -> HFE: the content IS known, Commodore GCR, yet the header said
 *     MFM ("Raw bitstream"). The HFE format's author defines a value for
 *     it: HxC libhxcfe.h C64_GCR_ENCODING = 0x12. Proof input:
 *     tests/corpus_free/vice_c1541_35trk.g64, written by VICE.
 *   - KryoFlux -> HFE got the same change as SCP but is not checked here:
 *     it cannot run, uft_kfx_read_stream() is a stub that always returns
 *     -1 (measured on both hxcfe streams and the fluxfox stream; P3-640).
 *
 * Found while checking a report from the local -37 session (a write-side
 * mapping in uft_hfe.c defaults unknown to MFM). That mapping,
 * uft_to_hfe_encoding(), has NO caller; these converters are the
 * production paths that actually write the field.
 */
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_scp_to_hfe(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const void *opts, void *result);
extern uft_error_t uftc_convert_g64_to_hfe(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const void *opts, void *result);

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  ROT  %s - %s\n", was, detail ? detail : ""); }
}

static uint8_t *lies(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    long g;
    uint8_t *b;
    *n = 0;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); g = ftell(f); fseek(f, 0, SEEK_SET);
    if (g <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)g);
    if (!b || fread(b, 1, (size_t)g, f) != (size_t)g) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)g; return b;
}

typedef uft_error_t (*wandler_t)(const uint8_t *, size_t, const char *,
                                 const void *, void *);

static void pruefe_wandler(const char *was, wandler_t w, const char *eingabe,
                           uint8_t soll)
{
    char quelle[600], ziel[600], det[200];
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(quelle, sizeof quelle, "%s/%s", UFT_CORPUS_DIR, eingabe);
    snprintf(ziel, sizeof ziel, "%s/uft_mf1440_%02x.hfe", d, (unsigned)soll);

    size_t n = 0, hn = 0;
    uint8_t *src = lies(quelle, &n);
    if (!src) {
        snprintf(det, sizeof det, "%s nicht lesbar", quelle);
        pruefe(was, 0, det);
        return;
    }
    uft_convert_result_t res;
    memset(&res, 0, sizeof res);
    const uft_error_t rc = w(src, n, ziel, NULL, &res);
    uint8_t *hfe = (rc == UFT_OK) ? lies(ziel, &hn) : NULL;
    remove(ziel);
    free(src);
    snprintf(det, sizeof det, "rc=%d, %u Byte HFE, track_encoding = 0x%02X",
             (int)rc, (unsigned)hn, (hfe && hn > 11) ? hfe[11] : 0u);
    pruefe(was, hfe && hn > 11 && memcmp(hfe, "HXCPICFE", 8) == 0
                && hfe[11] == soll, det);
    free(hfe);
}

int main(void)
{
    printf("HFE-Kopf: Kodierung nicht erfunden (MF-1440)\n");
    pruefe_wandler("SCP (FM-Aufnahme) -> HFE: Kopf sagt \"unbekannt\" (0xFF), "
                   "nicht ISO-MFM",
                   (wandler_t)uftc_convert_scp_to_hfe,
                   "gw_fm_acorn_3trk.scp", 0xFF);
    pruefe_wandler("G64 (VICE) -> HFE: Kopf sagt Commodore-GCR (0x12), nicht "
                   "ISO-MFM",
                   (wandler_t)uftc_convert_g64_to_hfe,
                   "vice_c1541_35trk.g64", 0x12);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
