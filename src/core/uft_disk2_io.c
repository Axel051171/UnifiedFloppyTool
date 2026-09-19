/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2_io.c
 * @brief UFTD lesen und schreiben.
 *
 * ── DREI STELLEN, AN DENEN DIESE FASSUNG VOM ENTWURF ABWEICHT ────────────
 *
 * 1. **Die Versionspruefung des Entwurfs konnte nie zuschlagen.** Dort
 *    stand `if ((ver >> 8) > (UFTD_VERSION >> 8) + 0u && ver > UFTD_VERSION)`
 *    — bei `UFTD_VERSION = 1` ist `UFTD_VERSION >> 8` gleich 0, und fuer
 *    jede Version unter 256 ist `ver >> 8` ebenfalls 0. Die Bedingung war
 *    fuer jede Fassung, die es geben kann, IMMER falsch: eine Datei der
 *    Fassung 2 haette ein Leser der Fassung 1 klaglos geoeffnet. Hier
 *    steht die grobe, aber wirksame Pruefung — dass sie grob ist, steht
 *    im Kopf von `uft_disk2_io.h`.
 *
 * 2. **`meta_hidden` wird mitgespeichert.** Der Entwurf sicherte
 *    `diag_hidden`/`diag_hidden_err`, aber nicht die Zahl der Metadaten
 *    ueber der Grenze. Ein Modell mit Ueberlauf haette nach einem
 *    Rundlauf einen anderen BERICHT gehabt als vorher — eine stille
 *    Aenderung genau der Art, gegen die der Behaelter gebaut ist.
 *
 * 3. **Befunde werden DIREKT eingesetzt, nicht ueber `uft_d2_diag()`.**
 *    Sie sind keine neuen Befunde, sie sind die gespeicherten. Ueber den
 *    normalen Weg haette ein Modell mit voller Befundliste beim Laden am
 *    reservierten letzten Platz angestossen und seinen eigenen
 *    Ueberlauf-Eintrag noch einmal als Ueberlauf gezaehlt.
 */

#include "uft_disk2_priv.h"
#include "uft/core/uft_disk2_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════ CRC32 ══════════════════════════════════════════ */

static uint32_t crc_tab[256];
static bool crc_ready;

static void crc_init(void) {
    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_tab[i] = c;
    }
    crc_ready = true;
}

