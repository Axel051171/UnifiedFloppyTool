/**
 * @file test_provenance_chain.c
 * @brief Improvement test — UFT's forensic provenance chain-of-custody.
 *
 * P3.3 / task #110, forensic category. Makes the DESIGN_PRINCIPLES
 * "chain-of-custody" property executable: every processing step is
 * recorded in a SHA-256 hash chain, and any later tampering with a
 * recorded entry breaks verification.
 *
 * Why gw cannot pass this: Greaseweazle keeps no audit log at all — its
 * output is anonymous, with no record of what produced it or whether it
 * was altered afterwards. The provenance chain is a UFT-only forensic
 * layer; this test proves it actually detects tampering rather than
 * merely storing metadata.
 *
 * Tests OBSERVED behaviour of the public API in
 * include/uft/forensic/uft_provenance.h — never reaches into internals
 * beyond the documented struct fields a tamper would touch.
 *
 * NOTE: real CHECK-style macros, not assert() — the suite builds with
 * -DNDEBUG, under which assert() is a no-op.
 */
#include "uft/forensic/uft_provenance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* ── create / add: the chain records each step ────────────────────── */
TEST(create_yields_empty_valid_chain) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    ASSERT(ch->count == 0);
    ASSERT(ch->valid);                 /* fresh chain is valid */
    ASSERT(uft_prov_verify(ch));        /* an empty chain trivially verifies */
    uft_prov_free(ch);
}

TEST(add_records_step_metadata) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);

    const uint8_t data[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    int rc = uft_prov_add(ch, UFT_PROV_CAPTURE, data, sizeof(data),
                          "raw flux captured", "axel");
    ASSERT(rc == 0);
    ASSERT(ch->count == 1);

    const uft_prov_entry_t *e = &ch->entries[0];
    ASSERT(e->action == UFT_PROV_CAPTURE);
    ASSERT(e->data_size == sizeof(data));
    ASSERT(strcmp(e->description, "raw flux captured") == 0);
    ASSERT(strcmp(e->operator_id, "axel") == 0);
    ASSERT(e->tool_version[0] != '\0');          /* tool stamped */
    /* non-empty input -> a non-zero input hash was computed */
    int any = 0;
    for (int i = 0; i < UFT_PROV_HASH_SIZE; i++) any |= e->input_hash[i];
    ASSERT(any != 0);
    /* root_hash is seeded from the first entry's chain hash */
    ASSERT(memcmp(ch->root_hash, e->chain_hash, UFT_PROV_HASH_SIZE) == 0);

    uft_prov_free(ch);
}

/* ── a clean multi-step chain verifies ────────────────────────────── */
TEST(multi_step_chain_verifies) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);

    const uint8_t d1[] = { 1, 2, 3 };
    const uint8_t d2[] = { 4, 5, 6, 7, 8 };
    const uint8_t d3[] = { 9 };
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d1, sizeof(d1), "capture", "op") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_DECODE,  d2, sizeof(d2), "decode",  "op") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_EXPORT,  d3, sizeof(d3), "export",  "op") == 0);
    ASSERT(ch->count == 3);
    ASSERT(uft_prov_verify(ch));        /* untouched chain is intact */

    uft_prov_free(ch);
}

/* ── THE forensic property: tampering breaks the chain ────────────── */
TEST(tampering_with_data_size_is_detected) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0xAA, 0xBB };
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d, sizeof(d), "a", "op") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_DECODE,  d, sizeof(d), "b", "op") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_EXPORT,  d, sizeof(d), "c", "op") == 0);
    ASSERT(uft_prov_verify(ch));

    /* Alter a recorded entry after the fact — data_size feeds the
     * chain hash, so verification MUST now fail. */
    ch->entries[1].data_size += 1;
    ASSERT(!uft_prov_verify(ch));

    uft_prov_free(ch);
}

