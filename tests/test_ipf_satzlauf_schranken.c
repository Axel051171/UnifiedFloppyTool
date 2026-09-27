/**
 * @file test_ipf_satzlauf_schranken.c
 * @brief IPF: der Satzlauf haelt an seinen Grenzen, die CRC erreicht den
 *        Aufrufer, und kein Spurkopf faellt still weg (MF-1372).
 *
 * ── Die drei Befunde ────────────────────────────────────────────────────
 *
 * 1. **Satzlauf ohne Schranke.** `ipf_air_parse()` rechnete Position und
 *    Satzlaenge in `uint32_t` und pruefte nie, ob ein Satz mindestens
 *    seinen eigenen Kopf umfasst. Gemessen vor der Behebung, an einem
 *    selbst gebauten Puffer, der nur „CAPS“ vorn braucht, um durch die
 *    Sonde zu kommen:
 *      - unbekannter Satz mit Laenge 0  -> Endlosschleife (nach 5 s
 *        abgebrochen), weil `pos = start_pos + rec_len` stehen bleibt;
 *      - DATA-Satz mit Nutzlaenge 0xFFFFFFF0 -> `pos + dr.length` wickelt
 *        ueber, die Pruefung besteht, und die CRC-Rechnung liest hinter
 *        den Puffer (ASan: stack-buffer-overflow in `air_crc32_buffer`).
 *    Dazu zwei stille Faelle: ein Satz, der ueber das Dateiende reicht,
 *    wurde ohne CRC-Pruefung uebersprungen, und eine DATA-Nutzlast hinter
 *    dem Dateiende ebenso — beide mit `IPF_AIR_OK`.
 *
 * 2. **CRC gerechnet, nie gemeldet.** `crc_ok` wurde auf false gesetzt
 *    und nur von einem `printf` gelesen; das Plugin meldete trotzdem
 *    `UFT_FORMAT_CAP_VERIFY`. Eine Spur, deren Kopf oder Nutzlast ihre
 *    Pruefsumme nicht haelt, kam als gewoehnliche Spur heraus.
 *
 * 3. **Stille Verluste.** Ein IMGE-Satz ausserhalb von 84 x 2 wurde ohne
 *    Zaehler verworfen, und ab dem 513. IMGE-Satz fand kein DATA-Satz
 *    mehr seinen Spurkopf (`images[512]`).
 *
 * ── Referenz ────────────────────────────────────────────────────────────
 *
 * Satzaufbau und CRC nach Jean Louis-Guerin, „Interchangeable Preservation
 * Format (IPF) Documentation“ V0.0, Januar 2012, Kap. 2 (sha256 der PDF
 * eb1bfcdb95c0ca6f59a526b8f08752c12b2ce76b6c5276bae3c006d3c28241b5):
 * Kopf = Name, Groesse, CRC32, alles BE32; die CRC laeuft ueber Kopf und
 * Satzdaten mit dem CRC-Feld auf 0, die DATA-Nutzlast hat ihre eigene
 * CRC32 im DATA-Block. INFO = 96 Byte, IMGE = 80 Byte.
 *
 * Die Pruefsummen dieses Tests rechnet der Test SELBST, mit einer eigenen
 * bitweisen CRC-32 (Polynom 0xEDB88320) — nicht mit der Tafel des
 * Prueflings. Dass die gute Datei gut gelesen wird, belegt damit zugleich,
 * dass beide Rechnungen dieselbe Beschreibung treffen.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/formats/ipf/uft_ipf_air.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_ipf;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s\n", was); }
}

/* ── eigene CRC-32, bitweise ───────────────────────────────────────────── */

static uint32_t crc32_bitweise(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++)
            c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);
    }
    return ~c;
}