uint32_t uftd_crc32(const uint8_t *p, size_t n) {
    if (!crc_ready) crc_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = crc_tab[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ═══════════════════════ Schreibpuffer ══════════════════════════════════ */

typedef struct { uint8_t *p; size_t n, cap; bool oom; } wbuf_t;

static bool wb_reserve(wbuf_t *w, size_t add) {
    if (w->oom) return false;
    if (w->n + add <= w->cap) return true;
    size_t c = w->cap ? w->cap : 4096u;
    while (c < w->n + add) c *= 2u;
    uint8_t *q = realloc(w->p, c);
    if (!q) { w->oom = true; return false; }
    w->p = q; w->cap = c;
    return true;
}
static void w8(wbuf_t *w, uint8_t v)   { if (wb_reserve(w, 1u)) w->p[w->n++] = v; }
static void w16(wbuf_t *w, uint16_t v) { w8(w, (uint8_t)v); w8(w, (uint8_t)(v >> 8)); }
static void w32(wbuf_t *w, uint32_t v) { w16(w, (uint16_t)v); w16(w, (uint16_t)(v >> 16)); }
static void w64(wbuf_t *w, uint64_t v) { w32(w, (uint32_t)v); w32(w, (uint32_t)(v >> 32)); }

static void wbytes(wbuf_t *w, const void *s, size_t n) {
    if (n && wb_reserve(w, n)) { memcpy(w->p + w->n, s, n); w->n += n; }
}

/** Zeichenketten IMMER in fester Feldbreite — kein Laengenpraefix, das
 *  beim Lesen ueber das Ende laufen kann. `memcpy` mit gemessener Laenge
 *  statt `strncpy`: das fuellt nicht, was es nicht kopiert. */
static void wstr(wbuf_t *w, const char *s, size_t field) {
    char tmp[256];
    memset(tmp, 0, sizeof(tmp));
    if (field > sizeof(tmp)) field = sizeof(tmp);
    if (s && field) {
        size_t n = strlen(s);
        if (n > field - 1u) n = field - 1u;
        memcpy(tmp, s, n);
    }
    wbytes(w, tmp, field);
}

/** Ein Block: Kennung, Laenge, Inhalt, CRC. Laenge und CRC werden nach dem
 *  Schreiben des Inhalts nachgetragen. */
typedef struct { size_t start; } blk_t;

static blk_t blk_begin(wbuf_t *w, const char tag[4]) {
    blk_t b; b.start = w->n;
    wbytes(w, tag, 4u);
    w32(w, 0u);                      /* Laenge, spaeter */
    return b;
}

static void blk_end(wbuf_t *w, blk_t b) {
    if (w->oom) return;
    const size_t body = b.start + 8u;
    const uint32_t len = (uint32_t)(w->n - body);
    w->p[b.start + 4u] = (uint8_t)len;
    w->p[b.start + 5u] = (uint8_t)(len >> 8);
    w->p[b.start + 6u] = (uint8_t)(len >> 16);
    w->p[b.start + 7u] = (uint8_t)(len >> 24);
    w32(w, uftd_crc32(w->p + body, len));
}

/* ═══════════════════════ Speichern ══════════════════════════════════════ */

static uftd_result_t res(uftd_err_t c, const char *what, size_t off,
                         const char *blk) {
    uftd_result_t r;
    memset(&r, 0, sizeof(r));
    r.code = c; r.what = what; r.offset = off;
    if (blk) memcpy(r.block, blk, 4u);
    return r;
}

uftd_result_t uftd_save(const uft_disk2_t *d, uint8_t **out_buf,
                        size_t *out_len) {
    if (!d || !out_buf || !out_len) return res(UFTD_E_ARG, "NULL", 0u, NULL);
    *out_buf = NULL; *out_len = 0u;
    wbuf_t w;
    memset(&w, 0, sizeof(w));

    /* Kopf */
    wbytes(&w, UFTD_MAGIC, 4u);
    w16(&w, UFTD_VERSION); w16(&w, 0u);
    w64(&w, 0u);                       /* Gesamtlaenge, spaeter */

    /* DERV — Platz 0 ist UFT_D2_DERIV_NONE und wird nicht geschrieben. */
    { blk_t b = blk_begin(&w, "DERV");
      w32(&w, (uint32_t)(d->nderiv ? d->nderiv - 1u : 0u));
      for (size_t i = 1; i < d->nderiv; ++i) {
          const uft_d2_derivation_t *v = &d->derivs[i];
          w8(&w, (uint8_t)v->from_layer); w8(&w, (uint8_t)v->origin);
          w32(&w, v->source_gen);
          wstr(&w, v->by, UFT_D2_DERIV_BY);
          wstr(&w, v->params, UFT_D2_DERIV_PARAMS);
      }
      blk_end(&w, b); }

    /* META — samt der Zahl derer ueber der Grenze. */
    { blk_t b = blk_begin(&w, "META");
      w32(&w, (uint32_t)d->nmeta);
      w32(&w, (uint32_t)d->meta_hidden);
      for (size_t i = 0; i < d->nmeta; ++i) {
          w8(&w, (uint8_t)d->meta[i].src);
          wstr(&w, d->meta[i].key, UFT_D2_META_KEY);
          wstr(&w, d->meta[i].value, UFT_D2_META_VALUE);
      }
      blk_end(&w, b); }

    /* TRAK je Spur */
    for (size_t i = 0; i < d->ntracks; ++i) {
        const uft_d2_track_t *t = &d->tracks[i];
        blk_t b = blk_begin(&w, "TRAK");
        w16(&w, t->cyl); w8(&w, t->head);
        w8(&w, (uint8_t)t->encoding); w8(&w, t->unformatted ? 1u : 0u);
        w8(&w, (uint8_t)((t->has_flux ? 1u : 0u) | (t->has_bitstream ? 2u : 0u)
                       | (t->has_sectors ? 4u : 0u)));

        if (t->has_flux) {
            blk_t f = blk_begin(&w, "FLUX");
            w16(&w, t->flux.deriv); w32(&w, t->flux.gen);
            w32(&w, (uint32_t)t->flux.count);
            for (size_t r = 0; r < t->flux.count; ++r) {
                const uft_d2_rev_t *v = &t->flux.revs[r];
                w32(&w, v->index_time_ns); w8(&w, v->complete ? 1u : 0u);
                w8(&w, v->conf); w32(&w, (uint32_t)v->count);
                wbytes(&w, v->intervals, v->count * 4u);
            }
            blk_end(&w, f);
        }
        if (t->has_bitstream) {
            const uft_d2_bitstream_t *s = &t->bitstream;
            blk_t f = blk_begin(&w, "BITS");
            w16(&w, s->deriv); w32(&w, s->gen);
            w64(&w, (uint64_t)s->nbits);
            w64(&w, (uint64_t)s->index_bit);
            w8(&w, (uint8_t)s->encoding); w32(&w, s->cell_ns);
            w8(&w, s->nrevs_fused);
            w8(&w, (uint8_t)((s->bit_conf ? 1u : 0u) | (s->agree ? 2u : 0u)
                           | (s->phase_q8 ? 4u : 0u) | (s->flux_count ? 8u : 0u)));
            wbytes(&w, s->bits, (s->nbits + 7u) / 8u);
            if (s->bit_conf)   wbytes(&w, s->bit_conf, s->nbits);
            if (s->agree)      wbytes(&w, s->agree, s->nbits);
            if (s->phase_q8)   wbytes(&w, s->phase_q8, s->nbits * 2u);
            if (s->flux_count) wbytes(&w, s->flux_count, s->nbits * 2u);
            blk_end(&w, f);
        }
        if (t->has_sectors) {
            blk_t f = blk_begin(&w, "SECT");
            w32(&w, t->sectors.gen);
            w32(&w, (uint32_t)t->sectors.count);
            for (size_t k = 0; k < t->sectors.count; ++k) {
                const uft_d2_sector_t *x = &t->sectors.items[k];
                w8(&w, x->id_cyl); w8(&w, x->id_head); w8(&w, x->id_sec);
                w8(&w, x->id_size_code);
                w8(&w, (uint8_t)((x->id_crc_ok ? 1u : 0u)
                               | (x->id_crc_known ? 2u : 0u)
                               | (x->data_crc_ok ? 4u : 0u)
                               | (x->data_crc_known ? 8u : 0u)
                               | (x->has_data ? 16u : 0u)));
                w8(&w, x->dam); w8(&w, (uint8_t)x->encoding);
                w8(&w, (uint8_t)x->origin); w8(&w, x->conf);
                w16(&w, x->deriv); w32(&w, x->source_gen);
                w64(&w, (uint64_t)x->idam_bit); w64(&w, (uint64_t)x->dam_bit);
                w64(&w, (uint64_t)x->data_end_bit);
                w32(&w, x->weak_bits); w32(&w, x->fuzzy_bits);
                w32(&w, x->data_len);
                if (x->data && x->data_len) wbytes(&w, x->data, x->data_len);
            }
            blk_end(&w, f);
        }
        blk_end(&w, b);
    }

    /* FSYS je Dateisystem */
    for (size_t i = 0; i < d->nfs; ++i) {
        const uft_d2_fs_t *f = &d->fs[i];
        blk_t b = blk_begin(&w, "FSYS");
        w8(&w, (uint8_t)f->kind); w8(&w, f->kind_conf);
        w16(&w, f->cyl_from); w16(&w, f->cyl_to); w8(&w, f->head_mask);
        w8(&w, (uint8_t)((f->counters_consistent ? 1u : 0u)
                       | (f->counters_checked ? 2u : 0u)));
        w16(&w, f->deriv); w32(&w, f->source_gen);
        wstr(&w, f->label, UFT_D2_FS_LABEL);
        w32(&w, (uint32_t)f->count);
        for (size_t k = 0; k < f->count; ++k) {
            const uft_d2_entry_t *e = &f->entries[k];
            wstr(&w, e->name, UFT_D2_ENTRY_NAME);
            w32(&w, e->size); w8(&w, e->type);
            w8(&w, (uint8_t)((e->deleted ? 1u : 0u) | (e->recoverable ? 2u : 0u)
                           | (e->chain_broken ? 4u : 0u)
                           | (e->cross_linked ? 8u : 0u)));
            w32(&w, e->start_unit); w8(&w, e->conf); w16(&w, e->deriv);
        }
        blk_end(&w, b);
    }

    /* DIAG */
    { blk_t b = blk_begin(&w, "DIAG");
      w32(&w, (uint32_t)d->ndiag);
      w32(&w, (uint32_t)d->diag_hidden); w32(&w, (uint32_t)d->diag_hidden_err);
      for (size_t i = 0; i < d->ndiag; ++i) {
          const uft_d2_diag_t *g = &d->diag[i];
          w8(&w, (uint8_t)g->sev); w8(&w, (uint8_t)g->layer);
          w16(&w, (uint16_t)g->cyl); w8(&w, (uint8_t)g->head);
          w16(&w, (uint16_t)g->sector);
          wstr(&w, g->code, UFT_D2_DIAG_CODE);
          wstr(&w, g->text, UFT_D2_DIAG_TEXT);
      }
      blk_end(&w, b); }

    if (w.oom) { free(w.p); return res(UFTD_E_NOMEM, "Speicher", 0u, NULL); }

    /* Gesamtlaenge in den Kopf, BEVOR die Gesamt-CRC gerechnet wird. Der
     * END-Block hat feste 16 Byte (Kennung 4, Laenge 4, Inhalt 4, CRC 4),
     * also ist die Laenge vorher bekannt. Der Entwurf rechnete die CRC
     * ueber einen Kopf mit Laenge 0 und trug die Laenge danach ein — jede
     * geladene Datei meldete daraufhin TOTAL_CRC. */
    const uint64_t L = (uint64_t)w.n + 16u;
    for (int k = 0; k < 8; ++k) w.p[8 + k] = (uint8_t)(L >> (8 * k));

    { const uint32_t total = uftd_crc32(w.p, w.n);
      blk_t b = blk_begin(&w, "END ");
      w32(&w, total);
      blk_end(&w, b); }
    if (w.oom) { free(w.p); return res(UFTD_E_NOMEM, "Speicher", 0u, NULL); }

    *out_buf = w.p; *out_len = w.n;
    return res(UFTD_OK, NULL, 0u, NULL);
}

/* ═══════════════════════ Lesepuffer ═════════════════════════════════════ */

typedef struct { const uint8_t *p; size_t n, pos; bool bad; } rbuf_t;

static bool rd_need(rbuf_t *r, size_t k) {
    if (r->bad || r->pos + k > r->n) { r->bad = true; return false; }
    return true;
}
static uint8_t r8(rbuf_t *r)   { return rd_need(r, 1u) ? r->p[r->pos++] : 0u; }
static uint16_t r16(rbuf_t *r) { uint16_t v = r8(r); return (uint16_t)(v | ((uint16_t)r8(r) << 8)); }
static uint32_t r32(rbuf_t *r) { uint32_t v = r16(r); return v | ((uint32_t)r16(r) << 16); }
static uint64_t r64(rbuf_t *r) { uint64_t v = r32(r); return v | ((uint64_t)r32(r) << 32); }

static const uint8_t *rbytes(rbuf_t *r, size_t k) {
    if (!rd_need(r, k)) return NULL;
    const uint8_t *q = r->p + r->pos; r->pos += k; return q;
}

static void rstr(rbuf_t *r, char *dst, size_t field) {
    const uint8_t *q = rbytes(r, field);
    if (q) { memcpy(dst, q, field); dst[field - 1u] = '\0'; }
    else dst[0] = '\0';
}

/** Einen Block einlesen und seine CRC pruefen. */
static bool blk_read(rbuf_t *r, char tag[5], rbuf_t *body, uftd_result_t *err) {
    const size_t at = r->pos;
    const uint8_t *t = rbytes(r, 4u);
    if (!t) { *err = res(UFTD_E_TRUNCATED, "Blockkopf fehlt", at, NULL); return false; }
    memcpy(tag, t, 4u); tag[4] = '\0';
    const uint32_t len = r32(r);
    const uint8_t *content = rbytes(r, len);
    if (!content) { *err = res(UFTD_E_TRUNCATED, "Blockinhalt fehlt", at, tag); return false; }
    const uint32_t crc_stored = r32(r);
    if (r->bad) { *err = res(UFTD_E_TRUNCATED, "Block-CRC fehlt", at, tag); return false; }
    if (uftd_crc32(content, len) != crc_stored) {
        *err = res(UFTD_E_CRC, "Blockpruefsumme falsch", at, tag);
        return false;
    }
    body->p = content; body->n = len; body->pos = 0u; body->bad = false;
    return true;
}

/* ═══════════════════════ Laden ══════════════════════════════════════════ */

#define FAIL_STRUCT(what) do { *err = res(UFTD_E_STRUCT, (what), off, tag); \
                               return false; } while (0)

static bool load_trak(uft_disk2_t *d, rbuf_t *b, size_t off, const char *tag,
                      uftd_result_t *err) {
    const uint16_t cyl = r16(b);
    const uint8_t head = r8(b);
    const uft_encoding_t enc = (uft_encoding_t)r8(b);
    const bool unf = r8(b) != 0u;
    const uint8_t has = r8(b);
    if (b->bad) FAIL_STRUCT("Spurkopf zu kurz");

    uft_d2_track_t *t = uft_d2_track(d, cyl, head);
    if (!t) FAIL_STRUCT("Spur konnte nicht angelegt werden");
    t->encoding = enc; t->unformatted = unf;

    while (b->pos < b->n) {
        char sub[5];
        rbuf_t sb;
        if (!blk_read(b, sub, &sb, err)) return false;

        if (strcmp(sub, "FLUX") == 0 && (has & 1u)) {
            const uft_d2_deriv_id_t dv = (uft_d2_deriv_id_t)r16(&sb);
            const uint32_t gen = r32(&sb);
            const uint32_t n = r32(&sb);
            for (uint32_t k = 0; k < n; ++k) {
                const uint32_t it = r32(&sb);
                const bool comp = r8(&sb) != 0u;
                const uft_d2_conf_t cf = r8(&sb);
                const uint32_t cnt = r32(&sb);
                const uint8_t *iv = rbytes(&sb, (size_t)cnt * 4u);
                if (sb.bad || !iv) FAIL_STRUCT("FLUX zu kurz");
                /* Die Umdrehung geht durch `add_revolution` und damit durch
                 * die Zuversichtsregel: eine gefaelschte Datei mit conf 255
                 * bei complete=false wird ABGEWIESEN, und der Fehlerbefund
                 * wird nie unterdrueckt. Laden ist nie stiller. */
                uint32_t *tmp = malloc((size_t)cnt * 4u + 4u);
                if (!tmp) { *err = res(UFTD_E_NOMEM, "Speicher", off, tag); return false; }
                if (cnt) memcpy(tmp, iv, (size_t)cnt * 4u);
                uft_d2_add_revolution(d, t, tmp, cnt, it, comp, cf, dv);
                free(tmp);
            }
            t->flux.gen = gen;                    /* Generation wiederher */
        } else if (strcmp(sub, "BITS") == 0 && (has & 2u)) {
            const uft_d2_deriv_id_t dv = (uft_d2_deriv_id_t)r16(&sb);
            const uint32_t gen = r32(&sb);
            const uint64_t nbits = r64(&sb);
            const uint64_t ixb = r64(&sb);
            const uft_encoding_t be = (uft_encoding_t)r8(&sb);
            const uint32_t cell = r32(&sb);
            const uint8_t nrf = r8(&sb);
            const uint8_t present = r8(&sb);
            if (sb.bad) FAIL_STRUCT("BITS-Kopf zu kurz");
            if (nbits > (1ull << 32)) FAIL_STRUCT("BITS: unplausible Laenge");
            const size_t nb = (size_t)nbits;
            const uint8_t *bits = rbytes(&sb, (nb + 7u) / 8u);
            const uint8_t *cf = (present & 1u) ? rbytes(&sb, nb) : NULL;
            const uint8_t *ag = (present & 2u) ? rbytes(&sb, nb) : NULL;
            const uint8_t *ph = (present & 4u) ? rbytes(&sb, nb * 2u) : NULL;
            const uint8_t *fc = (present & 8u) ? rbytes(&sb, nb * 2u) : NULL;
            if (sb.bad || !bits) FAIL_STRUCT("BITS zu kurz");
            /* phase und flux_count liegen in der Datei ungerade
             * ausgerichtet — kopieren statt casten. */
            int16_t *phv = NULL; uint16_t *fcv = NULL;
            if (ph) { phv = malloc(nb * 2u); if (phv) memcpy(phv, ph, nb * 2u); }
            if (fc) { fcv = malloc(nb * 2u); if (fcv) memcpy(fcv, fc, nb * 2u); }
            if ((ph && !phv) || (fc && !fcv)) {
                free(phv); free(fcv);
                *err = res(UFTD_E_NOMEM, "Speicher", off, tag);
                return false;
            }
            uft_d2_set_bitstream(d, t, bits, nb, cf, ag, nrf, phv, fcv,
                                 ixb == UINT64_MAX ? SIZE_MAX : (size_t)ixb,
                                 be, cell, dv);
            free(phv); free(fcv);
            t->bitstream.gen = gen;
        } else if (strcmp(sub, "SECT") == 0 && (has & 4u)) {
            const uint32_t gen = r32(&sb);
            const uint32_t n = r32(&sb);
            for (uint32_t k = 0; k < n; ++k) {
                uft_d2_sector_t x;
                memset(&x, 0, sizeof(x));
                x.id_cyl = r8(&sb); x.id_head = r8(&sb); x.id_sec = r8(&sb);
                x.id_size_code = r8(&sb);
                const uint8_t fl = r8(&sb);
                x.id_crc_ok      = (fl & 1u)  != 0u;
                x.id_crc_known   = (fl & 2u)  != 0u;
                x.data_crc_ok    = (fl & 4u)  != 0u;
                x.data_crc_known = (fl & 8u)  != 0u;
                x.has_data       = (fl & 16u) != 0u;
                x.dam = r8(&sb); x.encoding = (uft_encoding_t)r8(&sb);
                x.origin = (uft_d2_origin_t)r8(&sb);
                x.conf = r8(&sb);
                x.deriv = (uft_d2_deriv_id_t)r16(&sb);
                x.source_gen = r32(&sb);
                const uint64_t a = r64(&sb), bb = r64(&sb), c = r64(&sb);
                x.idam_bit     = a  == UINT64_MAX ? SIZE_MAX : (size_t)a;
                x.dam_bit      = bb == UINT64_MAX ? SIZE_MAX : (size_t)bb;
                x.data_end_bit = c  == UINT64_MAX ? SIZE_MAX : (size_t)c;
                x.weak_bits = r32(&sb); x.fuzzy_bits = r32(&sb);
                x.data_len = r32(&sb);
                const uint8_t *dat = x.data_len ? rbytes(&sb, x.data_len) : NULL;
                if (sb.bad) FAIL_STRUCT("SECT zu kurz");
                x.data = (uint8_t *)dat;
                /* prueft die Zuversichtsregel erneut */
                uft_d2_add_sector(d, t, &x);
            }
            t->sectors.gen = gen;
        } else {
            uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_FLUX, cyl, head, -1,
                        "UNKNOWN_BLOCK",
                        "Unbekannter Spurblock \"%s\" uebersprungen (%zu Byte).",
                        sub, sb.n);
        }
    }
    return true;
}

