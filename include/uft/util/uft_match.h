/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_match.h
 * @brief Vergleichen an der richtigen Stelle — Ersatz für suchendes Prüfen.
 *
 * Der Prüfer `tools/substring_audit.py` verweist auf diese Funktionen. Jede
 * davon löst genau eine Falle:
 *
 *   C1  Endung gesucht statt am Ende geprüft  ->  uft_suffix_eq()
 *   C2  Magic im Puffer gesucht               ->  uft_magic_at()
 *   C3  Präfixvergleich statt Gleichheit      ->  uft_id_eq()
 *   C4  halber Literalvergleich               ->  UFT_LIT_EQ()
 *   C5  Vergleich über das Literal hinaus     ->  UFT_LIT_EQ()
 *   C6  sizeof auf einem Zeiger als Länge     ->  UFT_LIT_EQ()
 *
 * DIE REGEL DAHINTER
 * ------------------
 * Ein Muster, das nicht weiß, WO es steht, darf nicht entscheiden.
 *
 *   Endung   prüft man am Ende
 *   Magic    prüft man an dem Versatz, den die Formatbeschreibung nennt
 *   Kennung  prüft man auf Gleichheit
 *
 * Alles andere ist Suchen, und Suchen findet auch dort, wo nichts ist.
 */

#ifndef UFT_MATCH_H
#define UFT_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───────────────────────── Endungen ────────────────────────────────────── */

/**
 * Endet @p path auf @p suffix? Vergleich ohne Rücksicht auf Groß- und
 * Kleinschreibung, nur ASCII.
 *
 * Das ist der Ersatz für `strstr(path, ".st")`. Der Unterschied ist nicht
 * akademisch: `.st` steckt in jedem Pfad, der irgendwo "st" enthält —
 * `C:\Bestand\x.scp` zum Beispiel.
 */
bool uft_suffix_eq(const char *path, const char *suffix);

/**
 * Wie uft_suffix_eq(), aber gegen eine Liste.
 *
 * @param suffixes  NULL-beendete Liste
 * @param out_index optional: welcher getroffen hat
 *
 * Die Liste wird von VORN nach hinten geprüft, also muss die längste
 * Endung zuerst stehen: sonst gewinnt `.st` gegen `.stx`, und genau das
 * ist die Falle noch einmal, nur eine Ebene höher. uft_suffix_list_ok()
 * prüft die Reihenfolge.
 */
bool uft_suffix_eq_any(const char *path, const char *const *suffixes,
                       size_t *out_index);

/**
 * Prüft, ob eine Endungsliste sicher sortiert ist: kein Eintrag ist Suffix
 * eines späteren.
 *
 * Gehört in einen Test, nicht in den Laufzeitpfad — eine falsch sortierte
 * Liste ist ein Programmfehler, kein Eingabefehler.
 *
 * @param out_bad optional: Index des ersten Eintrags, der zu früh steht
 */
bool uft_suffix_list_ok(const char *const *suffixes, size_t *out_bad);

/** Nur die Endung, ohne Punkt. NULL, wenn keine da ist. */
const char *uft_path_ext(const char *path);

/* ───────────────────────── Kennungen im Puffer ─────────────────────────── */

/**
 * Steht @p magic genau bei @p offset in @p data?
 *
 * Ersatz für `strstr(buf, "SINCLAIR")` und `memmem(...)`. Der Versatz ist
 * ein Pflichtargument, weil er in jeder Formatbeschreibung steht — wer ihn
 * nicht kennt, sucht, und Suchen ist genau das Problem.
 *
 * @return false auch, wenn der Puffer zu kurz ist. Kein Übergriff.
 */
bool uft_magic_at(const uint8_t *data, size_t size, size_t offset,
                  const void *magic, size_t magic_len);

/** Wie uft_magic_at() mit einem Zeichenkettenliteral, Länge automatisch. */
#define UFT_MAGIC_AT(data, size, offset, lit) \
    uft_magic_at((data), (size), (offset), (lit), sizeof(lit) - 1u)

/**
 * Sucht eine Kennung im Puffer — und sagt, dass das eine schwächere
 * Aussage ist.
 *
 * Es gibt Formate, bei denen der Versatz nicht festgelegt ist (ein
 * angehängter Footer, ein Bereich variabler Länge davor). Für die ist
 * diese Funktion da, und sie verlangt eine Begründung:
 *
 * @param why  Grund, warum hier gesucht werden MUSS. Wird nicht geprüft,
 *             steht aber in der Signatur, damit er beim Schreiben im Kopf
 *             ist und beim Lesen dasteht. NULL ist erlaubt und heißt
 *             "keine Begründung" — dann gehört hier uft_magic_at() hin.
 * @param out_offset optional: wo gefunden
 *
 * @return true bei Fund. Der Aufrufer MUSS das Ergebnis schwächer werten
 *         als einen Treffer an fester Stelle.
 */