/* ── Pufferbau ─────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *p;
    size_t   n, cap;
} puffer_t;

static void anhaengen(puffer_t *b, const void *q, size_t n)
{
    if (b->n + n > b->cap) {
        size_t neu = (b->cap ? b->cap * 2u : 256u);
        while (neu < b->n + n) neu *= 2u;
        b->p = (uint8_t *)realloc(b->p, neu);
        if (!b->p) { printf("kein Speicher\n"); exit(2); }
        b->cap = neu;
    }
    memcpy(b->p + b->n, q, n);
    b->n += n;
}

static void be32_an(puffer_t *b, uint32_t v)
{
    const uint8_t w[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                           (uint8_t)(v >> 8), (uint8_t)v };
    anhaengen(b, w, 4);
}

static void be32_setzen(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/** Satz aus Name und Datenwoertern, CRC nach der Beschreibung. */
static void satz(puffer_t *b, const char *name, const uint32_t *w, size_t nw)
{
    const size_t start = b->n;
    size_t i;
    anhaengen(b, name, 4);
    be32_an(b, (uint32_t)(12u + 4u * nw));
    be32_an(b, 0);
    for (i = 0; i < nw; i++) be32_an(b, w[i]);
    be32_setzen(b->p + start + 8, crc32_bitweise(b->p + start, 12u + 4u * nw));
}

static void caps_satz(puffer_t *b) { satz(b, "CAPS", NULL, 0); }

static void info_satz(puffer_t *b, uint32_t max_track, uint32_t max_side)
{
    uint32_t w[21];
    memset(w, 0, sizeof w);
    w[0] = 1;            /* Floppy */
    w[1] = 1;            /* CAPS-Kodierer */
    w[2] = 1;
    w[3] = 4711;         /* Release-ID */
    w[4] = 1;
    w[7] = max_track;
    w[9] = max_side;
    w[12] = 2;           /* Atari ST */
    satz(b, "INFO", w, 21);
}

static void imge_satz(puffer_t *b, uint32_t track, uint32_t side,
                      uint32_t blocks, uint32_t key)
{
    uint32_t w[17];
    memset(w, 0, sizeof w);
    w[0] = track;
    w[1] = side;
    w[2] = 2;            /* Dichte „Auto“ */
    w[3] = 1;            /* 2-us-Zellen */
    w[10] = blocks;
    w[7] = blocks * 80u;   /* Datenbits: 80 je Block (block_deskriptor) */
    w[9] = blocks * 80u;   /* Spurbits: kein Zwischenraum              */
    w[13] = key;
    satz(b, "IMGE", w, 17);
}

/** DATA-Satz; die Nutzlast bekommt ihre eigene CRC. */
static void data_satz(puffer_t *b, uint32_t key, const uint8_t *nutz,
                      uint32_t n)
{
    uint32_t w[4];
    w[0] = n;
    w[1] = n * 8u;
    w[2] = n ? crc32_bitweise(nutz, n) : 0u;
    w[3] = key;
    satz(b, "DATA", w, 4);
    if (n) anhaengen(b, nutz, n);
}

/** Ein Blockdeskriptor (32 Byte), CAPS-Lesart. */
static void block_deskriptor(uint8_t *ziel)
{
    memset(ziel, 0, 32);
    be32_setzen(ziel + 0, 80);   /* Datenbits */
    be32_setzen(ziel + 16, 1);   /* MFM */
}

/** Die gute Kleinstdatei: CAPS, INFO, IMGE 0/0, DATA mit 36 Byte. */
static void gute_datei(puffer_t *b, size_t *imge_off, size_t *nutz_off)
{
    uint8_t nutz[36];
    block_deskriptor(nutz);
    memcpy(nutz + 32, "UFT!", 4);
    caps_satz(b);
    info_satz(b, 0, 0);
    *imge_off = b->n;
    imge_satz(b, 0, 0, 1, 1);
    data_satz(b, 1, nutz, sizeof nutz);
    *nutz_off = b->n - sizeof nutz;
}

