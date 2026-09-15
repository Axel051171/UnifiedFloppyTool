/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_scp_integrity.c
 * @brief Umsetzung von uft_scp_integrity.h.
 *
 * Grundsatz: dieses Modul PRUEFT und BERICHTET. Es repariert nichts, es
 * schreibt nichts zurueck, und es trifft keine Entscheidung, die es nicht
 * begruendet. Eine Datei mit falscher Pruefsumme wird nicht abgelehnt —
 * davon sind zu viele im Umlauf, und eine Ablehnung wuerde Bestaende
 * unlesbar machen, die sonst brauchbar sind. Sie wird gemeldet.
 */

#include "uft/formats/uft_scp_integrity.h"

#include <stdio.h>
#include <string.h>

/* ───────────────────────────────── Hilfen ───────────────────────────────── */

static uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* ──────────────────────────────── Pruefsumme ────────────────────────────── */

uint32_t uft_scp_integrity_compute_checksum(const uint8_t *data, size_t size) {
    if (!data || size <= UFT_SCP_HEADER_SIZE) return 0u;

    uint32_t sum = 0u;
    for (size_t i = UFT_SCP_HEADER_SIZE; i < size; ++i) {
        sum += data[i];          /* vorsaetzlicher Ueberlauf, 32 Bit */
    }
    return sum;
}

uft_scp_cksum_result_t uft_scp_integrity_verify_checksum(const uint8_t *data, size_t size,
                                               uint32_t *out_stored,
                                               uint32_t *out_computed) {
    if (out_stored)   *out_stored = 0u;
    if (out_computed) *out_computed = 0u;

    if (!data || size < UFT_SCP_MIN_FILE_SIZE) return UFT_SCP_CKSUM_TOO_SHORT;

    const uint32_t stored   = rd_le32(data + UFT_SCP_OFF_CHECKSUM);
    const uint32_t computed = uft_scp_integrity_compute_checksum(data, size);

    if (out_stored)   *out_stored = stored;
    if (out_computed) *out_computed = computed;

    if (stored == 0u)        return UFT_SCP_CKSUM_NOT_STORED;
    if (stored == computed)  return UFT_SCP_CKSUM_OK;
    return UFT_SCP_CKSUM_MISMATCH;
}

/* ───────────────────────────── Spurtabellenbefund ───────────────────────── */

bool uft_scp_audit_tdht(const uint8_t *data, size_t size,
                        uft_scp_tdht_audit_t *out) {
    if (!data || !out) return false;
    memset(out, 0, sizeof(*out));
    out->first_cyl = 0xFFFFu;

    if (size < UFT_SCP_MIN_FILE_SIZE) return false;

    const uint8_t *tdht = data + UFT_SCP_TDHT_OFFSET;
    unsigned populated_cyls = 0u;

    for (unsigned cyl = 0; cyl < UFT_SCP_MAX_TRACKS / 2u; ++cyl) {
        const uint32_t a = rd_le32(tdht + (cyl * 2u + 0u) * 4u);
        const uint32_t b = rd_le32(tdht + (cyl * 2u + 1u) * 4u);

        const uint32_t pair[2] = { a, b };
        bool cyl_has_data = false;

        for (unsigned h = 0; h < 2u; ++h) {
            if (pair[h] == 0u) continue;

            cyl_has_data = true;
            out->populated++;
            if (h == 0u) out->populated_side0++;
            else         out->populated_side1++;

            if (pair[h] >= size)                     out->out_of_range++;
            else if (pair[h] < UFT_SCP_MIN_FILE_SIZE) out->below_tdht++;
        }

        if (cyl_has_data) {
            populated_cyls++;
            if (out->first_cyl == 0xFFFFu) out->first_cyl = cyl;
            out->last_cyl = cyl;
        }

        /* Beide Steckplaetze zeigen auf denselben Strom. Auf einer echten
         * Aufnahme kommt das nicht vor: jede Spur hat ihren eigenen TDH.
         * Es ist das Kennzeichen einer nachtraeglich umgeschriebenen
         * Tabelle — siehe SCPmodSideB.py, das genau das tut, um einseitige
         * Seite-B-Abzuege fuer a8rawconv lesbar zu machen. */
        if (a != 0u && a == b) out->aliased_pairs++;
    }

    out->side0_empty = (out->populated_side0 == 0u);
    out->side1_empty = (out->populated_side1 == 0u);

    /* Nur wenn ALLE belegten Zylinder aliasiert sind, ist das systematisch.
     * Ein oder zwei Zufallstreffer waeren kein Befund — aber Offsets sind
     * absolute Dateipositionen, ein echter Zufall ist praktisch ausgeschlossen.
     * Die Schwelle von zwei Zylindern haelt Einzelspur-Abzuege heraus. */
    out->looks_aliased = (out->aliased_pairs >= 2u &&
                          out->aliased_pairs == populated_cyls);

    return true;
}

