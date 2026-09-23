/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_roland_s_metadata.c
 * @brief Waechter fuer die semantische Roland-S-Auswertung (MF-1321).
 *
 * ── Was dieser Test BELEGT und was nicht ───────────────────────────────
 *
 * Er belegt, dass der Leser tut, was sein Header zu tun behauptet: an den
 * genannten Versaetzen lesen, Grenzen einhalten, bei Unkenntnis absagen,
 * ganzzahlig rechnen, nichts stillschweigend kuerzen.
 *
 * Er belegt NICHT, dass die Deutung der Roland-Felder stimmt. Die
 * Pruefdatei wird hier SELBST gebaut, mit genau den Versaetzen, die der
 * Leser erwartet — das ist ein geschlossener Kreis, die Bauform aus
 * MF-1009 (`apridisk`) und MF-1028 (`qrst`). Solange kein echtes
 * Roland-Abbild mit bekanntem Inhalt oder ein fremdes Werkzeug vorliegt,
 * das dieselben Felder nennt, bleibt das Modul im Sinne der
 * EINFRIER-REGEL UNGEPRUEFT. Der Satz steht auch im Header; er steht hier
 * ein zweites Mal, weil ein gruener Test sonst wie ein Beleg aussieht.
 *
 * ── Warum kein `assert()` ──────────────────────────────────────────────
 *
 * Die Vorlage nutzte 39 nackte `assert()`. Unter `NDEBUG` verschwinden
 * die restlos, und ein Test, der dann bedingungslos 0 zurueckgibt, ist
 * die Torform von „kann nicht rot werden" (MF-1000). Hier prueft ein
 * Makro, das immer bleibt.
 */

#include "uft/formats/uft_roland_s_metadata.h"
#include "uft/formats/uft_roland_ident.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define PRUEFE(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)

/* ── Pruefdatei ───────────────────────────────────────────────────────── */

/* Versaetze doppelt gefuehrt: der Test darf NICHT dieselbe Quelle
 * befragen wie der Pruefling, sonst bewegen sich beide zusammen und die
 * Zusage saegt sich selbst durch (Klasse MF-1000). */
enum {
    T_PATCH_BANK_1 = 64512,
    T_PATCH_BANK_2 = 66560,
    T_PATCH_RECORD = 256,
    T_PATCH_JACK   = 243,
    T_LABEL_BLOCK  = 68552,
    T_TONE_TABLE   = 69120,
    T_TONE_RECORD  = 16,
    T_TONE_PARAM   = 9
};

static void schreib_text(uint8_t *img, size_t off, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        img[off + i] = (uint8_t)(s[i] ? s[i] : ' ');
}

