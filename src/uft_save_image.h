/**
 * @file uft_save_image.h
 * @brief Speichern unter — wandeln oder ablehnen, nie umetikettieren
 *        (MF-664)
 *
 * Stufe 4 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`, erster Teil.
 *
 * ── Warum das vor dem Varianten-Wähler kommt ─────────────────────────────
 *
 * Der Plan sah als Nächstes vor, beim Speichern die **Zielvariante**
 * wählbar zu machen. Beim Messen des Speicherpfads kam heraus, dass
 * darunter kein Boden ist: `MainWindow::onSave()` liest die Quelldatei
 * und schreibt **dieselben Bytes** — es ruft keinen Format-Schreiber.
 *
 * „Speichern unter → HFE" schrieb bei einem geladenen D64 also dessen
 * Bytes in eine `.hfe`-Datei. Genau die Klasse GUI-1 (MF-635,
 * „Convert = `QFile::copy`") und dieselbe, die MF-568 im Dekodier-Auftrag
 * schon einmal beheben musste.
 *
 * Einen Varianten-Wähler darüberzusetzen hieße, eine zweite Zusage auf
 * eine gebrochene zu stapeln. Deshalb zuerst der Boden.
 *
 * ── Drei Fehler, alle in derselben Funktion ──────────────────────────────
 *
 * 1. **Umetikettieren statt wandeln** (siehe oben).
 * 2. **Speichern unter einem NEUEN Namen scheitert.** `onSaveAs()` setzte
 *    `m_currentFile` auf den Zielnamen und rief dann `onSave()`, das mit
 *    `if (!srcFile.exists())` prüft — die Quelle war da schon
 *    überschrieben. Die Meldung lautete „Source file no longer exists",
 *    was den Benutzer in die Irre führt: die Quelle existierte sehr wohl,
 *    nur hieß sie nicht mehr so.
 * 3. Beides zusammen heißt: der einzige Fall, der überhaupt lief, war
 *    „auf eine bestehende Datei desselben Formats speichern" — und der
 *    ist eine Kopie.
 *
 * ── Was diese Einheit tut ────────────────────────────────────────────────
 *
 *     Ziel == Quelle (Format)  ->  geprüfte Byte-Kopie (Identität)
 *     Ziel != Quelle           ->  uft_convert_file(), mit Preflight-Tor
 *     Ziel unbekannt           ->  ablehnen und sagen warum
 *
 * Der Wandler geht durch dasselbe Tor wie die Konvertieren-Schaltfläche
 * und lehnt UNGEPRÜFTE Paare ab, statt etwas zu erfinden (UFT-A01).
 * `accept_data_loss` bleibt **aus**: die Zustimmung gehört dorthin, wo
 * jemand sie geben kann, und „Speichern" ist keine Stelle, an der man
 * unbemerkt Daten verliert.
 */

#ifndef UFT_SAVE_IMAGE_H
#define UFT_SAVE_IMAGE_H

#include <QString>

/** Was beim Speichern herauskam — genug, um es dem Benutzer zu sagen. */
struct UftSaveOutcome {
    bool    ok = false;        ///< Datei liegt vollständig am Ziel
    bool    converted = false; ///< es wurde gewandelt (sonst Identität)
    QString message;           ///< immer gefüllt: Erfolg wie Ablehnung
};

/**
 * @brief Speichert @p source nach @p target — wandelnd, wenn nötig.
 *
 * @param source  Pfad des GELADENEN Abbilds. Wird nicht verändert.
 * @param target  Zielpfad; seine Endung bestimmt das Zielformat.
 *
 * Bei @p target == @p source ist das die Identität und damit eine
 * Byte-Kopie — die Matrix führt sie mit Messung als LOSSLESS (MF-532).
 */
UftSaveOutcome uftSaveImageAs(const QString &source, const QString &target);

#endif /* UFT_SAVE_IMAGE_H */
