/**
 * @file test_external_integration.c
 * @brief KryoFlux stream decoding, against the SHIPPED decoder (MF-399).
 *
 * This file used to define its own stream constants and its own RPM formula and
 * assert against those. The real decoder in src/flux/uft_kryoflux_stream.c —
 * which had no test of its own — was never called.
 *
 * That matters more here than in most places: the KryoFlux stream is a raw
 * capture format, so a decoding error does not produce an error message, it
 * produces plausible-looking flux with wrong timings. Everything downstream
 * (PLL, sector decode, protection analysis) then works confidently on wrong
 * numbers.
 *
 * The stream grammar is taken from the decoder itself
 * (src/flux/uft_kryoflux_stream.c:46-53):
 *   0x00..0x07  Flux2   two bytes, value = (op << 8) | next
 *   0x08/09/0A  Nop1/2/3  skipped, but they do advance the stream position
 *   0x0B        Ovl16   adds 0x10000 to the NEXT flux value
 *   0x0C        Flux3   three bytes, value in bytes 1..2
 *   0x0D        OOB     type + LE16 size + payload; does NOT advance the
 *                       stream position, which is what index positions refer to
 *   0x0E..0xFF  Flux1   one byte, value = op
 */

#include "uft/flux/uft_kryoflux.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A KryoFlux capture ends with an EOF OOB (0x0D 0x0D, size sentinel 0x0D0D).
 * The decoder deliberately reports MISSING_END when it is absent — see the
 * dedicated test below — so every "this must succeed" case appends it. */
#define KF_EOF_BLOCK  0x0D, 0x0D, 0x0D, 0x0D

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(single_byte_flux_cells_are_decoded_in_order) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    /* Three Flux1 cells; 0x0E is the lowest value that is a flux cell. */
    const uint8_t data[] = { 0x0E, 0x40, 0xFF, KF_EOF_BLOCK };
    ASSERT(uft_kf_decode(&s, data, sizeof(data)) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.flux_count == 3);
    ASSERT(s.flux_values[0] == 0x0E);
    ASSERT(s.flux_values[1] == 0x40);
    ASSERT(s.flux_values[2] == 0xFF);

    uft_kf_free(&s);
}

TEST(flux2_and_flux3_encode_the_same_value) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    /* 0x0312 written as Flux2 (op 0x03 + 0x12) and as Flux3 (0x0C 0x03 0x12) */
    const uint8_t data[] = { 0x03, 0x12,  0x0C, 0x03, 0x12, KF_EOF_BLOCK };
    ASSERT(uft_kf_decode(&s, data, sizeof(data)) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.flux_count == 2);
    ASSERT(s.flux_values[0] == 0x0312);
    ASSERT(s.flux_values[1] == 0x0312);
    ASSERT(s.flux_values[0] == s.flux_values[1]);

    uft_kf_free(&s);
}

TEST(overflow_marker_extends_the_following_cell) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    /* Ovl16 twice, then a Flux1 of 0x20 -> 0x20000 + 0x20.
     * Long gaps between transitions are exactly what unformatted or damaged
     * areas look like, so dropping the overflow would silently shorten them. */
    const uint8_t data[] = { 0x0B, 0x0B, 0x20, KF_EOF_BLOCK };
    ASSERT(uft_kf_decode(&s, data, sizeof(data)) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.flux_count == 1);
    ASSERT(s.flux_values[0] == 0x20000u + 0x20u);

    uft_kf_free(&s);
}

TEST(overflow_applies_only_to_the_next_cell) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    const uint8_t data[] = { 0x0B, 0x20, 0x20, KF_EOF_BLOCK };
    ASSERT(uft_kf_decode(&s, data, sizeof(data)) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.flux_count == 2);
    ASSERT(s.flux_values[0] == 0x10000u + 0x20u);
    ASSERT(s.flux_values[1] == 0x20u);          /* not carried over */

    uft_kf_free(&s);
}

TEST(nop_bytes_are_skipped_without_producing_flux) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    /* Nop1, Nop2(+1 byte), Nop3(+2 bytes), then one real cell. */
    const uint8_t data[] = { 0x08, 0x09, 0xAA, 0x0A, 0xBB, 0xCC, 0x30, KF_EOF_BLOCK };
    ASSERT(uft_kf_decode(&s, data, sizeof(data)) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.flux_count == 1);
    ASSERT(s.flux_values[0] == 0x30);

    uft_kf_free(&s);
}

