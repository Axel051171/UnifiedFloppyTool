/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_ipf_helper.h
 * @brief IPF ueber eine PROZESSGRENZE — die UFT-Seite der Schnittstelle.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WARUM DIESE DATEI EXISTIERT
 * ══════════════════════════════════════════════════════════════════════
 *
 * Das IPF-Format ist bewusst undokumentiert; die SPS behaelt sich seine
 * Erzeugung vor. Ein Clean-Room-Neubau aus einer Spezifikation ist
 * deshalb versperrt (`docs/QUARANTINE_PROCESS.md` §5, Weg 2 scheidet
 * aus). Bleibt Weg 3: die offizielle Bibliothek ueber eine
 * PROZESSGRENZE rufen, nie einlinken.
 *
 * Die Lizenz der Fremdkomponente ist GEMESSEN, nicht angenommen —
 * `LICENCE.txt` der SPS DECODER LIBRARY v1.02 (Quelle:
 * github.com/simonowen/capsimage, dort das vollstaendige v5.1-Paket;
 * Lizenz: proprietaer, nichtkommerziell, GPL-unvereinbar):
 *
 *     "Redistributions may not be sold, nor may they be used in a
 *      commercial product or activity."
 *
 * Ein Verkaufs- und Kommerzvorbehalt ist eine zusaetzliche
 * Beschraenkung im Sinne von GPL §6. Die Bibliothek ist damit
 * GPL-INKOMPATIBEL — dieselbe Rechtslage wie bei XCopy Pro (MF-746),
 * und keine Rechtsverletzung, sondern eine Unvereinbarkeit. Folge:
 *
 *   - kein Einlinken, keine Quelle im Baum, keine Auslieferung
 *   - der Helfer traegt SEINE Lizenz selbst, dieser Baum bleibt unberuehrt
 *   - UFT liest die IPF-Datei SELBST ein und reicht nur den Pfad weiter;
 *     der Helfer interpretiert, er besitzt nichts
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WAS HIER BELEGT IST UND WAS NICHT
 * ══════════════════════════════════════════════════════════════════════
 *
 * BELEGT (gemessen in `tests/test_ipf_helper.c`, Mutations-Gegenbeweis
 * im Commit MF-917): das Auffinden des Helfers, das Absetzen des
 * Aufrufs ueber einen EINGESETZTEN Runner, das Zerlegen der Antwort
 * nach Protokoll v1, das Nachladen der Nutzdaten aus der Beilage, und
 * jeder Ausfallpfad mit BENANNTEM Grund.
 *
 * NICHT BELEGT: dass ein gegen `capsimg` gebauter Helfer diese Antwort
 * tatsaechlich erzeugt. Auf dieser Maschine liegt kein `capsimg`
 * (gemessen: `which capsimg` leer, keine `CAPSImg.dll`), und die
 * Bibliothek darf hier nicht mitgeliefert werden. Der Vertrag steht in
 * `docs/specs/capsimg-helper/PROTOCOL.md`; wer den Helfer baut, misst
 * ihn gegen die dort benannten Pruefvektoren. Bis dahin ist die Tuer
 * gebaut und der Raum dahinter leer — und das Werkzeug SAGT das,
 * anstatt eine leere Spur vorzutaeuschen.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WARUM EIN EINGESETZTER RUNNER STATT popen()
 * ══════════════════════════════════════════════════════════════════════
 *
 * Der Baum hat dafuer bereits ein Muster: `Fc5025Runner`,
 * `FluxEngineRunner`, `KryoFluxRunner` (MF-256/257) reichen den
 * Prozessaufruf als einsetzbare Funktion herein — Produktion setzt
 * QProcess ein, Tests eine Attrappe. Das macht die Grenze pruefbar,
 * OHNE in der Testkette einen Prozess zu starten.
 *
 * Der Gegenentwurf steht ebenfalls im Baum und ist der Grund, es NICHT
 * so zu machen: `src/hal/uft_kryoflux_dtc.c` ruft viermal `popen()` mit
 * zusammengesetzten Kommandozeilen — das ist eine Shell, und
 * `CLAUDE.md` verbietet `shell=True`-Aequivalente.
 */
#ifndef UFT_IPF_HELPER_H
#define UFT_IPF_HELPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Name der Umgebungsvariable, die auf das Helfer-Programm zeigt. */
#define UFT_IPF_HELPER_ENV "UFT_IPF_HELPER"

/** Protokollfassung, die dieser Uebersetzungsstand versteht. */
#define UFT_IPF_HELPER_PROTOCOL 1

/** Hoechstlaenge einer Fehlermeldung samt NUL. */
#define UFT_IPF_HELPER_ERRLEN 256

/**
 * Eine Spur, wie der Helfer sie meldet.
 *
 * Die Felder spiegeln bewusst die Zugriffsfunktionen des vorhandenen
 * Lesers (`ipf_air_get_track_meta`, `ipf_air_get_track_raw`) — nicht
 * die Datenstrukturen von `capsimg`. Der Vertrag gehoert UFT; was der
 * Helfer intern tut, ist seine Sache und bleibt hinter der Grenze.
 */
typedef struct {
    int      cyl;        /**< Zylinder */
    int      head;       /**< Kopf */
    uint32_t bits;       /**< Spurlaenge in Zellen (0 = unbekannt) */
    uint32_t density;    /**< Dichtekennung der IPF-Datei */
    uint32_t flags;      /**< track_flags der IPF-Datei */
    bool     fuzzy;      /**< Datei meldet unscharfe Bits */
    uint64_t blob_off;   /**< Versatz der Nutzdaten in der Beilage */
    uint32_t blob_len;   /**< Laenge der Nutzdaten in Byte (0 = keine) */
} uft_ipf_helper_track_t;

