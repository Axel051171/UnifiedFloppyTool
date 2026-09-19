/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2.c — Umsetzung von uft_disk2.h.
 *
 * Entwurf des Eigentuemers (UFT-NN „Das Zentrum"), eingebaut MF-1272. Die
 * Abweichungen vom Entwurf stehen im Kopf von `uft_disk2.h` unter
 * „BERICHTIGT GEGENUEBER DEM ENTWURF" — hier je an der Stelle.
 */

#include "uft/core/uft_disk2.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct uft_disk2 {
    uft_d2_track_t *tracks;
    size_t          ntracks, tcap;
    uft_d2_fs_t     fs;
    bool            has_fs;
    uft_d2_meta_t   meta[UFT_D2_MAX_META];
    size_t          nmeta;
    bool            meta_overflow;
    uft_d2_diag_t   diag[UFT_D2_MAX_DIAG];
    size_t          ndiag;
    bool            diag_overflow;
};

/* ═══════════════════════ Anlegen und Freigeben ══════════════════════════ */

uft_disk2_t *uft_d2_create(void) {
    uft_disk2_t *d = calloc(1, sizeof(*d));
    return d;
}

static void free_track(uft_d2_track_t *t) {
    for (size_t r = 0; r < t->flux.count; ++r) free(t->flux.revs[r].intervals);
    free(t->flux.revs);
    free(t->bitstream.bits);
    free(t->bitstream.bit_conf);
    free(t->bitstream.phase_q8);
    free(t->bitstream.flux_count);
    for (size_t s = 0; s < t->sectors.count; ++s) free(t->sectors.items[s].data);
    free(t->sectors.items);
}

void uft_d2_destroy(uft_disk2_t *d) {
    if (!d) return;
    for (size_t i = 0; i < d->ntracks; ++i) free_track(&d->tracks[i]);
    free(d->tracks);
    free(d->fs.entries);
    free(d);
}

/* ═══════════════════════ Befunde ════════════════════════════════════════ */

bool uft_d2_diag(uft_disk2_t *d, uft_d2_diag_sev_t sev, uft_d2_layer_t layer,
                 int cyl, int head, int sector, const char *code,
                 const char *fmt, ...) {
    if (!d) return false;
    /* Der letzte Platz ist von Anfang an reserviert (MF-1272): der Entwurf
     * nahm den 512. Befund an und ueberschrieb ihn danach still mit der
     * Ueberlaufmeldung — ein Aufrufer bekam `true` fuer etwas, das nie
     * ankam. Jetzt werden UFT_D2_MAX_DIAG - 1 angenommen, und der erste
     * abgewiesene macht die Meldung. */
    if (d->ndiag >= UFT_D2_MAX_DIAG - 1u) {
        if (!d->diag_overflow) {
            uft_d2_diag_t *o = &d->diag[UFT_D2_MAX_DIAG - 1u];
            o->sev = UFT_D2_DIAG_WARN; o->layer = layer;
            o->cyl = -1; o->head = -1; o->sector = -1;
            o->code = "DIAG_OVERFLOW";
            snprintf(o->text, sizeof(o->text),
                     "Mehr als %u Befunde — die Liste ist UNVOLLSTAENDIG.",
                     (unsigned)(UFT_D2_MAX_DIAG - 1u));
            d->diag_overflow = true;
            d->ndiag = UFT_D2_MAX_DIAG;
        }
        return false;
    }
    uft_d2_diag_t *g = &d->diag[d->ndiag++];
    g->sev = sev; g->layer = layer;
    g->cyl = (int16_t)cyl; g->head = (int8_t)head; g->sector = (int16_t)sector;
    g->code = code ? code : "";
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->text, sizeof(g->text), fmt ? fmt : "", ap);
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

/* ═══════════════════════ Spuren ═════════════════════════════════════════ */

static uft_d2_track_t *find_track(uft_disk2_t *d, uint16_t cyl, uint8_t head) {
    for (size_t i = 0; i < d->ntracks; ++i)
        if (d->tracks[i].cyl == cyl && d->tracks[i].head == head)
            return &d->tracks[i];
    return NULL;
}

