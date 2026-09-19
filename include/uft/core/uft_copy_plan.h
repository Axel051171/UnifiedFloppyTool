/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_copy_plan.h
 * @brief Der Kopierplan in vier Dimensionen (MF-1232)
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * Eigentuemer-Vorgabe vom 2026-09-18, woertlich: *„File/Sector/Track/Raw/
 * Nibble/Flux sind Ebenen. Deep und Consensus sind Strategien. Protected
 * ist eine Erhaltungsrichtlinie. Evidence ist eine Sicherheitsrichtlinie.
 * Erst durch diese Trennung lassen sich die Parameter sauber verteilen,
 * ohne fuer jeden neuen Copy-Modus wieder eine eigene Sonderlogik zu
 * bauen."*
 *
 * Die vier Aufzaehlungen, die Stufen und die erzwungenen Werte stammen
 * aus dieser Vorgabe; sie sind **Modell**, nicht Messung, und das steht
 * hier, damit niemand sie fuer eine Messung haelt.
 *
 * ── Warum vier statt elf ─────────────────────────────────────────────────
 *
 * Bis MF-1232 fuehrte die Oberflaeche einen einzigen „Copy-Modus". Gemessen
 * gab es ihn sogar **zweimal**, mit zwei verschiedenen Wertelisten:
 *
 *   forms/tab_format.ui:102  comboXCopyMode  Sector, Track, Index
 *                            -> **0 Leser** im ganzen Baum
 *   forms/tab_xcopy.ui:205   comboCopyMode   Sector/Track/Flux/Nibble Copy
 *                            -> gelesen in xcopytab.cpp:406,418
 *
 * Mit EINER Achse ist `Flux + Deep + Protected + Evidence` nicht
 * ausdrueckbar — und genau das ist der Fall, um den es beim forensischen
 * Sichern geht. Die vier Dimensionen hier sind orthogonal: je eine Wahl,
 * und der Plan ist ihr Produkt.
 *
 * ── Der Fehler, den die Stufen beheben ───────────────────────────────────
 *
 * Das gelieferte Parametermodell fuehrt
 *
 *     k_cfl_passes[] = { "write_enable", UFT_SEV_HARD, …
 *                        "Beim Schreiben gibt es keine Ganzdurchlaeufe" }
 *
 * — ein **harter** Konflikt zwischen `passes` und `write_enable`. Fuer
 * einen Kopiervorgang ist das falsch: fuenfmal lesen, das beste Ergebnis
 * bilden, einmal schreiben ist der Normalfall. Die Aussage stimmt nur
 * INNERHALB der Schreibstufe.
 *
 * Deshalb traegt hier jeder Parameter seine `uft_copy_stage_t`, und
 * `uft_copy_conflict_applies()` laesst einen Konflikt nur gelten, wenn
 * beide Seiten in derselben Stufe liegen. Der Fall ist als Zusage
 * festgehalten (`tests/test_copy_plan.c`), nicht als Kommentar.
 *
 * ── Was dieser Header NICHT tut ──────────────────────────────────────────
 *
 * Er fuehrt kein Geraet und liest keine Diskette. Er beantwortet drei
 * Fragen ueber einen Plan:
 *   1. ist er in sich stimmig?              uft_copy_plan_check()
 *   2. was erzwingt er?                     uft_copy_plan_enforced()
 *   3. was bedeutet er fuer Parameter X?    uft_copy_param_state()
 *
 * Ob ein FORMAT den Plan traegt, entscheidet er nicht allein — dafuer
 * nimmt `uft_copy_plan_check()` die Faehigkeitsflaggen entgegen. Gemessen
 * (MF-1231) sagt **kein** Plugin `TIMING` und `WEAK_BITS` zugleich zu; die
 * Erhaltung PROTECTED ist damit heute fuer jedes Format unerfuellbar, und
 * das soll der Aufrufer erfahren statt es stillschweigend anzunehmen.
 */

