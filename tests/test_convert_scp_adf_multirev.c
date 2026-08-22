/**
 * @file test_convert_scp_adf_multirev.c
 * @brief SCP -> ADF: alle Umdrehungen zaehlen, und was sie ueber den Sektor
 *        aussagen (MF-473).
 *
 * Eine SCP-Aufnahme enthaelt bis zu fuenf Umdrehungen derselben Spur. Der
 * Wandler benutzte davon die erste; das SCP-Plugin sagt es in seinem eigenen
 * Kommentar ("Fuer bessere Ergebnisse koennte man alle Revolutions
 * kombinieren"). Alles nach der ersten Umdrehung war damit gemessene,
 * gespeicherte Information ohne jede Wirkung.
 *
 * Parallel dazu lag seit MF-473 eine Klassifikation im Baum
 * (`multiread_class_t` in src/recovery/uft_multiread_pipeline.c) — mit Test,
 * ohne einen einzigen Aufrufer in src/. Dieser Test prueft beides in einem
 * Zug, weil es dieselbe Sache ist: mehrere Lesungen desselben Sektors
 * auszuwerten heisst, sagen zu koennen WIE sie sich zueinander verhalten.
 *
 * ── Warum synthetischer Flux und kein Korpus ─────────────────────────────
 *
 * tests/test_convert_scp_adf.c deckt denselben Pfad gegen eine echte
 * Greaseweazle-Aufnahme ab — und SKIPt in CI, weil die Datei 32 MB gross und
 * gitignored ist. Ein Test, der nur lokal laeuft, schuetzt die Verdrahtung
 * nicht. Also wird hier eine AmigaDOS-Spur erzeugt.
 *
 * Der Kodierer unten ist die exakte Umkehrung von `decode_amiga_sector()`
 * (src/flux/uft_flux_decoder.c:1204) und pruefte sich selbst: laeuft
 * `the_encoder_produces_a_track_the_decoder_reads` gruen, stimmt er — der
 * Dekoder ist gegen die echte Aufnahme belegt (MF-438). Ein Kodierer, den der
 * belegte Dekoder liest, ist damit kein zweites unbelegtes Stueck Code.
 *
 * ── Die Rotproben ────────────────────────────────────────────────────────
 *
 *   1. Umdrehung 0 traegt einen kaputten Sektor, 1 und 2 nicht. Nur wer alle
 *      drei liest, bekommt die Diskette vollstaendig. Mit der alten Fassung
 *      (nur Umdrehung 0) fehlt genau dieser eine Sektor.
 *   2. Derselbe Sektor ist in ALLEN Umdrehungen gleich falsch. Das ist kein
 *      Schaden, sondern ein absichtlich falsch aufgezeichneter Kopierschutz;
 *      er darf nicht als weak gemeldet werden und nochmal lesen hilft nicht.
 *   3. Der Sektor liest sich jedes Mal anders — das ist weak.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* ──────────────────────────────────────────────────────────────────────────
 * Geometrie
 * ────────────────────────────────────────────────────────────────────────*/

#define ADF_SPT        11
#define ADF_SECSZ      512
#define ADF_SIZE       901120u          /* 80 x 2 x 11 x 512 */
#define AMIGA_CELL_NS  2000u
/* Eine AmigaDOS-DD-Umdrehung: 500 kbit/s bei 300 U/min = 100000 Zellen.
 * Genau diese Zahl macht die gemessene Umdrehung (200 ms) und die nominale
 * Zellendauer (2 us) deckungsgleich — der MF-475-Pfad waehlt dann 2000 ns. */
#define AMIGA_CELLS_PER_REV  100000u
#define AMIGA_REV_NS   (AMIGA_CELLS_PER_REV * AMIGA_CELL_NS)   /* 200 ms */

/* ──────────────────────────────────────────────────────────────────────────
 * Amiga-MFM-Kodierer — exakte Umkehrung von decode_amiga_sector()
 * ────────────────────────────────────────────────────────────────────────*/