TEST(tampering_with_action_is_detected) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0x01 };
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d, sizeof(d), "a", "op") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_MODIFY,  d, sizeof(d), "b", "op") == 0);
    ASSERT(uft_prov_verify(ch));

    /* Re-label what was done — action feeds the chain hash. */
    ch->entries[1].action = UFT_PROV_VERIFY;
    ASSERT(!uft_prov_verify(ch));

    uft_prov_free(ch);
}

TEST(tampering_with_chain_hash_is_detected) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0x42 };
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d, sizeof(d), "a", "op") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_DECODE,  d, sizeof(d), "b", "op") == 0);
    ASSERT(uft_prov_verify(ch));

    /* Flip one bit of a stored chain hash — the link is broken and the
     * following entry no longer chains from it. */
    ch->entries[0].chain_hash[0] ^= 0x01;
    ASSERT(!uft_prov_verify(ch));

    uft_prov_free(ch);
}

/* ── coverage boundary: documented, observed, honest ──────────────── */
TEST(chain_covers_crypto_fields_not_freetext_metadata) {
    /* The chain hash is SHA256(prev || action || timestamp ||
     * input_hash || output_hash || data_size). The free-text
     * description / operator_id fields are NOT folded into it, so
     * editing them does NOT break verification. This is the observed
     * coverage boundary — recorded here as truth, not asserted as a
     * desirable property. (Whether the chain SHOULD also bind the
     * free-text metadata is a separate forensic-integrity question.) */
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0x7F };
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d, sizeof(d), "original", "op") == 0);
    ASSERT(uft_prov_verify(ch));

    strncpy(ch->entries[0].description, "ALTERED",
            sizeof(ch->entries[0].description) - 1);
    /* observed: still verifies — description is outside the hash scope */
    ASSERT(uft_prov_verify(ch));

    uft_prov_free(ch);
}

/* ── NULL / bounds discipline ─────────────────────────────────────── */
TEST(null_and_bounds_guards) {
    ASSERT(uft_prov_add(NULL, UFT_PROV_CAPTURE, NULL, 0, "x", "y") == -1);
    ASSERT(!uft_prov_verify(NULL));

    /* Fill the chain to capacity, then one more add must be rejected. */
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    for (uint32_t i = 0; i < UFT_PROV_MAX_ENTRIES; i++) {
        ASSERT(uft_prov_add(ch, UFT_PROV_ANALYZE, NULL, 0, "fill", "op") == 0);
    }
    ASSERT(ch->count == UFT_PROV_MAX_ENTRIES);
    ASSERT(uft_prov_add(ch, UFT_PROV_ANALYZE, NULL, 0, "overflow", "op") == -1);
    ASSERT(ch->count == UFT_PROV_MAX_ENTRIES);   /* rejected, not appended */
    ASSERT(uft_prov_verify(ch));                  /* full chain still intact */

    uft_prov_free(ch);
}

/* ── export produces an audit artifact gw never could ─────────────── */
TEST(export_json_writes_audit_artifact) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0x10, 0x20 };
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d, sizeof(d),
                        "captured from drive A", "axel") == 0);
    ASSERT(uft_prov_add(ch, UFT_PROV_EXPORT, d, sizeof(d),
                        "exported to disk.scp", "axel") == 0);

    const char *path = "uft_prov_test_tmp.json";
    remove(path);
    ASSERT(uft_prov_export_json(ch, path) == 0);

    FILE *f = fopen(path, "r");
    ASSERT(f != NULL);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* The artifact records the chain — what gw output never carries. */
    ASSERT(strstr(buf, "\"count\": 2") != NULL);
    ASSERT(strstr(buf, "\"root_hash\"") != NULL);
    ASSERT(strstr(buf, "\"chain_hash\"") != NULL);
    ASSERT(strstr(buf, "CAPTURE") != NULL);
    ASSERT(strstr(buf, "EXPORT") != NULL);
    ASSERT(strstr(buf, "captured from drive A") != NULL);

    remove(path);
    uft_prov_free(ch);
}

