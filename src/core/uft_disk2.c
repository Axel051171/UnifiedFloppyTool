/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2.c
 * @brief Das Zentrum, zweite Fassung — Umsetzung.
 *
 * Die Regeln stehen im Kopf (`uft_disk2.h`); hier steht nur, wo sie
 * durchgesetzt werden:
 *
 *   Zuversicht           `zuversicht_traegt()`, gerufen aus add_revolution,
 *                        add_sector und add_entry — EINE Regel, drei
 *                        Schichten. In MF-1272 stand sie nur bei Sektoren.
 *   Generationen         jedes `set_*`/`add_*` erhoeht `gen`; abgeleitete
 *                        Objekte merken sich die Generation ihrer Quelle,
 *                        `uft_d2_validate()` vergleicht.
 *   Keine Kuerzung       wer nicht passt, wird ABGEWIESEN und gemeldet.
 *                        Nichts wird still gekappt (D5).
 */

#include "uft_disk2_priv.h"
#include "uft/core/uft_source_facts.h"  /* MF-1318 */
#include "uft/uft_format_plugin.h"          /* MF-1317: probe_* an der Scheibe */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════ kleine Helfer ══════════════════════════════════ */

/**
 * Ein Informationsbefund des EINSPEISENS. Beim Laden aus einem Behaelter
 * steht er schon in der Datei; ohne diese Klammer wuechse der Befundblock
 * bei jeder Sicherung. FEHLER gehen IMMER durch — ein Fehler, den das
 * Laden unterdrueckt, waere genau die stille Veraenderung, gegen die
 * dieses ganze Modul steht.
 */
#define INGEST_DIAG(d, sev, ...)                                              \
    do {                                                                      \
        if ((sev) == UFT_D2_DIAG_ERROR || !(d)->suppress_ingest_diag)         \
            uft_d2_diag((d), (sev), __VA_ARGS__);                             \
    } while (0)

/** Kopiert in feste Feldbreite und terminiert IMMER. `strncpy` tut das
 *  nicht, und der Uebersetzer mahnt es zu Recht an. */
static void feld_setzen(char *dst, size_t field, const char *src) {
    if (!dst || !field) return;
    memset(dst, 0, field);
    if (!src) return;
    size_t n = strlen(src);
    if (n > field - 1u) n = field - 1u;
    memcpy(dst, src, n);
}

static void *wachsen(void *p, size_t *cap, size_t need, size_t elem) {
    if (*cap >= need) return p;
    size_t c = *cap ? *cap : 8u;
    while (c < need) c *= 2u;
    void *q = realloc(p, c * elem);
    if (!q) return NULL;
    *cap = c;
    return q;
}

/**
 * Die Zuversichtsregel, an EINER Stelle.
 * @param belegt  hat der Aufrufer einen Beleg fuer Richtigkeit UND
 *                Vollstaendigkeit vorgelegt?
 * @return true, wenn @p conf zulaessig ist.
 */
static bool zuversicht_traegt(uft_d2_conf_t conf, bool belegt) {
    return conf != UFT_D2_CONF_CERTAIN || belegt;
}

/* ═══════════════════════ Leben ══════════════════════════════════════════ */

uft_disk2_t *uft_d2_create(void) {
    uft_disk2_t *d = calloc(1u, sizeof(*d));
    if (!d) return NULL;
    /* Platz 0 des Registers bleibt leer — 0 heisst „keine Ableitung". */
    d->derivs = calloc(1u, sizeof(*d->derivs));
    if (!d->derivs) { free(d); return NULL; }
    d->nderiv = 1u; d->deriv_cap = 1u;
    d->feat_dirty = true;
    return d;
}

static void track_free(uft_d2_track_t *t) {
    for (size_t r = 0; r < t->flux.count; ++r) free(t->flux.revs[r].intervals);
    free(t->flux.revs);
    free(t->bitstream.bits);
    free(t->bitstream.bit_conf);
    free(t->bitstream.agree);
    free(t->bitstream.phase_q8);
    free(t->bitstream.flux_count);
    for (size_t s = 0; s < t->sectors.count; ++s) free(t->sectors.items[s].data);
    free(t->sectors.items);
}

void uft_d2_destroy(uft_disk2_t *d) {
    if (!d) return;
    for (size_t i = 0; i < d->ntracks; ++i) track_free(&d->tracks[i]);
    free(d->tracks);
    free(d->tidx);
    for (size_t i = 0; i < d->nfs; ++i) free(d->fs[i].entries);
    free(d->fs);
    free(d->meta);
    free(d->derivs);
    free(d);
}

/* ═══════════════════════ Befunde ════════════════════════════════════════ */

bool uft_d2_diag(uft_disk2_t *d, uft_d2_diag_sev_t sev, uft_d2_layer_t layer,
                 int cyl, int head, int sector, const char *code,
                 const char *fmt, ...) {
    if (!d) return false;
    /* Der LETZTE Platz gehoert der Ueberlaufmeldung — von Anfang an, nicht
     * erst wenn er gebraucht wird. Sonst wird der 512. Befund angenommen
     * und danach still ueberschrieben (MF-1272). */
    if (d->ndiag >= UFT_D2_MAX_DIAG - 1u) {
        if (d->ndiag == UFT_D2_MAX_DIAG - 1u) {
            uft_d2_diag_t *o = &d->diag[UFT_D2_MAX_DIAG - 1u];
            memset(o, 0, sizeof(*o));
            o->sev = UFT_D2_DIAG_WARN; o->layer = layer;
            o->cyl = -1; o->head = -1; o->sector = -1;
            feld_setzen(o->code, UFT_D2_DIAG_CODE, "DIAG_OVERFLOW");
            feld_setzen(o->text, UFT_D2_DIAG_TEXT,
                        "Die Befundliste ist voll; weitere werden gezaehlt, "
                        "nicht gespeichert.");
            d->ndiag = UFT_D2_MAX_DIAG;
        }
        d->diag_hidden++;
        if (sev == UFT_D2_DIAG_ERROR) d->diag_hidden_err++;
        return false;
    }

    uft_d2_diag_t *g = &d->diag[d->ndiag++];
    memset(g, 0, sizeof(*g));
    g->sev = sev; g->layer = layer;
    g->cyl    = (int16_t)(cyl    < 0 ? -1 : cyl);
    g->head   = (int8_t) (head   < 0 ? -1 : head);
    g->sector = (int16_t)(sector < 0 ? -1 : sector);
    feld_setzen(g->code, UFT_D2_DIAG_CODE, code);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->text, UFT_D2_DIAG_TEXT, fmt, ap);
    va_end(ap);
    return true;
}

size_t uft_d2_diag_count(const uft_disk2_t *d) { return d ? d->ndiag : 0u; }

const uft_d2_diag_t *uft_d2_diag_at(const uft_disk2_t *d, size_t i) {
    return (d && i < d->ndiag) ? &d->diag[i] : NULL;
}

