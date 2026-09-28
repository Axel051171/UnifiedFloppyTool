/* Apple 5.25" GCR: a sector whose data field lies past the track seam is
 * found, and a sector whose data checksum fails is reported as not
 * readable — not as 256 zero bytes (P3-638, P3-639, MF-1485).
 *
 * 1. Seam. uft_apple_gcr_scan_track() reads exactly one revolution and the
 *    bit reader is a ring, so a field may run over the seam. But the loop
 *    stopped at `r.pos < bit_count` even when an address field had been
 *    read and its data field was still to come: that sector vanished
 *    without a word. Found by the uft-a2-code package review at the real
 *    Applesauce capture copy2plus_52 (LOCAL-ONLY): T4 P15 and T5 P7, whose
 *    address fields start at bits 50934 / 51016 just before the seam —
 *    558 instead of 560 sectors (P3-638). Here the same situation is made
 *    in CI from the committed tests/corpus_free/to_woz2_uftk_dos33.woz
 *    (written by the oracle to_woz2): track 0 is rotated in 8-bit steps
 *    over the whole revolution, and every rotation must give all 16
 *    sectors with good checksums, each exactly once. A rotation is the
 *    same physical track with the index somewhere else.
 *
 * 2. Checksum. uft_apple_gcr_denibblize_6_2() leaves `data` untouched on a
 *    checksum failure, on purpose (a checksum that does not add up does
 *    not tell which bytes are right, MF-721). The WOZ and NIB plugins
 *    then handed that untouched, zeroed buffer on as the sector's data,
 *    flagged only with a bad CRC. At the real capture gauntlet_e7
 *    (LOCAL-ONLY) that was 550 sectors of 256 zero bytes (P3-639). Here:
 *    one bit of track 0 is flipped until exactly one sector loses its
 *    checksum; the WOZ plugin must report that sector as not readable.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/apple/uft_apple_gcr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#error "UFT_CORPUS_FREE_DIR must be set by tests/CMakeLists.txt"
#endif

extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_nib;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
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

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* WOZ2: find TMAP and TRKS by walking the chunks after the 12-byte header;
 * return the byte offset and bit count of the track TMAP[0] names. */
static int woz_spur0(const uint8_t *d, size_t n, size_t *off, uint32_t *bits)
{
    size_t pos = 12;
    const uint8_t *tmap = NULL, *trks = NULL;
    while (pos + 8 <= n) {
        uint32_t len = le32(d + pos + 4);
        if (memcmp(d + pos, "TMAP", 4) == 0) tmap = d + pos + 8;
        if (memcmp(d + pos, "TRKS", 4) == 0) trks = d + pos + 8;
        pos += 8 + (size_t)len;
    }
    if (!tmap || !trks || tmap[0] == 0xFF) return 0;
    const uint8_t *e = trks + 8u * tmap[0];
    *off = (size_t)le16(e) * 512u;
    *bits = le32(e + 4);
    return *off + (*bits + 7u) / 8u <= n;
}

static int bit(const uint8_t *b, uint32_t i) { return (b[i >> 3] >> (7u - (i & 7u))) & 1; }

static void rotiere(const uint8_t *in, uint8_t *out, uint32_t bits, uint32_t um)
{
    memset(out, 0, (bits + 7u) / 8u);
    for (uint32_t i = 0; i < bits; i++)
        if (bit(in, (i + um) % bits)) out[i >> 3] |= (uint8_t)(0x80u >> (i & 7u));
}

/* 16 distinct sectors of track 0, all with good checksums, none twice */
static int vollstaendig(const uint8_t *b, uint32_t bits, int *gefunden)
{
    uft_a2_sector_t sek[64];
    int n = uft_apple_gcr_scan_track(b, bits, sek, 64);
    unsigned maske = 0;
    int gut = 0;
    for (int i = 0; i < n; i++) {
        if (!sek[i].has_data || !sek[i].data_checksum_ok || sek[i].sector > 15) continue;
        if (maske & (1u << sek[i].sector)) return 0;   /* twice */
        maske |= 1u << sek[i].sector;
        gut++;
    }
    *gefunden = gut;
    return gut == 16 && n == 16;
}