/* ── MF-1520 (P3-672): free text is ESCAPED in the audit artifact ──────
 *
 * uft_prov_export_json() wrote tool, operator and description with a bare
 * "%s". A file name with a quote or a backslash — "disk "A" side 1.scp",
 * "C:\archiv\x.scp" — made the forensic artifact invalid JSON (measured
 * with Python's json in the uft-intake-code review, neue-ideen t4), and
 * uft_fundus_provenance.c puts the file name into the description.
 *
 * REFERENCE: RFC 8259 §7 — inside a string, '"' and '\' must be escaped,
 * and so must every control character U+0000..U+001F. The checker below is
 * a minimal RFC 8259 validator (values, objects, arrays, strings, numbers,
 * literals); it also decodes one string to prove the text survives. */
static const char *js_ws(const char *p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; return p; }
static const char *js_value(const char *p, int tiefe);
static const char *js_string(const char *p, char *out, size_t cap)
{
    size_t n = 0;
    if (*p != '"') return NULL;
    for (p++; *p != '"'; p++) {
        unsigned char c = (unsigned char)*p;
        char ch = (char)c;
        if (c == 0 || c < 0x20) return NULL;              /* raw control char */
        if (c == '\\') {
            p++;
            switch (*p) {
            case '"': ch = '"'; break;   case '\\': ch = '\\'; break;
            case '/': ch = '/'; break;   case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;  case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;  case 't': ch = '\t'; break;
            case 'u': {
                unsigned v = 0;
                for (int k = 1; k <= 4; k++) {
                    char h = p[k];
                    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F'))) return NULL;
                    v = v * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
                }
                p += 4;
                ch = (char)v;
                break;
            }
            default: return NULL;                         /* invalid escape */
            }
        }
        if (out && n + 1 < cap) out[n++] = ch;
    }
    if (out && cap) out[n] = '\0';
    return p + 1;
}
static const char *js_value(const char *p, int tiefe)
{
    if (tiefe > 32) return NULL;
    p = js_ws(p);
    if (*p == '"') return js_string(p, NULL, 0);
    if (*p == '{' || *p == '[') {
        char zu = (*p == '{') ? '}' : ']';
        int obj = (*p == '{');
        p = js_ws(p + 1);
        if (*p == zu) return p + 1;
        for (;;) {
            if (obj) {
                p = js_string(js_ws(p), NULL, 0);
                if (!p) return NULL;
                p = js_ws(p);
                if (*p != ':') return NULL;
                p++;
            }
            p = js_value(p, tiefe + 1);
            if (!p) return NULL;
            p = js_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == zu) return p + 1;
            return NULL;
        }
    }
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        p++;
        while ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-') p++;
        return p;
    }
    if (!strncmp(p, "true", 4)) return p + 4;
    if (!strncmp(p, "false", 5)) return p + 5;
    if (!strncmp(p, "null", 4)) return p + 4;
    return NULL;
}

TEST(export_json_escapes_free_text) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0x10, 0x20 };
    const char *beschreibung = "Fundus: disk \"A\" side 1.scp from C:\\archiv\\x.scp";
    const char *bediener = "axel\tnachtschicht\nzeile2";
    ASSERT(uft_prov_add(ch, UFT_PROV_CAPTURE, d, sizeof(d), beschreibung, bediener) == 0);

    const char *path = "uft_prov_test_esc.json";
    remove(path);
    ASSERT(uft_prov_export_json(ch, path) == 0);
    FILE *f = fopen(path, "rb");
    ASSERT(f != NULL);
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    remove(path);

    const char *ende = js_value(buf, 0);
    ASSERT(ende != NULL);                       /* the whole file is JSON */
    ASSERT(*js_ws(ende) == '\0');               /* and nothing trails it */

    /* the description survives: decode it back and compare */
    const char *k = strstr(buf, "\"description\":");
    ASSERT(k != NULL);
    char zurueck[256];
    ASSERT(js_string(js_ws(k + strlen("\"description\":")), zurueck, sizeof zurueck) != NULL);
    ASSERT(strcmp(zurueck, beschreibung) == 0);
    k = strstr(buf, "\"operator\":");
    ASSERT(k != NULL);
    ASSERT(js_string(js_ws(k + strlen("\"operator\":")), zurueck, sizeof zurueck) != NULL);
    ASSERT(strcmp(zurueck, bediener) == 0);
    uft_prov_free(ch);
}

