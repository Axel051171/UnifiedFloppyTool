/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_schutzbefund.h
 * @brief Ein Schutzbefund, der „nicht gefunden" von „nicht geprüft" trennt.
 *
 * ── Die Regel, die diese Datei trägt ─────────────────────────────────────
 *
 * > **„Nicht gefunden" und „nicht geprüft" dürfen nie dasselbe Ergebnis
 * > erzeugen.**
 *
 * Wirft man ein `.ST`-Abbild in eine Schutzerkennung, können die
 * zeitbasierten Prüfungen gar nicht laufen — das Format trägt keine
 * Zeitinformation. Eine Funktion, die nur eine Liste von Befunden
 * zurückgibt, liefert dann eine **leere** Liste, und die ist von „sauber
 * untersucht, nichts gefunden" nicht zu unterscheiden.
 *
 * Genau daraus entsteht ein **erfundener Befund**: das Werkzeug behauptet
 * Abwesenheit, wo es nur blind war. Für ein Werkzeug mit dem Grundsatz
 * „Keine erfundenen Daten" ist das die schwerste Fehlerklasse, die es
 * kennt — und dieser Baum hat sie in dieser Sitzung dreimal gefunden
 * (MF-764 „kein Sync" als vier Zustände, MF-769 `UNBEKANNT` gegen
 * `FEHLT`, MF-777 ein Skip, der einen Bruch verdeckte).
 *
 * Deshalb ist der Rückgabewert **zweiteilig**: `befunde` UND
 * `uebersprungen`. Der zweite Teil wird nie weggelassen.
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * Jean Louis-Guérin (DrCoolZic), *Atari Floppy Disk Copy Protection*,
 * Rev. 1.4, 2015-06-24, 77 Seiten, **Copyleft** — keine
 * Lizenzunverträglichkeit, es darf unmittelbar daraus gearbeitet werden.
 * Quelle: `info-coach.fr/atari/documents/_mydoc/Atari-Copy-Protection.pdf`
 *
 * Das Dokument ist bereits ein Schema: 28 Codes, je fünf Felder, und das
 * Feld *Erkennung* ist praktisch die Funktionssignatur.
 *
 * **ACHTUNG, ABSTAMMUNG (MF-792):** derselbe Autor steht hinter dem
 * AIR-Projekt, und dieser Baum führt drei Portierungen davon mit —
 * `src/formats/stx/uft_stx_air.c`, `uft_ipf_air.c`, `uft_kfstream_air.c`
 * (alle GPL-3.0, `docs/OPEN_ITEMS.md:2119-2121`). Für **STX** ist
 * Louis-Guérin damit *dieselbe Hand*: eine Erkennung, die ihre Aussagen
 * aus einem STX-Abbild entnimmt, bestätigte nur, wovon sie abgeleitet ist.
 *
 * Diese Schnittstelle arbeitet deshalb auf **Fluss und Bitstrom**, nicht
 * auf einem fremden Abbildformat. Das ist kein Stilentscheid, sondern der
 * Grund, warum die Aussagen überhaupt etwas belegen.
 *
 * ── Was hier NICHT steht ─────────────────────────────────────────────────
 *
 * Keine `double confidence`. Eine Prozentzahl wäre erfunden; „in 4 von 5
 * Umdrehungen übereinstimmend" ist gemessen. Siehe @ref uft_schutz_halt_t.
 */
#ifndef UFT_SCHUTZBEFUND_H
#define UFT_SCHUTZBEFUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Die Taxonomie ───────────────────────────────────────────────────────
 *
 * 28 Codes aus der Referenz, in zwei Gruppen. Die Gruppe entscheidet,
 * welche EINGABE ein Detektor überhaupt braucht — und damit, ob er auf
 * einer gegebenen Quelle laufen kann oder übersprungen werden muss.
 *
 * Die Aufzählung ist vollständig eingetragen, obwohl erst einer davon
 * einen Detektor hat. Das ist Absicht: die Liste ist die Landkarte, und
 * ein fehlender Code wäre später nicht von einem übersehenen zu
 * unterscheiden. */