size_t uft_d2_diag_count_sev(const uft_disk2_t *d, uft_d2_diag_sev_t at_least) {
    if (!d) return 0u;
    size_t n = 0u;
    for (size_t i = 0; i < d->ndiag; ++i) if (d->diag[i].sev >= at_least) n++;
    return n;
}

/* ═══════════════════════ Ableitungsregister ═════════════════════════════ */

uft_d2_deriv_id_t uft_d2_register_deriv(uft_disk2_t *d, uft_d2_layer_t from,
                                        uft_d2_origin_t origin,
                                        const char *by, const char *params,
                                        uint32_t source_gen) {
    if (!d) return UFT_D2_DERIV_NONE;
    if (d->nderiv >= UFT_D2_MAX_DERIV) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, from, -1, -1, -1, "DERIV_FULL",
                    "Das Ableitungsregister ist voll (%u); \"%s\" wurde nicht "
                    "aufgenommen.", (unsigned)UFT_D2_MAX_DERIV,
                    by ? by : "(ohne Namen)");
        return UFT_D2_DERIV_NONE;
    }
    uft_d2_derivation_t *q = wachsen(d->derivs, &d->deriv_cap, d->nderiv + 1u,
                                     sizeof(*d->derivs));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, from, -1, -1, -1, "NO_MEMORY",
                    "Kein Speicher fuer das Ableitungsregister.");
        return UFT_D2_DERIV_NONE;
    }
    d->derivs = q;
    uft_d2_derivation_t *v = &d->derivs[d->nderiv];
    memset(v, 0, sizeof(*v));
    v->from_layer = from; v->origin = origin; v->source_gen = source_gen;
    feld_setzen(v->by,     UFT_D2_DERIV_BY,     by);
    feld_setzen(v->params, UFT_D2_DERIV_PARAMS, params);
    return (uft_d2_deriv_id_t)(d->nderiv++);
}

const uft_d2_derivation_t *uft_d2_deriv(const uft_disk2_t *d,
                                        uft_d2_deriv_id_t id) {
    if (!d || id == UFT_D2_DERIV_NONE || (size_t)id >= d->nderiv) return NULL;
    return &d->derivs[id];
}

size_t uft_d2_deriv_count(const uft_disk2_t *d) {
    return (d && d->nderiv) ? d->nderiv - 1u : 0u;
}

/* ═══════════════════════ Spuren ═════════════════════════════════════════ */

/** Baut den Index neu auf. Er haelt NUMMERN, keine Zeiger: `d->tracks`
 *  zieht beim Wachsen um, und ein gemerkter Zeiger sieht danach ins Leere. */
static bool idx_fassen(uft_disk2_t *d, uint32_t cyl, uint32_t head) {
    uint32_t nc = d->idx_cyls, nh = d->idx_heads;
    if (cyl  >= nc) nc = cyl  + 1u;
    if (head >= nh) nh = head + 1u;
    if (d->tidx && nc == d->idx_cyls && nh == d->idx_heads) return true;

    uint32_t *q = calloc((size_t)nc * nh, sizeof(*q));
    if (!q) return false;
    for (size_t i = 0; i < d->ntracks; ++i)
        q[(size_t)d->tracks[i].cyl * nh + d->tracks[i].head] = (uint32_t)(i + 1u);
    free(d->tidx);
    d->tidx = q; d->idx_cyls = nc; d->idx_heads = nh;
    return true;
}

static uft_d2_track_t *track_suchen(const uft_disk2_t *d, uint32_t cyl,
                                    uint32_t head) {
    if (!d->tidx || cyl >= d->idx_cyls || head >= d->idx_heads) return NULL;
    const uint32_t n = d->tidx[(size_t)cyl * d->idx_heads + head];
    return n ? (uft_d2_track_t *)&d->tracks[n - 1u] : NULL;
}

uft_d2_track_t *uft_d2_track(uft_disk2_t *d, uint16_t cyl, uint8_t head) {
    if (!d) return NULL;
    uft_d2_track_t *t = track_suchen(d, cyl, head);
    if (t) return t;

    uft_d2_track_t *q = wachsen(d->tracks, &d->track_cap, d->ntracks + 1u,
                                sizeof(*d->tracks));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, cyl, head, -1,
                    "NO_MEMORY", "Kein Speicher fuer eine weitere Spur.");
        return NULL;
    }
    d->tracks = q;
    t = &d->tracks[d->ntracks];
    memset(t, 0, sizeof(*t));
    t->cyl = cyl; t->head = head;
    t->encoding = UFT_ENC_UNKNOWN;
    t->bitstream.index_bit = SIZE_MAX;
    d->ntracks++;

    if (!idx_fassen(d, cyl, head)) {
        /* Der Index ist eine Beschleunigung, kein Speicher — aber ohne ihn
         * findet `track_get` die Spur nicht mehr. Also abweisen statt
         * halb vorhanden lassen. */
        d->ntracks--;
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, cyl, head, -1,
                    "NO_MEMORY", "Kein Speicher fuer den Spurindex.");
        return NULL;
    }
    d->tidx[(size_t)cyl * d->idx_heads + head] = (uint32_t)d->ntracks;
    d->feat_dirty = true;
    return t;
}

const uft_d2_track_t *uft_d2_track_get(const uft_disk2_t *d, uint16_t cyl,
                                       uint8_t head) {
    return d ? track_suchen(d, cyl, head) : NULL;
}

size_t uft_d2_track_count(const uft_disk2_t *d) { return d ? d->ntracks : 0u; }

const uft_d2_track_t *uft_d2_track_at(const uft_disk2_t *d, size_t i) {
    return (d && i < d->ntracks) ? &d->tracks[i] : NULL;
}

bool uft_d2_extent(const uft_disk2_t *d, uint16_t *out_max_cyl,
                   uint8_t *out_max_head) {
    if (!d || !d->ntracks) return false;
    uint16_t mc = 0u; uint8_t mh = 0u;
    for (size_t i = 0; i < d->ntracks; ++i) {
        if (d->tracks[i].cyl  > mc) mc = d->tracks[i].cyl;
        if (d->tracks[i].head > mh) mh = d->tracks[i].head;
    }
    if (out_max_cyl)  *out_max_cyl  = mc;
    if (out_max_head) *out_max_head = mh;
    return true;
}

/* ═══════════════════════ Schicht 1: Fluss ═══════════════════════════════ */

