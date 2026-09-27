/**
 * @file test_convert_d81_hfe_roundtrip.c
 * @brief D81 -> HFE -> D81 for the Commodore 1581 (A-037, MF-1437).
 *
 * A new conversion path. What it must hold, and where each promise comes
 * from:
 *
 *   1. The round trip D81 -> HFE -> D81 is byte-identical. The way in
 *      (uftc_convert_sectors_to_hfe, MFM encoder) and the way out
 *      (uftc_convert_hfe_to_sectors, MFM decode) are two implementations.
 *   2. The SIDES are the 1581's, not a PC's. The writer itself, the 1581
 *      ROM (neue-ideen/1/1581.zip, read only): MSUB.SRC gives logical
 *      sectors below numsec/2 = 20 the side 0 (`tcacheside`), MROUT.SRC
 *      `fmtrk` writes that side into the ID field, and `side_ctl` selects
 *      the PHYSICAL head 1 for side 0 ("lda #0 ; side one"). So the first
 *      half of every D81 track sits on physical head 1 with ID head byte
 *      0, the second half on physical head 0 with ID head byte 1.
 *      Greaseweazle (`image/d81.py` sides_swapped, `disk 1581`, Unlicense)
 *      was run on the same input and puts them there too (measured
 *      2026-09-27). MAME's d81_dsk.cpp puts the first half on head 0
 *      (P3-634).
 *   3. The track starts the way the ROM formats it: 32 x 0x4E and NO index
 *      address mark (MROUT.SRC `fmtrk`), so the first sync of a track
 *      comes after 32 gap + 12 zero bytes = 44 bytes = 704 MFM cells.
 *   4. What is not a 1581 disk is refused: a D81 with an appended error
 *      table (822 400 bytes), and an HFE of an 18-sector PC disk read back
 *      as D81 (it has 80 x 2 too, and without the check its sectors 1..10
 *      would have made a "complete" D81).
 *
 * No corpus file: the D81 is built here, every 256-byte block naming its
 * own track and sector, so a result says WHERE it came from.
 */
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_sectors_to_hfe(
        const uint8_t *src_data, size_t src_size, const char *dst_path,
        uft_format_t src_format, const uft_convert_options_ext_t *opts,
        uft_convert_result_t *result);
extern uft_error_t uftc_convert_hfe_to_sectors(
        const uint8_t *src_data, size_t src_size, const char *src_path,
        const char *dst_path, uft_format_t dst_format,
        const uft_convert_options_ext_t *opts,
        uft_convert_result_t *result);

#define D81SZ 819200u

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
    if (!b || fread(b, 1, (size_t)g, f) != (size_t)g) {
        fclose(f); free(b); return NULL;
    }
    fclose(f); *n = (size_t)g; return b;
}

static void temp_pfad(char *p, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_mf1437_%s", d, name);
}

/* ── the HFE side of cylinder 0, head `kopf`, as an MSB-first cell stream ── */

static uint8_t *hfe_seite(const uint8_t *hfe, size_t n, int kopf, size_t *bits)
{
    *bits = 0;
    if (n < 1024) return NULL;
    const size_t lut = (size_t)(hfe[18] | (hfe[19] << 8)) * 512u;
    if (lut + 4 > n) return NULL;
    const size_t off = (size_t)(hfe[lut] | (hfe[lut + 1] << 8)) * 512u;
    const size_t len = (size_t)(hfe[lut + 2] | (hfe[lut + 3] << 8));
    if (off + len > n) return NULL;
    const size_t je_seite = len / 2u;
    uint8_t *out = (uint8_t *)calloc(1, je_seite);
    if (!out) return NULL;
    for (size_t i = 0; i < je_seite; i++) {
        const size_t block = i / 256u, in_block = i % 256u;
        uint8_t v = hfe[off + block * 512u + (size_t)kopf * 256u + in_block];
        uint8_t r = 0;                         /* HFE stores LSB first */
        for (int b = 0; b < 8; b++) r = (uint8_t)(r | (((v >> b) & 1u) << (7 - b)));
        out[i] = r;
    }
    *bits = je_seite * 8u;
    return out;
}

static int zelle(const uint8_t *p, size_t i) { return (p[i >> 3] >> (7 - (i & 7u))) & 1; }

static int mfm_byte(const uint8_t *p, size_t start)
{
    int v = 0;
    for (int k = 0; k < 8; k++) v = (v << 1) | zelle(p, start + (size_t)k * 2u + 1u);
    return v;
}

/* First IDAM of the side: bit position of its first 0x4489 sync, the ID
 * head byte, and the first 16 data bytes of the sector behind it. */
