/**
 * @file test_write_gate.c
 * @brief Tests for the write-safety gate (restored from v3.7.0).
 *
 * Verifies the fail-closed policy layer: format capability, drive
 * diagnostics, snapshot verification.  "Kein Bit verloren" —
 * destructive operations must never proceed without explicit pass.
 *
 * ── Was bis MF-490 fehlte ────────────────────────
 *
 * Diese Datei pruefte ausschliesslich die ARGUMENTPRUEFUNG: Nullzeiger,
 * Bit-Distinktheit der Flags, Statustexte. Keine einzige ENTSCHEIDUNG des
 * Tores war geprueft — nicht, ob es eine schreibgeschuetzte Diskette
 * abweist, nicht, ob es den Schnappschuss wirklich anlegt, und vor allem
 * nicht, ob es ueberhaupt je „ja“ sagen kann.
 *
 * Das ist bei einem Sicherheitsmechanismus die gefaehrlichste Luecke: ein
 * Tor, das immer „nein“ sagt, faellt im Betrieb sofort auf. Ein Tor, das
 * immer „ja“ sagt, faellt nie auf — bis das erste Medium ueberschrieben
 * ist.
 *
 * Der Header dieses Tores sagt woertlich: „This MUST be called before any
 * destructive operation.“ Gemessen: uft_write_gate.c hat KEINEN Aufrufer
 * ausserhalb dieser Tests (siehe KNOWN_ISSUES POL-1). Die Tests unten
 * aendern daran nichts — sie machen die Verdrahtung pruefbar, statt sie
 * blind vorzunehmen.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "uft/policy/uft_write_gate.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ------------------------------------------------------------------ */
/* Status string helpers                                               */
/* ------------------------------------------------------------------ */

TEST(status_str_ok) {
    const char *s = uft_gate_status_str(UFT_GATE_OK);
    ASSERT(s != NULL);
    ASSERT(s[0] != '\0');
}

TEST(status_str_every_code_has_string) {
    const uft_gate_status_t codes[] = {
        UFT_GATE_OK,
        UFT_GATE_FORMAT_READONLY,
        UFT_GATE_DRIVE_UNSAFE,
        UFT_GATE_SNAPSHOT_FAILED,
        UFT_GATE_VERIFY_FAILED,
        UFT_GATE_NEEDS_OVERRIDE,
        UFT_GATE_PRECHECK_FAILED,
    };
    size_t n = sizeof(codes) / sizeof(codes[0]);
    for (size_t i = 0; i < n; i++) {
        const char *s = uft_gate_status_str(codes[i]);
        ASSERT(s != NULL);
        ASSERT(s[0] != '\0');
    }
}

/* ------------------------------------------------------------------ */
/* Precheck fail-closed semantics                                     */
/* ------------------------------------------------------------------ */

TEST(precheck_null_policy_rejected) {
    uint8_t img[256] = {0};
    uft_write_gate_result_t r;
    uft_gate_status_t s = uft_write_gate_precheck(
        NULL, img, sizeof(img), "/tmp", "snap", &r);
    ASSERT(s != UFT_GATE_OK);
}

TEST(precheck_null_result_rejected) {
    uft_write_gate_policy_t pol = {0};
    uint8_t img[256] = {0};
    uft_gate_status_t s = uft_write_gate_precheck(
        &pol, img, sizeof(img), "/tmp", "snap", NULL);
    ASSERT(s != UFT_GATE_OK);
}

TEST(precheck_zero_image_rejected) {
    uft_write_gate_policy_t pol = { .require_format_check = true,
                                     .min_confidence = 0 };
    uft_write_gate_result_t r = {0};
    uft_gate_status_t s = uft_write_gate_precheck(
        &pol, NULL, 0, "/tmp", "snap", &r);
    ASSERT(s != UFT_GATE_OK);
}

/* ------------------------------------------------------------------ */
/* Override semantics                                                  */
/* ------------------------------------------------------------------ */