#ifndef UFT_COPY_PLAN_H
#define UFT_COPY_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 1. Kopierebene — genau eine ──────────────────────────────────────
 *
 * Die Reihenfolge ist BEDEUTUNG, nicht Geschmack: sie waechst von der
 * hoechsten Abstraktion zur rohesten Aufzeichnung, und
 * `uft_copy_plan_check()` vergleicht mit `<`, wenn eine
 * Erhaltungsrichtlinie eine Mindestebene verlangt.
 *
 * `UFT_COPY_AUTO` steht deshalb HINTER Flux und nicht davor: es ist
 * keine Ebene, sondern die Abwesenheit einer Wahl. Wer es setzt, muss
 * `uft_copy_plan_resolve()` rufen, bevor eine Regel greift — ein
 * unaufgeloestes AUTO ergibt einen Befund, keine stille Annahme. */
typedef enum {
    UFT_COPY_FILE = 0,
    UFT_COPY_SECTOR,
    UFT_COPY_TRACK,
    UFT_COPY_BITSTREAM,
    UFT_COPY_NIBBLE,
    UFT_COPY_FLUX,
    UFT_COPY_AUTO,              /**< aus Format und Ziel ableiten */
    UFT_COPY_LEVEL_N
} uft_copy_level_t;

/** Die Ebenen, die wirklich Ebenen sind — ohne AUTO. Fuer Schleifen,
 *  die ueber die Rollen je Ebene laufen. */
#define UFT_COPY_LEVEL_ECHT UFT_COPY_AUTO

/** TrackCopy hat zwei Spielarten, und die Vorgabe verlangt sie
 *  ausdruecklich: bei `DECODED` wird die Spur neu erzeugt, bei `RAW`
 *  der vorhandene Bitstrom moeglichst erhalten. */
typedef enum {
    UFT_TRACK_DECODED = 0,
    UFT_TRACK_RAW,
    UFT_TRACK_MODE_N
} uft_track_mode_t;

/** Die Dateiebene hat Spezialisierungen. „Ein allgemeiner Schalter
 *  reicht nicht" gilt hier genauso wie bei GCR. */
typedef enum {
    UFT_FILE_GENERIC = 0,
    UFT_FILE_BAM,               /**< Commodore, BAM + Verzeichnis  */
    UFT_FILE_DOS,               /**< Atari/Apple DOS               */
    UFT_FILE_SPECIAL_N
} uft_file_special_t;

/** Vier GCR-Verfahren, nicht ein Schalter. Die Vorgabe sagt es
 *  woertlich: „Ein allgemeiner Schalter GCR reicht nicht." */
typedef enum {
    UFT_GCR_COMMODORE = 0,
    UFT_GCR_APPLE,
    UFT_GCR_MACINTOSH,
    UFT_GCR_VICTOR9K,
    UFT_GCR_N
} uft_gcr_variant_t;

/** Die sieben Abstimmungsverfahren aus der Vorgabe. */
typedef enum {
    UFT_VOTE_STRICT_MAJORITY = 0,
    UFT_VOTE_WEIGHTED_CONFIDENCE,
    UFT_VOTE_CRC_PREFERRED,
    UFT_VOTE_TIMING_DISTANCE,
    UFT_VOTE_PER_BIT,
    UFT_VOTE_PER_SECTOR,
    UFT_VOTE_PER_TRACK,
    UFT_VOTE_N
} uft_vote_method_t;

/** Hashverfahren. SHA-256 ist unter EVIDENCE Pflicht, SHA-512 optional,
 *  CRC32 nur zusaetzlich fuer technische Vergleiche — nie allein. */
typedef enum {
    UFT_HASH_NONE   = 0u,
    UFT_HASH_SHA256 = 1u << 0,
    UFT_HASH_SHA512 = 1u << 1,
    UFT_HASH_CRC32  = 1u << 2
} uft_hash_set_t;

/* ── 2. Lesestrategie ───────────────────────────────────────────────── */
typedef enum {
    UFT_READ_FAST = 0,
    UFT_READ_STANDARD,
    UFT_READ_DEEP,
    UFT_READ_CONSENSUS,
    UFT_READ_SALVAGE,
    UFT_READ_STRATEGY_N
} uft_read_strategy_t;