static bool erster_sektor(const uint8_t *p, size_t bits, size_t *sync_bit,
                          int *id_kopf, char *daten16)
{
    for (size_t i = 0; i + 48 + 16 * 10 < bits; i++) {
        int treffer = 1;
        for (int s = 0; s < 3 && treffer; s++)
            for (int k = 0; k < 16; k++)
                if (zelle(p, i + (size_t)s * 16u + (size_t)k)
                    != ((0x4489 >> (15 - k)) & 1)) { treffer = 0; break; }
        if (!treffer || mfm_byte(p, i + 48) != 0xFE) continue;
        *sync_bit = i;
        *id_kopf = mfm_byte(p, i + 48 + 16 * 2);
        /* the DAM: next 3x 0x4489 + 0xFB after the ID field */
        for (size_t j = i + 48 + 16 * 7; j + 48 + 16 * 17 < bits; j++) {
            int t2 = 1;
            for (int s = 0; s < 3 && t2; s++)
                for (int k = 0; k < 16; k++)
                    if (zelle(p, j + (size_t)s * 16u + (size_t)k)
                        != ((0x4489 >> (15 - k)) & 1)) { t2 = 0; break; }
            if (!t2 || mfm_byte(p, j + 48) != 0xFB) continue;
            for (int k = 0; k < 16; k++)
                daten16[k] = (char)mfm_byte(p, j + 64 + (size_t)k * 16u);
            daten16[16] = '\0';
            return true;
        }
        return false;
    }
    return false;
}