/** Ein gueltiges Abbild nach dem ERSTEN Eintrag der Erkennungstafel. */
static uint8_t *bau_abbild(const uft_roland_id_t **id_out) {
    const uft_roland_id_t *id = uft_roland_id_at(0);
    if (!id) return NULL;
    if (id_out) *id_out = id;

    uint8_t *img = (uint8_t *)malloc((size_t)UFT_ROLAND_IMAGE_SIZE);
    if (!img) return NULL;
    memset(img, ' ', (size_t)UFT_ROLAND_IMAGE_SIZE);

    memcpy(img + UFT_ROLAND_KEY_OFF_LO, id->key_lo, 4u);
    memcpy(img + UFT_ROLAND_KEY_OFF_HI, id->key_hi, 4u);

    /* Etikett: Zeile 0 zusammenhaengend, Zeilen 1..4 spaltenweise. */
    schreib_text(img, (size_t)T_LABEL_BLOCK + 8u, "UFT-KORPUS 1", 12u);
    for (unsigned row = 1u; row < UFT_ROLAND_S_LABEL_ROWS; row++)
        for (unsigned col = 0u; col < UFT_ROLAND_S_LABEL_COLUMNS; col++)
            img[(size_t)T_LABEL_BLOCK + 20u + (size_t)col * 4u + row - 1u] =
                (uint8_t)('A' + (int)row);

    /* Patch 1 und Patch 9 (zweite Bank), mit verschiedenen Buchsen. */
    schreib_text(img, (size_t)T_PATCH_BANK_1, "BASS  LOUD  ", 12u);
    img[(size_t)T_PATCH_BANK_1 + T_PATCH_JACK] = 3u;    /* -> "4" */
    schreib_text(img, (size_t)T_PATCH_BANK_2, "STRINGS SOFT", 12u);
    img[(size_t)T_PATCH_BANK_2 + T_PATCH_JACK] = 8u;    /* -> "T" */
    /* Patch 2 bleibt leer -> populated == false. */

    /* Tone 0: 30 kHz, Bank A, Laenge 10 Einheiten = 4000 ms. */
    schreib_text(img, (size_t)T_TONE_TABLE, "KICK    ", 8u);
    uint8_t *p0 = img + (size_t)T_TONE_TABLE + T_TONE_PARAM;
    p0[0] = 0u; p0[1] = 0u; p0[2] = 0u; p0[3] = 0u; p0[4] = 0u;
    p0[5] = 0u; p0[6] = 10u;

    /* Tone 1: 15 kHz, Bank B, Subton -> zaehlt NICHT in die Summe. */
    schreib_text(img, (size_t)T_TONE_TABLE + T_TONE_RECORD, "SNARE   ", 8u);
    uint8_t *p1 = img + (size_t)T_TONE_TABLE + T_TONE_RECORD + T_TONE_PARAM;
    p1[0] = 0u; p1[1] = 1u; p1[2] = 1u; p1[3] = 0u; p1[4] = 1u;
    p1[5] = 0u; p1[6] = 5u;

    /* Tone 2: unbekannte Rate UND unbekannte Bank -> zwei Warnungen. */
    schreib_text(img, (size_t)T_TONE_TABLE + 2u * T_TONE_RECORD, "WEIRD   ", 8u);
    uint8_t *p2 = img + (size_t)T_TONE_TABLE + 2u * T_TONE_RECORD + T_TONE_PARAM;
    p2[0] = 0u; p2[1] = 0u; p2[2] = 0x7Fu; p2[3] = 0u; p2[4] = 0x7Fu;
    p2[5] = 0u; p2[6] = 3u;

    /* Tone 3: Laenge 0xFF -> nicht verfuegbar, Dauer 0. */
    schreib_text(img, (size_t)T_TONE_TABLE + 3u * T_TONE_RECORD, "NOLEN   ", 8u);
    uint8_t *p3 = img + (size_t)T_TONE_TABLE + 3u * T_TONE_RECORD + T_TONE_PARAM;
    p3[0] = 0u; p3[1] = 0u; p3[2] = 0u; p3[3] = 0u; p3[4] = 0u;
    p3[5] = 0u; p3[6] = 0xFFu;

    /* Tone 4: nicht druckbares Byte im Namen -> Ersetzung wird gemeldet. */
    schreib_text(img, (size_t)T_TONE_TABLE + 4u * T_TONE_RECORD, "BAD     ", 8u);
    img[(size_t)T_TONE_TABLE + 4u * T_TONE_RECORD + 3u] = 0x01u;

    return img;
}

/* ── 1. Erkennung und Absagen ─────────────────────────────────────────── */

