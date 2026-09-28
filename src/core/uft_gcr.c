/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_gcr.c
 * @brief Umsetzung des GCR-Codec-Registers (MF-1509).
 *
 * Regeln, Belege und die Begruendung, warum genau EINE Tafel bleibt,
 * stehen im Kopf von `include/uft/core/uft_gcr.h`. Hier steht nur, wie
 * es gerechnet wird.
 */
#include "uft/core/uft_gcr.h"

/* Die Commodore-Zuordnung wird NICHT hier wiederholt, sondern von dort
 * gelesen, wo sie schon steht (MF-1509). Das Tor `audit_cbm_zonen`s
 * Schwester `audit_konstantenfamilien.py` hat den ersten Entwurf dieses
 * Registers abgewiesen: eine eigene 16-Byte-Tafel hier waere die
 * **13.** Kopie gewesen — ein Register, das die Kopien zusammenfuehren
 * soll und dabei eine neue anlegt.
 *
 * Solange die uebrigen Aufrufer nicht umgehaengt sind, ist der Header die
 * eine Stelle; mit dem letzten Umhaengen (Schritt 3 von P3-666) wandert
 * sie hierher, und dann verschwinden die anderen. Das ist die Richtung,
 * in die das Manifest schrumpfen soll — nicht eine Kopie mehr auf dem
 * Weg dorthin. */
#include "uft/uft_cbm_gcr.h"

#include <string.h>

/* ── Bausteine der Praedikate ────────────────────────────────────────── */

/** @return Zahl der Paare benachbarter Nullen in den unteren @p bits Bit. */
static int nullenpaare(uint32_t w, int bits) {
    int n = 0;
    for (int i = 0; i + 1 < bits; i++) {
        if (!((w >> (bits - 1 - i)) & 1u) && !((w >> (bits - 2 - i)) & 1u))
            n++;
    }
    return n;
}

/** @return true, wenn irgendwo zwei benachbarte Einsen stehen. */
static bool einserpaar(uint32_t w, int bits) {
    for (int i = 0; i + 1 < bits; i++) {
        if (((w >> (bits - 1 - i)) & 1u) && ((w >> (bits - 2 - i)) & 1u))
            return true;
    }
    return false;
}

/** @return true, wenn drei Nullen in Folge vorkommen. */
static bool drei_nullen(uint32_t w, int bits) {
    for (int i = 0; i + 2 < bits; i++) {
        if (!((w >> (bits - 1 - i)) & 1u) && !((w >> (bits - 2 - i)) & 1u)
            && !((w >> (bits - 3 - i)) & 1u))
            return true;
    }
    return false;
}

/* ── Die Praedikate ──────────────────────────────────────────────────── */

bool uft_gcr_wort_gueltig(uft_gcr_codec_t codec, uint32_t wort) {
    switch (codec) {
    case UFT_GCR_CBM_5_4:
        if (wort > 0x1Fu) return false;
        if (((wort >> 3) & 0x03u) == 0) return false;  /* zwei fuehrende 0 */
        if ((wort & 0x03u) == 0) return false;         /* zwei abschliessend */
        if (drei_nullen(wort, 5)) return false;
        /* Ein Wort aus lauter Einsen koennte mit dem naechsten zusammen
         * eine SYNC-Marke bilden. Ohne diese Zeile sind es 17 statt 16. */
        if (wort == 0x1Fu) return false;
        return true;

    case UFT_GCR_APPLE_6_2:
        if (wort > 0xFFu) return false;
        if (!(wort & 0x80u)) return false;
        if (nullenpaare(wort, 8) > 1) return false;    /* hoechstens EIN Paar */
        return einserpaar(wort & 0x7Fu, 7);            /* OHNE Bit 7 */

    case UFT_GCR_APPLE_5_3:
        if (wort > 0xFFu) return false;
        if (!(wort & 0x80u)) return false;
        if (nullenpaare(wort, 8) > 0) return false;    /* KEIN Paar */
        return einserpaar(wort & 0x7Fu, 7);

    default:
        return false;
    }
}

/* ── Die eine Tafel: Commodores Zuordnung 0..F ───────────────────────── */

