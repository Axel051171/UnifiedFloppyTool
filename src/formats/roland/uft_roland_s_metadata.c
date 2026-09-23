/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_roland_s_metadata.c
 * @brief Semantische Auswertung einer Roland-S-Diskette.
 *
 * Referenz, Abgrenzung, Lizenzlage und — wichtig — WAS HIER NICHT BELEGT
 * IST stehen im Kopf von `include/uft/formats/uft_roland_s_metadata.h`.
 * Kurz: die Feldversaetze stammen aus der Vorlage und sind gegen keine
 * aeussere Quelle geprueft. Dieses Modul ist damit im Sinne der
 * EINFRIER-REGEL UNGEPRUEFT und registriert deshalb auch kein Plugin.
 */

#include "uft/formats/uft_roland_s_metadata.h"
#include "uft/formats/uft_roland_ident.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Versaetze, saemtlich aus der Vorlage ──────────────────────────────
 *
 * Sie stehen hier an EINER Stelle und nicht verstreut, damit ein spaeterer
 * Beleg sie an einer Stelle berichtigen kann (MF-1177). */
enum {
    PATCH_BANK_1_OFFSET = 64512,
    PATCH_BANK_2_OFFSET = 66560,
    PATCH_RECORD_SIZE   = 256,
    PATCH_NAME_SIZE     = 12,
    PATCH_JACK_OFFSET   = 243,
    LABEL_BLOCK_OFFSET  = 68552,
    TONE_TABLE_OFFSET   = 69120,
    TONE_RECORD_SIZE    = 16,
    TONE_NAME_SIZE      = 8,
    TONE_PARAM_OFFSET   = 9
};

/* ── Hilfen ───────────────────────────────────────────────────────────── */

/** Liegt [offset, offset+length) vollstaendig im Puffer? */
static bool passt(size_t size, size_t offset, size_t length) {
    return offset <= size && length <= size - offset;
}

static bool druckbar(uint8_t c) { return c >= 0x20u && c <= 0x7Eu; }

/**
 * Text kopieren, nicht druckbare Zeichen durch '.' ersetzen und das
 * MELDEN. Ein stillschweigend ersetztes Byte waere eine Veraenderung ohne
 * Auskunft — genau das, was dieses Werkzeug nicht tut.
 */
static bool text_kopieren(char *ziel, size_t ziel_size, const uint8_t *data,
                          size_t size, size_t offset, size_t length,
                          bool *ersetzt) {
    if (!ziel || ziel_size == 0u) return false;
    ziel[0] = '\0';
    if (!data || length + 1u > ziel_size || !passt(size, offset, length))
        return false;

    for (size_t i = 0; i < length; i++) {
        const uint8_t c = data[offset + i];
        if (druckbar(c)) {
            ziel[i] = (char)c;
        } else {
            ziel[i] = '.';
            if (ersetzt) *ersetzt = true;
        }
    }
    ziel[length] = '\0';

    /* Nachlaufende Leerzeichen weg — Roland polstert damit. */
    for (size_t i = length; i > 0u && ziel[i - 1u] == ' '; i--)
        ziel[i - 1u] = '\0';
    return true;
}

static bool nur_leer(const char *s) {
    for (; s && *s; s++) if (*s != ' ' && *s != '.') return false;
    return true;
}

static void warnung(uft_roland_s_report_t *r,
                    uft_roland_s_warning_kind_t kind, int tone,
                    const char *fmt, ...) {
    if (!r) return;
    if (r->warning_count >= UFT_ROLAND_S_MAX_WARNINGS) {
        /* MF-1040: gekuerzt wird nicht stillschweigend. */
        r->warnings_truncated = true;
        return;
    }
    uft_roland_s_warning_t *w = &r->warnings[r->warning_count++];
    w->kind = kind;
    w->tone_number = tone;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(w->message, sizeof(w->message), fmt, ap);
    va_end(ap);
}

/* ── Patches ──────────────────────────────────────────────────────────── */

static void patch_lesen(uft_roland_s_report_t *r, const uint8_t *data,
                        size_t size, size_t index, size_t offset,
                        bool s50, bool *ersetzt) {
    uft_roland_s_patch_t *p = &r->patches[index];
    p->number = (unsigned)index + 1u;
    (void)text_kopieren(p->name, sizeof(p->name), data, size, offset,
                        PATCH_NAME_SIZE, ersetzt);
    p->populated = !nur_leer(p->name);

    /* Der S-50 hat keine Ausgangsbuchsen-Wahl. Eine Zahl dort zu zeigen
     * waere erfundene Auskunft, also wird sie gar nicht erst gelesen. */
    p->output_jack_available = !s50;
    if (s50 || !passt(size, offset + PATCH_JACK_OFFSET, 1u)) {
        p->output_jack_raw = 0u;
        p->output_jack[0] = '\0';
        return;
    }
    p->output_jack_raw = data[offset + PATCH_JACK_OFFSET];
    if (p->output_jack_raw <= 7u) {
        p->output_jack[0] = (char)('1' + p->output_jack_raw);
        p->output_jack[1] = '\0';
    } else if (p->output_jack_raw == 8u) {
        p->output_jack[0] = 'T';
        p->output_jack[1] = '\0';
    } else {
        p->output_jack[0] = '?';
        p->output_jack[1] = '\0';
    }
}