static bool load_fsys(uft_disk2_t *d, rbuf_t *b, size_t off, const char *tag,
                      uftd_result_t *err) {
    const uft_d2_fs_kind_t kind = (uft_d2_fs_kind_t)r8(b);
    const uft_d2_conf_t kc = r8(b);
    const uint16_t cf = r16(b), ct = r16(b);
    const uint8_t hm = r8(b);
    const uint8_t flags = r8(b);
    const uft_d2_deriv_id_t dv = (uft_d2_deriv_id_t)r16(b);
    const uint32_t sg = r32(b);
    char label[UFT_D2_FS_LABEL];
    rstr(b, label, UFT_D2_FS_LABEL);
    const uint32_t n = r32(b);
    if (b->bad) FAIL_STRUCT("FSYS-Kopf zu kurz");

    uft_d2_fs_t *f = uft_d2_add_fs(d, kind, kc, dv);
    if (!f) { *err = res(UFTD_E_NOMEM, "Speicher", off, tag); return false; }
    f->cyl_from = cf; f->cyl_to = ct; f->head_mask = hm;
    f->counters_consistent = (flags & 1u) != 0u;
    f->counters_checked    = (flags & 2u) != 0u;
    f->source_gen = sg;
    memcpy(f->label, label, sizeof(f->label));

    for (uint32_t k = 0; k < n; ++k) {
        uft_d2_entry_t e;
        memset(&e, 0, sizeof(e));
        rstr(b, e.name, UFT_D2_ENTRY_NAME);
        e.size = r32(b); e.type = r8(b);
        const uint8_t fl = r8(b);
        e.deleted      = (fl & 1u) != 0u;
        e.recoverable  = (fl & 2u) != 0u;
        e.chain_broken = (fl & 4u) != 0u;
        e.cross_linked = (fl & 8u) != 0u;
        e.start_unit = r32(b); e.conf = r8(b);
        e.deriv = (uft_d2_deriv_id_t)r16(b);
        if (b->bad) FAIL_STRUCT("FSYS-Eintrag zu kurz");
        uft_d2_add_entry(d, f, &e);
    }
    return true;
}