TEST(override_only_when_flagged_possible) {
    uft_write_gate_result_t r = {0};
    /* Unpopulated result → should NOT be overridable by default */
    bool ovr = uft_write_gate_can_override(&r);
    (void)ovr;    /* either answer is acceptable for a zeroed result;
                    this test ensures the function doesn't crash. */
}

/* ------------------------------------------------------------------ */
/* Drive diagnostics wiring                                            */
/* ------------------------------------------------------------------ */

TEST(diag_flags_are_bit_distinct) {
    /* Each flag must occupy a unique bit so they can be OR'd. */
    uint32_t all = UFT_DRIVE_DIAG_UNSTABLE_RPM |
                    UFT_DRIVE_DIAG_BAD_INDEX |
                    UFT_DRIVE_DIAG_BAD_SEEK |
                    UFT_DRIVE_DIAG_WRITE_UNSAFE |
                    UFT_DRIVE_DIAG_NO_DISK |
                    UFT_DRIVE_DIAG_WRITE_PROTECT;
    /* No overlap → popcount == 6 */
    int bits = 0;
    for (int i = 0; i < 32; i++) if ((all >> i) & 1u) bits++;
    ASSERT(bits == 6);
}

TEST(format_caps_are_bit_distinct) {
    uint32_t all = UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE |
                    UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_LOGICAL |
                    UFT_FMT_CAP_PROTECTED | UFT_FMT_CAP_VERIFY;
    int bits = 0;
    for (int i = 0; i < 32; i++) if ((all >> i) & 1u) bits++;
    ASSERT(bits == 6);
}

/* ------------------------------------------------------------------ */
/* Format probe                                                        */
/* ------------------------------------------------------------------ */

TEST(probe_null_inputs_return_error) {
    uft_format_probe_t p;
    uft_error_t e = uft_write_gate_probe_format(NULL, 100, &p);
    ASSERT(e != UFT_OK);

    uint8_t buf[32] = {0};
    e = uft_write_gate_probe_format(buf, sizeof(buf), NULL);
    ASSERT(e != UFT_OK);
}

TEST(probe_zero_len_rejected) {
    uft_format_probe_t p = {0};
    uint8_t buf[1] = {0};
    uft_error_t e = uft_write_gate_probe_format(buf, 0, &p);
    ASSERT(e != UFT_OK);
}


/* ------------------------------------------------------------------ */
/* Die Entscheidungen des Tores (MF-490)                               */
/* ------------------------------------------------------------------ */

static const char *temp_dir(void)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    return d;
}

/* 901120 Byte = ADF (Amiga DD), Konfidenz 900 in der Signaturtabelle. */
#define ADF_LEN  901120u

static uint8_t *make_adf(void)
{
    uint8_t *b = (uint8_t *)calloc(1, ADF_LEN);
    if (b) memcpy(b, "DOS\0", 4);
    return b;
}

TEST(the_gate_can_actually_say_yes)
{
    /* Der wichtigste Test der Datei. Ein Tor, das nie zustimmt, faellt im
     * Betrieb sofort auf; eines, das immer zustimmt, nie — bis das erste
     * Medium ueberschrieben ist. Ohne diesen Fall koennte jede Ablehnung
     * unten aus einem kaputten Tor stammen. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   temp_dir(), "uft_gate_ok",
                                                   &r);
    if (st != UFT_GATE_OK)
        printf("\n        Status %d: %s\n", (int)st, r.decision_reason);
    ASSERT(st == UFT_GATE_OK);
    ASSERT(r.override_required == false);
    ASSERT(r.checks_failed == 0);
    ASSERT((r.checks_passed & UFT_CHECK_FORMAT) != 0);
    ASSERT((r.checks_passed & UFT_CHECK_SNAPSHOT) != 0);
    ASSERT((r.checks_passed & UFT_CHECK_VERIFY) != 0);

    /* Der Schnappschuss ist wirklich da — nicht nur behauptet. */
    ASSERT(r.snapshot.path[0] != '\0');
    FILE *f = fopen(r.snapshot.path, "rb");
    ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    ASSERT(n > 0);
    remove(r.snapshot.path);

    free(img);
}

