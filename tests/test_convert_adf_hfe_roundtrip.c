/**
 * @file test_convert_adf_hfe_roundtrip.c
 * @brief ADF -> HFE gibt es wieder, und der Beleg ist eine echte Diskette
 *        (MF-1081, hebt MF-539 auf)
 *
 * ── Was vorher da war ───────────────────────────────────────────────────
 *
 * `uftc_convert_sectors_to_hfe()` schrieb fuer eine ADF eine
 * IBM-System-34-Spur mit einem AMIGA_MFM-Kopf darueber. MF-539 hat das
 * gemessen, gegen die echte Aufnahme `tests/corpus_free/gw_amigados.hfe`:
 *
 *                          echte HFE        UFTs Ausgabe
 *      Sync 0x4489              22                     0
 *      haeufigstes Byte    0x55 (12498)      0x4E (6712)
 *      rohe Nullbytes        0 von 12792   5969 von 12800
 *
 * Seit MF-539 wurde die Wandlung deshalb ABGELEHNT — richtig, denn eine
 * plausible unlesbare Datei ist schlimmer als ein ehrlicher Fehler. Was
 * fehlte, war ein AmigaDOS-Encoder.
 *
 * ── Was jetzt da ist, und wie es abgenommen wurde ───────────────────────
 *
 * `src/core/uft_amiga_mfm_encoder.c` ist die exakte Umkehrung des
 * baumeigenen, bereits abgenommenen Dekoders `decode_amiga_sector()`.
 * **Abgenommen wurde er nicht gegen sich selbst**, sondern gegen
 * dieselbe echte Aufnahme, an der MF-539 gemessen hat: deren Spur 0/0
 * dekodiert, mit dem neuen Encoder neu kodiert — und **11 von 11
 * Sektoren stehen byteidentisch in der Originalspur**, je 1084 Byte
 * samt Sync, Info-Long, Label, beiden Pruefsummen und jedem Taktbit.
 *
 * **Das hat einen Widerspruch zwischen zwei Referenzen entschieden.**
 * Keir Frasers `libdisk` (Public Domain, `disk-utilities`) nennt die
 * Feldreihenfolge `info_even, info_odd`; UFTs Dekoder nennt die erste
 * Haelfte `odd`. Beide beschreiben dieselben Bits. Entschieden wurde das
 * am Objekt, nicht durch Auswahl zwischen zwei Beschreibungen.
 *
 * ── Warum die Zellregel eigens geprueft wird ────────────────────────────
 *
 * Der Dekoder maskiert nur die `0x55`-Positionen; die **Taktbits sieht
 * er nicht an**. Ein Rundlauf durch die eigene Umkehrung koennte also
 * byteidentisch aufgehen, waehrend die Spur auf einem echten Laufwerk
 * unlesbar ist — woertlich die Falle aus MF-1079, wo jede Spurlaenge auf
 * das Bit stimmte und trotzdem 587 unmoegliche Zellpaare darin standen.
 * Zusage 3 misst sie deshalb getrennt.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **HD (22 Sektoren).** Der Zweig ist da und die Geometrie wird
 *   abgeleitet, aber im Baum liegt keine HD-ADF; ungeprueft heisst hier
 *   ungeprueft.
 * * **Sektor-Label.** Eine ADF traegt keine; sie werden als Nullen
 *   geschrieben. Steht so im Kopf des Encoders.
 * * **Ob ein echtes Laufwerk die Datei liest.** Das braucht Hardware
 *   (MF-310). Belegt ist, dass die Zellen denen einer echten Aufnahme
 *   entsprechen.
 */
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"
#include "uft/uft_amiga_mfm_encoder.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern uft_error_t uftc_convert_sectors_to_hfe(
        const uint8_t *src_data, size_t src_size, const char *dst_path,
        uft_format_t src_format, const uft_convert_options_ext_t *opts,
        uft_convert_result_t *result);