/** Befunde DIREKT einsetzen — sie sind die gespeicherten, keine neuen. */
static void load_diag(uft_disk2_t *d, rbuf_t *b) {
    const uint32_t n = r32(b);
    const uint32_t hid = r32(b), hide = r32(b);
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t sev = r8(b), lay = r8(b);
        const int16_t cy = (int16_t)r16(b);
        const int8_t hd = (int8_t)r8(b);
        const int16_t se = (int16_t)r16(b);
        char code[UFT_D2_DIAG_CODE], text[UFT_D2_DIAG_TEXT];
        rstr(b, code, UFT_D2_DIAG_CODE);
        rstr(b, text, UFT_D2_DIAG_TEXT);
        if (b->bad) break;
        if (d->ndiag >= UFT_D2_MAX_DIAG) { d->diag_hidden++; continue; }
        uft_d2_diag_t *g = &d->diag[d->ndiag++];
        memset(g, 0, sizeof(*g));
        g->sev = (uft_d2_diag_sev_t)sev;
        g->layer = (uft_d2_layer_t)lay;
        g->cyl = cy; g->head = hd; g->sector = se;
        memcpy(g->code, code, UFT_D2_DIAG_CODE);
        memcpy(g->text, text, UFT_D2_DIAG_TEXT);
    }
    d->diag_hidden += hid;
    d->diag_hidden_err += hide;
}