static void t1_absagen(void) {
    printf("Test 1: ohne Erkennung wird nichts behauptet\n");
    uft_roland_s_report_t r;

    PRUEFE(uft_roland_s_analyze(NULL, (size_t)UFT_ROLAND_IMAGE_SIZE, &r)
               == UFT_ROLAND_S_INVALID_ARGUMENT, "NULL-Abbild nicht abgewiesen");

    uint8_t klein[16] = { 0 };
    PRUEFE(uft_roland_s_analyze(klein, sizeof klein, NULL)
               == UFT_ROLAND_S_INVALID_ARGUMENT, "NULL-Ziel nicht abgewiesen");

    /* Falsche Groesse: TRUNCATED, und zwar VOR der Erkennung — sonst
     * liesse sich ein Sektor 0 mit gueltigem Schluessel als ganze
     * Diskette ausgeben. */
    uint8_t *img = bau_abbild(NULL);
    if (!img) { printf("  kein Tafeleintrag\n"); g_fail++; return; }
    PRUEFE(uft_roland_s_analyze(img, (size_t)UFT_ROLAND_IMAGE_SIZE - 1u, &r)
               == UFT_ROLAND_S_TRUNCATED, "zu kleine Datei nicht abgewiesen");
    PRUEFE(r.erkannt == false, "abgewiesene Datei gilt als erkannt");

    /* Richtige Groesse, falscher Schluessel: UNRECOGNIZED. */
    img[UFT_ROLAND_KEY_OFF_LO] = (uint8_t)(img[UFT_ROLAND_KEY_OFF_LO] ^ 0xFFu);
    PRUEFE(uft_roland_s_analyze(img, (size_t)UFT_ROLAND_IMAGE_SIZE, &r)
               == UFT_ROLAND_S_UNRECOGNIZED, "falscher Schluessel angenommen");
    PRUEFE(r.patch_count == 0u && r.tone_count == 0u,
           "nach UNRECOGNIZED stehen trotzdem %zu Patches / %zu Tones da",
           r.patch_count, r.tone_count);

    free(img);
    printf("    NULL, zu klein, falscher Schluessel: alle abgesagt\n");
}

/* ── 2. Der gute Fall ─────────────────────────────────────────────────── */

static void t2_inhalt(void) {
    printf("Test 2: die Felder kommen an ihrer eigenen Stelle an\n");
    const uft_roland_id_t *id = NULL;
    uint8_t *img = bau_abbild(&id);
    if (!img) { printf("  kein Tafeleintrag\n"); g_fail++; return; }

    uft_roland_s_report_t r;
    PRUEFE(uft_roland_s_analyze(img, (size_t)UFT_ROLAND_IMAGE_SIZE, &r)
               == UFT_ROLAND_S_OK, "gueltiges Abbild abgewiesen");
    PRUEFE(r.erkannt, "nicht als erkannt gemeldet");
    PRUEFE(strcmp(r.model, id->model) == 0,
           "Modell \"%s\" statt \"%s\"", r.model, id->model);
    PRUEFE(strcmp(r.content, id->content) == 0,
           "Inhalt \"%s\" statt \"%s\"", r.content, id->content);

    PRUEFE(strcmp(r.disk_label[0], "UFT-KORPUS 1") == 0,
           "Etikettzeile 0: \"%s\"", r.disk_label[0]);
    PRUEFE(strcmp(r.disk_label[1], "BBBBBBBBBBBB") == 0,
           "Etikettzeile 1: \"%s\"", r.disk_label[1]);

    PRUEFE(r.patch_count == UFT_ROLAND_S_PATCH_COUNT, "Patchzahl %zu",
           r.patch_count);
    PRUEFE(r.patches[0].populated, "Patch 1 gilt als leer");
    PRUEFE(strcmp(r.patches[0].name, "BASS  LOUD") == 0,
           "Patch 1 heisst \"%s\"", r.patches[0].name);
    PRUEFE(!r.patches[1].populated, "Patch 2 gilt als belegt");
    PRUEFE(strcmp(r.patches[8].name, "STRINGS SOFT") == 0,
           "Patch 9 (zweite Bank) heisst \"%s\"", r.patches[8].name);

    /* Die Buchse: 3 -> "4", 8 -> "T". Ein Off-by-one hier waere eine
     * falsche Auskunft ueber die Verkabelung. */
    PRUEFE(strcmp(r.patches[0].output_jack, "4") == 0,
           "Buchse Patch 1: \"%s\" statt \"4\"", r.patches[0].output_jack);
    PRUEFE(strcmp(r.patches[8].output_jack, "T") == 0,
           "Buchse Patch 9: \"%s\" statt \"T\"", r.patches[8].output_jack);

    /* Tone-Nummern sind I11..I48, nicht 0..31. */
    PRUEFE(r.tones[0].number == 11u, "Tone 0 heisst I%u", r.tones[0].number);
    PRUEFE(r.tones[8].number == 21u, "Tone 8 heisst I%u", r.tones[8].number);
    PRUEFE(r.tones[31].number == 48u, "Tone 31 heisst I%u", r.tones[31].number);

    /* GANZZAHLIG: 10 Einheiten x 400 ms = 4000 ms. Die Vorlage rechnete
     * `10 * 0.4` in `double`. */
    PRUEFE(r.tones[0].sample_rate_hz == 30000u,
           "Tone 0 Rate %u", r.tones[0].sample_rate_hz);
    PRUEFE(r.tones[0].wave_bank == UFT_ROLAND_S_BANK_A, "Tone 0 Bank falsch");
    PRUEFE(r.tones[0].sample_duration_ms == 4000u,
           "Tone 0 Dauer %u ms statt 4000", r.tones[0].sample_duration_ms);

    PRUEFE(r.tones[1].is_subtone, "Tone 1 nicht als Subton erkannt");
    PRUEFE(r.tones[1].sample_rate_hz == 15000u,
           "Tone 1 Rate %u", r.tones[1].sample_rate_hz);

    PRUEFE(!r.tones[3].sample_length_available,
           "0xFF gilt als verfuegbare Laenge");
    PRUEFE(r.tones[3].sample_duration_ms == 0u,
           "Tone 3 Dauer %u statt 0", r.tones[3].sample_duration_ms);

    /* Die Summe: nur Tone 0 zaehlt. Tone 1 ist Subton, Tone 2 hat eine
     * unbekannte Bank, Tone 3 keine Laenge. */
    PRUEFE(r.used_ms_a == 4000u, "Bank A: %u ms statt 4000", r.used_ms_a);
    PRUEFE(r.used_ms_b == 0u,
           "Bank B: %u ms statt 0 — ein Subton wurde mitgezaehlt", r.used_ms_b);

    free(img);
    printf("    Etikett, Patches, Buchsen, Tones, Ganzzahl-Dauern: halten\n");
}