TEST(index_oob_blocks_are_recorded) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    /* One flux cell, then an INDEX OOB with a 12-byte payload. */
    uint8_t data[3 + 4 + 12 + 4];
    size_t n = 0;
    data[n++] = 0x30;                       /* Flux1 */
    data[n++] = 0x0D;                       /* OOB   */
    data[n++] = 0x02;                       /* type: INDEX */
    data[n++] = 12; data[n++] = 0;          /* LE16 size */
    memset(&data[n], 0, 12);
    data[n + 0] = 0x01;                     /* stream_pos     = 1 */
    data[n + 4] = 0x10;                     /* sample_counter     */
    data[n + 8] = 0x02;                     /* index_counter      */
    n += 12;
    data[n++] = 0x0D; data[n++] = 0x0D; data[n++] = 0x0D; data[n++] = 0x0D;

    ASSERT(uft_kf_decode(&s, data, n) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.flux_count == 1);
    ASSERT(s.index_count == 1);

    uft_kf_free(&s);
}

TEST(a_truncated_cell_is_reported_not_silently_dropped) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    /* Flux2 opcode with its second byte missing: the capture is incomplete and
     * the decoder must say so rather than return a short flux list as if it
     * were the whole track. */
    const uint8_t trunc2[] = { 0x30, 0x03 };
    ASSERT(uft_kf_decode(&s, trunc2, sizeof(trunc2)) != UFT_UFT_KF_STATUS_OK);

    /* Same for a Flux3 block cut short. */
    const uint8_t trunc3[] = { 0x30, 0x0C, 0x03 };
    ASSERT(uft_kf_decode(&s, trunc3, sizeof(trunc3)) != UFT_UFT_KF_STATUS_OK);

    /* and for an OOB header that does not fit */
    const uint8_t truncoob[] = { 0x30, 0x0D, 0x02 };
    ASSERT(uft_kf_decode(&s, truncoob, sizeof(truncoob)) != UFT_UFT_KF_STATUS_OK);

    uft_kf_free(&s);
}

TEST(flux_to_time_conversion_uses_the_sample_clock) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);
    ASSERT(s.sample_clock > 0.0);

    /* The conversion is flux * 1e6 / sample_clock microseconds; check the
     * relationship rather than a magic number, and that ns is 1000x us. */
    double us = uft_kf_flux_to_us(&s, 1000);
    double ns = uft_kf_flux_to_ns(&s, 1000);
    ASSERT(us > 0.0);
    ASSERT(ns > 999.0 * us && ns < 1001.0 * us);

    /* twice the flux must be twice the time */
    double us2 = uft_kf_flux_to_us(&s, 2000);
    ASSERT(us2 > 1.99 * us && us2 < 2.01 * us);

    uft_kf_free(&s);
}

TEST(decoder_rejects_degenerate_arguments) {
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    ASSERT(uft_kf_decode(&s, NULL, 16) != UFT_UFT_KF_STATUS_OK);
    ASSERT(uft_kf_decode(NULL, (const uint8_t *)"\x30", 1) != UFT_UFT_KF_STATUS_OK);

    /* an empty stream carries no EOF block either, so it is reported as
     * MISSING_END rather than as a clean empty capture */
    ASSERT(uft_kf_decode(&s, (const uint8_t *)"", 0) == UFT_UFT_KF_STATUS_MISSING_END);
    ASSERT(s.flux_count == 0);

    uft_kf_free(&s);
}

TEST(a_capture_without_stream_end_is_flagged_but_kept) {
    /* Exemplary behaviour worth pinning: a stream that simply stops is
     * suspicious (truncated container), so the decoder reports MISSING_END —
     * but it KEEPS the flux it already decoded instead of discarding it or
     * padding it. Nothing is fabricated, and the caller can treat the data as
     * marginal. */
    uft_kf_stream_t s;
    ASSERT(uft_kf_init(&s) == UFT_UFT_KF_STATUS_OK);

    const uint8_t no_end[] = { 0x0E, 0x40, 0xFF };
    ASSERT(uft_kf_decode(&s, no_end, sizeof(no_end)) == UFT_UFT_KF_STATUS_MISSING_END);
    ASSERT(s.flux_count == 3);                 /* flux preserved */
    ASSERT(s.flux_values[2] == 0xFF);

    uft_kf_free(&s);
}

