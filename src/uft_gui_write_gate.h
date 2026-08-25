/**
 * @file uft_gui_write_gate.h
 * @brief Das Schreib-Sicherheitstor fuer Abbild-Aenderungen aus der
 *        Oberflaeche (MF-573).
 *
 * ── Warum es das gibt ────────────────────────────────────────────────────
 *
 * `docs/DESIGN_PRINCIPLES.md` verspricht „Keine stille Veraenderung". Der
 * Baum hat dafuer ein Tor — `uft_write_gate_precheck()` in
 * `src/policy/uft_write_gate.c`, mit 20 gruenen Tests, darunter einer, der
 * nachsieht, ob die Schnappschuss-DATEI wirklich existiert und nicht leer
 * ist.
 *
 * Gerufen hat es niemand. Der Rueckstand fuehrte den Punkt als
 * „blockiert — Hardware-Sitzung" (BACKLOG C2), und das war fuer den
 * Abbild-Pfad falsch: die Signatur nimmt `image_data`, `image_len`,
 * `snapshot_dir`, `snapshot_prefix` — keine Hardware. Es gibt sogar eine
 * fertige Richtlinie dafuer, @ref UFT_GATE_POLICY_IMAGE_ONLY.
 *
 * Ein Posten, der als blockiert gefuehrt wird, wird nicht angefasst.
 *
 * ── Was die Oberflaeche vorher tat ───────────────────────────────────────
 *
 * Vier Stellen aenderten Abbilder an Ort und Stelle:
 *
 *     ExplorerTab   ADF-Rename, FAT12-Delete, FAT12-Mkdir
 *     MainWindow    onSave()
 *
 * Gefragt WURDE vorher („Are you sure you want to delete the selected
 * files?") — es war also nichts Stilles. Was fehlte, ist der
 * SCHNAPPSCHUSS: die Zusage, dass der Zustand VOR der Aenderung noch
 * existiert, wenn die Aenderung sich als Fehler erweist.
 *
 * ── Was diese Hilfe NICHT tut ────────────────────────────────────────────
 *
 * Sie ersetzt keine Rueckfrage. Das Tor prueft Format, legt den
 * Schnappschuss an und verifiziert ihn; ob der Benutzer die Aenderung
 * WILL, entscheidet weiterhin der Dialog davor.
 */
#ifndef UFT_GUI_WRITE_GATE_H
#define UFT_GUI_WRITE_GATE_H

#include <QByteArray>
#include <QString>
#include <QWidget>

/**
 * @brief Vor einer Abbild-Aenderung: Format pruefen, Schnappschuss anlegen.
 *
 * @param parent      fuer die Meldung bei Ablehnung; darf nullptr sein
 * @param imagePath   Pfad des Abbilds, das gleich veraendert wird
 * @param imageData   sein Inhalt VOR der Aenderung
 * @param what        kurze Beschreibung der Aenderung, erscheint in der
 *                    Meldung und im Schnappschuss-Namen
 * @param snapshotOut nimmt den Pfad des angelegten Schnappschusses auf;
 *                    darf nullptr sein
 * @return true, wenn geschrieben werden darf. Bei false wurde der Benutzer
 *         bereits informiert und der Aufrufer muss abbrechen —
 *         **ohne** zu schreiben.
 */
bool uftGuiWriteGateAllows(QWidget *parent,
                           const QString &imagePath,
                           const QByteArray &imageData,
                           const QString &what,
                           QString *snapshotOut = nullptr);

#endif /* UFT_GUI_WRITE_GATE_H */