typedef struct {
    uint8_t *cells;        /* MSB-first Zellenstrom */
    size_t   cap;          /* Kapazitaet in Zellen */
    size_t   n;            /* belegte Zellen */
    int      prev;         /* letztes DATENbit, fuer die MFM-Taktregel */
} cellbuf_t;

static void cell_put(cellbuf_t *c, int bit)
{
    if (c->n >= c->cap) return;
    if (bit) c->cells[c->n / 8] |= (uint8_t)(0x80u >> (c->n % 8));
    c->n++;
}

/* Ein Datenbit anhaengen: Taktbit nach MFM-Regel (1 nur zwischen zwei
 * Null-Datenbits), dann das Datenbit selbst. */
static void mfm_put_data_bit(cellbuf_t *c, int d)
{
    cell_put(c, (!c->prev && !d) ? 1 : 0);
    cell_put(c, d);
    c->prev = d;
}

/* Ein rohes 16-Zellen-Wort anhaengen — fuer die Sync-Marke 0x4489, deren
 * fehlendes Taktbit sie ueberhaupt erst zur Marke macht. */
static void cell_put_word(cellbuf_t *c, uint16_t w)
{
    for (int i = 15; i >= 0; i--) cell_put(c, (w >> i) & 1);
    c->prev = w & 1;                       /* letzte Zelle ist ein Datenbit */
}

/* Fuellmuster 0xAA: abwechselnd, also lauter Datennullen mit Takt. */
static void cell_put_gap(cellbuf_t *c, size_t cells)
{
    for (size_t i = 0; i < cells; i++) mfm_put_data_bit(c, 0);
}

/**
 * Ein Odd/Even-Feld schreiben und in die Pruefsumme falten.
 *
 * Auf der Diskette stehen erst @p nbytes Rohbytes mit den UNGERADEN
 * Datenbits, dann @p nbytes mit den GERADEN; der Dekoder setzt sie als
 * `((odd & 0x55) << 1) | (even & 0x55)` zusammen. Also ist das Rohbyte der
 * ungeraden Haelfte `(D >> 1) & 0x55` und das der geraden `D & 0x55`.
 *
 * Die Pruefsumme ist das XOR der big-endian Rohbyte-Longs, am Ende mit
 * 0x55555555 maskiert. Weil die Maske genau die Taktbits wegnimmt, darf hier
 * mit taktfreien Rohbytes gerechnet werden: (a^b) & M == (a&M) ^ (b&M).
 */
static void amiga_put_field(cellbuf_t *c, const uint8_t *data, size_t nbytes,
                            uint32_t *csum)
{
    for (int half = 0; half < 2; half++) {
        uint32_t acc = 0;
        int      acc_n = 0;
        for (size_t j = 0; j < nbytes; j++) {
            uint8_t rb = (half == 0) ? (uint8_t)((data[j] >> 1) & 0x55u)
                                     : (uint8_t)(data[j] & 0x55u);
            for (int b = 3; b >= 0; b--)
                mfm_put_data_bit(c, (rb >> (2 * b)) & 1);
            if (csum) {
                acc = (acc << 8) | rb;
                if (++acc_n == 4) { *csum ^= acc; acc = 0; acc_n = 0; }
            }
        }
        if (csum && acc_n) {                 /* nbytes nicht durch 4 teilbar */
            acc <<= 8 * (4 - acc_n);
            *csum ^= acc;
        }
    }
}

static void be32(uint8_t out[4], uint32_t v)
{
    out[0] = (uint8_t)(v >> 24); out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >>  8); out[3] = (uint8_t)v;
}

/**
 * Einen AmigaDOS-Sektor anhaengen.
 *
 * @param force_bad_dchk  wenn true, wird die Datenpruefsumme absichtlich
 *                        falsch geschrieben — genau das, was ein
 *                        Kopierschutz auf die Diskette bringt.
 */