/* ── 3. Erhaltungsrichtlinie ────────────────────────────────────────── */
typedef enum {
    UFT_PRESERVE_LOGICAL = 0,
    UFT_PRESERVE_LAYOUT,
    UFT_PRESERVE_PROTECTED,
    UFT_PRESERVE_BIT_EXACT,
    UFT_PRESERVE_N
} uft_preservation_t;

/* ── 4. Beweis- und Sicherheitsrichtlinie ───────────────────────────── */
typedef enum {
    UFT_POLICY_NORMAL = 0,
    UFT_POLICY_VERIFY,
    UFT_POLICY_EVIDENCE,
    UFT_POLICY_N
} uft_copy_policy_t;

/** Bitgenau heisst nicht automatisch flussgleich — die Vorgabe verlangt
 *  die Unterscheidung ausdruecklich. */
typedef enum {
    UFT_EXACT_SECTOR = 0,       /**< nur Sektorinhalte              */
    UFT_EXACT_TRACK_BIT,        /**< Bitstrom je Spur               */
    UFT_EXACT_FLUX_TIMING,      /**< auch die Flusszeiten           */
    UFT_EXACT_N
} uft_bitexact_kind_t;

/** Die Verarbeitungsstufe eines Parameters. Ein Konflikt gilt nur
 *  innerhalb einer Stufe — siehe den Kopf dieser Datei. */
typedef enum {
    UFT_STAGE_READ = 0,
    UFT_STAGE_DECODE,
    UFT_STAGE_LAYOUT,
    UFT_STAGE_WRITE,
    UFT_STAGE_VERIFY,
    UFT_STAGE_FS,               /**< Dateisystemebene               */
    UFT_STAGE_EVIDENCE,         /**< Beweisfuehrung, Hash, Herkunft */
    UFT_STAGE_N
} uft_copy_stage_t;

/** Was der Plan mit einem Parameter macht. Genau die sechs Zustaende aus
 *  der Vorgabe: anzeigen, ausgrauen, sperren, vorausfuellen, zwingend
 *  setzen, Verlustwarnung. */
typedef enum {
    UFT_PSTATE_HIDDEN = 0,      /**< fuer diese Ebene nicht vorgesehen   */
    UFT_PSTATE_FORBIDDEN,       /**< ausdruecklich nicht anzeigen        */
    UFT_PSTATE_ACTIVE,          /**< frei einstellbar                    */
    UFT_PSTATE_CONDITIONAL,     /**< nur unter Bedingungen (ausgrauen)   */
    UFT_PSTATE_READONLY,        /**< gemessen, nicht einstellbar         */
    UFT_PSTATE_FORCED           /**< der Plan setzt den Wert, gesperrt   */
} uft_copy_pstate_t;

/** Faehigkeiten, die ein Plan vom Format oder vom Geraet verlangt.
 *  Bitmaske, damit `uft_copy_plan_check()` sie ohne Plugin-Abhaengigkeit
 *  entgegennehmen kann. */
typedef enum {
    UFT_CAP_NONE          = 0u,
    UFT_CAP_FLUX_IO       = 1u << 0,  /**< Fluss lesen UND schreiben     */
    UFT_CAP_BITSTREAM_IO  = 1u << 1,
    UFT_CAP_MULTI_REV     = 1u << 2,
    UFT_CAP_TIMING        = 1u << 3,
    UFT_CAP_WEAK_BITS     = 1u << 4,
    UFT_CAP_FILESYSTEM    = 1u << 5,
    UFT_CAP_CBM_BAM       = 1u << 6,
    UFT_CAP_GCR           = 1u << 7
} uft_copy_caps_t;

/** Der Plan selbst: vier Wahlen, dazu die Feinheiten, die die Vorgabe
 *  ausdruecklich verlangt — Spurart, Dateispezialisierung, GCR-Verfahren,
 *  Abstimmungsverfahren, Hashsatz und die Spielart der Bitgenauigkeit. */