bool uft_magic_search(const uint8_t *data, size_t size,
                      const void *magic, size_t magic_len,
                      const char *why, size_t *out_offset);

/* ───────────────────────── Format-IDs ──────────────────────────────────── */

/**
 * Sind zwei Format-IDs gleich? Ohne Rücksicht auf Groß- und
 * Kleinschreibung, aber GANZ.
 *
 * Ersatz für `strncmp(id, name, strlen(name))`. Bei 138 Format-IDs mit
 * gemeinsamen Präfixen — d64/d67/d71/d80/d81/d82, scp/scl/scr, st/stx,
 * fdi/fd — ist Präfixvergleich kein Randfall.
 */
bool uft_id_eq(const char *a, const char *b);

/**
 * Sucht eine ID in einer Tabelle. Exakter Vergleich, kein Präfix.
 *
 * @return Index oder SIZE_MAX.
 */
size_t uft_id_find(const char *id, const char *const *table, size_t count);

/**
 * Findet Paare in einer ID-Tabelle, bei denen eines Präfix des anderen
 * ist.
 *
 * Für einen Test: solche Paare sind nicht verboten, aber jede Stelle, die
 * sie mit einem Präfixvergleich prüft, ist falsch. Wer die Liste kennt,
 * kann gezielt nachsehen.
 *
 * @param on_pair Rückruf je Paar (kürzeres zuerst)
 * @return Zahl der Paare
 */
size_t uft_id_prefix_pairs(const char *const *table, size_t count,
                           void (*on_pair)(const char *shorter,
                                           const char *longer, void *ctx),
                           void *ctx);

/* ───────────────────────── Literalvergleich ────────────────────────────── */

/**
 * Vergleicht @p data ab @p offset mit einem Literal, Länge automatisch.
 *
 * Ersatz für alle drei Längenfehler auf einmal: die Länge kommt aus
 * `sizeof(lit) - 1`, kann also nicht zu kurz, nicht zu lang und nicht
 * `sizeof(ptr)` sein.
 */
#define UFT_LIT_EQ(data, size, offset, lit) \
    uft_magic_at((const uint8_t *)(data), (size), (offset), \
                 (lit), sizeof(lit) - 1u)

/**
 * Vergleicht zwei Puffer und sagt, ab welchem Byte sie abweichen.
 *
 * @param out_first_diff optional: erster abweichender Versatz
 * @return true bei Gleichheit
 *
 * Nützlich, wo ein Fehlschlag begründet werden soll: "Kennung weicht ab
 * Byte 3 ab" ist eine Aussage, "Kennung falsch" ist keine.
 */
bool uft_bytes_eq(const void *a, const void *b, size_t len,
                  size_t *out_first_diff);

/**
 * Steht @p wort als GANZES WORT in @p text?
 *
 * MF-1237: hierher gezogen aus `src/formats/scp/uft_scp_writer.c`, wo
 * es seit MF-1233 als dateilokales `hint_hat_wort()` lag. Der zweite
 * Bedarf war gemessen und nicht vorhergesehen:
 * `src/detect/mfm/mfm_detect.c:593` sucht `"TOS"` im OEM-Feld eines
 * BPB, und das trifft in `TOSHIBA` — bei einer Entscheidung, die
 * `mfm_detect.c:607` mit `return true` faellt. Eine zweite Kopie
 * derselben Regel waere „eine Groesse, zwei Rechnungen" (MF-1177,
 * fuenfmal in diesem Baum bezahlt), also steht sie jetzt an EINER
 * Stelle — hier, wo die uebrigen Musterregeln wohnen.
 *
 * Wortzeichen sind `[0-9A-Za-z.]`. **Der Punkt gehoert zum Wort**,
 * sonst zerfiele „1.44" in „1" und „44"; ein fuehrender oder
 * anhaengender Punkt wird dagegen abgestreift, damit „pc hd." das
 * Kennwort „hd" nicht verliert. Die Pruefung ist von Hand auf ASCII
 * gestellt statt `isalnum()` zu rufen, das bei einem Byte >= 0x80 in
 * `char` undefiniert ist — dieselbe Vorsicht wie `lc()` in
 * `uft_match.c`.
 *
 * **Gross- und Kleinschreibung unterscheidet sie**, wie `strstr` es
 * tat. Wer beide Schreibweisen will, fragt zweimal — das ist eine
 * Entscheidung des Aufrufers und keine des Vergleichers.
 *
 * @param text  nullterminiert; NULL ergibt false
 * @param wort  nullterminiert; NULL oder leer ergibt false
 * @return true, wenn @p wort als ganzes Wort vorkommt
 */
bool uft_wort_treffer(const char *text, const char *wort);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MATCH_H */