/* ── Tones ────────────────────────────────────────────────────────────── */

/** I11..I48: acht je Gruppe, Gruppen ab 1. */
static unsigned tone_nummer(size_t index) {
    return (unsigned)(((index / 8u) + 1u) * 10u + (index % 8u) + 1u);
}

static void tone_lesen(uft_roland_s_report_t *r, const uint8_t *data,
                       size_t size, size_t index, bool *ersetzt) {
    const size_t offset = (size_t)TONE_TABLE_OFFSET + index * TONE_RECORD_SIZE;
    uft_roland_s_tone_t *t = &r->tones[index];
    t->number = tone_nummer(index);
    (void)text_kopieren(t->name, sizeof(t->name), data, size, offset,
                        TONE_NAME_SIZE, ersetzt);
    t->populated = !nur_leer(t->name);

    if (!passt(size, offset + TONE_PARAM_OFFSET, 7u)) return;
    const uint8_t *p = data + offset + TONE_PARAM_OFFSET;

    t->original_tone_raw    = p[0];
    t->original_tone_number = (p[0] < 32u) ? tone_nummer(p[0]) : 0u;
    t->is_subtone           = (p[1] == 1u);

    t->sample_rate_raw = p[2];
    t->sample_rate_hz  = (p[2] == 0u) ? 30000u
                       : (p[2] == 1u) ? 15000u
                                      : 0u;   /* 0 = UNBEKANNT */

    t->wave_bank_raw = p[4];
    t->wave_bank = (p[4] == 0u) ? UFT_ROLAND_S_BANK_A
                 : (p[4] == 1u) ? UFT_ROLAND_S_BANK_B
                 : (p[4] == 2u) ? UFT_ROLAND_S_BANK_NONE
                                : UFT_ROLAND_S_BANK_UNKNOWN;

    t->sample_length_raw       = p[6];
    t->sample_length_available = (p[6] != 0xFFu);
    /* Ganzzahlig: eine Einheit sind 400 ms. Die Vorlage rechnete
     * `* 0.4` in `double`; das ist ueber Plattformen nicht bitgleich. */
    t->sample_duration_ms = t->sample_length_available
        ? (uint32_t)p[6] * UFT_ROLAND_S_LENGTH_UNIT_MS : 0u;

    if (!t->populated) return;

    if (t->sample_length_available && p[6] != 0u && t->sample_rate_hz == 0u) {
        warnung(r, UFT_ROLAND_S_WARNING_INVALID_SAMPLE_RATE, (int)t->number,
                "Tone I%u hat Sampledaten, aber ein unbekanntes Ratenbyte "
                "0x%02X", t->number, (unsigned)t->sample_rate_raw);
    }
    if (t->wave_bank == UFT_ROLAND_S_BANK_UNKNOWN) {
        warnung(r, UFT_ROLAND_S_WARNING_INVALID_WAVE_BANK, (int)t->number,
                "Tone I%u hat ein unbekanntes Wellenbank-Byte 0x%02X",
                t->number, (unsigned)t->wave_bank_raw);
    }

    /* Subtones teilen sich die Daten ihres Ursprungstons; sie doppelt zu
     * zaehlen waere eine erfundene Belegung. */
    if (!t->is_subtone && t->sample_length_available) {
        if (t->wave_bank == UFT_ROLAND_S_BANK_A)
            r->used_ms_a += t->sample_duration_ms;
        else if (t->wave_bank == UFT_ROLAND_S_BANK_B)
            r->used_ms_b += t->sample_duration_ms;
    }
}

/* ── Etikett ──────────────────────────────────────────────────────────── */