uft_d2_track_t *uft_d2_track(uft_disk2_t *d, uint16_t cyl, uint8_t head) {
    if (!d) return NULL;
    uft_d2_track_t *t = find_track(d, cyl, head);
    if (t) return t;

    if (d->ntracks == d->tcap) {
        const size_t cap = d->tcap ? d->tcap * 2u : 32u;
        uft_d2_track_t *p = realloc(d->tracks, cap * sizeof(*p));
        if (!p) return NULL;
        d->tracks = p;
        d->tcap = cap;
    }
    t = &d->tracks[d->ntracks++];
    memset(t, 0, sizeof(*t));
    t->cyl = cyl;
    t->head = head;
    t->bitstream.index_bit = SIZE_MAX;
    return t;
}

const uft_d2_track_t *uft_d2_track_get(const uft_disk2_t *d, uint16_t cyl,
                                       uint8_t head) {
    return d ? find_track((uft_disk2_t *)d, cyl, head) : NULL;
}

size_t uft_d2_track_count(const uft_disk2_t *d) { return d ? d->ntracks : 0u; }

bool uft_d2_extent(const uft_disk2_t *d, uint16_t *out_max_cyl,
                   uint8_t *out_max_head) {
    if (!d || d->ntracks == 0u) return false;
    uint16_t mc = 0u; uint8_t mh = 0u;
    for (size_t i = 0; i < d->ntracks; ++i) {
        if (d->tracks[i].cyl > mc) mc = d->tracks[i].cyl;
        if (d->tracks[i].head > mh) mh = d->tracks[i].head;
    }
    if (out_max_cyl) *out_max_cyl = mc;
    if (out_max_head) *out_max_head = mh;
    return true;
}

/* ═══════════════════════ Schicht 1: Fluss ═══════════════════════════════ */

bool uft_d2_add_revolution(uft_disk2_t *d, uft_d2_track_t *t,
                           const uint32_t *intervals, size_t count,
                           uint32_t index_time_ns, bool complete,
                           const uft_d2_derivation_t *deriv) {
    if (!d || !t || (!intervals && count)) return false;

    uft_d2_flux_t *f = &t->flux;
    uft_d2_rev_t *p = realloc(f->revs, (f->count + 1u) * sizeof(*p));
    if (!p) return false;
    f->revs = p;

    uft_d2_rev_t *r = &f->revs[f->count];
    memset(r, 0, sizeof(*r));
    if (count) {
        r->intervals = malloc(count * sizeof(uint32_t));
        if (!r->intervals) return false;
        memcpy(r->intervals, intervals, count * sizeof(uint32_t));
    }
    r->count = count;
    r->index_time_ns = index_time_ns;
    r->complete = complete;
    /* Der Entwurf setzte hier das Literal 128. Die Zahl hat jetzt einen
     * Namen und damit eine Bedeutung, die auch die Bruecke benutzt. */
    r->conf = complete ? UFT_D2_CONF_CERTAIN : UFT_D2_CONF_UNVERIFIED;
    f->count++;
    if (deriv) f->deriv = *deriv;
    t->has_flux = true;

    /* Ohne Indexzeit keine Drehzahl und kein Fuzzy-Nachweis. Das ist ein
     * BEFUND ueber den Transport — uft_scp_direct.c:355 verwirft sie
     * heute — und er gehoert ausgesprochen, nicht durchgereicht. */
    if (index_time_ns == 0u)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FLUX, t->cyl, t->head, -1,
                    "NO_INDEX_TIME",
                    "Umdrehung %zu ohne Indexzeit: Drehzahl nicht messbar, "
                    "Fuzzy-Bits nicht nachweisbar.", f->count - 1u);
    if (!complete)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FLUX, t->cyl, t->head, -1,
                    "REV_INCOMPLETE", "Umdrehung %zu unvollstaendig.",
                    f->count - 1u);
    return true;
}

/* ═══════════════════════ Schicht 2: Bitstrom ════════════════════════════ */

static void *dup_or_null(const void *src, size_t bytes) {
    if (!src || !bytes) return NULL;
    void *p = malloc(bytes);
    if (p) memcpy(p, src, bytes);
    return p;
}

static void bitstream_clear(uft_d2_bitstream_t *b) {
    free(b->bits); free(b->bit_conf); free(b->phase_q8); free(b->flux_count);
    memset(b, 0, sizeof(*b));
    b->index_bit = SIZE_MAX;
}

