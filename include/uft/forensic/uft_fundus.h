/**
 * @file uft_fundus.h
 * @brief Anhaengender Aufnahme-Speicher mit Herkunft (MF-503).
 *
 * Baustein 1.3 des Mammut-Plans, Kern.
 *
 * ── Wozu ─────────────────────────────────────────────────────────────────
 *
 * Eine Diskette wird selten einmal gelesen. Man liest sie, dreht sie um,
 * putzt sie, liest sie wieder — und am Ende soll nachvollziehbar sein,
 * welche Aufnahme wann unter welchen Bedingungen entstand. Ohne einen Ort
 * dafuer landet jede Aufnahme als Datei mit selbstgewaehltem Namen
 * irgendwo, und die Umstaende stehen bestenfalls im Gedaechtnis.
 *
 * Der Fundus ist dieser Ort: ein Verzeichnis, in das Artefakte
 * **angehaengt** werden, nie ersetzt.
 *
 * ── Die Zusicherungen ────────────────────────────────────────────────────
 *
 * - **Anhaengend, nicht ersetzend.** Ein abgelegtes Artefakt aendert sich
 *   nie wieder, und das Manifest waechst nur am Ende. Deshalb ist es eine
 *   Zeile je Eintrag (JSONL) und kein JSON-Feld: ein JSON-Array laesst
 *   sich nicht anhaengen, ohne die ganze Datei neu zu schreiben — und
 *   „neu schreiben" ist genau das, was hier nicht passieren darf.
 * - **Nummern werden nie wiederverwendet.** Auch wenn jemand eine Datei
 *   von aussen loescht, bekommt die naechste Aufnahme die naechste Nummer.
 *   Eine wiederverwendete Nummer waere zwei verschiedene Aufnahmen unter
 *   einem Namen.
 * - **Ohne UFT pruefbar.** Neben jedem Artefakt liegt eine Pruefsummen-
 *   Datei im Format von `sha256sum`, damit ein Archiv auch in zwanzig
 *   Jahren mit Bordmitteln geprueft werden kann. Ein Archiv, dessen
 *   Integritaet nur das erzeugende Programm bestaetigen kann, ist als
 *   Archiv wenig wert.
 * - **Keine erfundenen Angaben.** Was der Aufrufer nicht mitteilt, steht
 *   nicht im Manifest — es wird nicht durch einen Standardwert ersetzt.
 *
 * ── Abweichung vom Plan, begruendet ──────────────────────────────────────
 *
 * Der Plan nennt SHA-512-Sidecars. Hier steht **SHA-256**: der Baum hat
 * `uft_sha256.h`, und Provenance-Kette wie Korpus-Manifest rechnen bereits
 * damit. SHA-512 waere eine dritte Hash-Konvention in einem Projekt und
 * zugleich neuer, ungepruefter Kryptocode — unter der Einfrier-Regel
 * (MF-498) braeuchte er eine benannte Referenz (NIST-Testvektoren). Das
 * ist eigene Arbeit mit eigener Begruendung, nicht Beiwerk hier.
 * `sha256sum -c` ist genauso Bordmittel wie `sha512sum -c`.
 *
 * ── Was hier NICHT drin ist ──────────────────────────────────────────────
 *
 * Sitzungs-Fortsetzung („Metadaten der letzten Sitzung laden"), die
 * Teilaufnahme-Karte nach ddrescue-Vorbild und die Mining-Schleife. Der
 * Fundus ist ihre Voraussetzung, nicht ihre Umsetzung.
 */
#ifndef UFT_FUNDUS_H
#define UFT_FUNDUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Laengste unterstuetzte Pfadangabe. */
#define UFT_FUNDUS_PATH_MAX   512