uftd_result_t uftd_load(const uint8_t *buf, size_t len, uft_disk2_t **out_disk) {
    if (!buf || !out_disk) return res(UFTD_E_ARG, "NULL", 0u, NULL);
    *out_disk = NULL;
    rbuf_t r; r.p = buf; r.n = len; r.pos = 0u; r.bad = false;

    const uint8_t *m = rbytes(&r, 4u);
    if (!m || memcmp(m, UFTD_MAGIC, 4u) != 0)
        return res(UFTD_E_MAGIC, "keine UFTD-Kennung", 0u, NULL);
    const uint16_t ver = r16(&r);
    (void)r16(&r);                               /* Flags, noch ungenutzt */
    const uint64_t total = r64(&r);
    if (r.bad) return res(UFTD_E_TRUNCATED, "Kopf zu kurz", 0u, NULL);
    /* GROB, und das steht im Kopf: eine hoehere Fassung wird abgewiesen.
     * Die Bedingung des Entwurfs konnte fuer keine Fassung unter 256
     * zuschlagen — eine Datei der Fassung 2 waere klaglos geoeffnet
     * worden. */
    if (ver > UFTD_VERSION)
        return res(UFTD_E_VERSION, "Fassung zu neu", 4u, NULL);

    uft_disk2_t *d = uft_d2_create();
    if (!d) return res(UFTD_E_NOMEM, "Speicher", 0u, NULL);
    *out_disk = d;                    /* ab hier auch bei Fehlern gesetzt */
    /* Die Einspeise-Befunde stehen im DIAG-Block der Datei. Ohne dieses
     * Flag erzeugt jedes `add_*` sie ein zweites Mal, und der Block
     * waechst bei jeder Sicherung. FEHLER gehen trotzdem durch. */
    d->suppress_ingest_diag = true;

    if (total != (uint64_t)len)
        uft_d2_diag(d, UFT_D2_DIAG_WARN, UFT_D2_LAYER_FLUX, -1, -1, -1,
                    "LEN_MISMATCH",
                    "Der Kopf nennt %llu Byte, der Puffer hat %zu.",
                    (unsigned long long)total, len);

    uftd_result_t err = res(UFTD_OK, NULL, 0u, NULL);
    bool seen_end = false;

    while (r.pos < r.n && !seen_end) {
        const size_t off = r.pos;
        char tag[5];
        rbuf_t b;
        if (!blk_read(&r, tag, &b, &err)) {
            d->suppress_ingest_diag = false;
            uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, -1, -1, -1,
                        "LOAD_ABORT",
                        "Laden abgebrochen bei Versatz %zu (%s): %s",
                        err.offset, err.block[0] ? err.block : "?",
                        err.what ? err.what : "?");
            return err;
        }

        if (strcmp(tag, "DERV") == 0) {
            const uint32_t n = r32(&b);
            for (uint32_t i = 0; i < n; ++i) {
                const uft_d2_layer_t fl = (uft_d2_layer_t)r8(&b);
                const uft_d2_origin_t og = (uft_d2_origin_t)r8(&b);
                const uint32_t sg = r32(&b);
                char by[UFT_D2_DERIV_BY], pa[UFT_D2_DERIV_PARAMS];
                rstr(&b, by, UFT_D2_DERIV_BY);
                rstr(&b, pa, UFT_D2_DERIV_PARAMS);
                if (b.bad) break;
                uft_d2_register_deriv(d, fl, og, by, pa, sg);
            }
        } else if (strcmp(tag, "META") == 0) {
            const uint32_t n = r32(&b);
            const uint32_t hidden = r32(&b);
            for (uint32_t i = 0; i < n; ++i) {
                const uft_d2_meta_src_t s = (uft_d2_meta_src_t)r8(&b);
                char k[UFT_D2_META_KEY], v[UFT_D2_META_VALUE];
                rstr(&b, k, UFT_D2_META_KEY);
                rstr(&b, v, UFT_D2_META_VALUE);
                if (b.bad) break;
                uft_d2_add_meta(d, k, v, s);
            }
            d->meta_hidden += hidden;
        } else if (strcmp(tag, "TRAK") == 0) {
            if (!load_trak(d, &b, off, tag, &err)) {
                d->suppress_ingest_diag = false;
                uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, -1, -1,
                            -1, "LOAD_ABORT", "Spurblock bei %zu: %s", off,
                            err.what ? err.what : "?");
                return err;
            }
        } else if (strcmp(tag, "FSYS") == 0) {
            if (!load_fsys(d, &b, off, tag, &err)) {
                d->suppress_ingest_diag = false;
                uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FILESYSTEM, -1,
                            -1, -1, "LOAD_ABORT",
                            "Dateisystemblock bei %zu: %s", off,
                            err.what ? err.what : "?");
                return err;
            }
        } else if (strcmp(tag, "DIAG") == 0) {
            load_diag(d, &b);
        } else if (strcmp(tag, "END ") == 0) {
            const uint32_t stored = r32(&b);
            const uint32_t computed = uftd_crc32(buf, off);
            if (stored != computed)
                uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, -1, -1,
                            -1, "TOTAL_CRC",
                            "Gesamtpruefsumme falsch: gespeichert %08X, "
                            "gerechnet %08X.",
                            (unsigned)stored, (unsigned)computed);
            seen_end = true;
        } else {
            uft_d2_diag(d, UFT_D2_DIAG_NOTE, UFT_D2_LAYER_FLUX, -1, -1, -1,
                        "UNKNOWN_BLOCK",
                        "Unbekannter Block \"%s\" uebersprungen (%zu Byte).",
                        tag, b.n);
        }
    }

    d->suppress_ingest_diag = false;
    if (!seen_end) {
        uft_d2_diag(d, UFT_D2_DIAG_ERROR, UFT_D2_LAYER_FLUX, -1, -1, -1,
                    "NO_END",
                    "Die Datei endet ohne END-Block — moeglicherweise "
                    "abgeschnitten.");
        return res(UFTD_E_TRUNCATED, "kein END-Block", r.pos, NULL);
    }
    /* Der Merkmalcache wird NICHT geladen — er ist beim ersten Zugriff
     * schmutzig und wird aus den DATEN gerechnet. */
    d->feat_dirty = true;
    return res(UFTD_OK, NULL, 0u, NULL);
}

