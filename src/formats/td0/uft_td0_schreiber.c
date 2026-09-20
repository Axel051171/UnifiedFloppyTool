/**
 * @file uft_td0_schreiber.c
 * @brief Schreibt eine GEPACKTE TD0 (Advanced Compression) — MF-1297.
 *
 * AUFRUFER: `tests/test_td0_schreiber.c` (der Rotbeweis und der
 *   Korpus-Erzeuger). Kein Produktivpfad — das steht so im Bericht.
 * BERUEHRTE API: neu `uft_td0_schreibe_gepackt()`; ruft `uft_td0_crc()`
 *   (MF-1296) und `uft_td0_lzhuf_packen()` (MF-1297).
 * DATENSCHEMA: `uft_td0_schreibsatz_t` — Geometrie, flaches Sektorabbild,
 *   optionaler Kommentar mit Datum.
 * ANWEISUNG (woertlich): "ja mach es fertig Schreiber selbst (G6) und
 *   Korpus >= 3 (G7)."
 *
 * ── Warum UFT das selbst schreibt ────────────────────────────────────
 *
 * Fuer die Advanced Compression gibt es im Baum keinen fremden
 * SCHREIBER, und das ist gemessen, nicht angenommen:
 *
 *   fluxfox `src/file_parsers/td0.rs:228`
 *       can_write() -> ParserWriteCompatibility::UnsupportedFormat
 *   libdsk  `lib/drvtele.c`
 *       schreibt TD0, aber unkomprimiert
 *
 * Bemerkenswert und ausdruecklich festgehalten: fluxfox hat sehr wohl
 * einen LZHUF-**Kompressor** (`compression/lzhuf/lzhuf.rs:215
 * pub fn compress`, MIT, aus dfgordons `retrocompressor`) — ihm fehlt
 * nur der Behaelter. Die Aussage „es gibt keinen fremden Schreiber"
 * gilt also fuer den BEHAELTER, nicht fuer die Kompression.
 *
 * Der Weg ist deshalb der aus Regel E-14: UFT schreibt es selbst und
 * laesst ZWEI fremde Leser urteilen. Der eigene Leser zaehlt dabei
 * nicht — Packer und Entpacker aus derselben Hand koennen gemeinsam
 * falsch liegen (MF-1009, `apridisk`).
 *
 * ── Eine Bauvorschrift, die aus einer Messung kommt ──────────────────
 *
 * libdsk hat fuer TD0 **kein eigenes `tele_getgeom`** und nimmt die
 * Geometrie aus dem **BPB im Bootsektor**. Gemessen an fluxfox'
 * `sector_test_360k.td0`, deren Bootsektor komplett null ist: libdsk
 * meldet 40 x 1 x 8 und schreibt 163 840 statt 368 640 Byte — mehr als
 * die halbe Diskette. An `libdsk_uftk_pc720.td0`, deren Bootsektor
 * einen echten BPB traegt, meldet es 80 x 2 x 9, richtig.
 *
 * **Wer hier eine TD0 baut, die libdsk beurteilen soll, legt einen
 * gueltigen FAT-BPB in Sektor 1 von Spur 0.** Das ist Sache des
 * Aufrufers; dieser Schreiber prueft es nicht, er sagt es nur.
 *
 * ── Aufbau der Datei ─────────────────────────────────────────────────
 *
 *   12 Byte Dateikopf, IMMER UNGEPACKT
 *       'td'  Kennung fuer Advanced Compression ('TD' waere normal/RLE)
 *       [10..11] CRC-16 ueber die Byte 0..9
 *   danach: EIN LZHUF-Strom, der alles Weitere enthaelt —
 *       optionaler Kommentarblock (CRC, Laenge, Datum, Text)
 *       je Spur:  [nsec, cyl, head, crc8]
 *           je Sektor: [c, h, r, n, flags, daten-crc8]
 *                      [len_lo, len_hi, verfahren, Nutzlast]
 *       Endemarke 0xFF
 *
 * Alle drei Pruefsummen werden GESCHRIEBEN, nicht auf 0 gelassen: seit
 * MF-1296 rechnet UFTs eigener Leser sie nach, und eine Pruefdatei, die
 * ihre eigene Zusage nicht halten kann, ist keine.
 *
 * Quellen fuer die Feldlagen, beide nur GELESEN (Kanal *Spec*, MF-695):
 *   SAMdisk `src/samdisk/td0.cpp`   (MIT, im Baum)
 *   libdsk  `lib/drvtele.c`         (LGPL-2+, geklont)
 */