/** Ein geoeffneter Fundus. */
typedef struct {
    char   dir[UFT_FUNDUS_PATH_MAX];       /**< Verzeichnis */
    char   manifest[UFT_FUNDUS_PATH_MAX];  /**< Pfad der Manifest-Datei */
    /** Naechste zu vergebende Nummer. Beim Oeffnen aus dem Manifest
     *  gelesen, damit eine fortgesetzte Sitzung nicht bei 1 anfaengt. */
    unsigned next_seq;
} uft_fundus_t;

/** Wie vollstaendig eine Aufnahme ist (MF-506). */
typedef enum {
    /** Niemand hat etwas dazu gesagt. Bewusst 0. */
    UFT_FUNDUS_STATE_UNSPECIFIED = 0,
    /** Der Bediener erklaert die Aufnahme fuer vollstaendig. */
    UFT_FUNDUS_STATE_COMPLETE,
    /** Abgebrochen — die Daten sind da, aber unvollstaendig. */
    UFT_FUNDUS_STATE_INTERRUPTED
} uft_fundus_state_t;

/**
 * Angaben zu einem Artefakt.
 *
 * Alle Felder duerfen NULL sein. Ein NULL-Feld wird **weggelassen**, nicht
 * durch einen Standardwert ersetzt: „unbekannter Bediener" ist eine
 * andere Aussage als „Bediener: unbekannt".
 */
typedef struct {
    const char *identifier;        /**< Kennung der Diskette */
    const char *description;       /**< was das ist */
    const char *notes;             /**< freier Text */
    const char *operator_id;       /**< wer die Aufnahme gemacht hat */
    const char *capture_protocol;  /**< nach welchem Rezept */
    const char *tool;              /**< Erzeuger samt Version */

    /** Wie vollstaendig diese Aufnahme ist (MF-506).
     *
     *  @ref UFT_FUNDUS_STATE_UNSPECIFIED ist bewusst 0: eine genullte
     *  Angabe behauptet nichts. Waere „vollstaendig" der Standardwert,
     *  truege jede Aufnahme eine Aussage, die niemand gemacht hat. */
    uft_fundus_state_t state;

    /** Nummer der Aufnahme, die diese fortsetzt; 0 = keine (MF-506).
     *
     *  Der Plan wollte unterbrochene Aufnahmen „ergaenzen statt neu
     *  nummerieren". Woertlich hiesse das, den bestehenden Eintrag zu
     *  aendern — und der Fundus haengt an, er ersetzt nicht. „Ergaenzt"
     *  heisst hier deshalb: ein NEUER Eintrag, der auf den alten
     *  verweist. Im Manifest steht danach die ganze Geschichte statt
     *  eines Eintrags, dem man nicht mehr ansieht, dass er einmal
     *  unvollstaendig war. */
    unsigned continues_seq;

    /** Kopfhash der Herkunftskette, hexadezimal (MF-504).
     *
     *  **Nicht von Hand setzen.** Dieses Feld fuellt
     *  @ref uft_fundus_add_from_chain aus der geprueften Kette; wer es
     *  selbst setzt, bekommt von dort eine Absage. Beim einfachen
     *  @ref uft_fundus_add darf es gesetzt werden — dann behauptet der
     *  Aufrufer die Herkunft selbst und traegt sie auch. */
    const char *chain_hash;
} uft_fundus_meta_t;

/** Ergebnis einer Nachpruefung. */
typedef struct {
    int checked;      /**< Eintraege im Manifest */
    int ok;           /**< Datei da und Pruefsumme stimmt */
    int mismatched;   /**< Datei da, Pruefsumme stimmt NICHT */
    int missing;      /**< Datei fehlt */
    /** Erster beanstandeter Eintrag, oder leer. Ein Bericht, der nur eine
     *  Zahl nennt, zwingt zum Suchen. */
    char first_bad[UFT_FUNDUS_PATH_MAX];
} uft_fundus_verify_t;

/**
 * @brief Fundus oeffnen; das Verzeichnis wird bei Bedarf angelegt.
 *
 * Ein bereits bestehender Fundus wird fortgesetzt: @ref
 * uft_fundus_t::next_seq zaehlt hinter dem hoechsten bereits vergebenen
 * Eintrag weiter.
 *
 * @return false, wenn das Verzeichnis nicht benutzbar ist
 */