/* ── Plugin ueber eine Wegwerfdatei ────────────────────────────────────── */

static const char *wegwerf_pfad(void)
{
    static char pfad[1024];
    const char *d = getenv("TMPDIR");
    if (!d || !*d) d = getenv("TMP");
    if (!d || !*d) d = getenv("TEMP");
    if (!d || !*d) d = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_ipf_satzlauf.ipf", d);
    return pfad;
}

/** Oeffnet den Puffer ueber das Plugin und liest Spur 0/0.
 *  Rueckgabe: Rueckgabe von open(); *status/*rawbits aus read_track. */
static int ueber_plugin(const puffer_t *b, uint32_t *status,
                        size_t *raw_bits, int *read_rc)
{
    const char *pfad = wegwerf_pfad();
    FILE *f = fopen(pfad, "wb");
    uft_disk_t disk;
    uft_track_t t;
    int rc;
    if (!f) { printf("Wegwerf-Datei fehlgeschlagen: %s\n", pfad); exit(2); }
    fwrite(b->p, 1, b->n, f);
    fclose(f);
    memset(&disk, 0, sizeof disk);
    rc = uft_format_plugin_ipf.open(&disk, pfad, true);
    *status = 0; *raw_bits = 0; *read_rc = -999;
    if (rc == UFT_OK) {
        memset(&t, 0, sizeof t);
        *read_rc = uft_format_plugin_ipf.read_track(&disk, 0, 0, &t);
        *status = t.status;
        *raw_bits = t.raw_bits;   /* die ANGABE, auch ohne Daten */
        uft_track_release(&t);
        uft_format_plugin_ipf.close(&disk);
    }
    remove(pfad);
    return rc;
}

static ipf_air_status_t parse(const puffer_t *b, ipf_air_disk_t **aus)
{
    ipf_air_disk_t *d = ipf_air_alloc();
    ipf_air_status_t st;
    if (!d) { printf("kein Speicher\n"); exit(2); }
    st = ipf_air_parse(b->p, b->n, d);
    if (aus) *aus = d;
    else { ipf_air_free(d); free(d); }
    return st;
}

static void weg(ipf_air_disk_t *d) { ipf_air_free(d); free(d); }

