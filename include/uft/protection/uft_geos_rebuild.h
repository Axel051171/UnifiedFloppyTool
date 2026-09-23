/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_geos_rebuild.h
 * @brief Raeumt die GEOS-Schutzspur frei — als Transaktion
 *        (MF-1333, Stufe 4).
 *
 * ── Was das Verfahren ist, und von wem es stammt ─────────────────────
 *
 * Der GEOS-Bootschutz liegt in den LUECKEN zwischen den Sektoren von
 * Spur 21. Ein Kopierprogramm kann sie nicht uebertragen; erzeugen
 * kann man sie nur beim FORMATIEREN. Auf Spur 21 liegt aber ein Teil
 * des GEOS KERNAL — und den kann eine Formatierung nicht erzeugen.
 *
 * Die Beschreibung des Urhebers von GeoCopy (Christian Meilinger,
 * `neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt`) sagt, wie es geht:
 *
 *   "Also (kurz nachgedacht) lagert man den Inhalt dieses Tracks
 *    einfach vorher in einen anderen Bereich der Diskette, aendert die
 *    Blockverkettung und schon hat man ohne Veraenderung des
 *    Programm-Codes eine voll funktionstuechtige GEOS-Boot-Diskette!"
 *
 * Kanal nach MF-695: *Spec*. Die Lizenz im Paket gewaehrt die
 * Weitergabe, verbietet aber kommerziellen Vertrieb ohne Einwilligung —
 * unvereinbar mit diesem GPL-2-Baum. Uebernommen ist das VERFAHREN aus
 * der Beschreibung, keine Zeile des 6502-Quelltextes.
 *
 * ── Was dieses Modul TUT ─────────────────────────────────────────────
 *
 * Es verschiebt JEDEN verketteten Block, der auf Spur 21 liegt, auf
 * einen freien Block anderswo, zieht alle Verweise nach (Blockketten
 * UND Verzeichniseintraege) und bringt die BAM in denselben Stand.
 *
 * Bewusst NICHT "die Bloecke des GEOS KERNAL": welche Datei ein Block
 * traegt, ist fuer die Aufgabe gleichgueltig — die Spur muss von JEDEN
 * Nutzdaten frei sein. Die Frage "welches ist der KERNAL" waere eine
 * zusaetzliche Annahme ohne zusaetzlichen Nutzen.
 *
 * ── Die Transaktion ──────────────────────────────────────────────────
 *
 * Das Eingabeabbild wird NIE angefasst. Gearbeitet wird auf einer
 * privaten Kopie; sie erreicht das Ziel nur, wenn ALLE Pruefungen
 * bestanden sind. Scheitert eine, bleibt das Ziel unveraendert und der
 * Bericht nennt den Grund.
 *
 * Geprueft wird nach dem Umbau, nicht davor:
 *   - jede Blockkette laeuft bis zum Ende und hat keinen Zyklus;
 *   - KEIN Verweis zeigt mehr auf Spur 21;
 *   - die Zahl der Bloecke je Kette ist unveraendert;
 *   - die BAM stimmt mit der tatsaechlichen Belegung ueberein.
 *
 * ── Was dieses Modul NICHT tut, und das ist die Haelfte ──────────────
 *
 * 1. **Es schreibt die Luecken nicht.** Das ist eine Sache der
 *    GCR-Ebene und geht ueber `convert_options_t.gap_fill` (Stufe 3).
 *    Eine D64 kann Lueckenbytes prinzipiell nicht speichern — wer den
 *    Schutz erhalten will, braucht G64, NIB oder Fluss.
 *
 * 2. **Es ist an KEINER echten GEOS-Diskette abgenommen.** Im Korpus
 *    liegt keine; CBMFiles untersagt die Weitergabe seiner Abbilder,
 *    und dieses Haus hat keine Hardware (MF-310). Abgenommen ist die
 *    MECHANIK an synthetischen Disketten, deren Aufbau der Test selbst
 *    kennt. Das ist T2-artig und ausdruecklich nicht T1.
 *
 * 3. **Es entscheidet nichts ueber D71.** Der Urheber sagt woertlich:
 *    "Alle Programme koennen in den momentanen Versionen nur auf
 *    einseitige Disketten kopieren" — und bittet darum, sie fuer 1571
 *    zu erweitern. Fuer zweiseitige Disketten gibt es also keine
 *    Vorlage; dieses Modul nimmt nur 35-Spur-D64 an.
 */