bool uft_d2_add_revolution(uft_disk2_t *d, uft_d2_track_t *t,
                           const uint32_t *intervals, size_t count,
                           uint32_t index_time_ns, bool complete,
                           uft_d2_conf_t conf, uft_d2_deriv_id_t deriv) {
    if (!d || !t || (!intervals && count)) return false;

    if (!zuversicht_traegt(conf, complete)) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, t->cyl, t->head,
                    -1, "CONF_UNEARNED",
                    "Eine unvollstaendige Umdrehung beansprucht Zuversicht "
                    "%u; erlaubt ist hoechstens %u.",
                    (unsigned)conf, (unsigned)UFT_D2_CONF_CERTAIN - 1u);
        return false;
    }

    uft_d2_rev_t *q = realloc(t->flux.revs,
                              (t->flux.count + 1u) * sizeof(*t->flux.revs));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, t->cyl, t->head,
                    -1, "NO_MEMORY", "Kein Speicher fuer eine Umdrehung.");
        return false;
    }
    t->flux.revs = q;

    uft_d2_rev_t *r = &t->flux.revs[t->flux.count];
    memset(r, 0, sizeof(*r));
    if (count) {
        r->intervals = malloc(count * sizeof(*r->intervals));
        if (!r->intervals) {
            uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, t->cyl,
                        t->head, -1, "NO_MEMORY",
                        "Kein Speicher fuer %zu Flusswechsel.", count);
            return false;
        }
        memcpy(r->intervals, intervals, count * sizeof(*r->intervals));
    }
    r->count = count;
    r->index_time_ns = index_time_ns;
    r->complete = complete;
    r->conf = conf;

    t->flux.count++;
    t->flux.deriv = deriv;
    t->flux.gen++;
    t->has_flux = true;
    d->feat_dirty = true;

    if (!index_time_ns)
        INGEST_DIAG(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_FLUX, t->cyl, t->head,
                    -1, "NO_INDEX_TIME",
                    "Umdrehung %zu ohne gemessene Indexzeit — die Drehzahl "
                    "ist daraus nicht ableitbar.", t->flux.count - 1u);
    if (!complete)
        INGEST_DIAG(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_FLUX, t->cyl, t->head,
                    -1, "REV_PARTIAL",
                    "Umdrehung %zu ist nicht Index-zu-Index.",
                    t->flux.count - 1u);
    return true;
}

/* ═══════════════════════ Schicht 2: Bitstrom ════════════════════════════ */

static void bitstream_leeren(uft_d2_bitstream_t *b) {
    free(b->bits); free(b->bit_conf); free(b->agree);
    free(b->phase_q8); free(b->flux_count);
    const uft_d2_deriv_id_t dv = b->deriv;
    const uint32_t gen = b->gen;
    memset(b, 0, sizeof(*b));
    b->index_bit = SIZE_MAX;
    b->deriv = dv; b->gen = gen;
}

bool uft_d2_set_bitstream(uft_disk2_t *d, uft_d2_track_t *t,
                          const uint8_t *bits, size_t nbits,
                          const uft_d2_conf_t *bit_conf,
                          const uint8_t *agree, uint8_t nrevs_fused,
                          const int16_t *phase_q8, const uint16_t *flux_count,
                          size_t index_bit, uft_encoding_t enc,
                          uint32_t cell_ns, uft_d2_deriv_id_t deriv) {
    if (!d || !t || !bits || !nbits) return false;

    /* „3 von 5" braucht die 5. Stimmen ohne Umdrehungszahl sind nicht
     * deutbar — und eine nicht deutbare Zahl ist schlimmer als keine. */
    if (agree && !nrevs_fused) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_BITSTREAM, t->cyl,
                    t->head, -1, "VOTES_NO_BASE",
                    "Stimmen je Bit ohne Zahl der fusionierten Umdrehungen — "
                    "\"n von ?\" ist keine Aussage.");
        return false;
    }

    if (t->has_sectors)
        INGEST_DIAG(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_BITSTREAM, t->cyl,
                    t->head, -1, "BITSTREAM_REPLACED",
                    "Der Bitstrom wird ersetzt, waehrend %zu Sektoren daraus "
                    "abgeleitet sind; uft_d2_validate() nennt sie.",
                    t->sectors.count);

    bitstream_leeren(&t->bitstream);
    uft_d2_bitstream_t *b = &t->bitstream;

    const size_t nbytes = (nbits + 7u) / 8u;
    b->bits = malloc(nbytes);
    if (b->bits) memcpy(b->bits, bits, nbytes);

    /* Alles oder nichts: ein Bitstrom OHNE Konfidenz ist eine andere
     * Aussage als einer mit (MF-1272). Ein still weggelassenes Beiblatt
     * waere eine stille Veraenderung. */
    bool ok = b->bits != NULL;
    if (ok && bit_conf) {
        b->bit_conf = malloc(nbits * sizeof(*b->bit_conf));
        if (b->bit_conf) memcpy(b->bit_conf, bit_conf, nbits * sizeof(*b->bit_conf));
        else ok = false;
    }
    if (ok && agree) {
        b->agree = malloc(nbits);
        if (b->agree) memcpy(b->agree, agree, nbits);
        else ok = false;
    }
    if (ok && phase_q8) {
        b->phase_q8 = malloc(nbits * sizeof(*b->phase_q8));
        if (b->phase_q8) memcpy(b->phase_q8, phase_q8, nbits * sizeof(*b->phase_q8));
        else ok = false;
    }
    if (ok && flux_count) {
        b->flux_count = malloc(nbits * sizeof(*b->flux_count));
        if (b->flux_count) memcpy(b->flux_count, flux_count, nbits * sizeof(*b->flux_count));
        else ok = false;
    }
    if (!ok) {
        bitstream_leeren(b);
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_BITSTREAM, t->cyl,
                    t->head, -1, "NO_MEMORY",
                    "Kein Speicher fuer den Bitstrom (%zu Bit) samt Beilagen.",
                    nbits);
        return false;
    }

    b->nbits = nbits;              /* GEMESSEN, nie geklemmt */
    b->nrevs_fused = nrevs_fused;
    b->index_bit = index_bit;
    b->encoding = enc;
    b->cell_ns = cell_ns;
    b->deriv = deriv;
    b->gen++;
    t->has_bitstream = true;
    if (t->encoding == UFT_ENC_UNKNOWN) t->encoding = enc;
    d->feat_dirty = true;

    if (index_bit == SIZE_MAX)
        INGEST_DIAG(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_BITSTREAM, t->cyl,
                    t->head, -1, "NO_INDEX_BIT",
                    "Die Lage des Index im Bitstrom ist nicht bekannt — "
                    "Sektoren ueber dem Index sind nicht erkennbar.");
    return true;
}

bool uft_d2_sector_crosses_index(const uft_d2_track_t *t,
                                 const uft_d2_sector_t *s) {
    if (!t || !s || !t->has_bitstream) return false;
    const size_t ix = t->bitstream.index_bit;
    if (ix == SIZE_MAX) return false;
    /* BEIDE Felder: ein Adressfeld ueber dem Index (IOI) und ein Datenfeld
     * ueber dem Index (DOI) sind verschiedene Schutzmuster. Wer nur das
     * Datenfeld prueft, sieht IOI nicht. */
    const size_t von = (s->idam_bit != SIZE_MAX) ? s->idam_bit : s->dam_bit;
    const size_t bis = s->data_end_bit;
    if (von == SIZE_MAX || bis == SIZE_MAX || bis <= von) return false;
    return von < ix && ix < bis;
}