typedef enum {
    UFT_SCHUTZ_UNBEKANNT = 0,

    /* Datenbasiert — Bitstrom und Sektorgeometrie genügen. */
    UFT_SCHUTZ_EXT,   /**< Zusatzspuren */
    UFT_SCHUTZ_TNF,   /**< fehlende Spuren */
    UFT_SCHUTZ_SFT,   /**< verschobene Spur */
    UFT_SCHUTZ_DOI,   /**< Daten über Index */
    UFT_SCHUTZ_TLP,   /**< Spurlayoutmuster */
    UFT_SCHUTZ_NOS,   /**< Sektorzahl */
    UFT_SCHUTZ_SSZ,   /**< Sektorgrößen */
    UFT_SCHUTZ_IIF,   /**< ungültiges ID-Feld */
    UFT_SCHUTZ_DSN,   /**< doppelte Sektornummer */
    UFT_SCHUTZ_SWS,   /**< Sektor im Sektor */
    UFT_SCHUTZ_NSD,   /**< nicht-standardisiertes DAM */
    UFT_SCHUTZ_SNI,   /**< Sektor ohne ID */
    UFT_SCHUTZ_SND,   /**< Sektor ohne Daten */
    UFT_SCHUTZ_DCE,   /**< CRC-Fehler in Daten */
    UFT_SCHUTZ_DTT,   /**< Datenspur */
    UFT_SCHUTZ_HDG,   /**< Daten in der Lücke */
    UFT_SCHUTZ_HDT,   /**< Daten in Nicht-Standardspuren */
    UFT_SCHUTZ_IDG,   /**< ungültige Daten in der Lücke */
    UFT_SCHUTZ_ISS,   /**< ungültige Sync-Folge */
    UFT_SCHUTZ_PUT,   /**< teilformatierte Spur */
    UFT_SCHUTZ_FZS,   /**< Fuzzy-Sektor */
    UFT_SCHUTZ_FZT,   /**< Fuzzy-Spur */

    /* Zeitbasiert — FLUSS ZWINGEND. Auf einem dekodierten Abbild
     * (.ST, .MSA, .DIM) grundsätzlich nicht feststellbar. Wer dort
     * „kein Schutz gefunden" meldet, lügt. */
    UFT_SCHUTZ_LGS,   /**< langer Sektor */
    UFT_SCHUTZ_SHS,   /**< kurzer Sektor */
    UFT_SCHUTZ_LGT,   /**< lange Spur */
    UFT_SCHUTZ_SHT,   /**< kurze Spur */
    UFT_SCHUTZ_SBV,   /**< Bitratenvariation im Sektor */
    UFT_SCHUTZ_NFA,   /**< flusslose Zone */

    UFT_SCHUTZ_CODE_ANZAHL
} uft_schutz_code_t;

/** Ist dieser Code ohne Flussdaten überhaupt feststellbar? */
bool uft_schutz_braucht_fluss(uft_schutz_code_t code);

/** Kurzname („LGT") für Berichte und Tests. */
const char *uft_schutz_code_name(uft_schutz_code_t code);

/* ── Beobachtung gegen Schlussfolgerung ──────────────────────────────── */

/**
 * Fuzzy-Bytes sind **beobachtbar**; ob sie aus einer flussmehrdeutigen
 * Zone oder aus einer Taktratenverletzung stammen, ist eine
 * **Schlussfolgerung**. Die Referenz merkt ausdrücklich an, dass
 * Emulatoren diesen Unterschied gar nicht erst machen — sie speichern
 * nur genug, um die Wirkung nachzubilden.
 *
 * Für ein forensisches Werkzeug muss er im Datenmodell erhalten bleiben,
 * und zwar **im Typ**, nicht in einem Kommentar.
 */
typedef enum {
    UFT_BELEG_GEMESSEN = 0,   /**< unmittelbar beobachtet */
    UFT_BELEG_GEFOLGERT       /**< aus Beobachtetem geschlossen */
} uft_beleg_t;