typedef struct {
    uft_copy_level_t     level;
    uft_read_strategy_t  strategy;
    uft_preservation_t   preservation;
    uft_copy_policy_t    policy;

    uft_bitexact_kind_t  exact_kind;   /**< nur bei BIT_EXACT bedeutsam   */
    uft_track_mode_t     track_mode;   /**< nur auf der Spurebene         */
    uft_file_special_t   file_special; /**< nur auf der Dateiebene        */
    uft_gcr_variant_t    gcr;          /**< nur auf der Nibble-Ebene      */
    uft_vote_method_t    vote;         /**< nur bei CONSENSUS             */
    uint32_t             hashes;       /**< uft_hash_set_t, nur EVIDENCE  */
} uft_copy_plan_t;

/** Ein Befund aus der Pruefung. `hard` heisst: so nicht ausfuehrbar. */
typedef struct {
    bool        hard;
    const char *id;        /**< kurzer, stabiler Bezeichner des Befunds */
    const char *text;      /**< Begruendung, fuer den Bediener          */
} uft_copy_finding_t;

/**
 * Was fuer eine Art Wert steht in `uft_copy_enforced_t::value`?
 *
 * MF-1264: `value` ist eine Zeichenkette und trug DREI Dinge, die ein
 * Verbraucher nicht auseinanderhalten konnte. Gemessen ueber alle
 * Wertetafeln des Plans:
 *
 *     Wahrheitswert  46   "true" 36, "false" 10
 *     Forderung      13   "noetig" 7, "Pflicht" 3, "hoch" 2,
 *                         "gewaehlt" 1
 *     Zahl            9   "1" 3, "5" 2, "3", "2", "10", "0"
 *
 * Eine **Forderung ist kein Wert**: „hoch" sagt, dass jemand eine Zahl
 * waehlen MUSS, nicht dass die Zahl „hoch" ist. Wer beides als
 * Zeichenkette liest, kann „setze das auf hoch" nicht von „hier fehlt
 * noch eine Entscheidung" unterscheiden — und genau daran ist die
 * Abbildung auf die Wandlungsoptionen (MF-1263) beinahe gescheitert:
 * `strtoul("hoch")` ergibt 0, also eine erfundene Zahl.
 *
 * Gemessen gibt es in den Tafeln **keinen** echten Zeichenkettenwert.
 * Sollte einer dazukommen, faellt `tests/test_copy_plan.c` — die
 * Einteilung ist bewacht, nicht angenommen.
 */
typedef enum {
    UFT_WERT_ZAHL = 0,      /**< nur Ziffern: "0", "3", "10"            */
    UFT_WERT_WAHRHEIT,      /**< genau "true" oder "false"              */
    UFT_WERT_FORDERUNG      /**< „hoch", „noetig", „Pflicht", „gewaehlt" */
} uft_wert_art_t;

/** Ein vom Plan erzwungener Wert. */
typedef struct {
    const char       *param;   /**< z. B. "read.revolutions"            */
    const char       *value;   /**< "5", "true", "false", "Pflicht"     */
    uft_copy_stage_t  stage;
    const char       *reason;  /**< welche Dimension ihn erzwingt       */
    /** MF-1264: welche Art `value` ist — damit eine FORDERUNG nicht
     *  wie ein Wert gelesen wird. Additiv; wer sie nicht liest, sieht
     *  dasselbe wie vorher. */
    uft_wert_art_t    art;
} uft_copy_enforced_t;

/* ── API ────────────────────────────────────────────────────────────── */

/** Ein Plan, der nichts Besonderes verlangt: Sektorebene, Standard,
 *  logische Erhaltung, normale Sicherheit. */
uft_copy_plan_t uft_copy_plan_default(void);