int main(void)
{
    char det[320], hfe_pfad[600], d81_pfad[600], pc_pfad[600];
    uft_convert_options_ext_t opts;
    uft_convert_result_t res;
    uft_error_t rc;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("D81 -> HFE -> D81, Commodore 1581 (MF-1437)\n");
    temp_pfad(hfe_pfad, sizeof hfe_pfad, "probe.hfe");
    temp_pfad(d81_pfad, sizeof d81_pfad, "probe.d81");
    temp_pfad(pc_pfad, sizeof pc_pfad, "pc.d81");

    /* A self-naming D81: block (track 1..80, sector 0..39). */
    uint8_t *d81 = (uint8_t *)calloc(1, D81SZ);
    if (!d81) return 1;
    for (unsigned t = 0; t < 80; t++)
        for (unsigned s = 0; s < 40; s++) {
            uint8_t *b = d81 + ((size_t)t * 40u + s) * 256u;
            int l = snprintf((char *)b, 256, "UFT-1581 T%02u S%02u", t + 1, s);
            memset(b + l, (int)((t * 40u + s) & 0xFFu), 256u - (size_t)l);
        }

    /* 1. D81 -> HFE */
    memset(&opts, 0, sizeof opts); memset(&res, 0, sizeof res);
    rc = uftc_convert_sectors_to_hfe(d81, D81SZ, hfe_pfad, UFT_FORMAT_D81,
                                     &opts, &res);
    snprintf(det, sizeof det, "rc=%d", (int)rc);
    pruefe("D81 -> HFE laeuft durch", rc == UFT_OK, det);
    size_t hn = 0;
    uint8_t *hfe = (rc == UFT_OK) ? lies(hfe_pfad, &hn) : NULL;
    if (!hfe) { printf("\n%d gruen, %d rot\n", gruen, rot + 1); return 1; }

    /* HFE header: 80 cylinders, 2 heads, ISO/IBM MFM, Shugart, 250 kbit/s,
     * 300 rpm from the 1581 profile. */
    snprintf(det, sizeof det, "cyl %u heads %u enc %u rate %u rpm %u if %u",
             hfe[9], hfe[10], hfe[11], hfe[12] | (hfe[13] << 8),
             hfe[14] | (hfe[15] << 8), hfe[16]);
    pruefe("HFE-Kopf: 80 x 2, ISO-MFM, 250 kbit/s, 300 U/min, Shugart",
           hfe[9] == 80 && hfe[10] == 2 && hfe[11] == 0x00
           && (hfe[12] | (hfe[13] << 8)) == 250
           && (hfe[14] | (hfe[15] << 8)) == 300 && hfe[16] == 0x07, det);

    /* 2 + 3. physical placement and track start, cylinder 0 */
    for (int kopf = 0; kopf < 2; kopf++) {
        size_t bits = 0, sync = 0;
        int idk = -1;
        char d16[17] = "";
        uint8_t *seite = hfe_seite(hfe, hn, kopf, &bits);
        bool ok = seite && erster_sektor(seite, bits, &sync, &idk, d16);
        const char *soll = (kopf == 1) ? "UFT-1581 T01 S00" : "UFT-1581 T01 S20";
        snprintf(det, sizeof det, "physischer Kopf %d: ID-Kopfbyte %d, Daten "
                 "\"%s\", erster Sync bei Zelle %u", kopf, idk, d16,
                 (unsigned)sync);
        pruefe(kopf == 1
               ? "physischer Kopf 1 traegt ID-Seite 0 und die ERSTE Haelfte "
                 "der Spur (1581-ROM, gw)"
               : "physischer Kopf 0 traegt ID-Seite 1 und die ZWEITE Haelfte",
               ok && idk == 1 - kopf && strcmp(d16, soll) == 0, det);
        pruefe("die Spur beginnt mit 32 x 4E ohne IAM: erster Sync bei "
               "Zelle 704", ok && sync == 704u, det);
        free(seite);
    }

    /* 1. HFE -> D81, byte for byte */
    memset(&res, 0, sizeof res);
    rc = uftc_convert_hfe_to_sectors(hfe, hn, hfe_pfad, d81_pfad,
                                     UFT_FORMAT_D81, &opts, &res);
    size_t zn = 0, abweichend = 0;
    uint8_t *zurueck = (rc == UFT_OK) ? lies(d81_pfad, &zn) : NULL;
    if (zurueck && zn == D81SZ)
        for (size_t i = 0; i < D81SZ; i++) abweichend += (d81[i] != zurueck[i]);
    snprintf(det, sizeof det, "rc=%d, %u Byte zurueck, %u abweichend, %d "
             "Sektoren", (int)rc, (unsigned)zn, (unsigned)abweichend,
             res.sectors_converted);
    pruefe("D81 -> HFE -> D81 ist BYTEIDENTISCH (819 200 Byte, 1600 Sektoren)",
           rc == UFT_OK && zn == D81SZ && abweichend == 0
           && res.sectors_converted == 1600, det);
    free(zurueck);

    /* 4a. a D81 with an error table is refused, not cut */
    uint8_t *mit_tafel = (uint8_t *)calloc(1, D81SZ + 3200u);
    if (mit_tafel) {
        memcpy(mit_tafel, d81, D81SZ);
        memset(&res, 0, sizeof res);
        rc = uftc_convert_sectors_to_hfe(mit_tafel, D81SZ + 3200u, pc_pfad,
                                         UFT_FORMAT_D81, &opts, &res);
        snprintf(det, sizeof det, "rc=%d", (int)rc);
        pruefe("D81 mit Fehlertafel (822 400 Byte) wird abgewiesen, nicht "
               "beschnitten", rc != UFT_OK, det);
        free(mit_tafel);
    }

    /* 4b. an 18-sector PC HFE (80 x 2 as well) is no 1581 disk */
    const size_t pcsz = 1474560u;
    uint8_t *pc = (uint8_t *)calloc(1, pcsz);
    if (pc) {
        /* a minimal FAT12 boot sector so the IMG path derives 80/2/18 */
        pc[0] = 0xEB; pc[1] = 0x3C; pc[2] = 0x90;
        pc[11] = 0x00; pc[12] = 0x02;          /* 512 bytes/sector */
        pc[13] = 1; pc[14] = 1; pc[16] = 2;
        pc[17] = 224; pc[19] = 0x40; pc[20] = 0x0B;   /* 2880 sectors */
        pc[21] = 0xF0; pc[22] = 9; pc[24] = 18; pc[26] = 2;
        pc[510] = 0x55; pc[511] = 0xAA;
        char pc_hfe[600];
        temp_pfad(pc_hfe, sizeof pc_hfe, "pc.hfe");
        memset(&res, 0, sizeof res);
        rc = uftc_convert_sectors_to_hfe(pc, pcsz, pc_hfe, UFT_FORMAT_IMG,
                                         &opts, &res);
        size_t pn = 0;
        uint8_t *pch = (rc == UFT_OK) ? lies(pc_hfe, &pn) : NULL;
        if (!pch) {
            snprintf(det, sizeof det, "rc=%d", (int)rc);
            pruefe("Vorbereitung: PC-HFE 1.44M erzeugt", 0, det);
        } else {
            memset(&res, 0, sizeof res);
            rc = uftc_convert_hfe_to_sectors(pch, pn, pc_hfe, pc_pfad,
                                             UFT_FORMAT_D81, &opts, &res);
            snprintf(det, sizeof det, "rc=%d, %d Sektoren", (int)rc,
                     res.sectors_converted);
            pruefe("eine 18-Sektor-PC-HFE wird NICHT als D81 geschrieben",
                   rc != UFT_OK, det);
            free(pch);
        }
        remove(pc_hfe);
        free(pc);
    }

    remove(hfe_pfad); remove(d81_pfad); remove(pc_pfad);
    free(hfe); free(d81);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