#ifndef UFT_GEOS_REBUILD_H
#define UFT_GEOS_REBUILD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Die Schutzspur. Vom Urheber benannt: "die Nummer 21 (dezimal)". */
#define UFT_GEOS_SCHUTZSPUR   21

/** Groesse einer 35-Spur-D64 ohne Fehlerkarte. */
#define UFT_GEOS_D64_GROESSE  174848u

/** Obergrenze der Bloecke, die verschoben werden koennen. Spur 21 hat
 *  19 Sektoren; mehr kann dort nicht liegen. Eine hoehere Zahl waere
 *  eine Behauptung ueber eine Diskette, die es nicht gibt. */
#define UFT_GEOS_MAX_VERSCHIEBUNG 19

typedef struct {
    /** Wie viele verkettete Bloecke auf Spur 21 gefunden wurden. */
    unsigned gefunden;
    /** Wie viele davon verschoben wurden. Bei Erfolg gleich `gefunden`. */
    unsigned verschoben;
    /** Wie viele Dateien (Verzeichniseintraege) verfolgt wurden. */
    unsigned dateien;
    /** Wie viele Bloecke insgesamt in allen Ketten liegen — vor und nach
     *  dem Umbau gleich, sonst ist etwas verlorengegangen. */
    unsigned bloecke_vorher;
    unsigned bloecke_nachher;

    /** Die vier Pruefungen nach dem Umbau, einzeln. */
    bool ketten_gueltig;
    bool spur21_unreferenziert;
    bool blockzahl_gleich;
    bool bam_stimmig;

    /** true nur, wenn alles bestanden hat UND das Ziel beschrieben
     *  wurde. Ist es false, ist das Ziel UNVERAENDERT. */
    bool uebernommen;

    /** Warum nicht. Leer bei Erfolg. Immer nullterminiert. */
    char grund[192];
} uft_geos_rebuild_bericht_t;

/**
 * @brief Raeumt Spur 21 frei, transaktional.
 *
 * @param d64       Eingabeabbild. Wird NICHT veraendert.
 * @param groesse   Muss @ref UFT_GEOS_D64_GROESSE sein. Eine andere
 *                  Groesse wird ABGESAGT, nicht gedeutet — eine
 *                  Fehlerkarte oder 40 Spuren aendern die BAM-Lage,
 *                  und darueber gibt es hier keine Messung.
 * @param aus       Ziel, mindestens `groesse` Byte. Wird nur bei
 *                  vollstaendigem Erfolg beschrieben. Darf NICHT auf
 *                  `d64` zeigen.
 * @param aus_groesse Groesse des Ziels.
 * @param bericht   Ergebnis. Wird immer gefuellt, auch bei Absage.
 *
 * @return `UFT_OK` nur bei vollstaendigem Erfolg. Sonst
 *         `UFT_ERR_INVALID_ARG` (Zeiger, Groessen, Ueberlappung) oder
 *         `UFT_ERR_CORRUPTED` (die Diskette gibt das Verfahren nicht
 *         her — Grund steht im Bericht).
 */
uft_error_t uft_geos_rebuild(const uint8_t *d64, size_t groesse,
                             uint8_t *aus, size_t aus_groesse,
                             uft_geos_rebuild_bericht_t *bericht);

/** Der Absagegrund als Text. Nie NULL. */
const char *uft_geos_rebuild_grund(const uft_geos_rebuild_bericht_t *b);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GEOS_REBUILD_H */