bool uft_fundus_open(const char *dir, uft_fundus_t *out);

/**
 * @brief Fundus schliessen.
 *
 * Haelt keine offenen Dateien — jedes Anhaengen oeffnet und schliesst
 * selbst, damit ein Absturz nie einen halb geschriebenen Puffer
 * hinterlaesst. Diese Funktion nullt nur die Struktur; sie gibt es, damit
 * der Aufrufer nicht wissen muss, dass nichts freizugeben ist.
 * NULL ist erlaubt.
 */
void uft_fundus_close(uft_fundus_t *f);

/**
 * @brief Ein Artefakt anhaengen.
 *
 * Schreibt die Daten, die Pruefsummen-Datei daneben und eine Zeile ans
 * Manifest — in dieser Reihenfolge, damit ein Abbruch nie einen
 * Manifest-Eintrag ohne Datei hinterlaesst.
 *
 * @param suffix    Dateiendung ohne Punkt, z. B. "scp". NULL = "bin".
 * @param meta      Angaben, oder NULL
 * @param out_path  Pfad des geschriebenen Artefakts, oder NULL
 * @return false bei unbrauchbaren Argumenten oder Schreibfehler
 */
bool uft_fundus_add(uft_fundus_t *f, const void *data, size_t size,
                    const char *suffix, const uft_fundus_meta_t *meta,
                    char *out_path, size_t out_path_size);

/**
 * @brief Alle Eintraege des Manifests nachrechnen.
 *
 * @return false nur bei unbrauchbaren Argumenten. Ein beschaedigter
 *         Fundus ist **kein** Fehler dieser Funktion, sondern ihr
 *         Ergebnis — es steht in @p r.
 */
bool uft_fundus_verify(const uft_fundus_t *f, uft_fundus_verify_t *r);

/** Was die letzte Sitzung ueber diese Diskette wusste (MF-506). */
typedef struct {
    /** false = zu dieser Kennung steht nichts im Fundus. Dann sind alle
     *  uebrigen Felder leer — es wird nichts erfunden. */
    bool     found;
    unsigned seq;              /**< Nummer des jüngsten Eintrags */
    unsigned continues_seq;    /**< worauf er sich bezieht, 0 = nichts */
    uft_fundus_state_t state;

    char description[192];
    char notes[384];
    char capture_protocol[96];
    char file[128];            /**< Dateiname des Artefakts */
} uft_fundus_recall_t;

/**
 * @brief Die Angaben der letzten Aufnahme dieser Diskette holen.
 *
 * Damit muss beim Wiedereinlegen niemand Kennung, Beschreibung, Notizen
 * und Rezept neu tippen. Getippte Angaben, die jemand zum dritten Mal
 * eingibt, weichen voneinander ab — und dann steht dieselbe Diskette
 * unter zwei Beschreibungen im Archiv.
 *
 * Zurueck kommt der **juengste** Eintrag zu dieser Kennung: wer die
 * Beschreibung zwischendurch praezisiert hat, will die praezisere.
 *
 * Die Kennung wird **genau** verglichen. „DISK-1" ist nicht „DISK-10" —
 * ein Vergleich nach blossem Vorkommen liesse eine Diskette die Notizen
 * einer anderen tragen.
 *
 * @return false nur bei unbrauchbaren Argumenten. Eine unbekannte
 *         Diskette ist kein Fehler, sondern `found == false`.
 */
bool uft_fundus_recall(const uft_fundus_t *f, const char *identifier,
                       uft_fundus_recall_t *out);

/* ==========================================================================
 * Aufzaehlung (MF-561)
 * ========================================================================== */

