/**
 * @file uft_flux_sync_search.h
 * @brief Sync-Marken im Flussstrom finden, ohne die PLL (MF-492).
 *
 * Baustein 2.1 des Mammut-Plans, in der Fassung, die ohne fremde Quelle
 * auskommt.
 *
 * ── Das Problem ──────────────────────────────────────────────────────────
 *
 * Die uebliche Kette ist Flux -> PLL -> Bitstrom -> Sync-Suche. Verliert die
 * PLL den Takt, gibt es keinen Bitstrom, und damit findet auch niemand mehr
 * einen Sync. Gemessen (FLUX-11, MF-487): bei einer angenommenen Zellendauer
 * von 0,85 der wahren und 4 % Zittern kommen **0 von 11** Sektoren zurueck.
 * Nicht einer wird auch nur gefunden — obwohl alle elf Marken unveraendert
 * im Strom stehen.
 *
 * ── Der Ausweg ───────────────────────────────────────────────────────────
 *
 * Eine Sync-Marke ist im Flussstrom auch ohne Takt erkennbar. Die
 * Amiga-Marke 0x4489 0x4489 hat die Zellabstaende
 *
 *     4  3  4  3  2  4  3  4  3
 *
 * (gemessen am erzeugten Zellenstrom, nicht abgeleitet). Gesucht wird
 * deshalb nicht nach Zeiten, sondern nach einer Stelle, an der neun
 * aufeinanderfolgende Abstaende zu EINEM gemeinsamen Takt passen:
 *
 *     d_i ~ k_i * T
 *
 * T ist dabei unbekannt und wird aus den Daten geschaetzt. Damit ist die
 * Suche **massstabsunabhaengig**: ob die Diskette schnell oder langsam
 * laeuft, ob die angenommene Zellendauer stimmt, ob der Strom innerhalb
 * einer Umdrehung driftet — gefunden wird sie trotzdem. Genau deshalb
 * findet das Verfahren Marken dort, wo die PLL aussteigt. Und weil T
 * mitfaellt, sagt jeder Fund gleich, wie lang eine Zelle an dieser Stelle
 * der Spur wirklich ist.
 *
 * ── Was hier NICHT steht, und warum ──────────────────────────────────────
 *
 * Der Plan sieht eine Ordinal-Vorfilterung (Vorzeichen der Differenzen)
 * plus Rabin-Karp vor. Beides ist gemessen und wieder entfernt worden:
 *
 *  - Ein Vorzeichen-Vorfilter war gebaut und lief. Er brachte 1,4x
 *    (0,110 statt 0,155 ms je Spur) bei identischem Ergebnis. Kein
 *    Rotbeweis konnte ihn von seiner Abwesenheit unterscheiden — er war
 *    also ein Beschleuniger, kein Erkenner.
 *  - Rabin-Karp bei acht Symbolen Musterlaenge kostet mehr, als es spart.
 *
 * Der Ausschlag gab nicht die Geschwindigkeit, sondern die Fehlerart: ein
 * falscher Vorfilter verwirft still die richtige Stelle. Auf 45 Mikro-
 * sekunden je Spur ist ein stiller Falsch-Negativ-Pfad im forensischen
 * Lesepfad kein guter Handel.
 *
 * ── Grenzen, ausdruecklich ───────────────────────────────────────────────
 *
 * Das Verfahren findet SYNC-MARKEN, nicht Sektoren. Was danach kommt —
 * Kopf lesen, Pruefsumme rechnen — bleibt Aufgabe des Decoders. Und es
 * findet nur, was als Muster hineingegeben wurde: eine unbekannte
 * Kopierschutz-Marke bleibt unsichtbar.
 *
 * Gemessen an synthetischen AmigaDOS-Spuren (tests/flux_gen/amigados):
 *
 *   Zittern    Marken von 11
 *   ---------  -------------
 *     0…10 %   11
 *       20 %   2…3   (der Taktabgleich verwirft den Rest)
 *     ab 30 %  0
 *
 * Oberhalb von etwa 20 % Zittern traegt das Verfahren also nicht mehr —
 * nicht weil es dann falsch liegt, sondern weil es dann schweigt. Das ist
 * die gewollte Richtung: lieber keine Auskunft als eine erfundene.
 *
 * Was NICHT belegt ist: alle Zahlen stammen von synthetischem Flux mit
 * gleichverteiltem Zittern. Eine reale Aufnahme zittert anders (farbiges
 * Rauschen, Signalabfall, Bandbreitenbegrenzung des Kopfes). Die Grenze an
 * einer echten marginalen Diskette ist damit **offen** — dafuer braucht es
 * ein Korpus, kein Modell.
 */