TEST(an_unknown_format_is_refused)
{
    /* Eine Groesse, die keine Signatur trifft. Das Tor darf nicht raten. */
    uint8_t junk[1237];
    memset(junk, 0x5A, sizeof(junk));

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, junk, sizeof(junk),
                                                   temp_dir(), "uft_gate_junk",
                                                   &r);
    ASSERT(st == UFT_GATE_PRECHECK_FAILED);
    ASSERT((r.checks_failed & UFT_CHECK_FORMAT) != 0);
    ASSERT(r.decision_reason[0] != '\0');
}

TEST(too_low_a_confidence_is_refused)
{
    /* Dasselbe Abbild, strengere Schwelle. Das trennt „Format unbekannt“
     * von „Format erkannt, aber nicht sicher genug“ — zwei verschiedene
     * Aussagen, die ein Tor nicht verwechseln darf. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    pol.min_confidence = 999;          /* ueber der ADF-Konfidenz 900 */

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   temp_dir(), "uft_gate_conf",
                                                   &r);
    ASSERT(st == UFT_GATE_PRECHECK_FAILED);
    ASSERT((r.checks_failed & UFT_CHECK_FORMAT) != 0);
    ASSERT(r.format.confidence == 900);   /* erkannt wurde es sehr wohl */

    free(img);
}

TEST(a_missing_snapshot_target_blocks_the_write)
{
    /* Verlangt die Policy einen Schnappschuss und gibt es keinen Ort dafuer,
     * ist das kein Grund weiterzumachen. Fail-closed heisst: im Zweifel
     * nicht schreiben. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   NULL, NULL, &r);
    ASSERT(st == UFT_GATE_SNAPSHOT_FAILED);
    ASSERT((r.checks_failed & UFT_CHECK_SNAPSHOT) != 0);

    free(img);
}

TEST(a_write_protected_disk_stops_the_gate)
{
    /* Die Diagnose sagt: die Diskette ist schreibgeschuetzt. Dann gibt es
     * nichts abzuwaegen. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    pol.require_drive_diag = true;

    uft_drive_diag_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.flags = UFT_DRIVE_DIAG_WRITE_PROTECT;

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &diag, temp_dir(), "uft_gate_wp", &r);
    ASSERT(st == UFT_GATE_DRIVE_UNSAFE);
    ASSERT((r.checks_failed & UFT_CHECK_DRIVE) != 0);
    ASSERT(r.override_required == false);   /* hier gibt es kein Ueberstimmen */

    free(img);
}

TEST(no_disk_in_the_drive_stops_the_gate)
{
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    pol.require_drive_diag = true;

    uft_drive_diag_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.flags = UFT_DRIVE_DIAG_NO_DISK;

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &diag, temp_dir(), "uft_gate_nodisk", &r);
    ASSERT(st == UFT_GATE_DRIVE_UNSAFE);
    ASSERT((r.checks_failed & UFT_CHECK_DRIVE) != 0);

    free(img);
}