#include "uft_td0.h"
#include "uft/uft_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int uft_td0_lzhuf_packen(const uint8_t *daten, size_t len,
                         uint8_t *aus, size_t aus_kap, size_t *aus_len);

/* ── Strombauer ────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *b;
    size_t   len;
    size_t   kap;
    bool     voll;
} strom_t;

static void s_byte(strom_t *s, uint8_t v)
{
    if (s->len >= s->kap) { s->voll = true; return; }
    s->b[s->len++] = v;
}

static void s_le16(strom_t *s, uint16_t v)
{
    s_byte(s, (uint8_t)(v & 0xFFu));
    s_byte(s, (uint8_t)(v >> 8));
}

static void s_block(strom_t *s, const uint8_t *q, size_t n)
{
    for (size_t i = 0; i < n; i++) s_byte(s, q[i]);
}

/* Ist der Sektor ein durchgehend wiederholtes 2-Byte-Muster?
 *
 * Dann traegt TD0-Verfahren 1 ihn in FUENF Byte statt in 512. Das ist
 * die Stelle, an der TD0 seine Wiederholungen abfaengt — der
 * LZHUF-Packer dahinter muss sie gar nicht erst sehen. */
static bool ist_muster(const uint8_t *d, size_t n, uint8_t *p0, uint8_t *p1)
{
    if (n < 2u || (n & 1u) != 0u) return false;
    *p0 = d[0];
    *p1 = d[1];
    for (size_t i = 2; i < n; i += 2)
        if (d[i] != *p0 || d[i + 1] != *p1) return false;
    return true;
}