/* Sie steht in `include/uft/uft_cbm_gcr.h` als `cbm_gcr_encode_table` und
 * wird von dort gelesen — siehe die Begruendung beim Include oben. Die
 * MENGE ihrer Werte folgt dem Praedikat (geprueft in
 * `tests/test_gcr_praedikat_trifft_die_tafel.c`); die REIHENFOLGE ist
 * Commodores Wahl und deshalb ueberhaupt eine Tafel. Bei Apple ist auch
 * die Zuordnung Regel. */
#define CBM_ZUORDNUNG cbm_gcr_encode_table

/* ── Auskunft ────────────────────────────────────────────────────────── */

static const uft_gcr_info_t INFOS[UFT_GCR_ANZAHL] = {
    {
        "Commodore 5/4", "Commodore", 4, 5, 16,
        "keine zwei fuehrenden Nullen; keine zwei abschliessenden; "
        "nie drei Nullen in Folge; nicht 11111 (sonst kann ein Datenwort "
        "mit dem naechsten eine Sync-Marke bilden)",
        "gemessen MF-1507/1509 gegen cbm_gcr_encode_table in "
        "include/uft/uft_cbm_gcr.h: Menge identisch, Reihenfolge nicht",
        false
    },
    {
        "Apple 6&2", "Apple", 6, 8, 64,
        "Bit 7 gesetzt; hoechstens ein Paar benachbarter Nullen; "
        "mindestens ein Einserpaar in Bit 6..0",
        "gemessen MF-1507/1509 gegen A2_WRITE_TAB in "
        "src/formats/apple/uft_apple_gcr.c: Menge UND Reihenfolge identisch",
        true
    },
    {
        "Apple 5&3", "Apple", 5, 8, 32,
        "Bit 7 gesetzt; KEIN Paar benachbarter Nullen; "
        "mindestens ein Einserpaar in Bit 6..0 (echte Teilmenge von 6&2)",
        "gemessen MF-1509 gegen A2_TAB5 in "
        "src/formats/apple/uft_apple_gcr.c: Menge UND Reihenfolge identisch",
        true
    },
};

const uft_gcr_info_t *uft_gcr_info(uft_gcr_codec_t codec) {
    if (codec < 0 || codec >= UFT_GCR_ANZAHL) return NULL;
    return &INFOS[codec];
}

/* ── Aufzaehlen statt Speichern ──────────────────────────────────────── */

size_t uft_gcr_wortmenge(uft_gcr_codec_t codec, uint8_t *out, size_t max) {
    const uft_gcr_info_t *info = uft_gcr_info(codec);
    if (!info || !out || max < info->woerter) return 0;
    const uint32_t obergrenze = (codec == UFT_GCR_CBM_5_4) ? 32u : 256u;
    size_t n = 0;
    for (uint32_t w = 0; w < obergrenze; w++) {
        if (uft_gcr_wort_gueltig(codec, w)) {
            if (n >= max) return 0;
            out[n++] = (uint8_t)w;
        }
    }
    return n;
}

uint8_t uft_gcr_kodieren(uft_gcr_codec_t codec, uint8_t index) {
    const uft_gcr_info_t *info = uft_gcr_info(codec);
    if (!info || index >= info->woerter) return 0xFFu;
    if (!info->zuordnung_ist_regel)
        return CBM_ZUORDNUNG[index & 0x0Fu];
    /* Apple: das index-te gueltige Byte, aufsteigend. */
    uint8_t n = 0;
    for (uint32_t w = 0; w < 256u; w++) {
        if (uft_gcr_wort_gueltig(codec, w)) {
            if (n == index) return (uint8_t)w;
            n++;
        }
    }
    return 0xFFu;
}

/* ── Die Gegenrichtung ───────────────────────────────────────────────── */