/**
 * Wie gut ein Befund getragen ist — **gemessen, nicht geschätzt**.
 *
 * Hier stünde in einem gewöhnlichen Entwurf ein `double confidence`.
 * Eine solche Zahl wäre erfunden: es gibt kein Verfahren, das sie
 * berechnet. „In 4 von 5 Umdrehungen übereinstimmend" dagegen ist eine
 * Auszählung, und sie sagt dem Leser genau, worauf er sich stützt.
 */
typedef struct {
    int umdrehungen_geprueft;
    int umdrehungen_einig;
} uft_schutz_halt_t;

/** Wo der Befund sitzt. */
typedef struct {
    int      zylinder;
    int      kopf;
    int      sektor;        /**< -1 = betrifft die ganze Spur */
    uint32_t bit_von;       /**< 0/0 = kein Bitbereich angegeben */
    uint32_t bit_bis;
} uft_schutz_ort_t;

/** Ein Befund. */
typedef struct {
    uft_schutz_code_t code;
    uft_beleg_t       beleg;
    uft_schutz_halt_t halt;
    uft_schutz_ort_t  ort;
    /** Maschinenlesbare Begründung: der Messwert, auf dem der Befund
     *  steht. Kein Fließtext — wer den Befund nachprüfen will, braucht
     *  die Zahl, nicht ihre Beschreibung. */
    const char       *messgroesse;
    double            messwert;
} uft_schutz_befund_t;

/* ── Der zweite Teil des Berichts ────────────────────────────────────── */

/**
 * Warum ein Detektor NICHT gelaufen ist.
 *
 * Dieser Aufzählungstyp ist der eigentliche Gegenstand dieser Datei. Ohne
 * ihn wäre jede nicht durchgeführte Prüfung von einer durchgeführten
 * ohne Befund ununterscheidbar.
 */
typedef enum {
    UFT_UEBERSPRUNGEN_KEIN_FLUSS = 0, /**< Quelle trägt keine Zeitinformation */
    UFT_UEBERSPRUNGEN_ZU_WENIG_UMDREHUNGEN,
    UFT_UEBERSPRUNGEN_KEIN_INDEX,
    UFT_UEBERSPRUNGEN_NICHT_DEKODIERBAR,
    UFT_UEBERSPRUNGEN_ABGESCHALTET,
    /** Der Detektor lief, aber DIESER Sektor lag in zu wenigen
     *  Umdrehungen vor (MF-793).
     *
     *  Der Unterschied zu @ref UFT_UEBERSPRUNGEN_ZU_WENIG_UMDREHUNGEN
     *  ist nicht klein: dort war die AUFNAHME zu kurz, hier war sie
     *  lang genug und der Sektor trotzdem nicht oft genug lesbar. Das
     *  ist ein Befund ueber das MEDIUM. Wer beides zusammenwirft,
     *  verwechselt eine zu kurze Aufnahme mit einer beschaedigten
     *  Stelle — dieselbe Verwechslung wie bei
     *  `uft_splice_lage_t` (MF-769). */
    UFT_UEBERSPRUNGEN_ZU_WENIG_LESUNGEN,

    /** Die Quelle traegt zwar Sektoren, aber keine Fehlerinformation —
     *  ein D64 ohne Fehlerkarte, ein IMG, ein ADF (MF-876).
     *
     *  Abgegrenzt gegen @ref UFT_UEBERSPRUNGEN_NICHT_DEKODIERBAR, und
     *  der Unterschied ist nicht kosmetisch: dort war eine Spur nicht
     *  LESBAR, hier ist sie vollstaendig gelesen und die gesuchte
     *  Angabe steht im Format ueberhaupt nicht. Ein Benutzer, dem
     *  „nicht dekodierbar" gemeldet wird, sucht einen Defekt, den es
     *  nicht gibt.
     *
     *  Dieselbe Verwechslung wie `UNBEKANNT` gegen `FEHLT` (MF-769) —
     *  die Fehlerklasse, wegen der es diese Datei gibt. */
    UFT_UEBERSPRUNGEN_KEINE_FEHLERINFO
} uft_uebersprungen_grund_t;