bool uft_d2_set_bitstream(uft_disk2_t *d, uft_d2_track_t *t,
                          const uint8_t *bits, size_t nbits,
                          const uft_d2_conf_t *bit_conf,
                          const int16_t *phase_q8, const uint16_t *flux_count,
                          size_t index_bit, uft_encoding_t enc,
                          uint32_t cell_ns, const uft_d2_derivation_t *deriv) {
    if (!d || !t || !bits || !nbits) return false;

    uft_d2_bitstream_t *b = &t->bitstream;
    bitstream_clear(b);
    t->has_bitstream = false;

    const size_t nbytes = (nbits + 7u) / 8u;
    b->bits = dup_or_null(bits, nbytes);
    if (!b->bits) return false;
    b->nbits = nbits;                    /* GEMESSEN. Nicht geklemmt. */
    b->bit_conf   = dup_or_null(bit_conf,   nbits * sizeof(uft_d2_conf_t));
    b->phase_q8   = dup_or_null(phase_q8,   nbits * sizeof(int16_t));
    b->flux_count = dup_or_null(flux_count, nbits * sizeof(uint16_t));
    /* MF-1272: eine uebergebene Beilage, die nicht ankommt, ist kein
     * Bitstrom ohne Beilage — es ist ein Fehlschlag. Der Entwurf liess sie
     * still NULL, und `uft_d2_features()` haette daraufhin „keine Weak-Bits
     * messbar" gemeldet, obwohl der Aufrufer sie geliefert hatte. */
    if ((bit_conf && !b->bit_conf) || (phase_q8 && !b->phase_q8)
        || (flux_count && !b->flux_count)) {
        bitstream_clear(b);
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_BITSTREAM, t->cyl,
                    t->head, -1, "NO_MEMORY",
                    "Bitstrom mit %zu Bit nicht uebernommen: kein Speicher "
                    "fuer eine der Beilagen.", nbits);
        return false;
    }
    b->index_bit  = index_bit;
    b->encoding   = enc;
    b->cell_ns    = cell_ns;
    if (deriv) b->deriv = *deriv;
    t->has_bitstream = true;
    if (t->encoding == UFT_ENC_UNKNOWN) t->encoding = enc;

    /* Was fehlt, fehlt — und es wird gesagt. Ein Bitstrom ohne Konfidenz
     * je Bit kann Weak/Fuzzy nicht tragen, und ein Schutzerkenner, der
     * darauf laeuft, muss "nicht messbar" melden, nicht "nichts gefunden". */
    if (!bit_conf)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_BITSTREAM, t->cyl, t->head,
                    -1, "NO_BIT_CONF",
                    "Keine Konfidenz je Bit — Weak/Fuzzy nicht messbar.");
    if (index_bit == SIZE_MAX)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_BITSTREAM, t->cyl, t->head,
                    -1, "NO_INDEX_BIT",
                    "Indexlage im Bitstrom unbekannt — 'Sektor ueber dem "
                    "Index' nicht pruefbar.");
    return true;
}

/* ═══════════════════════ Schicht 3: Sektoren ════════════════════════════ */