/* ═══════════════════════ Dateien ════════════════════════════════════════ */

uftd_result_t uftd_save_file(const uft_disk2_t *d, const char *path) {
    if (!path) return res(UFTD_E_ARG, "kein Pfad", 0u, NULL);
    uint8_t *buf = NULL; size_t n = 0u;
    uftd_result_t r = uftd_save(d, &buf, &n);
    if (r.code != UFTD_OK) return r;
    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return res(UFTD_E_ARG, "Datei nicht schreibbar", 0u, NULL); }
    const bool ok = fwrite(buf, 1u, n, f) == n;
    const bool zu = fclose(f) == 0;
    free(buf);
    /* Ein `fclose`, das scheitert, heisst: die Bytes sind NICHT auf der
     * Platte. Das als Erfolg zu melden waere eine Schreibzusage ohne Tat
     * (MF-883). */
    return (ok && zu) ? r : res(UFTD_E_ARG, "Schreiben unvollstaendig", n, NULL);
}

uftd_result_t uftd_load_file(const char *path, uft_disk2_t **out_disk) {
    if (!path || !out_disk) return res(UFTD_E_ARG, "NULL", 0u, NULL);
    *out_disk = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return res(UFTD_E_ARG, "Datei nicht lesbar", 0u, NULL);
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f); return res(UFTD_E_ARG, "nicht positionierbar", 0u, NULL);
    }
    const long L = ftell(f);
    if (L < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f); return res(UFTD_E_ARG, "Groesse unbekannt", 0u, NULL);
    }
    uint8_t *buf = malloc((size_t)L + 1u);
    if (!buf) { fclose(f); return res(UFTD_E_NOMEM, "Speicher", 0u, NULL); }
    const size_t n = fread(buf, 1u, (size_t)L, f);
    fclose(f);
    uftd_result_t r = uftd_load(buf, n, out_disk);
    free(buf);
    return r;
}

const char *uftd_err_name(uftd_err_t e) {
    switch (e) {
    case UFTD_OK:          return "ok";
    case UFTD_E_ARG:       return "Argument";
    case UFTD_E_NOMEM:     return "Speicher";
    case UFTD_E_MAGIC:     return "keine UFTD-Datei";
    case UFTD_E_VERSION:   return "Fassung zu neu";
    case UFTD_E_TRUNCATED: return "abgeschnitten";
    case UFTD_E_CRC:       return "Pruefsumme";
    case UFTD_E_STRUCT:    return "Struktur";
    default:               return "?";
    }
}