const char *uft_uebersprungen_name(uft_uebersprungen_grund_t g);

typedef struct {
    uft_schutz_code_t         code;
    uft_uebersprungen_grund_t grund;
    uft_schutz_ort_t          ort;
} uft_schutz_uebersprungen_t;

/**
 * Der Bericht. **Beide Listen gehören dazu**; wer nur die erste liest,
 * liest die Hälfte.
 */
typedef struct {
    uft_schutz_befund_t        *befunde;
    size_t                      befund_anzahl;
    size_t                      befund_kapazitaet;

    uft_schutz_uebersprungen_t *uebersprungen;
    size_t                      uebersprungen_anzahl;
    size_t                      uebersprungen_kapazitaet;
} uft_schutz_bericht_t;

void uft_schutz_bericht_init(uft_schutz_bericht_t *b);
void uft_schutz_bericht_frei(uft_schutz_bericht_t *b);
bool uft_schutz_bericht_add(uft_schutz_bericht_t *b,
                            const uft_schutz_befund_t *f);
bool uft_schutz_bericht_uebersprungen(uft_schutz_bericht_t *b,
                                      uft_schutz_code_t code,
                                      uft_uebersprungen_grund_t grund,
                                      uft_schutz_ort_t ort);

/* ── Die Eingabe ─────────────────────────────────────────────────────── */

/**
 * Eine physische Spur, **mehrere Umdrehungen**.
 *
 * Die Referenz ist an dieser Stelle unmissverständlich: **eine Umdrehung
 * reicht nicht.** Drei sind das Minimum, fünf werden empfohlen, und die
 * Begründung ist dreiteilig — jeder Teil erzwingt es für sich:
 *
 *   * Fuzzy-Bytes sind **per Definition** nur durch Mehrfachlesung und
 *     Vergleich feststellbar; eine Mehrheitsregel braucht drei.
 *   * Bei einer verschobenen Spur liegt der Bereich unter dem Index
 *     mitten in einem ID- oder Datenfeld. Von Index zu Index zu lesen
 *     **zerschneidet** ihn.
 *   * Alte Medien liefern verzerrte Signale; zusätzliche Umdrehungen
 *     erlauben, einen Sektor mit gültiger Prüfsumme aus mehreren
 *     Durchgängen auszuwählen.
 *
 * Deshalb steht die Umdrehungszahl **im Typ** und hat keinen
 * Standardwert. Eine Schnittstelle, die eine einzelne Spur entgegennimmt,
 * wäre von vornherein falsch gebaut.
 */
typedef struct {
    int         zylinder;
    int         kopf;

    size_t      umdrehungen;        /**< Invariante: >= 1 */

    /** Fluss je Umdrehung, Nanosekunden zwischen Wechseln. NULL, wenn
     *  die Quelle keinen trägt (.ST, .MSA, .DIM) — das ist ein
     *  **Befund über die Quelle**, kein Fehler. */
    const uint32_t *const *fluss_ns;
    const size_t          *fluss_anzahl;

    /** Indexzeitpunkte je Umdrehung in Nanosekunden, oder NULL. */
    const uint32_t *index_ns;

    /* HIER STAND `bool dekodiert_vorhanden;` (entfernt MF-793).
     *
     * Das Tor prueft beim Fluss die DATEN (`!p->fluss_ns`), pruefte beim
     * Dekodierten aber eine BEHAUPTUNG. Die beiden Zweige standen
     * nebeneinander in derselben Funktion, und der Unterschied fiel
     * nicht auf, solange kein Detektor dekodierte Sektoren wirklich
     * anfasste.
     *
     * Gemessen an einer Probe mit `dekodiert_vorhanden = true` und
     * `sektoren = NULL`: das Tor liess FZS durch, FZS fand keine
     * Sektoren, kehrte zurueck — und der Bericht meldete null Befunde
     * UND null Uebersprungene. Also genau die Verwechslung, gegen die
     * diese Datei geschrieben wurde, im Tor der Datei selbst.
     *
     * Seither entscheidet die Anwesenheit von `sektoren`. Eine
     * Zusicherung, die keine Daten hinter sich hat, ist kein Tor. */

    /** Dekodierte Sektoren JE UMDREHUNG (MF-793).
     *
     * `sektoren[r]` ist das Feld der in Umdrehung `r` gelesenen
     * Sektoren, `sektor_anzahl[r]` seine Laenge. NULL, wenn die Quelle
     * keine dekodierten Sektoren liefert.
     *
     * Warum je Umdrehung und nicht zusammengefasst: ein Fuzzy-Sektor
     * ist PER DEFINITION nur durch Mehrfachlesung und Vergleich
     * feststellbar. Eine Schnittstelle, die die Umdrehungen vorher
     * verschmilzt — etwa durch Mehrheitsentscheid —, hat den Befund
     * bereits weggerechnet, bevor ein Detektor ihn sehen kann. */
    const struct uft_schutz_sektor *const *sektoren;
    const size_t                          *sektor_anzahl;
} uft_schutz_probe_t;