/**
 * Wer den geltenden Plan liefert — und wie man ihn erfragt (MF-1265).
 *
 * `P3-509`, zweiter Halbsatz: „FluxCopy + DeepCopy + Protected +
 * Evidence ist EIN Vorgang". Gemessen fuellen DREI Stellen die
 * Wandlungsoptionen, und keine davon kennt den Reiter, der den Plan
 * baut. Drei Wege einzeln zu verkabeln waere viel Klempnerei fuer eine
 * Einstellung, die es nur EINMAL gibt.
 *
 * **Eine FUNKTION, kein hinterlegter Wert** — und das ist der Punkt:
 * eine Kopie im Kern koennte veralten, sobald der Bediener etwas
 * umstellt. Die Auskunft fragt bei jedem Aufruf die Quelle.
 *
 * **Der Kern kennt dabei keine Oberflaeche.** Er haelt einen Zeiger auf
 * eine Funktion und einen undurchsichtigen Zusammenhang; wer ihn setzt,
 * ist seine Sache. Genau daran ist die erste Fassung gescheitert: sie
 * legte die Auskunft in `FormatTab`, womit `ToolsTab` an einem anderen
 * Reiter hing — **gemeldet hat es der BINDER**, nicht ich
 * (`undefined reference to FormatTab::aktuellerPlan()` in zwei
 * Qt-Tests, die `toolstab.cpp` ohne `formattab.cpp` binden).
 *
 * **NUR aus dem Faden rufen, in dem die Quelle lebt.** Ein
 * Hintergrundauftrag nimmt stattdessen eine KOPIE mit, die vor dem
 * Start gesetzt wird (`DecodeJob::setCopyPlan`).
 */
typedef uft_copy_plan_t (*uft_copy_plan_quelle_fn)(void *zusammenhang);

/** Die Quelle setzen. `fn = NULL` loescht sie — danach liefert
 *  `uft_copy_plan_current()` wieder die Vorgaben. Wer sich abmeldet,
 *  muss das im eigenen Destruktor tun; ein Zeiger auf einen
 *  freigegebenen Zusammenhang ist die einzige Gefahr dieser Bauform,
 *  und genau darauf zielt der Rotbeweis. */
void uft_copy_plan_set_quelle(uft_copy_plan_quelle_fn fn, void *zusammenhang);

/** Der geltende Plan. **Ohne Quelle die VORGABEN**, nicht ein genullter
 *  Zufall: ein genullter Plan waere ein GUELTIGER Plan
 *  (FILE/FAST/LOGICAL/NORMAL) und damit eine Aussage, die niemand
 *  getroffen hat. */
uft_copy_plan_t uft_copy_plan_current(void);

/** Namen fuer die Anzeige. Geben NULL fuer unbekannte Werte — nie eine
 *  erfundene Zeichenkette. */
const char *uft_copy_level_name(uft_copy_level_t v);
const char *uft_copy_strategy_name(uft_read_strategy_t v);
const char *uft_copy_preservation_name(uft_preservation_t v);
const char *uft_copy_policy_name(uft_copy_policy_t v);
const char *uft_copy_stage_name(uft_copy_stage_t v);

/**
 * Der Name EINER Faehigkeitsflagge (MF-1238).
 *
 * Nimmt genau eine der acht Flaggen. `UFT_CAP_NONE` und jede
 * Kombination ergeben NULL — „mehrere Flaggen" ist kein Name, und eine
 * erfundene Sammelbezeichnung waere genau die stille Zusage, gegen die
 * der Plan gebaut ist.
 *
 * Die Liste steht damit an EINER Stelle. Eine zweite Kopie in der
 * Oberflaeche waere die Lage aus MF-1177: zwei Listen derselben Sache
 * driften, und die Abweichung sieht aus wie ein Fehler in den Daten.
 */
const char *uft_copy_cap_name(uft_copy_caps_t v);

/** Wie viele Flaggen es gibt, und die i-te davon — damit ein Aufrufer
 *  ueber die Aufzaehlung laufen kann, statt sie abzuschreiben (MF-636). */
size_t          uft_copy_cap_count(void);
uft_copy_caps_t uft_copy_cap_at(size_t i);

/** Die Art eines Tafelwerts (MF-1264). An EINER Stelle entschieden,
 *  damit „hoch" nicht irgendwo als Zahl und anderswo als Text gilt. */