/* ── 3. Warnungen ─────────────────────────────────────────────────────── */

static void t3_warnungen(void) {
    printf("Test 3: was nicht deutbar ist, wird gemeldet\n");
    uint8_t *img = bau_abbild(NULL);
    if (!img) { printf("  kein Tafeleintrag\n"); g_fail++; return; }

    uft_roland_s_report_t r;
    (void)uft_roland_s_analyze(img, (size_t)UFT_ROLAND_IMAGE_SIZE, &r);

    bool rate = false, bank = false, text = false;
    for (size_t i = 0; i < r.warning_count; i++) {
        switch (r.warnings[i].kind) {
        case UFT_ROLAND_S_WARNING_INVALID_SAMPLE_RATE: rate = true; break;
        case UFT_ROLAND_S_WARNING_INVALID_WAVE_BANK:   bank = true; break;
        case UFT_ROLAND_S_WARNING_NONPRINTABLE_TEXT:   text = true; break;
        default: break;
        }
    }
    PRUEFE(rate, "unbekannte Abtastrate nicht gemeldet");
    PRUEFE(bank, "unbekannte Wellenbank nicht gemeldet");
    PRUEFE(text, "ersetztes Byte im Namen nicht gemeldet");
    PRUEFE(!r.warnings_truncated, "Warnungen gekuerzt, obwohl es nur drei gibt");

    /* Gegenprobe: ein sauberes Abbild darf KEINE Warnung erzeugen.
     * Ohne sie waere nicht belegt, dass die Meldungen an den Befunden
     * haengen und nicht einfach immer kommen. */
    uint8_t *sauber = bau_abbild(NULL);
    if (sauber) {
        /* BEIDE benannten Tones saeubern, nicht nur den einen.
         *
         * Die erste Fassung saeuberte nur Tone 2 — und blieb rot, weil
         * Tone 4 seine Parameterbytes aus der Leerzeichen-Fuellung des
         * Abbilds bezieht: 0x20 ist weder eine bekannte Rate noch eine
         * bekannte Bank. Der Leser lag richtig, die Gegenprobe war
         * nachlaessig gebaut. */
        uint8_t *p2 = sauber + (size_t)T_TONE_TABLE + 2u * T_TONE_RECORD
                    + T_TONE_PARAM;
        p2[2] = 0u; p2[4] = 0u; p2[6] = 1u;
        uint8_t *p4 = sauber + (size_t)T_TONE_TABLE + 4u * T_TONE_RECORD
                    + T_TONE_PARAM;
        p4[0] = 0u; p4[1] = 0u; p4[2] = 0u; p4[3] = 0u;
        p4[4] = 0u; p4[5] = 0u; p4[6] = 1u;
        sauber[(size_t)T_TONE_TABLE + 4u * T_TONE_RECORD + 3u] = (uint8_t)'X';
        uft_roland_s_report_t s;
        (void)uft_roland_s_analyze(sauber, (size_t)UFT_ROLAND_IMAGE_SIZE, &s);
        PRUEFE(s.warning_count == 0u,
               "sauberes Abbild erzeugt %zu Warnungen", s.warning_count);
        free(sauber);
    }

    free(img);
    printf("    drei Befunde gemeldet, sauberes Abbild schweigt\n");
}

