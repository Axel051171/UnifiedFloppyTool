/* SPDX-License-Identifier: GPL-2.0-or-later */
/** @file uft_match.c — Umsetzung von uft_match.h. */

#include "uft/util/uft_match.h"

#include <string.h>

/* Eigenes tolower für ASCII: das aus <ctype.h> hängt an der Locale, und
 * in einer türkischen Locale ist tolower('I') nicht 'i'. Bei Dateiendungen
 * und Format-IDs will man das nicht. */
static char lc(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* ───────────────────────── Endungen ────────────────────────────────────── */

bool uft_suffix_eq(const char *path, const char *suffix) {
    if (!path || !suffix) return false;

    const size_t pl = strlen(path);
    const size_t sl = strlen(suffix);
    if (sl == 0u || sl > pl) return false;

    /* AM ENDE vergleichen — das ist der ganze Unterschied. */
    const char *p = path + (pl - sl);
    for (size_t i = 0; i < sl; ++i)
        if (lc(p[i]) != lc(suffix[i])) return false;

    /* Ein Pfad, der GENAU die Endung ist (".st"), hat keinen Namen davor.
     * Das ist eher ein Versehen als eine Datei, also abweisen. */
    if (pl == sl) return false;
    return true;
}

bool uft_suffix_eq_any(const char *path, const char *const *suffixes,
                       size_t *out_index) {
    if (out_index) *out_index = (size_t)-1;
    if (!path || !suffixes) return false;

    for (size_t i = 0; suffixes[i] != NULL; ++i) {
        if (uft_suffix_eq(path, suffixes[i])) {
            if (out_index) *out_index = i;
            return true;
        }
    }
    return false;
}

bool uft_suffix_list_ok(const char *const *suffixes, size_t *out_bad) {
    if (out_bad) *out_bad = (size_t)-1;
    if (!suffixes) return false;

    size_t n = 0u;
    while (suffixes[n] != NULL) n++;

    /* Kein Eintrag darf Suffix eines SPAETEREN sein: sonst gewinnt der
     * kuerzere, und ".st" schlaegt ".stx". Das ist die Falle noch einmal,
     * nur in der Reihenfolge der Liste. */
    for (size_t i = 0; i < n; ++i) {
        const size_t li = strlen(suffixes[i]);
        for (size_t j = i + 1u; j < n; ++j) {
            const size_t lj = strlen(suffixes[j]);
            if (li >= lj) continue;
            const char *tail = suffixes[j] + (lj - li);
            bool same = true;
            for (size_t k = 0; k < li && same; ++k)
                same = (lc(tail[k]) == lc(suffixes[i][k]));
            if (same) {
                if (out_bad) *out_bad = i;
                return false;
            }
        }
    }
    return true;
}

const char *uft_path_ext(const char *path) {
    if (!path) return NULL;

    /* Von hinten suchen, aber nicht ueber einen Trennzeichen hinaus:
     * "/pfad.mit.punkt/datei" hat keine Endung. */
    const char *dot = NULL;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') dot = NULL;
        else if (*p == '.')          dot = p;
    }
    if (!dot || dot[1] == '\0') return NULL;
    /* Ein fuehrender Punkt ist ein versteckter Name, keine Endung. */
    if (dot == path || dot[-1] == '/' || dot[-1] == '\\') return NULL;
    return dot + 1;
}

/* ───────────────────────── Kennungen im Puffer ─────────────────────────── */

bool uft_magic_at(const uint8_t *data, size_t size, size_t offset,
                  const void *magic, size_t magic_len) {
    if (!data || !magic || magic_len == 0u) return false;
    /* Ueberlaufsicher: offset + magic_len kann umlaufen. */
    if (offset > size || magic_len > size - offset) return false;
    return memcmp(data + offset, magic, magic_len) == 0;
}

bool uft_magic_search(const uint8_t *data, size_t size,
                      const void *magic, size_t magic_len,
                      const char *why, size_t *out_offset) {
    (void)why;   /* Steht in der Signatur, damit die Begruendung beim
                  * Schreiben im Kopf ist und beim Lesen dasteht. */
    if (out_offset) *out_offset = (size_t)-1;
    if (!data || !magic || magic_len == 0u || magic_len > size) return false;

    const uint8_t *m = (const uint8_t *)magic;
    for (size_t i = 0; i + magic_len <= size; ++i) {
        if (data[i] != m[0]) continue;
        if (memcmp(data + i, m, magic_len) == 0) {
            if (out_offset) *out_offset = i;
            return true;
        }
    }
    return false;
}

/* ───────────────────────── Format-IDs ──────────────────────────────────── */

bool uft_id_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    size_t i = 0u;
    for (; a[i] != '\0' && b[i] != '\0'; ++i)
        if (lc(a[i]) != lc(b[i])) return false;
    /* GANZ vergleichen: beide muessen hier enden. Genau das fehlt bei
     * strncmp(a, b, strlen(b)). */
    return a[i] == '\0' && b[i] == '\0';
}

size_t uft_id_find(const char *id, const char *const *table, size_t count) {
    if (!id || !table) return (size_t)-1;
    for (size_t i = 0; i < count; ++i)
        if (table[i] && uft_id_eq(id, table[i])) return i;
    return (size_t)-1;
}

size_t uft_id_prefix_pairs(const char *const *table, size_t count,
                           void (*on_pair)(const char *, const char *, void *),
                           void *ctx) {
    if (!table) return 0u;
    size_t pairs = 0u;

    for (size_t i = 0; i < count; ++i) {
        if (!table[i]) continue;
        const size_t li = strlen(table[i]);
        for (size_t j = 0; j < count; ++j) {
            if (i == j || !table[j]) continue;
            const size_t lj = strlen(table[j]);
            if (li >= lj) continue;

            bool is_prefix = true;
            for (size_t k = 0; k < li && is_prefix; ++k)
                is_prefix = (lc(table[i][k]) == lc(table[j][k]));
            if (is_prefix) {
                pairs++;
                if (on_pair) on_pair(table[i], table[j], ctx);
            }
        }
    }
    return pairs;
}

/* ───────────────────────── Literalvergleich ────────────────────────────── */

bool uft_bytes_eq(const void *a, const void *b, size_t len,
                  size_t *out_first_diff) {
    if (out_first_diff) *out_first_diff = (size_t)-1;
    if (!a || !b) return false;
    if (len == 0u) return true;

    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < len; ++i) {
        if (pa[i] != pb[i]) {
            if (out_first_diff) *out_first_diff = i;
            return false;
        }
    }
    return true;
}