/** Ein dekodierter Sektor einer einzelnen Umdrehung. */
typedef struct uft_schutz_sektor {
    uint8_t        nummer;
    const uint8_t *daten;
    size_t         laenge;
    bool           crc_ok;
} uft_schutz_sektor_t;

/* ── Der Detektor ────────────────────────────────────────────────────── */

/**
 * Was ein Detektor braucht. Er meldet es **vorab**, damit die Registry
 * das Überspringen selbst protokolliert — statt es dem Detektor zu
 * überlassen, der es vergessen könnte.
 */
typedef struct {
    size_t min_umdrehungen;
    bool   braucht_fluss;
    bool   braucht_index;
    bool   braucht_dekodiert;
} uft_schutz_bedarf_t;

typedef struct uft_schutz_detektor {
    uft_schutz_code_t   code;
    uft_schutz_bedarf_t bedarf;
    void (*pruefe)(const struct uft_schutz_detektor *selbst,
                   const uft_schutz_probe_t *probe,
                   uft_schutz_bericht_t *bericht);
    const char *name;
} uft_schutz_detektor_t;

/**
 * Alle Detektoren über eine Probe laufen lassen.
 *
 * Wer seinen Bedarf nicht gedeckt findet, wird **übersprungen und
 * protokolliert** — nie stillschweigend weggelassen.
 */
void uft_schutz_pruefe_alle(const uft_schutz_detektor_t *const *detektoren,
                            size_t anzahl,
                            const uft_schutz_probe_t *probe,
                            uft_schutz_bericht_t *bericht);

/** Der eingebaute Satz. Heute: einer. */
const uft_schutz_detektor_t *const *uft_schutz_detektoren(size_t *anzahl);

/* ── Der erste Detektor: lange/kurze Spur ────────────────────────────── */

/**
 * Nennlänge einer Atari-ST-Spur in MFM-Byte, aus der Referenz.
 *
 * Auffällig wird es ab etwa 5 % Abweichung. Die Referenz nennt als
 * Beispiele Arkanoid unter 6027 Byte und Awesome auf Spur 79 unter
 * 6000 — beide unterhalb der Schwelle, beide dokumentiert.
 */
#define UFT_SCHUTZ_SPURLAENGE_NENN   6240.0
#define UFT_SCHUTZ_SPURLAENGE_TOL    0.05

extern const uft_schutz_detektor_t uft_schutz_detektor_spurlaenge;

/* ── Der zweite Detektor: Fuzzy-Sektor (FZS) ─────────────────────────── */

/**
 * Mindestzahl Lesungen EINES Sektors, damit ein Unterschied etwas
 * bedeutet.
 *
 * Drei, nicht zwei. Bei zwei Lesungen ist ein Unterschied nicht von
 * einem einmaligen Lesefehler zu trennen; ab drei traegt eine
 * Mehrheit, und genau das verlangt die Referenz.
 */
#define UFT_SCHUTZ_FZS_MIN_LESUNGEN  3

extern const uft_schutz_detektor_t uft_schutz_detektor_fuzzy_sektor;

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCHUTZBEFUND_H */