static void amiga_put_sector(cellbuf_t *c, uint8_t track, uint8_t sec,
                             const uint8_t *data, bool force_bad_dchk)
{
    cell_put_gap(c, 32);                     /* Vorspann */
    cell_put_word(c, 0x4489);                /* Amiga schreibt zwei Marken */
    cell_put_word(c, 0x4489);

    uint8_t info[4]  = { 0xFF, track, sec, (uint8_t)(ADF_SPT - sec) };
    uint8_t label[16]; memset(label, 0, sizeof(label));

    /* Kopf- und Datenpruefsumme muessen VOR dem Schreiben feststehen, weil
     * sie zwischen Label und Daten im Strom liegen. Also erst trocken
     * rechnen (cellbuf mit cap 0 schluckt die Zellen), dann schreiben. */
    cellbuf_t dry = { NULL, 0, 0, 0 };
    uint32_t hdr = 0, dat = 0;
    amiga_put_field(&dry, info,  4,  &hdr);
    amiga_put_field(&dry, label, 16, &hdr);
    amiga_put_field(&dry, data,  ADF_SECSZ, &dat);

    uint8_t hchk[4], dchk[4];
    be32(hchk, hdr & 0x55555555u);
    be32(dchk, (dat ^ (force_bad_dchk ? 0x00540000u : 0u)) & 0x55555555u);

    amiga_put_field(c, info,  4,  NULL);
    amiga_put_field(c, label, 16, NULL);
    amiga_put_field(c, hchk,  4,  NULL);
    amiga_put_field(c, dchk,  4,  NULL);
    amiga_put_field(c, data,  ADF_SECSZ, NULL);
}

/* Wie ein Sektor dieser Umdrehung aussehen soll. */
typedef struct {
    int  sector;          /* 0..10, oder -1 fuer "gilt fuer keinen" */
    bool bad_checksum;    /* Datenpruefsumme absichtlich falsch */
    uint8_t overwrite;    /* wenn != 0: Datenbyte 0 durch diesen Wert ersetzen */
} sector_defect_t;

/**
 * Eine ganze AmigaDOS-Spur als Zellenstrom bauen.
 *
 * @param adf     Quell-ADF, aus dem die 11 Sektoren stammen
 * @param track   AmigaDOS-Spurnummer (cyl*2 + head)
 * @param defect  Defekt fuer genau eine Sektorposition (sector = -1: keiner)
 */
static void build_track_cells(cellbuf_t *c, const uint8_t *adf, uint8_t track,
                              const sector_defect_t *defect)
{
    memset(c->cells, 0, (c->cap + 7) / 8);
    c->n = 0; c->prev = 0;

    for (int s = 0; s < ADF_SPT; s++) {
        const uint8_t *src = adf + ((size_t)track * ADF_SPT + (size_t)s) * ADF_SECSZ;
        uint8_t buf[ADF_SECSZ];
        memcpy(buf, src, ADF_SECSZ);

        bool bad = false;
        if (defect && defect->sector == s) {
            bad = defect->bad_checksum;
            if (defect->overwrite) buf[0] = defect->overwrite;
        }
        amiga_put_sector(c, track, (uint8_t)s, buf, bad);
    }
    /* Auf die volle Umdrehung auffuellen — 100000 Zellen sind das, was eine
     * AmigaDOS-DD-Spur bei 300 U/min traegt. */
    while (c->n + 1 < c->cap) mfm_put_data_bit(c, 0);
}

/* Zellenstrom -> ns-Intervalle zwischen den Flusswechseln. */
static size_t cells_to_intervals(const cellbuf_t *c, uint32_t *out, size_t cap)
{
    size_t n = 0, last = 0;
    for (size_t i = 0; i < c->n && n < cap; i++) {
        if (!((c->cells[i / 8] >> (7 - (i % 8))) & 1)) continue;
        out[n++] = (uint32_t)((i - last) * AMIGA_CELL_NS);
        last = i;
    }
    if (n > 0 && out[0] == 0) out[0] = AMIGA_CELL_NS;   /* Zelle 0 gesetzt */
    return n;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Testgeruest
 * ────────────────────────────────────────────────────────────────────────*/

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_mrev_%s_%d", dir, tag, rand() % 100000);
}