/* ── 4. JSON ──────────────────────────────────────────────────────────── */

static void t4_json(void) {
    printf("Test 4: JSON ist ganzzahlig und maskiert\n");
    uint8_t *img = bau_abbild(NULL);
    if (!img) { printf("  kein Tafeleintrag\n"); g_fail++; return; }

    /* Ein Anfuehrungszeichen und ein Backslash in einen Namen, damit die
     * Maskierung etwas zu tun hat. */
    img[(size_t)T_PATCH_BANK_1 + 0u] = (uint8_t)'"';
    img[(size_t)T_PATCH_BANK_1 + 1u] = (uint8_t)'\\';

    uft_roland_s_report_t r;
    (void)uft_roland_s_analyze(img, (size_t)UFT_ROLAND_IMAGE_SIZE, &r);

    char *json = NULL; size_t n = 0u;
    PRUEFE(uft_roland_s_report_to_json_alloc(&r, &json, &n) == UFT_ROLAND_S_OK,
           "JSON nicht erzeugt");
    if (json) {
        PRUEFE(n > 0u && strlen(json) == n, "Laenge %zu passt nicht", n);
        PRUEFE(strstr(json, "\\\"") != NULL, "Anfuehrungszeichen nicht maskiert");
        PRUEFE(strstr(json, "\\\\") != NULL, "Backslash nicht maskiert");
        PRUEFE(strstr(json, "\"sampleDurationMs\":4000") != NULL,
               "die Dauer steht nicht ganzzahlig im JSON");
        PRUEFE(strstr(json, "\"sampleDurationMs\":null") != NULL,
               "fehlende Laenge steht nicht als null");
        free(json);
    }

    json = NULL;
    PRUEFE(uft_roland_s_report_to_json_alloc(NULL, &json, &n)
               == UFT_ROLAND_S_INVALID_ARGUMENT, "NULL-Bericht angenommen");

    free(img);
    printf("    Maskierung, Ganzzahl-Dauer, null bei fehlender Laenge\n");
}

/* ── 5. Namen ─────────────────────────────────────────────────────────── */

static void t5_namen(void) {
    printf("Test 5: jeder Aufzaehlungswert hat einen Namen, Unsinn keinen\n");
    PRUEFE(uft_roland_s_status_name(UFT_ROLAND_S_OK) != NULL, "OK ohne Namen");
    PRUEFE(uft_roland_s_status_name((uft_roland_s_status_t)99) == NULL,
           "ein unbekannter Status bekommt einen Namen");
    PRUEFE(uft_roland_s_wave_bank_name(UFT_ROLAND_S_BANK_UNKNOWN) != NULL,
           "BANK_UNKNOWN ohne Namen");
    PRUEFE(uft_roland_s_wave_bank_name((uft_roland_s_wave_bank_t)99) == NULL,
           "eine unbekannte Bank bekommt einen Namen");
    PRUEFE(uft_roland_s_warning_name((uft_roland_s_warning_kind_t)99) == NULL,
           "eine unbekannte Warnung bekommt einen Namen");
    printf("    drei Namenstafeln, je mit Gegenprobe\n");
}

int main(void) {
    printf("=== Roland S: semantische Auswertung (MF-1321) ===\n\n");
    t1_absagen();
    t2_inhalt();
    t3_warnungen();
    t4_json();
    t5_namen();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN",
           g_fail);
    return g_fail ? 1 : 0;
}
