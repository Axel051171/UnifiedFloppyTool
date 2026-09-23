/* Die Rezept-Maschine und ihre Bruecke zum Kopierplan des Baums.
 *
 * Herkunft: UFT_ImagingRecipe_Kit v1.0.0, MIT, (c) 2026 Axel Kramer —
 * Eigentuemer-Code, GPL-2-vereinbar. Kanal nach MF-695: Port.
 *
 * Jede Zusage hier stand einmal ROT. Die sechs Befunde, die dabei
 * gemessen wurden, stehen namentlich an ihrer Zusage; die
 * Mutationsmatrix zu dieser Runde hat alle zehn Umkehrungen gefangen.
 *
 * Was dieser Test NICHT belegt, und das gehoert gesagt: er prueft die
 * Maschine an selbst gebauten Aufnahmen. Ein Beleg von fremder Hand —
 * eine echte Diskette, ein fremdes Werkzeug — liegt fuer keines der
 * acht Profile vor; alle tragen `verified_with_real_media = false`,
 * und das ist die ehrliche Haelfte des Pakets.
 */
#include "uft/core/uft_amiga_recipe_profiles.h"
#include "uft/core/uft_recipe_copyplan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fehler = 0;

#define ZUSAGE(bedingung, text)                                            \
    do {                                                                   \
        if (bedingung) {                                                   \
            printf("  [ok ] %s\n", (text));                                \
        } else {                                                           \
            printf("  [ROT] %s  (%s:%d)\n", (text), __FILE__, __LINE__);   \
            ++fehler;                                                      \
        }                                                                  \
    } while (0)

/* ── Hilfsquellen ──────────────────────────────────────────────────── */

static uint8_t g_spur[512];
static unsigned g_gelesen;

static uft_recipe_status_t lies(void *user, int cyl, int head,
                                uft_recipe_input_level_t level,
                                unsigned attempt, unsigned rev,
                                uft_recipe_capture_t *out)
{
    (void)user; (void)cyl; (void)head; (void)level; (void)attempt; (void)rev;
    ++g_gelesen;
    memset(out, 0, sizeof(*out));
    out->data = g_spur;
    out->size_bytes = sizeof(g_spur);
    out->size_bits = sizeof(g_spur) * 8u;
    return UFT_R_OK;
}

static uft_recipe_status_t lies_erst_beim_zweiten(void *user, int cyl, int head,
                                                  uft_recipe_input_level_t level,
                                                  unsigned attempt, unsigned rev,
                                                  uft_recipe_capture_t *out)
{
    (void)user; (void)cyl; (void)head; (void)level; (void)rev;
    ++g_gelesen;
    if (attempt == 0u) return UFT_R_EIO;
    memset(out, 0, sizeof(*out));
    out->data = g_spur;
    out->size_bytes = sizeof(g_spur);
    out->size_bits = sizeof(g_spur) * 8u;
    return UFT_R_OK;
}

static uint8_t g_ziel[64];
static size_t g_ziel_n;

static uft_recipe_status_t schreib(void *user, size_t offset,
                                   const uint8_t *data, size_t size)
{
    (void)user;
    if (offset > sizeof(g_ziel) || size > sizeof(g_ziel) - offset)
        return UFT_R_EBOUNDS;
    memcpy(g_ziel + offset, data, size);
    if (offset + size > g_ziel_n) g_ziel_n = offset + size;
    return UFT_R_OK;
}

static void setz_bit(uint8_t *p, size_t bit, int v)
{
    uint8_t m = (uint8_t)(1u << (7u - (bit & 7u)));
    if (v) p[bit >> 3] |= m;
    else   p[bit >> 3] = (uint8_t)(p[bit >> 3] & (uint8_t)~m);
}

static void setz_bits(uint8_t *p, size_t bit, uint32_t wert, unsigned n)
{
    for (unsigned i = 0; i < n; ++i)
        setz_bit(p, bit + i, (int)((wert >> (n - 1u - i)) & 1u));
}