static void etikett_lesen(uft_roland_s_report_t *r, const uint8_t *data,
                          size_t size, bool *ersetzt) {
    /* Zeile 0 liegt zusammenhaengend, die Zeilen 1..4 spaltenweise
     * verschraenkt — so steht es in der Vorlage. */
    (void)text_kopieren(r->disk_label[0], sizeof(r->disk_label[0]),
                        data, size, (size_t)LABEL_BLOCK_OFFSET + 8u,
                        UFT_ROLAND_S_LABEL_COLUMNS, ersetzt);

    for (unsigned row = 1u; row < UFT_ROLAND_S_LABEL_ROWS; row++) {
        for (unsigned col = 0u; col < UFT_ROLAND_S_LABEL_COLUMNS; col++) {
            const size_t off = (size_t)LABEL_BLOCK_OFFSET + 20u
                             + (size_t)col * 4u + row - 1u;
            if (!passt(size, off, 1u)) { r->disk_label[row][col] = '\0'; continue; }
            const uint8_t c = data[off];
            if (druckbar(c)) {
                r->disk_label[row][col] = (char)c;
            } else {
                r->disk_label[row][col] = '.';
                if (ersetzt) *ersetzt = true;
            }
        }
        r->disk_label[row][UFT_ROLAND_S_LABEL_COLUMNS] = '\0';
        for (size_t i = UFT_ROLAND_S_LABEL_COLUMNS;
             i > 0u && r->disk_label[row][i - 1u] == ' '; i--)
            r->disk_label[row][i - 1u] = '\0';
    }
}

/* ── Oeffentlich ──────────────────────────────────────────────────────── */

uft_roland_s_status_t uft_roland_s_analyze(const uint8_t *image,
                                           size_t image_size,
                                           uft_roland_s_report_t *report) {
    if (!image || !report) return UFT_ROLAND_S_INVALID_ARGUMENT;

    memset(report, 0, sizeof(*report));
    report->schema_version = UFT_ROLAND_S_SCHEMA_VERSION;
    report->source_size    = image_size;

    /* Die Groesse ZUERST — `uft_roland_identify()` sieht nur Sektor 0 und
     * kann ueber die Datei nichts sagen. */
    if (image_size != (size_t)UFT_ROLAND_IMAGE_SIZE)
        return UFT_ROLAND_S_TRUNCATED;

    /* EINE Erkennung, und es ist die vorhandene. Siehe Header. */
    const uft_roland_id_t *id =
        uft_roland_identify(image, (size_t)UFT_ROLAND_IDENT_LEN);
    if (!id) return UFT_ROLAND_S_UNRECOGNIZED;

    report->erkannt = true;
    snprintf(report->model, sizeof(report->model), "%s",
             id->model ? id->model : "");
    snprintf(report->content, sizeof(report->content), "%s",
             id->content ? id->content : "");

    const bool s50 = (id->model && strcmp(id->model, "S50") == 0);
    bool ersetzt = false;

    etikett_lesen(report, image, image_size, &ersetzt);

    for (size_t i = 0; i < UFT_ROLAND_S_PATCH_COUNT; i++) {
        const size_t bank = (i < 8u) ? (size_t)PATCH_BANK_1_OFFSET
                                     : (size_t)PATCH_BANK_2_OFFSET;
        const size_t off = bank + (i % 8u) * (size_t)PATCH_RECORD_SIZE;
        patch_lesen(report, image, image_size, i, off, s50, &ersetzt);
    }
    report->patch_count = UFT_ROLAND_S_PATCH_COUNT;

    for (size_t i = 0; i < UFT_ROLAND_S_TONE_COUNT; i++)
        tone_lesen(report, image, image_size, i, &ersetzt);
    report->tone_count = UFT_ROLAND_S_TONE_COUNT;

    if (ersetzt) {
        warnung(report, UFT_ROLAND_S_WARNING_NONPRINTABLE_TEXT, 0,
                "Mindestens ein Textfeld enthielt nicht druckbare Bytes; "
                "sie stehen als '.' im Bericht.");
    }
    return UFT_ROLAND_S_OK;
}

/* ── JSON ─────────────────────────────────────────────────────────────── */

static void json_text(char *ziel, size_t n, const char *s) {
    size_t o = 0;
    for (; s && *s && o + 2u < n; s++) {
        const unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            if (o + 3u >= n) break;
            ziel[o++] = '\\'; ziel[o++] = (char)c;
        } else if (c < 0x20u) {
            if (o + 7u >= n) break;
            o += (size_t)snprintf(ziel + o, n - o, "\\u%04X", (unsigned)c);
        } else {
            ziel[o++] = (char)c;
        }
    }
    ziel[o < n ? o : n - 1u] = '\0';
}