/* ══════════════════════════════════════════════════════════════════════
 *  MF-960: der erste ECHTE Gerätestrom durch diesen Dekoder
 *
 *  Alles darüber ist gegen handgeschriebene Byte-Felder in DIESER Datei
 *  geprüft. Die sind gut — sie kodieren Protokollwissen, etwa dass Flux2
 *  und Flux3 denselben Wert liefern müssen —, aber sie sind DIESELBE
 *  HAND wie der Dekoder. Ein systematisches Missverständnis, das
 *  Spezifikationslesung und Code teilen, kann kein solcher Test fangen
 *  (MF-644/760, die fünfte Frage).
 *
 *  `tests/corpus/fluxfox_sector_test/track00.0.raw` ist die Aufnahme
 *  eines Greaseweazle 1.18 von einer echten 5,25″-360K-Diskette,
 *  abgelegt im Repositorium `dbalsom/fluxfox` unter MIT. Herkunft,
 *  Prüfsumme und Rechtelage: `tests/corpus_manifest/manifest.json`.
 *
 *  Was hier geprüft wird, ist deshalb nicht dasselbe wie oben:
 *
 *    1. Die KFInfo-Zeile — vom Gerät geschrieben, nicht von uns.
 *    2. Die Zahl der Index-Blöcke, unabhängig vorgezählt.
 *    3. Die UMDREHUNGSDAUER. Das ist die eigentliche Gegenprobe: 300
 *       U/min ist die Physik eines 5,25″-Laufwerks und steht nirgends
 *       im Dekoder. Wer die Abstände falsch aufsummiert, landet nicht
 *       bei 200 ms.
 *    4. Dieselbe Dauer über ZWEI verschiedene Felder — `rotation_time`
 *       (das der Dekoder aus dem Index-OOB liest) und die Summe der
 *       Flusswerte zwischen zwei Indizes. Sie müssen übereinstimmen,
 *       ohne dass eines aus dem anderen berechnet wird.
 *
 *  Ohne Korpus überspringt sich der Fall benannt (77 = SKIP).
 * ══════════════════════════════════════════════════════════════════════ */

#define SKIP_CODE 77

/* Der Korpuspfad wird aus `__FILE__` abgeleitet, nicht aus einer
 * `-D`-Definition im Bausystem.
 *
 * Grund, benannt: `tests/CMakeLists.txt` definiert
 * `UFT_CORPUS_RESTRICTED_DIR` je Test, und dieser Test hat sie nicht.
 * Sie nachzutragen wäre eine Zeile — die Datei war beim Schreiben
 * dieses Tests aber von einer parallelen Arbeit belegt, und fremde
 * Änderungen mitzucommitten ist keine Option. `__FILE__` ist im Bau
 * dieses Baums ABSOLUT (gemessen: der Übersetzer wird mit dem vollen
 * Quellpfad gerufen), also genügt es, hinter `tests/` abzuschneiden.
 *
 * GRENZE: ein Bau, der mit relativen Quellpfaden übersetzt, findet den
 * Korpus so nicht — dann überspringt sich der Fall, statt falsch zu
 * melden. Wer `UFT_CORPUS_RESTRICTED_DIR` für diesen Test nachträgt,
 * darf diesen Block ersatzlos streichen. */
static int korpus_pfad(char *out, size_t n, const char *rest)
{
    const char *f = __FILE__;
    const char *marke = NULL;
    for (const char *p = f; *p; p++)
        if ((p[0] == 't') && strncmp(p, "tests", 5) == 0 &&
            (p == f || p[-1] == '/' || p[-1] == '\\'))
            marke = p;
    if (!marke) return -1;
    size_t wurzel = (size_t)(marke - f);
    if (wurzel + strlen(rest) + 1 >= n) return -1;
    memcpy(out, f, wurzel);
    out[wurzel] = '\0';
    strncat(out, rest, n - strlen(out) - 1);
    return 0;
}

#define KF_ECHT "tests/corpus/fluxfox_sector_test/track00.0.raw"

/* Erwartungswerte. Jede Zahl hat eine Quelle:
 *   - KFINFO_TEIL  steht in der Datei, vom Gerät geschrieben
 *   - INDIZES      unabhängig vorgezählt, bevor der Dekoder lief
 *   - UMDR_MS      Physik: 300 U/min = 200 ms, Toleranz 1 %
 */
#define KFINFO_TEIL   "name=Greaseweazle, version=1.18"
#define INDIZES       4u
#define UMDR_MS_MIN   198.0
#define UMDR_MS_MAX   202.0

