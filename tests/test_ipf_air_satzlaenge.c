/**
 * @file test_ipf_air_satzlaenge.c
 * @brief The IPF reader follows each record's own length (MF-1617).
 *
 * Louis-Guerin, "IPF Documentation" V0.0 (2012), ch. 2.1: every record
 * starts with type[4], length[4], crc[4], and the length covers the whole
 * record. Until MF-1617 `uft_ipf_air.c` read INFO and IMGE by a FIXED
 * advance (84 / 68 payload bytes) and never compared it with the length
 * the record states:
 *
 *   - an INFO record that is LONGER than 96 bytes put the reader in the
 *     middle of its own tail, and it read that tail as the next record
 *     header;
 *   - an INFO record that is SHORTER than 96 bytes was read on into the
 *     next record, whose bytes became geometry.
 *
 * The input is the free `disk_analyse_uftk_amiga.ipf` (Keir Fraser's
 * disk-analyse, Unlicense), rebuilt here with a changed INFO record and a
 * correct CRC; the rest of the chain is untouched. The CRC follows the
 * same document (CRC-32 over the record with its CRC field zeroed), via
 * the tree's `air_crc32_header()`.
 */
#include "uft/formats/ipf/uft_ipf_air.h"
#include "uft/formats/uft_air_crc32.h"
#include "uft/uft_endian.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* The file with the record at `off` resized by `delta` bytes: zeros
 * appended, or bytes cut off the end of the record. Length and CRC are
 * rewritten; the rest of the chain is copied unchanged. */
static uint8_t *mit_satz_laenge(const uint8_t *d, size_t n, size_t off,
                                long delta, size_t *out_n)
{
    const uint32_t alt = uft_read_be32(d + off + 4);
    const uint32_t neu = (uint32_t)((long)alt + delta);
    uint8_t *o = calloc(1, n + (delta > 0 ? (size_t)delta : 0));
    if (!o) return NULL;
    memcpy(o, d, off);                                    /* before */
    memcpy(o + off, d + off, neu < alt ? neu : alt);      /* record */
    /* the zeros for delta > 0 are already there (calloc) */
    memcpy(o + off + neu, d + off + alt, n - off - alt);  /* after */
    put32(o + off + 4, neu);
    put32(o + off + 8, air_crc32_header(o, off, neu));
    *out_n = n - alt + neu;
    return o;
}

static uint8_t *mit_info_laenge(const uint8_t *d, size_t n, long delta,
                                size_t *out_n)
{
    return mit_satz_laenge(d, n, 12, delta, out_n);   /* INFO at 12 */
}

/* offset of the first record of the given type */
static size_t erster_satz(const uint8_t *d, size_t n, const char *typ)
{
    size_t o = 0;
    while (o + 12 <= n) {
        if (memcmp(d + o, typ, 4) == 0) return o;
        uint32_t len = uft_read_be32(d + o + 4);
        if (len < 12) return 0;
        size_t next = o + len;
        if (memcmp(d + o, "DATA", 4) == 0) next += uft_read_be32(d + o + 12);
        o = next;
    }
    return 0;
}

static int zaehle_spuren(ipf_air_disk_t *a)
{
    int n = 0;
    for (int c = 0; c < 84; c++)
        for (int h = 0; h < 2; h++)
            if (ipf_air_track_present(a, c, h)) n++;
    return n;
}

