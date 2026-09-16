/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_track_layout.c
 * @brief Umsetzung von uft_track_layout.h.
 *
 * Herkunft und Begruendung stehen im Header. Kurz: Fuellung ist keine
 * Aussage ueber die Diskette, und ohne ein Feld dafuer wird sie eine —
 * dreimal geschehen (MF-1022, MF-1038, MF-1135).
 */

#include "uft/core/uft_track_layout.h"

#include <string.h>

uint32_t uft_tl_track_stored_bytes(const uft_tl_track_t *t)
{
    if (!t) return 0u;
    uint32_t n = 0u;
    for (uint8_t i = 0; i < t->count; ++i) n += t->sectors[i].size;
    return n;
}

uint32_t uft_tl_track_payload_bytes(const uft_tl_track_t *t)
{
    if (!t) return 0u;
    uint32_t n = 0u;
    for (uint8_t i = 0; i < t->count; ++i) {
        /* Fuellsektoren tragen nichts bei. Genau hier entscheidet sich, ob
         * das Werkzeug ueber die Diskette oder ueber seinen eigenen Puffer
         * Auskunft gibt.
         *
         * UNBEKANNT zaehlt aus demselben Grund nicht mit: wer die Herkunft
         * nicht gesetzt hat, hat nicht belegt, dass etwas vom Medium kam.
         * Das ist die Haelfte, die den Nullwert traegt — siehe Header. */
        switch (t->sectors[i].origin) {
        case UFT_SEC_UNBEKANNT:
        case UFT_SEC_PADDING_SECTOR:
        case UFT_SEC_ABSENT:
            continue;
        case UFT_SEC_FROM_MEDIUM:
        case UFT_SEC_PADDED_TAIL:
            n += t->sectors[i].payload_size;
            break;
        }
    }
    return n;
}

bool uft_tl_track_is_uniform(const uft_tl_track_t *t)
{
    if (!t || t->count == 0u) return true;
    const uint16_t s = t->sectors[0].size;
    for (uint8_t i = 1; i < t->count; ++i)
        if (t->sectors[i].size != s) return false;
    return true;
}

uint8_t uft_tl_count_origin(const uft_tl_track_t *t, uft_sec_origin_t o)
{
    if (!t) return 0u;
    uint8_t n = 0u;
    for (uint8_t i = 0; i < t->count; ++i)
        if (t->sectors[i].origin == o) n++;
    return n;
}

bool uft_tl_can_flatten(const uft_tl_track_t *t, const char **out_reason)
{
    if (out_reason) *out_reason = NULL;
    if (!t) { if (out_reason) *out_reason = "keine Spur"; return false; }

    /* Die UNBEKANNT-Pruefung steht ABSICHTLICH zuerst: eine Spur, deren
     * Herkunft niemand gesetzt hat, ist nicht „vielleicht flach ablegbar",
     * sondern unbeantwortet. Stuende sie hinter der Gleichmaessigkeits-
     * pruefung, kaeme fuer eine gleichmaessige Spur ohne gesetzte Herkunft
     * ein `true` heraus — und das waere genau die Zusage, die dieses Modul
     * verhindern soll. */
    if (uft_tl_count_origin(t, UFT_SEC_UNBEKANNT) > 0u) {
        if (out_reason)
            *out_reason = "Spur enthaelt Sektoren ohne gesetzte Herkunft — "
                          "ob sie vom Medium stammen, ist unbeantwortet, "
                          "nicht bejaht";
        return false;
    }
    if (!uft_tl_track_is_uniform(t)) {
        if (out_reason)
            *out_reason = "ungleiche Sektorgroessen in einer Spur "
                          "(Ensoniq-SQ80-Fall) — flaches Abbild wuerde die "
                          "Groessen verlieren";
        return false;
    }
    if (uft_tl_count_origin(t, UFT_SEC_PADDING_SECTOR) > 0u) {
        if (out_reason)
            *out_reason = "Spur enthaelt Fuellsektoren (FLEX-DD-Fall) — ein "
                          "flaches Abbild wuerde 0xFF als Mediendaten ausgeben";
        return false;
    }
    if (uft_tl_count_origin(t, UFT_SEC_PADDED_TAIL) > 0u) {
        if (out_reason)
            *out_reason = "Sektoren mit Fuellschwanz (Slogger/LSI-2-Fall) — "
                          "die gelesene Nutzlaenge steht nicht im Abbild";
        return false;
    }
    return true;
}

/* ───────────────────────── Vorlagen ────────────────────────────────────── */

static void tl_init(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                    uft_tl_encoding_t enc)
{
    /* Das `memset` setzt jede Herkunft auf UFT_SEC_UNBEKANNT (0). Die
     * Vorlagen unten schreiben sie danach ausdruecklich — wer eine Spur
     * von Hand baut und es vergisst, bekommt „unbekannt" und nicht „vom
     * Medium". */
    memset(t, 0, sizeof(*t));
    t->cyl = cyl;
    t->head = head;
    t->encoding = enc;
}

void uft_tl_make_sq80(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                      uint8_t count, uint16_t last_size)
{
    if (!t || count == 0u || count > UFT_TL_MAX_SECTORS_PER_TRACK) return;
    tl_init(t, cyl, head, UFT_ENC_TL_MFM);
    t->count = count;
    for (uint8_t i = 0; i < count; ++i) {
        const uint16_t sz = (i == (uint8_t)(count - 1u)) ? last_size : 1024u;
        t->sectors[i].id           = (uint8_t)(i + 1u);
        t->sectors[i].size         = sz;
        t->sectors[i].payload_size = sz;
        t->sectors[i].origin       = UFT_SEC_FROM_MEDIUM;
    }
}

void uft_tl_make_fm_padded_tail(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                                uint8_t count, uint16_t stored_size)
{
    if (!t || count == 0u || count > UFT_TL_MAX_SECTORS_PER_TRACK) return;
    tl_init(t, cyl, head, UFT_ENC_TL_FM);
    t->count = count;
    for (uint8_t i = 0; i < count; ++i) {
        t->sectors[i].id           = (uint8_t)(i + 1u);
        t->sectors[i].size         = stored_size;
        t->sectors[i].payload_size = (uint16_t)(stored_size / 2u);
        t->sectors[i].origin       = UFT_SEC_PADDED_TAIL;
    }
}

void uft_tl_make_flex_dd_fm(uft_tl_track_t *t, uint16_t cyl, uint8_t head,
                            uint8_t real, uint8_t target, uint16_t size)
{
    if (!t || target == 0u || target > UFT_TL_MAX_SECTORS_PER_TRACK) return;
    if (real > target) real = target;
    tl_init(t, cyl, head, UFT_ENC_TL_FM);
    t->count = target;
    for (uint8_t i = 0; i < target; ++i) {
        t->sectors[i].id   = (uint8_t)(i + 1u);
        t->sectors[i].size = size;
        if (i < real) {
            t->sectors[i].payload_size = size;
            t->sectors[i].origin       = UFT_SEC_FROM_MEDIUM;
        } else {
            t->sectors[i].payload_size = 0u;
            t->sectors[i].origin       = UFT_SEC_PADDING_SECTOR;
        }
    }
}