uft_wert_art_t uft_copy_wert_art(const char *v);

/** Die Stufe eines Parameters, ueber seinen Namensraum.
 *  Unbekannte Praefixe ergeben UFT_STAGE_N — „nicht eingeordnet", nicht
 *  „Lesen". */
uft_copy_stage_t uft_copy_param_stage(const char *param);

/**
 * Gilt ein Konflikt zwischen zwei Parametern?
 *
 * Nur wenn beide in derselben Stufe liegen. Das ist die Regel, an der
 * `passes` x `write_enable` scheitert: `read.passes` ist READ,
 * `write.enabled` ist WRITE — kein Konflikt.
 */
bool uft_copy_conflict_applies(const char *a, const char *b);

/**
 * Ist der Plan in sich stimmig, und traegt das Format ihn?
 *
 * @param plan  der Plan
 * @param caps  was Format und Geraet zusagen (Bitmaske)
 * @param out   Feld fuer die Befunde, darf NULL sein
 * @param max   Platz in `out`
 * @return      Zahl der Befunde — auch wenn `out` NULL ist oder zu klein.
 *              Ein Rueckgabewert groesser als `max` heisst: es gab mehr.
 */
size_t uft_copy_plan_check(const uft_copy_plan_t *plan,
                           uint32_t caps,
                           uft_copy_finding_t *out,
                           size_t max);

/** Traegt der Plan einen harten Befund? Bequemlichkeit ueber check(). */
bool uft_copy_plan_is_executable(const uft_copy_plan_t *plan, uint32_t caps);

/**
 * Was erzwingt dieser Plan?
 *
 * Liefert die Werte, die aus Strategie, Erhaltung und Sicherheit folgen —
 * in dieser Reihenfolge, damit die spaetere Dimension die fruehere
 * ueberschreiben kann (Sicherheit gewinnt).
 */
size_t uft_copy_plan_enforced(const uft_copy_plan_t *plan,
                              uft_copy_enforced_t *out,
                              size_t max);

/**
 * Was bedeutet der Plan fuer diesen einen Parameter?
 *
 * @param wert  wird, wenn nicht NULL, auf den erzwungenen Wert gesetzt —
 *              oder auf NULL, wenn keiner erzwungen ist.
 */
uft_copy_pstate_t uft_copy_param_state(const uft_copy_plan_t *plan,
                                       const char *param,
                                       const char **wert);

/** Wieviele Parameter kennt die Tafel? Fuer Laeufe ueber alle. */
size_t uft_copy_param_count(void);

/** Der Name des i-ten Parameters, oder NULL. */
const char *uft_copy_param_id(size_t i);

/* ── Je Parameter: welche Faehigkeiten er verlangt und welche ihn
 *    ausschliessen (Vorgabe: requiresCapabilities / forbidsCapabilities)
 *
 * Bis MF-1234 gab es die Pruefung nur je PLAN. Damit konnte die
 * Oberflaeche zwar sagen „dieser Plan geht nicht", aber nicht „dieses
 * Feld ist fuer dieses Format ohne Bedeutung". Jetzt beides. */

/** Bitmaske der Faehigkeiten, die dieser Parameter braucht. 0 = keine. */
uint32_t uft_copy_param_requires(const char *param);

/** Bitmaske der Faehigkeiten, die ihn ausschliessen. 0 = keine. */
uint32_t uft_copy_param_forbids(const char *param);

/**
 * Wie `uft_copy_param_state()`, aber mit den Faehigkeiten des Formats.
 *
 * Fehlt eine verlangte Faehigkeit, ist der Parameter HIDDEN — nicht
 * ausgegraut: ein Regler, der fuer dieses Format nichts bedeutet, ist
 * keine Einstellung, sondern eine Irrefuehrung.
 */
uft_copy_pstate_t uft_copy_param_state_caps(const uft_copy_plan_t *plan,
                                            uint32_t caps,
                                            const char *param,
                                            const char **wert);