/* Ein Rezept mit genau einer Regel, im Stack gebaut. */
typedef struct {
    uft_recipe_track_rule_t regel;
    uft_recipe_variant_t    variante;
    uft_imaging_recipe_t    rezept;
} bau_t;

static void bau_init(bau_t *b, unsigned zylinder)
{
    memset(b, 0, sizeof(*b));
    b->regel.track_step = 1;
    b->regel.decoder_id = "raw-copy";
    b->variante.id = "v";
    b->variante.rules = &b->regel;
    b->variante.rule_count = 1;
    b->rezept.api_version = UFT_RECIPE_API_VERSION;
    b->rezept.id = "t";
    b->rezept.cylinders = zylinder;
    b->rezept.heads = 1;
    b->rezept.side_order = UFT_RECIPE_SIDE_SINGLE_0;
    b->rezept.variants = &b->variante;
    b->rezept.variant_count = 1;
}

/* ── 1. Die Schrittweite, die nicht auf last_track landet ──────────── */

static void t_schrittweite(void)
{
    puts("1. Schrittweite, die nicht genau auf last_track landet");
    /* "Spur 0 bis 5, jede zweite" — der gewoehnliche Fall
     * 80-Spur-Laufwerk auf 40-Spur-Diskette. Vorher las der Lauf alle
     * drei Spuren richtig, meldete errors=0 und endete trotzdem mit
     * UFT_R_EBOUNDS und complete=false: ein Bericht, der sich selbst
     * widerspricht. */
    bau_t b; bau_init(&b, 6);
    b.regel.last_track = 5;
    b.regel.track_step = 2;
    b.regel.decoded_bytes = 16;
    b.regel.output_mode = UFT_RECIPE_OUTPUT_APPEND;

    uft_recipe_engine_t e; uft_recipe_engine_init(&e);
    char m[160];
    ZUSAGE(uft_recipe_validate(&e, &b.rezept, m, sizeof(m)) == UFT_R_OK,
           "validate nimmt die Regel an");

    uft_recipe_source_t q; memset(&q, 0, sizeof(q)); q.read = lies;
    uft_recipe_sink_t s; memset(&s, 0, sizeof(s)); s.write = schreib;
    g_gelesen = 0; g_ziel_n = 0;

    uft_recipe_report_t r;
    uft_recipe_status_t st = uft_recipe_run(&e, &b.rezept, &b.variante, &q, &s, &r);

    ZUSAGE(st == UFT_R_OK, "der Lauf endet ohne Fehler");
    ZUSAGE(r.track_count == 3, "genau drei Spuren (0, 2, 4)");
    ZUSAGE(g_gelesen == 3, "die Quelle wurde dreimal gefragt");
    ZUSAGE(r.errors == 0, "kein Fehler gezaehlt");
    ZUSAGE(r.complete, "der Bericht meldet sich als vollstaendig");
    uft_recipe_report_free(&r);
}

/* ── 2. checksum_ok sagt nur, was wirklich geprueft wurde ──────────── */