uft_encoding_t uft_d2_sector_encoding(const uft_d2_track_t *t,
                                      const uft_d2_sector_t *s) {
    if (!s) return UFT_ENC_UNKNOWN;
    if (s->encoding != UFT_ENC_UNKNOWN) return s->encoding;
    return t ? t->encoding : UFT_ENC_UNKNOWN;
}

/* ═══════════════════════ Schicht 3: Sektoren ════════════════════════════ */

bool uft_d2_add_sector(uft_disk2_t *d, uft_d2_track_t *t,
                       const uft_d2_sector_t *s) {
    if (!d || !t || !s) return false;

    /* Zuversicht 255 heisst: vom Traeger gelesen UND belegt. Alles andere
     * ist rekonstruiert, gepolstert oder unsicher. */
    const bool belegt = s->data_crc_known && s->data_crc_ok
                     && s->origin != UFT_D2_ORIGIN_RECONSTRUCTED
                     && s->origin != UFT_D2_ORIGIN_PADDING
                     && s->weak_bits == 0u && s->fuzzy_bits == 0u;
    if (!zuversicht_traegt(s->conf, belegt)) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, t->cyl,
                    t->head, s->id_sec, "CONF_UNEARNED",
                    "Sektor %u beansprucht Zuversicht %u ohne Beleg "
                    "(CRC %s, Herkunft %s, weak %u, fuzzy %u).",
                    (unsigned)s->id_sec, (unsigned)s->conf,
                    !s->data_crc_known ? "nicht getragen"
                                       : (s->data_crc_ok ? "stimmt" : "FALSCH"),
                    uft_d2_origin_name(s->origin),
                    (unsigned)s->weak_bits, (unsigned)s->fuzzy_bits);
        return false;
    }

    uft_d2_sector_t *q = wachsen(t->sectors.items, &t->sectors.capacity,
                                 t->sectors.count + 1u,
                                 sizeof(*t->sectors.items));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, t->cyl,
                    t->head, s->id_sec, "NO_MEMORY",
                    "Kein Speicher fuer einen weiteren Sektor.");
        return false;
    }
    t->sectors.items = q;

    uft_d2_sector_t *o = &t->sectors.items[t->sectors.count];
    *o = *s;
    o->data = NULL;
    if (s->has_data && s->data && s->data_len) {
        o->data = malloc(s->data_len);
        if (!o->data) {
            uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, t->cyl,
                        t->head, s->id_sec, "NO_MEMORY",
                        "Kein Speicher fuer %u Datenbytes.",
                        (unsigned)s->data_len);
            return false;
        }
        memcpy(o->data, s->data, s->data_len);
    }
    /* Die Generation des Bitstroms, aus dem dieser Sektor stammt. Der
     * Aufrufer darf sie setzen (Laden aus einem Behaelter); sonst ist es
     * die aktuelle. */
    if (o->source_gen == 0u) o->source_gen = t->bitstream.gen;

    t->sectors.count++;
    t->sectors.gen++;
    t->has_sectors = true;
    d->feat_dirty = true;

    if (s->data_crc_known && !s->data_crc_ok)
        INGEST_DIAG(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS, t->cyl,
                    t->head, s->id_sec, "DATA_CRC",
                    "Die Datenpruefsumme stimmt nicht.");
    if (s->origin == UFT_D2_ORIGIN_PADDING)
        INGEST_DIAG(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_SECTORS, t->cyl,
                    t->head, s->id_sec, "PADDING",
                    "Fuellmaterial — auf dem Traeger stand hier nichts.");
    if (s->origin == UFT_D2_ORIGIN_RECONSTRUCTED)
        INGEST_DIAG(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS, t->cyl,
                    t->head, s->id_sec, "RECONSTRUCTED",
                    "Rekonstruiert — ein Versuch, keine Lesung.");
    return true;
}

/* ═══════════════════════ Schicht 4: Dateisysteme ════════════════════════ */

uft_d2_fs_t *uft_d2_add_fs(uft_disk2_t *d, uft_d2_fs_kind_t kind,
                           uft_d2_conf_t kind_conf, uft_d2_deriv_id_t deriv) {
    if (!d) return NULL;
    uft_d2_fs_t *q = wachsen(d->fs, &d->fs_cap, d->nfs + 1u, sizeof(*d->fs));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "NO_MEMORY", "Kein Speicher fuer ein weiteres Dateisystem.");
        return NULL;
    }
    d->fs = q;
    uft_d2_fs_t *f = &d->fs[d->nfs++];
    memset(f, 0, sizeof(*f));
    f->kind = kind; f->kind_conf = kind_conf; f->deriv = deriv;
    f->cyl_to = 0xFFFFu;             /* ganze Diskette, bis jemand einengt */
    d->feat_dirty = true;
    return f;
}

size_t uft_d2_fs_count(const uft_disk2_t *d) { return d ? d->nfs : 0u; }

uft_d2_fs_t *uft_d2_fs_at(uft_disk2_t *d, size_t i) {
    return (d && i < d->nfs) ? &d->fs[i] : NULL;
}

bool uft_d2_add_entry(uft_disk2_t *d, uft_d2_fs_t *fs,
                      const uft_d2_entry_t *e) {
    if (!d || !fs || !e) return false;

    /* Dieselbe Regel wie bei Umdrehungen und Sektoren. In MF-1272 gab es
     * sie hier NICHT — ein Eintrag mit abgerissener Kette und Zuversicht
     * 255 ging durch. */
    const bool belegt = !e->chain_broken && !e->cross_linked && !e->deleted;
    if (!zuversicht_traegt(e->conf, belegt)) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "CONF_UNEARNED",
                    "Eintrag \"%s\" beansprucht Zuversicht %u, ist aber %s.",
                    e->name, (unsigned)e->conf,
                    e->chain_broken ? "mit abgerissener Kette"
                    : e->cross_linked ? "querverwiesen" : "geloescht");
        return false;
    }

    uft_d2_entry_t *q = wachsen(fs->entries, &fs->capacity, fs->count + 1u,
                                sizeof(*fs->entries));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "NO_MEMORY", "Kein Speicher fuer einen Verzeichniseintrag.");
        return false;
    }
    fs->entries = q;
    uft_d2_entry_t *o = &fs->entries[fs->count++];
    *o = *e;
    /* Ein 64 Zeichen langer Name darf den Bericht nicht ueber das Feldende
     * hinaus lesen lassen (MF-1272). */
    o->name[UFT_D2_ENTRY_NAME - 1u] = '\0';
    if (e->deleted) fs->deleted_count++;
    d->feat_dirty = true;

    if (e->chain_broken)
        INGEST_DIAG(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "CHAIN_BROKEN", "Die Kette von \"%s\" reisst ab.", o->name);
    if (e->cross_linked)
        INGEST_DIAG(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "CROSS_LINKED",
                    "\"%s\" teilt Einheiten mit einer anderen Datei.", o->name);
    return true;
}