int uft_td0_schreibe_gepackt(const uft_td0_schreibsatz_t *satz,
                             const char *pfad)
{
    if (!satz || !satz->daten || !pfad) return UFT_ERR_INVALID_ARG;
    if (satz->zylinder == 0u || satz->koepfe == 0u ||
        satz->sektoren_je_spur == 0u || satz->sektorgroesse == 0u)
        return UFT_ERR_INVALID_ARG;
    if (satz->sektoren_je_spur > 255u) return UFT_ERR_INVALID_ARG;

    /* Groessenkode: 128 << kode == sektorgroesse */
    uint8_t groessenkode = 0;
    {
        unsigned g = 128u;
        while (g < satz->sektorgroesse && groessenkode < 7u) {
            g <<= 1; groessenkode++;
        }
        if (g != satz->sektorgroesse) return UFT_ERR_INVALID_ARG;
    }

    const size_t nutz = (size_t)satz->zylinder * satz->koepfe *
                        satz->sektoren_je_spur * satz->sektorgroesse;

    /* Reichlich: jeder Sektor im schlechtesten Fall roh plus Koepfe. */
    const size_t kap = nutz + (size_t)satz->zylinder * satz->koepfe *
                              (4u + (size_t)satz->sektoren_je_spur * 16u)
                       + 1024u;

    strom_t s;
    memset(&s, 0, sizeof(s));
    s.b = (uint8_t *)malloc(kap);
    if (!s.b) return UFT_ERR_MEMORY;
    s.kap = kap;

    /* ── Kommentarblock (liegt IM gepackten Strom) ─────────────────── */
    if (satz->kommentar && satz->kommentar[0]) {
        const size_t klen = strlen(satz->kommentar);
        if (klen > 0xFFFFu) { free(s.b); return UFT_ERR_INVALID_ARG; }

        /* Die CRC deckt alles AB dem Laengenfeld: Laenge, Datum, Text.
         * Gemessen an `libdsk_uftk_pc720.td0`: gespeichert 0xFEF2,
         * gerechnet ueber genau diese Spanne 0xFEF2. */
        uint8_t kopf[8];
        kopf[0] = (uint8_t)(klen & 0xFFu);
        kopf[1] = (uint8_t)(klen >> 8);
        kopf[2] = satz->jahr;      /* roh: Jahre seit 1900, wie libdsk schreibt */
        kopf[3] = satz->monat;     /* roh: 0-basiert (MF-1285) */
        kopf[4] = satz->tag;
        kopf[5] = satz->stunde;
        kopf[6] = satz->minute;
        kopf[7] = satz->sekunde;

        uint16_t crc = uft_td0_crc(kopf, sizeof(kopf), 0u);
        crc = uft_td0_crc((const uint8_t *)satz->kommentar, klen, crc);

        s_le16(&s, crc);
        s_block(&s, kopf, sizeof(kopf));
        s_block(&s, (const uint8_t *)satz->kommentar, klen);
    }

    /* ── Spuren ───────────────────────────────────────────────────── */
    for (unsigned c = 0; c < satz->zylinder && !s.voll; c++) {
        for (unsigned h = 0; h < satz->koepfe && !s.voll; h++) {
            uint8_t th[4];
            th[0] = (uint8_t)satz->sektoren_je_spur;
            th[1] = (uint8_t)c;
            th[2] = (uint8_t)h;
            th[3] = (uint8_t)(uft_td0_crc(th, 3u, 0u) & 0xFFu);
            s_block(&s, th, 4u);

            for (unsigned r = 0; r < satz->sektoren_je_spur && !s.voll; r++) {
                const size_t off =
                    (((size_t)c * satz->koepfe + h) * satz->sektoren_je_spur + r)
                    * satz->sektorgroesse;
                const uint8_t *sd = satz->daten + off;

                uint8_t sh[6];
                sh[0] = (uint8_t)c;
                sh[1] = (uint8_t)h;
                sh[2] = (uint8_t)(r + 1u);      /* TD0 zaehlt ab 1 */
                sh[3] = groessenkode;
                sh[4] = 0;                       /* keine Fehlerflaggen */
                sh[5] = (uint8_t)(uft_td0_crc(sd, satz->sektorgroesse, 0u)
                                  & 0xFFu);
                s_block(&s, sh, 6u);

                uint8_t p0 = 0, p1 = 0;
                if (ist_muster(sd, satz->sektorgroesse, &p0, &p1)) {
                    s_le16(&s, 5u);             /* Verfahrensbyte + 4 */
                    s_byte(&s, 1u);             /* Verfahren 1: Muster */
                    s_le16(&s, (uint16_t)(satz->sektorgroesse / 2u));
                    s_byte(&s, p0);
                    s_byte(&s, p1);
                } else {
                    s_le16(&s, (uint16_t)(satz->sektorgroesse + 1u));
                    s_byte(&s, 0u);             /* Verfahren 0: roh */
                    s_block(&s, sd, satz->sektorgroesse);
                }
            }
        }
    }
    s_byte(&s, 0xFFu);                          /* Endemarke */

    if (s.voll) { free(s.b); return UFT_ERR_BUFFER_TOO_SMALL; }

    /* ── Packen ───────────────────────────────────────────────────── */
    const size_t gkap = s.len * 2u + 4096u;
    uint8_t *gepackt = (uint8_t *)malloc(gkap);
    if (!gepackt) { free(s.b); return UFT_ERR_MEMORY; }

    size_t glen = 0;
    int rc = uft_td0_lzhuf_packen(s.b, s.len, gepackt, gkap, &glen);
    free(s.b);
    if (rc != UFT_OK) { free(gepackt); return rc; }

    /* ── Dateikopf, IMMER ungepackt ───────────────────────────────── */
    uint8_t kopf[12];
    kopf[0] = 't';                  /* Kleinschreibung = Advanced Compression */
    kopf[1] = 'd';
    kopf[2] = 0;                    /* sequence */
    kopf[3] = 0;                    /* checksig */
    kopf[4] = 21;                   /* version 2.1, wie fluxfox' Datei */
    kopf[5] = satz->data_rate;
    kopf[6] = satz->drive_type;
    kopf[7] = (satz->kommentar && satz->kommentar[0]) ? 0x80u : 0x00u;
    kopf[8] = 0;                    /* dos_flag */
    kopf[9] = (uint8_t)satz->koepfe;
    {
        const uint16_t kcrc = uft_td0_crc(kopf, 10u, 0u);
        kopf[10] = (uint8_t)(kcrc & 0xFFu);
        kopf[11] = (uint8_t)(kcrc >> 8);
    }

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(gepackt); return UFT_ERR_IO; }
    bool ok = (fwrite(kopf, 1, sizeof(kopf), f) == sizeof(kopf)) &&
              (fwrite(gepackt, 1, glen, f) == glen);
    if (fclose(f) != 0) ok = false;
    free(gepackt);

    return ok ? UFT_OK : UFT_ERR_IO;
}
