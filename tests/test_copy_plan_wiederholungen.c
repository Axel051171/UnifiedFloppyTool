/**
 * @file test_copy_plan_wiederholungen.c
 * @brief The settings tab's retry count reaches the conversion, and the
 *        default plan is fully initialised (MF-1619).
 *
 * (A) `uft_copy_plan_default()` declared `uft_copy_plan_t p;` and set ten
 *     fields. The two fields MF-1311 appended, `caps` and `caps_bekannt`,
 *     stayed UNINITIALISED — and `uft_copy_plan_gate_caps()` reads them to
 *     decide between "deny" and "measure first". The settings tab starts
 *     every plan from this function. The test dirties the stack with a
 *     pattern first, so that garbage cannot pass for zero by luck.
 *
 * (B) "Max. Wiederholungen" in the settings tab had no reader (P3-711).
 *     The carrier exists: `uft_convert_options_t.decode_retries`, read in
 *     the SCP -> D64 conversion (uft_format_convert_flux.c), where it
 *     bounds the revolutions tried. Until MF-1619 only the read strategy
 *     set it (uft_copy_plan_to_convert_options). A plan now carries an
 *     explicit count when the field is set; unset, the strategy decides
 *     as before.
 */
#include "uft/core/uft_copy_plan.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/* Leaves a pattern where the next call's frame will lie. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void schmutz(void)
{
    volatile unsigned char b[4096];
    for (size_t i = 0; i < sizeof b; i++) b[i] = 0xA5;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
static uft_copy_plan_t vorgabe_nach_schmutz(void)
{
    schmutz();
    return uft_copy_plan_default();
}

int main(void)
{
    char det[200];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("copy plan: default initialised, retries reach the conversion - MF-1619\n");

    /* (A) */
    const uft_copy_plan_t p = vorgabe_nach_schmutz();
    snprintf(det, sizeof det, "caps = 0x%08x, caps_bekannt = %d",
             (unsigned)p.caps, (int)p.caps_bekannt);
    pruefe("(A1) default plan: caps 0 and caps_bekannt false (after a dirty stack)",
           p.caps == 0u && !p.caps_bekannt, det);
    pruefe("(A2) default plan: no explicit retry count",
           !p.read_retries_gesetzt && p.read_retries == 0u, "retries set");

    /* (B) the carrier */
    uft_convert_options_t o0, o1;
    memset(&o0, 0, sizeof o0);
    memset(&o1, 0, sizeof o1);
    o0.decode_retries = 99u;     /* a value the plan must not keep by accident */
    o1.decode_retries = 99u;
    uft_copy_plan_t q = uft_copy_plan_default();
    q.strategy = UFT_READ_DEEP;
    uft_copy_plan_to_convert_options(&q, &o0);           /* strategy decides */
    q.read_retries_gesetzt = true;
    q.read_retries = 7u;
    uft_copy_plan_to_convert_options(&q, &o1);           /* explicit count */
    snprintf(det, sizeof det, "strategy only: %u, explicit 7: %u",
             (unsigned)o0.decode_retries, (unsigned)o1.decode_retries);
    pruefe("(B1) an explicit count reaches decode_retries", o1.decode_retries == 7u, det);
    /* without an explicit count the plan must not write its (zero) field:
     * what stays is the strategy's number or the value that was there */
    pruefe("(B2) without it, the strategy's value stays",
           o0.decode_retries != 7u && o0.decode_retries != 0u, det);

    char j[4096];
    uft_copy_plan_to_json(&q, j, sizeof j);
    pruefe("(B3) the JSON names the explicit count",
           strstr(j, "\"readRetries\": 7") != NULL, j);
    q.read_retries_gesetzt = false;
    uft_copy_plan_to_json(&q, j, sizeof j);
    pruefe("(B4) and says nothing when there is none",
           strstr(j, "readRetries") == NULL, j);

    printf("%d ok, %d failed\n", gruen, rot);
    return rot ? 1 : 0;
}