/* ───────────────────────────── Seiten-Deutung ───────────────────────────── */

static void apply_interp(uft_scp_interp_t interp, uft_scp_side_plan_t *p) {
    switch (interp) {
    case UFT_SCP_INTERP_SS40:
        p->head_count = 1u; p->cylinders = 40u; p->double_step = true;  break;
    case UFT_SCP_INTERP_DS40:
        p->head_count = 2u; p->cylinders = 40u; p->double_step = true;  break;
    case UFT_SCP_INTERP_SS80:
        p->head_count = 1u; p->cylinders = 80u; p->double_step = false; break;
    case UFT_SCP_INTERP_DS80:
        p->head_count = 2u; p->cylinders = 80u; p->double_step = false; break;
    case UFT_SCP_INTERP_NONE:
    default:
        return;
    }
    p->from_override = true;
}

void uft_scp_resolve_sides(uint8_t heads_byte,
                           uint8_t start_trk, uint8_t end_trk,
                           const uft_scp_tdht_audit_t *audit,
                           uft_scp_sides_t want,
                           uft_scp_interp_t interp,
                           uft_scp_side_plan_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    /* 1. Welche Seiten? */
    if (want != UFT_SCP_SIDES_AUTO) {
        out->sides = want;
        out->from_override = true;
    } else if (heads_byte == UFT_SCP_HEADS_SIDE0) {
        out->sides = UFT_SCP_SIDES_SIDE0;
        out->from_header = true;
    } else if (heads_byte == UFT_SCP_HEADS_SIDE1) {
        out->sides = UFT_SCP_SIDES_SIDE1;
        out->from_header = true;
    } else {
        /* Kopf sagt "beide". Das ist der Vorgabewert vieler Erzeuger, auch
         * wenn nur eine Seite aufgenommen wurde — deshalb schauen wir in die
         * Tabelle, bevor wir ihm glauben. */
        out->sides = UFT_SCP_SIDES_BOTH;
        out->from_header = true;

        if (audit && audit->populated > 0u) {
            if (audit->side0_empty && !audit->side1_empty) {
                out->sides = UFT_SCP_SIDES_SIDE1;
                out->from_header = false;
                out->from_tdht = true;
                out->header_contradicts = true;
            } else if (audit->side1_empty && !audit->side0_empty) {
                out->sides = UFT_SCP_SIDES_SIDE0;
                out->from_header = false;
                out->from_tdht = true;
                out->header_contradicts = true;
            }
        }
    }

    /* Widerspruch auch melden, wenn der Kopf eindeutig war, die Tabelle aber
     * das Gegenteil zeigt. Nicht korrigieren — der Kopf ist die Aussage des
     * Aufnehmenden, die Tabelle die des Inhalts. Beides gehoert ins Protokoll. */
    if (audit && audit->populated > 0u && !out->from_tdht) {
        if (out->sides == UFT_SCP_SIDES_SIDE0 && audit->side0_empty)
            out->header_contradicts = true;
        if (out->sides == UFT_SCP_SIDES_SIDE1 && audit->side1_empty)
            out->header_contradicts = true;
    }

    switch (out->sides) {
    case UFT_SCP_SIDES_SIDE0: out->head_count = 1u; out->first_head = 0u; break;
    case UFT_SCP_SIDES_SIDE1: out->head_count = 1u; out->first_head = 1u; break;
    case UFT_SCP_SIDES_BOTH:
    default:                  out->head_count = 2u; out->first_head = 0u; break;
    }

    /* 2. Wie viele Zylinder?
     *
     * Die Spurnummern im Kopf zaehlen Spuren je DISKETTE, nicht je Seite —
     * so steht es in der Spezifikation und so rechnet a8rawconv
     * (rawdiskscp.cpp:104-106). Deshalb `(end + 1 + 1) / 2`, dieselbe Formel
     * wie in uft_scp_plugin.c:219. Bei einseitigen Abzuegen bleibt sie
     * richtig, weil die Tabelle die Plaetze der fehlenden Seite trotzdem
     * reserviert. */
    (void)start_trk;
    out->cylinders = ((unsigned)end_trk + 2u) / 2u;
    if (out->cylinders == 0u || out->cylinders > UFT_SCP_MAX_TRACKS / 2u)
        out->cylinders = UFT_SCP_MAX_TRACKS / 2u;

    out->double_step = false;

    /* 3. Erzwungene Deutung schlaegt alles. */
    apply_interp(interp, out);
}

/* ───────────────────────────── Gesamtbefund ─────────────────────────────── */