uft_roland_s_status_t uft_roland_s_report_to_json_alloc(
    const uft_roland_s_report_t *report, char **json_out,
    size_t *json_size_out) {
    if (!report || !json_out) return UFT_ROLAND_S_INVALID_ARGUMENT;
    *json_out = NULL;
    if (json_size_out) *json_size_out = 0u;

    const size_t cap = 32768u;
    char *buf = (char *)malloc(cap);
    if (!buf) return UFT_ROLAND_S_NO_MEMORY;

    char esc[256];
    size_t o = 0u;
#define APP(...) do { \
        const int _n = snprintf(buf + o, cap - o, __VA_ARGS__); \
        if (_n < 0 || (size_t)_n >= cap - o) { free(buf); return UFT_ROLAND_S_NO_MEMORY; } \
        o += (size_t)_n; \
    } while (0)

    APP("{\"schemaVersion\":%u,\"sourceSize\":%zu,\"recognized\":%s",
        (unsigned)report->schema_version, report->source_size,
        report->erkannt ? "true" : "false");
    json_text(esc, sizeof(esc), report->model);
    APP(",\"model\":\"%s\"", esc);
    json_text(esc, sizeof(esc), report->content);
    APP(",\"content\":\"%s\"", esc);

    APP(",\"diskLabel\":[");
    for (unsigned r = 0u; r < UFT_ROLAND_S_LABEL_ROWS; r++) {
        json_text(esc, sizeof(esc), report->disk_label[r]);
        APP("%s\"%s\"", r ? "," : "", esc);
    }
    APP("]");

    APP(",\"patches\":[");
    for (size_t i = 0; i < report->patch_count; i++) {
        const uft_roland_s_patch_t *p = &report->patches[i];
        json_text(esc, sizeof(esc), p->name);
        APP("%s{\"number\":%u,\"populated\":%s,\"name\":\"%s\"",
            i ? "," : "", p->number, p->populated ? "true" : "false", esc);
        if (p->output_jack_available) {
            json_text(esc, sizeof(esc), p->output_jack);
            APP(",\"outputJack\":\"%s\"", esc);
        } else {
            APP(",\"outputJack\":null");
        }
        APP("}");
    }
    APP("]");

    APP(",\"tones\":[");
    for (size_t i = 0; i < report->tone_count; i++) {
        const uft_roland_s_tone_t *t = &report->tones[i];
        json_text(esc, sizeof(esc), t->name);
        APP("%s{\"number\":%u,\"populated\":%s,\"name\":\"%s\""
            ",\"isSubtone\":%s,\"sampleRateHz\":%u,\"waveBank\":\"%s\"",
            i ? "," : "", t->number, t->populated ? "true" : "false", esc,
            t->is_subtone ? "true" : "false", t->sample_rate_hz,
            uft_roland_s_wave_bank_name(t->wave_bank));
        if (t->sample_length_available)
            APP(",\"sampleDurationMs\":%u", (unsigned)t->sample_duration_ms);
        else
            APP(",\"sampleDurationMs\":null");
        APP("}");
    }
    APP("]");

    APP(",\"usedMsA\":%u,\"usedMsB\":%u",
        (unsigned)report->used_ms_a, (unsigned)report->used_ms_b);

    APP(",\"warnings\":[");
    for (size_t i = 0; i < report->warning_count; i++) {
        json_text(esc, sizeof(esc), report->warnings[i].message);
        APP("%s{\"kind\":\"%s\",\"tone\":%d,\"message\":\"%s\"}",
            i ? "," : "", uft_roland_s_warning_name(report->warnings[i].kind),
            report->warnings[i].tone_number, esc);
    }
    APP("],\"warningsTruncated\":%s}",
        report->warnings_truncated ? "true" : "false");
#undef APP

    *json_out = buf;
    if (json_size_out) *json_size_out = o;
    return UFT_ROLAND_S_OK;
}

/* ── Namen ────────────────────────────────────────────────────────────── */

const char *uft_roland_s_status_name(uft_roland_s_status_t s) {
    switch (s) {
    case UFT_ROLAND_S_OK:               return "OK";
    case UFT_ROLAND_S_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case UFT_ROLAND_S_UNRECOGNIZED:     return "UNRECOGNIZED";
    case UFT_ROLAND_S_TRUNCATED:        return "TRUNCATED";
    case UFT_ROLAND_S_IO_ERROR:         return "IO_ERROR";
    case UFT_ROLAND_S_NO_MEMORY:        return "NO_MEMORY";
    default:                            return NULL;
    }
}

const char *uft_roland_s_wave_bank_name(uft_roland_s_wave_bank_t b) {
    switch (b) {
    case UFT_ROLAND_S_BANK_NONE:    return "none";
    case UFT_ROLAND_S_BANK_A:       return "A";
    case UFT_ROLAND_S_BANK_B:       return "B";
    case UFT_ROLAND_S_BANK_UNKNOWN: return "unknown";
    default:                        return NULL;
    }
}

const char *uft_roland_s_warning_name(uft_roland_s_warning_kind_t k) {
    switch (k) {
    case UFT_ROLAND_S_WARNING_INVALID_SAMPLE_RATE: return "invalid_sample_rate";
    case UFT_ROLAND_S_WARNING_INVALID_WAVE_BANK:   return "invalid_wave_bank";
    case UFT_ROLAND_S_WARNING_NONPRINTABLE_TEXT:   return "nonprintable_text";
    default:                                       return NULL;
    }
}