int main(void)
{
    printf("== Befund 1: Satzlauf-Schranken ==\n");
    {
        puffer_t b = {0};
        caps_satz(&b);
        anhaengen(&b, "XXXX", 4); be32_an(&b, 0); be32_an(&b, 0);
        pruefe("1a unbekannter Satz mit Laenge 0 wird abgesagt (keine "
               "Endlosschleife)", parse(&b, NULL) != IPF_AIR_OK);
        free(b.p);
    }
    {
        puffer_t b = {0};
        caps_satz(&b);
        anhaengen(&b, "XXXX", 4); be32_an(&b, 5); be32_an(&b, 0);
        anhaengen(&b, "\0\0\0\0\0\0\0\0\0\0\0\0", 12);
        pruefe("1b Satzlaenge kleiner als der eigene Kopf wird abgesagt",
               parse(&b, NULL) != IPF_AIR_OK);
        free(b.p);
    }
    {
        puffer_t b = {0};
        uint32_t w[4] = { 0xFFFFFFF0u, 0, 0, 1 };
        caps_satz(&b);
        info_satz(&b, 0, 0);
        imge_satz(&b, 0, 0, 0, 1);
        satz(&b, "DATA", w, 4);
        anhaengen(&b, "UFT!", 4);
        pruefe("1c DATA-Nutzlaenge 0xFFFFFFF0 (Ueberlauf) wird abgesagt",
               parse(&b, NULL) != IPF_AIR_OK);
        free(b.p);
    }
    {
        puffer_t b = {0};
        caps_satz(&b);
        anhaengen(&b, "XXXX", 4); be32_an(&b, 1000); be32_an(&b, 0);
        anhaengen(&b, "0123456789", 10);
        pruefe("1d ein Satz, der ueber das Dateiende reicht, ist "
               "IPF_AIR_TRUNCATED", parse(&b, NULL) == IPF_AIR_TRUNCATED);
        free(b.p);
    }
    {
        puffer_t b = {0};
        uint32_t w[4] = { 100, 800, 0, 1 };
        caps_satz(&b);
        info_satz(&b, 0, 0);
        imge_satz(&b, 0, 0, 0, 1);
        satz(&b, "DATA", w, 4);
        anhaengen(&b, "0123456789", 10);
        pruefe("1e eine DATA-Nutzlast hinter dem Dateiende ist "
               "IPF_AIR_TRUNCATED", parse(&b, NULL) == IPF_AIR_TRUNCATED);
        free(b.p);
    }

    printf("== Befund 2: die CRC erreicht den Aufrufer ==\n");
    {
        puffer_t b = {0};
        size_t io, no;
        uint32_t st; size_t bits; int rrc;
        ipf_air_disk_t *d = NULL;
        gute_datei(&b, &io, &no);
        pruefe("2a die gute Kleinstdatei wird gelesen (eigene CRC-32 "
               "trifft die des Lesers)", parse(&b, &d) == IPF_AIR_OK);
        pruefe("2b ... und ipf_air_crc_ok() sagt ja",
               d && ipf_air_crc_ok(d));
        pruefe("2c ... und haelt ihren einen Block",
               d && ipf_air_get_block_count(d, 0, 0) == 1);
        if (d) weg(d);
        pruefe("2d Plugin: open und read_track 0/0 gelingen",
               ueber_plugin(&b, &st, &bits, &rrc) == UFT_OK && rrc == UFT_OK);
        pruefe("2e Plugin: keine CRC-Flagge an der guten Spur",
               (st & (UFT_TRACK_HDR_CRC | UFT_TRACK_DATA_CRC)) == 0);
        pruefe("2e' ... und nennt die 80 Spurbits ihres Kopfes", bits == 80u);

        b.p[no + 33] ^= 0x01;   /* ein Bit in der Nutzlast */
        d = NULL;
        pruefe("2f Nutzlast-Bit gekippt: die Datei bleibt lesbar",
               parse(&b, &d) == IPF_AIR_OK);
        pruefe("2g ... ipf_air_crc_ok() sagt nein", d && !ipf_air_crc_ok(d));
        if (d) weg(d);
        (void)ueber_plugin(&b, &st, &bits, &rrc);
        pruefe("2h Plugin: die Spur traegt UFT_TRACK_DATA_CRC",
               rrc == UFT_OK && (st & UFT_TRACK_DATA_CRC) != 0);
        b.p[no + 33] ^= 0x01;

        b.p[io + 12 + 4 * 14] ^= 0x01;   /* reserviertes Wort im IMGE */
        (void)ueber_plugin(&b, &st, &bits, &rrc);
        pruefe("2i IMGE-Kopf gekippt: die Spur traegt UFT_TRACK_HDR_CRC",
               rrc == UFT_OK && (st & UFT_TRACK_HDR_CRC) != 0);
        pruefe("2j ... und nennt keine Zellzahl (die Spurbits stehen im "
               "gebrochenen Kopf)", bits == 0);
        b.p[io + 12 + 4 * 14] ^= 0x01;

        b.p[12 + 12 + 4 * 18] ^= 0x01;   /* reserviertes Wort im INFO */
        d = NULL;
        pruefe("2k INFO-Kopf gekippt: lesbar, aber ipf_air_crc_ok() "
               "sagt nein", parse(&b, &d) == IPF_AIR_OK && d
                            && !ipf_air_crc_ok(d));
        if (d) weg(d);
        free(b.p);
    }

    printf("== Befund 3: kein Spurkopf faellt still weg ==\n");
    {
        puffer_t b = {0};
        ipf_air_disk_t *d = NULL;
        uint8_t leer[36];
        caps_satz(&b);
        info_satz(&b, 0, 0);
        imge_satz(&b, 0, 0, 0, 1);
        imge_satz(&b, 84, 0, 0, 2);
        imge_satz(&b, 0, 2, 0, 3);
        pruefe("3a Spurkoepfe ausserhalb 84 x 2: die Datei bleibt lesbar",
               parse(&b, &d) == IPF_AIR_OK);
        pruefe("3b ... und genau 2 werden als verworfen GEZAEHLT",
               d && ipf_air_get_dropped_images(d) == 2u);
        if (d) weg(d);
        free(b.p);

        /* 600 IMGE-Saetze; der 600. (Schluessel 600) bekommt als einziger
         * einen DATA-Satz mit einem Block. */
        memset(&b, 0, sizeof b);
        block_deskriptor(leer);
        memcpy(leer + 32, "UFT!", 4);
        caps_satz(&b);
        info_satz(&b, 1, 0);
        {
            uint32_t k;
            for (k = 1; k < 600; k++) imge_satz(&b, 0, 0, 0, k);
        }
        imge_satz(&b, 1, 0, 1, 600);
        data_satz(&b, 600, leer, sizeof leer);
        d = NULL;
        pruefe("3c 600 Spurkoepfe: die Datei wird gelesen",
               parse(&b, &d) == IPF_AIR_OK);
        pruefe("3d ... und der DATA-Satz findet den 600. Spurkopf",
               d && ipf_air_get_block_count(d, 1, 0) == 1);
        pruefe("3e ... ohne dass ein Spurkopf verworfen wurde",
               d && ipf_air_get_dropped_images(d) == 0u);
        if (d) weg(d);
        free(b.p);
    }

    printf("== Befund 4 (MF-1373): ein Element ueber das Nutzlastende ==\n");
    {
        /* Ein Datenelement sagt 100 Byte an, die Nutzlast traegt 10. Vor
         * MF-1373 blieb ein uninitialisierter 100-Byte-Block als „Wert“
         * stehen, und die Zellsummen-Gleichung konnte ihn nicht sehen,
         * weil die Laenge stimmte. Seit MF-1373 bricht die Zerlegung ab. */
        puffer_t b = {0};
        ipf_air_disk_t *d = NULL;
        uint8_t nutz[32 + 2 + 10];
        memset(nutz, 0, sizeof nutz);
        be32_setzen(nutz + 0, 1600);   /* Datenbits */
        be32_setzen(nutz + 16, 1);     /* MFM */
        be32_setzen(nutz + 28, 32);    /* Datenversatz hinter dem Deskriptor */
        nutz[32] = 0x22;               /* Typ 2 (Daten), 1 Zaehlbyte */
        nutz[33] = 100;                /* 100 Byte angesagt */
        memcpy(nutz + 34, "UFT-K 0123", 10);
        /* SPS-Kodierer (2): dort hat auch der Stand VOR MF-1373 Elemente
         * zerlegt — mit dem CAPS-Kodierer waere 4b dort leer erfuellt. */
        uint32_t info_sps[21];
        memset(info_sps, 0, sizeof info_sps);
        info_sps[0] = 1; info_sps[1] = 2; info_sps[2] = 1; info_sps[12] = 2;
        caps_satz(&b);
        satz(&b, "INFO", info_sps, 21);
        imge_satz(&b, 0, 0, 1, 1);
        data_satz(&b, 1, nutz, sizeof nutz);
        pruefe("4a die Datei selbst bleibt lesbar", parse(&b, &d) == IPF_AIR_OK);
        pruefe("4b das Element ueber das Nutzlastende wird NICHT angelegt",
               d && ipf_air_get_elem_count(d, 0, 0, 0) == 0);
        if (d) weg(d);
        free(b.p);
    }

    printf("test_ipf_satzlauf_schranken: %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