bool uft_scp_check_integrity(const uint8_t *data, size_t size,
                             uft_scp_sides_t want, uft_scp_interp_t interp,
                             uft_scp_integrity_t *out) {
    if (!data || !out) return false;
    memset(out, 0, sizeof(*out));

    if (size < UFT_SCP_MIN_FILE_SIZE) {
        out->checksum = UFT_SCP_CKSUM_TOO_SHORT;
        return false;
    }

    out->checksum = uft_scp_integrity_verify_checksum(data, size,
                                            &out->checksum_stored,
                                            &out->checksum_computed);
    uft_scp_audit_tdht(data, size, &out->tdht);

    uft_scp_resolve_sides(data[UFT_SCP_OFF_HEADS],
                          data[UFT_SCP_OFF_START_TRK],
                          data[UFT_SCP_OFF_END_TRK],
                          &out->tdht, want, interp, &out->sides);

    out->tampered = (out->checksum == UFT_SCP_CKSUM_MISMATCH)
                 || out->tdht.looks_aliased;

    return true;
}

/* ───────────────────────────── Zusammenfassung ──────────────────────────── */

static const char *sides_name(uft_scp_sides_t s) {
    switch (s) {
    case UFT_SCP_SIDES_SIDE0: return "nur Seite 0";
    case UFT_SCP_SIDES_SIDE1: return "nur Seite 1 (Rueckseite)";
    case UFT_SCP_SIDES_BOTH:  return "beide Seiten";
    default:                  return "unbestimmt";
    }
}

size_t uft_scp_integrity_summary(const uft_scp_integrity_t *in,
                                 char *buf, size_t buflen) {
    if (!in || !buf || buflen == 0u) return 0u;

    size_t n = 0u;
    #define APPEND(...)                                                     \
        do {                                                                \
            if (n < buflen) {                                               \
                int _r = snprintf(buf + n, buflen - n, __VA_ARGS__);        \
                if (_r > 0) n += (size_t)_r;                                \
                if (n >= buflen) n = buflen - 1u;                           \
            }                                                               \
        } while (0)

    switch (in->checksum) {
    case UFT_SCP_CKSUM_OK:
        APPEND("Pruefsumme: stimmt (0x%08X)\n", in->checksum_stored);
        break;
    case UFT_SCP_CKSUM_NOT_STORED:
        APPEND("Pruefsumme: nicht gebildet (Feld ist 0) — kein Befund\n");
        break;
    case UFT_SCP_CKSUM_MISMATCH:
        APPEND("Pruefsumme: FALSCH — gespeichert 0x%08X, berechnet 0x%08X.\n"
               "  Die Datei wurde nach der Aufnahme veraendert.\n",
               in->checksum_stored, in->checksum_computed);
        break;
    case UFT_SCP_CKSUM_TOO_SHORT:
        APPEND("Pruefsumme: Datei zu kurz fuer Kopf und Spurtabelle\n");
        break;
    }

    APPEND("Spurtabelle: %u Eintraege belegt (Seite 0: %u, Seite 1: %u), "
           "Zylinder %u..%u\n",
           in->tdht.populated, in->tdht.populated_side0,
           in->tdht.populated_side1,
           in->tdht.first_cyl == 0xFFFFu ? 0u : in->tdht.first_cyl,
           in->tdht.last_cyl);

    if (in->tdht.looks_aliased) {
        APPEND("  WARNUNG: %u Zylinder fuehren beide Seiten auf denselben "
               "Flussstrom.\n"
               "  Das ist keine Aufnahme, sondern eine umgeschriebene Tabelle "
               "(vgl. SCPmodSideB.py).\n"
               "  Die Datei enthaelt EINE Seite, gibt aber zwei an.\n",
               in->tdht.aliased_pairs);
    } else if (in->tdht.aliased_pairs > 0u) {
        APPEND("  Hinweis: %u Zylinder mit gleichem Offset auf beiden Seiten\n",
               in->tdht.aliased_pairs);
    }

    if (in->tdht.out_of_range > 0u)
        APPEND("  WARNUNG: %u Offsets zeigen hinter das Dateiende\n",
               in->tdht.out_of_range);
    if (in->tdht.below_tdht > 0u)
        APPEND("  WARNUNG: %u Offsets zeigen in Kopf oder Spurtabelle\n",
               in->tdht.below_tdht);

    APPEND("Seiten: %s, %u Zylinder%s\n",
           sides_name(in->sides.sides), in->sides.cylinders,
           in->sides.double_step ? ", Doppelschritt" : "");
    APPEND("  Quelle der Deutung: %s\n",
           in->sides.from_override ? "Benutzervorgabe"
         : in->sides.from_tdht     ? "Spurtabelle"
         : in->sides.from_header   ? "Kopfbyte 0x0A"
                                   : "Vorgabe");
    if (in->sides.header_contradicts)
        APPEND("  WARNUNG: Kopfbyte 0x0A und Spurtabelle widersprechen sich\n");

    #undef APPEND
    return n;
}