/* Zwei Spuren (C0H0, C0H1) mit erkennbarem, nicht konstantem Inhalt. */
#define NTRACKS 2
static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, ADF_SIZE);
    if (!adf) return NULL;
    for (size_t i = 0; i < (size_t)NTRACKS * ADF_SPT * ADF_SECSZ; i++)
        adf[i] = (uint8_t)(0x40u + ((i * 7u + (i >> 9)) & 0x3Fu));
    return adf;
}

#define MAX_INTERVALS  AMIGA_CELLS_PER_REV

/**
 * Eine SCP-Datei mit @p nrevs Umdrehungen je Spur schreiben.
 *
 * @param defect_per_rev  Defekt je Umdrehung (NULL-Eintrag = saubere
 *                        Umdrehung). Nur Spur 0 bekommt Defekte; Spur 1
 *                        bleibt sauber und dient als Gegenprobe.
 */
static int write_scp(const char *path, const uint8_t *adf, int nrevs,
                     const sector_defect_t *const *defect_per_rev)
{
    uint8_t *cellbits = (uint8_t *)calloc((AMIGA_CELLS_PER_REV + 7) / 8 + 1, 1);
    uint32_t *iv = (uint32_t *)malloc(MAX_INTERVALS * sizeof(uint32_t));
    if (!cellbits || !iv) { free(cellbits); free(iv); return -1; }

    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, (uint8_t)nrevs);
    if (!w) { free(cellbits); free(iv); return -1; }

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++) {
        for (int r = 0; r < nrevs && rc == 0; r++) {
            cellbuf_t c = { cellbits, AMIGA_CELLS_PER_REV, 0, 0 };
            const sector_defect_t *d =
                (track == 0 && defect_per_rev) ? defect_per_rev[r] : NULL;
            build_track_cells(&c, adf, (uint8_t)track, d);

            size_t n = cells_to_intervals(&c, iv, MAX_INTERVALS);
            rc = scp_writer_add_track(w, track / 2, track % 2, iv, n,
                                      AMIGA_REV_NS, r);
        }
    }
    if (rc == 0) rc = scp_writer_save(w, path);

    scp_writer_free(w);
    free(cellbits);
    free(iv);
    return rc;
}

/* SCP -> ADF ueber die oeffentliche Wandler-API. */
static uft_error_t convert(const char *scp, const char *adf_out,
                           bool use_multiple_revs, uft_convert_result_t *res)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.use_multiple_revs = use_multiple_revs;
    /* Flux -> Sektor verliert Timing und Weak-Bits; der Preflight verlangt
     * dafuer ausdrueckliche Zustimmung (UFT-A05). Hier ist sie gegeben. */
    o.accept_data_loss = true;
    memset(res, 0, sizeof(*res));
    uft_error_t rc = uft_convert_file(scp, adf_out, UFT_FORMAT_ADF, &o, res);
    if (rc != UFT_OK) {
        printf("\n        convert rc=%d error=%d\n", (int)rc, (int)res->error);
        for (int i = 0; i < res->warning_count; i++)
            printf("        warn: %s\n", res->warnings[i]);
    }
    return rc;
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    *len = fread(b, 1, (size_t)n, f);
    fclose(f);
    return b;
}

static bool warned_about(const uft_convert_result_t *r, const char *needle)
{
    for (int i = 0; i < r->warning_count; i++)
        if (strstr(r->warnings[i], needle)) return true;
    return false;
}

static void dump_warnings(const uft_convert_result_t *r)
{
    for (int i = 0; i < r->warning_count; i++)
        printf("\n        warn: %s", r->warnings[i]);
    printf("\n");
}