/* ═══════════════════════ Metadaten ══════════════════════════════════════ */

bool uft_d2_add_meta(uft_disk2_t *d, const char *key, const char *value,
                     uft_d2_meta_src_t src) {
    if (!d || !key || !value) return false;
    if (d->nmeta >= UFT_D2_MAX_META) {
        d->meta_hidden++;
        /* In MF-1272 fiel der 65. Eintrag STILL heraus. */
        if (d->meta_hidden == 1u)
            uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS, -1, -1, -1,
                        "META_OVERFLOW",
                        "Mehr als %u Metadaten; weitere werden gezaehlt, nicht "
                        "gespeichert.", (unsigned)UFT_D2_MAX_META);
        return false;
    }
    uft_d2_meta_t *q = wachsen(d->meta, &d->meta_cap, d->nmeta + 1u,
                               sizeof(*d->meta));
    if (!q) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, -1, -1, -1,
                    "NO_MEMORY", "Kein Speicher fuer Metadaten.");
        return false;
    }
    d->meta = q;
    uft_d2_meta_t *m = &d->meta[d->nmeta++];
    memset(m, 0, sizeof(*m));
    feld_setzen(m->key,   UFT_D2_META_KEY,   key);
    feld_setzen(m->value, UFT_D2_META_VALUE, value);
    m->src = src;
    d->feat_dirty = true;
    return true;
}

const char *uft_d2_meta(const uft_disk2_t *d, const char *key) {
    if (!d || !key) return NULL;
    for (size_t i = 0; i < d->nmeta; ++i)
        if (strcmp(d->meta[i].key, key) == 0) return d->meta[i].value;
    return NULL;
}

size_t uft_d2_meta_count(const uft_disk2_t *d) { return d ? d->nmeta : 0u; }

const uft_d2_meta_t *uft_d2_meta_at(const uft_disk2_t *d, size_t i) {
    return (d && i < d->nmeta) ? &d->meta[i] : NULL;
}

/* ═══════════════════════ Stimmigkeit ════════════════════════════════════ */

size_t uft_d2_validate(uft_disk2_t *d) {
    if (!d) return 0u;
    const size_t vorher = d->ndiag;

    for (size_t i = 0; i < d->ntracks; ++i) {
        uft_d2_track_t *t = &d->tracks[i];

        for (size_t r = 0; r < t->flux.count; ++r)
            if (t->flux.revs[r].index_time_ns && !t->flux.revs[r].count)
                uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FLUX, t->cyl,
                            t->head, -1, "REV_EMPTY_INDEX",
                            "Umdrehung %zu nennt eine Indexzeit, hat aber "
                            "keinen einzigen Flusswechsel.", r);

        if (t->has_bitstream && t->encoding != UFT_ENC_UNKNOWN
            && t->bitstream.encoding != UFT_ENC_UNKNOWN
            && t->encoding != t->bitstream.encoding)
            uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_BITSTREAM, t->cyl,
                        t->head, -1, "ENC_MISMATCH",
                        "Die Spur sagt Kodierung %d, ihr Bitstrom %d.",
                        (int)t->encoding, (int)t->bitstream.encoding);

        for (size_t s = 0; s < t->sectors.count; ++s) {
            const uft_d2_sector_t *x = &t->sectors.items[s];

            if (t->has_bitstream && x->source_gen
                && x->source_gen != t->bitstream.gen)
                uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS, t->cyl,
                            t->head, x->id_sec, "STALE_DERIV",
                            "Sektor %u stammt aus Bitstrom-Generation %u; "
                            "die aktuelle ist %u.", (unsigned)x->id_sec,
                            (unsigned)x->source_gen,
                            (unsigned)t->bitstream.gen);

            if (t->has_bitstream) {
                const size_t n = t->bitstream.nbits;
                if ((x->idam_bit     != SIZE_MAX && x->idam_bit     >= n)
                 || (x->dam_bit      != SIZE_MAX && x->dam_bit      >= n)
                 || (x->data_end_bit != SIZE_MAX && x->data_end_bit >  n))
                    uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS,
                                t->cyl, t->head, x->id_sec, "POS_BEYOND",
                                "Sektor %u nennt Bitlagen jenseits der %zu "
                                "Bit des Bitstroms.", (unsigned)x->id_sec, n);
            }
        }
    }

    uint16_t mc = 0u;
    const bool haben = uft_d2_extent(d, &mc, NULL);
    for (size_t i = 0; i < d->nfs; ++i) {
        const uft_d2_fs_t *f = &d->fs[i];
        if (f->cyl_to == 0xFFFFu) continue;          /* ganze Diskette */
        if (!haben || f->cyl_from > mc)
            uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FILESYSTEM, -1, -1,
                        -1, "FS_RANGE",
                        "Dateisystem %zu nennt die Zylinder %u..%u; die "
                        "hoechste vorhandene Spur ist %u.", i,
                        (unsigned)f->cyl_from, (unsigned)f->cyl_to,
                        haben ? (unsigned)mc : 0u);
    }
    return d->ndiag - vorher;
}

/* ═══════════════════════ Schichten und Merkmale ═════════════════════════ */

uint32_t uft_d2_layers(const uft_disk2_t *d) {
    if (!d) return 0u;
    uint32_t l = 0u;
    for (size_t i = 0; i < d->ntracks; ++i) {
        if (d->tracks[i].has_flux)      l |= 1u << UFT_D2_LAYER_FLUX;
        if (d->tracks[i].has_bitstream) l |= 1u << UFT_D2_LAYER_BITSTREAM;
        if (d->tracks[i].has_sectors)   l |= 1u << UFT_D2_LAYER_SECTORS;
    }
    if (d->nfs) l |= 1u << UFT_D2_LAYER_FILESYSTEM;
    return l;
}