/* ── Automatik aufloesen ────────────────────────────────────────────── */

/**
 * Loest `UFT_COPY_AUTO` in eine echte Ebene auf — aus dem, was die
 * Faehigkeiten hergeben, und NUR daraus.
 *
 * Fluss, wenn Fluss da ist; sonst Bitstrom; sonst Dateien, wenn ein
 * Dateisystem erkannt ist; sonst Sektoren. Ist der Plan schon konkret,
 * bleibt er unveraendert.
 */
uft_copy_plan_t uft_copy_plan_resolve(const uft_copy_plan_t *plan,
                                      uint32_t caps);

/* ── Der Plan als JSON, in der vorgegebenen Gestalt ─────────────────── */

/**
 * Schreibt den Plan als JSON nach `buf`.
 *
 * Die Gestalt ist die der Vorgabe, Schluessel fuer Schluessel:
 * `copyPlan`, `read`, `consensus`, `preserve`, `write`, `evidence` —
 * camelCase, und nur die Abschnitte, die der Plan wirklich betrifft.
 *
 * @return die Zahl der Zeichen, die geschrieben WUERDEN (wie snprintf).
 *         Ein Wert >= `n` heisst: der Puffer war zu klein, und der
 *         Inhalt ist abgeschnitten — nicht still, sondern erkennbar.
 */
size_t uft_copy_plan_to_json(const uft_copy_plan_t *plan,
                             char *buf, size_t n);

/* ── Profile: benannte Plaene (MF-1236) ─────────────────────────────
 *
 * Vier Achsen ergeben 360 Plaene. Das ist eine vollstaendige Beschreibung
 * und ein schlechter Einstieg: ein Bediener denkt nicht in
 * „bitstream/standard/layout/verify“, sondern in „RawCopy“.
 *
 * Ein Profil ist deshalb ein **Name fuer einen Plan** — keine zweite
 * Moduslogik daneben. Es setzt die vier Achsen und sonst nichts; wer
 * eine davon anfasst, arbeitet danach ohne Profil weiter
 * („Benutzerdefiniert“), und das ist ein Zustand der Oberflaeche, kein
 * Eintrag dieser Tafel.
 *
 * Die Tafel steht im KERN und nicht in der Oberflaeche. Der Grund ist
 * gemessen: `comboXCopyMode` (Settings) und `comboCopyMode` (XCopy)
 * fuehren heute zwei verschiedene Moduslisten, und welche gilt,
 * entscheidet der Reiter. Zwei Listen derselben Sache driften (MF-1177).
 */
typedef struct {
    const char       *id;      /**< stabil, fuer JSON und Einstellungen  */
    const char       *name;    /**< was im Auswahlfeld steht             */
    const char       *text;    /**< ein Satz, was das Profil tut         */
    uft_copy_plan_t   plan;    /**< die vier Achsen, die es setzt        */
    uint32_t          braucht; /**< Faehigkeiten, ohne die es sinnlos ist */

    /** **Behelf, und als solcher gekennzeichnet.**
     *
     * `braucht` waere der richtige Weg — nur setzt heute KEIN Plugin
     * `UFT_CAP_GCR`, `UFT_CAP_CBM_BAM` oder `UFT_CAP_FILESYSTEM`
     * (gemessen ueber alle 88). Solange das so ist, entscheidet
     * ersatzweise diese Liste von Formatnamen, durch `;` getrennt.
     *
     * Eine gepflegte Namensliste veraltet still — dieser Baum hat vier
     * belegte Vorfaelle (MF-636). Sie steht hier, damit sie sichtbar ist
     * und verschwindet, sobald die Flaggen gefuellt sind; NULL heisst:
     * keine Einschraenkung. */
    const char       *behelf_formate;
} uft_copy_profile_t;

/** Wieviele Profile kennt der Kern? */
size_t uft_copy_profile_count(void);

/** Das i-te Profil, oder NULL. */
const uft_copy_profile_t *uft_copy_profile(size_t i);

/** Profil zu seiner stabilen Kennung, oder NULL. */
const uft_copy_profile_t *uft_copy_profile_by_id(const char *id);