/* Wie viele der NTRACKS x 11 Sektoren stimmen mit der Quelle ueberein? */
static int sectors_matching(const uint8_t *want, const uint8_t *got)
{
    int ok = 0;
    for (int i = 0; i < NTRACKS * ADF_SPT; i++) {
        size_t off = (size_t)i * ADF_SECSZ;
        if (memcmp(want + off, got + off, ADF_SECSZ) == 0) ok++;
    }
    return ok;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Tests
 * ────────────────────────────────────────────────────────────────────────*/

TEST(the_encoder_produces_a_track_the_decoder_reads) {
    /* Selbstpruefung des Kodierers: was er schreibt, muss der belegte
     * AmigaDOS-Dekoder als 11 saubere Sektoren zurueckgeben. Faellt dieser
     * Test, sagen alle folgenden nichts ueber den Wandler aus. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    uint8_t *cellbits = (uint8_t *)calloc((AMIGA_CELLS_PER_REV + 7) / 8 + 1, 1);
    uint32_t *iv = (uint32_t *)malloc(MAX_INTERVALS * sizeof(uint32_t));
    ASSERT(cellbits != NULL && iv != NULL);

    cellbuf_t c = { cellbits, AMIGA_CELLS_PER_REV, 0, 0 };
    build_track_cells(&c, adf, 0, NULL);
    ASSERT(c.n > 90000);                       /* eine volle Umdrehung */

    size_t n = cells_to_intervals(&c, iv, MAX_INTERVALS);
    ASSERT(n > 20000);

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, n, AMIGA_REV_NS, &raw)
           == FLUX_OK);
    ASSERT(raw.index_count == 2);              /* die Messung kam an */

    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    o.media = UFT_MEDIA_AMIGA_DD;

    flux_decoded_track_t dt;
    memset(&dt, 0, sizeof(dt));
    ASSERT(flux_decode_amiga(&raw, &dt, &o) == FLUX_OK);
    if (dt.sector_count != ADF_SPT)
        printf("\n        %zu Sektoren, %zu mit falscher Datenpruefsumme\n",
               (size_t)dt.sector_count, (size_t)dt.bad_data_crc);
    ASSERT(dt.sector_count == ADF_SPT);
    ASSERT(dt.bad_data_crc == 0);
    ASSERT(dt.bad_id_crc == 0);

    for (size_t s = 0; s < dt.sector_count; s++) {
        const flux_decoded_sector_t *sec = &dt.sectors[s];
        ASSERT(sec->sector < ADF_SPT);
        ASSERT(sec->data_size == ADF_SECSZ);
        ASSERT(memcmp(sec->data, adf + (size_t)sec->sector * ADF_SECSZ,
                      ADF_SECSZ) == 0);
    }

    flux_decoded_track_free(&dt);
    flux_raw_free(&raw);
    free(cellbits); free(iv); free(adf);
}

TEST(a_clean_capture_converts_to_the_source_adf) {
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "clean.scp");
    get_temp_path(out, sizeof(out), "clean.adf");
    ASSERT(write_scp(scp, adf, 3, NULL) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL);
    ASSERT(got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT);

    free(got); free(adf);
    remove(scp); remove(out);
}