static uint32_t merkmale_rechnen(const uft_disk2_t *d) {
    uint32_t f = 0u;
    uft_encoding_t first_enc = UFT_ENC_UNKNOWN;

    for (size_t i = 0; i < d->ntracks; ++i) {
        const uft_d2_track_t *t = &d->tracks[i];

        if (t->flux.count > 1u) f |= UFT_D2_FEAT_REVOLUTIONS;
        for (size_t r = 0; r < t->flux.count; ++r)
            if (t->flux.revs[r].index_time_ns) f |= UFT_D2_FEAT_INDEX_TIME;

        if (t->has_bitstream) {
            f |= UFT_D2_FEAT_GAPS;
            if (t->bitstream.bit_conf) f |= UFT_D2_FEAT_WEAK_BITS;
            if (t->bitstream.agree)    f |= UFT_D2_FEAT_VOTES;
        }
        if (t->unformatted) f |= UFT_D2_FEAT_UNFORMATTED;

        if (t->encoding != UFT_ENC_UNKNOWN) {
            if (first_enc == UFT_ENC_UNKNOWN) first_enc = t->encoding;
            else if (first_enc != t->encoding) f |= UFT_D2_FEAT_MIXED_ENC;
        }

        /* Doppelte Sektornummern: eine Bitmenge je Spur statt des
         * quadratischen Vergleichs aus MF-1272. 256 Nummern passen in
         * 32 Byte, und mehr kann es nicht geben — die ID ist ein Byte. */
        uint8_t gesehen[32];
        memset(gesehen, 0, sizeof(gesehen));

        bool have_ref = false;
        uint32_t ref = 0u;
        for (size_t s = 0; s < t->sectors.count; ++s) {
            const uft_d2_sector_t *x = &t->sectors.items[s];
            if (x->has_data) {
                /* MF-1272: verglichen werden nur Sektoren MIT Datenfeld,
                 * und der Bezug ist der erste davon. */
                if (!have_ref) { ref = x->data_len; have_ref = true; }
                else if (x->data_len != ref) f |= UFT_D2_FEAT_VAR_SECTOR_SZ;
            }
            if (x->data_crc_known && !x->data_crc_ok) f |= UFT_D2_FEAT_BAD_CRC;
            if (x->dam == 0xF8u || x->dam == 0xF9u) f |= UFT_D2_FEAT_DELETED_DAM;
            if (!x->has_data) f |= UFT_D2_FEAT_NO_DATA_SEC;
            if (x->weak_bits || x->fuzzy_bits) f |= UFT_D2_FEAT_WEAK_BITS;
            /* Kodierung je Sektor — Schutzverfahren mischen INNERHALB der
             * Spur, und das sah MF-1272 nicht. */
            if (x->encoding != UFT_ENC_UNKNOWN && t->encoding != UFT_ENC_UNKNOWN
                && x->encoding != t->encoding)
                f |= UFT_D2_FEAT_MIXED_ENC;

            const uint8_t bit = (uint8_t)(1u << (x->id_sec & 7u));
            if (gesehen[x->id_sec >> 3] & bit) f |= UFT_D2_FEAT_DUP_SECTORS;
            gesehen[x->id_sec >> 3] |= bit;
        }
    }
    if (d->nfs > 1u) f |= UFT_D2_FEAT_MULTI_FS;
    for (size_t i = 0; i < d->nfs; ++i)
        if (d->fs[i].deleted_count) { f |= UFT_D2_FEAT_DELETED_FILES; break; }
    if (d->nmeta) f |= UFT_D2_FEAT_METADATA;
    return f;
}

uint32_t uft_d2_features(const uft_disk2_t *d) {
    if (!d) return 0u;
    /* Der Cache ist eine Eigenschaft der Umsetzung, nicht des Zustands —
     * deshalb darf ein lesender Aufruf ihn fuellen. */
    uft_disk2_t *m = (uft_disk2_t *)d;
    if (m->feat_dirty) {
        m->feat_cache = merkmale_rechnen(m);
        m->feat_dirty = false;
    }
    return m->feat_cache;
}

bool uft_d2_check_loss(const uft_disk2_t *d, uint32_t target_layers,
                       uint32_t target_features,
                       uint32_t *out_lost_layers, uint32_t *out_lost_features) {
    const uint32_t have_l = uft_d2_layers(d);
    const uint32_t have_f = uft_d2_features(d);
    const uint32_t lost_l = have_l & ~target_layers;
    const uint32_t lost_f = have_f & ~target_features;
    if (out_lost_layers)   *out_lost_layers = lost_l;
    if (out_lost_features) *out_lost_features = lost_f;
    return lost_l == 0u && lost_f == 0u;
}

/* ═══════════════════════ Namen ══════════════════════════════════════════ */

const char *uft_d2_layer_name(uft_d2_layer_t l) {
    switch (l) {
    case UFT_D2_LAYER_FLUX:       return "Fluss";
    case UFT_D2_LAYER_BITSTREAM:  return "Bitstrom";
    case UFT_D2_LAYER_SECTORS:    return "Sektoren";
    case UFT_D2_LAYER_FILESYSTEM: return "Dateisystem";
    default:                      return "?";
    }
}

const char *uft_d2_feature_name(uft_d2_feature_t f) {
    switch (f) {
    case UFT_D2_FEAT_REVOLUTIONS:   return "getrennte Umdrehungen";
    case UFT_D2_FEAT_INDEX_TIME:    return "Indexzeiten";
    case UFT_D2_FEAT_WEAK_BITS:     return "Weak/Fuzzy-Bits";
    case UFT_D2_FEAT_GAPS:          return "Luecken";
    case UFT_D2_FEAT_VAR_SECTOR_SZ: return "ungleiche Sektorgroessen";
    case UFT_D2_FEAT_DUP_SECTORS:   return "doppelte Sektornummern";
    case UFT_D2_FEAT_BAD_CRC:       return "Sektoren mit falscher CRC";
    case UFT_D2_FEAT_DELETED_DAM:   return "geloeschte Adressmarken";
    case UFT_D2_FEAT_MIXED_ENC:     return "gemischte Kodierung";
    case UFT_D2_FEAT_NO_DATA_SEC:   return "Sektoren ohne Datenfeld";
    case UFT_D2_FEAT_UNFORMATTED:   return "unformatierte Spuren";
    case UFT_D2_FEAT_DELETED_FILES: return "geloeschte Dateien";
    case UFT_D2_FEAT_METADATA:      return "Metadaten";
    case UFT_D2_FEAT_MULTI_FS:      return "mehrere Dateisysteme";
    case UFT_D2_FEAT_VOTES:         return "Stimmen je Bit";
    default:                        return "?";
    }
}

const char *uft_d2_origin_name(uft_d2_origin_t o) {
    switch (o) {
    case UFT_D2_ORIGIN_MEDIUM:        return "Traeger";
    case UFT_D2_ORIGIN_CONTAINER:     return "Behaelter";
    case UFT_D2_ORIGIN_DERIVED:       return "abgeleitet";
    case UFT_D2_ORIGIN_FUSED:         return "fusioniert";
    case UFT_D2_ORIGIN_PADDING:       return "Fuellmaterial";
    case UFT_D2_ORIGIN_RECONSTRUCTED: return "rekonstruiert";
    default:                          return "unbekannt";
    }
}

/* ═══════════════════════ Bericht ════════════════════════════════════════ */

/** snprintf-Buchfuehrung: geschrieben wird, was passt; zurueckgegeben wird,
 *  was gebraucht WUERDE. Ein Aufrufer, der beides verwechselt, kuerzt still. */
typedef struct { char *buf; size_t buflen, written, need; } rep_t;

static void rep_app(rep_t *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[512];
    const int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    const size_t len = (size_t)n < sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1u;
    r->need += len;
    if (r->buf && r->written + 1u < r->buflen) {
        const size_t frei = r->buflen - r->written - 1u;
        const size_t k = len < frei ? len : frei;
        memcpy(r->buf + r->written, tmp, k);
        r->written += k;
        r->buf[r->written] = '\0';
    }
}

