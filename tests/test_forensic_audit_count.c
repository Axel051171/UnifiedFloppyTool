/**
 * @file test_forensic_audit_count.c
 * @brief Der Audit-Zähler zählt, statt einen Zeiger zu verschieben (MF-507).
 *
 * ── Der Fehler ───────────────────────────────────────────────────────────
 *
 * `uft_forensic_log()` schloss mit
 *
 *     session->audit_entries++;
 *
 * `audit_entries` ist aber ein `void *` — im Header ausdruecklich als
 * „opaque list, may be NULL" beschrieben. Der Ausdruck zaehlt also nicht,
 * sondern schiebt den ZEIGER je Logzeile um ein Byte weiter (GCC erlaubt
 * Arithmetik auf `void *` als Erweiterung; jeder strengere Uebersetzer
 * lehnt es ab).
 *
 * Direkt daneben liegt `size_t audit_count` — und die wurde nie angefasst.
 * Es ist also nicht bloss ein toter Ausdruck, sondern eine Verwechslung
 * mit dem Feld, das gemeint war.
 *
 * ── Warum das mehr als ein Schoenheitsfehler ist ─────────────────────────
 *
 * Der Zeiger wandert. Wird die Liste jemals angelegt und freigegeben,
 * zeigt er nicht mehr auf ihren Anfang — Heap-Korruption im forensischen
 * Audit-Pfad, also ausgerechnet dort, wo dieses Werkzeug seine Aussagen
 * belegt. Gefunden nicht durch einen Test, sondern durch einen
 * Uebersetzerlauf mit `-Wpointer-arith`.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Dass die Zahl steigt und der Zeiger stehenbleibt. Beides zusammen,
 * denn jedes fuer sich waere auch mit dem Fehler zu haben gewesen.
 */

#include "uft/uft_types.h"
#include "uft/recovery/uft_forensic_types.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-56s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

TEST(logging_counts_entries_and_leaves_the_list_pointer_alone)
{
    uft_forensic_session_t s;
    memset(&s, 0, sizeof(s));
    uft_forensic_config_t cfg = uft_forensic_config_default();
    ASSERT(uft_forensic_session_init(&s, &cfg) == 0);

    /* Einen erkennbaren Wert einsetzen: der Zeiger darf sich nicht
     * bewegen, egal wie oft geloggt wird. */
    void *marker = (void *)(uintptr_t)0x1000;
    s.audit_entries = marker;
    s.audit_count = 0;

    for (int i = 0; i < 5; i++)
        uft_forensic_log(&s, 0, "Eintrag %d", i);

    if (s.audit_entries != marker)
        printf("\n        Listenzeiger wanderte um %ld Byte\n",
               (long)((char *)s.audit_entries - (char *)marker));
    ASSERT(s.audit_entries == marker);

    if (s.audit_count != 5)
        printf("\n        audit_count ist %zu statt 5\n", s.audit_count);
    ASSERT(s.audit_count == 5);

    uft_forensic_session_finish(&s);
}

TEST(a_null_session_does_not_crash_the_log)
{
    /* Gegenprobe: die Funktion muss den Fall aushalten, sonst waere der
     * Zaehler-Test von der Reihenfolge der Aufrufe abhaengig. */
    uft_forensic_log(NULL, 0, "ohne Sitzung");
}

int main(void)
{
    printf("=== Audit-Zaehler statt Zeiger-Arithmetik (MF-507) ===\n");
    RUN(logging_counts_entries_and_leaves_the_list_pointer_alone);
    RUN(a_null_session_does_not_crash_the_log);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