/**
 * Ist dieses Profil fuer das vorliegende Format sinnvoll?
 *
 * @param caps   Faehigkeiten des Formats
 * @param format Formatname fuer den Behelf, darf NULL sein
 * @param grund  wird bei `false` auf eine Begruendung gesetzt
 */
bool uft_copy_profile_available(const uft_copy_profile_t *p,
                                uint32_t caps, const char *format,
                                const char **grund);

/* ── Der Plan wirkt: Abbildung auf die Wandlungsoptionen (MF-1263) ──── */

/* Vorwaerts deklariert statt eingebunden. Dieser Kopf kommt mit
 * `stdbool/stddef/stdint` aus, und das soll er behalten —
 * `uft/uft_types.h` waere ein grosser Nachbar fuer ein Feld. */
struct uft_convert_options;

/**
 * Traegt den Plan in die Optionen einer Wandlung.
 *
 * ── WARUM ES DIESE FUNKTION GIBT ─────────────────────────────────────
 *
 * `P3-509`: der Plan hatte ausser seinem Reiter keinen Leser. Vier
 * Achsen, eine Anzeige — und kein Vorgang, der danach handelt. Das ist
 * die Klasse P0-2/MF-930: **Bestand, nicht Faehigkeit.**
 *
 * ── WAS SIE ABBILDET, UND WORAUS ─────────────────────────────────────
 *
 * Die Zahlen stehen NICHT in dieser Funktion. Sie stehen in der
 * Wertetafel des Plans (`k_strategie[]` in `uft_copy_plan.c`), die die
 * Vorgabe je Lesestrategie fuehrt — `read.retries` und
 * `read.revolutions`. Die Abbildung liest sie dort und traegt sie
 * weiter; eine zweite Zahlenreihe hier waere die Drift aus MF-541.
 *
 * ── WAS SIE AUSDRUECKLICH NICHT TUT ──────────────────────────────────
 *
 * 1. **Sie erfindet keine Zahl.** `UFT_READ_SALVAGE` traegt
 *    `read.retries = "hoch"` — ein Forderungswort, keine Zahl. Wo die
 *    Tafel keine Zahl nennt, bleibt die Option, wie sie war.
 * 2. **Sie bildet nicht auf tote Optionen ab.** Gemessen (MF-1263)
 *    haben `preserve_timing`, `normalize` und `interpolate_errors`
 *    **null** Leser, die danach handeln. Eine Achse darauf abzubilden
 *    haette „der Plan wirkt" behauptet, ohne dass sich etwas aendert —
 *    also `P3-509` noch einmal, eine Ebene hoeher.
 * 3. **Sie schwaecht nichts ab.** `preserve_errors` und
 *    `preserve_weak_bits` bleiben unberuehrt; eine Erhaltungsachse, die
 *    Fehlermarken abschaltet, waere ein stiller Verlust.
 *
 * Damit erreichen heute **zwei** der vier Achsen die Wandlung:
 * Lesestrategie und Richtlinie. Ebene und Erhaltung erreichen sie
 * nicht, weil es dort keinen lebenden Verbraucher gibt — das steht so
 * in `P3-509` und ist kein Versehen.
 *
 * @param plan  der Plan; NULL laesst `opts` unveraendert
 * @param opts  bereits mit `uft_convert_default_options()` gefuellt
 */
void uft_copy_plan_to_convert_options(const uft_copy_plan_t *plan,
                                      struct uft_convert_options *opts);

/* ── Namen der Feinheiten ───────────────────────────────────────────── */
const char *uft_copy_track_mode_name(uft_track_mode_t v);
const char *uft_copy_file_special_name(uft_file_special_t v);
const char *uft_copy_gcr_name(uft_gcr_variant_t v);
const char *uft_copy_vote_name(uft_vote_method_t v);
const char *uft_copy_exact_name(uft_bitexact_kind_t v);

#ifdef __cplusplus
}
#endif

#endif /* UFT_COPY_PLAN_H */