/* ── MF-1318: dieselben Zahlen, aber lesbar statt nur druckbar ────────
 *
 * `uft_d2_report()` rechnet unten sechs Zahlen — Umdrehungen, davon ohne
 * Indexzeit, Sektoren, mit CRC-Angabe, davon falsch, ohne CRC-Angabe —
 * und schreibt sie in einen Text. Es gibt im ganzen Baum KEINE Funktion,
 * die sie zurueckgibt (gemessen). Wer sie braucht, muesste den Bericht
 * zerlegen; das waere ein Parser fuer die eigene Ausgabe.
 *
 * Diese Projektion liefert genau dieselben Zahlen aus derselben
 * Schleifenform. Sie ist bewusst KEINE zweite Rechnung mit eigener
 * Logik — zwei Rechnungen derselben Groesse driften (MF-1177). Wenn hier
 * einmal etwas anderes herauskommt als im Bericht, ist das ein Fehler
 * und kein Spielraum; der Test nagelt beide gegeneinander fest. */
const char *uft_fakt_zustand_name(uft_fakt_zustand_t z) {
    switch (z) {
    case UFT_FAKT_UNGEMESSEN:     return "ungemessen";
    case UFT_FAKT_GEMESSEN:       return "gemessen";
    case UFT_FAKT_KEIN_ERZEUGER:  return "kein Erzeuger";
    default:                      return NULL;
    }
}

bool uft_d2_facts(const uft_disk2_t *d, uft_source_facts_t *aus) {
    if (!aus) return false;

    /* Alles auf UNGEMESSEN — das ist der Wert 0, und genau deshalb ist er
     * der Vorgabewert: eine genullte Struktur behauptet damit nichts. */
    memset(aus, 0, sizeof(*aus));

    /* Ohne Modell bleibt es dabei. Ein `false` waere hier falsch: die
     * Frage "gibt es Tatsachen?" ist beantwortet — mit "keine gemessen",
     * und das steht in der Struktur. */
    if (!d) return true;

    aus->ebenen   = UFT_FAKT_GEMESSEN;
    aus->layers   = uft_d2_layers(d);
    aus->merkmale = UFT_FAKT_GEMESSEN;
    aus->features = uft_d2_features(d);

    /* EIN Durchlauf ueber das Modell. Die Weak-Zaehlung stand zuerst als
     * zweite Schleife daneben — dasselbe Muster, das diese Funktion einen
     * Schritt vorher aus `uft_d2_report()` entfernt hat (MF-1177). Zwei
     * Schleifen ueber dieselbe Menge driften auseinander, sobald jemand
     * eine Abbruchbedingung nur in einer aendert. */
    size_t revs_total = 0u, no_index = 0u, sectors_total = 0u;
    size_t crc_checked = 0u, crc_bad = 0u, crc_unknown = 0u;
    size_t weak_sekt = 0u, weak_bits = 0u;
    for (size_t i = 0; i < d->ntracks; ++i) {
        const uft_d2_track_t *t = &d->tracks[i];
        revs_total += t->flux.count;
        for (size_t k = 0; k < t->flux.count; ++k)
            if (!t->flux.revs[k].index_time_ns) no_index++;
        sectors_total += t->sectors.count;
        for (size_t k = 0; k < t->sectors.count; ++k) {
            const uft_d2_sector_t *x = &t->sectors.items[k];

            /* Flackern zuerst: die CRC-Zweige unten springen mit
             * `continue` weiter, und ein Sektor OHNE CRC-Angabe kann sehr
             * wohl Weak-Bits tragen — in der ersten Fassung stand die
             * Zaehlung in einer eigenen Schleife und war davon nicht
             * betroffen; hier waere sie es. */
            if (x->weak_bits || x->fuzzy_bits) {
                weak_sekt++;
                weak_bits += (size_t)x->weak_bits + (size_t)x->fuzzy_bits;
            }

            if (!x->data_crc_known) { crc_unknown++; continue; }
            crc_checked++;
            if (!x->data_crc_ok) crc_bad++;
        }
    }

    aus->sektoren      = UFT_FAKT_GEMESSEN;
    aus->sectors_total = sectors_total;
    aus->crc_checked   = crc_checked;
    aus->crc_bad       = crc_bad;
    aus->crc_unknown   = crc_unknown;

    aus->umdrehungen     = UFT_FAKT_GEMESSEN;
    aus->revs_total      = revs_total;
    aus->revs_ohne_index = no_index;

    /* Fluss — der Zustand FOLGT dem Modell, er wird nicht behauptet.
     *
     * Die erste Fassung setzte hier unbedingt KEIN_ERZEUGER, begruendet
     * mit `FLUX_NOT_BRIDGED`. Das ist eine Aussage ueber EINEN Fuellweg
     * (`uft_d2_from_disk()`), und sie stimmt fuer den; als Aussage ueber
     * das MODELL war sie falsch, denn der UFTD-Leser
     * (`uft_disk2_io.c:370`) ruft `uft_d2_add_revolution()`. Die Struktur
     * haette sonst `revs_total > 0` neben „kein Erzeuger fuer Fluss"
     * getragen — ein Widerspruch in sich selbst, Gestalt von MF-1038. */
    aus->fluss = revs_total ? UFT_FAKT_GEMESSEN : UFT_FAKT_KEIN_ERZEUGER;

    /* PLL — hier haelt die Luecke, und zwar gemessen: `uft_d2_track_t`
     * hat kein Feld dafuer, und `flux_pll_t` rechnet `locked` und
     * `residual_rms` (MF-1136), ist aber an allen fuenf Stellen in
     * `uft_flux_decoder.c` eine Stapelvariable und wird nie herausgegeben.
     * Es gibt also nichts, was diese Werte je gefuellt haette. */
    aus->pll = UFT_FAKT_KEIN_ERZEUGER;

    /* Weak — ebenfalls berichtigt. `WEAK_MASK_NOT_BRIDGED` meldet, dass
     * die per-BIT-Maske nicht uebersetzt wird; die Angabe JE SEKTOR wird
     * es sehr wohl (`uft_disk2_bridge.c:88-94`). Zwei Dinge unter einem
     * Wort — aufgeloest durch eine Unterscheidung, nicht durch eine
     * Entscheidung (Gestalt von MF-1037).
     *
     * `weak_bits_min` heisst so, weil es eine Untergrenze IST: die
     * Bruecke traegt fuer eine blosse Flagge ohne Maske den Wert 1 ein
     * („mindestens eines") und zaehlt mit Maske markierte BYTES. Eine
     * Zahl, die kleiner sein kann als die Wahrheit, darf nicht klingen
     * wie eine Zaehlung (MF-980). */
    aus->weak          = UFT_FAKT_GEMESSEN;
    aus->weak_sektoren = weak_sekt;
    aus->weak_bits_min = weak_bits;

    return true;
}

