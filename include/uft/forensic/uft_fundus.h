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

#ifdef __cplusplus
}
#endif

#endif /* UFT_FUNDUS_H */