static void t_pruefsumme(void)
{
    puts("2. checksum_ok nur nach einer wirklichen Pruefung");
    /* Vorher stand in der Maschine `tr->checksum_ok = true;`, unbedingt,
     * an genau einer Stelle, ohne Vergleich — und `rule->checksum_id`
     * hatte im ganzen Kit null Leser. */
    uft_recipe_engine_t e; uft_recipe_engine_init(&e);
    uft_recipe_source_t q; memset(&q, 0, sizeof(q)); q.read = lies;

    /* (a) ohne checksum_id: nicht geprueft, also nicht "in Ordnung" */
    bau_t b; bau_init(&b, 1);
    b.regel.decoded_bytes = 16;
    b.regel.output_mode = UFT_RECIPE_OUTPUT_ANALYSIS_ONLY;
    uft_recipe_report_t r;
    g_gelesen = 0;
    ZUSAGE(uft_recipe_run(&e, &b.rezept, &b.variante, &q, NULL, &r) == UFT_R_OK,
           "ohne Pruefsumme laeuft es durch");
    ZUSAGE(r.track_count == 1 && r.tracks[0].checksum_ok == false,
           "checksum_ok ist FALSE, wenn nichts verglichen wurde");
    ZUSAGE(r.track_count && r.tracks[0].checksum_state == UFT_RECIPE_CHECK_NONE,
           "der Zustand sagt NICHT GEPRUEFT, nicht FALSCH");
    ZUSAGE(r.track_count && r.tracks[0].verify_kind == UFT_RECIPE_VERIFY_NONE,
           "verify_kind ist NONE");
    uint16_t ist = r.track_count ? r.tracks[0].crc16 : 0u;
    uft_recipe_report_free(&r);

    /* (b) falscher Sollwert: ECHECKSUM und CHECK_FAILED */
    b.regel.checksum_id = UFT_RECIPE_CHECKSUM_CRC16_CCITT;
    b.regel.checksum_expect = (uint16_t)(ist ^ 0xFFFFu);
    b.regel.checksum_expect_valid = true;
    g_gelesen = 0;
    ZUSAGE(uft_recipe_run(&e, &b.rezept, &b.variante, &q, NULL, &r)
               == UFT_R_ECHECKSUM,
           "eine falsche Pruefsumme faellt auf");
    ZUSAGE(r.track_count && r.tracks[0].checksum_state == UFT_RECIPE_CHECK_FAILED,
           "der Zustand sagt GEPRUEFT UND FALSCH");
    ZUSAGE(r.complete == false, "der Bericht meldet sich als unvollstaendig");
    uft_recipe_report_free(&r);

    /* (c) richtiger Sollwert: OK und CHECK_OK */
    b.regel.checksum_expect = ist;
    g_gelesen = 0;
    ZUSAGE(uft_recipe_run(&e, &b.rezept, &b.variante, &q, NULL, &r) == UFT_R_OK,
           "die richtige Pruefsumme geht durch");
    ZUSAGE(r.track_count && r.tracks[0].checksum_ok == true &&
           r.tracks[0].checksum_state == UFT_RECIPE_CHECK_OK,
           "erst jetzt heisst checksum_ok wirklich in Ordnung");
    uft_recipe_report_free(&r);

    /* (d) ein Bezeichner, den es nicht gibt, wird abgewiesen statt
     *     ignoriert; und eine Pruefsumme ohne Sollwert ist keine. */
    char m[160];
    b.regel.checksum_id = "crc32";
    ZUSAGE(uft_recipe_validate(&e, &b.rezept, m, sizeof(m))
               == UFT_R_EUNSUPPORTED,
           "ein unbekannter Pruefsummen-Name wird abgewiesen");
    b.regel.checksum_id = UFT_RECIPE_CHECKSUM_CRC16_CCITT;
    b.regel.checksum_expect_valid = false;
    ZUSAGE(uft_recipe_validate(&e, &b.rezept, m, sizeof(m)) == UFT_R_EINVAL,
           "eine Pruefsumme ohne Sollwert ist keine Pruefung");
}

/* ── 3. Der Dekoder liest ab dem Bit, an dem der Sync endet ────────── */