/** Zerlegte Antwort des Helfers. Mit uft_ipf_helper_reply_free() freigeben. */
typedef struct {
    int                      cylinders;
    int                      heads;
    uint32_t                 platform;
    char                     blob_path[512]; /**< Beilage; leer = keine */
    uft_ipf_helper_track_t  *tracks;
    size_t                   track_count;
} uft_ipf_helper_reply_t;

/**
 * Die Prozessgrenze, eingesetzt statt fest verdrahtet.
 *
 * Der Helfer wird mit GENAU DREI Argumenten gerufen:
 *
 *     <helper> <ipf_path> <index_path> <blob_path>
 *
 * und schreibt seinen Index nach @p index_path, seine Nutzdaten nach
 * @p blob_path. Es gibt bewusst KEINE Pipe:
 *
 *   - `popen()` waere eine Shell, und `CLAUDE.md` verbietet
 *     shell=True-Aequivalente. `src/hal/uft_kryoflux_dtc.c` tut es an
 *     vier Stellen; das ist Altlast, kein Vorbild.
 *   - Ein eigener Rohr-Aufbau (CreateProcess+Pipe / fork+pipe) waere
 *     ~130 Zeilen Plattformcode. Gemessen hat der Baum dafuer KEIN
 *     Hilfsmittel: 0 Treffer auf CreateProcess/posix_spawn/execv in
 *     `src/` und `include/`.
 *   - Die Nutzdaten muessen ohnehin in eine Datei (binaer; ueber einen
 *     Textkanal braeuchten sie eine Kodierung, und ein Kodierfehler
 *     waere eine stille Datenveraenderung).
 *
 * @return Rueckgabewert des Helfers, oder -1 wenn er sich nicht
 *         starten liess
 */
typedef struct {
    int (*run)(void *ctx, const char *helper, const char *ipf_path,
               const char *index_path, const char *blob_path);
    void *ctx;
} uft_ipf_helper_runner_t;

/**
 * Der Laeufer fuer den Produktionsbetrieb: startet den Helfer als
 * eigenen Prozess (argv-Feld, KEINE Shell) und wartet auf ihn.
 */
const uft_ipf_helper_runner_t *uft_ipf_helper_system_runner(void);

/**
 * Pfad des Helfers, oder NULL wenn keiner eingerichtet ist.
 *
 * Quelle ist ausschliesslich die Umgebungsvariable
 * UFT_IPF_HELPER_ENV. Es wird BEWUSST nicht der PATH durchsucht: ein
 * Programm, das zufaellig `uft-ipf-helper` heisst, waere sonst eine
 * stille Entscheidung darueber, wer unsere Abbilder deutet.
 */
const char *uft_ipf_helper_path(void);

/**
 * Zerlegt die Antwort nach Protokoll v1.
 *
 * @param text    Standardausgabe des Helfers
 * @param out     Ergebnis; bei Erfolg mit reply_free() freizugeben
 * @param err     Puffer fuer den BENANNTEN Grund (darf NULL sein)
 * @param errcap  Groesse von @p err
 * @retval UFT_OK                  Antwort verstanden
 * @retval UFT_ERR_FORMAT_INVALID  Antwort unverstaendlich, @p err sagt warum
 * @retval UFT_ERR_MEMORY          Speicher
 */
uft_error_t uft_ipf_helper_parse(const char *text,
                                 uft_ipf_helper_reply_t *out,
                                 char *err, size_t errcap);

/**
 * Auffinden + Aufruf + Zerlegen in einem Schritt.
 *
 * @retval UFT_OK                 Antwort liegt vor
 * @retval UFT_ERR_NOT_SUPPORTED  kein Helfer eingerichtet — @p err traegt
 *                                den Satz, den der Benutzer sehen soll
 * @retval UFT_ERROR_TOOL_FAILED  Helfer lief nicht oder brach ab
 * @retval UFT_ERR_FORMAT_INVALID Antwort unverstaendlich
 */
uft_error_t uft_ipf_helper_query(const uft_ipf_helper_runner_t *runner,
                                 const char *ipf_path,
                                 const char *index_path,
                                 const char *blob_path,
                                 uft_ipf_helper_reply_t *out,
                                 char *err, size_t errcap);

/**
 * Sucht eine Spur in der Antwort.
 * @return Zeiger in @p reply, oder NULL wenn die Spur fehlt.
 */
const uft_ipf_helper_track_t *
uft_ipf_helper_find(const uft_ipf_helper_reply_t *reply, int cyl, int head);

/**
 * Laedt die Nutzdaten einer Spur aus der Beilage nach.
 *
 * @param out_buf  neu belegter Puffer (Aufrufer gibt frei), NULL wenn
 *                 die Spur keine Nutzdaten hat
 * @retval UFT_OK                  gelesen (oder: nichts zu lesen)
 * @retval UFT_ERR_FILE_OPEN       Beilage fehlt
 * @retval UFT_ERR_FILE_READ       Beilage zu kurz fuer die Zusage
 */
uft_error_t uft_ipf_helper_payload(const uft_ipf_helper_reply_t *reply,
                                   const uft_ipf_helper_track_t *track,
                                   uint8_t **out_buf, size_t *out_len,
                                   char *err, size_t errcap);

/** Gibt die Antwort frei. Mehrfachaufruf ist harmlos. */
void uft_ipf_helper_reply_free(uft_ipf_helper_reply_t *reply);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_HELPER_H */