bool uft_facts_erkennung(const struct uft_disk *disk, uft_source_facts_t *aus) {
    if (!aus) return false;
    if (!disk) return true;   /* bleibt UNGEMESSEN */

    const uft_disk_t *d = (const uft_disk_t *)disk;
    if (!d->probe_gemessen) return true;   /* vor MF-1317 geoeffnet */

    aus->erkennung  = UFT_FAKT_GEMESSEN;
    aus->confidence = d->probe_confidence;
    aus->tied       = d->probe_tied;
    return true;
}

size_t uft_d2_report(const uft_disk2_t *d, char *buf, size_t buflen) {
    rep_t r = { buf, buflen, 0u, 0u };
    if (buf && buflen) buf[0] = '\0';
    if (!d) return 0u;
    #define APP(...) rep_app(&r, __VA_ARGS__)

    /* Die Fehlerzahl steht OBEN. Wer den Bericht kuerzt oder nur die erste
     * Zeile zeigt, sieht trotzdem, dass etwas falsch ist. */
    const size_t fehler = uft_d2_diag_count_sev(d, UFT_D2_DIAG_ERROR)
                        + d->diag_hidden_err;
    if (fehler) APP("FEHLER: %zu\n", fehler);

    uint16_t mc; uint8_t mh;
    if (uft_d2_extent(d, &mc, &mh))
        APP("Traeger: %zu Spuren, hoechste Lage C%u H%u (gemessen)\n",
            d->ntracks, (unsigned)mc, (unsigned)mh);
    else
        APP("Traeger: keine Spuren\n");

    const uint32_t L = uft_d2_layers(d);
    APP("Schichten:");
    for (unsigned l = 0; l < UFT_D2_LAYER_COUNT; ++l)
        if (L & (1u << l)) APP(" %s", uft_d2_layer_name((uft_d2_layer_t)l));
    if (!L) APP(" keine");
    APP("\n");

    const uint32_t F = uft_d2_features(d);
    APP("Merkmale:");
    unsigned nf = 0u;
    for (unsigned b = 0; b < UFT_D2_FEAT_COUNT; ++b)
        if (F & (1u << b)) {
            APP(" %s;", uft_d2_feature_name((uft_d2_feature_t)(1u << b)));
            nf++;
        }
    if (!nf) APP(" keine");
    APP("\n");

    /* MF-1318: der Bericht RECHNET diese Zahlen nicht mehr selbst.
     *
     * Bis hierher stand die Zaehlschleife zweimal woertlich im Baum — einmal
     * hier und einmal in `uft_d2_facts()`. Gefunden hat das die eigene
     * Mutationsmatrix: drei Mutationen liessen sich nicht isolieren, weil ihr
     * Anker zweimal vorkam. Das ist der Fall aus MF-1177 („eine Groesse, eine
     * Rechnung"), und er waegt hier doppelt, weil der Test die beiden
     * gegeneinander haelt: zwei Kopien, die sich einig sind, belegen nur ihre
     * Einigkeit. Seit MF-1318 gibt es EINE Rechnung, und der Bericht ist ihr
     * Leser. */
    uft_source_facts_t f;
    (void)uft_d2_facts(d, &f);

    if (f.revs_total && f.revs_ohne_index)
        APP("Fluss: %zu Umdrehungen — davon ohne Indexzeit: %zu\n",
            f.revs_total, f.revs_ohne_index);
    else if (f.revs_total)
        APP("Fluss: %zu Umdrehungen\n", f.revs_total);
    /* MF-1272: „falsche CRC: 0" ist bei einem Abbild ohne Pruefsumme kein
     * Befund, sondern ein Urteil in eine Richtung, das niemand gefaellt
     * hat. Deshalb drei Zahlen statt einer. */
    if (f.sectors_total)
        APP("Sektoren: %zu — mit CRC-Angabe: %zu (davon falsch: %zu), "
            "ohne CRC-Angabe: %zu\n",
            f.sectors_total, f.crc_checked, f.crc_bad, f.crc_unknown);

    for (size_t i = 0; i < d->nfs; ++i) {
        const uft_d2_fs_t *f = &d->fs[i];
        APP("Dateisystem %zu: %s (Zuversicht %u)", i,
            f->kind == UFT_D2_FS_NONE_TRACKLOADER ? "KEINES — Trackloader"
                                                  : "erkannt",
            (unsigned)f->kind_conf);
        if (f->label[0]) APP(", Etikett \"%s\"", f->label);
        if (f->cyl_to != 0xFFFFu)
            APP(", Zylinder %u..%u", (unsigned)f->cyl_from, (unsigned)f->cyl_to);
        if (f->count) APP(", %zu Eintraege, %zu geloescht",
                          f->count, f->deleted_count);
        if (f->counters_checked)
            APP(", Zaehler %s", f->counters_consistent ? "stimmen"
                                                       : "WIDERSPRECHEN");
        APP("\n");
    }

    if (d->nmeta) {
        APP("Metadaten:\n");
        for (size_t i = 0; i < d->nmeta; ++i)
            APP("  %-18s %s\n", d->meta[i].key, d->meta[i].value);
        if (d->meta_hidden)
            APP("  ... und %zu weitere, nicht gespeichert\n", d->meta_hidden);
    }

    if (d->ndiag) {
        APP("Befunde (%zu):\n", d->ndiag);
        const size_t zeig = d->ndiag < 40u ? d->ndiag : 40u;
        for (size_t i = 0; i < zeig; ++i) {
            const uft_d2_diag_t *g = &d->diag[i];
            const char *sv = g->sev == UFT_D2_DIAG_ERROR ? "FEHLER"
                           : g->sev == UFT_D2_DIAG_WARN  ? "WARN"
                           : g->sev == UFT_D2_DIAG_NOTE  ? "note" : "info";
            if (g->cyl >= 0)
                APP("  [%s] %s C%d H%d%s%d: %s\n", sv, g->code,
                    (int)g->cyl, (int)g->head, g->sector >= 0 ? " S" : "",
                    g->sector >= 0 ? (int)g->sector : 0, g->text);
            else
                APP("  [%s] %s: %s\n", sv, g->code, g->text);
        }
        /* Ungezeigt sind die gespeicherten hinter Platz 40 UND die, die gar
         * nicht erst gespeichert wurden. „und n weitere" allein waere eine
         * Zahl ohne Gewicht: ein Fehler darunter wiegt anders als eine
         * Notiz. */
        const size_t rest = d->ndiag - zeig + d->diag_hidden;
        size_t rest_err = d->diag_hidden_err;
        for (size_t i = zeig; i < d->ndiag; ++i)
            if (d->diag[i].sev == UFT_D2_DIAG_ERROR) rest_err++;
        if (rest && rest_err)
            APP("  ... und %zu weitere, davon %zu FEHLER\n", rest, rest_err);
        else if (rest)
            APP("  ... und %zu weitere\n", rest);
    }
    #undef APP
    return r.need;
}
