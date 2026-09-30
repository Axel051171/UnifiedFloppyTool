/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_recovery_wizard_ohne_prozente.c
 * @brief The recovery wizard names a strategy, not an invented success
 *        percentage (P3-705, MF-1611).
 *
 * src/recovery/uft_recovery_wizard.c carried fixed "success probabilities"
 * per strategy and quality band (0.95 … 0.05), none measured, and printed
 * them into its texts ("Recommended: Re-read (est. 95% success)") and the
 * dialog's "95%" badge. Their only real use was ORDER, and the order is the
 * array `strategies[]` of each band. Owner decision 2026-09-29: remove them.
 *
 * Asserted on a real free corpus file: the recommendation and the
 * EXECUTE-step description name a strategy and contain no '%'.
 */
#include "uft/recovery/uft_recovery_wizard.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

int main(void)
{
    char pfad[600];
    printf("Recovery-Assistent ohne erfundene Prozente (MF-1611)\n");
    if (uft_register_all_formats() != UFT_OK) { printf("REGISTRY FEHLER\n"); return 1; }
    snprintf(pfad, sizeof pfad, "%s/xdftool_dd_ofs.adf", UFT_CORPUS_DIR);

    uft_recovery_wizard_t *wiz = uft_recovery_wizard_create();
    pruefe("Assistent angelegt", wiz != NULL, NULL);
    if (!wiz) return 1;
    int rc = uft_recovery_wizard_assess(wiz, pfad);
    char h[600];
    snprintf(h, sizeof h, "rc=%d", rc);
    pruefe("Bewertung einer freien Korpusdatei laeuft", rc == 0, h);

    const char *rat = uft_recovery_wizard_get_advice(wiz);
    snprintf(h, sizeof h, "Empfehlung: %s", rat ? rat : "(keine)");
    pruefe("die Empfehlung nennt keine Prozentschaetzung",
           rat && rat[0] && strchr(rat, '%') == NULL, h);
    /* MF-1622 (P3-708): the file is a clean dump; the wizard neither calls
     * it damaged nor claims a disk condition a dump cannot show. */
    pruefe("die Empfehlung erfindet weder Schaden noch Diskettenzustand",
           rat && strstr(rat, "damage") == NULL
               && strstr(rat, "quality is good") == NULL
               && strstr(rat, "sampled sectors") != NULL, h);

    /* forward to the EXECUTE step, whose description carried "est. N%" */
    int schritte = 0;
    while (wiz->current_step != UFT_REC_STEP_EXECUTE && schritte++ < 5)
        uft_recovery_wizard_next_step(wiz);
    snprintf(h, sizeof h, "Schritt %d: %s", (int)wiz->current_step, wiz->step_description);
    pruefe("die Beschreibung 'Ausfuehren' nennt keine Prozentschaetzung",
           wiz->current_step == UFT_REC_STEP_EXECUTE &&
           strchr(wiz->step_description, '%') == NULL, h);

    uft_recovery_wizard_free(wiz);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