/* Erzeugt, nicht gespeichert: beim ersten Aufruf je Codec aus dem
 * Praedikat aufgezaehlt. Das ist der Unterschied zu den zehn
 * Dekodiertafeln, die heute im Baum stehen (gemessen MF-1506) — hier
 * steht keine Zahlenfolge im Quelltext, nur die Ableitung.
 *
 * Warum ueberhaupt gehalten: ein Dekoder laeuft je BYTE. Die Position im
 * Praedikat jedes Mal neu zu suchen waere ueber 256 Werte hundertfacher
 * Aufwand gegenueber einem Tafelzugriff — und dieser Pfad liegt in der
 * Dekodierung einer ganzen Spur. */
static uint8_t g_rueck[UFT_GCR_ANZAHL][256];
static bool g_rueck_bereit[UFT_GCR_ANZAHL];

static void rueck_aufbauen(uft_gcr_codec_t codec) {
    memset(g_rueck[codec], UFT_GCR_UNGUELTIG, sizeof(g_rueck[codec]));
    /* Gewonnen aus `uft_gcr_kodieren()`, nicht aus dem Praedikat direkt:
     * bei Commodore traegt die TAFEL die Zuordnung, bei Apple die
     * Reihenfolge — jene Funktion kennt beide Faelle, hier wird der
     * Unterschied also nicht ein zweites Mal geschrieben. */
    const uft_gcr_info_t *info = &INFOS[codec];
    for (uint16_t i = 0; i < info->woerter; i++) {
        const uint8_t wort = uft_gcr_kodieren(codec, (uint8_t)i);
        g_rueck[codec][wort] = (uint8_t)i;
    }
    g_rueck_bereit[codec] = true;
}

uint8_t uft_gcr_dekodieren(uft_gcr_codec_t codec, uint32_t wort) {
    if (codec < 0 || codec >= UFT_GCR_ANZAHL || wort > 0xFFu)
        return UFT_GCR_UNGUELTIG;
    if (!g_rueck_bereit[codec]) rueck_aufbauen(codec);
    return g_rueck[codec][wort];
}

/* ── Erkennung ───────────────────────────────────────────────────────── */

/** @return @p breite Bits ab @p pos, MSB zuerst. */
static uint32_t bits_lesen(const uint8_t *bits, size_t pos, int breite) {
    uint32_t w = 0;
    for (int i = 0; i < breite; i++) {
        const size_t p = pos + (size_t)i;
        w = (w << 1) | ((bits[p >> 3] >> (7 - (p & 7))) & 1u);
    }
    return w;
}

float uft_gcr_anteil_gueltig(uft_gcr_codec_t codec, const uint8_t *bits,
                             size_t nbits, uft_gcr_erkennung_t *out) {
    const uft_gcr_info_t *info = uft_gcr_info(codec);
    if (!info || !bits) return -1.0f;
    const int breite = (int)info->plattenbits;
    if (nbits < (size_t)breite) return -1.0f;

    uint32_t gesamt = 0, gut = 0;
    for (size_t p = 0; p + (size_t)breite <= nbits; p += (size_t)breite) {
        gesamt++;
        if (uft_gcr_wort_gueltig(codec, bits_lesen(bits, p, breite))) gut++;
    }
    if (gesamt == 0) return -1.0f;

    const float anteil = (float)gut / (float)gesamt;
    if (out) {
        out->codec = codec;
        out->woerter = gesamt;
        out->gueltig = gut;
        out->anteil = anteil;
    }
    return anteil;
}

size_t uft_gcr_erkennen(const uint8_t *bits, size_t nbits,
                        uft_gcr_erkennung_t out[UFT_GCR_ANZAHL]) {
    if (!bits || !out) return 0;
    size_t n = 0;
    for (int c = 0; c < UFT_GCR_ANZAHL; c++) {
        uft_gcr_erkennung_t e;
        memset(&e, 0, sizeof(e));
        if (uft_gcr_anteil_gueltig((uft_gcr_codec_t)c, bits, nbits, &e) >= 0.0f)
            out[n++] = e;
    }
    /* Bester zuerst — Einfuegesortierung ueber hoechstens drei Eintraege. */
    for (size_t i = 1; i < n; i++) {
        uft_gcr_erkennung_t t = out[i];
        size_t j = i;
        while (j > 0 && out[j - 1].anteil < t.anteil) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = t;
    }
    return n;
}