#ifndef UFT_FLUX_SYNC_SEARCH_H
#define UFT_FLUX_SYNC_SEARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Hoechstzahl Zellabstaende in einem Muster. Das 0x4489-Paar braucht 9. */
#define UFT_SYNC_MAX_CELLS   64

/** Standard-Toleranz des Taktabgleichs: eine Viertelzelle. */
#define UFT_SYNC_DEFAULT_TOL   0.25

/** Ein Suchmuster, aus einer Sync-Wortfolge gebildet. */
typedef struct {
    /** Zellabstaende zwischen den Flusswechseln der Marke. */
    uint8_t k[UFT_SYNC_MAX_CELLS];
    size_t  k_count;
} uft_sync_pattern_t;

/** Ein bestaetigter Fund. */
typedef struct {
    /** Index im Abstandsstrom, an dem die Marke beginnt. */
    size_t index;

    /** Lokale Zellendauer an dieser Stelle, in denselben Einheiten wie die
     *  Abstaende (bei ns-Abstaenden also ns). */
    double local_clock;

    /** Groesste Abweichung eines Abstands vom erwarteten Vielfachen,
     *  relativ zur Zellendauer. Kleiner ist besser; 0 waere perfekt. */
    double fit_error;
} uft_sync_hit_t;

/**
 * @brief Muster aus einer Sync-Wortfolge bilden.
 *
 * @param words   Sync-Woerter, je 16 Zellen, MSB zuerst (Amiga: zweimal
 *                0x4489; IBM: dreimal 0x4489 oder 0x5224)
 * @param n_words Anzahl
 * @param out     Ergebnis
 * @return false, wenn zu wenige Flusswechsel entstehen — unter drei
 *         Abstaenden traegt das Muster zu wenig Form, um im Rauschen zu
 *         bestehen
 */
bool uft_sync_pattern_from_words(const uint16_t *words, size_t n_words,
                                 uft_sync_pattern_t *out);

/**
 * @brief Muster per Taktabgleich im Abstandsstrom suchen.
 *
 * @param deltas     Abstaende zwischen Flusswechseln (INTERVALLE, keine
 *                   kumulierten Zeiten — die Verwechslung hat MF-438 einmal
 *                   gekostet)
 * @param n          Anzahl
 * @param pat        Suchmuster
 * @param tolerance  Erlaubte Abweichung je Abstand, relativ zur Zellendauer;
 *                   <= 0 waehlt @ref UFT_SYNC_DEFAULT_TOL
 * @param hits       Ergebnisfeld
 * @param max_hits   Groesse von @p hits
 * @return Anzahl Funde (hoechstens @p max_hits)
 */
size_t uft_sync_search_intervals(const uint32_t *deltas, size_t n,
                                 const uft_sync_pattern_t *pat,
                                 double tolerance,
                                 uft_sync_hit_t *hits, size_t max_hits);

/**
 * @brief Was sagen mehrere Funde gemeinsam ueber die Zellendauer?
 *
 * Der Median, nicht der Mittelwert und nicht der erste Fund. Eine reale
 * Diskette laeuft nicht ueberall gleich schnell — ein verzogenes Band, eine
 * schwankende Andruckrolle, ein Laufwerk mit Gleichlauffehler. Dann traegt
 * EIN Abschnitt der Spur eine andere Zellendauer als der Rest. Der erste
 * Fund koennte gerade in diesem Abschnitt liegen und verzoege daran die
 * ganze Spur; der Mittelwert liesse sich von ihm anteilig mitziehen. Der
 * Median bleibt bei dem Wert, den die Mehrheit der Marken zeigt.
 *
 * @return Zellendauer, oder 0 bei @p n == 0
 */
double uft_sync_median_clock(const uft_sync_hit_t *hits, size_t n);

/** Wie @ref uft_sync_search_intervals, aber fuer KUMULIERTE Uebergangs-
 *  zeiten — die Darstellung in `flux_raw_data_t`. Die Abstaende werden im
 *  Durchlauf gebildet; gesucht wird dasselbe. */
size_t uft_sync_search_transitions(const uint32_t *transitions, size_t n,
                                   const uft_sync_pattern_t *pat,
                                   double tolerance,
                                   uft_sync_hit_t *hits, size_t max_hits);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_SYNC_SEARCH_H */
