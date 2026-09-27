/**
 * @file uft_schutz_auswahl.h
 * @brief Was der Protection Analyzer an der laufenden Schutzanalyse steuert
 *        (P3-635 Weg A, MF-1437)
 *
 * Gemessen (MF-1436): der Reiter "Protection Analyzer" hatte 54
 * Bedienelemente, und keines erreichte eine Analyse. Die einzige
 * Schutzanalyse, die laeuft, ist `ufm_c64_prot_analyze()` im
 * ProtectionAnalysisWidget. Sie meldet fuenf Merkmale:
 *
 *     UFM_PROT_LONG_TRACK   UFM_PROT_HALF_TRACK   UFM_PROT_CUSTOM_SYNC
 *     UFM_PROT_DUPLICATE_ID UFM_PROT_BAD_GCR
 *
 * Im Reiter gibt es fuer vier davon gleichnamige Kaestchen — aber
 * gemessen stehen drei ("Long Track", "Duplicate IDs", "Sync Anomaly")
 * in der Gruppe "Detected Protection Features", die das Formular
 * AUSGESCHALTET anlegt: eine Ergebnisanzeige, keine Eingabe. Eine Anzeige
 * zur Eingabe umzudeuten waere eine Bedeutungsaenderung des Formulars.
 * Unter den EINGABEN hat genau eine ein Merkmal: "Enable Half-Track
 * Detection" ↔ UFM_PROT_HALF_TRACK. Sie waehlt seit MF-1437, ob die
 * Analyse Halbspur-Treffer ZEIGT — sie aendert nicht, was gemessen
 * wird. Die anderen vier Merkmale werden immer gezeigt.
 *
 * Ausblenden ist sichtbar: die Analyse nennt, was sie ausgeblendet hat
 * und wie viele Treffer das waren. Voreinstellung ist "zeigen".
 *
 * Eigene Schluessel (`zeige*`), nicht die alten: die alten Werte hatten
 * nie eine Wirkung und `halfTrack` stand voreingestellt auf `false` —
 * ihn zu uebernehmen haette gemessene Treffer beim ersten Start still
 * ausgeblendet.
 *
 * Eine Stelle fuer Schluessel, Zuordnung und Lesen; der Reiter schreibt,
 * die Analyse liest. Nur ein Header, damit keiner den anderen mitbauen
 * muss.
 */
#ifndef UFT_SCHUTZ_AUSWAHL_H
#define UFT_SCHUTZ_AUSWAHL_H

#include <QSettings>
#include <QString>

#include "uft/protection/ufm_c64_protection_taxonomy.h"

/* QSettings-Gruppe von ProtectionTab (ProtectionTab::SETTINGS_GROUP) */
#define UFT_SCHUTZ_AUSWAHL_GRUPPE "ProtectionTab"

struct UftSchutzAuswahlEintrag {
    const char         *schluessel;   /**< QSettings-Schluessel */
    const char         *kaestchen;    /**< objectName im Formular */
    ufm_c64_prot_type_t merkmal;      /**< Wert, den die Analyse meldet */
};

/* Die Zuordnungen — die einzige Tafel dafuer im Baum. Eine Zeile je
 * EINGABE mit gemessenem Merkmal; kommt ein Erkenner dazu, kommt hier
 * eine Zeile dazu. */
static const UftSchutzAuswahlEintrag UFT_SCHUTZ_AUSWAHL[] = {
    { "zeigeHalbspur",     "checkHalfTrack",   UFM_PROT_HALF_TRACK   },
};
static const int UFT_SCHUTZ_AUSWAHL_ANZAHL =
    int(sizeof(UFT_SCHUTZ_AUSWAHL) / sizeof(UFT_SCHUTZ_AUSWAHL[0]));

/** Soll ein gemessenes Merkmal gezeigt werden? Merkmale ohne Kaestchen:
 *  immer. Ohne gespeicherten Wert: zeigen. */
inline bool uftSchutzAuswahlZeigt(int merkmal)
{
    QSettings s;
    s.beginGroup(QStringLiteral(UFT_SCHUTZ_AUSWAHL_GRUPPE));
    for (int i = 0; i < UFT_SCHUTZ_AUSWAHL_ANZAHL; i++)
        if (int(UFT_SCHUTZ_AUSWAHL[i].merkmal) == merkmal)
            return s.value(QString::fromLatin1(UFT_SCHUTZ_AUSWAHL[i].schluessel),
                           true).toBool();
    return true;
}

#endif /* UFT_SCHUTZ_AUSWAHL_H */