/* ── MF-1604 (P3-704): a hash nobody measured is not written as one ──
 *
 * uft_prov_add() takes ONE data buffer — the input. It then copied the
 * input hash into output_hash, and the export wrote both as SHA-256 values:
 * an output nobody hashed, reported as measured. An entry without data
 * (the OTDR panel's only call passes NULL, 0) kept 64 zero digits in
 * input_hash, which the export printed exactly like a real hash. Found in
 * the review of the UFT_RecoveryCaseEngine_v1 package (neue-ideen, t4). */

static int alles_null(const uint8_t *h)
{
    int any = 0;
    for (int i = 0; i < UFT_PROV_HASH_SIZE; i++) any |= h[i];
    return any == 0;
}

static int export_lesen(const uft_provenance_chain_t *ch, char *buf, size_t cap)
{
    const char *path = "uft_prov_test_mf1604.json";
    remove(path);
    if (uft_prov_export_json(ch, path) != 0) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    remove(path);
    return 1;
}

TEST(output_hash_is_not_invented) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    const uint8_t d[] = { 0x10, 0x20, 0x30 };
    ASSERT(uft_prov_add(ch, UFT_PROV_CONVERT, d, sizeof(d), "convert", "op") == 0);
    const uft_prov_entry_t *e = &ch->entries[0];
    ASSERT(!alles_null(e->input_hash));          /* the input WAS hashed */
    ASSERT(alles_null(e->output_hash));          /* the output was not */

    char buf[8192];
    ASSERT(export_lesen(ch, buf, sizeof buf));
    ASSERT(strstr(buf, "\"output_hash\": null") != NULL);
    const char *ende = js_value(buf, 0);
    ASSERT(ende != NULL && *js_ws(ende) == '\0');   /* still one JSON value */
    ASSERT(uft_prov_verify(ch));
    uft_prov_free(ch);
}

TEST(missing_input_is_null_not_a_zero_hash) {
    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    ASSERT(uft_prov_add(ch, UFT_PROV_ANALYZE, NULL, 0, "analyze", "op") == 0);
    /* a size without data is not a measured size */
    ASSERT(uft_prov_add(ch, UFT_PROV_ANALYZE, NULL, 512, "analyze", "op") == 0);
    ASSERT(ch->entries[1].data_size == 0);

    char buf[8192];
    ASSERT(export_lesen(ch, buf, sizeof buf));
    ASSERT(strstr(buf, "\"input_hash\": null") != NULL);
    ASSERT(strstr(buf, "0000000000000000000000000000000000000000000000000000000000000000") == NULL);
    const char *ende = js_value(buf, 0);
    ASSERT(ende != NULL && *js_ws(ende) == '\0');
    ASSERT(uft_prov_verify(ch));
    uft_prov_free(ch);
}

int main(void) {
    printf("=== Improvement: forensic provenance chain-of-custody "
           "(P3.3 / #110) ===\n");
    RUN(output_hash_is_not_invented);
    RUN(missing_input_is_null_not_a_zero_hash);
    RUN(export_json_escapes_free_text);
    RUN(create_yields_empty_valid_chain);
    RUN(add_records_step_metadata);
    RUN(multi_step_chain_verifies);
    RUN(tampering_with_data_size_is_detected);
    RUN(tampering_with_action_is_detected);
    RUN(tampering_with_chain_hash_is_detected);
    RUN(chain_covers_crypto_fields_not_freetext_metadata);
    RUN(null_and_bounds_guards);
    RUN(export_json_writes_audit_artifact);

    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