bool uft_d2_add_sector(uft_disk2_t *d, uft_d2_track_t *t,
                       const uft_d2_sector_t *s) {
    if (!d || !t || !s) return false;

    /* Zuversicht steigt nie ohne Beleg. Das ist die eine Regel, die das
     * ganze Modell zusammenhaelt — und sie wird beim Einspeisen geprueft,
     * nicht irgendwo spaeter. */
    if (s->conf == UFT_D2_CONF_CERTAIN) {
        const char *why = NULL;
        if (s->data_crc_known && !s->data_crc_ok)
            why = "CRC falsch";
        else if (s->origin == UFT_D2_ORIGIN_RECONSTRUCTED)
            why = "Herkunft ist rekonstruiert";
        else if (s->origin == UFT_D2_ORIGIN_PADDING)
            why = "Herkunft ist Fuellmaterial";
        else if (s->weak_bits || s->fuzzy_bits)
            why = "Datenfeld enthaelt flackernde Bits";
        if (why) {
            uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_SECTORS, t->cyl,
                        t->head, s->id_sec, "CONF_UNEARNED",
                        "Sektor mit Zuversicht 255 abgewiesen: %s. Zuversicht "
                        "steigt nie ohne Beleg.", why);
            return false;
        }
    }

    uft_d2_sectors_t *S = &t->sectors;
    if (S->count == S->capacity) {
        const size_t cap = S->capacity ? S->capacity * 2u : 16u;
        uft_d2_sector_t *p = realloc(S->items, cap * sizeof(*p));
        if (!p) return false;
        S->items = p;
        S->capacity = cap;
    }
    uft_d2_sector_t *n = &S->items[S->count];
    *n = *s;
    n->data = NULL;
    if (s->data && s->data_len) {
        n->data = dup_or_null(s->data, s->data_len);
        if (!n->data) return false;
    }
    S->count++;
    t->has_sectors = true;

    /* Tatsachen, die ein Format sonst still glaettet, werden BENANNT. */
    for (size_t i = 0; i + 1u < S->count; ++i)
        if (S->items[i].id_sec == s->id_sec) {
            uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_SECTORS, t->cyl,
                        t->head, s->id_sec, "DUP_SECTOR",
                        "Sektornummer %u kommt mehrfach vor.", s->id_sec);
            break;
        }
    if (s->id_cyl != t->cyl && s->id_crc_known && s->id_crc_ok)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_SECTORS, t->cyl, t->head,
                    s->id_sec, "ID_CYL_MISMATCH",
                    "Adressfeld nennt Zylinder %u, Spur liegt auf %u.",
                    s->id_cyl, t->cyl);
    if (s->has_data && s->data_crc_known && !s->data_crc_ok)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_SECTORS, t->cyl, t->head,
                    s->id_sec, "DATA_CRC", "Daten-CRC falsch.");
    if (!s->has_data)
        uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_SECTORS, t->cyl, t->head,
                    s->id_sec, "NO_DATA_FIELD",
                    "Adressfeld ohne Datenfeld.");
    return true;
}

/* ═══════════════════════ Schicht 4: Dateisystem ═════════════════════════ */

uft_d2_fs_t *uft_d2_fs(uft_disk2_t *d) {
    if (!d) return NULL;
    d->has_fs = true;
    return &d->fs;
}

bool uft_d2_add_entry(uft_disk2_t *d, const uft_d2_entry_t *e) {
    if (!d || !e) return false;
    uft_d2_fs_t *f = &d->fs;
    if (f->count == f->capacity) {
        const size_t cap = f->capacity ? f->capacity * 2u : 32u;
        uft_d2_entry_t *p = realloc(f->entries, cap * sizeof(*p));
        if (!p) return false;
        f->entries = p;
        f->capacity = cap;
    }
    uft_d2_entry_t *n = &f->entries[f->count++];
    *n = *e;
    /* MF-1272: ein 64 Zeichen langer Name ohne NUL liesse den Bericht
     * hinter das Feld lesen. Das Feld ist der Vertrag, nicht der Aufrufer. */
    n->name[sizeof(n->name) - 1u] = '\0';
    f->label[sizeof(f->label) - 1u] = '\0';
    d->has_fs = true;
    if (n->deleted) f->deleted_count++;
    if (n->chain_broken)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "CHAIN_BROKEN",
                    "\"%s\": Kette abgerissen, fortlaufend gelesen — ein "
                    "VERSUCH, kein Befund.", n->name);
    if (n->cross_linked)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FILESYSTEM, -1, -1, -1,
                    "CROSS_LINKED",
                    "\"%s\" beansprucht Bloecke einer anderen Datei.", n->name);
    return true;
}

/* ═══════════════════════ Metadaten ══════════════════════════════════════ */

