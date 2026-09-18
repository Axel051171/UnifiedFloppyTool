/**
 * @file uft_format_filter_qt.h
 * @brief Die EINE Stelle, an der aus der Plugin-Registry ein
 *        Qt-Dateifilter wird (MF-1245).
 *
 * ANLASS, GEMESSEN: sieben Dateidialoge der Oberflaeche hielten je
 * eine eigene, von Hand gepflegte Endungsliste — 16, 14, 13, 8, 8, 7
 * und 6 Eintraege, alle verschieden. Derselbe Benutzer sah je nach
 * Reiter andere Formate. Gegen die Registry gehalten: die
 * registrierten Plugins beanspruchen 105 eindeutige Endungen, und 85
 * davon kamen in KEINEM Dialog des ganzen Baums vor — darunter `atr`,
 * `dmk`, `d88`, `2mg`, `cas`, `dc42`, `86f`, `cqm`, alle auf Stufe
 * T1b. Ein geprueftes Format, das im Filter fehlt, ist fuer den
 * Bediener nicht da.
 *
 * Diese Datei ist bewusst ein Header mit einer `inline`-Funktion und
 * keine zweite Uebersetzungseinheit: sie braucht damit keine eigene
 * Bauverdrahtung, und die Umhuellung („Name (%1);;All Files (*)")
 * steht trotzdem nur EINMAL. Fuenf Kopien davon waeren derselbe
 * Fehler eine Ebene hoeher.
 *
 * WAS SIE BEI EINEM FEHLSCHLAG TUT, UND WARUM: sie behauptet KEINE
 * Formatliste. Ist die Registry leer oder passt die Kette nicht, gibt
 * sie allein „All Files (*)" zurueck — der Dialog bleibt brauchbar,
 * und niemand liest eine Liste, die nicht gilt. Ein leeres
 * „All Disk Images ()" waere ein Filter, der auf nichts passt, und
 * saehe dabei vollstaendig aus.
 */

#ifndef UFT_FORMAT_FILTER_QT_H
#define UFT_FORMAT_FILTER_QT_H

#include <QCoreApplication>
#include <QString>

#include <vector>

#include "uft/uft_format_plugin.h"

/**
 * Der Dateifilter fuer „ein Diskettenabbild oeffnen", aus der Registry.
 *
 * @param gruppe Beschriftung der ersten Gruppe, z. B. „Disk Images".
 *               Die Aufrufer behalten damit ihren gewohnten Wortlaut.
 */
inline QString uftAbbildDateifilter(const QString &gruppe)
{
    size_t noetig = 0;
    (void)uft_format_endungen_sammeln(nullptr, 0, &noetig);

    if (noetig > 1) {
        std::vector<char> puffer(noetig);
        if (uft_format_endungen_sammeln(puffer.data(), puffer.size(),
                                        nullptr)) {
            return gruppe + QStringLiteral(" (")
                 + QString::fromLatin1(puffer.data())
                 + QStringLiteral(");;")
                 + QCoreApplication::translate("uft", "All Files (*)");
        }
    }
    /* Keine Liste behaupten, die nicht gilt. */
    return QCoreApplication::translate("uft", "All Files (*)");
}

#endif /* UFT_FORMAT_FILTER_QT_H */
