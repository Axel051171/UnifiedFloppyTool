/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "uft/copy/uft_gap_policy.h"

#include <limits.h>

static bool add_overflow(size_t a, size_t b, size_t *out)
{
    if (b > SIZE_MAX - a) return true;
    *out = a + b;
    return false;
}

static bool mul_overflow(size_t a, size_t b, size_t *out)
{
    if (a && b > SIZE_MAX / a) return true;
    *out = a * b;
    return false;
}

static int used_bytes(const uft_gap_layout_t *g, const uft_gap_budget_t *b,
                      uint16_t gap3, size_t *used)
{
    size_t prefix, each, body, total;
    if (add_overflow(g->gap4a, g->gap1, &prefix) ||
        (g->has_index_address_mark &&
         add_overflow(prefix, b->index_address_mark_bytes, &prefix)) ||
        add_overflow(b->encoded_bytes_per_sector, g->gap2, &each) ||
        add_overflow(each, gap3, &each) ||
        mul_overflow(each, b->sector_count, &body) ||
        add_overflow(prefix, body, &total) ||
        add_overflow(total, g->gap4b, &total))
        return UFT_GAP_EOVERFLOW;
    *used = total;
    return UFT_GAP_OK;
}

int uft_gap_validate(const uft_gap_layout_t *g, const uft_gap_budget_t *b,
                     size_t *format_used, size_t *rw_used)
{
    size_t f = 0, r = 0;
    int rc;
    if (format_used) *format_used = 0;
    if (rw_used) *rw_used = 0;
    if (!g || !b || !b->track_bytes || !b->sector_count ||
        !b->encoded_bytes_per_sector)
        return UFT_GAP_EINVAL;
    if (g->gap3_format < g->gap3_read_write) return UFT_GAP_BAD_ORDER;
    rc = used_bytes(g, b, g->gap3_format, &f);
    if (rc != UFT_GAP_OK) return rc;
    rc = used_bytes(g, b, g->gap3_read_write, &r);
    if (rc != UFT_GAP_OK) return rc;
    if (format_used) *format_used = f;
    if (rw_used) *rw_used = r;
    return (f > b->track_bytes || r > b->track_bytes)
        ? UFT_GAP_DOES_NOT_FIT : UFT_GAP_OK;
}

int uft_gap_calculate_gap4b(uft_gap_layout_t *g,
                            const uft_gap_budget_t *b,
                            uint16_t minimum_gap4b)
{
    size_t original, used_without_tail, remaining;
    int rc;
    if (!g || !b) return UFT_GAP_EINVAL;
    original = g->gap4b;
    g->gap4b = 0;
    rc = used_bytes(g, b, g->gap3_format, &used_without_tail);
    if (rc != UFT_GAP_OK) { g->gap4b = (uint16_t)original; return rc; }
    if (used_without_tail > b->track_bytes) {
        g->gap4b = (uint16_t)original;
        return UFT_GAP_DOES_NOT_FIT;
    }
    remaining = b->track_bytes - used_without_tail;
    if (remaining < minimum_gap4b || remaining > UINT16_MAX) {
        g->gap4b = (uint16_t)original;
        return remaining > UINT16_MAX ? UFT_GAP_EOVERFLOW : UFT_GAP_DOES_NOT_FIT;
    }
    g->gap4b = (uint16_t)remaining;
    return UFT_GAP_OK;
}

uft_gap_layout_t uft_gap_profile_cbm1581(void)
{
    /* The source evidence distinguishes the controller's R/W GAP (0x0c)
     * from its format GAP (0x23). Unknown pre/post-index gaps remain zero;
     * callers must obtain them from a complete physical track profile. */
    const uft_gap_layout_t g = {
        0, 0, 0, 0x0c, 0x23, 0, 0x4e, false
    };
    return g;
}
