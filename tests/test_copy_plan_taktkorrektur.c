/**
 * @file test_copy_plan_taktkorrektur.c
 * @brief The settings tab's clock correction reaches the decoder (MF-1621).
 *
 * "Taktkorrektur" (50..200 %, 100 = unchanged) in the settings tab had no
 * reader (P3-711). The carrier exists and is read:
 * `uft_convert_options_t.decode_cell_adjust_pct` (MF-480, "percent nudge
 * for the decoder's cell time, 50..200; 0 or 100 means unchanged"), which
 * `uftc_apply_decode_options()` puts into the flux decoder's media
 * profile on the SCP -> ADF path. Nothing in the user interface set it.
 *
 * A plan now carries an explicit value when the field is set; unset
 * (0), the conversion keeps its default of 100.
 */
#include "uft/core/uft_copy_plan.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    char det[200];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("copy plan: clock correction reaches the decoder - MF-1621\n");

    uft_copy_plan_t q = uft_copy_plan_default();
    pruefe("(1) default plan: no clock correction", q.cell_adjust_pct == 0.0, "set");

    uft_convert_options_t o0, o1;
    memset(&o0, 0, sizeof o0);
    memset(&o1, 0, sizeof o1);
    o0.decode_cell_adjust_pct = 100.0;   /* the dispatcher's default */
    o1.decode_cell_adjust_pct = 100.0;
    uft_copy_plan_to_convert_options(&q, &o0);
    q.cell_adjust_pct = 97.5;
    uft_copy_plan_to_convert_options(&q, &o1);
    snprintf(det, sizeof det, "unset: %.1f, 97.5: %.1f",
             o0.decode_cell_adjust_pct, o1.decode_cell_adjust_pct);
    pruefe("(2) a set value reaches decode_cell_adjust_pct",
           o1.decode_cell_adjust_pct == 97.5, det);
    pruefe("(3) unset, the default stays",
           o0.decode_cell_adjust_pct == 100.0, det);

    char j[4096];
    uft_copy_plan_to_json(&q, j, sizeof j);
    pruefe("(4) the JSON names the value", strstr(j, "\"cellAdjustPct\": 97.5") != NULL, j);
    q.cell_adjust_pct = 0.0;
    uft_copy_plan_to_json(&q, j, sizeof j);
    pruefe("(5) and says nothing when unset", strstr(j, "cellAdjustPct") == NULL, j);

    /* MF-1625: fixed clock (the PLL switched off) travels the same way. */
    uft_copy_plan_t r = uft_copy_plan_default();
    pruefe("(6) default plan: adaptive clock", !r.fixed_clock, "fixed");
    uft_convert_options_t o2, o3;
    memset(&o2, 0, sizeof o2);
    memset(&o3, 0, sizeof o3);
    uft_copy_plan_to_convert_options(&r, &o2);
    r.fixed_clock = true;
    uft_copy_plan_to_convert_options(&r, &o3);
    pruefe("(7) fixed clock reaches decode_fixed_clock, unset stays adaptive",
           o3.decode_fixed_clock && !o2.decode_fixed_clock, "mapping");
    uft_copy_plan_to_json(&r, j, sizeof j);
    pruefe("(8) the JSON names a fixed clock", strstr(j, "\"fixedClock\": true") != NULL, j);
    r.fixed_clock = false;
    uft_copy_plan_to_json(&r, j, sizeof j);
    pruefe("(9) and says nothing when adaptive", strstr(j, "fixedClock") == NULL, j);

    printf("%d ok, %d failed\n", gruen, rot);
    return rot ? 1 : 0;
}