static void t_bitgenau(void)
{
    puts("3. Sync abseits der Bytegrenze");
    /* Vorher rechneten beide MFM-Dekoder
     * `start = (sync_bit + sync_bits + 7) / 8` und begannen an der
     * naechsten BYTEGRENZE. Gemessen an einem Sync bei Bit 3: der Sync
     * endet an Bit 11, der Dekoder begann bei Bit 16 — fuenf Bit still
     * uebersprungen, Ergebnis als UFT_R_OK gemeldet. In einem echten
     * MFM-Bitstrom ist genau das der Normalfall, nicht die Ausnahme. */
    const uint32_t nutzlast = 0xDEADBEEFu;
    const uint32_t gerade   = nutzlast & 0x55555555u;
    const uint32_t ungerade = (nutzlast >> 1) & 0x55555555u;

    memset(g_spur, 0, sizeof(g_spur));
    setz_bits(g_spur, 3u, 0xA1u, 8u);       /* Sync ab Bit 3  */
    setz_bits(g_spur, 11u, ungerade, 32u);  /* odd  ab Bit 11 */
    setz_bits(g_spur, 43u, gerade, 32u);    /* even ab Bit 43 */

    uint8_t muster[1] = { 0xA1u };
    ZUSAGE(uft_recipe_find_bits(g_spur, sizeof(g_spur) * 8u, muster, 8u,
                                0u, false) == 3u,
           "die Bitsuche findet den Sync an Bit 3");

    bau_t b; bau_init(&b, 1);
    b.regel.decoded_bytes = 4;
    b.regel.decoder_id = "mfm-odd-even";
    b.regel.input_level = UFT_RECIPE_INPUT_BITSTREAM;
    b.regel.sync_kind = UFT_RECIPE_SYNC_WORD;
    b.regel.sync[0] = 0xA1u;
    b.regel.sync_bits = 8;
    b.regel.output_mode = UFT_RECIPE_OUTPUT_APPEND;

    uft_recipe_engine_t e; uft_recipe_engine_init(&e);
    uft_recipe_source_t q; memset(&q, 0, sizeof(q));
    q.read = lies;
    q.capabilities = UFT_RECIPE_CAP_BITSTREAM | UFT_RECIPE_CAP_CUSTOM_DECODER;
    uft_recipe_sink_t s; memset(&s, 0, sizeof(s)); s.write = schreib;

    g_gelesen = 0; g_ziel_n = 0; memset(g_ziel, 0, sizeof(g_ziel));
    uft_recipe_report_t r;
    ZUSAGE(uft_recipe_run(&e, &b.rezept, &b.variante, &q, &s, &r) == UFT_R_OK,
           "der Lauf gelingt");
    const uint8_t erwartet[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ZUSAGE(g_ziel_n == 4 && memcmp(g_ziel, erwartet, 4) == 0,
           "die Nutzlast ist byteidentisch — gelesen ab Bit 11, nicht Byte 2");
    uft_recipe_report_free(&r);
    memset(g_spur, 0x5A, sizeof(g_spur));
}

/* ── 4. Ein Lauf ohne Senke meldet keinen Erfolg ───────────────────── */

static void t_ohne_senke(void)
{
    puts("4. Schreibende Regel ohne Senke");
    /* Vorher wurde der Schreibblock still uebersprungen, `st` blieb
     * UFT_R_OK, und der Bericht meldete complete=true bei null
     * geschriebenen Byte. */
    bau_t b; bau_init(&b, 1);
    b.regel.decoded_bytes = 16;
    b.regel.output_mode = UFT_RECIPE_OUTPUT_APPEND;

    uft_recipe_engine_t e; uft_recipe_engine_init(&e);
    uft_recipe_source_t q; memset(&q, 0, sizeof(q)); q.read = lies;
    uft_recipe_report_t r;
    g_gelesen = 0;
    ZUSAGE(uft_recipe_run(&e, &b.rezept, &b.variante, &q, NULL, &r)
               == UFT_R_EINVAL,
           "ohne Senke wird abgesagt, bevor ein Laufwerk anlaeuft");
    ZUSAGE(g_gelesen == 0, "und es wurde nichts gelesen");
}

/* ── 5. Eine Wiederholung ist eine Warnung ─────────────────────────── */

static void t_warnung(void)
{
    puts("5. Erst der zweite Versuch gelang");
    /* `warnings` war ein Feld, das niemand erhoeht — eine Zahl im
     * Bericht, die immer 0 war. */
    bau_t b; bau_init(&b, 1);
    b.regel.decoded_bytes = 16;
    b.regel.output_mode = UFT_RECIPE_OUTPUT_ANALYSIS_ONLY;
    b.regel.error_policy = UFT_RECIPE_ERROR_RETRY;
    b.regel.retries = 1;

    uft_recipe_engine_t e; uft_recipe_engine_init(&e);
    uft_recipe_source_t q; memset(&q, 0, sizeof(q));
    q.read = lies_erst_beim_zweiten;
    uft_recipe_report_t r;
    g_gelesen = 0;
    ZUSAGE(uft_recipe_run(&e, &b.rezept, &b.variante, &q, NULL, &r) == UFT_R_OK,
           "der zweite Versuch gelingt");
    ZUSAGE(r.warnings == 1, "das zaehlt als EINE Warnung");
    ZUSAGE(r.errors == 0, "und als kein Fehler");
    ZUSAGE(r.track_count && r.tracks[0].retried,
           "die Spur ist als wiederholt gekennzeichnet");
    uft_recipe_report_free(&r);
}

/* ── 6. Die Bruecke zum Kopierplan des Baums ───────────────────────── */

static void t_bruecke(void)
{
    puts("6. Bruecke Rezept -> uft_copy_plan_t");

    /* (a) Die Mindestebene ist das MAXIMUM ueber alle Regeln.
     *     Vorher setzte eine spaetere BITSTREAM-Regel die FLUX-Forderung
     *     einer frueheren zurueck: Regel 0 = flux, Regel 1 = bitstream
     *     ergab "bitstream". Ein Rezept, dessen Schutzspur Fluss
     *     braucht, haette damit ein Geraet ohne Fluss zugelassen. */
    uft_recipe_track_rule_t regeln[2];
    memset(regeln, 0, sizeof(regeln));
    regeln[0].track_step = 1;
    regeln[0].decoder_id = "raw-copy";
    regeln[0].input_level = UFT_RECIPE_INPUT_FLUX;
    regeln[1].first_track = 1;
    regeln[1].last_track = 159;
    regeln[1].track_step = 1;
    regeln[1].decoder_id = "raw-copy";
    regeln[1].input_level = UFT_RECIPE_INPUT_BITSTREAM;

    uft_recipe_variant_t v; memset(&v, 0, sizeof(v));
    v.id = "v"; v.rules = regeln; v.rule_count = 2;
    uft_imaging_recipe_t rez; memset(&rez, 0, sizeof(rez));
    rez.api_version = UFT_RECIPE_API_VERSION;
    rez.id = "b"; rez.cylinders = 80; rez.heads = 2;
    rez.variants = &v; rez.variant_count = 1;
    rez.preservation_recipe = true;
    rez.required_capabilities = UFT_RECIPE_CAP_FLUX |
                                UFT_RECIPE_CAP_MULTI_REV |
                                UFT_RECIPE_CAP_INDEX |
                                UFT_RECIPE_CAP_CUSTOM_DECODER;

    uft_copy_plan_t plan;
    uint32_t caps = 0u, fehlt = 0u;
    ZUSAGE(uft_recipe_to_copy_plan(&rez, &v, &plan, &caps, &fehlt) == UFT_R_OK,
           "die Bruecke antwortet");
    ZUSAGE(plan.level == UFT_COPY_FLUX,
           "die Ebene ist FLUX, weil EINE Regel Fluss braucht");
    ZUSAGE(plan.preservation == UFT_PRESERVE_PROTECTED,
           "ein Erhaltungs-Rezept traegt PROTECTED");

    /* (b) Die Faehigkeiten, die ein Gegenstueck haben, kommen an. */
    ZUSAGE((caps & (uint32_t)UFT_CAP_FLUX_IO) != 0u, "FLUX_IO ist verlangt");
    ZUSAGE((caps & (uint32_t)UFT_CAP_MULTI_REV) != 0u, "MULTI_REV ist verlangt");

    /* (c) Und die drei ohne Gegenstueck fallen NICHT still weg.
     *     Gemessen: `uft_copy_caps_t` hat neun Konstanten und darunter
     *     keine fuer Index, Spurbytes oder eigenen Dekoder. */
    ZUSAGE((fehlt & (uint32_t)UFT_RECIPE_UNUEBERSETZT_INDEX) != 0u,
           "Index steht als unuebersetzt da, statt zu verschwinden");
    ZUSAGE((fehlt & (uint32_t)UFT_RECIPE_UNUEBERSETZT_DECODER) != 0u,
           "eigener Dekoder ebenso");

    /* (d) caps_bekannt bleibt FALSE: die Schnittmenge aus Quelle und
     *     Ziel ist hier nicht gemessen worden, und eine 0 in `caps`
     *     allein waere mehrdeutig. */
    ZUSAGE(plan.caps == 0u && plan.caps_bekannt == false,
           "der Plan behauptet keine gemessene Faehigkeitsmenge");

    /* (e) Ein Rezept ohne Pruefung bekommt NORMAL, nicht VERIFY. */
    ZUSAGE(plan.policy == UFT_POLICY_NORMAL,
           "ohne Pruefpfad ist die Richtlinie NORMAL");

    /* (f) Mit einer echten Pruefung wird daraus VERIFY. */
    regeln[0].checksum_id = UFT_RECIPE_CHECKSUM_CRC16_CCITT;
    regeln[0].checksum_expect_valid = true;
    ZUSAGE(uft_recipe_to_copy_plan(&rez, &v, &plan, &caps, &fehlt) == UFT_R_OK,
           "die Bruecke antwortet auch mit Pruefung");
    ZUSAGE(plan.policy == UFT_POLICY_VERIFY,
           "erst mit Pruefpfad wird die Richtlinie VERIFY");

    /* (g) Fehlende Zeiger werden abgewiesen. */
    ZUSAGE(uft_recipe_to_copy_plan(NULL, &v, &plan, &caps, NULL) == UFT_R_EINVAL,
           "ohne Rezept: EINVAL");
    ZUSAGE(uft_recipe_to_copy_plan(&rez, &v, &plan, NULL, NULL) == UFT_R_EINVAL,
           "ohne Faehigkeitsausgabe: EINVAL");
}

/* ── 7. Die acht Profile ───────────────────────────────────────────── */

static void t_profile(void)
{
    puts("7. Die acht Amiga-Profile");
    uft_recipe_engine_t e; uft_recipe_engine_init(&e);
    size_t n = 0;
    const uft_imaging_recipe_t *const *p = uft_amiga_recipe_profiles(&n);
    ZUSAGE(p != NULL && n == 8, "acht Profile");
    if (!p) return;

    unsigned belegt = 0;
    unsigned uebersetzt = 0;
    for (size_t i = 0; i < n; ++i) {
        char m[160];
        if (uft_recipe_validate(&e, p[i], m, sizeof(m)) != UFT_R_OK) {
            printf("  [ROT] Profil %s: %s\n", p[i]->id, m);
            ++fehler;
        }
        if (p[i]->verified_with_real_media) ++belegt;

        uft_copy_plan_t plan; uint32_t caps = 0u, fehlt = 0u;
        if (uft_recipe_to_copy_plan(p[i], &p[i]->variants[0], &plan,
                                    &caps, &fehlt) == UFT_R_OK)
            ++uebersetzt;
    }
    ZUSAGE(belegt == 0,
           "kein Profil behauptet einen Beleg an echtem Material");
    ZUSAGE(uebersetzt == n,
           "alle acht uebersetzen sich in einen Kopierplan");
}

int main(void)
{
    memset(g_spur, 0x5A, sizeof(g_spur));
    puts("== Rezept-Maschine und Kopierplan-Bruecke ==");
    t_schrittweite();
    t_pruefsumme();
    t_bitgenau();
    t_ohne_senke();
    t_warnung();
    t_bruecke();
    t_profile();
    if (fehler) {
        printf("\n%d Zusage(n) ROT\n", fehler);
        return 1;
    }
    puts("\nalle Zusagen gruen");
    return 0;
}