/**
 * @brief Ein Eintrag des Manifests, so wie er dasteht.
 *
 * Dieselben Felder wie @ref uft_fundus_recall_t plus die Kennung — denn
 * beim Durchgehen weiss der Aufrufer noch nicht, zu welcher Diskette ein
 * Eintrag gehoert.
 *
 * Leere Felder bleiben leer. Ein Manifest-Eintrag, der keine Notizen hat,
 * bekommt hier keine erfundenen.
 */
typedef struct {
    unsigned seq;              /**< laufende Nummer */
    unsigned continues_seq;    /**< worauf er sich bezieht, 0 = nichts */
    uft_fundus_state_t state;

    char identifier[128];      /**< Kennung der Diskette, ggf. leer */
    char description[192];
    char notes[384];
    char capture_protocol[96];
    char file[128];            /**< Dateiname des Artefakts */
} uft_fundus_entry_t;

/**
 * @brief Jeden Eintrag des Manifests einmal sehen.
 *
 * ── Warum es das gibt ────────────────────────────────────────────────────
 *
 * `uft_fundus_recall()` liefert **einen** Eintrag — den juengsten zu einer
 * Kennung. Das ist richtig fuer seinen Zweck (fortsetzen, wo man
 * aufgehoert hat), reicht aber fuer zwei Dinge nicht:
 *
 *   * **Multi-Capture-Overlay** braucht ALLE Aufnahmen derselben
 *     Diskette, um sie uebereinanderzulegen.
 *   * **Die Mining-Schleife** braucht ALLE Eintraege, um sie erneut durch
 *     eine verbesserte Dekodierung zu schicken.
 *
 * `uft_fundus_verify()` laeuft zwar ueber das Manifest, gibt aber nichts
 * heraus; es meldet nur eine Bilanz. Bis MF-561 gab es keine Aufzaehlung.
 *
 * ── Reihenfolge ──────────────────────────────────────────────────────────
 *
 * Manifest-Reihenfolge, also so, wie angehaengt wurde — aeltester Eintrag
 * zuerst. Das ist die Reihenfolge, in der die Aufnahmen entstanden sind,
 * und die einzige, die ohne Sortieren stimmt.
 *
 * @param fn    Rueckruf je Eintrag. Gibt er `false` zurueck, bricht der
 *              Durchgang ab — der Aufrufer bestimmt, wann genug ist.
 * @param user  wird durchgereicht
 * @return false nur bei unbrauchbaren Argumenten oder wenn das Manifest
 *         nicht lesbar ist. Ein LEERER Fundus ist kein Fehler: der
 *         Rueckruf wird dann nie gerufen, und die Antwort ist `true`.
 */
bool uft_fundus_walk(const uft_fundus_t *f,
                     bool (*fn)(const uft_fundus_entry_t *e, void *user),
                     void *user);

/**
 * @brief Alle Eintraege EINER Diskette holen, aelteste zuerst.
 *
 * Genau das, was ein Overlay braucht: mehrere Aufnahmen derselben
 * Diskette, in der Reihenfolge ihrer Entstehung.
 *
 * Die Kennung wird **genau** verglichen, nicht als Teilzeichenkette —
 * dieselbe Regel wie in `uft_fundus_recall()`. Ein Vergleich nach blossem
 * Vorkommen liesse eine Diskette die Aufnahmen einer anderen tragen.
 *
 * @param out       Feld des Aufrufers
 * @param max       seine Groesse
 * @param out_count wie viele geschrieben wurden. Gibt es MEHR als @p max,
 *                  werden die ersten @p max geschrieben und `out_count`
 *                  steht auf @p max — der Aufrufer sieht nicht, dass
 *                  abgeschnitten wurde. Wer das wissen muss, benutzt
 *                  @ref uft_fundus_walk.
 * @return false nur bei unbrauchbaren Argumenten. Eine unbekannte
 *         Diskette ist kein Fehler, sondern `*out_count == 0`.
 */
bool uft_fundus_collect_for(const uft_fundus_t *f, const char *identifier,
                            uft_fundus_entry_t *out, size_t max,
                            size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FUNDUS_H */
