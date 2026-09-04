/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_multiread_selbsttests_leben.c
 * @brief Die Selbsttests der Multiread-Pipeline, erstmals ausgefuehrt (MF-851).
 *
 * Die Faelle standen als `#ifdef UFT_UNIT_TESTS`-Block am Ende von
 * `src/recovery/uft_multiread_pipeline.c`. `UFT_UNIT_TESTS` wird im
 * ganzen Baum nirgends definiert — der Block wurde nie uebersetzt
 * (P3-89, aufgefallen beim Rotbeweis zu MF-845).
 *
 * ── Abgrenzung zu tests/test_multiread_kein_mischbyte.c ──────────────
 *
 * Jener Test bewacht EINE Zusage: dass das Voting keinen Sektor
 * erfindet, den keine Lesung enthielt (MF-845). Dieser hier deckt den
 * Rest der Schnittstelle ab — die zweite, davon unabhaengige API
 * `multiread_vote_buffers()`, die Fehlerpfade und den Bericht.
 *
 * Beide zusammen sind noch keine vollstaendige Abdeckung. Sie sind das,
 * was vorhanden war, plus das, was der Rotbeweis noetig machte.
 */
#include "uft/recovery/uft_multiread_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

TEST(mehrheit_entscheidet)
{
    /* `multiread_vote_buffers()` ist die zustandslose Schwester von
     * `multiread_execute()`: drei Puffer, ein abweichendes Byte, die
     * Mehrheit traegt. Hier ist die byteweise Abstimmung RICHTIG — es
     * gibt keine CRC-Aussage, die etwas anderes verlangen wuerde. */
    uint8_t buf1[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t buf2[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t buf3[] = { 0x01, 0x02, 0xFF, 0x04 };

    const uint8_t *buffers[] = { buf1, buf2, buf3 };
    size_t lengths[] = { 4, 4, 4 };

    uint8_t output[4];
    uint8_t confidence = 0;

    ASSERT(multiread_vote_buffers(buffers, lengths, 3, output, 4,
                                  &confidence) == MULTIREAD_OK);
    ASSERT(output[0] == 0x01);
    ASSERT(output[1] == 0x02);
    ASSERT(output[2] == 0x03);          /* Mehrheit, nicht 0xFF */
    ASSERT(output[3] == 0x04);
    ASSERT(confidence >= 75);
}

TEST(einigkeit_gibt_volle_konfidenz)
{
    /* GEGENPROBE, im Original nicht vorhanden: sind sich alle einig,
     * muss die Konfidenz HOEHER liegen als im Fall darueber. Sonst
     * traegt die Zahl keine Aussage. */
    uint8_t b[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    const uint8_t *buffers[] = { b, b, b };
    size_t lengths[] = { 4, 4, 4 };

    uint8_t output[4];
    uint8_t conf_einig = 0;
    ASSERT(multiread_vote_buffers(buffers, lengths, 3, output, 4,
                                  &conf_einig) == MULTIREAD_OK);
    ASSERT(memcmp(output, b, 4) == 0);
    ASSERT(conf_einig == 100);
}

TEST(kontext_mit_drei_lesungen)
{
    /* Zwei CRC-valide Lesungen, die uebereinstimmen, plus eine dritte
     * ohne gueltige CRC. Die beiden validen sind sich einig — der Fall
     * ist STABLE_GOOD, nicht der AMBIGUOUS-Fall aus MF-845. */
    multiread_config_t cfg = multiread_config_default();
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);

    uint8_t data1[] = { 0xAA, 0xBB, 0xCC };
    uint8_t data2[] = { 0xAA, 0xBB, 0xCC };
    uint8_t data3[] = { 0xAA, 0xBB, 0xDD };

    ASSERT(multiread_add_pass(ctx, data1, 3, 100, true)  == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, data2, 3, 100, true)  == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, data3, 3,  80, false) == MULTIREAD_OK);

    uint8_t output[3];
    multiread_sector_t result;
    memset(&result, 0, sizeof result);

    ASSERT(multiread_execute(ctx, output, 3, &result) == MULTIREAD_OK);
    ASSERT(output[0] == 0xAA);
    ASSERT(output[1] == 0xBB);
    ASSERT(output[2] == 0xCC);
    ASSERT(result.good_reads == 2);
    ASSERT(result.total_reads == 3);

    free(result.weak_mask);
    multiread_destroy(ctx);
}

TEST(fehlerpfade)
{
    ASSERT(multiread_vote_buffers(NULL, NULL, 0, NULL, 0, NULL)
           == MULTIREAD_ERR_NULL_PARAM);

    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);              /* NULL = Vorgabewerte */

    uint8_t output[4];
    multiread_sector_t result;
    memset(&result, 0, sizeof result);

    /* Ohne Lesungen darf nichts herauskommen — auch kein leerer Puffer,
     * der wie ein Ergebnis aussieht. */
    ASSERT(multiread_execute(ctx, output, 4, &result)
           == MULTIREAD_ERR_INSUFFICIENT_PASSES);

    multiread_destroy(ctx);
}

TEST(bericht_entsteht)
{
    multiread_config_t cfg = multiread_config_default();
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);

    char *report = multiread_generate_report(ctx, NULL, 0);
    ASSERT(report != NULL);
    ASSERT(strstr(report, "Multi-Read Recovery Report") != NULL);

    free(report);
    multiread_destroy(ctx);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Multiread: Selbsttests erstmals ausgefuehrt (MF-851) ===\n");
    RUN(mehrheit_entscheidet);
    RUN(einigkeit_gibt_volle_konfidenz);
    RUN(kontext_mit_drei_lesungen);
    RUN(fehlerpfade);
    RUN(bericht_entsteht);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