TEST(an_unsafe_drive_blocks_unless_the_policy_allows_overriding)
{
    /* Zwei Ausgaenge desselben Befunds, und der Unterschied liegt in der
     * Policy — nicht im Zufall. Ohne Erlaubnis: Schluss. Mit Erlaubnis:
     * ueberstimmbar, aber ausdruecklich. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_drive_diag_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.flags = UFT_DRIVE_DIAG_WRITE_UNSAFE;

    uft_write_gate_policy_t strict = UFT_GATE_POLICY_IMAGE_ONLY;
    strict.require_drive_diag = true;
    strict.allow_unsafe_drive = false;

    uft_write_gate_result_t r1;
    uft_gate_status_t s1 = uft_write_gate_precheck_with_diag(
        &strict, img, ADF_LEN, &diag, temp_dir(), "uft_gate_u1", &r1);
    ASSERT(s1 == UFT_GATE_DRIVE_UNSAFE);
    ASSERT(r1.override_required == false);
    ASSERT(uft_write_gate_can_override(&r1) == false);

    uft_write_gate_policy_t lax = strict;
    lax.allow_unsafe_drive = true;

    uft_write_gate_result_t r2;
    uft_gate_status_t s2 = uft_write_gate_precheck_with_diag(
        &lax, img, ADF_LEN, &diag, temp_dir(), "uft_gate_u2", &r2);
    ASSERT(s2 == UFT_GATE_DRIVE_UNSAFE);
    ASSERT(r2.override_required == true);
    ASSERT(uft_write_gate_can_override(&r2) == true);
    if (r2.snapshot.path[0]) remove(r2.snapshot.path);

    free(img);
}

TEST(strict_mode_without_diagnostics_demands_an_override)
{
    /* Der Unterschied zwischen „ich habe nachgesehen und es ist gut“ und
     * „ich habe nicht nachgesehen“. Im strengen Modus ist das zweite kein
     * stilles Ja. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_STRICT;

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   temp_dir(), "uft_gate_str",
                                                   &r);
    ASSERT(st == UFT_GATE_NEEDS_OVERRIDE);
    ASSERT(r.override_required == true);
    ASSERT((r.checks_failed & UFT_CHECK_DRIVE) != 0);

    free(img);
}

TEST(a_relaxed_policy_without_diagnostics_still_passes)
{
    /* Gegenprobe zum vorigen: dieselbe fehlende Diagnose, nicht-strenger
     * Modus. Faellt dieser Test zusammen mit dem vorigen, unterscheidet das
     * Tor die Modi nicht — dann waere strict_mode ein Feld ohne Wirkung. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_STRICT;
    pol.strict_mode = false;

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   temp_dir(), "uft_gate_lax",
                                                   &r);
    if (st != UFT_GATE_OK)
        printf("\n        Status %d: %s\n", (int)st, r.decision_reason);
    ASSERT(st == UFT_GATE_OK);
    if (r.snapshot.path[0]) remove(r.snapshot.path);

    free(img);
}


/* ------------------------------------------------------------------ */
/* Der Read-only-Zweig (MF-491)                                        */
/* ------------------------------------------------------------------ */

/* Ein WOZ-Abbild: Magie "WOZ1" am Anfang. Das Plugin dazu kann NICHT
 * schreiben (src/formats/apple/uft_woz_plugin.c:94, kein write_track), und
 * seit MF-491 sagt das Tor dasselbe. */
static uint8_t *make_woz(size_t *len_out)
{
    const size_t n = 4096;
    uint8_t *b = (uint8_t *)calloc(1, n);
    if (b) memcpy(b, "WOZ1", 4);
    if (len_out) *len_out = n;
    return b;
}

TEST(a_format_without_a_writer_is_blocked)
{
    /* Bis MF-491 fuehrte das Tor WOZ als beschreibbar, obwohl kein Plugin
     * es schreiben kann. Der Read-only-Zweig war damit fuer dieses Format
     * unerreichbar \u2014 und ein durchgewinkter Schreibvorgang, den niemand
     * ausfuehren kann, ist genau die Sorte Vertrauen, die nichts traegt. */
    size_t n = 0;
    uint8_t *img = make_woz(&n);
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    /* IMAGE_ONLY erlaubt kein Ueberstimmen: hier ist Schluss. */
    ASSERT(pol.allow_readonly_override == false);

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, n,
                                                   temp_dir(), "uft_gate_ro",
                                                   &r);
    if (st != UFT_GATE_FORMAT_READONLY)
        printf("\n        Status %d: %s\n", (int)st, r.decision_reason);
    ASSERT(st == UFT_GATE_FORMAT_READONLY);
    ASSERT((r.checks_failed & UFT_CHECK_FORMAT) != 0);
    ASSERT(r.override_required == false);
    ASSERT(uft_write_gate_can_override(&r) == false);

    /* Und es wurde nicht einmal ein Schnappschuss angelegt \u2014 das Tor
     * bricht ab, bevor es Arbeit macht. */
    ASSERT((r.checks_passed & UFT_CHECK_SNAPSHOT) == 0);

    free(img);
}