int main(void)
{
    char h[200];
    size_t n = 0, off = 0;
    uint32_t bits = 0;

    printf("Apple GCR: Naht und Pruefsumme (MF-1485)\n");
    uint8_t *woz = lies(UFT_CORPUS_FREE_DIR "/to_woz2_uftk_dos33.woz", &n);
    if (!woz || !woz_spur0(woz, n, &off, &bits)) {
        pruefe("to_woz2_uftk_dos33.woz lesbar, Spur 0 gefunden", 0, NULL);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    const size_t nb = (bits + 7u) / 8u;
    uint8_t *spur = woz + off;
    uint8_t *r = (uint8_t *)malloc(nb);

    /* 1. every rotation, 8-bit steps */
    int g = 0;
    pruefe("Kontrolle: unrotierte Spur 0 liefert 16 Sektoren",
           vollstaendig(spur, bits, &g), NULL);
    unsigned schlecht = 0, versuche = 0, erste = 0, erste_g = 0;
    for (uint32_t um = 0; um < bits; um += 8, versuche++) {
        rotiere(spur, r, bits, um);
        if (!vollstaendig(r, bits, &g)) {
            if (!schlecht) { erste = um; erste_g = (unsigned)g; }
            schlecht++;
        }
    }
    snprintf(h, sizeof h, "%u von %u Rotationen unvollstaendig (erste bei Bit %u: %u Sektoren)",
             schlecht, versuche, erste, erste_g);
    pruefe("jede Rotation liefert 16 Sektoren, keiner doppelt (Feld hinter der Naht)",
           schlecht == 0, h);

    /* 1b. an address field just before the seam WITHOUT a data field: the
     *     scan reads on past the seam and must stop at the track's first
     *     address prologue instead of recording sector 0 a second time.
     *     Synthetic track: [addr S0][data S0] ... [addr S1] | seam. The
     *     data field of 256 zero bytes is 343 x 0x96 in 6-and-2 (every
     *     value of the running XOR is 0, and 0 is written as 0x96). */
    {
        uint8_t syn[2048];
        size_t k = 0;
        #define PUT(b) (syn[k++] = (uint8_t)(b))
        #define ADDR(s) do { uint8_t v_[4] = { 254, 0, (s), (uint8_t)(254 ^ 0 ^ (s)) };      \
                             PUT(0xD5); PUT(0xAA); PUT(0x96);                                  \
                             for (int q_ = 0; q_ < 4; q_++) { PUT((v_[q_] >> 1) | 0xAA);       \
                                                              PUT(v_[q_] | 0xAA); }            \
                             PUT(0xDE); PUT(0xAA); PUT(0xEB); } while (0)
        for (int i = 0; i < 20; i++) PUT(0xFF);
        ADDR(0);
        for (int i = 0; i < 6; i++) PUT(0xFF);
        PUT(0xD5); PUT(0xAA); PUT(0xAD);
        for (int i = 0; i < 343; i++) PUT(0x96);
        PUT(0xDE); PUT(0xAA); PUT(0xEB);
        for (int i = 0; i < 20; i++) PUT(0xFF);
        ADDR(1);                                   /* no data field follows */
        for (int i = 0; i < 10; i++) PUT(0xFF);
        #undef ADDR
        #undef PUT
        uft_a2_sector_t sek[8];
        int m = uft_apple_gcr_scan_track(syn, (uint32_t)(k * 8u), sek, 8);
        int s0 = 0;
        for (int i = 0; i < m; i++) if (sek[i].sector == 0 && sek[i].has_data) s0++;
        snprintf(h, sizeof h, "%d Felder, Sektor 0 %d-mal mit Daten", m, s0);
        pruefe("Adressfeld ohne Daten vor der Naht: Sektor 0 genau einmal", s0 == 1, h);
    }

    /* 2. one flipped bit, one bad checksum -> reported as not readable */
    long kipp = -1;
    for (uint32_t i = 0; i < bits && kipp < 0; i += 7) {
        memcpy(r, spur, nb);
        r[i >> 3] ^= (uint8_t)(0x80u >> (i & 7u));
        uft_a2_sector_t sek[64];
        int m = uft_apple_gcr_scan_track(r, bits, sek, 64);
        int schlechte = 0, gute = 0;
        for (int k = 0; k < m; k++) {
            if (!sek[k].has_data) continue;
            if (sek[k].data_checksum_ok) gute++; else schlechte++;
        }
        if (m == 16 && gute == 15 && schlechte == 1) kipp = (long)i;
    }
    pruefe("ein Bit gefunden, das genau eine Pruefsumme bricht", kipp >= 0, NULL);
    if (kipp >= 0) {
        spur[kipp >> 3] ^= (uint8_t)(0x80u >> (kipp & 7));
        char pfad[600];
        const char *d = getenv("TMPDIR");
        if (!d || !d[0]) d = getenv("TMP");
        if (!d || !d[0]) d = getenv("TEMP");
        if (!d || !d[0]) d = ".";
        snprintf(pfad, sizeof pfad, "%s/uft_mf1485.woz", d);
        FILE *f = fopen(pfad, "wb");
        if (f) { fwrite(woz, 1, n, f); fclose(f); }
        uft_disk_t disk;
        memset(&disk, 0, sizeof disk);
        if (f && uft_format_plugin_woz.open(&disk, pfad, true) == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            uft_error_t rc = uft_format_plugin_woz.read_track(&disk, 0, 0, &t);
            int nicht_lesbar = 0, null_gut = 0, gemeldet = (int)t.sector_count;
            for (size_t i = 0; i < t.sector_count; i++) {
                const uft_sector_t *s = &t.sectors[i];
                if (s->status & UFT_SECTOR_UNAVAILABLE) nicht_lesbar++;
                else if (s->status != UFT_SECTOR_OK) null_gut++;
            }
            snprintf(h, sizeof h, "rc=%d, %d Sektoren, %d nicht lesbar, %d mit CRC-Fehler und Daten",
                     (int)rc, gemeldet, nicht_lesbar, null_gut);
            pruefe("Pruefsummenfehler: Sektor gemeldet als nicht lesbar, keine Nullen als Daten",
                   rc == UFT_OK && gemeldet == 16 && nicht_lesbar == 1 && null_gut == 0, h);
            for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
            free(t.sectors);
            uft_format_plugin_woz.close(&disk);
        } else pruefe("veraenderte WOZ oeffnet", 0, pfad);
        remove(pfad);
    }

    free(r);
    free(woz);

    /* 2b. the same for the NIB plugin, which had the same two lines. A
     *     synthetic NIB (35 x 6656 raw nibble bytes): track 0 holds
     *     sector 0 whose data field is 343 x 0x96 except the checksum
     *     nibble, 0x97 (value 1, running XOR 0) — the checksum fails. */
    {
        enum { NT = 35, NS = 6656 };
        uint8_t *nib = (uint8_t *)malloc((size_t)NT * NS);
        memset(nib, 0xFF, (size_t)NT * NS);
        size_t k = 40;
        const uint8_t addr[8] = { 0xFF, 0xFE, 0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFE };
        nib[k++] = 0xD5; nib[k++] = 0xAA; nib[k++] = 0x96;       /* vol 254, T0, S0 */
        memcpy(nib + k, addr, 8); k += 8;
        nib[k++] = 0xDE; nib[k++] = 0xAA; nib[k++] = 0xEB;
        k += 6;
        nib[k++] = 0xD5; nib[k++] = 0xAA; nib[k++] = 0xAD;
        for (int i = 0; i < 342; i++) nib[k++] = 0x96;
        nib[k++] = 0x97;                                          /* bad checksum */
        nib[k++] = 0xDE; nib[k++] = 0xAA; nib[k++] = 0xEB;
        char pfad[600];
        const char *d = getenv("TMPDIR");
        if (!d || !d[0]) d = getenv("TMP");
        if (!d || !d[0]) d = getenv("TEMP");
        if (!d || !d[0]) d = ".";
        snprintf(pfad, sizeof pfad, "%s/uft_mf1485.nib", d);
        FILE *f = fopen(pfad, "wb");
        if (f) { fwrite(nib, 1, (size_t)NT * NS, f); fclose(f); }
        free(nib);
        uft_disk_t disk;
        memset(&disk, 0, sizeof disk);
        if (f && uft_format_plugin_nib.open(&disk, pfad, true) == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            uft_error_t rc = uft_format_plugin_nib.read_track(&disk, 0, 0, &t);
            snprintf(h, sizeof h, "rc=%d, %u Sektoren, Status 0x%X", (int)rc,
                     (unsigned)t.sector_count,
                     t.sector_count ? (unsigned)t.sectors[0].status : 0u);
            pruefe("NIB: Pruefsummenfehler als nicht lesbar gemeldet",
                   rc == UFT_OK && t.sector_count == 1
                       && (t.sectors[0].status & UFT_SECTOR_UNAVAILABLE), h);
            for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
            free(t.sectors);
            uft_format_plugin_nib.close(&disk);
        } else pruefe("synthetische NIB oeffnet", 0, pfad);
        remove(pfad);
    }

    /* 3. LOCAL-ONLY: the two real Applesauce captures the review measured.
     *    Absent in CI (gitignored corpus) — then nothing is claimed for
     *    them, and it is said; the CI part above still ran. */
#ifdef UFT_CORPUS_RESTRICTED_DIR
    {
        static const struct { const char *datei; unsigned gut, nicht_lesbar; } echt[] = {
            /* P3-638: T4 P15 and T5 P7 past the seam — 558 before */
            { "wozaday_copy2plus_52.woz", 560u, 0u },
            /* P3-639: only T0 has good checksums (a foreign RWTS, residue
             * 0x3F on every field) — 550 zero sectors before */
            { "wozaday_gauntlet_e7.woz",   10u, 550u },
        };
        for (size_t k = 0; k < sizeof echt / sizeof echt[0]; k++) {
            char pfad[700];
            snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_RESTRICTED_DIR, echt[k].datei);
            FILE *probe = fopen(pfad, "rb");
            if (!probe) { printf("  [--] %s fehlt (LOCAL-ONLY) — nichts behauptet\n", echt[k].datei); continue; }
            fclose(probe);
            uft_disk_t disk;
            memset(&disk, 0, sizeof disk);
            if (uft_format_plugin_woz.open(&disk, pfad, true) != UFT_OK) {
                pruefe(echt[k].datei, 0, "oeffnet nicht");
                continue;
            }
            unsigned gut = 0, nl = 0, sonst = 0;
            for (int c = 0; c < 35; c++) {
                uft_track_t t;
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_woz.read_track(&disk, c, 0, &t) == UFT_OK)
                    for (size_t i = 0; i < t.sector_count; i++) {
                        if (t.sectors[i].status == UFT_SECTOR_OK) gut++;
                        else if (t.sectors[i].status & UFT_SECTOR_UNAVAILABLE) nl++;
                        else sonst++;
                    }
                for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
                free(t.sectors);
            }
            uft_format_plugin_woz.close(&disk);
            snprintf(h, sizeof h, "%u gut, %u nicht lesbar, %u sonst", gut, nl, sonst);
            char was[200];
            snprintf(was, sizeof was, "%s: %u gut, %u nicht lesbar", echt[k].datei,
                     echt[k].gut, echt[k].nicht_lesbar);
            pruefe(was, gut == echt[k].gut && nl == echt[k].nicht_lesbar && sonst == 0, h);
        }
    }
#endif

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