bool uft_d2_add_meta(uft_disk2_t *d, const char *key, const char *value,
                     uft_d2_meta_src_t src) {
    if (!d || !key || !value || !value[0]) return false;
    if (d->nmeta >= UFT_D2_MAX_META) { d->meta_overflow = true; return false; }
    uft_d2_meta_t *m = &d->meta[d->nmeta++];
    strncpy(m->key, key, sizeof(m->key) - 1u);   m->key[sizeof(m->key) - 1u] = '\0';
    strncpy(m->value, value, sizeof(m->value) - 1u);
    m->value[sizeof(m->value) - 1u] = '\0';
    if (strlen(value) >= sizeof(m->value)) {
        /* Kuerzung sichtbar, nicht stillschweigend. */
        memcpy(m->value + sizeof(m->value) - 4u, "...", 4u);
    }
    m->src = src;
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

/* ═══════════════════════ Was der Traeger traegt ═════════════════════════ */

uint32_t uft_d2_layers(const uft_disk2_t *d) {
    if (!d) return 0u;
    uint32_t m = 0u;
    for (size_t i = 0; i < d->ntracks; ++i) {
        if (d->tracks[i].has_flux)      m |= 1u << UFT_D2_LAYER_FLUX;
        if (d->tracks[i].has_bitstream) m |= 1u << UFT_D2_LAYER_BITSTREAM;
        if (d->tracks[i].has_sectors)   m |= 1u << UFT_D2_LAYER_SECTORS;
    }
    if (d->has_fs && d->fs.count) m |= 1u << UFT_D2_LAYER_FILESYSTEM;
    return m;
}

uint32_t uft_d2_features(const uft_disk2_t *d) {
    if (!d) return 0u;
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
        }
        if (t->unformatted) f |= UFT_D2_FEAT_UNFORMATTED;

        if (t->encoding != UFT_ENC_UNKNOWN) {
            if (first_enc == UFT_ENC_UNKNOWN) first_enc = t->encoding;
            else if (first_enc != t->encoding) f |= UFT_D2_FEAT_MIXED_ENC;
        }

        /* MF-1272: verglichen werden nur Sektoren MIT Datenfeld, und der
         * Bezug ist der erste davon. Der Entwurf nahm Sektor 0 als Bezug —
         * ein Adressfeld ohne Daten (Laenge 0) haette damit jede Spur als
         * „ungleiche Groessen" gemeldet. */
        bool have_ref = false;
        uint32_t ref = 0u;
        for (size_t s = 0; s < t->sectors.count; ++s) {
            const uft_d2_sector_t *x = &t->sectors.items[s];
            if (x->has_data) {
                if (!have_ref) { ref = x->data_len; have_ref = true; }
                else if (x->data_len != ref) f |= UFT_D2_FEAT_VAR_SECTOR_SZ;
            }
            if (x->data_crc_known && !x->data_crc_ok) f |= UFT_D2_FEAT_BAD_CRC;
            if (x->dam == 0xF8u || x->dam == 0xF9u) f |= UFT_D2_FEAT_DELETED_DAM;
            if (!x->has_data) f |= UFT_D2_FEAT_NO_DATA_SEC;
            if (x->weak_bits || x->fuzzy_bits) f |= UFT_D2_FEAT_WEAK_BITS;
            for (size_t k = 0; k < s; ++k)
                if (t->sectors.items[k].id_sec == x->id_sec) {
                    f |= UFT_D2_FEAT_DUP_SECTORS; break;
                }
        }
    }
    if (d->has_fs && d->fs.deleted_count) f |= UFT_D2_FEAT_DELETED_FILES;
    if (d->nmeta) f |= UFT_D2_FEAT_METADATA;
    return f;
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
    default:                        return "?";
    }
}

const char *uft_d2_origin_name(uft_d2_origin_t o) {
    switch (o) {
    case UFT_D2_ORIGIN_MEDIUM:        return "Traeger";
    case UFT_D2_ORIGIN_CONTAINER:     return "Abbilddatei";
    case UFT_D2_ORIGIN_DERIVED:       return "abgeleitet";
    case UFT_D2_ORIGIN_FUSED:         return "fusioniert";
    case UFT_D2_ORIGIN_PADDING:       return "Fuellmaterial";
    case UFT_D2_ORIGIN_RECONSTRUCTED: return "REKONSTRUIERT";
    default:                          return "unbekannt";
    }
}

/* ═══════════════════════ Bericht ════════════════════════════════════════ */

/* Schreibt nach `buf` und zaehlt in `need` mit, was der volle Bericht
 * braeuchte — auch dann, wenn nichts mehr passt. Bauform `snprintf`:
 * der Aufrufer sieht am Rueckgabewert, ob gekuerzt wurde (MF-1272). */
typedef struct { char *buf; size_t buflen; size_t written; size_t need; } rep_t;