TEST(a_read_only_format_can_be_overridden_when_the_policy_says_so)
{
    /* Die Gegenrichtung, und der Unterschied liegt in der Policy \u2014 nicht
     * im Zufall. RELAXED erlaubt das Ueberstimmen; dann meldet das Tor den
     * Befund und verlangt eine ausdrueckliche Entscheidung, statt still
     * durchzuwinken. */
    size_t n = 0;
    uint8_t *img = make_woz(&n);
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_RELAXED;
    ASSERT(pol.allow_readonly_override == true);

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, n,
                                                   temp_dir(), "uft_gate_ro2",
                                                   &r);
    ASSERT(st == UFT_GATE_FORMAT_READONLY);
    ASSERT(r.override_required == true);
    ASSERT(uft_write_gate_can_override(&r) == true);

    /* Hier laeuft das Tor weiter und legt den Schnappschuss trotzdem an \u2014
     * wer ueberstimmen will, soll wenigstens eine Sicherung haben. */
    ASSERT((r.checks_passed & UFT_CHECK_SNAPSHOT) != 0);
    if (r.snapshot.path[0]) remove(r.snapshot.path);

    free(img);
}

TEST(a_writable_format_takes_the_other_branch)
{
    /* Gegenprobe: dasselbe Tor, dieselbe Policy, ein Format MIT Schreiber.
     * Ohne sie koennte der Read-only-Zweig aus einem Tor stammen, das
     * grundsaetzlich alles ablehnt. */
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = UFT_GATE_POLICY_IMAGE_ONLY;
    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck(&pol, img, ADF_LEN,
                                                   temp_dir(), "uft_gate_rw",
                                                   &r);
    ASSERT(st == UFT_GATE_OK);
    ASSERT((r.checks_passed & UFT_CHECK_FORMAT) != 0);
    if (r.snapshot.path[0]) remove(r.snapshot.path);

    free(img);
}

int main(void) {
    printf("=== uft_write_gate tests ===\n");
    RUN(status_str_ok);
    RUN(status_str_every_code_has_string);
    RUN(precheck_null_policy_rejected);
    RUN(precheck_null_result_rejected);
    RUN(precheck_zero_image_rejected);
    RUN(override_only_when_flagged_possible);
    RUN(diag_flags_are_bit_distinct);
    RUN(format_caps_are_bit_distinct);

    /* MF-490: die Entscheidungen, nicht nur die Argumentpruefung */
    RUN(the_gate_can_actually_say_yes);
    RUN(an_unknown_format_is_refused);
    RUN(too_low_a_confidence_is_refused);
    RUN(a_missing_snapshot_target_blocks_the_write);
    RUN(a_write_protected_disk_stops_the_gate);
    RUN(no_disk_in_the_drive_stops_the_gate);
    RUN(an_unsafe_drive_blocks_unless_the_policy_allows_overriding);
    RUN(strict_mode_without_diagnostics_demands_an_override);
    RUN(a_relaxed_policy_without_diagnostics_still_passes);

    /* MF-491: der Read-only-Zweig, jetzt erreichbar */
    RUN(a_format_without_a_writer_is_blocked);
    RUN(a_read_only_format_can_be_overridden_when_the_policy_says_so);
    RUN(a_writable_format_takes_the_other_branch);
    RUN(probe_null_inputs_return_error);
    RUN(probe_zero_len_rejected);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