extern uft_error_t uftc_convert_hfe_to_sectors(
        const uint8_t *src_data, size_t src_size, const char *src_path,
        const char *dst_path, uft_format_t dst_format,
        const uft_convert_options_ext_t *opts,
        uft_convert_result_t *result);

#define CYLS   80
#define HEADS   2
#define SPT    11
#define SECSZ 512
#define ADFSZ ((size_t)CYLS * HEADS * SPT * SECSZ)   /* 901 120 */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static uint8_t *lies(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    long g;
    uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); g = ftell(f); fseek(f, 0, SEEK_SET);
    if (g <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)g);
    if (!b || fread(b, 1, (size_t)g, f) != (size_t)g) {
        fclose(f); free(b); return NULL;
    }
    fclose(f); *n = (size_t)g; return b;
}

static int bit_von(const uint8_t *p, size_t i)
{
    return (p[i >> 3] >> (7 - (i & 7u))) & 1;
}

int main(void)
{
    char det[300];
    const char *hfe_pfad = "uft_mf1081_probe.hfe";
    const char *adf_pfad = "uft_mf1081_probe.adf";
    uint8_t *adf = (uint8_t *)calloc(1, ADFSZ);
    uint8_t *hfedat = NULL, *zurueck = NULL;
    size_t hn = 0, zn = 0, i;
    uft_convert_options_ext_t opts;
    uft_convert_result_t res;
    uft_error_t rc;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("ADF -> HFE ist zurueck - MF-1081 (hebt MF-539 auf)\n");
    printf("==================================================\n");
    if (!adf) { printf("SKIP: kein Speicher.\n"); return 77; }

    /* Selbstbenennende ADF: jeder Sektor traegt seine eigene Nummer.
     * Damit sagt ein Leseergebnis nicht nur, DASS etwas zurueckkam,
     * sondern ob die RICHTIGE Stelle getroffen wurde (MF-1020). */
    for (i = 0; i < ADFSZ / SECSZ; i++) {
        char m[40];
        int l = snprintf(m, sizeof m, "UFT-AMIGA %06u", (unsigned)i);
        memcpy(adf + i * SECSZ, m, (size_t)l);
        memset(adf + i * SECSZ + l, (int)(i & 0xFFu), SECSZ - (size_t)l);
    }

    /* ── 1. Die Wandlung wird nicht mehr abgelehnt ─────────────────── */
    memset(&opts, 0, sizeof opts);
    memset(&res, 0, sizeof res);
    rc = uftc_convert_sectors_to_hfe(adf, ADFSZ, hfe_pfad, UFT_FORMAT_ADF,
                                     &opts, &res);
    snprintf(det, sizeof det, "rc=%d", (int)rc);
    pruefe("ADF -> HFE laeuft durch - bis MF-1081 kam hier "
           "UFT_ERR_NOT_IMPLEMENTED, weil dem Baum der AmigaDOS-Encoder "
           "fehlte (MF-539)", rc == UFT_OK, det);
    if (rc != UFT_OK) {
        free(adf);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    hfedat = lies(hfe_pfad, &hn);
    if (!hfedat) {
        pruefe("die HFE ist lesbar", 0, "Datei fehlt");
        free(adf);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* ── 2. Der Rundlauf ist byteidentisch ─────────────────────────── */
    memset(&res, 0, sizeof res);
    rc = uftc_convert_hfe_to_sectors(hfedat, hn, hfe_pfad, adf_pfad,
                                     UFT_FORMAT_ADF, &opts, &res);
    zurueck = (rc == UFT_OK) ? lies(adf_pfad, &zn) : NULL;
    {
        size_t abweichend = 0;
        if (zurueck && zn == ADFSZ)
            for (i = 0; i < ADFSZ; i++)
                if (adf[i] != zurueck[i]) abweichend++;
        snprintf(det, sizeof det, "rc=%d, %u Byte zurueck (Quelle %u), "
                 "%u abweichend", (int)rc, (unsigned)zn, (unsigned)ADFSZ,
                 (unsigned)abweichend);
        pruefe("ADF -> HFE -> ADF ist BYTEIDENTISCH ueber alle 1760 "
               "Sektoren - der Weg hinein und der Weg heraus sind zwei "
               "verschiedene Umsetzungen",
               rc == UFT_OK && zurueck && zn == ADFSZ && abweichend == 0,
               det);
    }

    /* ── 3. Die Zellregel, die der Dekoder NICHT prueft ────────────── */
    {
        uint8_t *spur = (uint8_t *)malloc(SPT * UFT_AMIGA_SECTOR_MFM_BYTES);
        size_t n = spur ? uft_amiga_mfm_encode_track(
                              adf, SPT, 0u, NULL, spur,
                              SPT * UFT_AMIGA_SECTOR_MFM_BYTES) : 0;
        size_t paare = 0, maxnull = 0, lauf = 0, bits = n * 8;
        int vor = 0;
        for (i = 0; i < bits; i++) {
            const int v = bit_von(spur, i);
            if (v && vor) paare++;
            if (v) { if (lauf > maxnull) maxnull = lauf; lauf = 0; }
            else lauf++;
            vor = v;
        }
        if (lauf > maxnull) maxnull = lauf;
        snprintf(det, sizeof det, "%u Byte, %u Paare, Nulllauf %u",
                 (unsigned)n, (unsigned)paare, (unsigned)maxnull);
        pruefe("die kodierte Spur haelt die MFM-Zellregel: keine zwei "
               "benachbarten 1-Zellen, hoechstens drei Nullen am Stueck - "
               "der Dekoder sieht die Taktbits gar nicht an (MF-1079)",
               n == (size_t)SPT * UFT_AMIGA_SECTOR_MFM_BYTES
               && paare == 0 && maxnull == 3, det);
        free(spur);
    }

    /* ── 4. Die Signatur der echten Aufnahme ───────────────────────── */
    {
        uint8_t *spur = (uint8_t *)malloc(SPT * UFT_AMIGA_SECTOR_MFM_BYTES);
        size_t n = spur ? uft_amiga_mfm_encode_track(
                              adf, SPT, 0u, NULL, spur,
                              SPT * UFT_AMIGA_SECTOR_MFM_BYTES) : 0;
        size_t sync = 0, nullbytes = 0;
        for (i = 0; i + 1 < n; i++)
            if (spur[i] == 0x44u && spur[i + 1] == 0x89u) sync++;
        for (i = 0; i < n; i++) if (spur[i] == 0x00u) nullbytes++;
        snprintf(det, sizeof det, "%u Sync, %u Nullbytes von %u",
                 (unsigned)sync, (unsigned)nullbytes, (unsigned)n);
        pruefe("die Spur traegt 22 Synchronworte 0x4489 und KEIN einziges "
               "rohes Nullbyte - genau die Signatur, die MF-539 an der "
               "echten Aufnahme gemessen hat (vorher: 0 Sync, 5969 "
               "Nullbytes)",
               sync == 22u && nullbytes == 0u, det);
        free(spur);
    }

    /* ── 5. Gegenprobe: eine Groesse, die keine Amiga-Spur ist ─────── */
    {
        memset(&res, 0, sizeof res);
        rc = uftc_convert_sectors_to_hfe(adf, ADFSZ - 1u,
                                         "uft_mf1081_bad.hfe",
                                         UFT_FORMAT_ADF, &opts, &res);
        snprintf(det, sizeof det, "rc=%d", (int)rc);
        pruefe("eine Dateigroesse, die keine ganze Zahl Amiga-Spuren ist, "
               "wird ABGESAGT statt mit einer erfundenen Geometrie "
               "gewandelt (MF-1073)", rc != UFT_OK, det);
        remove("uft_mf1081_bad.hfe");
    }

    free(adf); free(hfedat); free(zurueck);
    remove(hfe_pfad);
    remove(adf_pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