static void rep_app(rep_t *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *dst = (r->written + 1u < r->buflen) ? r->buf + r->written : NULL;
    const size_t room = dst ? r->buflen - r->written : 0u;
    const int n = vsnprintf(dst, room, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    r->need += (size_t)n;
    if (dst) {
        r->written += (size_t)n;
        if (r->written >= r->buflen) r->written = r->buflen - 1u;
    }
}

size_t uft_d2_report(const uft_disk2_t *d, char *buf, size_t buflen) {
    if (!d || !buf || !buflen) return 0u;
    buf[0] = '\0';
    rep_t r = { buf, buflen, 0u, 0u };
    #define APP(...) rep_app(&r, __VA_ARGS__)

    /* Warnungen ZUERST. */
    const size_t nerr = uft_d2_diag_count_sev(d, UFT_D2_DIAG_ERROR);
    const size_t nwarn = uft_d2_diag_count_sev(d, UFT_D2_DIAG_WARN) - nerr;
    if (nerr)  APP("FEHLER: %zu Befunde der Stufe Fehler.\n", nerr);
    if (nwarn) APP("WARNUNG: %zu Befunde der Stufe Warnung.\n", nwarn);
    if (d->diag_overflow) APP("WARNUNG: Befundliste UNVOLLSTAENDIG.\n");
    if (d->meta_overflow) APP("WARNUNG: Metadatenliste UNVOLLSTAENDIG.\n");

    uint16_t mc = 0u; uint8_t mh = 0u;
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

    /* Je Spur eine Zeile — mit dem, was da ist, und mit dem, was fehlt. */
    size_t revs_total = 0u, no_index = 0u, sectors_total = 0u;
    size_t crc_checked = 0u, crc_bad = 0u, crc_unknown = 0u;
    for (size_t i = 0; i < d->ntracks; ++i) {
        const uft_d2_track_t *t = &d->tracks[i];
        revs_total += t->flux.count;
        for (size_t k = 0; k < t->flux.count; ++k)
            if (!t->flux.revs[k].index_time_ns) no_index++;
        sectors_total += t->sectors.count;
        for (size_t k = 0; k < t->sectors.count; ++k) {
            const uft_d2_sector_t *x = &t->sectors.items[k];
            if (!x->data_crc_known) { crc_unknown++; continue; }
            crc_checked++;
            if (!x->data_crc_ok) crc_bad++;
        }
    }
    /* Der erste Entwurf setzte hier `p--` und haengte die Zahl an die
     * schon geschriebene Zeile — ein Trick, der beim Puffer-Ende falsch
     * rechnet und den der Uebersetzer zu Recht angemahnt hat. Zwei
     * ehrliche Zweige sind laenger und richtig. */
    if (revs_total && no_index)
        APP("Fluss: %zu Umdrehungen — davon ohne Indexzeit: %zu\n",
            revs_total, no_index);
    else if (revs_total)
        APP("Fluss: %zu Umdrehungen\n", revs_total);
    /* MF-1272: „falsche CRC: 0" ist bei einem Abbild ohne Pruefsumme kein
     * Befund, sondern ein Urteil in eine Richtung, das niemand gefaellt
     * hat. Deshalb drei Zahlen statt einer. */
    if (sectors_total)
        APP("Sektoren: %zu — mit CRC-Angabe: %zu (davon falsch: %zu), "
            "ohne CRC-Angabe: %zu\n",
            sectors_total, crc_checked, crc_bad, crc_unknown);

    if (d->has_fs) {
        APP("Dateisystem: %s (Zuversicht %u)",
            d->fs.kind == UFT_D2_FS_NONE_TRACKLOADER ? "KEINES — Trackloader"
                                                     : "erkannt",
            (unsigned)d->fs.kind_conf);
        if (d->fs.count) APP(", %zu Eintraege, %zu geloescht",
                             d->fs.count, d->fs.deleted_count);
        if (d->fs.counters_checked)
            APP(", Zaehler %s", d->fs.counters_consistent ? "stimmen"
                                                           : "WIDERSPRECHEN");
        APP("\n");
    }

    if (d->nmeta) {
        APP("Metadaten:\n");
        for (size_t i = 0; i < d->nmeta; ++i)
            APP("  %-18s %s\n", d->meta[i].key, d->meta[i].value);
    }

    if (d->ndiag) {
        APP("Befunde (%zu):\n", d->ndiag);
        for (size_t i = 0; i < d->ndiag && i < 40u; ++i) {
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
        if (d->ndiag > 40u) APP("  ... und %zu weitere\n", d->ndiag - 40u);
    }
    #undef APP
    return r.need;
}