int main(void)
{
    char pfad[600], det[300];
    snprintf(pfad, sizeof pfad, "%s/disk_analyse_uftk_amiga.ipf", UFT_CORPUS_DIR);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("IPF: each record by its own length - MF-1617\n");

    FILE *f = fopen(pfad, "rb");
    if (!f) { printf("  FAIL cannot open %s\n", pfad); return 1; }
    fseek(f, 0, SEEK_END);
    const long gr = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)gr);
    if (!d || fread(d, 1, (size_t)gr, f) != (size_t)gr) { fclose(f); return 1; }
    fclose(f);
    const size_t n = (size_t)gr;

    pruefe("(0) the second record is INFO with 96 bytes",
           memcmp(d + 12, "INFO", 4) == 0 && uft_read_be32(d + 16) == 96u,
           "fixture changed");

    /* reference: the untouched file */
    ipf_air_disk_t *ref = ipf_air_alloc();
    ipf_air_status_t st = ipf_air_parse(d, n, ref);
    int rc = 0, rs = 0; uint32_t rp = 0;
    ipf_air_get_geometry(ref, &rc, &rs, &rp);
    const int rt = zaehle_spuren(ref);
    snprintf(det, sizeof det, "status %d, %dx%d, %d tracks", (int)st, rc, rs, rt);
    pruefe("(1) the untouched file parses", st == IPF_AIR_OK && rt > 0, det);

    /* (2) INFO 4 bytes longer: the same disk must come out */
    size_t n2 = 0;
    uint8_t *d2 = mit_info_laenge(d, n, +4, &n2);
    ipf_air_disk_t *a2 = ipf_air_alloc();
    st = ipf_air_parse(d2, n2, a2);
    int c2 = 0, s2 = 0; uint32_t p2 = 0;
    ipf_air_get_geometry(a2, &c2, &s2, &p2);
    const int t2 = zaehle_spuren(a2);
    snprintf(det, sizeof det, "status %d, %dx%d, %d tracks (reference %dx%d, %d)",
             (int)st, c2, s2, t2, rc, rs, rt);
    pruefe("(2) INFO with 100 bytes: read by its length, same disk",
           st == IPF_AIR_OK && c2 == rc && s2 == rs && t2 == rt
           && ipf_air_crc_ok(a2), det);

    /* (3) INFO cut to 60 bytes: too short for its fields -> refused */
    size_t n3 = 0;
    uint8_t *d3 = mit_info_laenge(d, n, -36, &n3);
    ipf_air_disk_t *a3 = ipf_air_alloc();
    st = ipf_air_parse(d3, n3, a3);
    snprintf(det, sizeof det, "status %d (BAD_RECORD = %d)", (int)st,
             (int)IPF_AIR_BAD_RECORD);
    pruefe("(3) INFO with 60 bytes: refused, not read into the next record",
           st == IPF_AIR_BAD_RECORD, det);

    /* (3b) the first IMGE record 4 bytes longer: same disk */
    const size_t imge = erster_satz(d, n, "IMGE");
    size_t n5 = 0;
    uint8_t *d5 = imge ? mit_satz_laenge(d, n, imge, +4, &n5) : NULL;
    ipf_air_disk_t *a5 = ipf_air_alloc();
    st = d5 ? ipf_air_parse(d5, n5, a5) : IPF_AIR_FILE_ERROR;
    const int t5 = zaehle_spuren(a5);
    snprintf(det, sizeof det, "IMGE at %zu, status %d, %d tracks (reference %d)",
             imge, (int)st, t5, rt);
    pruefe("(3b) first IMGE with 84 bytes: read by its length, same disk",
           imge > 0 && st == IPF_AIR_OK && t5 == rt && ipf_air_crc_ok(a5), det);

    /* (3c) the first DATA record claiming 20 bytes (its fields need 28):
     * refused. Only the length field changes; the CRC is rebuilt over 20. */
    const size_t dat = erster_satz(d, n, "DATA");
    uint8_t *d6 = malloc(n);
    memcpy(d6, d, n);
    if (dat) {
        put32(d6 + dat + 4, 20);
        put32(d6 + dat + 8, air_crc32_header(d6, dat, 20));
    }
    ipf_air_disk_t *a6 = ipf_air_alloc();
    st = dat ? ipf_air_parse(d6, n, a6) : IPF_AIR_FILE_ERROR;
    snprintf(det, sizeof det, "DATA at %zu, status %d (BAD_RECORD = %d)",
             dat, (int)st, (int)IPF_AIR_BAD_RECORD);
    pruefe("(3c) DATA claiming 20 bytes: refused",
           dat > 0 && st == IPF_AIR_BAD_RECORD, det);
    ipf_air_free(a5); free(a5); free(d5);
    ipf_air_free(a6); free(a6); free(d6);

    /* (4) two CTEI records and one unknown record appended: the disk is
     * unchanged, and all three are COUNTED. Before MF-1617 the CTEI
     * values went into a structure nothing read, and the second CTEI
     * leaked the first (visible under ASan); an unknown record vanished
     * without a trace. */
    const size_t zusatz = 3 * 64;
    uint8_t *d4 = calloc(1, n + zusatz);
    memcpy(d4, d, n);
    const char *typ[3] = { "CTEI", "CTEI", "XTRA" };
    for (int i = 0; i < 3; i++) {
        uint8_t *r = d4 + n + (size_t)i * 64;
        memcpy(r, typ[i], 4);
        put32(r + 4, 64);
        put32(r + 8, air_crc32_header(d4, n + (size_t)i * 64, 64));
    }
    ipf_air_disk_t *a4 = ipf_air_alloc();
    st = ipf_air_parse(d4, n + zusatz, a4);
    uint32_t ct = 0, fremd = 0;
    const uint32_t alle = ipf_air_get_unread_records(a4, &ct, &fremd);
    const int t4 = zaehle_spuren(a4);
    snprintf(det, sizeof det, "status %d, %d tracks, unread %u (CT %u, other %u)",
             (int)st, t4, alle, ct, fremd);
    pruefe("(4) 2 CTEI + 1 unknown record: same disk, all three counted",
           st == IPF_AIR_OK && t4 == rt && alle == 3u && ct == 2u
           && fremd == 1u && ipf_air_crc_ok(a4), det);
    snprintf(det, sizeof det, "unread %u", ipf_air_get_unread_records(ref, NULL, NULL));
    pruefe("(5) the untouched file has no unread records",
           ipf_air_get_unread_records(ref, NULL, NULL) == 0u, det);

    ipf_air_free(ref); free(ref);
    ipf_air_free(a2); free(a2);
    ipf_air_free(a3); free(a3);
    ipf_air_free(a4); free(a4);
    free(d); free(d2); free(d3); free(d4);
    printf("%d ok, %d failed\n", gruen, rot);
    return rot ? 1 : 0;
}