/* Wie ASSERT, aber in einer Funktion mit Rueckgabewert brauchbar. */
#define ASSERT2(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; free(buf); return 0; } } while (0)

static int echte_aufnahme(void)
{
    char pfad[1024];
    if (korpus_pfad(pfad, sizeof pfad, KF_ECHT) != 0) {
        printf("  [SKIP] Korpuspfad nicht ableitbar (__FILE__ relativ?)\n");
        return SKIP_CODE;
    }
    FILE *f = fopen(pfad, "rb");
    if (!f) {
        printf("  [SKIP] %s fehlt.\n", KF_ECHT);
        printf("         Beschaffung und Pruefsumme: "
               "tests/corpus_manifest/manifest.json\n");
        return SKIP_CODE;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)(n > 0 ? n : 1));
    if (!buf || n <= 0 || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f); free(buf);
        printf("FAIL: Korpusdatei nicht lesbar\n"); _fail++; return 0;
    }
    fclose(f);

    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) {
        free(buf); printf("FAIL: init\n"); _fail++; return 0;
    }

    printf("  [TEST] %-44s ... ", "echte_greaseweazle_aufnahme");
    int vorher = _fail;

    ASSERT2(uft_kf_decode(&s, buf, (size_t)n) == UFT_UFT_KF_STATUS_OK);

    /* 1 — die Kennzeile stammt vom Geraet. */
    ASSERT2(strstr(s.info_string, KFINFO_TEIL) != NULL);

    /* 2 — Zahl der Indexbloecke, unabhaengig vorgezaehlt. */
    ASSERT2(s.index_count == INDIZES);

    /* 3+4 — die Umdrehungsdauer, ueber ZWEI Felder. */
    ASSERT2(s.sample_clock > 0.0 && s.index_clock > 0.0);
    for (uint32_t i = 1; i < s.index_count; i++) {
        uint64_t takte = 0;
        for (uint32_t k = s.indexes[i - 1].flux_position;
             k < s.indexes[i].flux_position && k < s.flux_count; k++)
            takte += s.flux_values[k];

        const double aus_fluss = (double)takte / s.sample_clock * 1000.0;
        const double aus_feld  =
            (double)s.indexes[i].rotation_time / s.index_clock * 1000.0;

        ASSERT2(aus_fluss > UMDR_MS_MIN && aus_fluss < UMDR_MS_MAX);
        /* Die beiden Wege duerfen um hoechstens 0,1 % auseinanderliegen.
         * Keiner wird aus dem anderen berechnet. */
        const double d = aus_fluss - aus_feld;
        ASSERT2((d < 0 ? -d : d) < aus_fluss * 0.001);
    }

    if (_fail == vorher) { printf("OK\n"); _pass++; }
    uft_kf_free(&s);
    free(buf);
    return 0;
}

int main(void) {
    printf("=== KryoFlux stream decoder, real code (MF-399) ===\n");
    RUN(single_byte_flux_cells_are_decoded_in_order);
    RUN(a_capture_without_stream_end_is_flagged_but_kept);
    RUN(flux2_and_flux3_encode_the_same_value);
    RUN(overflow_marker_extends_the_following_cell);
    RUN(overflow_applies_only_to_the_next_cell);
    RUN(nop_bytes_are_skipped_without_producing_flux);
    RUN(index_oob_blocks_are_recorded);
    RUN(a_truncated_cell_is_reported_not_silently_dropped);
    RUN(flux_to_time_conversion_uses_the_sample_clock);
    RUN(decoder_rejects_degenerate_arguments);

    /* MF-960: der erste echte Geraetestrom. Fehlt der Korpus, wird der
     * GANZE Test uebersprungen — nicht dieser eine Fall stillschweigend
     * weggelassen. Ein Test, der ohne Korpus dasselbe „bestanden" meldet
     * wie mit, verschweigt den Unterschied. */
    const int kf = echte_aufnahme();

    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    if (_fail != 0) return 1;
    if (kf == SKIP_CODE) {
        printf("UEBERSPRUNGEN: die synthetischen Faelle liefen, der ECHTE\n"
               "Strom fehlt. Der Dekoder ist damit nur gegen die eigene\n"
               "Hand geprueft (MF-644, fuenfte Frage).\n");
        return SKIP_CODE;
    }
    return 0;
}