TEST(a_sector_broken_in_revolution_zero_is_recovered_from_the_others) {
    /* DIE Rotprobe fuer die Verdrahtung. Umdrehung 0 traegt einen Sektor mit
     * falscher Datenpruefsumme und veraendertem Inhalt, die Umdrehungen 1 und
     * 2 sind sauber. Wer nur Umdrehung 0 liest, verliert diesen Sektor. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d = { 3, true, 0x5A };
    const sector_defect_t *per_rev[3] = { &d, NULL, NULL };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "rev0.scp");
    get_temp_path(out, sizeof(out), "rev0.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    /* Mit allen Umdrehungen: vollstaendig. */
    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);
    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    int matched = sectors_matching(adf, got);
    if (matched != NTRACKS * ADF_SPT) {
        printf("\n        %d von %d Sektoren stimmen\n",
               matched, NTRACKS * ADF_SPT);
        dump_warnings(&r);
    }
    ASSERT(matched == NTRACKS * ADF_SPT);
    ASSERT(r.sectors_converted == NTRACKS * ADF_SPT);
    free(got);

    /* Mit `use_multiple_revs = false` ausdruecklich nur die erste: der
     * Sektor fehlt. Das ist zugleich der Beleg, dass der Unterschied wirklich
     * von den weiteren Umdrehungen kommt und nicht von etwas anderem. */
    uft_convert_result_t r1;
    ASSERT(convert(scp, out, false, &r1) == UFT_OK);
    got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT - 1);
    ASSERT(r1.sectors_converted == NTRACKS * ADF_SPT - 1);
    free(got);

    free(adf);
    remove(scp); remove(out);
}

TEST(a_stable_crc_error_is_reported_as_copy_protection) {
    /* Derselbe Sektor, in jeder Umdrehung mit derselben falschen
     * Pruefsumme — ein absichtlich falsch aufgezeichneter Sektor. Er darf
     * nicht ins ADF, er ist nicht weak, und die Meldung muss sagen, dass
     * weiteres Lesen nichts aendert. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d = { 5, true, 0 };
    const sector_defect_t *per_rev[3] = { &d, &d, &d };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "stable.scp");
    get_temp_path(out, sizeof(out), "stable.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);

    /* Genau ein Sektor fehlt, und zwar Nummer 5 der Spur 0. */
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT - 1);
    ASSERT(memcmp(got + 5 * ADF_SECSZ, adf + 5 * ADF_SECSZ, ADF_SECSZ) != 0);
    ASSERT(r.sectors_converted == NTRACKS * ADF_SPT - 1);

    if (!warned_about(&r, "stabiler CRC-Fehler")) dump_warnings(&r);
    ASSERT(warned_about(&r, "stabiler CRC-Fehler"));
    ASSERT(warned_about(&r, "Kopierschutz"));
    ASSERT(!warned_about(&r, "Sektoren weak"));

    free(got); free(adf);
    remove(scp); remove(out);
}

TEST(a_sector_that_reads_differently_each_time_is_reported_as_weak) {
    /* Drei Umdrehungen, drei verschiedene Inhalte, jedes Mal falsche
     * Pruefsumme. Kein Kopierschutz — ein wackelnder Sektor. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d0 = { 7, true, 0x11 };
    sector_defect_t d1 = { 7, true, 0x22 };
    sector_defect_t d2 = { 7, true, 0x33 };
    const sector_defect_t *per_rev[3] = { &d0, &d1, &d2 };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "weak.scp");
    get_temp_path(out, sizeof(out), "weak.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT - 1);

    if (!warned_about(&r, "Sektoren weak")) dump_warnings(&r);
    ASSERT(warned_about(&r, "Sektoren weak"));
    ASSERT(!warned_about(&r, "stabiler CRC-Fehler"));

    free(got); free(adf);
    remove(scp); remove(out);
}

int main(void)
{
    printf("=== SCP -> ADF: alle Umdrehungen, und was sie sagen (MF-473) ===\n");

    /* Ohne Registrierung ist die Plugin-Registry leer und uft_convert_file()
     * erkennt die SCP-Datei nicht (MF-447). */
    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats()\n");
        return 1;
    }

    RUN(the_encoder_produces_a_track_the_decoder_reads);
    RUN(a_clean_capture_converts_to_the_source_adf);
    RUN(a_sector_broken_in_revolution_zero_is_recovered_from_the_others);
    RUN(a_stable_crc_error_is_reported_as_copy_protection);
    RUN(a_sector_that_reads_differently_each_time_is_reported_as_weak);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
